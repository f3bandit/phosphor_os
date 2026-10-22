#!/bin/bash
# Phosphor installer. Run from the unpacked release folder on the MiSTer.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   install.sh             copy Phosphor to /media/fat/phosphor; nothing starts at boot
#   install.sh --boot      also start phosphord (library service, not the shell) at boot,
#                          through a marked block in linux/user-startup.sh
#   install.sh --boot-phosphor
#                          boot into Phosphor: phosphord, Phosphor's Main in place of MiSTer's, the shell
#                          (the same block; MiSTer's menu is still there: Launch Phosphor, or quit the shell)
#
# Nothing outside /media/fat/phosphor is changed, except user-startup.sh with --boot (backed up
# first; uninstall.sh takes the block out again). Console Mode and MiSTer's settings are not touched.
# Reinstalling keeps the library, art, settings and logs.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
FAT="${PHOS_FAT:-/media/fat}"
ROOT="$FAT/phosphor"
# where that folder is on the MiSTer, for the boot block (differs only when building an SD card image)
RUNROOT="${PHOS_FAT_TARGET:-$FAT}/phosphor"
STARTUP="$FAT/linux/user-startup.sh"
TAG="phosphor"
BOOT=0
[ "${1:-}" = "--boot" ] && BOOT=1
[ "${1:-}" = "--boot-phosphor" ] && BOOT=2

for f in bin/phosphor bin/phosphord bin/phos phosphor.sh uninstall.sh VERSION; do
	[ -e "$HERE/$f" ] || { echo "missing $f: run this from the unpacked Phosphor folder" >&2; exit 1; }
done
[ -d "$FAT" ] || { echo "$FAT not found" >&2; exit 1; }

# stop a running copy before replacing it
[ -x "$ROOT/phosphor.sh" ] && "$ROOT/phosphor.sh" stop > /dev/null 2>&1

mkdir -p "$ROOT/bin" "$ROOT/config" "$ROOT/data" "$ROOT/logs" "$ROOT/backup" || exit 1
for f in phosphor phosphord phos; do
	cp "$HERE/bin/$f" "$ROOT/bin/$f.new" && chmod 755 "$ROOT/bin/$f.new" && mv -f "$ROOT/bin/$f.new" "$ROOT/bin/$f" || { echo "couldn't copy $f" >&2; exit 1; }
done
# phosphor-main (Phosphor's Main fork): only copied; nothing starts it or replaces MiSTer's Main
if [ -f "$HERE/bin/MiSTer_phosphor" ]; then
	cp "$HERE/bin/MiSTer_phosphor" "$ROOT/bin/MiSTer_phosphor.new" && chmod 755 "$ROOT/bin/MiSTer_phosphor.new" && mv -f "$ROOT/bin/MiSTer_phosphor.new" "$ROOT/bin/MiSTer_phosphor"
fi
# the Phosphor menu core (front ports): only kept here; Phosphor loads it, menu.rbf is never replaced
if [ -f "$HERE/cores/menu_phosphor.rbf" ]; then
	mkdir -p "$ROOT/cores" && cp "$HERE/cores/menu_phosphor.rbf" "$ROOT/cores/menu_phosphor.rbf.new" && mv -f "$ROOT/cores/menu_phosphor.rbf.new" "$ROOT/cores/menu_phosphor.rbf"
fi
cp "$HERE/phosphor.sh" "$HERE/uninstall.sh" "$ROOT/" && chmod 755 "$ROOT/phosphor.sh" "$ROOT/uninstall.sh"
cp "$HERE/VERSION" "$ROOT/VERSION"
# example settings: only when there isn't one already
for f in "$HERE"/config/*.ini; do
	[ -e "$f" ] || continue
	n="${f##*/}"
	[ -e "$ROOT/config/$n" ] || cp "$f" "$ROOT/config/$n.example"
done
# diag (whole-system checks, Settings > Diagnostics) only runs when asked: always installed.
# The other modules change how the MiSTer boots, so they're installed one by one, by hand:
#   modules/<name>/install.sh  (cifs-helper, backup-agent, health-watch, update-guard, cloud-sync)
if [ -x "$HERE/modules/diag/install.sh" ]; then
	PHOSPHOR_ROOT="$ROOT" "$HERE/modules/diag/install.sh" > /dev/null || echo "note: the diag module didn't install" >&2
fi

if [ "$BOOT" != 0 ]; then
	mkdir -p "${STARTUP%/*}"
	if [ -f "$STARTUP" ] && ! grep -q "^# $TAG: BEGIN" "$STARTUP"; then
		cp "$STARTUP" "$ROOT/backup/user-startup.sh.$(date +%Y%m%d-%H%M%S)"
	fi
	[ -f "$STARTUP" ] || printf '#!/bin/sh\n\n' > "$STARTUP"
	tmp="$(mktemp)"
	awk -v t="$TAG" '$0 == "# " t ": BEGIN" {skip=1} !skip {print} $0 == "# " t ": END" {skip=0}' "$STARTUP" > "$tmp"
	# S99user also runs user-startup.sh with "stop" at shutdown: start only on start
	WHAT=service; [ "$BOOT" = 2 ] && WHAT=boot
	printf '# %s: BEGIN\n[ "${1:-start}" = start ] && %s/phosphor.sh %s > /dev/null 2>&1 &\n# %s: END\n' "$TAG" "$RUNROOT" "$WHAT" "$TAG" >> "$tmp"
	cat "$tmp" > "$STARTUP" && rm -f "$tmp" && chmod +x "$STARTUP"
	if [ "$BOOT" = 2 ]; then echo "boots into Phosphor (block \"$TAG\" in $STARTUP)"
	else echo "phosphord will start at boot (block \"$TAG\" in $STARTUP)"; fi
fi

echo "Phosphor $(cat "$ROOT/VERSION") installed in $ROOT"
echo "  start:  $ROOT/phosphor.sh start     stop: $ROOT/phosphor.sh stop"
echo "  remove: $ROOT/uninstall.sh"
[ -d "$HERE/modules" ] && echo "  modules (each optional): $HERE/modules/<name>/install.sh"
