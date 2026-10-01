cmake_minimum_required(VERSION 3.13)

project(ecorun_front_panel_pc C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/../include/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_USE_FIND_PACKAGE ON CACHE BOOL "" FORCE)
set(LV_USE_PKG_CONFIG ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lvgl lvgl)
target_compile_definitions(lvgl PUBLIC FRONTPANEL_PC=1)
target_include_directories(lvgl PRIVATE ${SDL2_INCLUDE_DIRS})

add_library(frontpanel_gui STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../src/ui.c
)
target_include_directories(frontpanel_gui PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/..
)
target_link_libraries(frontpanel_gui PUBLIC lvgl)

add_executable(ecorun_front_panel_pc
    ${CMAKE_CURRENT_LIST_DIR}/../src/pc_main.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/pc_platform.c
)
target_compile_definitions(ecorun_front_panel_pc PRIVATE FRONTPANEL_PC=1)
target_include_directories(ecorun_front_panel_pc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(ecorun_front_panel_pc
    frontpanel_gui
    PkgConfig::SDL2
)
