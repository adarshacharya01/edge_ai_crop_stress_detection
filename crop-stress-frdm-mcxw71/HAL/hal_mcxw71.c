#include "HAL/hal.h"
#include "HAL/board_adc.h"
#include "Config/config.h"

#include <stddef.h>
#include <stdbool.h>

#if !defined(CROP_SIMULATOR)

#include "board.h"
#include "clock_config.h"
#include "pin_mux.h"
#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_port.h"
#include "fsl_lpi2c.h"

/**
 * @brief Configures PTB4 (SDA) and PTB5 (SCL) for LPI2C1 on FRDM-MCXW71.
 * Verified from NXP App Code Hub demo: dm-mcxw71-accel-and-light-ble.
 */
static void init_i2c_pins(void)
{
    CLOCK_EnableClock(kCLOCK_PortB);

    const port_pin_config_t lpi2c1_pin = {
        .pullSelect = (uint16_t)kPORT_PullUp,
        .pullValueSelect = (uint16_t)kPORT_LowPullResistor,
        .slewRate = (uint16_t)kPORT_FastSlewRate,
        .passiveFilterEnable = (uint16_t)kPORT_PassiveFilterDisable,
        .openDrainEnable = (uint16_t)kPORT_OpenDrainDisable,
        .driveStrength = (uint16_t)kPORT_LowDriveStrength,
        .driveStrength1 = (uint16_t)kPORT_NormalDriveStrength,
        .mux = (uint16_t)kPORT_MuxAlt4,
        .lockRegister = (uint16_t)kPORT_UnlockRegister
    };

    /* PTB4 -> LPI2C1_SDA (Pin 2) */
    PORT_SetPinConfig(PORTB, 4U, &lpi2c1_pin);
    /* PTB5 -> LPI2C1_SCL (Pin 3) */
    PORT_SetPinConfig(PORTB, 5U, &lpi2c1_pin);
}

bool hal_init(void)
{
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

    /* Configure I2C Clock: FRO 192 MHz / 16 = 12 MHz */
    CLOCK_SetIpSrc(kCLOCK_Lpi2c1, kCLOCK_IpSrcFro192M);
    CLOCK_SetIpSrcDiv(kCLOCK_Lpi2c1, kSCG_SysClkDivBy16);

    /* Configure I2C Pins */
    init_i2c_pins();

    /* Initialize LPADC and VREF for analog soil moisture and light sensing */
    if (!crop_adc_init()) {
        return false;
    }

    /* Initialize LPI2C Master (100 kHz) */
    lpi2c_master_config_t config;
    LPI2C_MasterGetDefaultConfig(&config);
    config.baudRate_Hz = CROP_I2C_BAUDRATE_HZ;
    uint32_t src_clock = CLOCK_GetIpFreq(kCLOCK_Lpi2c1);
    if (src_clock == 0U) {
        src_clock = CROP_I2C_SOURCE_CLOCK_HZ;
    }
    LPI2C_MasterInit(CROP_I2C_BASE, &config, src_clock);

    return true;
}

void hal_delay_ms(uint32_t milliseconds)
{
    SDK_DelayAtLeastUs(milliseconds * 1000U, CLOCK_GetCoreSysClkFreq());
}

static bool transfer(uint8_t address, uint8_t *data, uint32_t size,
                     lpi2c_direction_t direction)
{
    if (data == NULL || size == 0U) {
        return false;
    }
    lpi2c_master_transfer_t request = {0};
    request.slaveAddress = address;
    request.direction = direction;
    request.data = data;
    request.dataSize = size;
    request.flags = kLPI2C_TransferDefaultFlag;
    return LPI2C_MasterTransferBlocking(CROP_I2C_BASE, &request) == kStatus_Success;
}

bool hal_i2c_write(uint8_t address, const uint8_t *data, uint32_t size)
{
    return transfer(address, (uint8_t *)data, size, kLPI2C_Write);
}

bool hal_i2c_read(uint8_t address, uint8_t *data, uint32_t size)
{
    return transfer(address, data, size, kLPI2C_Read);
}

bool hal_soil_millivolts(float *millivolts)
{
    return crop_adc_read_mv(millivolts);
}

#endif /* !defined(CROP_SIMULATOR) */
