#!/bin/bash
# Phosphor uninstaller.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   uninstall.sh            stop Phosphor, take its boot entry out, remove the programs;
#                           keep the library, art, settings and logs in /media/fat/phosphor
#   uninstall.sh --all      remove /media/fat/phosphor completely
set -u
FAT="${PHOS_FAT:-/media/fat}"
ROOT="$FAT/phosphor"
STARTUP="$FAT/linux/user-startup.sh"
TAG="phosphor"

[ -x "$ROOT/phosphor.sh" ] && "$ROOT/phosphor.sh" stop > /dev/null 2>&1
# MiSTer's own SSH again (its own password, no keys-only)
[ -x "$ROOT/bin/phos-ssh.sh" ] && "$ROOT/bin/phos-ssh.sh" stock > /dev/null 2>&1
# MiSTer.ini: take out Phosphor's main= and put back the one that was there
INI="$FAT/MiSTer.ini"
if [ -f "$INI" ] && grep -q "^main=phosphor/bin/MiSTer_phosphor" "$INI"; then
	prev="$(head -1 "$ROOT/backup/main.prev" 2>/dev/null)"
	tmp="$(mktemp)"
	awk -v p="$prev" '/^main=phosphor\/bin\/MiSTer_phosphor/ { if (p != "") print "main=" p "\r"; next } { print }' "$INI" > "$tmp" && cat "$tmp" > "$INI"
	rm -f "$tmp"
	echo "MiSTer.ini: main= put back (${prev:-the default})"
fi
# menu.rbf: MiSTer's back, if Phosphor's is there
# Phosphor's picture behind MiSTer's menu, if it's still the one Phosphor put there
if [ -f "$ROOT/backup/menu.png.phosphor" ] && [ "$(md5sum "$FAT/menu.png" 2>/dev/null | cut -c1-32)" = "$(cat "$ROOT/backup/menu.png.phosphor")" ]; then
	rm -f "$FAT/menu.png" && echo "menu.png: Phosphor's picture taken away"
fi
if [ -f "$ROOT/backup/menu.rbf.mister" ] && [ -f "$ROOT/cores/menu_phosphor.rbf" ] && cmp -s "$ROOT/cores/menu_phosphor.rbf" "$FAT/menu.rbf"; then
	cp "$ROOT/backup/menu.rbf.mister" "$FAT/menu.rbf.new" && mv -f "$FAT/menu.rbf.new" "$FAT/menu.rbf" && echo "menu.rbf: MiSTer's put back"
fi
ENTRY="$FAT/Scripts/phosphor.sh"
if [ -f "$ENTRY" ] && grep -q "^# phosphor: menu entry" "$ENTRY"; then rm -f "$ENTRY" && echo "menu entry removed"; fi
if [ -f "$STARTUP" ] && grep -q "^# $TAG: BEGIN" "$STARTUP"; then
	tmp="$(mktemp)"
	awk -v t="$TAG" '$0 == "# " t ": BEGIN" {skip=1} !skip {print} $0 == "# " t ": END" {skip=0}' "$STARTUP" > "$tmp"
	cat "$tmp" > "$STARTUP" && rm -f "$tmp"
	echo "boot entry removed from $STARTUP"
fi
rm -f "$ROOT/bin/phosphor" "$ROOT/bin/phosphord" "$ROOT/bin/phos" "$ROOT/phosphor.sh" "$ROOT/VERSION" "$ROOT/bin/MiSTer_phosphor" "$ROOT/cores/menu_phosphor.rbf"
rmdir "$ROOT/bin" 2>/dev/null
rmdir "$ROOT/cores" 2>/dev/null
rm -f /tmp/phosphor/phosphord.sock
if [ "${1:-}" = "--all" ]; then
	rm -rf "$ROOT"
	echo "Phosphor removed completely"
else
	rm -f "$ROOT/uninstall.sh"
	echo "Phosphor removed; library, art, settings and logs kept in $ROOT (uninstall.sh --all removes them too)"
fi
