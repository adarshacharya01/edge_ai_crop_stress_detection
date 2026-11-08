if(NOT DEFINED FPU)
	set(FPU "-mfpu=fpv5-sp-d16 -mfloat-abi=hard")
endif()

if(${LIBRARY_TYPE} STREQUAL "REDLIB")
	set(SPECS "-specs=redlib.specs")
elseif(${LIBRARY_TYPE} STREQUAL "NEWLIB_NANO")
	set(SPECS "--specs=nano.specs")
endif()

if(NOT DEFINED DEBUG_CONSOLE_CONFIG)
	set(DEBUG_CONSOLE_CONFIG "-DSDK_DEBUGCONSOLE=1")
endif()

set(CMAKE_ASM_FLAGS_DEBUG " \
    ${CMAKE_ASM_FLAGS_DEBUG} \
    ${FPU} \
    -mcpu=cortex-m33 \
    -mthumb \
")

set(CMAKE_C_FLAGS_DEBUG " \
    ${CMAKE_C_FLAGS_DEBUG} \
    ${FPU} \
    ${DEBUG_CONSOLE_CONFIG} \
    -std=gnu99 \
    -DCPU_MCXW716CMFTA \
    -DCPU_MCXW716CMFTA_cm33 \
    -DSERIAL_MANAGER_NON_BLOCKING_MODE=1 \
    -DSERIAL_USE_CONFIGURE_STRUCTURE=1 \
    -DgButtonSupported_d=1 \
    -DOSA_USED \
    -DSDK_DEBUGCONSOLE_UART \
    -DSDK_COMPONENT_INTEGRATION=1 \
    -DFSL_OSA_TASK_ENABLE=1 \
    -DMCXW716C \
    -DCFG_BLE_PRJ=1 \
    -DENABLE_RAM_VECTOR_TABLE=1 \
    -DNXP_SSSAPI \
    -DNXP_ELE200 \
    -DHAL_RPMSG_SELECT_ROLE=0 \
    -DSERIAL_PORT_TYPE_RPMSG=1 \
    -DRPMSG_ADAPTER_NON_BLOCKING_MODE=1 \
    -DHAL_FLASH_ROMAPI_DRIVER=1 \
    -DgUseHciTransportDownward_d=1 \
    -DTM_ENABLE_TIME_STAMP=1 \
    -DBOARD_FRDMCXW71_SUPPORT=1 \
    -DMCUXPRESSO_SDK \
    -DTIMER_PORT_TYPE_LPTMR=1 \
    -DSERIAL_PORT_TYPE_UART=1 \
    -DSDK_OS_BAREMETAL \
    -DSSS_CONFIG_FILE=\\\"fsl_sss_config_elemu.h\\\" \
    -DSSCP_CONFIG_FILE=\\\"fsl_sscp_config_elemu.h\\\" \
    -DMULTICORE_APP=1 \
    -DGENERIC_LIST_LIGHT=1 \
    -DCR_INTEGER_PRINTF \
    -D__MCUXPRESSO \
    -D__USE_CMSIS \
    -DDEBUG \
    -Og \
    -fno-common \
    -fmerge-constants \
    -g3 \
    -mcpu=cortex-m33 -ffunction-sections -fdata-sections -fno-builtin -imacros \"${ProjDirPath}/source/app_preinclude.h\" \
    -fstack-usage \
    -mcpu=cortex-m33 \
    -mthumb \
")

set(CMAKE_CXX_FLAGS_DEBUG " \
    ${CMAKE_CXX_FLAGS_DEBUG} \
    ${FPU} \
    ${DEBUG_CONSOLE_CONFIG} \
    -O0 \
    -fno-common \
    -fmerge-constants \
    -g3 \
    -Wall \
    -fstack-usage \
    -mcpu=cortex-m33 \
    -mthumb \
")

set(CMAKE_EXE_LINKER_FLAGS_DEBUG " \
    ${CMAKE_EXE_LINKER_FLAGS_DEBUG} \
    ${FPU} \
    ${SPECS} \
    -nostdlib \
    -Xlinker \
    --sort-section=alignment \
    -Xlinker \
    --defsym=__ram_vector_table__=1 \
    -Xlinker \
    --defsym=gUseNVMLink_d=1 \
    -Xlinker \
    --gc-sections \
    -Xlinker \
    -print-memory-usage \
    -Xlinker \
    -Map=output.map \
    -Xlinker \
    --defsym=gUseStackEnd_d=1 \
    -Xlinker \
    --defsym=m_lowpower_flag_start=0x489C007C \
    -Xlinker \
    --no-warn-rwx-segments \
    -Xlinker \
    -no-warn-rwx-segments \
    -Xlinker \
    --cref \
    -mcpu=cortex-m33 \
    -mthumb \
    -T\"${ProjDirPath}/frdmmcxw71_wireless_uart_bm_light_sensor_Debug.ld\" \
    -L\"${ProjDirPath}/linkerScripts/Debug\" \
")

set(CMAKE_ASM_FLAGS_RELEASE " \
    ${CMAKE_ASM_FLAGS_RELEASE} \
    ${FPU} \
    -mcpu=cortex-m33 \
    -mthumb \
")

set(CMAKE_C_FLAGS_RELEASE " \
    ${CMAKE_C_FLAGS_RELEASE} \
    ${FPU} \
    ${DEBUG_CONSOLE_CONFIG} \
    -std=gnu99 \
    -DCPU_MCXW716CMFTA \
    -DCPU_MCXW716CMFTA_cm33 \
    -DSERIAL_MANAGER_NON_BLOCKING_MODE=1 \
    -DSERIAL_USE_CONFIGURE_STRUCTURE=1 \
    -DgButtonSupported_d=1 \
    -DOSA_USED \
    -DSDK_DEBUGCONSOLE_UART \
    -DSDK_COMPONENT_INTEGRATION=1 \
    -DFSL_OSA_TASK_ENABLE=1 \
    -DMCXW716C \
    -DCFG_BLE_PRJ=1 \
    -DENABLE_RAM_VECTOR_TABLE=1 \
    -DNXP_SSSAPI \
    -DNXP_ELE200 \
    -DHAL_RPMSG_SELECT_ROLE=0 \
    -DSERIAL_PORT_TYPE_RPMSG=1 \
    -DRPMSG_ADAPTER_NON_BLOCKING_MODE=1 \
    -DHAL_FLASH_ROMAPI_DRIVER=1 \
    -DgUseHciTransportDownward_d=1 \
    -DTM_ENABLE_TIME_STAMP=1 \
    -DBOARD_FRDMCXW71_SUPPORT=1 \
    -DMCUXPRESSO_SDK \
    -DTIMER_PORT_TYPE_LPTMR=1 \
    -DSERIAL_PORT_TYPE_UART=1 \
    -DSDK_OS_BAREMETAL \
    -DSSS_CONFIG_FILE=\\\"fsl_sss_config_elemu.h\\\" \
    -DSSCP_CONFIG_FILE=\\\"fsl_sscp_config_elemu.h\\\" \
    -DMULTICORE_APP=1 \
    -DGENERIC_LIST_LIGHT=1 \
    -DCR_INTEGER_PRINTF \
    -D__MCUXPRESSO \
    -D__USE_CMSIS \
    -DNDEBUG \
    -Os \
    -fno-common \
    -fmerge-constants \
    -mcpu=cortex-m33 -ffunction-sections -fdata-sections -fno-builtin -imacros \"${ProjDirPath}/source/app_preinclude.h\" \
    -fstack-usage \
    -mcpu=cortex-m33 \
    -mthumb \
")

set(CMAKE_CXX_FLAGS_RELEASE " \
    ${CMAKE_CXX_FLAGS_RELEASE} \
    ${FPU} \
    ${DEBUG_CONSOLE_CONFIG} \
    -fno-common \
    -fmerge-constants \
    -Wall \
    -fstack-usage \
    -mcpu=cortex-m33 \
    -mthumb \
")

set(CMAKE_EXE_LINKER_FLAGS_RELEASE " \
    ${CMAKE_EXE_LINKER_FLAGS_RELEASE} \
    ${FPU} \
    ${SPECS} \
    -nostdlib \
    -Xlinker \
    --sort-section=alignment \
    -Xlinker \
    --defsym=__ram_vector_table__=1 \
    -Xlinker \
    --defsym=gUseNVMLink_d=1 \
    -Xlinker \
    --gc-sections \
    -Xlinker \
    -print-memory-usage \
    -Xlinker \
    -Map=output.map \
    -Xlinker \
    --defsym=gUseStackEnd_d=1 \
    -Xlinker \
    --defsym=m_lowpower_flag_start=0x489C007C \
    -Xlinker \
    --no-warn-rwx-segments \
    -Xlinker \
    -no-warn-rwx-segments \
    -Xlinker \
    --cref \
    -mcpu=cortex-m33 \
    -mthumb \
    -T\"${ProjDirPath}/frdmmcxw71_wireless_uart_bm_light_sensor_Release.ld\" \
    -L\"${ProjDirPath}/linkerScripts/Release\" \
")
