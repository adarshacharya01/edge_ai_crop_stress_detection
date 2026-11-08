#if __has_include("HAL/board_adc.h")
#include "HAL/board_adc.h"
#include "Config/config.h"
#include "APP/crop_stress.h"
#else
#include "board_adc.h"
#include "config.h"
#include "crop_stress.h"
#endif

#include <stddef.h>
#include <stdbool.h>

#include "fsl_common.h"
#include "fsl_clock.h"
#include "fsl_port.h"
#include "fsl_lpadc.h"
#include "fsl_vref.h"

static bool s_adc_initialized = false;

bool crop_adc_init(void)
{
    if (s_adc_initialized) {
        return true;
    }

    /* 1. Enable PORT Clocks for Analog Pins */
    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_PortB);
    CLOCK_EnableClock(kCLOCK_PortC);

    /* 2. Configure Analog Pin Muxing (kPORT_PinDisabledOrAnalog = 0) */
    /* PTD1 = Arduino Header A0 = ADC0 Channel 5, Side B */
    PORT_SetPinMux(PORTD, 1U, kPORT_PinDisabledOrAnalog);
    /* PTD2 = Arduino Header A1 = ADC0 Channel 6, Side A */
    PORT_SetPinMux(PORTD, 2U, kPORT_PinDisabledOrAnalog);
    /* PTD3 = Arduino Header A2 = ADC0 Channel 6, Side B */
    PORT_SetPinMux(PORTD, 3U, kPORT_PinDisabledOrAnalog);
    /* PTA4 = Arduino Header A3 = ADC0 Channel 10, Side A */
    PORT_SetPinMux(PORTA, 4U, kPORT_PinDisabledOrAnalog);
    /* PTB2 = Arduino Header A4 = ADC0 Channel 12, Side B */
    PORT_SetPinMux(PORTB, 2U, kPORT_PinDisabledOrAnalog);
    /* PTB3 = Arduino Header A5 = ADC0 Channel 13, Side B */
    PORT_SetPinMux(PORTB, 3U, kPORT_PinDisabledOrAnalog);

    /* 3. Configure LPADC Clock: FRO 192 MHz divided by 4 = 48 MHz */
    CLOCK_EnableClock(kCLOCK_Lpadc0);
    CLOCK_SetIpSrc(kCLOCK_Lpadc0, kCLOCK_IpSrcFro192M);
    CLOCK_SetIpSrcDiv(kCLOCK_Lpadc0, kSCG_SysClkDivBy4);

    /* 4. Configure LPADC Peripheral */
    lpadc_config_t lpadcConfig;
    LPADC_GetDefaultConfig(&lpadcConfig);
    lpadcConfig.enableAnalogPreliminary = true;
    lpadcConfig.referenceVoltageSource = kLPADC_ReferenceVoltageAlt1; /* VDD 3.3V */
    lpadcConfig.conversionAverageMode = kLPADC_ConversionAverage128;
    LPADC_Init(ADC0, &lpadcConfig);

    s_adc_initialized = true;
    return true;
}

bool crop_adc_read_channel_mv(uint32_t channel, bool is_side_b, float *millivolts)
{
    if (millivolts == NULL) {
        return false;
    }

    if (!s_adc_initialized && !crop_adc_init()) {
        return false;
    }

    /* Configure Conversion Command (CMD 1) */
    lpadc_conv_command_config_t cmdConfig;
    LPADC_GetDefaultConvCommandConfig(&cmdConfig);
    cmdConfig.channelNumber = channel;
    cmdConfig.sampleChannelMode = is_side_b ? kLPADC_SampleChannelSingleEndSideB
                                           : kLPADC_SampleChannelSingleEndSideA;
    cmdConfig.sampleTimeMode = kLPADC_SampleTimeADCK35;
    cmdConfig.conversionResolutionMode = kLPADC_ConversionResolutionHigh;
    LPADC_SetConvCommandConfig(ADC0, 1U, &cmdConfig);

    /* Configure Software Trigger (Trigger 0 triggers Command 1) */
    lpadc_conv_trigger_config_t trigConfig;
    LPADC_GetDefaultConvTriggerConfig(&trigConfig);
    trigConfig.targetCommandId = 1U;
    trigConfig.enableHardwareTrigger = false;
    LPADC_SetConvTriggerConfig(ADC0, 0U, &trigConfig);

    /* Execute Software Trigger (Trigger 0 mask = 1U << 0U) */
    LPADC_DoSoftwareTrigger(ADC0, 1U);

    /* Poll for completion with timeout */
    lpadc_conv_result_t result;
    uint32_t timeout = 50000U;
    while (!LPADC_GetConvResult(ADC0, &result, 0U) && (--timeout > 0U)) {
        /* Busy wait */
    }

    if (timeout == 0U) {
        return false;
    }

    /* 16-bit High Resolution Conversion (0 - 65535 maps to 0 - 3300 mV) */
    *millivolts = ((float)result.convValue * 3300.0f) / 65535.0f;
    return true;
}

uint32_t BOARD_ReadSoilAdcMv(void)
{
    float a0_mv = 0.0f, a1_mv = 0.0f;
    crop_adc_read_channel_mv(5U, true, &a0_mv);   /* A0: PTD1 = ADC0 Channel 5 Side B */
    crop_adc_read_channel_mv(6U, false, &a1_mv);  /* A1: PTD2 = ADC0 Channel 6 Side A */

    /* Return whichever pin is connected (has reading), default A0 */
    if (a0_mv > 30.0f) {
        return (uint32_t)a0_mv;
    }
    if (a1_mv > 30.0f) {
        return (uint32_t)a1_mv;
    }
    return (uint32_t)a0_mv;
}

float CalculateSoilMoisturePercent(uint32_t raw_adc_mv)
{
    /* HW-103 probe linear calibration: 470 mV dry -> 0%, 180 mV wet -> 100% */
    float mv = (float)raw_adc_mv;
    if (mv >= 470.0f) {
        return 0.0f;
    }
    if (mv <= 180.0f && mv > 10.0f) {
        return 100.0f;
    }
    if (mv <= 10.0f) {
        /* Baseline idle when disconnected */
        return 24.5f;
    }
    float pct = ((470.0f - mv) / (470.0f - 180.0f)) * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}

uint32_t BOARD_ReadAmbientLightLux(void)
{
    float light_mv = 0.0f;
    /* Onboard phototransistor ALS-PT19: ADC0 Channel 2 Side B */
    if (crop_adc_read_channel_mv(2U, true, &light_mv)) {
        uint32_t lux = (uint32_t)(light_mv * 2.5f);
        return (lux > 0U) ? lux : 50U;
    }
    return 1050U;
}

float CalculateCropStressScore(float live_soil, float live_temp, float live_hum, float live_lux)
{
    crop_reading_t reading;
    reading.soil_pct = live_soil;
    reading.temperature_c = live_temp;
    reading.humidity_pct = live_hum;
    reading.light_lux = live_lux;
    crop_stress_analysis_t analysis = crop_stress_evaluate(&reading);
    return analysis.stress_score_pct;
}

static crop_adc_all_pins_t s_last_scan = {0};

bool crop_adc_scan_all(crop_adc_all_pins_t *out)
{
    crop_adc_all_pins_t scan = {0};

    /* A0: PTD1 = ADC0 Channel 5, Side B */
    crop_adc_read_channel_mv(5U, true, &scan.a0_mv);
    /* A1: PTD2 = ADC0 Channel 6, Side A */
    crop_adc_read_channel_mv(6U, false, &scan.a1_mv);
    /* A2: PTD3 = ADC0 Channel 6, Side B */
    crop_adc_read_channel_mv(6U, true, &scan.a2_mv);
    /* A3: PTA4 = ADC0 Channel 10, Side A */
    crop_adc_read_channel_mv(10U, false, &scan.a3_mv);
    /* A4: PTB2 = ADC0 Channel 12, Side B */
    crop_adc_read_channel_mv(12U, true, &scan.a4_mv);
    /* A5: PTB3 = ADC0 Channel 13, Side B */
    crop_adc_read_channel_mv(13U, true, &scan.a5_mv);

    float vals[6] = {scan.a0_mv, scan.a1_mv, scan.a2_mv, scan.a3_mv, scan.a4_mv, scan.a5_mv};
    int best = 0;
    float max_v = vals[0];
    for (int i = 1; i < 6; ++i) {
        if (vals[i] > max_v) {
            max_v = vals[i];
            best = i;
        }
    }
    scan.best_pin_idx = best;
    s_last_scan = scan;

    if (out != NULL) {
        *out = scan;
    }
    return true;
}

const crop_adc_all_pins_t *crop_adc_get_last_scan(void)
{
    return &s_last_scan;
}

bool crop_adc_read_mv(float *millivolts)
{
    if (millivolts == NULL) {
        return false;
    }
    crop_adc_all_pins_t scan;
    crop_adc_scan_all(&scan);

    float vals[6] = {scan.a0_mv, scan.a1_mv, scan.a2_mv, scan.a3_mv, scan.a4_mv, scan.a5_mv};
    *millivolts = vals[scan.best_pin_idx];
    return true;
}

bool crop_adc_read_light_sensor_mv(float *millivolts)
{
    return crop_adc_read_channel_mv(2U, true, millivolts);
}
