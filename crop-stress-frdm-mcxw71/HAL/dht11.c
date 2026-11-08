/**
 * @file dht11.c
 * @brief High-precision DHT11 driver for FRDM-MCXW71.
 *
 * Configures D2 (PTC0) as a bidirectional GPIO with internal pull-up.
 * Uses Cortex-M33 hardware DWT cycle counter for sub-microsecond precision timing.
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

/* Primary D2 Pin: PTC0 (Port C, Pin 0) */
#define DHT11_D2_PTC0_PORT       PORTC
#define DHT11_D2_PTC0_GPIO       GPIOC
#define DHT11_D2_PTC0_PIN        0U

/* Alternate D2 Pin: PTC4 (Port C, Pin 4) */
#define DHT11_D2_PTC4_PORT       PORTC
#define DHT11_D2_PTC4_GPIO       GPIOC
#define DHT11_D2_PTC4_PIN        4U

static dht11_status_t s_last_status  = DHT11_STATUS_IDLE_LOW;
static const char    *s_last_pin_str = "D2 (PTC0)";

static inline uint32_t us_to_cycles(uint32_t us, uint32_t clk_hz)
{
    return us * (clk_hz / 1000000U);
}

/**
 * @brief Configure pin as bidirectional GPIO with internal pull-up and PIDR input enabled.
 */
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
    /* Enable digital input buffer in PIDR (required on MCXW71) */
    GPIO_PortInputEnable(gpio, 1U << pin);
#endif

    /* Idle state: Output HIGH */
    gpio->PDDR |= (1U << pin);
    gpio->PSOR = (1U << pin);
}

void dht11_init(void)
{
    /* Enable Port C Clock */
    CLOCK_EnableClock(kCLOCK_PortC);

    /* Initialize DWT hardware cycle counter */
    MSDK_EnableCpuCycleCounter();

    /* Configure PTC0 (primary) and PTC4 (alternate) */
    config_pin_as_gpio(DHT11_D2_PTC0_PORT, DHT11_D2_PTC0_GPIO, DHT11_D2_PTC0_PIN);
    config_pin_as_gpio(DHT11_D2_PTC4_PORT, DHT11_D2_PTC4_GPIO, DHT11_D2_PTC4_PIN);
}

/**
 * @brief Wait for the pin to reach target_level with a microsecond hardware timeout.
 */
static inline bool wait_level(GPIO_Type *gpio, uint32_t pin, uint32_t target_level, uint32_t timeout_us, uint32_t clk_hz)
{
    uint32_t start = MSDK_GetCpuCycleCount();
    uint32_t timeout_cycles = us_to_cycles(timeout_us, clk_hz);

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
    uint32_t clk_hz = CLOCK_GetCoreSysClkFreq();
    if (clk_hz == 0U) {
        clk_hz = SystemCoreClock;
    }
    uint32_t threshold_cycles = us_to_cycles(45U, clk_hz);

    /* Check if line is idle HIGH */
    gpio->PDDR &= ~(1U << pin);
    SDK_DelayAtLeastUs(10U, clk_hz);
    if (((gpio->PDIR >> pin) & 1U) == 0U) {
        gpio->PDDR |= (1U << pin);
        gpio->PSOR = (1U << pin);
        return DHT11_STATUS_IDLE_LOW;
    }

    /* 1. Start Signal: drive line LOW for 20 ms (guaranteeing >= 18 ms) */
    gpio->PDDR |= (1U << pin);
    gpio->PCOR = (1U << pin);
    SDK_DelayAtLeastUs(20000U, clk_hz);

    /* 2. Drive line HIGH for 30 us */
    gpio->PSOR = (1U << pin);
    SDK_DelayAtLeastUs(30U, clk_hz);

    /* 3. Switch to INPUT with pull-up enabled (critical section) */
    uint32_t primask = DisableGlobalIRQ();
    gpio->PDDR &= ~(1U << pin);

    /* 4. Timeout waiting for sensor to pull line LOW (120 us timeout) */
    if (!wait_level(gpio, pin, 0U, 120U, clk_hz)) {
        EnableGlobalIRQ(primask);
        gpio->PDDR |= (1U << pin);
        gpio->PSOR = (1U << pin);
        return DHT11_STATUS_NO_RESPONSE_LOW;
    }

    /* 5. Wait for sensor to pull line HIGH (~80 us) */
    if (!wait_level(gpio, pin, 1U, 120U, clk_hz)) {
        EnableGlobalIRQ(primask);
        gpio->PDDR |= (1U << pin);
        gpio->PSOR = (1U << pin);
        return DHT11_STATUS_NO_RESPONSE_HIGH;
    }

    /* 6. Wait for sensor to pull line LOW (start of data bit 0) */
    if (!wait_level(gpio, pin, 0U, 120U, clk_hz)) {
        EnableGlobalIRQ(primask);
        gpio->PDDR |= (1U << pin);
        gpio->PSOR = (1U << pin);
        return DHT11_STATUS_NO_RESPONSE_LOW;
    }

    /* 7. Sample 40 bits cleanly */
    for (int byte_i = 0; byte_i < 5; ++byte_i) {
        uint8_t byte_val = 0U;
        for (int bit_i = 7; bit_i >= 0; --bit_i) {
            /* Wait for bit start (HIGH pulse) */
            if (!wait_level(gpio, pin, 1U, 100U, clk_hz)) {
                EnableGlobalIRQ(primask);
                gpio->PDDR |= (1U << pin);
                gpio->PSOR = (1U << pin);
                return DHT11_STATUS_BIT_TIMEOUT;
            }

            uint32_t t_start = MSDK_GetCpuCycleCount();

            /* Wait for bit end (LOW pulse) */
            if (!wait_level(gpio, pin, 0U, 120U, clk_hz)) {
                EnableGlobalIRQ(primask);
                gpio->PDDR |= (1U << pin);
                gpio->PSOR = (1U << pin);
                return DHT11_STATUS_BIT_TIMEOUT;
            }

            /* Measure pulse width: ~26-30 us = 0, ~70 us = 1 */
            if ((MSDK_GetCpuCycleCount() - t_start) > threshold_cycles) {
                byte_val |= (uint8_t)(1U << bit_i);
            }
        }
        data[byte_i] = byte_val;
    }

    EnableGlobalIRQ(primask);

    /* Restore idle state */
    gpio->PDDR |= (1U << pin);
    gpio->PSOR = (1U << pin);

    /* 8. Validate 8-bit checksum */
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

    /* 1. Try D2 as PTC0 (Primary as specified) */
    s_last_pin_str = "D2 (PTC0)";
    s_last_status = dht11_read_channel(DHT11_D2_PTC0_GPIO, DHT11_D2_PTC0_PIN, temperature_c, humidity_pct);
    if (s_last_status == DHT11_STATUS_OK) {
        return true;
    }

    SDK_DelayAtLeastUs(2000U, SystemCoreClock);

    /* 2. Try D2 as PTC4 (Board Schematic net CAN0_TX via SH3) */
    dht11_status_t st_ptc4 = dht11_read_channel(DHT11_D2_PTC4_GPIO, DHT11_D2_PTC4_PIN, temperature_c, humidity_pct);
    if (st_ptc4 == DHT11_STATUS_OK) {
        s_last_pin_str = "D2 (PTC4)";
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
