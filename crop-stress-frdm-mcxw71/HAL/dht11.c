/**
 * @file dht11.c
 * @brief High-precision DHT11 driver for FRDM-MCXW71 with Open-Drain configuration.
 *
 * Requirements fulfilled:
 *   1. PIN MUX & PORT CONFIGURATION (PTC0 / D2):
 *      - PORTC Pin 0 configured with kPORT_MuxAsGpio.
 *      - Internal pull-up enabled (kPORT_PullUp, kPORT_LowPullResistor).
 *      - Open-Drain mode enabled (kPORT_OpenDrainEnable).
 *      - Input buffer continuously enabled in PIDR via GPIO_PortInputEnable().
 *   2. IDLE & START SIGNAL:
 *      - Default Idle: Input mode floating to 3.3V. Must read logic 1 at idle.
 *      - Start: Output LOW for 20 ms, drive HIGH for 30 us, switch to INPUT.
 *   3. HIGH-ACCURACY TIMING & TIMEOUTS:
 *      - Cortex-M33 DWT cycle counter with dynamic core clock.
 *      - Interrupts disabled during 40-bit transmission window.
 *      - Timeouts for LOW/HIGH responses and bit pulses (>45 us = '1', <35 us = '0').
 *   4. CHECKSUM & OUTPUT:
 *      - 5-byte payload validated against 8-bit checksum.
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

/* Primary Pin: D2 = PTC0 (Port C, Pin 0) */
#define DHT11_PRIMARY_PORT       PORTC
#define DHT11_PRIMARY_GPIO       GPIOC
#define DHT11_PRIMARY_PIN        0U

/* Alternate Pin: D2 = PTC4 (Port C, Pin 4 via SH3) */
#define DHT11_ALT_PORT           PORTC
#define DHT11_ALT_GPIO           GPIOC
#define DHT11_ALT_PIN            4U

static dht11_status_t s_last_status  = DHT11_STATUS_IDLE_LOW;
static const char    *s_last_pin_str = "D2 (PTC0)";

static inline uint32_t us_to_cycles(uint32_t us, uint32_t clk_hz)
{
    return us * (clk_hz / 1000000U);
}

/**
 * @brief Configure pin as Open-Drain bidirectional GPIO with internal pull-up.
 */
static void config_pin_open_drain(PORT_Type *port, GPIO_Type *gpio, uint32_t pin)
{
    const port_pin_config_t pin_cfg = {
        .pullSelect          = (uint16_t)kPORT_PullUp,
        .pullValueSelect     = (uint16_t)kPORT_LowPullResistor,
        .slewRate            = (uint16_t)kPORT_FastSlewRate,
        .passiveFilterEnable = (uint16_t)kPORT_PassiveFilterDisable,
        .openDrainEnable     = (uint16_t)kPORT_OpenDrainEnable,  /* CRITICAL: Open-Drain mode */
        .driveStrength       = (uint16_t)kPORT_LowDriveStrength,
        .driveStrength1      = (uint16_t)kPORT_NormalDriveStrength,
        .mux                 = (uint16_t)kPORT_MuxAsGpio,
        .lockRegister        = (uint16_t)kPORT_UnlockRegister
    };
    PORT_SetPinConfig(port, pin, &pin_cfg);

#if defined(FSL_FEATURE_GPIO_HAS_PORT_INPUT_CONTROL) && FSL_FEATURE_GPIO_HAS_PORT_INPUT_CONTROL
    /* Keep digital input buffer continuously enabled in PIDR */
    GPIO_PortInputEnable(gpio, 1U << pin);
#endif

    /* Default idle state: INPUT (Open-Drain floats HIGH to 3.3V via pull-up) */
    gpio->PDDR &= ~(1U << pin);
    gpio->PSOR = (1U << pin);
}

void dht11_init(void)
{
    /* 1. Enable Port C clock */
    CLOCK_EnableClock(kCLOCK_PortC);

    /* 2. Initialize Cortex-M33 DWT cycle counter for sub-microsecond timing */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    MSDK_EnableCpuCycleCounter();

    /* 3. Configure PTC0 (primary D2) and PTC4 (alternate D2) */
    config_pin_open_drain(DHT11_PRIMARY_PORT, DHT11_PRIMARY_GPIO, DHT11_PRIMARY_PIN);
    config_pin_open_drain(DHT11_ALT_PORT, DHT11_ALT_GPIO, DHT11_ALT_PIN);
}

/**
 * @brief Wait for the pin to transition to target_level with a microsecond timeout.
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

    /* ------------------------------------------------------------------
     * Step 1: Idle Check
     *   Pin is in INPUT mode. It MUST read logic HIGH (3.3V via pull-up)
     *   before attempting communication. If 0V, line is shorted or floating.
     * ---------------------------------------------------------------- */
    gpio->PDDR &= ~(1U << pin);
    SDK_DelayAtLeastUs(10U, clk_hz);
    if (((gpio->PDIR >> pin) & 1U) == 0U) {
        return DHT11_STATUS_IDLE_LOW;
    }

    /* ------------------------------------------------------------------
     * Step 2: Start Signal
     *   a. Switch direction to OUTPUT, drive LOW for 20 ms
     *   b. Drive HIGH for 30 us
     *   c. Switch direction back to INPUT
     * ---------------------------------------------------------------- */
    gpio->PCOR = (1U << pin);
    gpio->PDDR |= (1U << pin);  /* Output LOW */
    SDK_DelayAtLeastUs(20000U, clk_hz);

    gpio->PSOR = (1U << pin);   /* Output HIGH */
    SDK_DelayAtLeastUs(30U, clk_hz);

    /* Critical section: disable interrupts during the 4ms data frame */
    uint32_t primask = DisableGlobalIRQ();

    gpio->PDDR &= ~(1U << pin); /* Switch to INPUT */

    /* ------------------------------------------------------------------
     * Step 3: Sensor Acknowledgment
     *   - Wait for sensor to pull line LOW (80 us response, 120 us timeout)
     *   - Wait for sensor to pull line HIGH (80 us response, 120 us timeout)
     *   - Wait for sensor to pull line LOW (start of data bit 0)
     * ---------------------------------------------------------------- */
    if (!wait_level(gpio, pin, 0U, 120U, clk_hz)) {
        EnableGlobalIRQ(primask);
        return DHT11_STATUS_NO_RESPONSE_LOW;
    }

    if (!wait_level(gpio, pin, 1U, 120U, clk_hz)) {
        EnableGlobalIRQ(primask);
        return DHT11_STATUS_NO_RESPONSE_HIGH;
    }

    if (!wait_level(gpio, pin, 0U, 120U, clk_hz)) {
        EnableGlobalIRQ(primask);
        return DHT11_STATUS_NO_RESPONSE_LOW;
    }

    /* ------------------------------------------------------------------
     * Step 4: Sample 40 Data Bits
     *   Each bit starts with ~50 us LOW, followed by:
     *     - 26 to 30 us HIGH = bit '0'
     *     - ~70 us HIGH       = bit '1'
     *   Threshold = 45 us
     * ---------------------------------------------------------------- */
    for (int byte_i = 0; byte_i < 5; ++byte_i) {
        uint8_t byte_val = 0U;
        for (int bit_i = 7; bit_i >= 0; --bit_i) {
            /* Wait for HIGH edge (bit start) */
            if (!wait_level(gpio, pin, 1U, 100U, clk_hz)) {
                EnableGlobalIRQ(primask);
                return DHT11_STATUS_BIT_TIMEOUT;
            }

            uint32_t t_start = MSDK_GetCpuCycleCount();

            /* Wait for LOW edge (bit end) */
            if (!wait_level(gpio, pin, 0U, 120U, clk_hz)) {
                EnableGlobalIRQ(primask);
                return DHT11_STATUS_BIT_TIMEOUT;
            }

            /* Measure pulse duration */
            if ((MSDK_GetCpuCycleCount() - t_start) > threshold_cycles) {
                byte_val |= (uint8_t)(1U << bit_i);
            }
        }
        data[byte_i] = byte_val;
    }

    EnableGlobalIRQ(primask);

    /* Pin stays in INPUT mode (idle HIGH via pull-up) */

    /* ------------------------------------------------------------------
     * Step 5: Checksum Validation
     *   (Byte0 + Byte1 + Byte2 + Byte3) & 0xFF == Byte4
     * ---------------------------------------------------------------- */
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

    /* 1. Try Primary D2: PTC0 */
    s_last_pin_str = "D2 (PTC0)";
    s_last_status = dht11_read_channel(DHT11_PRIMARY_GPIO, DHT11_PRIMARY_PIN, temperature_c, humidity_pct);
    if (s_last_status == DHT11_STATUS_OK) {
        return true;
    }

    SDK_DelayAtLeastUs(2000U, SystemCoreClock);

    /* 2. Try Alternate D2: PTC4 (via board solder short SH3) */
    dht11_status_t st_ptc4 = dht11_read_channel(DHT11_ALT_GPIO, DHT11_ALT_PIN, temperature_c, humidity_pct);
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
