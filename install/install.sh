#!/bin/bash
# Phosphor installer. Run from the unpacked release folder on the MiSTer.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   install.sh             copy Phosphor to /media/fat/phosphor; nothing starts at boot
#   install.sh --boot      also start phosphord (library service, not the shell) at boot,
#                          through a marked block in linux/user-startup.sh
#   install.sh --boot-phosphor
#                          boot into Phosphor: main=phosphor/bin/MiSTer_phosphor in MiSTer.ini (the way
#                          every MiSTer frontend does it; backed up first, uninstall.sh puts it back),
#                          phosphord and the shell from the same user-startup.sh block
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
# phos-nvme: the NVMe drive's health and temperature behind the dock's bridge (storage checks, Dashboard)
if [ -f "$HERE/bin/phos-nvme" ]; then
	cp "$HERE/bin/phos-nvme" "$ROOT/bin/phos-nvme.new" && chmod 755 "$ROOT/bin/phos-nvme.new" && mv -f "$ROOT/bin/phos-nvme.new" "$ROOT/bin/phos-nvme"
fi
# phos-ssh.sh: SSH for Settings > Remote access (Off, Password, Keys only) and its copy on another drive
if [ -f "$HERE/bin/phos-ssh.sh" ]; then
	cp "$HERE/bin/phos-ssh.sh" "$ROOT/bin/phos-ssh.sh.new" && chmod 755 "$ROOT/bin/phos-ssh.sh.new" && mv -f "$ROOT/bin/phos-ssh.sh.new" "$ROOT/bin/phos-ssh.sh"
fi
# phosphor-main (Phosphor's Main fork): only copied; nothing starts it or replaces MiSTer's Main
if [ -f "$HERE/bin/MiSTer_phosphor" ]; then
	cp "$HERE/bin/MiSTer_phosphor" "$ROOT/bin/MiSTer_phosphor.new" && chmod 755 "$ROOT/bin/MiSTer_phosphor.new" && mv -f "$ROOT/bin/MiSTer_phosphor.new" "$ROOT/bin/MiSTer_phosphor"
fi
# the Phosphor menu core (front ports): only kept here; Phosphor loads it, menu.rbf is never replaced
if [ -f "$HERE/cores/menu_phosphor.rbf" ]; then
	mkdir -p "$ROOT/cores" && cp "$HERE/cores/menu_phosphor.rbf" "$ROOT/cores/menu_phosphor.rbf.new" && mv -f "$ROOT/cores/menu_phosphor.rbf.new" "$ROOT/cores/menu_phosphor.rbf"
fi
cp "$HERE/phosphor.sh" "$HERE/uninstall.sh" "$ROOT/" && chmod 755 "$ROOT/phosphor.sh" "$ROOT/uninstall.sh"
# Phosphor's picture behind MiSTer's own menu: MiSTer's menu.png, only when there's no menu.png or
# menu.jpg of your own (uninstall.sh takes it away again, if it's still Phosphor's)
if [ -f "$HERE/wallpaper/phosphor-wallpaper.png" ]; then
	if [ ! -e "$FAT/menu.png" ] && [ ! -e "$FAT/menu.jpg" ]; then
		cp "$HERE/wallpaper/phosphor-wallpaper.png" "$FAT/menu.png.new" && mv -f "$FAT/menu.png.new" "$FAT/menu.png" && \
			md5sum "$FAT/menu.png" | cut -c1-32 > "$ROOT/backup/menu.png.phosphor" && echo "menu.png: Phosphor's picture behind MiSTer's menu"
	elif [ -f "$ROOT/backup/menu.png.phosphor" ] && [ "$(md5sum "$FAT/menu.png" 2>/dev/null | cut -c1-32)" = "$(cat "$ROOT/backup/menu.png.phosphor")" ]; then
		# still ours: the new version of it
		cp "$HERE/wallpaper/phosphor-wallpaper.png" "$FAT/menu.png.new" && mv -f "$FAT/menu.png.new" "$FAT/menu.png" && \
			md5sum "$FAT/menu.png" | cut -c1-32 > "$ROOT/backup/menu.png.phosphor"
	fi
fi
# the updater (Settings > Update Phosphor), when the package has one
[ -f "$HERE/update.sh" ] && cp "$HERE/update.sh" "$ROOT/update.sh" && chmod 755 "$ROOT/update.sh"
# MiSTer's updaters as Phosphor starts them (a restore point first, then the updater)
if [ -d "$HERE/scripts" ]; then
	mkdir -p "$ROOT/scripts" && cp "$HERE"/scripts/*.sh "$ROOT/scripts/" && chmod 755 "$ROOT"/scripts/*.sh
fi
cp "$HERE/VERSION" "$ROOT/VERSION"
# example settings: only when there isn't one already
for f in "$HERE"/config/*.ini; do
	[ -e "$f" ] || continue
	n="${f##*/}"
	[ -e "$ROOT/config/$n" ] || cp "$f" "$ROOT/config/$n.example"
done
# diag (whole-system checks, Settings > Diagnostics) and storage (SD / NVMe / USB checks, Settings >
# Storage) only run when asked: always installed.
# The other modules change how the MiSTer boots, so they're installed one by one, by hand:
#   modules/<name>/install.sh  (cifs-helper, backup-agent, health-watch, update-guard, cloud-sync)
for m in diag storage; do
	if [ -x "$HERE/modules/$m/install.sh" ]; then
		PHOSPHOR_ROOT="$ROOT" "$HERE/modules/$m/install.sh" > /dev/null || echo "note: the $m module didn't install" >&2
	fi
done

if [ "$BOOT" != 0 ]; then
	mkdir -p "${STARTUP%/*}"
	if [ -f "$STARTUP" ] && ! grep -q "^# $TAG: BEGIN" "$STARTUP"; then
		cp "$STARTUP" "$ROOT/backup/user-startup.sh.$(date +%Y%m%d-%H%M%S)"
	fi
	[ -f "$STARTUP" ] || printf '#!/bin/sh\n\n' > "$STARTUP"
	tmp="$(mktemp)"
	awk -v t="$TAG" '$0 == "# " t ": BEGIN" {skip=1} !skip {print} $0 == "# " t ": END" {skip=0}' "$STARTUP" > "$tmp"
	# S99user also runs user-startup.sh with "stop" at shutdown: start only on start; on stop (any
	# restart that goes through init, the downloader's after an update included), Phosphor's menu core
	# goes back in as menu.rbf so the restart shows Phosphor's loading screen, not MiSTer's static
	WHAT=service; [ "$BOOT" = 2 ] && WHAT=boot
	printf '# %s: BEGIN\n[ "${1:-start}" = start ] && %s/phosphor.sh %s > /dev/null 2>&1 &\n[ "$1" = stop ] && %s/phosphor.sh menu-core > /dev/null 2>&1 || :\n# %s: END\n' "$TAG" "$RUNROOT" "$WHAT" "$RUNROOT" "$TAG" >> "$tmp"
	cat "$tmp" > "$STARTUP" && rm -f "$tmp" && chmod +x "$STARTUP"
	if [ "$BOOT" = 2 ]; then echo "boots into Phosphor (block \"$TAG\" in $STARTUP)"
	else echo "phosphord will start at boot (block \"$TAG\" in $STARTUP)"; fi
fi

# booting into Phosphor: every module too, since Settings uses them (Backups, Cloud saves, shares,
# health, update safety). On the MiSTer: now. For an SD card image: into the image (files, settings,
# boot entries; nothing started), so the first boot has nothing left to install except the part of
# cifs-helper that lives inside Linux (its network hook), which a quick first-boot step does.
MODS="cifs-helper backup-agent health-watch update-guard cloud-sync"
if [ "$BOOT" = 2 ]; then
	for m in $MODS; do
		[ -x "$HERE/modules/$m/install.sh" ] || continue
		if [ -n "${PHOS_FAT_TARGET:-}" ]; then
			PHOSPHOR_OFFLINE=1 PHOSPHOR_ROOT="$ROOT" PHOSPHOR_TARGET_ROOT="$RUNROOT" PHOSPHOR_USER_STARTUP="$STARTUP" \
				"$HERE/modules/$m/install.sh" > /dev/null 2>&1 && echo "module $m installed (image)" || { echo "module $m didn't install" >&2; exit 1; }
		else
			PHOSPHOR_ROOT="$ROOT" "$HERE/modules/$m/install.sh" > /dev/null 2>&1 && echo "module $m installed" || echo "note: module $m didn't install" >&2
		fi
	done
	[ -n "${PHOS_FAT_TARGET:-}" ] && echo "cifs-helper" > "$ROOT/firstboot"
fi

# Phosphor's menu core as menu.rbf: the bootloader loads menu.rbf before Linux starts, and Phosphor's
# shows black instead of MiSTer's static until Phosphor is on screen. MiSTer's own is kept in
# phosphor/backup/menu.rbf.mister (uninstall.sh puts it back); an update that brings a new one back is
# kept there too, and Phosphor's goes in again at the next boot.
put_menu_core() {
	ours="$ROOT/cores/menu_phosphor.rbf"; m="$1/menu.rbf"
	[ -s "$ours" ] || return 0
	cmp -s "$ours" "$m" 2>/dev/null && return 0
	mkdir -p "$ROOT/backup"
	[ -f "$m" ] && cp "$m" "$ROOT/backup/menu.rbf.mister"
	cp "$ours" "$m.new" && mv -f "$m.new" "$m" && echo "menu.rbf: Phosphor's menu core (MiSTer's kept in $ROOT/backup/menu.rbf.mister)"
}

# Phosphor's Main as the system's Main: MiSTer's Main hands over to whatever main= names, at once
if [ "$BOOT" = 2 ] && [ -f "$ROOT/bin/MiSTer_phosphor" ]; then
	put_menu_core "$FAT"
	INI="$FAT/MiSTer.ini"
	mkdir -p "$ROOT/backup"
	if [ -f "$INI" ]; then
		grep -q "^main=phosphor/bin/MiSTer_phosphor" "$INI" || cp "$INI" "$ROOT/backup/MiSTer.ini.$(date +%Y%m%d-%H%M%S)"
	else
		# no MiSTer.ini yet (a new card): start from MiSTer's example, so every other setting is MiSTer's
		if [ -f "$FAT/MiSTer_example.ini" ]; then cp "$FAT/MiSTer_example.ini" "$INI"; else printf '[MiSTer]\n' > "$INI"; fi
		: > "$ROOT/backup/MiSTer.ini.created"
	fi
	# the main= that was there (in [MiSTer]), for uninstall.sh
	grep -q "^main=phosphor/bin/MiSTer_phosphor" "$INI" || \
	awk 'BEGIN{s=0} /^[[:space:]]*\[/ {s = (tolower($0) ~ /^[[:space:]]*\[mister\]/)} s && /^[[:space:]]*main[[:space:]]*=/ && !/phosphor\/bin\/MiSTer_phosphor/ {sub(/^[^=]*=[[:space:]]*/,""); sub(/[[:space:]]*;.*$/,""); gsub(/\r/,""); print; exit}' "$INI" > "$ROOT/backup/main.prev"
	tmp="$(mktemp)"
	awk 'BEGIN{s=0; done=0}
		/^[[:space:]]*\[/ { s = (tolower($0) ~ /^[[:space:]]*\[mister\]/); print; if (s && !done) { print "main=phosphor/bin/MiSTer_phosphor"; done=1 }; next }
		s && /^[[:space:]]*main[[:space:]]*=/ { next }
		{ print }
		END { if (!done) { print "[MiSTer]"; print "main=phosphor/bin/MiSTer_phosphor" } }' "$INI" | tr -d '\r' | sed 's/$/\r/' > "$tmp"
	cat "$tmp" > "$INI" && rm -f "$tmp"
	echo "MiSTer.ini: main=phosphor/bin/MiSTer_phosphor (before: $(head -1 "$ROOT/backup/main.prev" 2>/dev/null | grep . || echo "MiSTer's own"))"
fi

echo "Phosphor $(cat "$ROOT/VERSION") installed in $ROOT"
echo "  start:  $ROOT/phosphor.sh start     stop: $ROOT/phosphor.sh stop"
echo "  remove: $ROOT/uninstall.sh"
[ -d "$HERE/modules" ] && echo "  modules (each optional): $HERE/modules/<name>/install.sh"
