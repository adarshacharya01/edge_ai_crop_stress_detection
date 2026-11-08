/**
 * @file dht11.c
 * @brief High-precision DHT11 driver for FRDM-MCXW71.
 *
 * Implements strict hardware specifications:
 *   1. PIN CONFIGURATION (PTC0 / D2 & PTC4):
 *      - Muxed as GPIO (kPORT_MuxAsGpio).
 *      - Internal pull-up enabled (kPORT_PullUp).
 *      - Push-pull driver for active low/high start pulses.
 *      - Input buffer unmasked in PIDR via GPIO_PortInputEnable().
 *   2. IDLE & START SEQUENCE:
 *      - Start pulse: Output LOW for 20 ms, drive HIGH for 30 us, switch to INPUT.
 *   3. HARDWARE DWT TIMING & TIMEOUTS:
 *      - Uses Cortex-M33 DWT->CYCCNT hardware cycle counter.
 *      - Disables global interrupts during 40-bit transmission window.
 *      - Explicit wait timeouts (120 us) for responses and bit pulses (>45 us = 1, <35 us = 0).
 *   4. DATA VALIDATION:
 *      - 8-bit checksum verification: (B0 + B1 + B2 + B3) & 0xFF == B4.
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

/* Primary D2 Pin: PTC0 (MCU Port C, Pin 0 / Arduino Header D2) */
#define DHT11_PTC0_PORT          PORTC
#define DHT11_PTC0_GPIO          GPIOC
#define DHT11_PTC0_PIN           0U

/* Alternate D2 Pin: PTC4 (MCU Port C, Pin 4 via Header J1 Pin 3) */
#define DHT11_PTC4_PORT          PORTC
#define DHT11_PTC4_GPIO          GPIOC
#define DHT11_PTC4_PIN           4U

/* Fallback D4 Pin: PTA19 (MCU Port A, Pin 19 via Header J1 Pin 5) */
#define DHT11_PTA19_PORT         PORTA
#define DHT11_PTA19_GPIO         GPIOA
#define DHT11_PTA19_PIN          19U

static dht11_status_t s_last_status  = DHT11_STATUS_IDLE_LOW;
static const char    *s_last_pin_str = "D2 (PTC0)";

static inline uint32_t us_to_cycles(uint32_t us, uint32_t clk_hz)
{
    return us * (clk_hz / 1000000U);
}

/**
 * @brief Configure pin as GPIO with internal pull-up and input enabled.
 */
static void config_pin_dht11(PORT_Type *port, GPIO_Type *gpio, uint32_t pin)
{
    const port_pin_config_t pin_cfg = {
        .pullSelect          = (uint16_t)kPORT_PullUp,
        .pullValueSelect     = (uint16_t)kPORT_LowPullResistor,
        .slewRate            = (uint16_t)kPORT_FastSlewRate,
        .passiveFilterEnable = (uint16_t)kPORT_PassiveFilterDisable,
        .openDrainEnable     = (uint16_t)kPORT_OpenDrainDisable, /* Push-pull for active start pulse */
        .driveStrength       = (uint16_t)kPORT_LowDriveStrength,
        .driveStrength1      = (uint16_t)kPORT_NormalDriveStrength,
        .mux                 = (uint16_t)kPORT_MuxAsGpio,
        .lockRegister        = (uint16_t)kPORT_UnlockRegister
    };
    PORT_SetPinConfig(port, pin, &pin_cfg);

#if defined(FSL_FEATURE_GPIO_HAS_PORT_INPUT_CONTROL) && FSL_FEATURE_GPIO_HAS_PORT_INPUT_CONTROL
    /* Unmask digital input buffer in PIDR */
    GPIO_PortInputEnable(gpio, 1U << pin);
#endif

    /* Default idle state: INPUT with internal pull-up */
    gpio->PDDR &= ~(1U << pin);
    gpio->PSOR = (1U << pin);
}

void dht11_init(void)
{
    /* 1. Enable PORT and GPIO peripheral clocks for Port C and Port A */
    CLOCK_EnableClock(kCLOCK_PortC);
    CLOCK_EnableClock(kCLOCK_GpioC);
    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_GpioA);

    /* 2. Initialize Cortex-M33 DWT cycle counter for sub-microsecond timing */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    MSDK_EnableCpuCycleCounter();

    /* 3. Configure pins with internal pull-up and unmasked input buffers */
    config_pin_dht11(DHT11_PTC0_PORT, DHT11_PTC0_GPIO, DHT11_PTC0_PIN);
    config_pin_dht11(DHT11_PTC4_PORT, DHT11_PTC4_GPIO, DHT11_PTC4_PIN);
    config_pin_dht11(DHT11_PTA19_PORT, DHT11_PTA19_GPIO, DHT11_PTA19_PIN);
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
     * Step 1: Start Pulse Sequence
     *   1. Switch to OUTPUT, drive LOW (0U) for 20 ms
     *   2. Drive HIGH (1U) for 30 us
     *   3. Switch to INPUT with pull-up
     * ---------------------------------------------------------------- */
    gpio->PCOR = (1U << pin);
    gpio->PDDR |= (1U << pin);  /* Output LOW */
    SDK_DelayAtLeastUs(20000U, clk_hz);

    gpio->PSOR = (1U << pin);   /* Actively drive HIGH */
    SDK_DelayAtLeastUs(30U, clk_hz);

    /* Critical Section: disable interrupts during data reception */
    uint32_t primask = DisableGlobalIRQ();

    gpio->PDDR &= ~(1U << pin); /* Switch to INPUT */

    /* ------------------------------------------------------------------
     * Step 2: Sensor Acknowledgment Response
     *   - Wait for sensor to pull line LOW (80 us response, 140 us timeout)
     *   - Wait for sensor to pull line HIGH (80 us response, 140 us timeout)
     *   - Wait for sensor to pull line LOW (start of data bit 0)
     * ---------------------------------------------------------------- */
    if (!wait_level(gpio, pin, 0U, 140U, clk_hz)) {
        EnableGlobalIRQ(primask);
        return DHT11_STATUS_NO_RESPONSE_LOW;
    }

    if (!wait_level(gpio, pin, 1U, 140U, clk_hz)) {
        EnableGlobalIRQ(primask);
        return DHT11_STATUS_NO_RESPONSE_HIGH;
    }

    if (!wait_level(gpio, pin, 0U, 140U, clk_hz)) {
        EnableGlobalIRQ(primask);
        return DHT11_STATUS_NO_RESPONSE_LOW;
    }

    /* ------------------------------------------------------------------
     * Step 3: Sample 40 Data Bits
     *   Threshold = 45 us (>45 us = '1', <35 us = '0')
     * ---------------------------------------------------------------- */
    for (int byte_i = 0; byte_i < 5; ++byte_i) {
        uint8_t byte_val = 0U;
        for (int bit_i = 7; bit_i >= 0; --bit_i) {
            /* Wait for bit start (HIGH pulse) */
            if (!wait_level(gpio, pin, 1U, 100U, clk_hz)) {
                EnableGlobalIRQ(primask);
                return DHT11_STATUS_BIT_TIMEOUT;
            }

            uint32_t t_start = MSDK_GetCpuCycleCount();

            /* Wait for bit end (LOW pulse) */
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

    /* ------------------------------------------------------------------
     * Step 4: Checksum Validation
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

    /* 1. Try PTC0 FIRST (Arduino Header D2 / Primary DHT11 Pin) */
    s_last_pin_str = "D2 (PTC0)";
    s_last_status = dht11_read_channel(DHT11_PTC0_GPIO, DHT11_PTC0_PIN, temperature_c, humidity_pct);
    if (s_last_status == DHT11_STATUS_OK) {
        return true;
    }

    SDK_DelayAtLeastUs(2000U, SystemCoreClock);

    /* 2. Try PTC4 (Alternate D2 route) */
    dht11_status_t st_ptc4 = dht11_read_channel(DHT11_PTC4_GPIO, DHT11_PTC4_PIN, temperature_c, humidity_pct);
    if (st_ptc4 == DHT11_STATUS_OK) {
        s_last_pin_str = "D2 (J1 Pin 3 / PTC4)";
        s_last_status = DHT11_STATUS_OK;
        return true;
    }

    SDK_DelayAtLeastUs(2000U, SystemCoreClock);

    /* 3. Try PTA19 (Header J1 Pin 5 / D4) */
    dht11_status_t st_d4 = dht11_read_channel(DHT11_PTA19_GPIO, DHT11_PTA19_PIN, temperature_c, humidity_pct);
    if (st_d4 == DHT11_STATUS_OK) {
        s_last_pin_str = "D4 (J1 Pin 5 / PTA19)";
        s_last_status = DHT11_STATUS_OK;
        return true;
    }

    return false;
}

static float s_cached_live_temp = 26.2f;
static float s_cached_live_hum  = 58.0f;
static bool  s_has_valid_dht    = false;

int DHT11_Read(float *humidity_pct, float *temperature_c)
{
    float t = 0.0f, h = 0.0f;
    bool success = dht11_read(&t, &h);
    if (success) {
        s_cached_live_temp = t;
        s_cached_live_hum  = h;
        s_has_valid_dht    = true;
        if (temperature_c) *temperature_c = t;
        if (humidity_pct)  *humidity_pct  = h;
        return 0; /* OK */
    }

    /* Fallback to last valid reading if single read dropped */
    if (temperature_c) *temperature_c = s_cached_live_temp;
    if (humidity_pct)  *humidity_pct  = s_cached_live_hum;
    return s_has_valid_dht ? 0 : -1;
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
