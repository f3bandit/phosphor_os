# Cross build for the MiSTer (DE10-Nano: ARM Cortex-A9, hard float).
# Linked statically, so it doesn't depend on the libraries in MiSTer's Linux (glibc 2.31 there).
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -s")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
