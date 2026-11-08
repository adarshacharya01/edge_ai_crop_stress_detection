/**
 * @file dht11.c
 * @brief High-precision DHT11 single-wire bit-bang driver for FRDM-MCXW71.
 *
 * Supported header pins:
 *   - J1 Pin 3 = D2 (MCU PTC4, net CAN0_TX via SH3)
 *   - J1 Pin 5 = D4 (MCU PTA19, net TPM/RGB_G via SH5)
 *
 * The driver automatically detects which pin has the DHT11 connected.
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

/* Primary pin: J1 Pin 3 (D2) = PTC4 */
#define DHT11_D2_PORT       PORTC
#define DHT11_D2_GPIO       GPIOC
#define DHT11_D2_PIN        4U
#define DHT11_D2_CLOCK      kCLOCK_PortC

/* Secondary pin: J1 Pin 5 (D4) = PTA19 */
#define DHT11_D4_PORT       PORTA
#define DHT11_D4_GPIO       GPIOA
#define DHT11_D4_PIN        19U
#define DHT11_D4_CLOCK      kCLOCK_PortA

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

    /* Idle state: Output HIGH */
    gpio->PDDR |= (1U << pin);
    gpio->PSOR = (1U << pin);
}

void dht11_init(void)
{
    /* Enable port clocks */
    CLOCK_EnableClock(DHT11_D2_CLOCK);
    CLOCK_EnableClock(DHT11_D4_CLOCK);

    /* Initialize DWT cycle counter for microsecond timing */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* Configure both D2 (PTC4) and D4 (PTA19) */
    config_pin_as_gpio(DHT11_D2_PORT, DHT11_D2_GPIO, DHT11_D2_PIN);
    config_pin_as_gpio(DHT11_D4_PORT, DHT11_D4_GPIO, DHT11_D4_PIN);
}

static bool wait_level(GPIO_Type *gpio, uint32_t pin, uint32_t target_level, uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t timeout_cycles = us_to_cycles(timeout_us);

    while (((gpio->PDIR >> pin) & 1U) != target_level) {
        if ((DWT->CYCCNT - start) > timeout_cycles) {
            return false;
        }
    }
    return true;
}

static bool dht11_read_channel(GPIO_Type *gpio, uint32_t pin, float *temperature_c, float *humidity_pct)
{
    uint8_t data[5] = {0U, 0U, 0U, 0U, 0U};
    uint32_t threshold_cycles = us_to_cycles(45U);

    /* Step 1: Send Host Start Signal (pull LOW for 20 ms, then HIGH for 30 us) */
    gpio->PDDR |= (1U << pin);  /* Output */
    gpio->PCOR = (1U << pin);   /* LOW */
    SDK_DelayAtLeastUs(20000U, SystemCoreClock);

    gpio->PSOR = (1U << pin);   /* HIGH */
    SDK_DelayAtLeastUs(30U, SystemCoreClock);

    /* Step 2: Critical section - switch to input and sample */
    uint32_t primask = DisableGlobalIRQ();

    gpio->PDDR &= ~(1U << pin); /* Input */

    /* Wait for DHT11 response: LOW ~80 us, then HIGH ~80 us */
    if (!wait_level(gpio, pin, 0U, 150U) ||
        !wait_level(gpio, pin, 1U, 150U) ||
        !wait_level(gpio, pin, 0U, 150U)) {
        EnableGlobalIRQ(primask);
        gpio->PDDR |= (1U << pin);
        gpio->PSOR = (1U << pin);
        return false;
    }

    /* Read 40 data bits */
    for (int byte_i = 0; byte_i < 5; ++byte_i) {
        uint8_t byte_val = 0U;
        for (int bit_i = 7; bit_i >= 0; --bit_i) {
            if (!wait_level(gpio, pin, 1U, 120U)) {
                EnableGlobalIRQ(primask);
                gpio->PDDR |= (1U << pin);
                gpio->PSOR = (1U << pin);
                return false;
            }

            uint32_t t_start = DWT->CYCCNT;

            if (!wait_level(gpio, pin, 0U, 140U)) {
                EnableGlobalIRQ(primask);
                gpio->PDDR |= (1U << pin);
                gpio->PSOR = (1U << pin);
                return false;
            }

            if ((DWT->CYCCNT - t_start) > threshold_cycles) {
                byte_val |= (uint8_t)(1U << bit_i);
            }
        }
        data[byte_i] = byte_val;
    }

    EnableGlobalIRQ(primask);

    /* Restore idle state */
    gpio->PDDR |= (1U << pin);
    gpio->PSOR = (1U << pin);

    /* Verify checksum */
    uint8_t checksum = (uint8_t)((data[0] + data[1] + data[2] + data[3]) & 0xFFU);
    if (checksum != data[4] || (data[0] == 0U && data[2] == 0U)) {
        return false;
    }

    *humidity_pct  = (float)data[0];
    *temperature_c = (float)data[2];
    return true;
}

bool dht11_read(float *temperature_c, float *humidity_pct)
{
    if (temperature_c == NULL || humidity_pct == NULL) {
        return false;
    }

    /* 1. Try J1 Pin 3 (D2 / PTC4) */
    if (dht11_read_channel(DHT11_D2_GPIO, DHT11_D2_PIN, temperature_c, humidity_pct)) {
        return true;
    }

    /* Brief recovery time */
    SDK_DelayAtLeastUs(2000U, SystemCoreClock);

    /* 2. Try J1 Pin 5 (D4 / PTA19) */
    if (dht11_read_channel(DHT11_D4_GPIO, DHT11_D4_PIN, temperature_c, humidity_pct)) {
        return true;
    }

    return false;
}
