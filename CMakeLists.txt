# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

#
# Title:	        EgoRAM Makefile
# Description:		Makefile 
# Author:	        Dean Belfield
# Created:	        31/01/2021
# Last Updated:		26/09/2024
#
# Modinfo:
# 01/02/2022:		Added this header comment, fixed typo in executable filename, added extra target sources
# 19/02/2022:		Added terminal.c
# 26/09/2024:		Updated build files so that the project can be built more easily

#
# See the official documentation https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html
# for guidance on setting up the Pico C/C++ SDK.
#

cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(EgoRAM C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable( EgoRAM 
                EgoRAM.c 
                cvideo.c
                atari_cart.c
		)

#target_compile_options(EgoRAM PRIVATE -save-temps=obj -g)

target_link_libraries(
        EgoRAM PRIVATE
        pico_stdlib
        pico_multicore
        pico_rand
        hardware_uart
        hardware_dma
        hardware_gpio
        hardware_pio
        pico_runtime
)

pico_enable_stdio_uart(EgoRAM 0)   # UART als stdio
pico_enable_stdio_usb(EgoRAM 1)    # USB Serial deaktivieren

pico_add_extra_outputs(EgoRAM)

pico_generate_pio_header(EgoRAM ${CMAKE_CURRENT_LIST_DIR}/cvideo_sync.pio)
pico_generate_pio_header(EgoRAM ${CMAKE_CURRENT_LIST_DIR}/clock.pio)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/cvideo_sync.pio.h
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/cvideo_sync.pio
        COMMAND Pioasm ${CMAKE_CURRENT_LIST_DIR}/cvideo_sync.pio ${CMAKE_CURRENT_LIST_DIR}/generated/cvideo_sync.pio.h
)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/clock.pio.h
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/clock.pio
        COMMAND Pioasm ${CMAKE_CURRENT_LIST_DIR}/cvideo_sync.pio ${CMAKE_CURRENT_LIST_DIR}/generated/clock.pio.h
)

#add_custom_command(
#        OUTPUT ${CMAKE_CURRENT_LIST_DIR}/generated/cvideo_data.pio.h
#        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/cvideo_data.pio
#        COMMAND Pioasm ${CMAKE_CURRENT_LIST_DIR}/cvideo_data.pio ${CMAKE_CURRENT_LIST_DIR}/generated/cvideo_data.pio.h
#)
