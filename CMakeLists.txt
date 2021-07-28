# Copyright (c) 2021 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.19.0)

set(MBED_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mbed-os CACHE INTERNAL "")
set(MBED_CONFIG_PATH ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "")
set(APP_TARGET mbed-app)

include(${MBED_PATH}/tools/cmake/app.cmake)

add_subdirectory(${MBED_PATH})

add_subdirectory(FXOS8700CQ)

add_executable(${APP_TARGET}
    main.cpp
    RTPMIDI.cpp
)

project(${APP_TARGET})

target_link_libraries(${APP_TARGET}
    mbed-os
    mbed-netsocket
    fxos8700cq-lib
    mbed-usb
)

target_include_directories(${APP_TARGET}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/FXOS8700CQ
)

mbed_set_post_build(${APP_TARGET})

option(VERBOSE_BUILD "Have a verbose build process")
if(VERBOSE_BUILD)
    set(CMAKE_VERBOSE_MAKEFILE ON)
endif()
