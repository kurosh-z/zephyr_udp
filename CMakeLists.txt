#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set(DTC_OVERLAY_FILE nrf9160dk_nrf9160ns.overlay)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(udp)

# NORDIC SDK APP START
target_sources(app PRIVATE src/udp.c)
target_sources(app PRIVATE src/uart.c)
target_sources(app PRIVATE src/UART_handler.c)
# NORDIC SDK APP END

zephyr_include_directories(src)
