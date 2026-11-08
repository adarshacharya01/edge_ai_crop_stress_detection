#if __has_include("HAL/board_adc.h")
#include "HAL/board_adc.h"
#include "Config/config.h"
#else
#include "board_adc.h"
#include "config.h"
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

    /* 1. Initialize VREF module (provides reference voltage for LPADC) */
    vref_config_t vrefConfig;
    VREF_GetDefaultConfig(&vrefConfig);
    VREF_Init(VREF0, &vrefConfig);

    /* 2. Configure LPADC Clock: Enable clock gate & FRO 192 MHz divided by 10 */
    CLOCK_EnableClock(kCLOCK_Lpadc0);
    CLOCK_SetIpSrc(kCLOCK_Lpadc0, kCLOCK_IpSrcFro192M);
    CLOCK_SetIpSrcDiv(kCLOCK_Lpadc0, kSCG_SysClkDivBy10);



    /* 4. Configure LPADC Peripheral */
    lpadc_config_t lpadcConfig;
    LPADC_GetDefaultConfig(&lpadcConfig);
    lpadcConfig.enableAnalogPreliminary = true;
#if defined(FSL_FEATURE_LPADC_HAS_CTRL_CAL_AVGS) && FSL_FEATURE_LPADC_HAS_CTRL_CAL_AVGS
    lpadcConfig.conversionAverageMode = kLPADC_ConversionAverage128;
#endif
    LPADC_Init(CROP_ADC_BASE, &lpadcConfig);

    /* 5. Request Hardware Calibration as done in NXP example */
    LPADC_DoOffsetCalibration(CROP_ADC_BASE);
    LPADC_DoAutoCalibration(CROP_ADC_BASE);

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
    cmdConfig.conversionResolutionMode = kLPADC_ConversionResolutionHigh;
    LPADC_SetConvCommandConfig(CROP_ADC_BASE, 1U, &cmdConfig);

    /* Configure Software Trigger (Trigger 0 triggers Command 1) */
    lpadc_conv_trigger_config_t trigConfig;
    LPADC_GetDefaultConvTriggerConfig(&trigConfig);
    trigConfig.targetCommandId = 1U;
    trigConfig.enableHardwareTrigger = false;
    LPADC_SetConvTriggerConfig(CROP_ADC_BASE, 0U, &trigConfig);

    /* Execute Software Trigger (Trigger 0 mask = 1U << 0U) */
    LPADC_DoSoftwareTrigger(CROP_ADC_BASE, 1U);

    /* Poll for completion with timeout */
    lpadc_conv_result_t result;
    uint32_t timeout = 100000U;
    while (!LPADC_GetConvResult(CROP_ADC_BASE, &result, 0U) && (--timeout > 0U)) {
        /* Busy wait */
    }

    if (timeout == 0U) {
        return false;
    }

    /* 16-bit High Resolution Conversion (0 - 65535 maps to 0 - CROP_ADC_VREF_MV) */
    *millivolts = ((float)result.convValue * CROP_ADC_VREF_MV) / 65535.0f;
    return true;
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

    /* Determine best/active pin (whichever has highest voltage, default A0) */
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
    /* Onboard phototransistor: ALS-PT19 -> ADC0 Channel 2, Side B */
    return crop_adc_read_channel_mv(CROP_ADC_LIGHT_CHANNEL, (bool)CROP_ADC_LIGHT_SIDE_B, millivolts);
}

