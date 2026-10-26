#!/bin/bash
# The C library Phosphor's ARM programs are built with: musl (MIT), linked in statically.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   tools/musl/build-toolchain.sh [prefix]      (default /opt/musl-armhf)
#
# Builds musl for the MiSTer's ARM Cortex-A9 with the arm-linux-gnueabihf-gcc cross compiler and puts
# a compiler wrapper, <prefix>/bin/arm-musl-gcc, next to it; frontend/cmake/armhf.cmake uses it.
# musl instead of glibc: glibc is LGPL, and a program linked with it statically has to come with what's
# needed to link it again; musl's MIT licence only asks for its notice (THIRD_PARTY_NOTICES.md).
set -euo pipefail
PREFIX="${1:-/opt/musl-armhf}"
V=1.2.5
SHA=a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT
curl -sSfL -o "$W/musl.tgz" "https://musl.libc.org/releases/musl-$V.tar.gz"
echo "$SHA  $W/musl.tgz" | sha256sum -c --quiet
tar xzf "$W/musl.tgz" -C "$W"
cd "$W/musl-$V"
CROSS_COMPILE=arm-linux-gnueabihf- CFLAGS="-O2 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard" \
	./configure --target=arm-linux-gnueabihf --prefix="$PREFIX" --disable-shared > /dev/null
make -j"$(nproc)" > /dev/null
make install > /dev/null
# the kernel's headers (linux/, asm/...): musl has none of its own
K="$PREFIX/kinclude"
mkdir -p "$K"
for d in linux asm asm-generic mtd sound; do
	[ -d "/usr/arm-linux-gnueabihf/include/$d" ] && cp -r "/usr/arm-linux-gnueabihf/include/$d" "$K/"
done
cat > "$PREFIX/bin/arm-musl-gcc" << EOF
#!/bin/sh
# arm-linux-gnueabihf-gcc against musl (static): Phosphor's ARM programs without glibc
exec arm-linux-gnueabihf-gcc -specs "$PREFIX/lib/musl-gcc.specs" -isystem "$K" "\$@"
EOF
chmod +x "$PREFIX/bin/arm-musl-gcc"
echo "musl $V in $PREFIX: $PREFIX/bin/arm-musl-gcc"
