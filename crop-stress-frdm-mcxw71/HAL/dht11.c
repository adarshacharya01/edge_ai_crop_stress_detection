/**
 * @file dht11.c
 * @brief Universal high-precision DHT11 single-wire bit-bang driver for FRDM-MCXW71.
 *
 * Scans all candidate Arduino header pins on J1:
 *   - D2: PTC4
 *   - D3: PTC1
 *   - D4: PTA19
 *   - D5: PTC2
 *   - D6: PTA20
 *   - D7: PTA21
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

typedef struct {
    const char *name;
    PORT_Type  *port;
    GPIO_Type  *gpio;
    uint32_t   pin;
    clock_ip_name_t clock_ip;
} dht11_pin_desc_t;

static const dht11_pin_desc_t k_dht11_pins[] = {
    {"D2 (J1 Pin 3 / PTC4)",  PORTC, GPIOC, 4U,  kCLOCK_PortC},
    {"D3 (J1 Pin 4 / PTC1)",  PORTC, GPIOC, 1U,  kCLOCK_PortC},
    {"D4 (J1 Pin 5 / PTA19)", PORTA, GPIOA, 19U, kCLOCK_PortA},
    {"D5 (J1 Pin 6 / PTC2)",  PORTC, GPIOC, 2U,  kCLOCK_PortC},
    {"D6 (J1 Pin 7 / PTA20)", PORTA, GPIOA, 20U, kCLOCK_PortA},
    {"D7 (J1 Pin 8 / PTA21)", PORTA, GPIOA, 21U, kCLOCK_PortA}
};

#define NUM_DHT11_PINS (sizeof(k_dht11_pins) / sizeof(k_dht11_pins[0]))

static dht11_status_t s_last_status  = DHT11_STATUS_IDLE_LOW;
static const char    *s_last_pin_str = "D2 (J1 Pin 3 / PTC4)";

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
    /* CRITICAL for MCXW71: Enable digital input buffer in PIDR! */
    GPIO_PortInputEnable(gpio, 1U << pin);
#endif

    /* Idle state: Output HIGH */
    gpio->PDDR |= (1U << pin);
    gpio->PSOR = (1U << pin);
}

void dht11_init(void)
{
    /* Enable port clocks */
    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_PortC);

    /* Initialize DWT cycle counter for microsecond timing */
    MSDK_EnableCpuCycleCounter();

    /* Configure all candidate pins */
    for (size_t i = 0; i < NUM_DHT11_PINS; ++i) {
        CLOCK_EnableClock(k_dht11_pins[i].clock_ip);
        config_pin_as_gpio(k_dht11_pins[i].port, k_dht11_pins[i].gpio, k_dht11_pins[i].pin);
    }
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

    /* Check if line is idle HIGH */
    gpio->PDDR &= ~(1U << pin);
    SDK_DelayAtLeastUs(10U, SystemCoreClock);
    if (((gpio->PDIR >> pin) & 1U) == 0U) {
        gpio->PDDR |= (1U << pin);
        gpio->PSOR = (1U << pin);
        return DHT11_STATUS_IDLE_LOW;
    }

    /* Step 1: Send Host Start Signal (pull LOW for 25 ms) */
    gpio->PDDR |= (1U << pin);
    gpio->PCOR = (1U << pin);
    SDK_DelayAtLeastUs(25000U, SystemCoreClock);

    /* Actively drive HIGH for 30 us to provide sharp rising edge */
    gpio->PSOR = (1U << pin);
    SDK_DelayAtLeastUs(30U, SystemCoreClock);

    /* Step 2: Critical section - switch to input */
    uint32_t primask = DisableGlobalIRQ();

    gpio->PDDR &= ~(1U << pin);

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

    /* Wait for DHT11 to pull LOW (start of data bit 0) */
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

    /* Scan candidate pins on J1 */
    for (size_t i = 0; i < NUM_DHT11_PINS; ++i) {
        dht11_status_t st = dht11_read_channel(k_dht11_pins[i].gpio, k_dht11_pins[i].pin, temperature_c, humidity_pct);
        if (st == DHT11_STATUS_OK) {
            s_last_pin_str = k_dht11_pins[i].name;
            s_last_status = DHT11_STATUS_OK;
            return true;
        }

        /* If this was D2 (the primary configured pin), record its failure status */
        if (i == 0) {
            s_last_status = st;
            s_last_pin_str = k_dht11_pins[0].name;
        }

        SDK_DelayAtLeastUs(2000U, SystemCoreClock);
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
