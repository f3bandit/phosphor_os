#!/bin/sh
# Builds the dock's IR receiver firmware: Retro-Remake's DockIR at a known commit, with Phosphor's patch
# (ss1update.patch: updating it from the SuperStation). Retro-Remake's source isn't kept here (the DockIR
# repository has no licence file): it's fetched from GitHub at DOCKIR_COMMIT and patched.
# Needs: git, cmake, make, arm-none-eabi-gcc with newlib (Debian/Ubuntu: gcc-arm-none-eabi
# libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib). Writes dockir-<id>.uf2 next to this script.
# Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see ../../LICENSE.md).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
DOCKIR_COMMIT=9403adc160d46d62bbb1c21d157ff83a4b1dc105
PICO_SDK_TAG=2.3.0      # DockIR's PR #4 fixed building with pico-sdk 2.3.0 (2.2.0 doesn't link it)
W="${WORK:-$HERE/build}"
mkdir -p "$W"
[ -d "$W/pico-sdk" ] || { git clone -q --depth 1 -b "$PICO_SDK_TAG" https://github.com/raspberrypi/pico-sdk.git "$W/pico-sdk"
	git -C "$W/pico-sdk" submodule update -q --init --depth 1 lib/tinyusb; }
rm -rf "$W/DockIR"
git clone -q https://github.com/Retro-Remake/DockIR.git "$W/DockIR"
git -C "$W/DockIR" checkout -q "$DOCKIR_COMMIT"
git -C "$W/DockIR" apply "$HERE/ss1update.patch"
id=$(sed -n 's/^#define FW_ID *"\([A-Z0-9]*\)".*/\1/p' "$W/DockIR/src/main.c")
mkdir -p "$W/DockIR/build"
(cd "$W/DockIR/build" && PICO_SDK_PATH="$W/pico-sdk" cmake .. > /dev/null && make -j"$(nproc)" > /dev/null)
cp "$W/DockIR/build/pico_ir_keyboard.uf2" "$HERE/dockir-$id.uf2"
echo "built $HERE/dockir-$id.uf2 ($(sha256sum "$HERE/dockir-$id.uf2" | cut -c1-16)...)"
