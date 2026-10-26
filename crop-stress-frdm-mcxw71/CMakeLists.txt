cmake_minimum_required(VERSION 3.16)
project(crop_stress C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Model/model_params.h")
    message(FATAL_ERROR "Train on labelled readings first: python tools/train.py readings.csv")
endif()

# ARM Cortex-M33 architecture flags
add_compile_options(
    -mcpu=cortex-m33
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv5-sp-d16
    -g
)

add_compile_definitions(
    CPU_MCXW716CMFTA
    CPU_MCXW716CMFTA_cm33
    __USE_CMSIS
    DISABLE_WDOG=1
    SDK_DEBUGCONSOLE=1
    SDK_DEBUGCONSOLE_UART=1
    DEBUG
)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/board"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/CMSIS"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/device"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/drivers"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/component/uart"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/utilities"
)

add_executable(crop_stress
    main.c
    APP/app.c
    Sensors/sensors.c
    Model/model.c
    HAL/hal_mcxw71.c
    HAL/board_adc.c
    
    # Board, clock and pin initialization
    sdk/board/board.c
    sdk/board/clock_config.c
    sdk/board/pin_mux.c
    
    # System startup
    sdk/device/system_MCXW716C.c
    sdk/startup/startup_mcxw716.c
    
    # NXP Peripheral Drivers
    sdk/drivers/fsl_common.c
    sdk/drivers/fsl_common_arm.c
    sdk/drivers/fsl_clock.c
    sdk/drivers/fsl_lpi2c.c
    sdk/drivers/fsl_lpadc.c
    sdk/drivers/fsl_vref.c
    sdk/drivers/fsl_gpio.c
    sdk/drivers/fsl_spc.c
    sdk/drivers/fsl_cmc.c
    sdk/drivers/fsl_lpuart.c
    
    # Utilities & Debug Console
    sdk/utilities/fsl_debug_console.c
    sdk/utilities/fsl_str.c
    sdk/utilities/fsl_assert.c
    sdk/component/uart/fsl_adapter_lpuart.c
)

target_link_options(crop_stress PRIVATE
    -mcpu=cortex-m33
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv5-sp-d16
    --specs=nano.specs
    --specs=nosys.specs
    "-T"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk/frdmmcxw71_wireless_uart_bm_light_sensor_Debug.ld"
    "-L${CMAKE_CURRENT_SOURCE_DIR}/sdk"
)

target_link_libraries(crop_stress PRIVATE m)

add_custom_command(TARGET crop_stress POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:crop_stress> "${CMAKE_CURRENT_BINARY_DIR}/crop_stress.bin"
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:crop_stress> "${CMAKE_CURRENT_BINARY_DIR}/crop_stress.hex"
    COMMENT "Generated crop_stress.bin and crop_stress.hex for flashing"
)
