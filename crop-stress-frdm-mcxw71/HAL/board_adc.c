#include "HAL/board_adc.h"
#include "Config/config.h"

#include <stddef.h>
#include <stdbool.h>

#if !defined(CROP_SIMULATOR)

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

    /* 2. Configure LPADC Clock: FRO 192 MHz divided by 10 */
    CLOCK_SetIpSrc(kCLOCK_Lpadc0, kCLOCK_IpSrcFro192M);
    CLOCK_SetIpSrcDiv(kCLOCK_Lpadc0, kSCG_SysClkDivBy10);

    /* 3. Configure Pin Muxing for External Analog Input (PTD1 = Arduino A0 = ADC0_B5) */
    CLOCK_EnableClock(kCLOCK_PortD);
    PORT_SetPinMux(PORTD, 1U, kPORT_PinDisabledOrAnalog);

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

bool crop_adc_read_mv(float *millivolts)
{
    /* Soil probe: Pin PTD1 (Arduino Header A0) -> ADC0 Channel 5, Side B */
    return crop_adc_read_channel_mv(CROP_ADC_SOIL_CHANNEL, (bool)CROP_ADC_SOIL_SIDE_B, millivolts);
}

bool crop_adc_read_light_sensor_mv(float *millivolts)
{
    /* Onboard phototransistor: ALS-PT19 -> ADC0 Channel 2, Side B */
    return crop_adc_read_channel_mv(CROP_ADC_LIGHT_CHANNEL, (bool)CROP_ADC_LIGHT_SIDE_B, millivolts);
}

#else

/* Desktop simulator dummy stubs */
bool crop_adc_init(void) { return true; }
bool crop_adc_read_mv(float *millivolts) {
    if (millivolts) *millivolts = 1800.0f;
    return true;
}
bool crop_adc_read_light_sensor_mv(float *millivolts) {
    if (millivolts) *millivolts = 1500.0f;
    return true;
}
bool crop_adc_read_channel_mv(uint32_t channel, bool is_side_b, float *millivolts) {
    (void)channel; (void)is_side_b;
    if (millivolts) *millivolts = 1800.0f;
    return true;
}

#endif /* !defined(CROP_SIMULATOR) */
