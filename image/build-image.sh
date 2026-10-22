#!/bin/bash
# Builds a ready-to-flash SD card image: stock MiSTer (MiSTer-devel release) + Phosphor preinstalled.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   image/build-image.sh [phosphor-<v>-armhf.tar.gz] [options]
#     --mister release_YYYYMMDD   MiSTer release to install (default: the newest in SD-Installer-Win64_MiSTer)
#     --mrfusion v2.9             Mr. Fusion version the image is built from
#     --out DIR                   where the .img.zip goes (default: dist/)
#     --cache DIR                 downloads are kept here between builds (default: image/cache/)
#
# How it works: the image is Mr. Fusion (MiSTer-devel's universal installer image, GPL-3.0), unchanged
# except for what its installer copies onto the card. On first boot Mr. Fusion expands the card to
# full size and installs the MiSTer release it carries; here that release also holds Phosphor
# (/media/fat/phosphor) and a start block in linux/user-startup.sh, made by Phosphor's own install.sh
# --boot. No kernel, Linux or Main of our own. Nothing here needs root: the image is put together
# with sfdisk, mkfs.vfat, mtools and dd on plain files.
#
# Needs: curl, git, 7z (7-Zip or p7zip), sfdisk, mkfs.vfat, mtools, unzip, zip, sha256sum.
set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$PWD"
MRF_VER="v2.9"
MISTER=""
OUT="$REPO/dist"
CACHE="$REPO/image/cache"
TARBALL=""
while [ $# -gt 0 ]; do
	case "$1" in
		--mister) MISTER="$2"; shift 2 ;;
		--mrfusion) MRF_VER="$2"; shift 2 ;;
		--out) OUT="$2"; shift 2 ;;
		--cache) CACHE="$2"; shift 2 ;;
		-h|--help) sed -n '2,18p' "$0"; exit 0 ;;
		*) TARBALL="$1"; shift ;;
	esac
done
if [ -z "$TARBALL" ]; then
	TARBALL="$(ls -t dist/phosphor-*-armhf.tar.gz 2>/dev/null | head -1 || true)"
	[ -n "$TARBALL" ] || { echo "no Phosphor release: run install/build-release.sh or give the .tar.gz" >&2; exit 1; }
fi
TARBALL="$(readlink -f "$TARBALL")"
for t in curl git 7z sfdisk mkfs.vfat mcopy mmd unzip zip sha256sum; do
	command -v "$t" > /dev/null || { echo "missing tool: $t" >&2; exit 1; }
done
export MTOOLS_SKIP_CHECK=1
mkdir -p "$CACHE" "$OUT"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/phos-image.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
say() { printf '\n== %s\n' "$*"; }

# ---- 1. Mr. Fusion
MRF_ZIP="$CACHE/mr-fusion-$MRF_VER.img.zip"
if [ ! -s "$MRF_ZIP" ]; then
	say "Mr. Fusion $MRF_VER"
	curl -fL --retry 3 -o "$MRF_ZIP.part" "https://github.com/MiSTer-devel/mr-fusion/releases/download/$MRF_VER/mr-fusion-$MRF_VER.img.zip"
	mv "$MRF_ZIP.part" "$MRF_ZIP"
fi
unzip -q -o "$MRF_ZIP" -d "$WORK/mrf"
MRF_IMG="$(ls "$WORK"/mrf/*.img | head -1)"
# its layout: partition 1 = FAT "MRFUSION" (kernel, installer files), partition 2 = type a2 (bootloader)
read -r P1_START P2_START P2_SIZE < <(sfdisk -d "$MRF_IMG" | awk -F'[=, ]+' '
	/img1 :/ {for (i=1;i<=NF;i++) if ($i=="start") s1=$(i+1)}
	/img2 :/ {for (i=1;i<=NF;i++) {if ($i=="start") s2=$(i+1); if ($i=="size") z2=$(i+1)}}
	END {print s1, s2, z2}')
[ -n "$P1_START" ] && [ -n "$P2_START" ] && [ -n "$P2_SIZE" ] || { echo "Mr. Fusion image layout not recognised" >&2; exit 1; }
mkdir -p "$WORK/fat"
mcopy -s -n -i "$MRF_IMG@@$((P1_START * 512))" "::*" "$WORK/fat/"
dd if="$MRF_IMG" of="$WORK/boot.img" bs=512 skip="$P2_START" count="$P2_SIZE" status=none
for f in zImage release.7z extlinux/extlinux.conf; do
	[ -e "$WORK/fat/$f" ] || { echo "Mr. Fusion image has no $f" >&2; exit 1; }
done
rm -f "$WORK/fat/release.7z"

# ---- 2. MiSTer release (the same files the official SD installer uses)
SDI="https://github.com/MiSTer-devel/SD-Installer-Win64_MiSTer"
if [ -z "$MISTER" ]; then
	say "Finding the newest MiSTer release"
	git clone -q --depth 1 --filter=blob:none --no-checkout "$SDI" "$WORK/sdi"
	MISTER="$(git -C "$WORK/sdi" ls-tree --name-only HEAD | sed -n 's/^\(release_[0-9]\{8\}\)\.7z.*/\1/p' | sort -u | tail -1)"
	PARTS="$(git -C "$WORK/sdi" ls-tree --name-only HEAD | grep "^$MISTER\.7z" | sort)"
else
	git clone -q --depth 1 --filter=blob:none --no-checkout "$SDI" "$WORK/sdi"
	PARTS="$(git -C "$WORK/sdi" ls-tree --name-only HEAD | grep "^$MISTER\.7z" | sort || true)"
fi
[ -n "$MISTER" ] && [ -n "$PARTS" ] || { echo "MiSTer release ${MISTER:-?} not found in $SDI" >&2; exit 1; }
REL="$CACHE/$MISTER.7z"
if [ ! -s "$REL" ]; then
	say "MiSTer $MISTER"
	: > "$REL.part"
	for p in $PARTS; do
		curl -fL --retry 3 "https://raw.githubusercontent.com/MiSTer-devel/SD-Installer-Win64_MiSTer/master/$p" >> "$REL.part"
	done
	mv "$REL.part" "$REL"
fi
mkdir -p "$WORK/rel"
7z x -y -bd -o"$WORK/rel" "$REL" > /dev/null
[ -f "$WORK/rel/files/linux/linux.img" ] || { echo "$MISTER doesn't look like a MiSTer release" >&2; exit 1; }

# ---- 3. Phosphor, installed with its own installer into the release's files/
say "Phosphor from ${TARBALL##*/}"
mkdir -p "$WORK/phos"
tar -C "$WORK/phos" -xzf "$TARBALL"
PDIR="$(ls -d "$WORK"/phos/phosphor-* | head -1)"
PHOS_VER="$(cut -d' ' -f1 "$PDIR/VERSION")"
PHOS_FAT="$WORK/rel/files" PHOS_FAT_TARGET=/media/fat bash "$PDIR/install.sh" --boot
grep -q '^# phosphor: BEGIN' "$WORK/rel/files/linux/user-startup.sh" || { echo "boot block missing" >&2; exit 1; }
grep -q ' /media/fat/phosphor/phosphor.sh service' "$WORK/rel/files/linux/user-startup.sh" || { echo "boot block has the wrong path" >&2; exit 1; }
# the Phosphor release itself, so modules can be installed later without downloading anything
cp "$TARBALL" "$WORK/rel/files/phosphor/"

# ---- 4. Repack. Plain LZMA2 without filters, so Mr. Fusion's older 7zr unpacks it.
say "Packing the release"
(cd "$WORK/rel" && 7z a -t7z -bd -mx=7 -m0=LZMA2 -mf=off -ms=on "$WORK/fat/release.7z" . > /dev/null)
cp image/splash.png "$WORK/fat/splash.png"

# ---- 5. The image: same layout as Mr. Fusion, partition 1 made big enough
need=$(du -s -B1M "$WORK/fat" | cut -f1)
P1_MB=$(( need + need / 10 + 16 ))
P1_SECT=$(( P1_MB * 2048 ))
IMG="$WORK/phosphor.img"
truncate -s $(( (P1_START + P1_SECT) * 512 )) "$IMG"
sfdisk --quiet --no-reread "$IMG" > /dev/null <<EOF
start=$P1_START, size=$P1_SECT, type=0b
start=$P2_START, size=$P2_SIZE, type=a2
EOF
dd if="$WORK/boot.img" of="$IMG" bs=512 seek="$P2_START" conv=notrunc status=none
truncate -s $(( P1_SECT * 512 )) "$WORK/p1.img"
mkfs.vfat -n MRFUSION "$WORK/p1.img" > /dev/null
mcopy -s -i "$WORK/p1.img" "$WORK"/fat/* ::/
dd if="$WORK/p1.img" of="$IMG" bs=512 seek="$P1_START" conv=notrunc status=none

NAME="phosphor-$PHOS_VER-sdcard-${MISTER#release_}"
mv "$IMG" "$WORK/$NAME.img"
rm -f "$OUT/$NAME.img.zip"
(cd "$WORK" && zip -q -9 "$OUT/$NAME.img.zip" "$NAME.img")
(cd "$OUT" && sha256sum "$NAME.img.zip" > "$NAME.img.zip.sha256")
cat > "$OUT/$NAME.txt" <<EOF
Phosphor $PHOS_VER SD card image
  MiSTer release: $MISTER (https://github.com/MiSTer-devel/SD-Installer-Win64_MiSTer)
  Mr. Fusion:     $MRF_VER (https://github.com/MiSTer-devel/mr-fusion, GPL-3.0; source and
                  licences of the Linux kernel and tools it contains: its legal-info folder)
  Phosphor:       $(cat "$PDIR/VERSION")
EOF
say "Done"
ls -l "$OUT/$NAME.img.zip"
