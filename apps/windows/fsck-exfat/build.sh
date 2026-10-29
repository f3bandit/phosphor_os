#!/bin/sh
# Builds fsck.exfat.exe: exfatprogs' fsck.exfat for Windows (Phosphor for Windows' SD card repair).
# exfatprogs, as released upstream (EXFATPROGS_TAG), unchanged; win32/ is the Windows layer under it.
# Needs: git, x86_64-w64-mingw32-gcc (Debian/Ubuntu: gcc-mingw-w64-x86-64).
# Out: fsck.exfat.exe here (go:embed'ed into Phosphor.exe).
set -e
EXFATPROGS_TAG=1.4.3
EXFATPROGS_COMMIT=a37d327
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=${EXFATPROGS_SRC:-$HERE/.src}
if [ ! -d "$SRC/.git" ]; then
	rm -rf "$SRC"
	git clone -q --branch "$EXFATPROGS_TAG" --depth 1 https://github.com/exfatprogs/exfatprogs.git "$SRC"
fi
got=$(git -C "$SRC" rev-parse --short=7 HEAD)
[ "$got" = "$EXFATPROGS_COMMIT" ] || { echo "exfatprogs $EXFATPROGS_TAG is $got, expected $EXFATPROGS_COMMIT" >&2; exit 1; }
CC=${CC:-x86_64-w64-mingw32-gcc}
$CC -O2 -Wall -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE \
	-include "$HERE/win32/compat/win_compat.h" -I"$HERE/win32/compat" -I"$SRC/include" \
	"$SRC/fsck/fsck.c" "$SRC/fsck/repair.c" "$SRC/lib/libexfat.c" "$SRC/lib/exfat_fs.c" \
	"$SRC/lib/exfat_dir.c" "$SRC/lib/utils.c" "$HERE/win32/win32_dev.c" \
	-static -s -o "$HERE/fsck.exfat.exe"
echo "built $HERE/fsck.exfat.exe (exfatprogs $EXFATPROGS_TAG)"
