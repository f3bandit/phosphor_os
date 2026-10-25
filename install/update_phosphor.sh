#!/bin/bash
# Update Phosphor (one-time, for test builds from before Phosphor had its own updater).
# Put phosphor-<version>-armhf.tar.gz in /media/fat/phosphor/updates (or next to this script, or at
# the top of the SD card), then run this from the Scripts menu. From 0.27.6 on, Settings > Update
# Phosphor does the same from the TV or the app.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
FAT=/media/fat
HERE="$(cd "$(dirname "$0")" && pwd)"
pkg="$(ls -1t "$FAT"/phosphor/updates/phosphor-*-armhf.tar.gz "$HERE"/phosphor-*-armhf.tar.gz "$FAT"/phosphor-*-armhf.tar.gz 2>/dev/null | head -1)"
if [ -z "$pkg" ]; then
	echo "No Phosphor package found."
	echo "Copy phosphor-<version>-armhf.tar.gz into phosphor/updates on the SD card (over the network"
	echo "share works), then run this again."
	exit 1
fi
mkdir -p "$FAT/phosphor/updates"
case "$pkg" in "$FAT/phosphor/updates/"*) ;; *) cp -f "$pkg" "$FAT/phosphor/updates/" && pkg="$FAT/phosphor/updates/$(basename "$pkg")" ;; esac
# files at the package's top (phosphor-<version>/...) by exact name: the package also has
# scripts/update.sh (MiSTer's own updater, started from Phosphor), which a '*/update.sh' wildcard matched
top="$(tar -tzf "$pkg" 2>/dev/null | head -1 | cut -d/ -f1)"
v="$(tar -xzOf "$pkg" "$top/VERSION" 2>/dev/null | head -1 | cut -d' ' -f1)"
[ -n "$v" ] || { echo "$(basename "$pkg") is damaged or isn't a Phosphor package."; exit 1; }
echo "Installed: Phosphor $(cut -d' ' -f1 "$FAT/phosphor/VERSION" 2>/dev/null)"
echo "Package:   Phosphor $v ($(basename "$pkg"))"
# the updater from the package itself
u=/tmp/phosphor-update.sh
tar -xzOf "$pkg" "$top/update.sh" > "$u" 2>/dev/null && [ -s "$u" ] || { echo "This package has no updater: it's older than 0.27.6."; exit 1; }
chmod 755 "$u"
PHOS_UPDATE_DELAY=8 "$u" install "$pkg" > /dev/null
echo
echo "The update starts in a few seconds: Phosphor closes, installs $v (the installed"
echo "version is backed up first) and opens again by itself. Your library, box art and"
echo "settings stay. If MiSTer's Main changed, restart once it's back."
echo
echo "Press a button now to close this window."
