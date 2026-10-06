# Restricted-C++20 tree (core/ reader/ app/ boards/ drivers/) -- see docs/MIGRATION.md.
# UNVERIFIED IN THIS SANDBOX: no ESP-IDF toolchain is available to actually run this
# CMake project here. Reviewed by hand against the Makefile's BOARD=sim build, which
# does compile and pass `make test` in this sandbox.

file(GLOB XR_FONT_SRCS "../fonts/*.c")

set(XR_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${XR_GENERATED_DIR}")
execute_process(
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/gen_plugins.py"
            --plugins "${CMAKE_CURRENT_SOURCE_DIR}/../plugins"
            --config "${CMAKE_CURRENT_SOURCE_DIR}/../.config"
            --out "${XR_GENERATED_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    RESULT_VARIABLE XR_PLUGIN_RESULT
)
if(NOT XR_PLUGIN_RESULT EQUAL 0)
    message(FATAL_ERROR "failed to generate plugin bindings")
endif()

# app_main.cpp expects "build/selected_board.hpp" on its include path (the Makefile
# generates this literally; ESP-IDF's CMake build generates the same content here).
set(XR_SELECTED_BOARD_DIR "${CMAKE_CURRENT_BINARY_DIR}/selected_board")
file(MAKE_DIRECTORY "${XR_SELECTED_BOARD_DIR}")
file(WRITE "${XR_SELECTED_BOARD_DIR}/selected_board.hpp"
    "#pragma once\n#include \"boards/m5paper/runtime.hpp\"\nnamespace selected_board = board::m5paper;\n")

idf_component_register(
    SRCS
        "../app_main.cpp"
        "../drivers/it8951/it8951.cpp"
        "../drivers/gt911/gt911.cpp"
        "../drivers/inflate/inflate.cpp"
        "../boards/m5paper/board.cpp"
        ${XR_FONT_SRCS}
    INCLUDE_DIRS
        ".."
        "../include"
        "../fonts"
        "${XR_SELECTED_BOARD_DIR}"
        "${XR_GENERATED_DIR}"
    REQUIRES
        driver
        esp_adc
        esp_timer
        fatfs
        vfs
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wpedantic -Werror
                       -Wno-pedantic -Wno-error=missing-field-initializers
                       $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -fno-threadsafe-statics>)
