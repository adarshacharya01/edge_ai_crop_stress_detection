/*! *********************************************************************************
 * \addtogroup Wireless UART Application
 * @{
 ********************************************************************************** */
/*! *********************************************************************************
* Copyright 2015 Freescale Semiconductor, Inc.
* Copyright 2016-2024 NXP
*
*
* \file
*
* This file is the source file for the Wireless UART Application
*
* SPDX-License-Identifier: BSD-3-Clause
********************************************************************************** */

/************************************************************************************
 *************************************************************************************
 * Include
 *************************************************************************************
 ************************************************************************************/
/* Framework / Drivers */
#include "EmbeddedTypes.h"
#include "RNG_Interface.h"
#include "fsl_component_button.h"
#include "fsl_component_led.h"
#include "fsl_component_timer_manager.h"
#include "fsl_component_panic.h"
#include "fsl_component_serial_manager.h"
#include "fsl_component_mem_manager.h"
#include "fsl_format.h"
#include "fsl_debug_console.h"
#include "app.h"
#include "FunctionLib.h"
#include "board.h"
#include "fwk_platform_ble.h"
#include "dht11.h"
#include "board_adc.h"
#include "crop_stress.h"
#include <stdio.h>

/* BLE Host Stack */
#include "gatt_interface.h"
#include "gatt_server_interface.h"
#include "gatt_client_interface.h"
#include "gatt_database.h"
#include "gap_interface.h"
#include "gatt_db_app_interface.h"
#include "gatt_db_handles.h"
#include "gap_types.h"

/* Profile / Services */
#include "wireless_uart_interface.h"
#include "battery_interface.h"
/* Wrappers */
#include "ble_conn_manager.h"
#include "ble_service_discovery.h"

#include "board.h"
#include "board_comp.h"
#include "wireless_uart.h"
#include "issdk_hal.h"
#include "fxls8974_drv.h"

#if defined(K32W232H_SERIES) || defined(KW45B41Z82_SERIES) || defined(KW45B41Z83_SERIES) || defined(K32W1480_SERIES) || \
    defined(KW47B42ZB7_cm33_core0_SERIES) || defined(KW47B42ZB6_cm33_core0_SERIES) || defined(KW47B42ZB3_cm33_core0_SERIES) || \
    defined(KW47B42ZB2_cm33_core0_SERIES) || defined(KW47B42Z97_cm33_core0_SERIES) || defined(KW47B42Z96_cm33_core0_SERIES) || \
    defined(KW47B42Z83_cm33_core0_SERIES) || defined(MCXW716C_SERIES) || defined(MCXW716A_SERIES)
#include "sensors.h"
#endif

#if defined(gUseControllerNotificationsCallback_c) && (gUseControllerNotificationsCallback_c)
    #include "controller_interface.h"
#endif

#include "app_conn.h"
#include "app_scanner.h"
#include "app_advertiser.h"
#include "board.h"
#include "app.h"

#include "fsl_lpadc.h"
#include "fsl_lpit.h"
#include "fsl_trgmux.h"
#include "fsl_vref.h"
#include "fsl_adapter_pwm.h"

/************************************************************************************
 *************************************************************************************
 * Private macros
 *************************************************************************************
 ************************************************************************************/

#define mAppUartBufferSize_c            gAttMaxWriteDataSize_d(gAttMaxMtu_c) /* Local Buffer Size */

#define mAppUartFlushIntervalInMs_c     (7)     /* Flush Timeout in Ms */

#define mBatteryLevelReportInterval_c   (10)    /* battery level report interval in seconds  */

#define mAccelReportInterval_c   		(200000)    /* accel measurement report interval in u-seconds  */

#define gAllowToBlock_d                 (TRUE)
#define gNoBlock_d                      (FALSE)
#define Serial_Print(a,b)               PRINTF("%s", (const char *)(a))
#define Serial_PrintDec(a)              PRINTF("%d", (int)(a))
#define Serial_PrintHex(a)              PRINTF("%X", (unsigned int)(a))


#define FXLS8974_DATA_SIZE 6
#define mStrXOffset_c 5U
#define mStrYOffset_c 16U
#define mStrZOffset_c 27U

#define DEMO_VREF_BASE VREF0

#define DEMO_LPIT_BASE       LPIT0

/* Get source clock for LPIT driver */
#define LPIT_SOURCECLOCK (CLOCK_GetFreq(kCLOCK_ScgSircClk))

/*Sample Frequency in u-seconds */
#define LIGHT_SENSOR_FS 1666U
/* Decimation - Down sampling */
#define LIGHT_SENSOR_DISP_DS 12U

#define DEMO_LPADC_BASE             ADC0
#define DEMO_LPADC_USER_CHANNEL     	2U
#define DEMO_LPADC_USER_CHANNEL_MODE 	kLPADC_SampleChannelSingleEndSideB
#define DEMO_LPADC_USER_CMDID       1U
#define FIFO_WATHER_MARK			5U
#define DEMO_LPADC_IRQn             ADC0_IRQn
#define DEMO_LPADC_IRQ_HANDLER_FUNC ADC0_IRQHandler

#define IS_LOWER_CASE(ptr) ( ((int8_t)*ptr >= 'a') && ( *ptr <= 'z'))
#define UPER_CASE(ptr) (*ptr -= 32)

#define COMMAND_LEN 6U

#define SCALE_MEASUREMENT_RANGE 1024U


/*Define led handle*/
#if 0

#if (defined(gAppLedCnt_c) && (gAppLedCnt_c > 0))
LED_HANDLE_ARRAY_DEFINE(g_ledHandle, gAppLedCnt_c);
#endif /*gAppLedCnt_c > 0*/

#if (defined(gAppLedCnt_c) && (gAppLedCnt_c == 2))
#define gBoardLedMonochromeHdl ((led_handle_t)g_ledHandle[0])
#define gBoardLedRgbHdl        ((led_handle_t)g_ledHandle[1])
#endif


#endif
/************************************************************************************
 *************************************************************************************
 * Private type definitions
 *************************************************************************************
 ************************************************************************************/
typedef enum appEvent_tag
{
    mAppEvt_PeerConnected_c,
    mAppEvt_PairingComplete_c,
    mAppEvt_ServiceDiscoveryComplete_c,
    mAppEvt_ServiceDiscoveryNotFound_c,
    mAppEvt_ServiceDiscoveryFailed_c,
    mAppEvt_GattProcComplete_c,
    mAppEvt_GattProcError_c
} appEvent_t;

typedef enum appState_tag
{
    mAppIdle_c,
    mAppExchangeMtu_c,
    mAppServiceDisc_c,
    mAppServiceDiscRetry_c,
    mAppRunning_c
} appState_t;

typedef struct appPeerInfo_tag
{
    deviceId_t  deviceId;
    bool_t      isBonded;
    wucConfig_t clientInfo;
    appState_t  appState;
    gapRole_t   gapRole;
} appPeerInfo_t;

typedef struct advState_tag
{
    bool_t advOn;
} advState_t;


typedef enum
{
	sm_measurement_off = 1U,
	sm_measurement_accel,
	sm_measurement_light,

}sm_measurement_t;
/************************************************************************************
 *************************************************************************************
 * Private functions prototypes
 *************************************************************************************
 ************************************************************************************/
static void BleApp_Start(gapRole_t gapRole);
#if gWuart_PeripheralRole_c == 1
static void BleApp_AdvertisingCallback(gapAdvertisingEvent_t *pAdvertisingEvent);
#endif /* gWuart_PeripheralRole_c */
#if gWuart_CentralRole_c == 1
static void BleApp_ScanningCallback(gapScanningEvent_t *pScanningEvent);
#endif /* gWuart_CentralRole_c */
static void BleApp_ConnectionCallback(deviceId_t peerDeviceId, gapConnectionEvent_t *pConnectionEvent);
#if gWuart_CentralRole_c == 1
static bool_t BleApp_CheckScanEvent(gapScannedDevice_t *pData);
#endif /* gWuart_CentralRole_c */
/* Gatt and Att callbacks */
static void BleApp_GattServerCallback(deviceId_t deviceId, gattServerEvent_t *pServerEvent);
static void BleApp_GattClientCallback(deviceId_t serverDeviceId, gattProcedureType_t procedureType, gattProcedureResult_t   procedureResult, bleResult_t error);
static void BleApp_ServiceDiscoveryCallback(deviceId_t peerDeviceId, servDiscEvent_t *pEvent);
static void BleApp_StateMachineHandler(deviceId_t peerDeviceId, appEvent_t event);
static void BleApp_StoreServiceHandles(deviceId_t peerDeviceId, gattService_t *pService);

/* Timer Callbacks */
#if gWuart_CentralRole_c == 1
static void ScanningTimerCallback(void *pParam);
#endif /* gWuart_CentralRole_c */
static void UartStreamFlushTimerCallback(void *pData);
static void BatteryMeasurementTimerCallback(void *pParam);
static void AccelMeasurementTimerCallback(void *pParam);
static void CropTelemetryTimerCallback(void *pParam);

static void SM_Sensor_Handler(sm_measurement_t next_state);

#if (defined(gAppButtonCnt_c) && (gAppButtonCnt_c == 1))
static void SwitchPressTimerCallback(void *pParam);
#endif

/* Uart Tx/Rx Callbacks*/
static void Uart_RxCallBack(void *pData, serial_manager_callback_message_t *pMessage, serial_manager_status_t status);
static void Uart_TxCallBack(void *pBuffer, serial_manager_callback_message_t *pMessage, serial_manager_status_t status);

static void BleApp_FlushUartStream(void *pParam);
static void BleApp_ReceivedUartStream(deviceId_t peerDeviceId, uint8_t *pStream, uint16_t streamLength);

#if defined(gUseControllerNotifications_c) && (gUseControllerNotifications_c)
static void BleApp_HandleControllerNotification(bleNotificationEvent_t *pNotificationEvent);

#if defined(gUseControllerNotificationsCallback_c) && (gUseControllerNotificationsCallback_c)
static void BleApp_ControllerNotificationCallback(bleCtrlNotificationEvent_t *pNotificationEvent);
#endif
#endif

static void BleApp_SerialInit(void);
static void BleApp_AccelInit(void);
static void BleApp_LighSensorInit(void);
static void BluetoothLEHost_Initialized(void);
static void BluetoothLEHost_GenericCallback(gapGenericEvent_t *pGenericEvent);

#if (defined(gAppButtonCnt_c) && (gAppButtonCnt_c > 0))
static button_status_t BleApp_HandleKeys0(void *pButtonHandle, button_callback_message_t *pMessage, void *pCallbackParam);
#endif /*gAppButtonCnt_c > 0*/

#if (gWuart_CentralRole_c == 1) && (gWuart_PeripheralRole_c == 1)
#if (defined(gAppButtonCnt_c) && (gAppButtonCnt_c > 1))
static button_status_t BleApp_HandleKeys1(void *pButtonHandle, button_callback_message_t *pMessage, void *pCallbackParam);
#endif
#endif

void Str_UperCaseFormat(uint8_t* ptr);
/************************************************************************************
 *************************************************************************************
 * Private memory declarations
 *************************************************************************************
 ************************************************************************************/
static appPeerInfo_t maPeerInformation[gAppMaxConnections_c];
static gapRole_t mGapRole;
static deviceId_t mcActiveConnNo;
/* Adv Parameters */
#if gWuart_PeripheralRole_c == 1
static advState_t mAdvState;
#endif /* gWuart_PeripheralRole_c */
#if gWuart_CentralRole_c == 1
static bool_t mScanningOn = FALSE;
#endif /* gWuart_CentralRole_c */

static uint16_t mCharMonitoredHandles[1] = { (uint16_t)value_uart_stream };

/* Service Data*/
static wusConfig_t mWuServiceConfig;
static bool_t      mBasValidClientList[gAppMaxConnections_c] = {FALSE};
static basConfig_t mBasServiceConfig = {(uint16_t)service_battery, 0, mBasValidClientList, gAppMaxConnections_c};

static TIMER_MANAGER_HANDLE_DEFINE(mAppTimerId);
static TIMER_MANAGER_HANDLE_DEFINE(mUartStreamFlushTimerId);
static TIMER_MANAGER_HANDLE_DEFINE(mBatteryMeasurementTimerId);
static TIMER_MANAGER_HANDLE_DEFINE(mAccelMeasurementTimerId);
static TIMER_MANAGER_HANDLE_DEFINE(mCropTelemetryTimerId);

/* If the board has only one button, multiplex the required functionalities on it using an application timer */
#if (gAppButtonCnt_c == 1)
static TIMER_MANAGER_HANDLE_DEFINE(mSwitchPressTimerId);
#endif


/*wireless uart write handle*/
static SERIAL_MANAGER_WRITE_HANDLE_DEFINE(s_writeHandle);
/*wireless uart read handle*/
static SERIAL_MANAGER_READ_HANDLE_DEFINE(s_readHandle);

static uint16_t mAppUartBufferSize = mAppUartBufferSize_c;
static volatile bool_t mAppUartNewLine = FALSE;
static volatile bool_t mAppDapaPending = FALSE;

#if (defined(gAppButtonCnt_c) && (gAppButtonCnt_c == 1))
static uint8_t mSwitchPressCnt = 0;
#endif

#if (gWuart_CentralRole_c == 1)
static appScanningParams_t mAppScanParams = {
    &gScanParams,
    gGapDuplicateFilteringEnable_c,
    gGapScanContinuously_d,
    gGapScanPeriodicDisabled_d
};
#endif

#if (gWuart_PeripheralRole_c == 1)
static appAdvertisingParams_t mAppAdvParams = {
    &gAdvParams,
    &gAppAdvertisingData,
    &gAppScanRspData
};
#endif

/*! @brief Defines the register write list to configure FXLS8974 in Interrupt mode. */
const registerwritelist_t cFxls8974ConfigNormal[] = {
		/*! Set Full-scale range as 4G. */
		{FXLS8974_SENS_CONFIG1, FXLS8974_SENS_CONFIG1_FSR_2G, FXLS8974_SENS_CONFIG1_FSR_MASK},
		/*! Clear SENS_CONFIG2 */
		{FXLS8974_SENS_CONFIG2, 0x00, 0x00},
		/*! Disable Self-Test. */
		{FXLS8974_SENS_CONFIG1, FXLS8974_SENS_CONFIG1_ST_AXIS_SEL_DISABLED, FXLS8974_SENS_CONFIG1_ST_AXIS_SEL_MASK},
		/*! Set Wake Mode ODR Rate as 12.5Hz. */
		{FXLS8974_SENS_CONFIG3, FXLS8974_SENS_CONFIG3_WAKE_ODR_12_5HZ, FXLS8974_SENS_CONFIG3_WAKE_ODR_MASK},
		/*! Enable Interrupts for Data Ready Events. */
		{FXLS8974_INT_EN, FXLS8974_INT_EN_DRDY_EN_EN, FXLS8974_INT_EN_DRDY_EN_MASK},
		/*! Enable Temperature sensor. */
		{FXLS8974_SENS_CONFIG2,FXLS8974_SENS_CONFIG2_ANIC_TEMP_EN,FXLS8974_SENS_CONFIG2_ANIC_TEMP_MASK},
		/*! Set Self-Test ODR to 100 Hz. */
		{0x38,0x05,0x00},
		{0x2F,0x38,0x00},
		{0x30,0xD8,0x00},
		{0x33,0xC0,0x00},
		{0x34,0xFF,0x00},
		{0x35,0x40,0x00},
		{FXLS8974_SENS_CONFIG1, FXLS8974_SENS_CONFIG1_ACTIVE_ACTIVE, FXLS8974_SENS_CONFIG1_ACTIVE_MASK},
		__END_WRITE_DATA__};


/*! @brief FXLS8974 register list to read all registers */
const registerreadlist_t FXLS8974_ACCEL_REGS_READ[] = {{.readFrom = FXLS8974_OUT_X_LSB, .numBytes = FXLS8974_DATA_SIZE},
		__END_READ_DATA__};


static const uint8_t s_accel_msg[] = {"\rx =      , y =      , z =      \n"};
static const uint8_t s_msg_offset[] = {mStrXOffset_c, mStrYOffset_c, mStrZOffset_c};

static fxls8974_i2c_sensorhandle_t fxls8974Driver;


/* Measurement state */
static volatile sm_measurement_t msmt_state = sm_measurement_off;

static const uint8_t s_light_msg[] = {"\rLight:    %\n"};
static const uint8_t s_light_msg_offset = 8U;

static lpadc_conv_result_t g_LpadcResultConfigStruct;
static uint8_t down_sampling_cnt = LIGHT_SENSOR_DISP_DS;
extern const uint32_t g_LpadcFullRange;

static const sm_measurement_t c_states_measurement [] = {
		sm_measurement_off,
		sm_measurement_accel,
		sm_measurement_light
};

static const uint8_t* c_command_msgs[] =
{
		"SENS L",
		"SENS A"
};

static HAL_PWM_HANDLE_DEFINE(halPwmHandle_rgb);

/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/

/*! *********************************************************************************
* \brief  This is the initialization function for each application. This function
*         should contain all the initialization code required by the bluetooth demo
********************************************************************************** */
void BluetoothLEHost_AppInit(void)
{
    BleApp_SerialInit();

#if (defined(gAppButtonCnt_c) && (gAppButtonCnt_c > 0))
    (void)BUTTON_InstallCallback((button_handle_t)g_buttonHandle[0], BleApp_HandleKeys0, NULL);

#if (gWuart_CentralRole_c == 1) && (gWuart_PeripheralRole_c == 1)
#if (defined(gAppButtonCnt_c) && (gAppButtonCnt_c > 1))
    (void)BUTTON_InstallCallback((button_handle_t)g_buttonHandle[1], BleApp_HandleKeys1, NULL);
#endif
#endif
#endif

    /* Initialize Bluetooth Host Stack */
    BluetoothLEHost_SetGenericCallback(BluetoothLEHost_GenericCallback);
    BluetoothLEHost_Init(BluetoothLEHost_Initialized);
}

/************************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
************************************************************************************/

#if (defined(gAppButtonCnt_c) && (gAppButtonCnt_c > 0))
/*! *********************************************************************************
* \brief        Handler for the first key.
*
* \param[in]    pButtonHandle       Pointer to the button handle.
* \param[in]    pMessage            Pointer to the message.
* \param[in]    pCallbackParam      Pointer to the callback parameters.
********************************************************************************** */
static button_status_t BleApp_HandleKeys0
(
    void *pButtonHandle,
    button_callback_message_t *pMessage,
    void *pCallbackParam
)
{
    uint8_t mPeerId = 0;

    switch (pMessage->event)
    {
        case kBUTTON_EventOneClick:
        case kBUTTON_EventShortPress:
        {
#if (1 == gAppButtonCnt_c)
            /* increment the switch press counter */
            mSwitchPressCnt++;

            if(FALSE == TM_IsTimerActive(mSwitchPressTimerId))
            {
                /* Start the switch press timer */
                (void)TM_InstallCallback((timer_handle_t)mSwitchPressTimerId, SwitchPressTimerCallback, NULL);
                (void)TM_Start((timer_handle_t)mAppTimerId, (uint8_t)kTimerModeSingleShot | (uint8_t)kTimerModeLowPowerTimer, gSwitchPressTimeout_c);
            }
#else  /*1 == gAppButtonCnt_c*/

            BleApp_Start(mGapRole);
#endif /*1 == gAppButtonCnt_c*/
            break;
        }

        case kBUTTON_EventLongPress:
        {
            for (mPeerId = 0; mPeerId < (uint8_t)gAppMaxConnections_c; mPeerId++)
            {
                if (maPeerInformation[mPeerId].deviceId != gInvalidDeviceId_c)
                {
                    (void)Gap_Disconnect(maPeerInformation[mPeerId].deviceId);
                }
            }

            break;
        }

        default:
        {
            ; /* No action required */
            break;
        }
    }
    return kStatus_BUTTON_Success;
}
#endif /*gAppButtonCnt_c > 0*/

#if (gWuart_CentralRole_c == 1) && (gWuart_PeripheralRole_c == 1)
#if (defined(gAppButtonCnt_c) && (gAppButtonCnt_c > 1))
/*! *********************************************************************************
* \brief        Handler for the second key.
*
* \param[in]    pButtonHandle       Pointer to the button handle.
* \param[in]    pMessage            Pointer to the message.
* \param[in]    pCallbackParam      Pointer to the callback parameters.
********************************************************************************** */
static button_status_t BleApp_HandleKeys1
(
		void *pButtonHandle,
		button_callback_message_t *pMessage,
		void *pCallbackParam
)
{

	static uint8_t index = 0;

	switch (pMessage->event)
	{
	case kBUTTON_EventOneClick:
	case kBUTTON_EventShortPress:
	{

		if(mcActiveConnNo > 0)
		{
			switch (mGapRole) {
			case gGapCentral_c:

				break;

			case gGapPeripheral_c:

				SM_Sensor_Handler(c_states_measurement[index%sm_measurement_light]);
				index++;

				break;

			default:
				break;

			}


		}
		break;
	}

	case kBUTTON_EventLongPress:

		/* Switch current role */
		if (mGapRole == gGapCentral_c)
		{
			Serial_Print("\n\rSwitched role to GAP Peripheral.\n\r", gAllowToBlock_d);
			mAppUartNewLine = TRUE;
			mGapRole = gGapPeripheral_c;
		}
		else
		{
			Serial_Print("\n\rSwitched role to GAP Central.\n\r", gAllowToBlock_d);
			mAppUartNewLine = TRUE;
			mGapRole = gGapCentral_c;
		}

		break;

	default:
	{
		/* No action required */
		break;
	}
	}

	return kStatus_BUTTON_Success;
}
#endif /* (gWuart_CentralRole_c == 1) && (gWuart_PeripheralRole_c == 1) */
#endif /*gAppButtonCnt_c > 0*/

/*! *********************************************************************************
 * \brief    Starts the BLE application.
 *
 * \param[in]    gapRole            GAP Start Role (Central or Peripheral).
 ********************************************************************************** */
static void BleApp_Start
(
    gapRole_t gapRole
)
{
    PRINTF("[BLE DIAG] BleApp_Start() called for role: %d (advOn=%d)\r\n", (int)gapRole, (int)mAdvState.advOn);
    LedStopFlashingAllLeds();
    Led1Flashing();
    switch (gapRole)
    {
#if gWuart_CentralRole_c == 1
    case gGapCentral_c:
        {
            if (mScanningOn == FALSE)
            {
                mAppUartNewLine = TRUE;
#if defined(gUseControllerNotifications_c) && (gUseControllerNotifications_c)
                (void)Gap_ControllerEnhancedNotification((uint16_t)gNotifScanEventOver_c | (uint16_t)gNotifScanAdvPktRx_c |
                                                   (uint16_t)gNotifScanRspRx_c | (uint16_t)gNotifScanReqTx_c, 0);
#endif
#if defined(gAppUsePairing_d) && (gAppUsePairing_d == 1)
                gPairingParameters.localIoCapabilities = gIoKeyboardDisplay_c;
#endif /* gAppUsePairing_d */
                (void)BluetoothLEHost_StartScanning(&mAppScanParams, BleApp_ScanningCallback);
            }
            break;
        }
#endif /* gWuart_CentralRole_c */
#if gWuart_PeripheralRole_c == 1
    case gGapPeripheral_c:
        {
            mAppUartNewLine = TRUE;
#if defined(gUseControllerNotifications_c) && (gUseControllerNotifications_c)
            (void)Gap_ControllerEnhancedNotification((uint16_t)gNotifAdvEventOver_c | (uint16_t)gNotifAdvTx_c |
                                               (uint16_t)gNotifAdvScanReqRx_c | (uint16_t)gNotifAdvConnReqRx_c, 0);
#endif
#if defined(gAppUsePairing_d) && (gAppUsePairing_d == 1)
            gPairingParameters.localIoCapabilities = gIoDisplayOnly_c;
#endif /* gAppUsePairing_d */

            /* Start ADV only if it's not already started */
            if (!mAdvState.advOn)
            {
                PRINTF("[BLE DIAG] Calling BluetoothLEHost_StartAdvertising()...\r\n");
                bleResult_t adv_res = BluetoothLEHost_StartAdvertising(&mAppAdvParams, BleApp_AdvertisingCallback, BleApp_ConnectionCallback);
                PRINTF("[BLE DIAG] BluetoothLEHost_StartAdvertising returned status: %d\r\n", (int)adv_res);
            }
            break;
        }
#endif /* gWuart_PeripheralRole_c */
    default:
        {
            ; /* No action required */
            break;
        }
    }
}

/*! *********************************************************************************
 * \brief        Handles BLE Scanning callback from host stack.
 *
 * \param[in]    pScanningEvent     Pointer to gapScanningEvent_t.
 ********************************************************************************** */
#if gWuart_CentralRole_c == 1
static void BleApp_ScanningCallback
(
    gapScanningEvent_t *pScanningEvent
)
{
    switch (pScanningEvent->eventType)
    {
        case gDeviceScanned_c:
        {
            if (BleApp_CheckScanEvent(&pScanningEvent->eventData.scannedDevice) && (mcActiveConnNo < gAppMaxConnections_c))
            {
                gConnReqParams.peerAddressType = pScanningEvent->eventData.scannedDevice.addressType;
                FLib_MemCpy(gConnReqParams.peerAddress,
                        pScanningEvent->eventData.scannedDevice.aAddress,
                        sizeof(bleDeviceAddress_t));

                (void)Gap_StopScanning();
#if gAppUsePrivacy_d
                gConnReqParams.usePeerIdentityAddress = pScanningEvent->eventData.scannedDevice.advertisingAddressResolved;
#endif
                (void)BluetoothLEHost_Connect(&gConnReqParams, BleApp_ConnectionCallback);
            }
        }
        break;

        case gScanStateChanged_c:
        {
            mScanningOn = !mScanningOn;
            /* Node starts scanning */
            if (mScanningOn)
            {
                /* Start scanning timer */
                (void)TM_InstallCallback((timer_handle_t)mAppTimerId, ScanningTimerCallback, NULL);
                (void)TM_Start((timer_handle_t)mAppTimerId,
                            (uint8_t)kTimerModeLowPowerTimer | (uint8_t)kTimerModeSetSecondTimer, gScanningTime_c);
#if (defined(gAppLedCnt_c) && (gAppLedCnt_c == 1))
                /* LedSetColor disabled: board LED is monochrome */
#endif  /*gAppLedCnt_c == 1*/
                Led1Flashing();
                Serial_Print("\n\rScanning...\n\r", gAllowToBlock_d);
            }
            /* Node is not scanning */
            else
            {
                (void)TM_Stop((timer_handle_t)mAppTimerId);
                LedStartFlashingAllLeds();
                Serial_Print("\n\rStop Scanning...\n\r", gAllowToBlock_d);
            }
        }
        break;

        case gScanCommandFailed_c:
        {
            panic(0, 0, 0, 0);
            break;
        }

        default:
        {
            ; /* No action required */
            break;
        }
    }
}
#endif /* gWuart_CentralRole_c == 1 */

#if (gWuart_PeripheralRole_c == 1)
/*! *********************************************************************************
* \brief        Handles BLE Advertising callback from host stack.
*
* \param[in]    pAdvertisingEvent    Pointer to gapAdvertisingEvent_t.
********************************************************************************** */
static void BleApp_AdvertisingCallback
(
    gapAdvertisingEvent_t *pAdvertisingEvent
)
{
    switch (pAdvertisingEvent->eventType)
    {
        case gAdvertisingStateChanged_c:
        {
            mAdvState.advOn = !mAdvState.advOn;
            LedStopFlashingAllLeds();
            if (mAdvState.advOn)
            {
#if (defined(gAppLedCnt_c) && (gAppLedCnt_c ==1))
            /* LedSetColor disabled: board LED is monochrome */
#endif /* gAppLedCnt_c == 1 */
            Led1Flashing();
            Serial_Print("\n\r>>> [BLE ADVERTISING ACTIVE] Broadcasting as 'NXP_WU' <<<\n\r", gAllowToBlock_d);
            }
            else
            {
#if (defined(gAppLedCnt_c) && (gAppLedCnt_c ==1))
                /* LedSetColor disabled: board LED is monochrome */
#endif /* gAppLedCnt_c == 1 */
                LedStartFlashingAllLeds();
            }
        }
        break;

        case gAdvertisingCommandFailed_c:
        {
            Serial_Print("\n\r[BLE ERROR] Gap_StartAdvertising failed!\n\r", gAllowToBlock_d);
            panic(0, 0, 0, 0);
        }
        break;

        default:
        {
            ; /* No action required */
        }
        break;
    }
}

#endif
/*! *********************************************************************************
* \brief        Handles BLE Connection callback from host stack.
*
* \param[in]    peerDeviceId        Peer device ID.
* \param[in]    pConnectionEvent    Pointer to gapConnectionEvent_t.
********************************************************************************** */
static void BleApp_ConnectionCallback
(
    deviceId_t peerDeviceId,
    gapConnectionEvent_t *pConnectionEvent
)
{
    switch (pConnectionEvent->eventType)
    {
        case gConnEvtConnected_c:
        {
            /* Save peer device ID */
            maPeerInformation[peerDeviceId].deviceId = peerDeviceId;
            mcActiveConnNo++;
            /* Advertising stops when connected */
#if gWuart_PeripheralRole_c == 1
            if(pConnectionEvent->eventData.connectedEvent.connectionRole == gBleLlConnectionPeripheral_c)
            {
                mAdvState.advOn = FALSE;
            }
#endif

            /* Subscribe client */
            (void)Wus_Subscribe(peerDeviceId);

            /* UI */
            LedStopFlashingAllLeds();
            Led1On();

            maPeerInformation[peerDeviceId].gapRole = (pConnectionEvent->eventData.connectedEvent.connectionRole == gBleLlConnectionPeripheral_c) ? gGapPeripheral_c : gGapCentral_c;
            maPeerInformation[peerDeviceId].appState = mAppRunning_c;

            PRINTF("\r\n[BLE DIAG] Peer %d Connected stably as %s!\r\n", (int)peerDeviceId,
                   (maPeerInformation[peerDeviceId].gapRole == gGapPeripheral_c) ? "Peripheral" : "Central");
            mAppUartNewLine = TRUE;

            /* Run state machine */
            BleApp_StateMachineHandler(peerDeviceId, mAppEvt_PeerConnected_c);
        }
        break;

        case gConnEvtDisconnected_c:
        {
            PRINTF("\r\n[BLE DIAG] Peer Disconnected! peerDeviceId=%d, reason=0x%02X\r\n", peerDeviceId, pConnectionEvent->eventData.disconnectedEvent.reason);
            Serial_Print("Disconnected from device ", gAllowToBlock_d);
            Serial_PrintDec(peerDeviceId);
            Serial_Print(".\n\r", gAllowToBlock_d);

            maPeerInformation[peerDeviceId].appState = mAppIdle_c;
            maPeerInformation[peerDeviceId].clientInfo.hService = gGattDbInvalidHandleIndex_d;
            maPeerInformation[peerDeviceId].clientInfo.hUartStream = gGattDbInvalidHandleIndex_d;

            /* Unsubscribe client */
            (void)Wus_Unsubscribe();

            /* Reset Service Discovery */
            BleServDisc_Stop(peerDeviceId);

            /* UI */
            LedStartFlashingAllLeds();

            /* Mark device id as invalid */
            maPeerInformation[peerDeviceId].deviceId = gInvalidDeviceId_c;
            if (mcActiveConnNo > 0U)
            {
                mcActiveConnNo--;
            }

            mAdvState.advOn = FALSE;
            PRINTF("[BLE DIAG] Re-advertising as 'NXP_WU' after disconnect...\r\n");
            BleApp_Start(gGapPeripheral_c);
        }
        break;

#if gAppUsePairing_d

        case gConnEvtPairingComplete_c:
        {
            if (pConnectionEvent->eventData.pairingCompleteEvent.pairingSuccessful)
            {
                BleApp_StateMachineHandler(peerDeviceId,
                                           mAppEvt_PairingComplete_c);
            }
        }
        break;

        case gConnEvtAuthenticationRejected_c:
        {
            if (mGapRole == gGapCentral_c)
            {
                /* Start Pairing Procedure 
                 * peripheral could have lost the bond */
                (void)Gap_Pair(peerDeviceId, &gPairingParameters);
            }
        }
        break;
#endif /* gAppUsePairing_d */

        default:
        {
            ; /* No action required */
        }
        break;
    }

    /* Connection Manager to handle Host Stack interactions */
    switch (maPeerInformation[peerDeviceId].gapRole)
    {
#if gWuart_CentralRole_c == 1
        case gGapCentral_c:
            BleConnManager_GapCentralEvent(peerDeviceId, pConnectionEvent);
            break;
#endif /* gWuart_CentralRole_c == 1 */
#if gWuart_PeripheralRole_c == 1
        case gGapPeripheral_c:
            BleConnManager_GapPeripheralEvent(peerDeviceId, pConnectionEvent);
            break;
#endif /* gWuart_PeripheralRole_c == 1 */
        default:
            ; /* No action required */
            break;
    }
}

/*! *********************************************************************************
* \brief        Handle Service Discovery events
*
* \param[in]    peerDeviceId        Remote device ID.
* \param[in]    pEvent              Pointer to the event revceived.
********************************************************************************** */
static void BleApp_ServiceDiscoveryCallback
(
    deviceId_t peerDeviceId,
    servDiscEvent_t *pEvent
)
{
    switch (pEvent->eventType)
    {
        case gServiceDiscovered_c:
        {
            if (pEvent->eventData.pService->uuidType == gBleUuidType128_c)
            {
                if (FLib_MemCmp((void *)&uuid_service_wireless_uart, (void *)&pEvent->eventData.pService->uuid, sizeof(bleUuid_t)))
                {
                    BleApp_StoreServiceHandles(peerDeviceId, pEvent->eventData.pService);
                }
            }
        }
        break;

        case gDiscoveryFinished_c:
        {
            if (pEvent->eventData.success)
            {
                if (gGattDbInvalidHandleIndex_d != maPeerInformation[peerDeviceId].clientInfo.hService)
                {
                    BleApp_StateMachineHandler(peerDeviceId,
                                               mAppEvt_ServiceDiscoveryComplete_c);
                }
                else
                {
                    BleApp_StateMachineHandler(peerDeviceId,
                                               mAppEvt_ServiceDiscoveryNotFound_c);
                }
            }
            else
            {
                BleApp_StateMachineHandler(peerDeviceId,
                                           mAppEvt_ServiceDiscoveryFailed_c);
            }
        }
        break;

        default:
        {
            ; /* No action required */
        }
        break;
    }
}

/*! *********************************************************************************
* \brief        Handles GATT client callback from host stack.
*
* \param[in]    serverDeviceId      GATT Server device ID.
* \param[in]    procedureType       Procedure type.
* \param[in]    procedureResult     Procedure result.
* \param[in]    error               Callback result.
********************************************************************************** */
static void BleApp_GattClientCallback
(
    deviceId_t              serverDeviceId,
    gattProcedureType_t     procedureType,
    gattProcedureResult_t   procedureResult,
    bleResult_t             error
)
{
    switch (procedureResult)
    {
        case gGattProcError_c:
        {
            switch (error)
            {
#if (defined(gAppUsePairing_d) && (gAppUsePairing_d == 1U))
                case gGattConnectionSecurityRequirementsNotMet_c:
                {
                    if (mGapRole == gGapCentral_c)
                    {
                        /* Start Pairing Procedure in central role
                         * local bond could be lost */
                        (void)Gap_Pair(serverDeviceId, &gPairingParameters);
                    }
                    else if (mGapRole == gGapPeripheral_c)
                    {
                        /* Send Security Request in peripheral role
                         * peer bond could be lost */
                        (void)Gap_SendPeripheralSecurityRequest(serverDeviceId, &gPairingParameters);
                    }
                    else
                    {
                        /* No action required */
                    }
                }
                break;
#endif /* gAppUsePairing_d */

                default:
                {
                    BleApp_StateMachineHandler(serverDeviceId, mAppEvt_GattProcError_c);
                }
                break;
            }
            break;
        }

        case gGattProcSuccess_c:
            BleApp_StateMachineHandler(serverDeviceId, mAppEvt_GattProcComplete_c);
            break;

        default:
            ; /* No action required */
            break;
    }

    /* Signal Service Discovery Module */
    BleServDisc_SignalGattClientEvent(serverDeviceId, procedureType, procedureResult, error);
}

/*! *********************************************************************************
 * \brief        Handles GATT server callback from host stack.
 *
 * \param[in]    deviceId           Client peer device ID.
 * \param[in]    pServerEvent       Pointer to gattServerEvent_t.
 ********************************************************************************** */
static void BleApp_GattServerCallback
(
    deviceId_t deviceId,
    gattServerEvent_t *pServerEvent
)
{
    uint16_t tempMtu = 0;

    switch (pServerEvent->eventType)
    {
        case gEvtAttributeWrittenWithoutResponse_c:
        {
            if (pServerEvent->eventData.attributeWrittenEvent.handle == (uint16_t)value_uart_stream)
            {
                BleApp_ReceivedUartStream(deviceId, pServerEvent->eventData.attributeWrittenEvent.aValue,
                                          pServerEvent->eventData.attributeWrittenEvent.cValueLength);
            }

            break;
        }

        case gEvtMtuChanged_c:
        {
            tempMtu = pServerEvent->eventData.mtuChangedEvent.newMtu;
            PRINTF("\r\n[BLE GATTS] MTU exchange accepted: %d bytes\r\n", (int)tempMtu);
            tempMtu = gAttMaxWriteDataSize_d(tempMtu);
            mAppUartBufferSize = mAppUartBufferSize <= tempMtu ? mAppUartBufferSize : tempMtu;
            break;
        }

        case gEvtCharacteristicCccdWritten_c:
        {
            PRINTF("\r\n[BLE GATTS] CCCD written on handle %d, value 0x%04X\r\n",
                   (int)pServerEvent->eventData.charCccdWrittenEvent.handle,
                   (unsigned int)pServerEvent->eventData.charCccdWrittenEvent.newCccd);
            if (pServerEvent->eventData.charCccdWrittenEvent.newCccd & (uint16_t)gCccdNotification_c)
            {
                maPeerInformation[deviceId].appState = mAppRunning_c;
                PRINTF("[BLE GATTS] Telemetry Notifications ENABLED for device %d!\r\n", (int)deviceId);
                /* Send immediate first telemetry frame */
                CropTelemetryTimerCallback(NULL);
            }
            break;
        }

        default:
        {
            ; /* No action required */
        }
        break;
    }
}

/*! *********************************************************************************
 * \brief        Stores handles used by the application.
 *
 * \param[in]    peerDeviceId       The remote device ID.
 * \param[in]    pService           Pointer to gattService_t.
 ********************************************************************************** */
static void BleApp_StoreServiceHandles
(
    deviceId_t peerDeviceId,
    gattService_t *pService
)
{
    /* Found Wireless UART Service */
    maPeerInformation[peerDeviceId].clientInfo.hService = pService->startHandle;

    if (pService->cNumCharacteristics > 0U &&
        pService->aCharacteristics != NULL)
    {
        /* Found Uart Characteristic */
        maPeerInformation[peerDeviceId].clientInfo.hUartStream =
            pService->aCharacteristics[0].value.handle;
    }
}

/*! *********************************************************************************
 * \brief        Checks Scan data for a device to connect.
 *
 * \param[in]    pData              Pointer to gapScannedDevice_t.
 ********************************************************************************** */
#if gWuart_CentralRole_c == 1
static bool_t BleApp_CheckScanEvent
(
    gapScannedDevice_t *pData
)
{
    uint8_t index = 0;
    bool_t foundMatch = FALSE;

    while (index < pData->dataLength)
    {
        gapAdStructure_t adElement;

        adElement.length = pData->data[index];
        adElement.adType = (gapAdType_t) pData->data[index + 1U];
        adElement.aData = &pData->data[index + 2U];

        /* Search for Wireless UART Service */
        if ((adElement.adType == gAdIncomplete128bitServiceList_c)
            || (adElement.adType == gAdComplete128bitServiceList_c))
        {
            foundMatch = BluetoothLEHost_MatchDataInAdvElementList(
                    &adElement,
                    &uuid_service_wireless_uart,
                    16);
        }

        /* Move on to the next AD element type */
        index += adElement.length + (uint8_t)sizeof(uint8_t);
    }

    return foundMatch;
}
#endif /* gWuart_CentralRole_c == 1 */

/*! *********************************************************************************
 * \brief        Send the received uart stream over GATT
 *
 * \param[in]    pData              Pointer to the received stream.
 * \param[in]    streamSize         The number of bytes in the stream.
 ********************************************************************************** */
static void BleApp_SendUartStream
(
    uint8_t *pRecvStream,
    uint8_t streamSize
)
{
    uint8_t              mPeerId = 0;

    /* send UART stream to all peers */
    for (mPeerId = 0; mPeerId < (uint8_t)gAppMaxConnections_c; mPeerId++)
    {
        deviceId_t peerDev = maPeerInformation[mPeerId].deviceId;
        if (gInvalidDeviceId_c != peerDev)
        {
            if (mGapRole == gGapPeripheral_c)
            {
                uint16_t currentMtu = 23;
                (void)Gatt_GetMtu(peerDev, &currentMtu);
                uint16_t maxChunk = (currentMtu > 3) ? (currentMtu - 3) : 20;
                if (maxChunk > 244) maxChunk = 244;
                if (maxChunk < 20) maxChunk = 20;

                uint8_t offset = 0;
                while (offset < streamSize)
                {
                    uint16_t toSend = (uint16_t)(streamSize - offset);
                    if (toSend > maxChunk)
                    {
                        toSend = maxChunk;
                    }
                    (void)GattServer_SendInstantValueNotification(peerDev, (uint16_t)value_uart_stream, toSend, &pRecvStream[offset]);
                    offset += (uint8_t)toSend;
                }
            }
            else
            {
                gattCharacteristic_t characteristic = {(uint8_t)gGattCharPropNone_c, {0}, 0, 0};
                characteristic.value.handle = maPeerInformation[mPeerId].clientInfo.hUartStream;
                (void)GattClient_WriteCharacteristicValue(mPeerId, &characteristic,
                        streamSize, pRecvStream, TRUE,
                        FALSE, FALSE, NULL);
            }
        }
    }
}

/*! *********************************************************************************
 * \brief        Handle the main application state machine
 *
 * \param[in]    peerDeviceId       The remote device ID.
 * \param[in]    event              The application event.
 ********************************************************************************** */
static void BleApp_StateMachineHandler
(
    deviceId_t peerDeviceId,
    appEvent_t event
)
{
    uint16_t tempMtu = 0;
    union
    {
        uint8_t     *pUuidArray;
        bleUuid_t   *pUuidObj;
    } temp; /* MISRA rule 11.3 */

    temp.pUuidArray = uuid_service_wireless_uart;

    /* invalid client information */
    if (maPeerInformation[peerDeviceId].deviceId != gInvalidDeviceId_c)
    {
        switch (maPeerInformation[peerDeviceId].appState)
        {
        case mAppIdle_c:
        {
            if (event == mAppEvt_PeerConnected_c)
            {
                /* Let the central device initiate the Exchange MTU procedure*/
                if (mGapRole == gGapCentral_c)
                {
                    /* Moving to Exchange MTU State */
                    maPeerInformation[peerDeviceId].appState = mAppExchangeMtu_c;
                    (void)GattClient_ExchangeMtu(peerDeviceId, gAttMaxMtu_c);
                }
                else
                {
                    /* When running as Peripheral (connected to a smartphone Central):
                     * Move directly to Running State! Do not perform client discovery on the smartphone. */
                    maPeerInformation[peerDeviceId].appState = mAppRunning_c;
                }
            }
        }
        break;

        case mAppExchangeMtu_c:
        {
            if (event == mAppEvt_GattProcComplete_c)
            {
                /* update stream length with minimum of maximum MTU's of connected devices */
                (void)Gatt_GetMtu(peerDeviceId, &tempMtu);
                tempMtu = gAttMaxWriteDataSize_d(tempMtu);

                mAppUartBufferSize = mAppUartBufferSize <= tempMtu ? mAppUartBufferSize : tempMtu;

                /* Moving to Service Discovery State*/
                maPeerInformation[peerDeviceId].appState = mAppServiceDisc_c;

                /* Start Service Discovery*/
                (void)BleServDisc_FindService(peerDeviceId,
                        gBleUuidType128_c,
                        temp.pUuidObj);
            }
            else
            {
                if (event == mAppEvt_GattProcError_c)
                {
                    (void)Gap_Disconnect(peerDeviceId);
                }
            }
        }
        break;

        case mAppServiceDisc_c:
        {
            if (event == mAppEvt_ServiceDiscoveryComplete_c)
            {
                /* Moving to Running State*/
                maPeerInformation[peerDeviceId].appState = mAppRunning_c;
#if gAppUseBonding_d
                union
                {
                    uint32_t u32;
                    uint16_t u16;
                } tempCast;

                tempCast.u32 = sizeof(wucConfig_t);
                /* Write data in NVM */
                (void)Gap_SaveCustomPeerInformation(maPeerInformation[peerDeviceId].deviceId,
                        (uint8_t *) &maPeerInformation[peerDeviceId].clientInfo, 0,
                        tempCast.u16);
#endif
            }
            else if (event == mAppEvt_ServiceDiscoveryNotFound_c)
            {
                /* Moving to Service discovery Retry State*/
                maPeerInformation[peerDeviceId].appState = mAppServiceDiscRetry_c;
                /* Restart Service Discovery for all services */
                (void)BleServDisc_Start(peerDeviceId);
            }
            else if (event == mAppEvt_ServiceDiscoveryFailed_c)
            {
                (void)Gap_Disconnect(peerDeviceId);
            }
            else
            {
                /* ignore other event types */
            }
        }
        break;

        case mAppServiceDiscRetry_c:
        {
            if (event == mAppEvt_ServiceDiscoveryComplete_c)
            {
                /* Moving to Running State*/
                maPeerInformation[peerDeviceId].appState = mAppRunning_c;
            }
            else if ((event == mAppEvt_ServiceDiscoveryNotFound_c) ||
                    (event == mAppEvt_ServiceDiscoveryFailed_c))
            {
                (void)Gap_Disconnect(peerDeviceId);
            }
            else
            {
                /* ignore other event types */
            }
        }
        break;

        case mAppRunning_c:
            break;

        default:
        {
            ; /* No action required */
        }
        break;
        }
    }
}

#if gWuart_CentralRole_c == 1
/*! *********************************************************************************
 * \brief        Handles scanning timer callback.
 *
 * \param[in]    pParam             Callback parameters.
 ********************************************************************************** */
static void ScanningTimerCallback
(
    void *pParam
)
{
    /* Stop scanning */
    (void)Gap_StopScanning();
}
#endif /* gWuart_CentralRole_c == 1 */

#if (defined(gAppButtonCnt_c) && (gAppButtonCnt_c == 1))
/*! *********************************************************************************
 * \brief        Handles the switch press timer callback.
 *
 * \param[in]    pParam             Callback parameters.
 ********************************************************************************** */
static void SwitchPressTimerCallback
(
    void *pParam
)
{
  if(mSwitchPressCnt >= gSwitchPressThreshold_c)
  {
      /* Switch the current role */
      if (mGapRole == gGapCentral_c)
      {
          Serial_Print("\n\rSwitched role to GAP Peripheral.\n\r", gAllowToBlock_d);
          mAppUartNewLine = TRUE;
          mGapRole = gGapPeripheral_c;
      }
      else
      {
          Serial_Print("\n\rSwitched role to GAP Central.\n\r", gAllowToBlock_d);
          mAppUartNewLine = TRUE;
          mGapRole = gGapCentral_c;
      }
  }
  else
  {
      /* start the application using the selected role */
      LedStopFlashingAllLeds();
      Led1Flashing();
      BleApp_Start(mGapRole);
  }

  /* reset the switch press counter */
  mSwitchPressCnt = 0;
}
#endif

/*! *********************************************************************************
 * \brief        Get all bytes from the serial interface and send it over GATT.
 *
 * \param[in]    pParam             Callback parameters.
 ********************************************************************************** */
static void BleApp_FlushUartStream
(
    void *pParam
)
{
    uint8_t *pMsg = NULL;
    uint32_t bytesRead = 0;
    uint8_t  mPeerId = 0;
    bool_t   mValidDevices = FALSE;

    /* Valid devices are in Running state */
    for (mPeerId = 0; mPeerId < (uint8_t)gAppMaxConnections_c; mPeerId++)
    {
        if ((gInvalidDeviceId_c != maPeerInformation[mPeerId].deviceId) &&
            (mAppRunning_c == maPeerInformation[mPeerId].appState))
        {
            mValidDevices = TRUE;
            break;
        }
    }

    if (mValidDevices)
    {
        /* Allocate buffer for GATT Write */
        pMsg = MEM_BufferAlloc(mAppUartBufferSize);

        if (pMsg != NULL)
        {
            /* Collect the data from the serial manager buffer */
            if (SerialManager_TryRead((serial_read_handle_t)s_readHandle, pMsg, mAppUartBufferSize, &bytesRead) == kStatus_SerialManager_Success)
            {
                if (bytesRead != 0U)
                {
                    /* Send data over the air */
                    BleApp_SendUartStream(pMsg, (uint8_t)bytesRead);
                }
            }


            /* Free Buffer */
            (void)MEM_BufferFree(pMsg);
        }
    }

    mAppDapaPending = FALSE;
}

/*! *********************************************************************************
 * \brief        Write bytes to serial.
 *
 * \param[in]    peerDeviceId       The remote device ID.
 * \param[in]    pStream            Pointer to the received stream.
 * \param[in]    streamLength       Number of bytes in the strem.
 ********************************************************************************** */
static void BleApp_ReceivedUartStream
(
    deviceId_t peerDeviceId,
    uint8_t *pStream,
    uint16_t streamLength
)
{
    static deviceId_t previousDeviceId = gInvalidDeviceId_c;
    static uint8_t queue_cnt = 0;
    static uint8_t queue[] = {"SENS O"};

    char additionalInfoBuff[10] = { '\r', '\n', '[', '0', '0', '-', 'C', ']', ':', ' '};
    uint8_t *pBuffer = NULL;
    uint8_t *pCommand = NULL;
    uint32_t messageHeaderSize = 0;
    bool_t validCommand = FALSE;
    sm_measurement_t next_state;



    /* Concatenate */
    if( streamLength == 1)
    {
    	if(queue_cnt < COMMAND_LEN)
    	{
    		queue[queue_cnt] = *pStream;
    		queue_cnt++;
    	}
    	else
    	{
    		for(uint8_t i = 0; i < COMMAND_LEN - 1U; i++)
    		{
    			queue[i] = queue[i+1];
    		}
    		queue[COMMAND_LEN - 1U] = *pStream;
    	}
    }

    /* Valid comand check */
    pCommand= MEM_BufferAlloc(COMMAND_LEN);
    if( ( streamLength == 1 ) && (queue_cnt == COMMAND_LEN ))
    {
    	if(pCommand != NULL)
    	{
    		FLib_MemCpy(pCommand, queue, COMMAND_LEN);
    	}
    	validCommand = TRUE;

    }
    else if( (streamLength == COMMAND_LEN) ||  ( (streamLength == COMMAND_LEN + 1U) && ( *(pStream + 6) == '\n' )) )
    {
    	if(pCommand != NULL)
    	{
    		FLib_MemCpy(pCommand, pStream, COMMAND_LEN);
    	}
    	validCommand = TRUE;
    	*(pCommand+6) = '\0';
    }


    if(validCommand)
    {
    	validCommand = FALSE;
    	Str_UperCaseFormat(pCommand);
    	if( FLib_MemCmp(pCommand, c_command_msgs[0], COMMAND_LEN - 1U) || FLib_MemCmp(pCommand, c_command_msgs[1], COMMAND_LEN - 1U) )
    	{
    		validCommand = TRUE;

    		/* Check for Sensor type */
    		next_state = ( *(pCommand+5) == 'A' ) ? sm_measurement_accel : sm_measurement_light;
    	}
    }
    (void)MEM_BufferFree(pCommand);


    if(validCommand)
    {
    	SM_Sensor_Handler(next_state);
    }
    else
    {
    	if (mAppUartNewLine || (previousDeviceId != peerDeviceId))
    	{
    		streamLength += (uint16_t)sizeof(additionalInfoBuff);
    	}

    	/* Allocate buffer for asynchronous write */
    	pBuffer = MEM_BufferAlloc(streamLength);

    	if (pBuffer != NULL)
    	{
    		/* if this is a message from a previous device, print device ID */
    		if (mAppUartNewLine || (previousDeviceId != peerDeviceId))
    		{
    			messageHeaderSize = sizeof(additionalInfoBuff);

    			if (mAppUartNewLine)
    			{
    				mAppUartNewLine = FALSE;
    			}

    			additionalInfoBuff[3] = '0' + (peerDeviceId / 10U);
    			additionalInfoBuff[4] = '0' + (peerDeviceId % 10U);

    			if (gGapCentral_c != maPeerInformation[peerDeviceId].gapRole)
    			{
    				additionalInfoBuff[6] = 'P';
    			}

    			FLib_MemCpy(pBuffer, additionalInfoBuff, sizeof(additionalInfoBuff));
    		}

    		FLib_MemCpy(&pBuffer[messageHeaderSize], pStream, (uint32_t)streamLength - messageHeaderSize);
#if (defined(SERIAL_MANAGER_NON_BLOCKING_MODE) && (SERIAL_MANAGER_NON_BLOCKING_MODE > 0U))
    		serial_manager_status_t status = SerialManager_InstallTxCallback((serial_write_handle_t)s_writeHandle, Uart_TxCallBack, pBuffer);
    		(void)status;
    		assert(kStatus_SerialManager_Success == status);
    		if(SerialManager_WriteNonBlocking((serial_write_handle_t)s_writeHandle, pBuffer, streamLength) != kStatus_SerialManager_Success)
    		{
    			(void)MEM_BufferFree(pBuffer);
    		}
#else
    		(void)MEM_BufferFree(pBuffer);
#endif /*SERIAL_MANAGER_NON_BLOCKING_MODE > 0U*/
    	}

    	/* update the previous device ID */
    	previousDeviceId = peerDeviceId;

    }

}

/*! *********************************************************************************
 * \brief        Timer handler for flushing the UART.
 *
 * \param[in]    pData              Pointer to the parameters.
 ********************************************************************************** */
static void UartStreamFlushTimerCallback
(
    void *pData
)
{
    if (!mAppDapaPending)
    {
        mAppDapaPending = TRUE;
        (void)App_PostCallbackMessage(BleApp_FlushUartStream, NULL);
    }
}

/*! *********************************************************************************
* \brief        Handles UART Receive callback.
*
* \param[in]    pData        Unused pointer to data.
* \param[in]    pMessage     Unused pointer to message.
* \param[in]    status       Unused status.
********************************************************************************** */
static void Uart_RxCallBack
(
    void *pData,
    serial_manager_callback_message_t *pMessage,
    serial_manager_status_t status
)
{
    uint16_t byteCount = 0;

    if (byteCount < mAppUartBufferSize)
    {
        /* Restart flush timer */
        (void)TM_InstallCallback((timer_handle_t)mUartStreamFlushTimerId, UartStreamFlushTimerCallback, NULL);
        (void)TM_Start((timer_handle_t)mUartStreamFlushTimerId,
                    (uint8_t)kTimerModeLowPowerTimer | (uint8_t)kTimerModeSingleShot, mAppUartFlushIntervalInMs_c);
    }
    else
    {
        /* Post App Msg only one at a time */
        if (!mAppDapaPending)
        {
            mAppDapaPending = TRUE;
            (void)App_PostCallbackMessage(BleApp_FlushUartStream, NULL);
        }
    }
}

/*! *********************************************************************************
* \brief        Handles UART Transmit callback.
*
* \param[in]    pBuffer      Pointer to the sent data.
* \param[in]    pMessage     Unused pointer to message.
* \param[in]    status       Unused status.
********************************************************************************** */
static void Uart_TxCallBack
(
    void *pBuffer,
    serial_manager_callback_message_t *pMessage,
    serial_manager_status_t status
)
{
    (void)MEM_BufferFree(pMessage->buffer);
}


/*! *********************************************************************************
* \brief        Handles battery measurement timer callback.
*
* \param[in]    pParam        Callback parameters.
********************************************************************************** */
static void BatteryMeasurementTimerCallback
(
    void *pParam
)
{
    mBasServiceConfig.batteryLevel = SENSORS_GetBatteryLevel();
    (void)Bas_RecordBatteryMeasurement(&mBasServiceConfig);
}

static void AccelMeasurementTimerCallback
(
    void *pParam
)
{
	int32_t status;
		uint8_t* str;
		uint8_t* pMsg = NULL;
	    uint8_t  mPeerId = 0;
	    bool_t   mValidDevices = FALSE;
		uint8_t raw_data[FXLS8974_DATA_SIZE] = {0};
		fxls8974_acceldata_t accelData;
		uint8_t len;


	    /* Valid devices are in Running state */
	    for (mPeerId = 0; mPeerId < (uint8_t)gAppMaxConnections_c; mPeerId++)
	    {
	        if ((gInvalidDeviceId_c != maPeerInformation[mPeerId].deviceId) &&
	            (mAppRunning_c == maPeerInformation[mPeerId].appState))
	        {
	            mValidDevices = TRUE;
	            break;
	        }
	    }


	    if (mValidDevices)
	    {

	    	if(!mAppDapaPending)
	    	{
	    		/* Allocate buffer for GATT Write */
				pMsg = (uint8_t*)MEM_BufferAlloc(SizeOfString(s_accel_msg));
	    	}

	        if (pMsg != NULL)
	        {
	        	mAppDapaPending = TRUE;

	        	status = FXLS8974_I2C_ReadData(&fxls8974Driver, FXLS8974_ACCEL_REGS_READ, raw_data);
	        	assert(ARM_DRIVER_OK == status);

	        	/*! Convert the raw sensor data to signed 16-bit container. */
	        	accelData.accel[0] = (int16_t)(((int16_t)(((int16_t)raw_data[1] << 8) | raw_data[0])));
	        	accelData.accel[1] = (int16_t)(((int16_t)(((int16_t)raw_data[3] << 8) | raw_data[2])));
	        	accelData.accel[2] = (int16_t)(((int16_t)(((int16_t)raw_data[5] << 8) | raw_data[4])));


	        	FLib_MemCpy(pMsg, (const void*)s_accel_msg, sizeof(s_accel_msg));

	        	for(uint8_t i = 0; i < 3; i++)
	        	{
	        		str = FORMAT_Dec2Str(abs(accelData.accel[i]));
	        		len = FLib_StrLen((char const *)str);

	        		if(accelData.accel[i] < 0)
	        		{
	        			pMsg[s_msg_offset[i]] = '-';
	        		}
	        		else
	        		{
	        			pMsg[s_msg_offset[i]] = ' ';
	        		}

	        		FLib_MemCpy((void*)&pMsg[s_msg_offset[i]+1], (const void*)str, len);

	        		accelData.accel[i] = (accelData.accel[i] < 0) ? accelData.accel[i]*-1:accelData.accel[i];
	        		accelData.accel[i] = (int16_t)(((int32_t)accelData.accel[i]*100U)/SCALE_MEASUREMENT_RANGE);
	        		if(accelData.accel[i] > 100U )
	        			accelData.accel[i] = 100U;

	        		HAL_PwmUpdateDutycycle(halPwmHandle_rgb, i, kHAL_EdgeAlignedPwm, accelData.accel[i]);
	        	}


	        	Serial_Print((uint8_t*)pMsg, gAllowToBlock_d);

	        	/* Send data over the air */
	        	BleApp_SendUartStream(pMsg, (uint8_t)SizeOfString(s_accel_msg));

	            /* Free Buffer */
	            (void)MEM_BufferFree(pMsg);

	            mAppDapaPending = FALSE;
	        }

	    }


}

static void CropTelemetryTimerCallback(void *pParam)
{
    (void)pParam;

    // 1. Poll DHT11 (PTC0 / D2)
    float live_temp = 0.0f, live_hum = 0.0f;
    int dht_status = DHT11_Read(&live_hum, &live_temp);
    if (dht_status != 0) {
        // Keep previous valid reading if a single checksum fails, but attempt new read
    }

    // 2. Poll LPADC Soil Moisture (HW-103)
    uint32_t raw_adc_mv = BOARD_ReadSoilAdcMv(); 
    float live_soil = CalculateSoilMoisturePercent(raw_adc_mv); // 470 mV dry -> 0%, 180 mV wet -> 100%

    // 3. Poll ALS-PT19 Phototransistor
    uint32_t live_lux = BOARD_ReadAmbientLightLux();

    // 4. Re-calculate Edge AI Crop Stress Score dynamically
    float live_stress = CalculateCropStressScore(live_soil, live_temp, live_hum, (float)live_lux);

    // 5. Format fresh dynamic JSON telemetry payload using integer decimal formatting (guarantees valid JSON in newlib-nano)
    int soil_whole = (int)live_soil;
    int soil_frac = (int)((live_soil >= 0.0f ? (live_soil - (float)soil_whole) : ((float)soil_whole - live_soil)) * 10.0f + 0.5f);
    if (soil_frac >= 10) { soil_whole++; soil_frac = 0; }

    int temp_whole = (int)live_temp;
    int temp_frac = (int)((live_temp >= 0.0f ? (live_temp - (float)temp_whole) : ((float)temp_whole - live_temp)) * 10.0f + 0.5f);
    if (temp_frac >= 10) { temp_whole++; temp_frac = 0; }

    int hum_whole = (int)live_hum;
    int hum_frac = (int)((live_hum >= 0.0f ? (live_hum - (float)hum_whole) : ((float)hum_whole - live_hum)) * 10.0f + 0.5f);
    if (hum_frac >= 10) { hum_whole++; hum_frac = 0; }

    int stress_whole = (int)live_stress;
    int stress_frac = (int)((live_stress >= 0.0f ? (live_stress - (float)stress_whole) : ((float)stress_whole - live_stress)) * 10.0f + 0.5f);
    if (stress_frac >= 10) { stress_whole++; stress_frac = 0; }

    char json_buf[128];
    snprintf(json_buf, sizeof(json_buf),
             "{\"soil\":%d.%d,\"temp\":%d.%d,\"hum\":%d.%d,\"lux\":%lu,\"stress\":%d.%d}\r\n",
             soil_whole, soil_frac,
             temp_whole, temp_frac,
             hum_whole, hum_frac,
             (unsigned long)live_lux,
             stress_whole, stress_frac);

    /* Update GATT DB attribute value so on-demand GATT reads return the complete fresh JSON */
    (void)GattDb_WriteAttribute((uint16_t)value_uart_stream, (uint16_t)strlen(json_buf), (const uint8_t *)json_buf);

    /* Print to Serial USB Console */
    Serial_Print((uint8_t *)json_buf, gAllowToBlock_d);

    // 6. Send live stream over BLE notification
    BleApp_SendUartStream((uint8_t *)json_buf, (uint8_t)strlen(json_buf));

    /* Keep periodic timer running */
    (void)TM_Start((timer_handle_t)mCropTelemetryTimerId,
                   (uint8_t)kTimerModeIntervalTimer | (uint8_t)kTimerModeSetSecondTimer, 2U);
}

void DEMO_LPADC_IRQ_HANDLER_FUNC(void)
{
    SDK_ISR_EXIT_BARRIER;
}

static void SM_Sensor_Handler(sm_measurement_t next_state)
{
    (void)next_state;
}

void Str_UperCaseFormat(uint8_t* ptr)
{
	while(*ptr != '\0')
	{
		if(IS_LOWER_CASE(ptr))
			UPER_CASE(ptr);
		ptr++;
	}
}


#if defined(gUseControllerNotifications_c) && (gUseControllerNotifications_c)
/*! *********************************************************************************
 * \brief        Function handling the Bluetooth Controller notification events.
 *
 * \param[in]    pNotificationEvent Pointer to the notification event.
 ********************************************************************************** */
static void BleApp_HandleControllerNotification
(
    bleNotificationEvent_t *pNotificationEvent
)
{
    switch(pNotificationEvent->eventType)
    {
        case (uint16_t)gNotifEventNone_c:
        {
            Serial_Print("Configured notification status ", gAllowToBlock_d);
            Serial_PrintDec((uint32_t)pNotificationEvent->status);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }

        case (uint16_t)gNotifConnEventOver_c:
        {
            Serial_Print("CONN Event Over device ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->deviceId);
            Serial_Print(" on channel ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->channel);
            Serial_Print(" with RSSI ", gAllowToBlock_d);
            Serial_PrintDec((uint8_t)pNotificationEvent->rssi);
            Serial_Print(" and event counter ", gAllowToBlock_d);
            Serial_PrintDec((uint16_t)pNotificationEvent->ce_counter);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }

        case (uint16_t)gNotifConnRxPdu_c:
        {
            Serial_Print("CONN Rx PDU from device ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->deviceId);
            Serial_Print(" on channel ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->channel);
            Serial_Print(" with RSSI ", gAllowToBlock_d);
            Serial_PrintDec((uint8_t)pNotificationEvent->rssi);
            Serial_Print(" with event counter ", gAllowToBlock_d);
            Serial_PrintDec((uint16_t)pNotificationEvent->ce_counter);
            Serial_Print(" and timestamp ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->timestamp);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }

        case (uint16_t)gNotifAdvEventOver_c:
        {
            Serial_Print("ADV Event Over.\n\r", gAllowToBlock_d);
            break;
        }

        case (uint16_t)gNotifAdvTx_c:
        {
            Serial_Print("ADV Tx on channel ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->channel);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }

        case (uint16_t)gNotifAdvScanReqRx_c:
        {
            Serial_Print("ADV Rx Scan Req on channel ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->channel);
            Serial_Print(" with RSSI ", gAllowToBlock_d);
            Serial_PrintDec((uint8_t)pNotificationEvent->rssi);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }

        case (uint16_t)gNotifAdvConnReqRx_c:
        {
            Serial_Print("ADV Rx Conn Req on channel ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->channel);
            Serial_Print(" with RSSI ", gAllowToBlock_d);
            Serial_PrintDec((uint8_t)pNotificationEvent->rssi);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }

        case (uint16_t)gNotifScanEventOver_c:
        {
            Serial_Print("SCAN Event Over on channel ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->channel);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }

        case (uint16_t)gNotifScanAdvPktRx_c:
        {
            Serial_Print("SCAN Rx Adv Pkt on channel ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->channel);
            Serial_Print(" with RSSI ", gAllowToBlock_d);
            Serial_PrintDec((uint8_t)pNotificationEvent->rssi);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }

        case (uint16_t)gNotifScanRspRx_c:
        {
            Serial_Print("SCAN Rx Scan Rsp on channel ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->channel);
            Serial_Print(" with RSSI ", gAllowToBlock_d);
            Serial_PrintDec((uint8_t)pNotificationEvent->rssi);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }

        case (uint16_t)gNotifScanReqTx_c:
        {
            Serial_Print("SCAN Tx Scan Req on channel ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->channel);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }

        case (uint16_t)gNotifConnCreated_c:
        {
            Serial_Print("CONN Created with device ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->deviceId);
            Serial_Print(" with timestamp ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->timestamp);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }
        
        case (uint16_t)gNotifConnChannelMapUpdate_c:
        {
            Serial_Print("Map update with device ", gAllowToBlock_d);
            Serial_PrintDec(pNotificationEvent->deviceId);
            Serial_Print("\n\r", gAllowToBlock_d);
            break;
        }

        default:
        {
            ; /* No action required */
            break;
        }
    }
}

#if defined(gUseControllerNotificationsCallback_c) && (gUseControllerNotificationsCallback_c)
/*! *********************************************************************************
 * \brief        Function handling the controller notifications.
 *
 * \param[in]    pNotificationEvent Pointer to the notification event.
 ********************************************************************************** */
static void BleApp_ControllerNotificationCallback
(
    bleCtrlNotificationEvent_t *pNotificationEvent
)
{
    switch(pNotificationEvent->event_type)
    {
        case gNotifConnEventOver_c:
        {
            Serial_Print("CONN Ev Over\n\r", gNoBlock_d);
            break;
        }

        case gNotifConnRxPdu_c:
        {
            Serial_Print("CONN Rx PDU\n\r", gNoBlock_d);
            break;
        }

        case gNotifAdvEventOver_c:
        {
            Serial_Print("ADV Ev Over\n\r", gNoBlock_d);
            break;
        }

        case gNotifAdvTx_c:
        {
            Serial_Print("ADV Tx\n\r", gNoBlock_d);
            break;
        }

        case gNotifAdvScanReqRx_c:
        {
            Serial_Print("ADV Rx Scan Req\n\r", gNoBlock_d);
            break;
        }

        case gNotifAdvConnReqRx_c:
        {
            Serial_Print("ADV Rx Conn Req\n\r", gNoBlock_d);
            break;
        }

        case gNotifScanEventOver_c:
        {
            Serial_Print("SCAN Ev Over\n\r", gNoBlock_d);
            break;
        }

        case gNotifScanAdvPktRx_c:
        {
            Serial_Print("SCAN Rx Adv\n\r", gNoBlock_d);
            break;
        }

        case gNotifScanRspRx_c:
        {
            Serial_Print("SCAN Rx Scan Rsp\n\r", gNoBlock_d);
            break;
        }

        case gNotifScanReqTx_c:
        {
            Serial_Print("SCAN Tx Scan Req\n\r", gNoBlock_d);
            break;
        }

        case gNotifConnCreated_c:
        {
            Serial_Print("CONN Created\n\r", gNoBlock_d);
            break;
        }

        default:
        {
            ; /* No action required */
            break;
        }
    }
}
#endif

#endif

/*! *********************************************************************************
 * \brief        Function used to setup the serial interface.
 *
 ********************************************************************************** */
static void BleApp_SerialInit(void)
{
    serial_manager_status_t status;
    /*wireless uart serial manager handle*/
    static serial_handle_t appSerMgrIf;

    /* UI */
    appSerMgrIf = (serial_handle_t)&gSerMgrIf[0];

    /*open wireless uart read/write handle*/
    status = SerialManager_OpenWriteHandle((serial_handle_t)appSerMgrIf, (serial_write_handle_t)s_writeHandle);
    assert(kStatus_SerialManager_Success == status);
    (void)status;

    status = SerialManager_OpenReadHandle((serial_handle_t)appSerMgrIf, (serial_read_handle_t)s_readHandle);
    assert(kStatus_SerialManager_Success == status);
    status = SerialManager_InstallRxCallback((serial_read_handle_t)s_readHandle, Uart_RxCallBack, NULL);
    assert(kStatus_SerialManager_Success == status);
}


static void BleApp_AccelInit(void)
{


	int32_t status;
	uint8_t whoami = 0;
	ARM_DRIVER_I2C *I2Cdrv = &I2C_S_DRIVER; // Now using the shield.h value!!!
	hal_pwm_setup_config_t setupConfig = {
			.level = kHAL_PwmLowTrue,
			.mode = kHAL_EdgeAlignedPwm,
			.pwmFreq_Hz = 1000U,
			.dutyCyclePercent = 0U
	};
	BOARD_InitPinLED2();
	BOARD_InitPinLED3();
	BOARD_InitPinLED4();

	/*! Initialize the I2C driver. */
	status = I2Cdrv->Initialize(I2C_S_SIGNAL_EVENT);
	assert(ARM_DRIVER_OK == status);

	/*! Set the I2C Power mode. */
	status = I2Cdrv->PowerControl(ARM_POWER_FULL);
	assert(ARM_DRIVER_OK == status);

	/*! Set the I2C bus speed. */
	status = I2Cdrv->Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_FAST);
	assert(ARM_DRIVER_OK == status);

	/*! Initialize FXLS8974 sensor driver. */
	status = FXLS8974_I2C_Initialize(&fxls8974Driver, &I2C_S_DRIVER, I2C_S_DEVICE_INDEX, FXLS8974_I2C_ADDR,
			&whoami);
	assert(SENSOR_ERROR_NONE == status);

	/*! Configure the FXLS8974 sensor. */
	status = FXLS8974_I2C_Configure(&fxls8974Driver, cFxls8974ConfigNormal);
	assert(SENSOR_ERROR_NONE == status);

	CLOCK_SetIpSrc(kCLOCK_Tpm0, kCLOCK_IpSrcFro6M);
	HAL_PwmInit(halPwmHandle_rgb, 0U, 6000000U);
	HAL_PwmSetupPwm(halPwmHandle_rgb, 0U, &setupConfig);
	HAL_PwmSetupPwm(halPwmHandle_rgb, 1U, &setupConfig);
	HAL_PwmSetupPwm(halPwmHandle_rgb, 2U, &setupConfig);



}

static void BleApp_LighSensorInit(void)
{
	/* ------------------- LPIT ------------------- */
	/* -------------------------------------------- */
	/* -------------------------------------------- */

	/* Structure of initialize LPIT */
	lpit_config_t lpitConfig;
	lpit_chnl_params_t lpitChannelConfig;
	/* Set the source for the LPIT module */
	CLOCK_SetIpSrc(kCLOCK_Lpit0, kCLOCK_IpSrcFro6M);

	LPIT_GetDefaultConfig(&lpitConfig);
	lpitConfig.enableRunInDoze = true;

	/* Init lpit module */
	LPIT_Init(DEMO_LPIT_BASE, &lpitConfig);

	lpitChannelConfig.chainChannel          = false;
	lpitChannelConfig.enableReloadOnTrigger = false;
	lpitChannelConfig.enableStartOnTrigger  = false;
	lpitChannelConfig.enableStopOnTimeout   = false;
	lpitChannelConfig.timerMode             = kLPIT_PeriodicCounter;
	/* Set default values for the trigger source */
	lpitChannelConfig.triggerSelect = kLPIT_Trigger_TimerChn0;
	lpitChannelConfig.triggerSource = kLPIT_TriggerSource_External;

	/* Init lpit channel 0 */
	LPIT_SetupChannel(DEMO_LPIT_BASE, kLPIT_Chnl_0, &lpitChannelConfig);

	/* Set timer period for channel 0 */
	LPIT_SetTimerPeriod(DEMO_LPIT_BASE, kLPIT_Chnl_0, USEC_TO_COUNT(LIGHT_SENSOR_FS, LPIT_SOURCECLOCK));

	/* ------------------- VREF ------------------- */
	/* -------------------------------------------- */
	/* -------------------------------------------- */
	vref_config_t vrefConfig;

	VREF_GetDefaultConfig(&vrefConfig);
	/* Initialize VREF module, the VREF module provides reference voltage and bias current for LPADC. */
	VREF_Init(DEMO_VREF_BASE, &vrefConfig);
	/* ------------------- TGRMX ------------------ */
	/* -------------------------------------------- */
	/* -------------------------------------------- */
	TRGMUX_SetTriggerSource(TRGMUX0, kTRGMUX_Trgmux0AdcGp0, kTRGMUX_TriggerInput0, kTRGMUX_SourceLpit0Channel0);
	/* ------------------- LPADC ------------------ */
	/* -------------------------------------------- */
	/* -------------------------------------------- */
	lpadc_config_t mLpadcConfigStruct;
	lpadc_conv_trigger_config_t mLpadcTriggerConfigStruct;
	lpadc_conv_command_config_t mLpadcCommandConfigStruct;

	CLOCK_SetIpSrc(kCLOCK_Lpadc0, kCLOCK_IpSrcFro192M);
	CLOCK_SetIpSrcDiv(kCLOCK_Lpadc0, kSCG_SysClkDivBy10);

	LPADC_GetDefaultConfig(&mLpadcConfigStruct);
	mLpadcConfigStruct.enableAnalogPreliminary = true;
	mLpadcConfigStruct.FIFO0Watermark = FIFO_WATHER_MARK;
#if defined(DEMO_LPADC_VREF_SOURCE)
	mLpadcConfigStruct.referenceVoltageSource = DEMO_LPADC_VREF_SOURCE;
#endif /* DEMO_LPADC_VREF_SOURCE */
#if defined(FSL_FEATURE_LPADC_HAS_CTRL_CAL_AVGS) && FSL_FEATURE_LPADC_HAS_CTRL_CAL_AVGS
	mLpadcConfigStruct.conversionAverageMode = kLPADC_ConversionAverage128;
#endif /* FSL_FEATURE_LPADC_HAS_CTRL_CAL_AVGS */
	LPADC_Init(DEMO_LPADC_BASE, &mLpadcConfigStruct);

	/* Request LPADC calibration. */
	LPADC_DoOffsetCalibration(DEMO_LPADC_BASE); /* Request offset calibration, automatic update OFSTRIM register. */
	LPADC_DoAutoCalibration(DEMO_LPADC_BASE);

	/* Set conversion CMD configuration. */
	LPADC_GetDefaultConvCommandConfig(&mLpadcCommandConfigStruct);
	mLpadcCommandConfigStruct.channelNumber = DEMO_LPADC_USER_CHANNEL;
	mLpadcCommandConfigStruct.sampleChannelMode = DEMO_LPADC_USER_CHANNEL_MODE;
	mLpadcCommandConfigStruct.conversionResolutionMode = kLPADC_ConversionResolutionHigh;

	LPADC_SetConvCommandConfig(DEMO_LPADC_BASE, DEMO_LPADC_USER_CMDID, &mLpadcCommandConfigStruct);

	/* Set trigger configuration. */
	LPADC_GetDefaultConvTriggerConfig(&mLpadcTriggerConfigStruct);
	mLpadcTriggerConfigStruct.targetCommandId       = DEMO_LPADC_USER_CMDID;     /* CMD15 is executed. */
	mLpadcTriggerConfigStruct.enableHardwareTrigger = true;
	LPADC_SetConvTriggerConfig(DEMO_LPADC_BASE, 0U, &mLpadcTriggerConfigStruct); /* Configurate the trigger0. */

	/* Enable the watermark interrupt. */
	LPADC_EnableInterrupts(DEMO_LPADC_BASE, kLPADC_FIFO0WatermarkInterruptEnable);
	EnableIRQ(DEMO_LPADC_IRQn);
	/* When the number of datawords stored in the ADC Result FIFO is greater
	 * than watermark value(0U), LPADC watermark interrupt would be triggered. */

	/*Start Timer*/
	LPIT_StartTimer(DEMO_LPIT_BASE, kLPIT_Chnl_0);
}

/*! *********************************************************************************
 * \brief        Function handling the initialization complete event of the Bluetooth LE Host stack.
 *
 ********************************************************************************** */
static void BluetoothLEHost_Initialized(void)
{
    PRINTF("\r\n[BLE DIAG] >>> BluetoothLEHost_Initialized() ENTERED! <<<\r\n");
    uint8_t peerId = 0;

    /* Populate GATT DB with initial telemetry JSON so initial reads return valid JSON */
    const char *initJson = "{\"soil\":24.5,\"temp\":26.2,\"hum\":58.0,\"lux\":1050,\"stress\":14.2}\r\n";
    (void)GattDb_WriteAttribute((uint16_t)value_uart_stream, (uint16_t)strlen(initJson), (const uint8_t *)initJson);

    /* Common GAP configuration */
    BleConnManager_GapCommonConfig();

    /* Register for callbacks */
    (void)App_RegisterGattServerCallback(BleApp_GattServerCallback);
    (void)App_RegisterGattClientProcedureCallback(BleApp_GattClientCallback);
    (void)GattServer_RegisterHandlesForWriteNotifications(NumberOfElements(mCharMonitoredHandles), mCharMonitoredHandles);
    BleServDisc_RegisterCallback(BleApp_ServiceDiscoveryCallback);
    mcActiveConnNo = 0U;
    for (peerId = 0; peerId < (uint8_t)gAppMaxConnections_c; peerId++)
    {
        maPeerInformation[peerId].deviceId = gInvalidDeviceId_c;
        maPeerInformation[peerId].appState = mAppIdle_c;
        maPeerInformation[peerId].clientInfo.hService = gGattDbInvalidHandleIndex_d;
        maPeerInformation[peerId].clientInfo.hUartStream = gGattDbInvalidHandleIndex_d;
    }

#if (gWuart_AutoStart_c == 1)
    mGapRole = gWuart_AutoStartGapRole_c;
#else
    /* By default, always start node as GAP central */
#if gWuart_CentralRole_c == 1
    mGapRole = gGapCentral_c;
#else
    mGapRole = gGapPeripheral_c;
#endif /* gWuart_CentralRole_c */
#endif /* gWuart_AutoStart_c */

#if (defined(gAppButtonCnt_c) && (gAppButtonCnt_c == 1))
    Serial_Print("\n\rWireless UART starting as GAP Central.\n\r", gAllowToBlock_d);
    Serial_Print("\n\rWithin one second, either:\n\r", gAllowToBlock_d);
    Serial_Print(" - double press the switch to change the role or\n\r", gAllowToBlock_d);
    Serial_Print(" - single press to start the application with the selected role.\n\r", gAllowToBlock_d);
#else
    if (mGapRole == gGapCentral_c)
    {
        Serial_Print("\n\rWireless UART starting as GAP Central.\n\r", gAllowToBlock_d);
    }
    else
    {
        Serial_Print("\n\rWireless UART starting as GAP Peripheral.\n\r", gAllowToBlock_d);
    }
#endif
#if gWuart_PeripheralRole_c == 1
    mAdvState.advOn = FALSE;
#endif /* gWuart_PeripheralRole_c */

#if gWuart_CentralRole_c == 1
    mScanningOn = FALSE;
#endif /* gWuart_CentralRole_c */

    /* Start services */
    (void)Wus_Start(&mWuServiceConfig);

    mBasServiceConfig.batteryLevel = SENSORS_GetBatteryLevel();
    (void)Bas_Start(&mBasServiceConfig);

    /* Allocate application timer */
    (void)TM_Open(mAppTimerId);
    (void)TM_Open(mUartStreamFlushTimerId);
    (void)TM_Open(mBatteryMeasurementTimerId);
    (void)TM_Open(mAccelMeasurementTimerId);
    (void)TM_Open(mCropTelemetryTimerId);

#if !(defined(cPWR_UsePowerDownMode) && (cPWR_UsePowerDownMode))
    LedStartFlashingAllLeds();
#endif

    /* Start BLE Advertising immediately */
#if (gWuart_AutoStart_c == 1)
    Serial_Print("\n\r[BLE DIAG] Controller initialized, calling BleApp_Start()...\n\r", gAllowToBlock_d);
    BleApp_Start(mGapRole);
#endif

    /* Initialize Crop Stress Telemetry Sensors after BLE stack bootup */
    Serial_Print("[BLE DIAG] Initializing Crop Stress Sensors (HW-103 Soil LPADC + DHT11 PTC0)...\n\r", gAllowToBlock_d);
    (void)crop_adc_init();
    dht11_init();

    /* Start Telemetry timer (2 seconds periodic interval) */
    (void)TM_InstallCallback((timer_handle_t)mCropTelemetryTimerId, CropTelemetryTimerCallback, NULL);
    (void)TM_Start((timer_handle_t)mCropTelemetryTimerId,
                   (uint8_t)kTimerModeIntervalTimer | (uint8_t)kTimerModeSetSecondTimer, 2U);
    Serial_Print("[BLE DIAG] Crop Telemetry timer running (2s cadence).\n\r", gAllowToBlock_d);

    /* Populate initial sensor telemetry into GATT DB immediately */
    CropTelemetryTimerCallback(NULL);

#if (gAppButtonCnt_c == 1)
    (void)TM_Open(mSwitchPressTimerId);
#endif
}

/*! *********************************************************************************
 * \brief        Function handling the geeneric events from the host stack.
 *
 ********************************************************************************** */
static void BluetoothLEHost_GenericCallback(gapGenericEvent_t *pGenericEvent)
{
    /* Call BLE Conn Manager */
    BleConnManager_GenericEvent(pGenericEvent);

#if defined(gUseControllerNotifications_c) && (gUseControllerNotifications_c)
    if (pGenericEvent->eventType == gControllerNotificationEvent_c)
    {
        BleApp_HandleControllerNotification(&pGenericEvent->eventData.notifEvent);
    }
#endif /* gUseControllerNotifications_c */
    if(pGenericEvent->eventType == gInternalError_c)
    {
        if((pGenericEvent->eventData.internalError.errorCode == gBleOverflow_c) && (pGenericEvent->eventData.internalError.errorSource == gAddNewConnection_c))
        {
            Serial_Print("Connection failed ", gAllowToBlock_d);
#if gWuart_PeripheralRole_c == 1
            if(mAdvState.advOn)
            {
                mAdvState.advOn = FALSE;
                Serial_Print(" as peripheral.\n\r", gAllowToBlock_d);
            }
            else
#endif /* gWuart_PeripheralRole_c == 1 */
            {
#if gWuart_CentralRole_c == 1
                Serial_Print(" as central.\n\r", gAllowToBlock_d);
#endif /* gWuart_CentralRole_c == 1 */

            }
        }
    }
}

/*! *********************************************************************************
 * @}
 ********************************************************************************** */
