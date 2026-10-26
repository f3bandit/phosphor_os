# Cross build for the MiSTer (DE10-Nano: ARM Cortex-A9, hard float).
# Linked statically against musl (MIT), so it doesn't depend on the libraries in MiSTer's Linux and
# carries no LGPL code. The compiler wrapper comes from tools/musl/build-toolchain.sh.
# Threads get 8 MB of stack, as with glibc (musl's own default is 128 KB).
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(PHOS_MUSL "/opt/musl-armhf" CACHE PATH "where tools/musl/build-toolchain.sh put musl")
set(CMAKE_C_COMPILER ${PHOS_MUSL}/bin/arm-musl-gcc)
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -s -Wl,-z,stack-size=8388608")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
