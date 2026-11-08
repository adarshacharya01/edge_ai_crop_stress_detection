/**
 * @file dht11.c
 * @brief High-precision DHT11 single-wire bit-bang driver for FRDM-MCXW71.
 *
 * Pin: Arduino Header J1 Pin 3 (D2) -> MCU PTC0.
 *
 * Uses the Cortex-M33 DWT->CYCCNT (Cycle Counter) running at 96 MHz to
 * measure pulse widths with sub-microsecond precision.
 * Disables global interrupts during the 4ms transfer window to prevent
 * jitter from SysTick or peripherals.
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

#define DHT11_PORT          PORTC
#define DHT11_GPIO          GPIOC
#define DHT11_PIN           0U  /* PTC0 */
#define DHT11_CLOCK_GATE    kCLOCK_PortC

/* Direct register bit manipulations for maximum speed */
static inline void dht11_set_output(void)
{
    DHT11_GPIO->PDDR |= (1U << DHT11_PIN);
}

static inline void dht11_set_input(void)
{
    DHT11_GPIO->PDDR &= ~(1U << DHT11_PIN);
}

static inline void dht11_write_high(void)
{
    DHT11_GPIO->PSOR = (1U << DHT11_PIN);
}

static inline void dht11_write_low(void)
{
    DHT11_GPIO->PCOR = (1U << DHT11_PIN);
}

static inline uint32_t dht11_read_pin(void)
{
    return (DHT11_GPIO->PDIR >> DHT11_PIN) & 1U;
}

static inline uint32_t get_cycles(void)
{
    return DWT->CYCCNT;
}

static inline uint32_t us_to_cycles(uint32_t us)
{
    /* At 96 MHz, 1 us = 96 cycles */
    return us * (SystemCoreClock / 1000000U);
}

void dht11_init(void)
{
    /* 1. Enable Port C clock */
    CLOCK_EnableClock(DHT11_CLOCK_GATE);

    /* 2. Configure PTC0 as GPIO with internal pull-up enabled */
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
    PORT_SetPinConfig(DHT11_PORT, DHT11_PIN, &pin_cfg);

    /* 3. Enable DWT cycle counter if not already running */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 4. Default idle state: Output HIGH */
    dht11_set_output();
    dht11_write_high();
}

/**
 * @brief Wait for the pin to transition to `target_level` with a timeout.
 * @return true if target level reached before timeout, false on timeout.
 */
static bool wait_pin_level(uint32_t target_level, uint32_t timeout_us)
{
    uint32_t start = get_cycles();
    uint32_t timeout_cycles = us_to_cycles(timeout_us);

    while (dht11_read_pin() != target_level) {
        if ((get_cycles() - start) > timeout_cycles) {
            return false;
        }
    }
    return true;
}

bool dht11_read(float *temperature_c, float *humidity_pct)
{
    if (temperature_c == NULL || humidity_pct == NULL) {
        return false;
    }

    uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};
    uint32_t threshold_cycles = us_to_cycles(45U); /* ~45 us differentiates 0 (28us) from 1 (70us) */

    /* ------------------------------------------------------------------
     * Step 1: Send Host Start Signal
     *   Pull LOW for 20 ms, then pull HIGH for 30 us, then switch to input.
     * ---------------------------------------------------------------- */
    dht11_set_output();
    dht11_write_low();
    SDK_DelayAtLeastUs(20000U, SystemCoreClock); /* 20 ms */

    dht11_write_high();
    SDK_DelayAtLeastUs(30U, SystemCoreClock);    /* 30 us */

    /* ------------------------------------------------------------------
     * Step 2: Critical Section - Read Response and 40 Data Bits
     * ---------------------------------------------------------------- */
    uint32_t primask = DisableGlobalIRQ();

    dht11_set_input();

    /* DHT11 response: pulls LOW for ~80 us, then HIGH for ~80 us */
    if (!wait_pin_level(0U, 120U)) {
        EnableGlobalIRQ(primask);
        dht11_set_output();
        dht11_write_high();
        return false;
    }

    if (!wait_pin_level(1U, 120U)) {
        EnableGlobalIRQ(primask);
        dht11_set_output();
        dht11_write_high();
        return false;
    }

    if (!wait_pin_level(0U, 120U)) {
        EnableGlobalIRQ(primask);
        dht11_set_output();
        dht11_write_high();
        return false;
    }

    /* Read 40 bits (5 bytes) */
    for (int byte_i = 0; byte_i < 5; ++byte_i) {
        uint8_t byte_val = 0U;
        for (int bit_i = 7; bit_i >= 0; --bit_i) {
            /* Each bit starts with ~50 us LOW */
            if (!wait_pin_level(1U, 100U)) {
                EnableGlobalIRQ(primask);
                dht11_set_output();
                dht11_write_high();
                return false;
            }

            /* Measure length of the HIGH pulse */
            uint32_t t_start = get_cycles();

            if (!wait_pin_level(0U, 120U)) {
                EnableGlobalIRQ(primask);
                dht11_set_output();
                dht11_write_high();
                return false;
            }

            uint32_t duration = get_cycles() - t_start;
            if (duration > threshold_cycles) {
                byte_val |= (uint8_t)(1U << bit_i);
            }
        }
        data[byte_i] = byte_val;
    }

    EnableGlobalIRQ(primask);

    /* Restore pin to output HIGH (idle) */
    dht11_set_output();
    dht11_write_high();

    /* ------------------------------------------------------------------
     * Step 3: Checksum Verification
     * ---------------------------------------------------------------- */
    uint8_t checksum = (uint8_t)((data[0] + data[1] + data[2] + data[3]) & 0xFFU);
    if (checksum != data[4] || (data[0] == 0U && data[2] == 0U)) {
        return false;
    }

    *humidity_pct  = (float)data[0];
    *temperature_c = (float)data[2];

    return true;
}
