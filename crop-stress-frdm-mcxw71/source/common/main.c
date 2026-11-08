/*! *********************************************************************************
 * \addtogroup Main
 * @{
 ********************************************************************************** */
/*! *********************************************************************************
* Copyright 2021-2023 NXP
*
*
* \file
*
* This is the source file for the main entry point for a bare-metal application.
*
* SPDX-License-Identifier: BSD-3-Clause
********************************************************************************** */

/************************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ************************************************************************************/
#include "app.h"
#include "app_conn.h"
#include "fsl_os_abstraction.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "fsl_port.h"

#if defined(gAppLowpowerEnabled_d) && (gAppLowpowerEnabled_d > 0)
#include "PWR_Interface.h"
#endif

/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/
int main(void)
{
    /* Init OSA: should be called before any other OSA API */
    OSA_Init();

    BOARD_InitHardware();

    PRINTF("\r\n\r\n=========================================================\r\n");
    PRINTF("   FRDM-MCXW71 BLE CROP STRESS MONITOR - BOOT DIAGNOSTIC   \r\n");
    PRINTF("=========================================================\r\n");

    PRINTF("[BLE DIAG] Step 1: Initializing Application Services...\r\n");
    APP_InitServices();
    PRINTF("[BLE DIAG] Application Services initialized.\r\n");

    PRINTF("[BLE DIAG] Step 2: Initializing Platform & NBU BLE Controller...\r\n");
    int ble_status = APP_InitBle();
    if (ble_status != 0)
    {
        PRINTF("[BLE DIAG] ERROR: APP_InitBle() failed with status: %d!\r\n", ble_status);
    }
    else
    {
        PRINTF("[BLE DIAG] Controller & NBU Core Synced successfully (status=0).\r\n");
    }

#if (FSL_OSA_BM_TIMER_CONFIG != FSL_OSA_BM_TIMER_NONE)
    OSA_TimeInit();
#endif

    PRINTF("[BLE DIAG] Step 3: Initializing Bluetooth LE Host Stack...\r\n");
    BluetoothLEHost_AppInit();
    PRINTF("[BLE DIAG] BluetoothLEHost_AppInit() done.\r\n");

    PRINTF("[BLE DIAG] Step 4: Entering Main Loop (Dispatching BLE Messages & OSA Tasks)...\r\n");
#if (FSL_OSA_BM_TIMER_CONFIG != FSL_OSA_BM_TIMER_NONE)
    OSA_TimeInit();
#endif
    while (1)
    {
        BluetoothLEHost_HandleMessages();
        OSA_ProcessTasks();
    }

    /* Won't run here */
    assert(0);
    return 0;
}

/*! *********************************************************************************
 * @}
 ********************************************************************************** */
