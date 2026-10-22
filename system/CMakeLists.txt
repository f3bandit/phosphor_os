# Phosphor system: phosphord (service) and phos (command line client).
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   cmake -B build && cmake --build build -j
#   cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=../frontend/cmake/armhf.cmake && cmake --build build-arm -j
cmake_minimum_required(VERSION 3.16)
project(phosphor_system C)

set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_definitions(_GNU_SOURCE)

add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE
  SQLITE_THREADSAFE=1 SQLITE_DEFAULT_WAL_SYNCHRONOUS=1 SQLITE_OMIT_LOAD_EXTENSION
  SQLITE_DQS=0 SQLITE_DEFAULT_MEMSTATUS=0 SQLITE_LIKE_DOESNT_MATCH_BLOBS)
target_compile_options(sqlite3 PRIVATE -w)

add_library(cjson STATIC third_party/cjson/cJSON.c)
target_include_directories(cjson PUBLIC third_party/cjson)
target_compile_options(cjson PRIVATE -w)

file(GLOB PHOSD_SOURCES CONFIGURE_DEPENDS phosphord/*.c)
add_executable(phosphord ${PHOSD_SOURCES})
target_include_directories(phosphord PRIVATE phosphord third_party/stb)
target_link_libraries(phosphord PRIVATE sqlite3 cjson pthread m)
target_compile_options(phosphord PRIVATE -Wall -Wextra -Wno-unused-parameter)
set_source_files_properties(phosphord/art.c PROPERTIES COMPILE_OPTIONS "-Wno-sign-compare;-Wno-unused-function")

add_executable(phos phos/phos.c)
target_compile_options(phos PRIVATE -Wall -Wextra)
