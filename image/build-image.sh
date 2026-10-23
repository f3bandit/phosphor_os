#!/bin/bash
# Builds a ready-to-flash SD card image: stock MiSTer (MiSTer-devel release) + Phosphor preinstalled.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   image/build-image.sh [phosphor-<v>-armhf.tar.gz] [options]
#     --mister release_YYYYMMDD   MiSTer release to install (default: the newest in SD-Installer-Win64_MiSTer)
#     --mrfusion v2.9             Mr. Fusion version the image is built from
#     --wifi-conf FILE            TESTING ONLY: put this wpa_supplicant.conf on the card (linux/), so the
#                                 SS1 is on WiFi right after installing; the image is named ...-wifi-test
#     --out DIR                   where the .img.zip goes (default: dist/)
#     --cache DIR                 downloads are kept here between builds (default: image/cache/)
#
# How it works: the image is Mr. Fusion (MiSTer-devel's universal installer image, GPL-3.0), unchanged
# except for what its installer copies onto the card and one colour in its HDMI start-up program (no
# blue screen before the splash). On first boot Mr. Fusion expands the card to
# full size and installs the MiSTer release it carries; here that release also holds Phosphor
# (/media/fat/phosphor) and a start block in linux/user-startup.sh, made by Phosphor's own install.sh
# --boot-phosphor. No kernel or Linux of our own; Phosphor's Main runs in place of MiSTer's, which isn't replaced. Nothing here needs root: the image is put together
# with sfdisk, mkfs.vfat, mtools and dd on plain files.
#
# Needs: curl, git, 7z (7-Zip or p7zip), sfdisk, mkfs.vfat, mtools, unzip, zip, sha256sum.
set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$PWD"
MRF_VER="v2.9"
MISTER=""
WIFI_CONF=""
OUT="$REPO/dist"
CACHE="$REPO/image/cache"
TARBALL=""
while [ $# -gt 0 ]; do
	case "$1" in
		--mister) MISTER="$2"; shift 2 ;;
		--mrfusion) MRF_VER="$2"; shift 2 ;;
		--out) OUT="$2"; shift 2 ;;
		--wifi-conf) WIFI_CONF="$2"; shift 2 ;;
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
# No blue screen before the splash: Mr. Fusion's bootloader runs de10_nano_hdmi_config.bin, which paints
# the screen a fixed blue (0x0071C5) before it looks for STARTUP.BMP. Change that one constant (Thumb
# MOVW r6,#0x71C5 at 0x9C6) to the splash's dark background. Checked byte for byte, so any other
# version of the file stops the build instead of being patched blind.
HCFG="$WORK/fat/de10_nano_hdmi_config.bin"
python3 - "$HCFG" <<'PY' || { echo "de10_nano_hdmi_config.bin isn't the expected version" >&2; exit 1; }
import sys
p = sys.argv[1]; d = bytearray(open(p, 'rb').read())
old = bytes.fromhex('47f2c516')  # movw r6, #0x71c5
new = bytes.fromhex('40f61056')  # movw r6, #0x0d10
assert d.count(old) == 1 and d[0x9c6:0x9ca] == old
d[0x9c6:0x9ca] = new
open(p, 'wb').write(d)
PY

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
PHOS_FAT="$WORK/rel/files" PHOS_FAT_TARGET=/media/fat bash "$PDIR/install.sh" --boot-phosphor
grep -q '^# phosphor: BEGIN' "$WORK/rel/files/linux/user-startup.sh" || { echo "boot block missing" >&2; exit 1; }
grep -q ' /media/fat/phosphor/phosphor.sh boot' "$WORK/rel/files/linux/user-startup.sh" || { echo "boot block has the wrong path" >&2; exit 1; }
grep -q "^main=phosphor/bin/MiSTer_phosphor" "$WORK/rel/files/MiSTer.ini" || { echo "MiSTer.ini main= missing" >&2; exit 1; }
# the modules are in the image, their boot entries point at the card (not at this machine)
for m in cifs-helper backup-agent health-watch update-guard cloud-sync; do
	[ -x "$WORK/rel/files/phosphor/modules/$m/$m.sh" ] || { echo "module $m isn't in the image" >&2; exit 1; }
	grep -q "phosphor $m: BEGIN\|phosphor $m" "$WORK/rel/files/linux/user-startup.sh" || { echo "module $m has no boot entry" >&2; exit 1; }
done
if grep -q "$WORK" "$WORK/rel/files/linux/user-startup.sh"; then echo "user-startup.sh points at the build folder" >&2; exit 1; fi
cmp -s "$WORK/rel/files/menu.rbf" "$WORK/rel/files/phosphor/cores/menu_phosphor.rbf" || { echo "menu.rbf isn't Phosphor's menu core" >&2; exit 1; }
# MiSTer's video filters, shadow masks, gamma tables and presets (MiSTer-devel), so Screen filters works
# from the start; the downloader keeps them current later
say "MiSTer's filters"
for r in Filters_MiSTer ShadowMasks_MiSTer Presets_MiSTer; do
	[ -d "$CACHE/$r/.git" ] || git clone -q --depth 1 "https://github.com/MiSTer-devel/$r" "$CACHE/$r"
done
cp -r "$CACHE/Filters_MiSTer/Filters" "$CACHE/Filters_MiSTer/Filters_Audio" "$CACHE/Filters_MiSTer/Gamma" "$WORK/rel/files/"
cp -r "$CACHE/ShadowMasks_MiSTer/Shadow_Masks" "$WORK/rel/files/"
cp -r "$CACHE/Presets_MiSTer/Presets" "$WORK/rel/files/"
# Update All (theypsilon, GPL-3.0): its bootstrap script, as people install it (it fetches the rest itself)
UA="$CACHE/update_all.sh"
[ -s "$UA" ] || curl -fsSL --retry 3 -o "$UA" "https://raw.githubusercontent.com/theypsilon/Update_All_MiSTer/master/update_all.sh"
head -1 "$UA" | grep -q '^#!/bin/bash' || { echo "update_all.sh doesn't look right" >&2; exit 1; }
mkdir -p "$WORK/rel/files/Scripts" && cp "$UA" "$WORK/rel/files/Scripts/update_all.sh" && chmod 755 "$WORK/rel/files/Scripts/update_all.sh"
# the Phosphor release itself, so modules can be installed later without downloading anything
# the SS1 documentation's recommended downloader.ini (games and docs to the NVMe/USB drive when there
# is one), with Linux updates left on
DLI="$REPO/image/downloader.ini"
grep -q "^storage_priority = 'prefer_external'" "$DLI" && grep -q "^update_linux = true" "$DLI" || { echo "image/downloader.ini doesn't look right" >&2; exit 1; }
cp "$DLI" "$WORK/rel/files/downloader.ini"
cp "$TARBALL" "$WORK/rel/files/phosphor/"
# testing only: a WiFi network the SS1 joins straight away (never in a release image)
if [ -n "$WIFI_CONF" ]; then
	say "WiFi settings from $WIFI_CONF (test image)"
	sed 's/\r$//' "$WIFI_CONF" > "$WORK/rel/files/linux/wpa_supplicant.conf"
fi

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

NAME="phosphor-$PHOS_VER-sdcard-${MISTER#release_}${WIFI_CONF:+-wifi-test}"
mv "$IMG" "$WORK/$NAME.img"
rm -f "$OUT/$NAME.img.zip"
(cd "$WORK" && zip -q -9 "$OUT/$NAME.img.zip" "$NAME.img")
(cd "$OUT" && sha256sum "$NAME.img.zip" > "$NAME.img.zip.sha256")
cat > "$OUT/$NAME.txt" <<EOF
Phosphor $PHOS_VER SD card image
  MiSTer release: $MISTER (https://github.com/MiSTer-devel/SD-Installer-Win64_MiSTer)
  Mr. Fusion:     $MRF_VER (https://github.com/MiSTer-devel/mr-fusion, GPL-3.0; source and
                  licences of the Linux kernel and tools it contains: its legal-info folder);
                  de10_nano_hdmi_config.bin's background colour changed from blue to dark
  Update All:     Scripts/update_all.sh (https://github.com/theypsilon/Update_All_MiSTer, GPL-3.0)
  Phosphor:       $(cat "$PDIR/VERSION")
EOF
# this version's section of docs/changelog.md (fixes and changes since the last image)
CHANGES="$(awk -v v="## $PHOS_VER" '$0 == v { on = 1; next } on && /^## / { exit } on' "$REPO/docs/changelog.md" 2>/dev/null)"
[ -n "$CHANGES" ] && printf '\nWhat changed in %s\n%s\n' "$PHOS_VER" "$CHANGES" >> "$OUT/$NAME.txt"
say "Done"
ls -l "$OUT/$NAME.img.zip"
