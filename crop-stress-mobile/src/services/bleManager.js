/**
 * @file bleManager.js
 * @brief Universal Bluetooth Low Energy Manager for FRDM-MCXW71.
 *
 * Supports:
 *   1. Capacitor Native Android BLE (@capacitor-community/bluetooth-le) when running inside the Android APK.
 *   2. Web Bluetooth API (navigator.bluetooth) when running in Google Chrome / Edge over HTTPS.
 *
 * Implements strict connection lifecycle:
 *   [ DISCONNECTED | SCANNING | CONNECTING | CONNECTED ]
 *
 * Zero simulations. If BLE is not actively receiving hardware packets,
 * no data is generated or retained.
 */

import { Capacitor } from '@capacitor/core';
import { BleClient } from '@capacitor-community/bluetooth-le';

export const BLE_STATE = {
  DISCONNECTED: 'DISCONNECTED',
  SCANNING: 'SCANNING',
  CONNECTING: 'CONNECTING',
  CONNECTED: 'CONNECTED'
};

// Known Service UUIDs on FRDM-MCXW71 & standard Wireless UART profiles
const NXP_WIRELESS_UART_SERVICE = '01ff0100-ba5e-f4ee-5ca1-eb1e5e4b1ce0';
const NXP_WIRELESS_UART_CHAR    = '01ff0101-ba5e-f4ee-5ca1-eb1e5e4b1ce0';
const NORDIC_UART_SERVICE       = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NORDIC_UART_TX_CHAR       = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';
const ENV_SENSING_SERVICE       = '0000181a-0000-1000-8000-00805f9b34fb';

class BleManager {
  constructor() {
    this.state = BLE_STATE.DISCONNECTED;
    this.isNative = Capacitor.isNativePlatform();
    this.nativeClientInitialized = false;
    this.nativeDeviceId = null;

    this.device = null;
    this.server = null;
    this.characteristic = null;

    this.stateListeners = new Set();
    this.dataListeners = new Set();
    this.errorListeners = new Set();

    this.rxBuffer = '';
  }

  getState() {
    return this.state;
  }

  isConnected() {
    return this.state === BLE_STATE.CONNECTED;
  }

  onStateChange(callback) {
    this.stateListeners.add(callback);
    callback(this.state);
    return () => this.stateListeners.delete(callback);
  }

  onData(callback) {
    this.dataListeners.add(callback);
    return () => this.dataListeners.delete(callback);
  }

  onError(callback) {
    this.errorListeners.add(callback);
    return () => this.errorListeners.delete(callback);
  }

  _setState(newState) {
    if (this.state !== newState) {
      this.state = newState;
      this.stateListeners.forEach((fn) => {
        try {
          fn(this.state);
        } catch (e) {
          console.error('[BLE] Error in state listener', e);
        }
      });
    }
  }

  _emitData(data) {
    this.dataListeners.forEach((fn) => {
      try {
        fn(data);
      } catch (e) {
        console.error('[BLE] Error in data listener', e);
      }
    });
  }

  _emitError(err) {
    this.errorListeners.forEach((fn) => {
      try {
        fn(err);
      } catch (e) {
        console.error('[BLE] Error in error listener', e);
      }
    });
  }

  isSupported() {
    if (this.isNative) {
      return true; // Native Capacitor Android/iOS BLE is fully supported
    }
    return typeof navigator !== 'undefined' && 'bluetooth' in navigator;
  }

  getUnsupportedReason() {
    if (this.isNative) return null;

    if (typeof window !== 'undefined' && !window.isSecureContext) {
      return `Web Bluetooth requires a secure HTTPS connection. Please open https://${window.location.hostname}:5173`;
    }

    const ua = typeof navigator !== 'undefined' ? navigator.userAgent : '';
    const isIOS = /iPad|iPhone|iPod/.test(ua) && !window.MSStream;
    const isAndroid = /Android/.test(ua);

    if (isIOS) {
      return 'iOS Safari and Chrome do not support Web Bluetooth due to Apple restrictions. Please use the free "Bluefy - Web BLE Browser" app from the App Store, or use Chrome/Edge on your PC.';
    }

    if (isAndroid) {
      return 'Web Bluetooth is not supported in this browser or in-app view. Please open this URL directly inside the Google Chrome app on your phone, or use Chrome/Edge on your PC.';
    }

    return 'Web Bluetooth is not supported in this browser. Please use Google Chrome or Microsoft Edge.';
  }

  async connect() {
    if (!this.isSupported()) {
      const reason = this.getUnsupportedReason();
      const err = new Error(reason);
      this._emitError(err);
      throw err;
    }

    if (this.state === BLE_STATE.CONNECTING || this.state === BLE_STATE.CONNECTED) {
      return;
    }

    // Branch 1: Native Capacitor BLE (Android APK / iOS App)
    if (this.isNative) {
      await this._connectNative();
      return;
    }

    // Branch 2: Web Bluetooth API (Desktop / Mobile Chrome / Edge)
    await this._connectWeb();
  }

  async _connectNative() {
    try {
      this._setState(BLE_STATE.SCANNING);

      if (!this.nativeClientInitialized) {
        await BleClient.initialize();
        this.nativeClientInitialized = true;
      }

      // Request device via native BLE picker
      const device = await BleClient.requestDevice({
        name: 'NXP_WU',
        optionalServices: [NXP_WIRELESS_UART_SERVICE, NORDIC_UART_SERVICE, ENV_SENSING_SERVICE]
      });

      if (!device || !device.deviceId) {
        this._setState(BLE_STATE.DISCONNECTED);
        return;
      }

      this.nativeDeviceId = device.deviceId;
      this._setState(BLE_STATE.CONNECTING);

      await BleClient.connect(this.nativeDeviceId, () => {
        console.log('[BLE Native] Disconnected from device');
        this._handleDisconnect();
      });

      // Start notifications on NXP Wireless UART
      await BleClient.startNotifications(
        this.nativeDeviceId,
        NXP_WIRELESS_UART_SERVICE,
        NXP_WIRELESS_UART_CHAR,
        (value) => {
          this._handleDataView(value);
        }
      );

      this._setState(BLE_STATE.CONNECTED);

      // Trigger single initial GATT read to populate immediately if available
      setTimeout(() => this.readTelemetry().catch(() => {}), 300);
    } catch (err) {
      console.warn('[BLE Native] Connection error:', err.message);
      this._handleDisconnect();
      if (!err.message?.includes('cancel') && !err.message?.includes('User cancelled')) {
        this._emitError(err);
      }
      throw err;
    }
  }

  async _connectWeb() {
    try {
      this._setState(BLE_STATE.SCANNING);

      // Request device with specific filter for NXP_WU and optionalServices for GATT access
      this.device = await navigator.bluetooth.requestDevice({
        filters: [{ name: 'NXP_WU' }],
        optionalServices: [
          NXP_WIRELESS_UART_SERVICE,
          NORDIC_UART_SERVICE,
          ENV_SENSING_SERVICE,
          'generic_access'
        ]
      });

      if (!this.device) {
        this._setState(BLE_STATE.DISCONNECTED);
        return;
      }

      this.device.addEventListener('gattserverdisconnected', () => {
        this._handleDisconnect();
      });

      this._setState(BLE_STATE.CONNECTING);

      this.server = await this.device.gatt.connect();

      // Locate notification characteristic across known services
      let charFound = null;

      // 1. Try NXP Wireless UART
      try {
        const nxpService = await this.server.getPrimaryService(NXP_WIRELESS_UART_SERVICE);
        charFound = await nxpService.getCharacteristic(NXP_WIRELESS_UART_CHAR);
      } catch (_) {
        // Fall through
      }

      // 2. Try Nordic UART Service
      if (!charFound) {
        try {
          const nusService = await this.server.getPrimaryService(NORDIC_UART_SERVICE);
          charFound = await nusService.getCharacteristic(NORDIC_UART_TX_CHAR);
        } catch (_) {
          // Fall through
        }
      }

      // 3. Fallback: inspect any available service with notify characteristics
      if (!charFound) {
        try {
          const services = await this.server.getPrimaryServices();
          for (const s of services) {
            const chars = await s.getCharacteristics();
            for (const c of chars) {
              if (c.properties.notify || c.properties.indicate) {
                charFound = c;
                break;
              }
            }
            if (charFound) break;
          }
        } catch (_) {
          // Fall through
        }
      }

      if (!charFound) {
        throw new Error('No compatible telemetry notification characteristic found on NXP_WU.');
      }

      this.characteristic = charFound;
      await this.characteristic.startNotifications();
      this.characteristic.addEventListener('characteristicvaluechanged', (evt) => {
        this._handleDataView(evt.target.value);
      });

      this._setState(BLE_STATE.CONNECTED);

      // Trigger single initial GATT read to populate immediately if available
      setTimeout(() => this.readTelemetry().catch(() => {}), 300);
    } catch (err) {
      console.warn('[BLE Web] Connection canceled or failed:', err.message);
      this._handleDisconnect();
      if (err.name !== 'NotFoundError') {
        this._emitError(err);
      }
      throw err;
    }
  }

  async readTelemetry() {
    if (this.characteristic && typeof this.characteristic.readValue === 'function') {
      try {
        const val = await this.characteristic.readValue();
        this._handleDataView(val);
      } catch (err) {
        console.warn('[BLE Web] readValue error:', err);
      }
    } else if (this.isNative && this.nativeDeviceId) {
      try {
        const val = await BleClient.read(this.nativeDeviceId, NXP_WIRELESS_UART_SERVICE, NXP_WIRELESS_UART_CHAR);
        this._handleDataView(val);
      } catch (err) {
        console.warn('[BLE Native] read error:', err);
      }
    }
  }

  async disconnect() {
    if (this.isNative && this.nativeDeviceId) {
      try {
        await BleClient.disconnect(this.nativeDeviceId);
      } catch (e) {
        console.warn('[BLE Native] Error during disconnect', e);
      }
    } else if (this.device && this.device.gatt && this.device.gatt.connected) {
      try {
        await this.device.gatt.disconnect();
      } catch (e) {
        console.warn('[BLE Web] Error during disconnect', e);
      }
    }
    this._handleDisconnect();
  }

  _handleDisconnect() {
    if (this.pollInterval) {
      clearInterval(this.pollInterval);
      this.pollInterval = null;
    }
    this.device = null;
    this.server = null;
    this.characteristic = null;
    this.nativeDeviceId = null;
    this.rxBuffer = '';
    this._setState(BLE_STATE.DISCONNECTED);
    // Explicitly notify listeners that data is purged
    this._emitData(null);
  }

  _handleDataView(rawVal) {
    if (!rawVal) return;

    // 1. Unwrap Capacitor object wrapper: { value: ... }
    let actual = rawVal;
    if (typeof rawVal === 'object' && rawVal !== null && 'value' in rawVal) {
      actual = rawVal.value;
    }
    if (!actual) return;

    // 2. If it is a string (Hex string from native BLE or UTF-8 text)
    if (typeof actual === 'string') {
      let str = actual;
      const trimmed = actual.trim();
      // If it looks like a hex string without JSON braces (e.g. "7b22736f..." or "00"):
      if (!trimmed.startsWith('{') && !trimmed.startsWith('[') && /^[0-9a-fA-F]+$/.test(trimmed) && trimmed.length % 2 === 0) {
        str = '';
        for (let i = 0; i < trimmed.length; i += 2) {
          str += String.fromCharCode(parseInt(trimmed.substr(i, 2), 16));
        }
      }
      this._appendAndProcessRxString(str);
      return;
    }

    // 3. If it is a DataView, Uint8Array, or ArrayBuffer
    let value = null;
    if (actual instanceof DataView) {
      value = actual;
    } else if (actual instanceof ArrayBuffer) {
      value = new DataView(actual);
    } else if (actual instanceof Uint8Array || (actual.buffer instanceof ArrayBuffer)) {
      value = new DataView(actual.buffer, actual.byteOffset || 0, actual.byteLength || actual.buffer.byteLength);
    }

    if (!value || value.byteLength === 0) return;

    // Binary format check (20 bytes: 5x float32)
    if (value.byteLength === 20) {
      const soil = value.getFloat32(0, true);
      const temp = value.getFloat32(4, true);
      const rh   = value.getFloat32(8, true);
      const lux  = value.getFloat32(12, true);
      const stress = value.getFloat32(16, true);
      if (Number.isFinite(soil) && Number.isFinite(temp) && soil >= 0 && soil <= 100 && temp >= -20 && temp <= 80) {
        const payload = {
          soil_pct: Math.max(0, Math.min(100, soil)),
          temperature_c: temp,
          humidity_pct: Math.max(0, Math.min(100, rh)),
          light_lux: Math.max(0, lux),
          stress_score: Math.max(0, Math.min(1, stress))
        };
        console.log('[BLE RX Binary Telemetry]', payload);
        this._emitData(payload);
        return;
      }
    }

    // UTF-8 Text / JSON format
    const decoder = new TextDecoder('utf-8');
    const chunk = decoder.decode(value);
    this._appendAndProcessRxString(chunk);
  }

  _appendAndProcessRxString(chunk) {
    if (!chunk) return;
    this.rxBuffer += chunk;

    // Process complete lines terminated by newline
    let newlineIndex;
    while ((newlineIndex = this.rxBuffer.indexOf('\n')) !== -1) {
      const line = this.rxBuffer.substring(0, newlineIndex).trim();
      this.rxBuffer = this.rxBuffer.substring(newlineIndex + 1);

      if (line.length > 0) {
        this._parseLine(line);
      }
    }

    // Also scan for any complete JSON object {...} that arrived without newline
    let startIdx = this.rxBuffer.indexOf('{');
    let endIdx = this.rxBuffer.indexOf('}', startIdx);
    while (startIdx !== -1 && endIdx !== -1 && endIdx > startIdx) {
      const candidate = this.rxBuffer.slice(startIdx, endIdx + 1);
      this.rxBuffer = this.rxBuffer.slice(endIdx + 1);
      this._parseLine(candidate);
      startIdx = this.rxBuffer.indexOf('{');
      endIdx = this.rxBuffer.indexOf('}', startIdx);
    }

    // Guard against runaway buffer
    if (this.rxBuffer.length > 512) {
      this.rxBuffer = this.rxBuffer.slice(-128);
    }
  }

  _parseLine(str) {
    if (!str || typeof str !== 'string') return;

    // Sanitize any empty colons (e.g. `{"soil":,` -> `{"soil":null,`)
    const sanitized = str.replace(/:\s*(?=[,}])/g, ':null');

    // 1. Try JSON
    if (sanitized.includes('{') && sanitized.includes('}')) {
      try {
        const jsonStr = sanitized.slice(sanitized.indexOf('{'), sanitized.lastIndexOf('}') + 1);
        const obj = JSON.parse(jsonStr);
        const soil = obj.soil ?? obj.soil_pct ?? obj.s ?? null;
        const temp = obj.temp ?? obj.temperature_c ?? obj.t ?? null;
        const rh   = obj.hum ?? obj.humidity_pct ?? obj.rh ?? obj.humidity ?? obj.h ?? null;
        const lux  = obj.lux ?? obj.light_lux ?? obj.l ?? null;
        let stress = obj.stress ?? obj.stress_score ?? obj.p ?? null;
        if (stress !== null && stress > 1.0) stress /= 100.0;

        const parsed = {
          soil: soil !== null ? Number(soil) : null,
          temp: temp !== null ? Number(temp) : null,
          hum:  rh !== null ? Number(rh) : null,
          lux:  lux !== null ? Number(lux) : null,
          stress: stress !== null ? Number(stress) : null,
          soil_pct: soil !== null ? Number(soil) : null,
          temperature_c: temp !== null ? Number(temp) : null,
          humidity_pct: rh !== null ? Number(rh) : null,
          light_lux: lux !== null ? Number(lux) : null,
          stress_score: stress !== null ? Number(stress) : null
        };

        console.log('[BLE Telemetry Decoded]', parsed);
        this._emitData(parsed);
        return;
      } catch (e) {
        console.warn('[BLE JSON parse failed, falling back to regex]', e, str);
      }
    }

    // 2. Fallback: extract via regex
    const soilMatch = str.match(/(?:soil|moisture)[^\d]*([\d.]+)/i);
    const tempMatch = str.match(/(?:temp|temperature)[^\d]*([\d.-]+)/i);
    const humMatch  = str.match(/(?:humidity|hum|rh)[^\d]*([\d.]+)/i);
    const luxMatch  = str.match(/(?:lux|light)[^\d]*([\d.]+)/i);
    const stressMatch = str.match(/(?:stress)[^\d]*([\d.]+)/i);

    if (soilMatch || tempMatch || humMatch || luxMatch || stressMatch) {
      const soil = soilMatch ? parseFloat(soilMatch[1]) : null;
      const temp = tempMatch ? parseFloat(tempMatch[1]) : null;
      const rh   = humMatch ? parseFloat(humMatch[1]) : null;
      const lux  = luxMatch ? parseFloat(luxMatch[1]) : null;
      let stress = stressMatch ? parseFloat(stressMatch[1]) : null;
      if (stress !== null && stress > 1.0) stress /= 100.0;

      const parsed = {
        soil,
        temp,
        hum: rh,
        lux,
        stress,
        soil_pct: soil,
        temperature_c: temp,
        humidity_pct: rh,
        light_lux: lux,
        stress_score: stress
      };
      console.log('[BLE Telemetry Regex Matched]', parsed);
      this._emitData(parsed);
    }
  }
}

export const bleManager = new BleManager();
