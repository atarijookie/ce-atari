# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ce_pico_fdd_vsc C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(ce_pico_fdd_vsc
        ce_pico_fdd_vsc.cpp
        buttons.cpp
        serial_config.cpp
        connection.cpp
        display.cpp
        ikbd.cpp
        mfm.cpp
        psram.cpp
        utils.cpp
        client_context.h
        adafruit_gfx.cpp
        lcdfont.cpp
        ssd1306.cpp
        )

pico_set_program_name(ce_pico_fdd_vsc "ce_pico_fdd_vsc")
pico_set_program_version(ce_pico_fdd_vsc "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(ce_pico_fdd_vsc 1)
pico_enable_stdio_usb(ce_pico_fdd_vsc 1)

# Add the standard library to the build
target_link_libraries(ce_pico_fdd_vsc
        pico_stdlib
        )

# Add the standard include files to the build
target_include_directories(ce_pico_fdd_vsc PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/stdio_usb/include
)

# Add any user requested libraries
target_link_libraries(ce_pico_fdd_vsc 
        hardware_spi
        hardware_i2c
        hardware_dma
        hardware_pwm
        hardware_timer
        pico_cyw43_arch_lwip_threadsafe_background
        pico_multicore
        hardware_flash
        )

pico_set_linker_script(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/memmap_custom.ld)

pico_add_extra_outputs(ce_pico_fdd_vsc)

