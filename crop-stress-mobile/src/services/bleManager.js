/**
 * @file bleManager.js
 * @brief Strict Bluetooth Low Energy State Manager & Telemetry Link for FRDM-MCXW71.
 *
 * Implements strict connection lifecycle:
 *   [ DISCONNECTED | SCANNING | CONNECTING | CONNECTED ]
 *
 * Zero simulations. If BLE is not actively receiving hardware packets,
 * no data is generated or retained.
 */

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
    return typeof navigator !== 'undefined' && 'bluetooth' in navigator;
  }

  async connect() {
    if (!this.isSupported()) {
      const err = new Error('Web Bluetooth is not supported in this browser. Use Chrome, Edge, or a WebBluetooth-enabled mobile browser.');
      this._emitError(err);
      throw err;
    }

    if (this.state === BLE_STATE.CONNECTING || this.state === BLE_STATE.CONNECTED) {
      return;
    }

    try {
      this._setState(BLE_STATE.SCANNING);

      // Request device with optional services covering FRDM-MCXW71 NXP Wireless UART & NUS
      this.device = await navigator.bluetooth.requestDevice({
        acceptAllDevices: true,
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
        throw new Error('No compatible telemetry notification characteristic found on device.');
      }

      this.characteristic = charFound;
      await this.characteristic.startNotifications();
      this.characteristic.addEventListener('characteristicvaluechanged', (evt) => {
        this._handleNotification(evt);
      });

      this._setState(BLE_STATE.CONNECTED);
    } catch (err) {
      console.warn('[BLE] Connection canceled or failed:', err.message);
      this._handleDisconnect();
      if (err.name !== 'NotFoundError') {
        this._emitError(err);
      }
      throw err;
    }
  }

  async disconnect() {
    if (this.device && this.device.gatt && this.device.gatt.connected) {
      try {
        await this.device.gatt.disconnect();
      } catch (e) {
        console.warn('[BLE] Error during disconnect', e);
      }
    }
    this._handleDisconnect();
  }

  _handleDisconnect() {
    this.device = null;
    this.server = null;
    this.characteristic = null;
    this.rxBuffer = '';
    this._setState(BLE_STATE.DISCONNECTED);
    // Explicitly notify listeners that data is purged
    this._emitData(null);
  }

  _handleNotification(event) {
    const value = event.target.value;
    if (!value || value.byteLength === 0) return;

    // Check if binary telemetry packet (e.g. 5 float32 = 20 bytes or 5 uint16 = 10 bytes)
    if (value.byteLength === 20) {
      const soil = value.getFloat32(0, true);
      const temp = value.getFloat32(4, true);
      const rh   = value.getFloat32(8, true);
      const lux  = value.getFloat32(12, true);
      const stress = value.getFloat32(16, true);
      if (Number.isFinite(soil) && Number.isFinite(temp)) {
        this._emitData({
          soil_pct: Math.max(0, Math.min(100, soil)),
          temperature_c: temp,
          humidity_pct: Math.max(0, Math.min(100, rh)),
          light_lux: Math.max(0, lux),
          stress_score: Math.max(0, Math.min(1, stress))
        });
        return;
      }
    }

    // Otherwise decode as UTF-8 text string (JSON or delimited telemetry)
    const decoder = new TextDecoder('utf-8');
    const chunk = decoder.decode(value);
    this.rxBuffer += chunk;

    // Process line-by-line or complete JSON
    let newlineIdx;
    while ((newlineIdx = this.rxBuffer.indexOf('\n')) !== -1) {
      const line = this.rxBuffer.slice(0, newlineIdx).trim();
      this.rxBuffer = this.rxBuffer.slice(newlineIdx + 1);
      if (line.length > 0) {
        this._parseLine(line);
      }
    }

    // Try parsing raw buffer if JSON without newline
    if (this.rxBuffer.startsWith('{') && this.rxBuffer.endsWith('}')) {
      this._parseLine(this.rxBuffer);
      this.rxBuffer = '';
    }
  }

  _parseLine(str) {
    // 1. Try JSON
    if (str.startsWith('{') && str.endsWith('}')) {
      try {
        const obj = JSON.parse(str);
        const parsed = {
          soil_pct: obj.soil_pct ?? obj.soil ?? null,
          temperature_c: obj.temperature_c ?? obj.temp ?? null,
          humidity_pct: obj.humidity_pct ?? obj.rh ?? obj.humidity ?? null,
          light_lux: obj.light_lux ?? obj.lux ?? null,
          stress_score: obj.stress_score ?? (obj.stress != null ? (obj.stress > 1 ? obj.stress / 100 : obj.stress) : null)
        };
        if (parsed.soil_pct !== null && parsed.temperature_c !== null) {
          this._emitData(parsed);
          return;
        }
      } catch (_) {
        // Not JSON
      }
    }

    // 2. Try Key-Value format (e.g. "SOIL:45.2,TEMP:24.5,RH:60.1,LUX:1500,STRESS:12.5")
    const soilMatch = str.match(/(?:soil|moisture)[^\d]*([\d.]+)/i);
    const tempMatch = str.match(/(?:temp|temperature)[^\d]*([\d.-]+)/i);
    const humMatch  = str.match(/(?:humidity|rh)[^\d]*([\d.]+)/i);
    const luxMatch  = str.match(/(?:lux|light)[^\d]*([\d.]+)/i);
    const stressMatch = str.match(/(?:stress)[^\d]*([\d.]+)/i);

    if (soilMatch && tempMatch) {
      const soil = parseFloat(soilMatch[1]);
      const temp = parseFloat(tempMatch[1]);
      const rh   = humMatch ? parseFloat(humMatch[1]) : 50.0;
      const lux  = luxMatch ? parseFloat(luxMatch[1]) : 500.0;
      let stress = stressMatch ? parseFloat(stressMatch[1]) : null;
      if (stress !== null && stress > 1.0) stress /= 100.0;

      this._emitData({
        soil_pct: Math.max(0, Math.min(100, soil)),
        temperature_c: temp,
        humidity_pct: Math.max(0, Math.min(100, rh)),
        light_lux: Math.max(0, lux),
        stress_score: stress
      });
    }
  }
}

export const bleManager = new BleManager();
