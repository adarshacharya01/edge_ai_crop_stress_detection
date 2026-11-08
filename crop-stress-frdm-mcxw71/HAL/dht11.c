/**
 * @file dht11.c
 * @brief High-precision DHT11 single-wire bit-bang driver for FRDM-MCXW71.
 *
 * Supported header pins:
 *   - J1 Pin 3 = D2 (MCU PTC4, net CAN0_TX via SH3)
 *   - J1 Pin 5 = D4 (MCU PTA19, net TPM/RGB_G via SH5)
 *   - J1 Pin 4 = D3 (MCU PTC1, net CAN0_RX via SH4)
 *
 * Automatically tests candidate pins and reports hardware status.
 */

#if __has_include("HAL/dht11.h")
#include "HAL/dht11.h"
#include "Config/config.h"
#else
#include "dht11.h"
#include "config.h"
#endif

#include "fsl_clock.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "fsl_common.h"

/* Channel 1 (Primary): J1 Pin 3 = D2 (MCU PTC4) */
#define DHT11_D2_PORT       PORTC
#define DHT11_D2_GPIO       GPIOC
#define DHT11_D2_PIN        4U
#define DHT11_D2_CLOCK      kCLOCK_PortC

/* Channel 2: J1 Pin 5 = D4 (MCU PTA19) */
#define DHT11_D4_PORT       PORTA
#define DHT11_D4_GPIO       GPIOA
#define DHT11_D4_PIN        19U
#define DHT11_D4_CLOCK      kCLOCK_PortA

/* Channel 3: J1 Pin 4 = D3 (MCU PTC1) */
#define DHT11_D3_PORT       PORTC
#define DHT11_D3_GPIO       GPIOC
#define DHT11_D3_PIN        1U
#define DHT11_D3_CLOCK      kCLOCK_PortC

static dht11_status_t s_last_status = DHT11_STATUS_IDLE_LOW;
static const char *s_last_pin_str   = "D2 (J1 Pin 3)";

static inline uint32_t us_to_cycles(uint32_t us)
{
    return us * (SystemCoreClock / 1000000U);
}

static void config_pin_as_gpio(PORT_Type *port, GPIO_Type *gpio, uint32_t pin)
{
    const port_pin_config_t pin_cfg = {
        .pullSelect          = (uint16_t)kPORT_PullUp,
        .pullValueSelect     = (uint16_t)kPORT_LowPullResistor,
        .slewRate            = (uint16_t)kPORT_FastSlewRate,
        .passiveFilterEnable = (uint16_t)kPORT_PassiveFilterDisable,
        .openDrainEnable     = (uint16_t)kPORT_OpenDrainDisable,
        .driveStrength       = (uint16_t)kPORT_LowDriveStrength,
        .driveStrength1      = (uint16_t)kPORT_NormalDriveStrength,
        .mux                 = (uint16_t)kPORT_MuxAsGpio,
        .lockRegister        = (uint16_t)kPORT_UnlockRegister
    };
    PORT_SetPinConfig(port, pin, &pin_cfg);

#if defined(FSL_FEATURE_GPIO_HAS_PORT_INPUT_CONTROL) && FSL_FEATURE_GPIO_HAS_PORT_INPUT_CONTROL
    /* CRITICAL for MCXW71: Enable digital input buffer in PIDR!
     * By default on MCXW71, PIDR is 0xFFFFFFFF (all input buffers disabled).
     * Without clearing the bit in PIDR, PDIR will ALWAYS read 0! */
    GPIO_PortInputEnable(gpio, 1U << pin);
#endif

    /* Idle state: Output HIGH */
    gpio->PDDR |= (1U << pin);
    gpio->PSOR = (1U << pin);
}

void dht11_init(void)
{
    /* Enable port clocks */
    CLOCK_EnableClock(DHT11_D2_CLOCK);
    CLOCK_EnableClock(DHT11_D4_CLOCK);
    CLOCK_EnableClock(DHT11_D3_CLOCK);

    /* Initialize DWT cycle counter for microsecond timing */
    MSDK_EnableCpuCycleCounter();

    /* Configure pins with internal pull-up and input buffer enabled */
    config_pin_as_gpio(DHT11_D2_PORT, DHT11_D2_GPIO, DHT11_D2_PIN);
    config_pin_as_gpio(DHT11_D4_PORT, DHT11_D4_GPIO, DHT11_D4_PIN);
    config_pin_as_gpio(DHT11_D3_PORT, DHT11_D3_GPIO, DHT11_D3_PIN);
}

static inline bool wait_level(GPIO_Type *gpio, uint32_t pin, uint32_t target_level, uint32_t timeout_us)
{
    uint32_t start = MSDK_GetCpuCycleCount();
    uint32_t timeout_cycles = us_to_cycles(timeout_us);

    while (((gpio->PDIR >> pin) & 1U) != target_level) {
        if ((MSDK_GetCpuCycleCount() - start) > timeout_cycles) {
            return false;
        }
    }
    return true;
}

static dht11_status_t dht11_read_channel(GPIO_Type *gpio, uint32_t pin, float *temperature_c, float *humidity_pct)
{
    uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};
    uint32_t threshold_cycles = us_to_cycles(45U);

    /* Check if line is idle HIGH (pull-up working) */
    gpio->PDDR &= ~(1U << pin);
    SDK_DelayAtLeastUs(10U, SystemCoreClock);
    if (((gpio->PDIR >> pin) & 1U) == 0U) {
        /* Idle LOW: check wire connection, 3.3V power, or GND */
        gpio->PDDR |= (1U << pin);
        gpio->PSOR = (1U << pin);
        return DHT11_STATUS_IDLE_LOW;
    }

    /* Step 1: Send Host Start Signal */
    gpio->PDDR |= (1U << pin);  /* Output */
    gpio->PCOR = (1U << pin);   /* Pull LOW */
    SDK_DelayAtLeastUs(20000U, SystemCoreClock); /* 20 ms */

    /* Step 2: Release line, switch to input and sample */
    uint32_t primask = DisableGlobalIRQ();

    gpio->PDDR &= ~(1U << pin); /* Input */
    gpio->PSOR = (1U << pin);
    SDK_DelayAtLeastUs(30U, SystemCoreClock); /* Wait 30 us for line release */

    /* Step 3: Wait for DHT11 response LOW (~80 us) */
    if (!wait_level(gpio, pin, 0U, 200U)) {
        EnableGlobalIRQ(primask);
        gpio->PDDR |= (1U << pin);
        gpio->PSOR = (1U << pin);
        return DHT11_STATUS_NO_RESPONSE_LOW;
    }

    /* Wait for DHT11 response HIGH (~80 us) */
    if (!wait_level(gpio, pin, 1U, 200U)) {
        EnableGlobalIRQ(primask);
        gpio->PDDR |= (1U << pin);
        gpio->PSOR = (1U << pin);
        return DHT11_STATUS_NO_RESPONSE_HIGH;
    }

    /* Wait for end of HIGH response (DHT pulls LOW for bit 0) */
    if (!wait_level(gpio, pin, 0U, 200U)) {
        EnableGlobalIRQ(primask);
        gpio->PDDR |= (1U << pin);
        gpio->PSOR = (1U << pin);
        return DHT11_STATUS_NO_RESPONSE_LOW;
    }

    /* Step 4: Read 40 data bits */
    for (int byte_i = 0; byte_i < 5; ++byte_i) {
        uint8_t byte_val = 0U;
        for (int bit_i = 7; bit_i >= 0; --bit_i) {
            /* Wait for bit start (HIGH pulse) */
            if (!wait_level(gpio, pin, 1U, 120U)) {
                EnableGlobalIRQ(primask);
                gpio->PDDR |= (1U << pin);
                gpio->PSOR = (1U << pin);
                return DHT11_STATUS_BIT_TIMEOUT;
            }

            uint32_t t_start = MSDK_GetCpuCycleCount();

            /* Wait for bit end (LOW pulse) */
            if (!wait_level(gpio, pin, 0U, 140U)) {
                EnableGlobalIRQ(primask);
                gpio->PDDR |= (1U << pin);
                gpio->PSOR = (1U << pin);
                return DHT11_STATUS_BIT_TIMEOUT;
            }

            if ((MSDK_GetCpuCycleCount() - t_start) > threshold_cycles) {
                byte_val |= (uint8_t)(1U << bit_i);
            }
        }
        data[byte_i] = byte_val;
    }

    EnableGlobalIRQ(primask);

    /* Restore pin to output HIGH (idle) */
    gpio->PDDR |= (1U << pin);
    gpio->PSOR = (1U << pin);

    /* Step 5: Checksum Verification */
    uint8_t checksum = (uint8_t)((data[0] + data[1] + data[2] + data[3]) & 0xFFU);
    if (checksum != data[4] || (data[0] == 0U && data[2] == 0U)) {
        return DHT11_STATUS_CHECKSUM_ERROR;
    }

    *humidity_pct  = (float)data[0] + (float)data[1] * 0.1f;
    *temperature_c = (float)data[2] + (float)data[3] * 0.1f;
    return DHT11_STATUS_OK;
}

bool dht11_read(float *temperature_c, float *humidity_pct)
{
    if (temperature_c == NULL || humidity_pct == NULL) {
        return false;
    }

    /* 1. Try J1 Pin 3 (D2 / PTC4) */
    s_last_pin_str = "D2 (J1 Pin 3 / PTC4)";
    s_last_status = dht11_read_channel(DHT11_D2_GPIO, DHT11_D2_PIN, temperature_c, humidity_pct);
    if (s_last_status == DHT11_STATUS_OK) {
        return true;
    }

    /* Brief recovery */
    SDK_DelayAtLeastUs(2000U, SystemCoreClock);

    /* 2. Try J1 Pin 5 (D4 / PTA19) */
    dht11_status_t st_d4 = dht11_read_channel(DHT11_D4_GPIO, DHT11_D4_PIN, temperature_c, humidity_pct);
    if (st_d4 == DHT11_STATUS_OK) {
        s_last_pin_str = "D4 (J1 Pin 5 / PTA19)";
        s_last_status = DHT11_STATUS_OK;
        return true;
    }

    /* Brief recovery */
    SDK_DelayAtLeastUs(2000U, SystemCoreClock);

    /* 3. Try J1 Pin 4 (D3 / PTC1) */
    dht11_status_t st_d3 = dht11_read_channel(DHT11_D3_GPIO, DHT11_D3_PIN, temperature_c, humidity_pct);
    if (st_d3 == DHT11_STATUS_OK) {
        s_last_pin_str = "D3 (J1 Pin 4 / PTC1)";
        s_last_status = DHT11_STATUS_OK;
        return true;
    }

    return false;
}

const char *dht11_get_status_str(void)
{
    switch (s_last_status) {
        case DHT11_STATUS_OK:
            return "OK (Data valid)";
        case DHT11_STATUS_IDLE_LOW:
            return "Pin idle LOW (0V) -> Verify 3.3V/GND power & signal wire";
        case DHT11_STATUS_NO_RESPONSE_LOW:
            return "No response -> Sensor did not pull line LOW after start";
        case DHT11_STATUS_NO_RESPONSE_HIGH:
            return "No response -> Sensor held line LOW, did not pull HIGH";
        case DHT11_STATUS_BIT_TIMEOUT:
            return "Bit timeout -> Signal corrupted or disconnected";
        case DHT11_STATUS_CHECKSUM_ERROR:
            return "Checksum mismatch -> Data packet error";
        default:
            return "Unknown error";
    }
}

const char *dht11_get_active_pin_str(void)
{
    return s_last_pin_str;
}
