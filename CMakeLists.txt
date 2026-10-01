cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")

include(${CMAKE_CURRENT_LIST_DIR}/pico-sdk/external/pico_sdk_import.cmake)

project(ecorun_front_panel C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose Release or MinSizeRel" FORCE)
endif()

pico_sdk_init()

add_executable(ecorun_front_panel
    src/main.c
    src/hfp_client.c
    src/settings.c
    src/usb_debug.c
    src/display_ili9341.c
    src/touch_xpt2046.c
    src/command.c
    src/sim7070.c
    src/websocket_client.c
)

pico_generate_pio_header(ecorun_front_panel
    ${CMAKE_CURRENT_LIST_DIR}/src/display_pio_ili9341.pio
    ${CMAKE_CURRENT_LIST_DIR}/src/touch_xpt2046.pio
)

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h" CACHE PATH "" FORCE)
add_compile_definitions(LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/include/lv_conf.h")

include(FetchContent)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG        v1.7.18
)
FetchContent_MakeAvailable(cjson)

target_include_directories(ecorun_front_panel PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}
    ${cjson_SOURCE_DIR}
)

target_compile_definitions(ecorun_front_panel PRIVATE
    ENABLE_HFP=1
    # Work around the RP2040 USB device enumeration race documented by the
    # Pico SDK/TinyUSB integration. Without this, some Linux hosts can report
    # EPIPE (-32) while reading the device/configuration descriptors.
    PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
)

add_subdirectory(lvgl)

add_library(frontpanel_gui STATIC
    src/ui.c
)

target_include_directories(frontpanel_gui PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(frontpanel_gui PUBLIC lvgl)

target_compile_options(ecorun_front_panel PRIVATE
    -ffunction-sections
    -fdata-sections
)

target_link_options(ecorun_front_panel PRIVATE
    -Wl,--gc-sections
)

pico_enable_stdio_usb(ecorun_front_panel 0)
pico_enable_stdio_uart(ecorun_front_panel 1)

target_link_libraries(ecorun_front_panel
    pico_stdlib
    pico_bootrom
    pico_cyw43_arch_none
    pico_btstack_classic
    pico_btstack_cyw43
    cjson
    hardware_spi
    hardware_pio
    hardware_gpio
    hardware_pwm
    hardware_timer
    hardware_uart
    tinyusb_device
    tinyusb_host
    tinyusb_board
    lvgl
    frontpanel_gui
)

pico_add_extra_outputs(ecorun_front_panel)

add_custom_command(TARGET ecorun_front_panel POST_BUILD
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/pico-map/pico-map.py $<TARGET_FILE:ecorun_front_panel>.map
    COMMENT "Analyzing memory map with pico-map..."
)
