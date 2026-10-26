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

# libchdr (BSD-3, with LZMA SDK public domain, miniz MIT, zstd BSD): product codes inside .chd discs
set(CHDR_DIR third_party/libchdr)
add_library(chdr STATIC ${CHDR_DIR}/unity.c ${CHDR_DIR}/src/libchdr_codec_avhuff.c)   # unity.c leaves avhuff out
target_include_directories(chdr PUBLIC ${CHDR_DIR}/include ${CHDR_DIR}/deps/miniz-3.1.2
                           PRIVATE ${CHDR_DIR} ${CHDR_DIR}/deps/lzma-26.02/include ${CHDR_DIR}/deps/zstd-1.5.7)
target_compile_definitions(chdr PRIVATE Z7_ST _7ZIP_ST ZSTD_DISABLE_ASM)
target_compile_options(chdr PRIVATE -w -O2)

# rcheevos' hashing (RetroAchievements, MIT): matching the library to RetroAchievements' games
set(RC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rcheevos)
add_library(rhash STATIC ${RC_DIR}/src/rhash/hash.c ${RC_DIR}/src/rhash/hash_rom.c ${RC_DIR}/src/rhash/hash_disc.c
                         ${RC_DIR}/src/rhash/hash_zip.c ${RC_DIR}/src/rhash/md5.c ${RC_DIR}/src/rhash/cdreader.c
                         ${RC_DIR}/src/rc_compat.c ${RC_DIR}/src/rc_util.c ${RC_DIR}/src/rcheevos/consoleinfo.c)
target_include_directories(rhash PUBLIC ${RC_DIR}/include PRIVATE ${RC_DIR}/src)
target_compile_definitions(rhash PRIVATE RC_HASH_NO_ENCRYPTED)
target_compile_options(rhash PRIVATE -w -O2)

file(GLOB PHOSD_SOURCES CONFIGURE_DEPENDS phosphord/*.c)
add_executable(phosphord ${PHOSD_SOURCES})
target_include_directories(phosphord PRIVATE phosphord third_party/stb)
target_link_libraries(phosphord PRIVATE sqlite3 cjson chdr rhash pthread m)
# Phosphor's own ScreenScraper developer login: a build secret from the environment (PHOSPHOR_SS_DEVID,
# PHOSPHOR_SS_DEVPASSWORD), written to a header in the build folder, never in the repo
if(DEFINED ENV{PHOSPHOR_SS_DEVID} AND DEFINED ENV{PHOSPHOR_SS_DEVPASSWORD})
  set(_id "$ENV{PHOSPHOR_SS_DEVID}")
  set(_pw "$ENV{PHOSPHOR_SS_DEVPASSWORD}")
  foreach(v _id _pw)
    string(REPLACE "\\" "\\\\" ${v} "${${v}}")
    string(REPLACE "\"" "\\\"" ${v} "${${v}}")
  endforeach()
  file(WRITE ${CMAKE_BINARY_DIR}/ss_secret.h "#define PHOS_SS_DEVID \"${_id}\"\n#define PHOS_SS_DEVPASSWORD \"${_pw}\"\n")
  target_compile_definitions(phosphord PRIVATE PHOS_SS_SECRET)
  target_include_directories(phosphord PRIVATE ${CMAKE_BINARY_DIR})
endif()
target_compile_options(phosphord PRIVATE -Wall -Wextra -Wno-unused-parameter)
set_source_files_properties(phosphord/art.c PROPERTIES COMPILE_OPTIONS "-Wno-sign-compare;-Wno-unused-function")
set_source_files_properties(phosphord/ctrl.c phosphord/vtry.c phosphord/sysbk.c PROPERTIES COMPILE_OPTIONS "-Wno-format-truncation")

add_executable(phos phos/phos.c)
# the NVMe drive behind the SS1 dock's JMicron bridge (model, temperature, health)
add_executable(phos-nvme tools/phos-nvme.c)
target_compile_options(phos-nvme PRIVATE -Wall -Wextra)
target_compile_options(phos PRIVATE -Wall -Wextra)
