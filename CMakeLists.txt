cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")

include(${CMAKE_CURRENT_LIST_DIR}/pico-sdk/external/pico_sdk_import.cmake)

project(ecorun_front_panel C CXX ASM)

pico_sdk_init()

add_executable(ecorun_front_panel
    src/main.c
)

pico_enable_stdio_usb(ecorun_front_panel 1)
pico_enable_stdio_uart(ecorun_front_panel 0)

target_link_libraries(ecorun_front_panel pico_stdlib)

pico_add_extra_outputs(ecorun_front_panel)
