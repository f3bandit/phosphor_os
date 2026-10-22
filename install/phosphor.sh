#!/bin/bash
# Phosphor: start, stop and check phosphord and the shell by hand.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   phosphor.sh start          phosphord + the shell on the framebuffer
#   phosphor.sh service        phosphord only (library, scraper, API)
#   phosphor.sh boot           at boot (install.sh --boot-phosphor): phosphord, Phosphor's Main in place
#                              of MiSTer's (if it's installed), then the shell
#   phosphor.sh main           switch to Phosphor's Main now (MiSTer's Main comes back on the next boot)
#   phosphor.sh stop           stop both
#   phosphor.sh status         what's running, versions, the library
#   phosphor.sh log            last lines of both logs
#
# The shell draws on the Linux framebuffer and asks MiSTer Main for a 1280x720 mode. With Phosphor's
# Main (bin/MiSTer_phosphor) it takes the screen by itself; with MiSTer's own Main press F9 on a
# keyboard (fb_terminal=1 in MiSTer.ini) if the shell isn't visible.
ROOT="${PHOSPHOR_ROOT:-$(cd "$(dirname "$(readlink -f "$0")")" && pwd)}"   # the folder this script is in
BIN="$ROOT/bin"
TMP="${PHOS_TMP:-/tmp/phosphor}"
mkdir -p "$TMP" "$ROOT/logs"

pids() { for p in $(ps -eo pid,args 2>/dev/null | awk -v b="$BIN/$1" '$2 == b {print $1}'); do echo "$p"; done; }
running() { [ -n "$(pids "$1")" ]; }

start_service() {
	running phosphord && return 0
	PHOSD_BIN="$BIN/phosphord" setsid "$BIN/phosphord" >> "$ROOT/logs/phosphord.log" 2>&1 < /dev/null &
	for _ in 1 2 3 4 5 6 7 8 9 10; do [ -S "$TMP/phosphord.sock" ] && return 0; sleep 0.3; done
	echo "phosphord didn't start: see $ROOT/logs/phosphord.log" >&2
	return 1
}

# MiSTer's Main (/media/fat/MiSTer, started by inittab) is swapped for Phosphor's: same program plus
# Phosphor's API. Nothing on the card is replaced; the next boot starts MiSTer's Main again unless the
# boot block runs this again.
switch_main() {
	[ -x "$BIN/MiSTer_phosphor" ] || return 1
	pidof MiSTer_phosphor > /dev/null && return 0
	# another MiSTer Main (Console Mode's, for one) runs this system: never two at once
	other="$(ps -o comm= 2> /dev/null | grep '^MiSTer_' | grep -v '^MiSTer_phosphor$' | head -1)"
	if [ -n "$other" ]; then echo "$other is running: left as it is" >&2; return 1; fi
	for _ in $(seq 1 50); do pidof MiSTer > /dev/null && break; sleep 0.2; done   # boot: wait for it
	sleep 1
	killall MiSTer 2> /dev/null
	for _ in $(seq 1 25); do pidof MiSTer > /dev/null || break; sleep 0.2; done
	cd /media/fat && setsid "$BIN/MiSTer_phosphor" >> "$TMP/main.log" 2>&1 < /dev/null &
	for _ in 1 2 3 4 5 6 7 8 9 10; do [ -S /tmp/phosphor-main.sock ] && return 0; sleep 0.5; done
	echo "Phosphor's Main didn't start: see $TMP/main.log" >&2
	return 1
}

case "${1:-status}" in
	boot)
		start_service
		switch_main
		"$0" start
		;;
	main)
		switch_main && echo "Phosphor's Main is running (MiSTer's comes back on the next boot)"
		;;
	start)
		start_service || exit 1
		if ! running phosphor; then
			PHOSD_BIN="$BIN/phosphord" setsid "$BIN/phosphor" ${PHOSPHOR_ARGS:-} >> "$ROOT/logs/phosphor.log" 2>&1 < /dev/null &
		fi
		sleep 1
		running phosphor && echo "Phosphor is running (press F9 on a keyboard if it isn't on screen)" || { echo "the shell didn't start: see $ROOT/logs/phosphor.log" >&2; exit 1; }
		;;
	service)
		start_service && echo "phosphord is running"
		;;
	stop)
		for p in $(pids phosphor) $(pids phosphord); do kill "$p" 2>/dev/null; done
		sleep 0.5
		for p in $(pids phosphor) $(pids phosphord); do kill -9 "$p" 2>/dev/null; done
		echo "stopped"
		;;
	status)
		echo "shell:     $(running phosphor && echo running || echo stopped)"
		echo "phosphord: $(running phosphord && echo running || echo stopped)"
		[ -x "$BIN/phos" ] && running phosphord && "$BIN/phos" status && echo && "$BIN/phos" '{"cmd":"scrape"}' && echo
		[ -f "$ROOT/VERSION" ] && echo "installed: $(cat "$ROOT/VERSION")"
		;;
	log)
		tail -n 30 "$ROOT/logs/phosphord.log" "$ROOT/logs/phosphor.log" 2>/dev/null
		;;
	*)
		sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'
		exit 2
		;;
esac
