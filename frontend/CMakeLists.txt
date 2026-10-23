# Phosphor frontend.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   cmake -B build && cmake --build build -j                     # this machine (snapshots, checks)
#   cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/armhf.cmake && cmake --build build-arm -j
#                                                                  # MiSTer (static, armhf)
cmake_minimum_required(VERSION 3.16)
project(phosphor C)

set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl)
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS ${LVGL_DIR}/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${LVGL_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_compile_options(lvgl PRIVATE -w)


# cJSON (MIT) is shared with the system service
set(CJSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../system/third_party/cjson)
add_library(cjson STATIC ${CJSON_DIR}/cJSON.c)
target_include_directories(cjson PUBLIC ${CJSON_DIR})
target_compile_options(cjson PRIVATE -w)

file(GLOB FONT_SOURCES CONFIGURE_DEPENDS fonts/*.c)
file(GLOB APP_SOURCES CONFIGURE_DEPENDS src/*.c)
add_executable(phosphor ${APP_SOURCES} ${FONT_SOURCES})
target_link_libraries(phosphor PRIVATE lvgl cjson m pthread)
target_include_directories(phosphor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../system/third_party/stb)
set_source_files_properties(src/jpeg.c PROPERTIES COMPILE_OPTIONS "-w")
target_compile_options(phosphor PRIVATE -Wall -Wextra -Wno-unused-parameter)
set_source_files_properties(${FONT_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
