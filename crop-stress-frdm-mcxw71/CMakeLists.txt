cmake_minimum_required(VERSION 3.16)
project(crop_stress C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Model/model_params.h")
    message(FATAL_ERROR "Train on labelled readings first: python tools/train.py readings.csv")
endif()

add_executable(crop_stress
    main.c
    APP/app.c
    Sensors/sensors.c
    Model/model.c
    HAL/hal_sim.c
    HAL/board_adc.c
)
target_compile_definitions(crop_stress PRIVATE CROP_SIMULATOR=1)
target_include_directories(crop_stress PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT MSVC)
    target_link_libraries(crop_stress PRIVATE m)
endif()
