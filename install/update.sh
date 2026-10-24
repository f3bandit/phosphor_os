#!/bin/bash
# Phosphor updater: installs a Phosphor release package (phosphor-<version>-armhf.tar.gz) over the
# installed Phosphor, or rolls back to the version before. Run by Phosphor itself (Settings > Update
# Phosphor, on the TV or in the app) or from MiSTer's Scripts menu.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   update.sh list                    packages found, one per line: path|version
#                                     (phosphor/updates, the top of the SD card and of each USB drive)
#   update.sh install <package>       check it, back up the installed version, install, start Phosphor (which
#                                     then restarts the SuperStation to finish)
#   update.sh rollbacks               versions kept to go back to, one per line: version|date
#   update.sh rollback <version>      put that version back the same way
#   update.sh state                   the last run: state=idle|working|done|failed, version, restart, msg
#
# Kept as they are: the library, box art, settings, logs, controller mappings, everything outside
# phosphor/bin, phosphor/modules and phosphor/cores. The installed version is backed up first to
# phosphor/backup/versions/<version> (the newest 3 are kept). When MiSTer's Main changes, the new one
# runs from the next restart; the state says so (restart=1).
set -u
FAT="${PHOS_FAT:-/media/fat}"
ROOT="$FAT/phosphor"
TMPD="${PHOS_TMP:-/tmp/phosphor}"
STATE="$TMPD/update-state"
LOG="$ROOT/logs/update.log"
KEEP=3
mkdir -p "$TMPD" "$ROOT/logs" "$ROOT/updates"

state() { # state, message, version, restart
	printf 'state=%s\nversion=%s\nrestart=%s\nmsg=%s\nat=%s\n' "$1" "${3:-}" "${4:-0}" "$2" "$(date +%s)" > "$STATE.new" && mv -f "$STATE.new" "$STATE"
	echo "$(date '+%Y-%m-%d %H:%M:%S')  $1: $2" >> "$LOG"
	echo "$2"
}

installed_version() { cut -d' ' -f1 "$ROOT/VERSION" 2>/dev/null; }

pkg_version() { # the version inside a package, without unpacking it
	tar -xzOf "$1" --wildcards '*/VERSION' 2>/dev/null | head -1 | cut -d' ' -f1
}

list() {
	local f v
	for f in "$ROOT"/updates/phosphor-*-armhf.tar.gz "$FAT"/phosphor-*-armhf.tar.gz /media/usb[0-9]*/phosphor-*-armhf.tar.gz; do
		[ -f "$f" ] || continue
		v="$(pkg_version "$f")"
		[ -n "$v" ] && echo "$f|$v"
	done
}

rollbacks() {
	local d
	for d in $(ls -1dt "$ROOT"/backup/versions/*/ 2>/dev/null); do
		[ -f "$d/VERSION" ] && echo "$(basename "$d")|$(cat "$d/.saved" 2>/dev/null || date -r "$d" '+%Y-%m-%d %H:%M' 2>/dev/null)"
	done
}

# how Phosphor starts now, so the installer keeps it that way
boot_flag() {
	if grep -q "phosphor/bin/MiSTer_phosphor" "$FAT/MiSTer.ini" 2>/dev/null; then echo --boot-phosphor
	elif grep -q '^# phosphor: BEGIN' "$FAT/linux/user-startup.sh" 2>/dev/null; then echo --boot
	fi
}

backup_installed() {
	local v d
	v="$(installed_version)"
	[ -n "$v" ] || return 0
	d="$ROOT/backup/versions/$v"
	rm -rf "$d.new" && mkdir -p "$d.new" || return 1
	cp -a "$ROOT/bin" "$d.new/" 2>/dev/null
	[ -d "$ROOT/modules" ] && cp -a "$ROOT/modules" "$d.new/"
	[ -d "$ROOT/cores" ] && cp -a "$ROOT/cores" "$d.new/"
	for f in phosphor.sh uninstall.sh update.sh VERSION; do [ -f "$ROOT/$f" ] && cp -a "$ROOT/$f" "$d.new/"; done
	date '+%Y-%m-%d %H:%M' > "$d.new/.saved"                 # (the SD card's file times can't be trusted)
	rm -rf "$d" && mv "$d.new" "$d" || return 1
	# the newest $KEEP versions
	ls -1dt "$ROOT"/backup/versions/*/ 2>/dev/null | tail -n +$((KEEP + 1)) | while read -r old; do rm -rf "$old"; done
	sync
}

md5() { md5sum "$1" 2>/dev/null | cut -c1-32; }

# an install or roll back is running: its process (kept in $LOCK) is still there
LOCK="$TMPD/update.lock"
running() {
	local p
	p="$(cat "$LOCK" 2>/dev/null)"
	[ -n "$p" ] && kill -0 "$p" 2>/dev/null
}

install_pkg() {
	local pkg="$1" w="$TMPD/update-unpack" dir v old main_before modules m
	[ -f "$pkg" ] || { state failed "no such package: $pkg"; return 1; }
	state working "Checking the package..." "" 0
	rm -rf "$w" && mkdir -p "$w"
	tar -xzf "$pkg" -C "$w" 2>>"$LOG" || { state failed "The package is damaged (it doesn't unpack)"; return 1; }
	dir="$(ls -1d "$w"/phosphor-*/ 2>/dev/null | head -1)"
	[ -n "$dir" ] && [ -x "$dir/install.sh" ] && [ -f "$dir/VERSION" ] && [ -x "$dir/bin/phosphor" ] || { state failed "That isn't a Phosphor package"; return 1; }
	if [ -f "$dir/SHA256SUMS" ]; then
		(cd "$dir" && sha256sum -c SHA256SUMS > /dev/null 2>>"$LOG") || { state failed "The package is damaged (a file doesn't match its checksum)"; return 1; }
	fi
	v="$(cut -d' ' -f1 "$dir/VERSION")"
	old="$(installed_version)"
	state working "Backing up Phosphor $old..." "$v" 0
	backup_installed || { state failed "Couldn't back up the installed version: nothing was changed" "$v"; return 1; }
	main_before="$(md5 "$ROOT/bin/MiSTer_phosphor")"
	modules="$(ls -1 "$ROOT/modules" 2>/dev/null)"
	state working "Installing Phosphor $v..." "$v" 0
	# the installer stops Phosphor, copies, and keeps the library, art and settings
	PHOS_FAT="$FAT" bash "$dir/install.sh" $(boot_flag) >> "$LOG" 2>&1 || { state failed "The installer stopped: see phosphor/logs/update.log. Roll back to $old from Settings > Update Phosphor" "$v"; "$ROOT/phosphor.sh" start > /dev/null 2>&1; return 1; }
	# the modules that were installed get their new versions too (each installer keeps its settings)
	for m in $modules; do
		case "$m" in lib) continue ;; esac
		[ -x "$dir/modules/$m/install.sh" ] && PHOSPHOR_ROOT="$ROOT" "$dir/modules/$m/install.sh" >> "$LOG" 2>&1
	done
	cp -f "$dir/update.sh" "$ROOT/update.sh" 2>/dev/null && chmod 755 "$ROOT/update.sh"
	sync
	local restart=0
	[ "$(md5 "$ROOT/bin/MiSTer_phosphor")" != "$main_before" ] && restart=1
	"$ROOT/phosphor.sh" start >> "$LOG" 2>&1
	rm -rf "$w"
	if [ "$restart" = 1 ]; then state done "Updated to Phosphor $v" "$v" 1
	else state done "Updated to Phosphor $v" "$v" 0; fi
}

rollback() {
	local v="$1" d="$ROOT/backup/versions/$1" main_before
	case "$v" in */*|*..*|"") state failed "no such version"; return 1 ;; esac
	[ -f "$d/VERSION" ] || { state failed "Phosphor $v isn't kept"; return 1; }
	state working "Putting back Phosphor $v..." "$v" 0
	local now; now="$(installed_version)"
	[ "$now" != "$v" ] && backup_installed
	main_before="$(md5 "$ROOT/bin/MiSTer_phosphor")"
	"$ROOT/phosphor.sh" stop > /dev/null 2>&1
	for x in bin modules cores; do
		[ -d "$d/$x" ] || continue
		rm -rf "$ROOT/$x.old" && mv "$ROOT/$x" "$ROOT/$x.old" 2>/dev/null
		cp -a "$d/$x" "$ROOT/$x" && rm -rf "$ROOT/$x.old" || { rm -rf "$ROOT/$x"; mv "$ROOT/$x.old" "$ROOT/$x"; }
	done
	for f in phosphor.sh uninstall.sh update.sh VERSION; do [ -f "$d/$f" ] && cp -a "$d/$f" "$ROOT/$f"; done
	# a version without the updater keeps this one, so it can come forward again
	[ -f "$ROOT/update.sh" ] || cp -a "$0" "$ROOT/update.sh"
	sync
	"$ROOT/phosphor.sh" start >> "$LOG" 2>&1
	if [ "$(md5 "$ROOT/bin/MiSTer_phosphor")" != "$main_before" ]; then state done "Back to Phosphor $v" "$v" 1
	else state done "Back to Phosphor $v" "$v" 0; fi
}

case "${1:-}" in
	list) list ;;
	rollbacks) rollbacks ;;
	state) cat "$STATE" 2>/dev/null || echo "state=idle" ;;
	# install and rollback stop Phosphor on the way: run on their own, so they finish whoever asked
	install|rollback)
		[ -n "${2:-}" ] || { echo "$1 <package|version>" >&2; exit 2; }
		# one at a time: a second ask while one runs is turned down (exit 3), never run alongside
		if [ -z "${PHOS_UPDATE_CHILD:-}" ]; then
			if running; then echo "An update is already running"; exit 3; fi
		fi
		if [ -z "${PHOS_UPDATE_CHILD:-}" ] && [ "${3:-}" != --wait ]; then
			state working "Starting..." "" 0
			PHOS_UPDATE_CHILD=1 setsid "$0" "$1" "$2" < /dev/null >> "$LOG" 2>&1 &
			echo $! > "$LOCK"
			exit 0
		fi
		echo $$ > "$LOCK"
		trap 'rm -f "$LOCK"' EXIT
		# (a script from MiSTer's Scripts menu asks for a moment, so its window can close first)
		[ -n "${PHOS_UPDATE_DELAY:-}" ] && sleep "$PHOS_UPDATE_DELAY"
		if [ "$1" = install ]; then install_pkg "$2"; else rollback "$2"; fi ;;
	*) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
