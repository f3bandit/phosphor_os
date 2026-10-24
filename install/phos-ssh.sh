#!/bin/bash
# Phosphor: SSH for Settings > Remote access (Off, Password, Keys only), and the copy of the remote
# access setup kept on another drive (so a freshly flashed SD card gets it back).
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   phos-ssh.sh apply               put the saved setup into effect (phosphord runs it at start and after
#                                   each change): MiSTer's system files are read-only and a Linux update
#                                   replaces them, so nothing there is edited; copies in /run are mounted
#                                   over sshd_config, /etc/shadow and /root/.ssh instead
#   phos-ssh.sh setpw               a new root password, read from stdin (only its hash is kept)
#   phos-ssh.sh resetpw             back to MiSTer's own password
#   phos-ssh.sh stock               MiSTer's own SSH again (uninstalling)
#   phos-ssh.sh keep <dir>          copy the setup to <dir>/phosphor-keep/remote
#   phos-ssh.sh find                copies on the drives, newest first: dir|saved (unix time)|MiSTer name
#   phos-ssh.sh bring <dir>         put a copy back (a fresh SD card)
#
# The setup: phosphor/config/ssh/ (mode: off|password|keys, root-hash, authorized_keys) and
# phosphor/config/remote.ini (paired devices). Only local drives get a copy: never the internet.
# Restarting SSH stops only the listener, so open sessions (phosphor-link, a terminal) stay.
set -u
FAT="${PHOS_FAT:-/media/fat}"
ROOT="$FAT/phosphor"
CONF="$ROOT/config/ssh"
RUN=/run/phosphor-ssh
mkdir -p "$CONF"

mode() { local m; m="$(cat "$CONF/mode" 2>/dev/null)"; case "$m" in off|keys) echo "$m" ;; *) echo password ;; esac; }

unbind() { local i; for i in 1 2 3; do grep -q " $1 " /proc/mounts || break; umount "$1" 2>/dev/null || umount -l "$1" 2>/dev/null; done; return 0; }

listener_stop() {
	local p; p="$(cat /var/run/sshd.pid 2>/dev/null)"
	[ -n "$p" ] && kill "$p" 2>/dev/null
	for _ in 1 2 3 4 5 6 7 8 9 10; do [ -n "$p" ] && kill -0 "$p" 2>/dev/null || break; sleep 0.2; done
	rm -f /var/run/sshd.pid /var/lock/sshd
}
listener_start() { /usr/bin/ssh-keygen -A > /dev/null 2>&1; /usr/sbin/sshd && touch /var/lock/sshd; }

apply() {
	local m; m="$(mode)"
	# the keys-only setting needs a key: without one it'd lock everyone out, so passwords stay
	if [ "$m" = keys ] && ! grep -q '^ssh-\|^ecdsa-' "$CONF/authorized_keys" 2>/dev/null; then m=password; echo "keys only needs a key: passwords stay on" >&2; fi
	mkdir -p "$RUN" && chmod 700 "$RUN"
	# originals (what's under the mounts)
	unbind /etc/ssh/sshd_config; unbind /etc/shadow; unbind /root/.ssh
	if [ "$m" = off ]; then listener_stop; echo "mode=off"; return 0; fi
	# nothing of Phosphor's (MiSTer's password, no keys): MiSTer's own SSH as it is, running
	if [ "$m" = password ] && [ ! -s "$CONF/root-hash" ] && [ ! -s "$CONF/authorized_keys" ]; then
		{ [ -f /var/run/sshd.pid ] && kill -0 "$(cat /var/run/sshd.pid)" 2>/dev/null; } || listener_start
		echo "mode=password"; return 0
	fi
	# keys work in both modes
	rm -rf "$RUN/dotssh" && mkdir -p "$RUN/dotssh" && chmod 700 "$RUN/dotssh"
	cp -a /root/.ssh/. "$RUN/dotssh/" 2>/dev/null
	if [ -s "$CONF/authorized_keys" ]; then tr -d '\r' < "$CONF/authorized_keys" > "$RUN/dotssh/authorized_keys"; chmod 600 "$RUN/dotssh/authorized_keys"; fi
	chown -R root:root "$RUN/dotssh"
	mount --bind "$RUN/dotssh" /root/.ssh
	# a password of your own
	if [ -s "$CONF/root-hash" ]; then
		local h; h="$(head -1 "$CONF/root-hash" | tr -d '\r\n')"
		case "$h" in '$'[1-6]'$'*) awk -F: -v OFS=: -v h="$h" '$1=="root"{$2=h}1' /etc/shadow > "$RUN/shadow" && chmod 600 "$RUN/shadow" && mount --bind "$RUN/shadow" /etc/shadow ;; esac
	fi
	# keys only: no passwords (sshd takes the first value it reads, so these go first)
	if [ "$m" = keys ]; then
		{ printf 'PasswordAuthentication no\nChallengeResponseAuthentication no\nPermitRootLogin prohibit-password\n'; grep -v '^PermitRootLogin' /etc/ssh/sshd_config; } > "$RUN/sshd_config"
		if /usr/sbin/sshd -t -f "$RUN/sshd_config" 2>/dev/null; then mount --bind "$RUN/sshd_config" /etc/ssh/sshd_config
		else echo "sshd didn't accept keys-only: passwords stay on" >&2; m=password; fi
	fi
	listener_stop; listener_start
	echo "mode=$m"
}

stock() { unbind /etc/ssh/sshd_config; unbind /etc/shadow; unbind /root/.ssh; listener_stop; listener_start; }

keep() { # copy everything to <dir>/phosphor-keep/remote
	local d="$1/phosphor-keep/remote"
	[ -d "$1" ] || { echo "no such drive: $1" >&2; return 1; }
	rm -rf "$d.new" && mkdir -p "$d.new/ssh" || return 1
	cp -f "$ROOT/config/remote.ini" "$d.new/" 2>/dev/null
	cp -f "$CONF"/mode "$CONF"/root-hash "$CONF"/authorized_keys "$d.new/ssh/" 2>/dev/null
	printf 'saved=%s\nname=%s\n' "$(date +%s)" "$(hostname)" > "$d.new/stamp"
	rm -rf "$d" && mv "$d.new" "$d" && sync && echo "saved=$(date +%s)"
}

find_copies() {
	local f d
	for f in /media/usb[0-9]*/phosphor-keep/remote/stamp; do
		[ -f "$f" ] || continue
		d="${f%/phosphor-keep/remote/stamp}"
		echo "$d|$(sed -n 's/^saved=//p' "$f")|$(sed -n 's/^name=//p' "$f")"
	done | sort -t'|' -k2,2nr
}

bring() {
	local d="$1/phosphor-keep/remote"
	[ -f "$d/stamp" ] || { echo "no copy on $1" >&2; return 1; }
	mkdir -p "$ROOT/config" "$CONF"
	[ -f "$d/remote.ini" ] && cp -f "$d/remote.ini" "$ROOT/config/remote.ini"
	for f in mode root-hash authorized_keys; do [ -f "$d/ssh/$f" ] && cp -f "$d/ssh/$f" "$CONF/$f"; done
	sync
	echo "brought=$(sed -n 's/^saved=//p' "$d/stamp")"
}

case "${1:-}" in
	apply) apply ;;
	setpw)
		IFS= read -r pw || true
		[ -n "${pw:-}" ] || { echo "no password" >&2; exit 2; }
		h="$(printf '%s' "$pw" | openssl passwd -5 -stdin 2>/dev/null)"
		case "$h" in '$5$'*) printf '%s\n' "$h" > "$CONF/root-hash.new" && mv -f "$CONF/root-hash.new" "$CONF/root-hash" && sync ;; *) echo "couldn't make the password" >&2; exit 1 ;; esac
		apply ;;
	resetpw) rm -f "$CONF/root-hash"; sync; apply ;;
	stock) stock ;;
	keep) [ -n "${2:-}" ] || exit 2; keep "$2" ;;
	find) find_copies ;;
	bring) [ -n "${2:-}" ] || exit 2; bring "$2" ;;
	*) sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
