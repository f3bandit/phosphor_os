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
#   phosphor.sh check          quick checks for testing (what runs, what answers): ok / -- / FAIL lines
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
		# a new card: the modules, once, from the Phosphor release kept next to this script
		if [ -f "$ROOT/firstboot" ]; then
			tb="$(ls -t "$ROOT"/phosphor-*-armhf.tar.gz 2>/dev/null | head -1)"
			w="$TMP/firstboot"
			rm -rf "$w"; mkdir -p "$w"
			if [ -n "$tb" ] && tar xzf "$tb" -C "$w" 2>> "$ROOT/logs/firstboot.log"; then
				for m in $(cat "$ROOT/firstboot"); do
					s="$(ls "$w"/*/modules/"$m"/install.sh 2>/dev/null | head -1)"
					[ -n "$s" ] || continue
					PHOSPHOR_ROOT="$ROOT" bash "$s" >> "$ROOT/logs/firstboot.log" 2>&1 && echo "module $m installed" >> "$ROOT/logs/firstboot.log"
				done
				rm -f "$ROOT/firstboot"
			fi
			rm -rf "$w"
		fi
		# Phosphor's Main is already the system's (main= in MiSTer.ini); without it, swap now
		start_service
		pidof MiSTer_phosphor > /dev/null || switch_main
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
	check)
		n_fail=0
		ok() { echo "ok    $*"; }
		no() { echo "--    $*"; }
		bad() { echo "FAIL  $*"; n_fail=$((n_fail + 1)); }
		q() { "$BIN/phos" "$1" 2> /dev/null; }
		has() { case "$1" in *"$2"*) return 0 ;; esac; return 1; }
		[ -f "$ROOT/VERSION" ] && ok "installed: $(cat "$ROOT/VERSION")" || bad "not installed in $ROOT"
		running phosphord && ok "phosphord running" || bad "phosphord not running"
		s="$(q status)"; has "$s" '"ok":true' && ok "phosphord answers ($(echo "$s" | sed -n 's/.*"version":"\([^"]*\)".*/\1/p'), $(echo "$s" | sed -n 's/.*"games":\([0-9]*\).*/\1/p') games)" || bad "phosphord doesn't answer"
		running phosphor && ok "shell running" || no "shell not running (phosphor.sh start)"
		if pidof MiSTer_phosphor > /dev/null; then ok "Phosphor's Main running"
		else m="$(ps -o comm= 2> /dev/null | grep '^MiSTer' | head -1)"; no "Phosphor's Main not running (${m:-no Main} is)"; fi
		p="$(q '{"cmd":"ports"}')"
		has "$p" '"main_api":true' && ok "Main's Phosphor API answers" || no "Main's Phosphor API not there"
		if has "$p" '"available":true'; then ok "front ports available (Phosphor menu core)"
		elif has "$p" '"in_game":true'; then no "front ports: a game is running"
		else no "front ports not available (stock menu core or no phosphor-main)"; fi
		[ -f "$ROOT/cores/menu_phosphor.rbf" ] && ok "Phosphor menu core installed" || no "Phosphor menu core not installed"
		b="$(q '{"cmd":"bluetooth"}')"
		if has "$b" '"present":true'; then ok "Bluetooth adapter ($(echo "$b" | grep -o '"mac"' | wc -l) paired)"; else no "no Bluetooth adapter"; fi
		w="$(q '{"cmd":"wifi"}')"; has "$w" '"connected":true' && ok "WiFi connected" || no "WiFi not connected"
		r="$(q '{"cmd":"remote"}')"; has "$r" '"listening":true' && ok "remote access listening" || no "remote access off"
		grep -q "^# phosphor: BEGIN" /media/fat/linux/user-startup.sh 2> /dev/null && ok "boot block: $(grep -A1 '^# phosphor: BEGIN' /media/fat/linux/user-startup.sh | grep -o 'phosphor.sh [a-z]*')" || no "no boot block (starts by hand)"
		for l in "$ROOT/logs/phosphord.log" "$ROOT/logs/phosphor.log"; do
			[ -f "$l" ] || continue
			e="$(tail -n 300 "$l" | grep -ciE 'error|fail|can.t|crash' )"
			[ "$e" = 0 ] && ok "${l##*/}: nothing alarming in the last 300 lines" || no "${l##*/}: $e lines mention errors (phosphor.sh log)"
		done
		df -h /media/fat 2> /dev/null | awk 'NR==2 {print "ok    free on the SD card: " $4}'
		[ "$n_fail" = 0 ] && echo "no failures" || { echo "$n_fail failure(s)"; exit 1; }
		;;
	log)
		tail -n 30 "$ROOT/logs/phosphord.log" "$ROOT/logs/phosphor.log" 2>/dev/null
		;;
	*)
		sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'
		exit 2
		;;
esac
