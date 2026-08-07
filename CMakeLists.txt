cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")

include(${CMAKE_CURRENT_LIST_DIR}/pico-sdk/external/pico_sdk_import.cmake)

project(ecorun_front_panel C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(ecorun_front_panel
    src/main.c
    src/display_ili9341.c
    src/touch_xpt2046.c
)

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h" CACHE PATH "" FORCE)

target_include_directories(ecorun_front_panel PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)
add_compile_definitions(LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h")

add_subdirectory(lvgl)

#pico_enable_stdio_usb(ecorun_front_panel 1)
#pico_enable_stdio_uart(ecorun_front_panel 0)

target_link_libraries(ecorun_front_panel
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_timer
    lvgl
)

pico_add_extra_outputs(ecorun_front_panel)
