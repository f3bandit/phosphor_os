#!/bin/bash
# Phosphor: start, stop and check phosphord and the shell by hand.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
#   phosphor.sh start          phosphord + the shell on the framebuffer
#   phosphor.sh service        phosphord only (library, scraper, API)
#   phosphor.sh boot           at boot (install.sh --boot-phosphor): phosphord, Phosphor's Main in place
#                              of MiSTer's (if it's installed), then the shell
#   phosphor.sh main           switch to Phosphor's Main now (MiSTer's Main comes back on the next boot)
#   phosphor.sh safe-stop      before power off / restart: modules stopped, drives flushed and read-only
#                              (exFAT marked clean, so Windows doesn't ask to scan), each drive's state
#   phosphor.sh menu-core      put Phosphor's menu core back as menu.rbf (at shutdown, after updates)
#   phosphor.sh shutdown       Linux shutting down (user-startup.sh stop: a restart from anywhere, Update All's
#                              included): the menu core back, then safe-stop unless it ran already
#   phosphor.sh repair-hook    the repair drive at /media/repair, read-only, never a /media/usbN (the hook
#                              MiSTer's USB mounting runs, put back if a Linux update took it); one at a
#                              /media/usbN already moved there
#   phosphor.sh reload         restart the shell (Settings > Reload, the PC app); phosphord keeps running
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

# Phosphor's menu core as menu.rbf: the bootloader loads menu.rbf before Linux starts, and Phosphor's
# shows black instead of MiSTer's static until Phosphor is on screen. MiSTer's own is kept in
# phosphor/backup/menu.rbf.mister (uninstall.sh puts it back); an update that brings a new one back is
# kept there too, and Phosphor's goes in again at the next boot.
put_menu_core() {
	ours="$ROOT/cores/menu_phosphor.rbf"; m="$1/menu.rbf"
	[ -s "$ours" ] || return 0
	cmp -s "$ours" "$m" 2>/dev/null && return 0
	mkdir -p "$ROOT/backup"
	# only MiSTer's is kept: an older Phosphor core (an update brought a new one) never takes its place
	known="$ROOT/backup/menu.rbf.phosphor"   # the Phosphor cores put in before, one md5 a line
	if [ -f "$m" ]; then
		h="$(md5sum "$m" | cut -c1-32)"; phos=0
		grep -qxF "$h" "$known" 2>/dev/null && phos=1
		for c in "$ROOT"/backup/versions/*/cores/menu_phosphor.rbf; do cmp -s "$c" "$m" && phos=1; done
		[ "$phos" = 1 ] || cp "$m" "$ROOT/backup/menu.rbf.mister"
	fi
	cp "$ours" "$m.new" && mv -f "$m.new" "$m" || return 1
	h="$(md5sum "$m" | cut -c1-32)"; grep -qxF "$h" "$known" 2>/dev/null || echo "$h" >> "$known"
	echo "menu.rbf: Phosphor's menu core (MiSTer's kept in $ROOT/backup/menu.rbf.mister)"
}

# exFAT's "dirty" flag on a USB / NVMe drive, cleared with a disk check at boot. Linux never clears it
# itself on a drive that was dirty when it was mounted (fs/exfat keeps it as a "persistent" flag), so one
# restart without a clean stop leaves the drive dirty on every boot after, until a disk check runs. Only
# a drive nothing has open yet and only mounted once (no bind mounts) is checked: unmounted, fsck.exfat
# -p (repairs only what's safe to repair without asking), mounted again with its own options. The SD
# card can't be checked here (Linux runs from linux.img on it): Windows' scan does that one.
# One line a drive in logs/drives.log.
exfat_flags() { dd if="$1" bs=1 skip=106 count=1 2> /dev/null | od -An -tu1 | tr -d ' '; }
check_dirty_drives() {
	log="$ROOT/logs/drives.log"
	grep -E '^/dev/[^ ]+ /media/usb[0-9]+ exfat ' /proc/mounts | while read -r dev mnt _ opts _; do
		[ -b "$dev" ] && [ "$(dd if="$dev" bs=1 skip=3 count=5 2> /dev/null)" = EXFAT ] || continue
		f="$(exfat_flags "$dev")"
		[ $(( ${f:-0} & 2 )) -ne 0 ] || continue
		now="$(date '+%Y-%m-%d %H:%M:%S')"
		# running from this drive (nvmeboot): it's the system's own file system, mounted here a second time
		if [ "$(stat -c %d "$mnt" 2> /dev/null)" = "$(stat -c %d /media/fat 2> /dev/null)" ]; then echo "$now  $dev ($mnt)  the system's own drive (running from it): not checked" >> "$log"; continue; fi
		if [ "$(grep -c "^$dev " /proc/mounts)" != 1 ]; then echo "$now  $dev ($mnt)  dirty, not checked: mounted more than once" >> "$log"; continue; fi
		sync
		if ! umount "$mnt" 2> /dev/null; then echo "$now  $dev ($mnt)  dirty, not checked: in use" >> "$log"; continue; fi
		out="$(timeout 300 fsck.exfat -p "$dev" < /dev/null 2>&1 | tail -1)"
		mkdir -p "$mnt"
		mounted=""
		for try in 1 2 3; do
			if mount -t exfat -o "$opts" "$dev" "$mnt" 2>> "$log" || mount "$dev" "$mnt" 2>> "$log"; then mounted=1; break; fi
			sleep 2
		done
		[ -n "$mounted" ] || echo "$now  $dev ($mnt)  COULDN'T MOUNT IT AGAIN (3 tries): restart the SuperStation" >> "$log"
		f="$(exfat_flags "$dev")"; st=clean; [ $(( ${f:-0} & 2 )) -ne 0 ] && st="still dirty"
		echo "$now  $dev ($mnt)  was dirty, checked: $st ($out)" >> "$log"
	done
}

# Before a restart or switching off: every USB device left idle, so none comes back stuck. A restart
# keeps the dock powered, and its NVMe has come up "Unit Not Ready" (hardware error) after one.
#  - USB disks: manage_shutdown on, so the kernel sends "sync your cache, stop" (START STOP UNIT) when
#    the disk goes, here or at the restart itself
#  - each USB device then let go by its driver (authorized 0): the drives first, the WiFi card last;
#    hubs and USB serial ports (the dock's debug port: Phosphor Link) are left alone
# Prints nothing (safe-stop's output is read by phosphord). PHOS_SYS: a test /sys.
usb_quiesce() {
	S="${PHOS_SYS:-/sys}"
	for f in "$S"/class/scsi_disk/*/manage_shutdown "$S"/class/scsi_disk/*/manage_system_start_stop; do
		[ -w "$f" ] && echo 1 > "$f" 2> /dev/null
	done
	sync
	stor="" wifi="" rest=""
	for d in "$S"/bus/usb/devices/*-*; do
		n="${d##*/}"
		case "$n" in *:*) continue ;; esac
		[ -w "$d/authorized" ] || continue
		[ "$(cat "$d/bDeviceClass" 2> /dev/null)" = 09 ] && continue          # a hub
		drv=""
		for i in "$d/$n":*; do [ -e "$i/driver" ] && drv="$drv $(basename "$(readlink "$i/driver")")"; done
		case "$drv" in *ch341*|*ftdi*|*cp210x*|*pl2303*|*cdc_acm*) continue ;; esac   # a serial port
		prod="$(cat "$d/product" 2> /dev/null)"
		case "$drv $prod" in
			*usb-storage*|*uas*) stor="$stor $d" ;;
			*802.11*|*WLAN*|*Wireless*|*wlan*|*rtw*|*rtl88*|*mt76*) wifi="$wifi $d" ;;
			*) rest="$rest $d" ;;
		esac
	done
	for d in $stor $rest $wifi; do echo 0 > "$d/authorized" 2> /dev/null; done
}

# ---------- the repair drive: /media/repair, read-only, never a /media/usbN ----------
# MiSTer mounts every USB drive at the next free /media/usbN, and anything looking for games or room
# (MiSTer's Main, Update All's downloader with prefer_external, other tools) looks there: the repair stick
# got the downloader's games folders once. So the drive with a repair copy of Linux on it (Phosphor's
# phosphor/rescue/os.img or SS1 Tool's ss1tool/rescue/os.img; never the dock's NVMe drive) is moved to
# /media/repair as soon as MiSTer mounts it, before the next drive is mounted (the NVMe drive keeps
# usb0), and made read-only. The repair tools make it writable only while they write their own files.
# The hook is on Linux's own file system (/etc/usbmount/mount.d): a MiSTer Linux update takes it away,
# so it's checked at every boot.
REPAIR_MNT=/media/repair
HOOK_DIR="${PHOS_USBMOUNT:-/etc/usbmount}"
HOOK="$HOOK_DIR/mount.d/00-phosphor-repair"
HOOK_RM="$HOOK_DIR/phosphor-repair-remove"
HOOK_RULE="${PHOS_UDEV_RULES:-/etc/udev/rules.d}/99-phosphor-repair.rules"
hook_body() {
	cat << 'EOF'
#!/bin/sh
# Phosphor (phosphor.sh repair-hook; uninstall.sh removes it): a repair drive goes to /media/repair,
# read-only, never a /media/usbN, so nothing that looks for games or room on /media/usb* uses it.
# MiSTer's usbmount runs this after mounting a drive (UM_MOUNTPOINT, UM_DEVICE).
m="$UM_MOUNTPOINT"; d="$UM_DEVICE"; R=/media/repair
[ -n "$m" ] && [ -n "$d" ] && [ "$m" != "$R" ] || exit 0
[ -f "$m/phosphor/rescue/os.img" ] || [ -f "$m/ss1tool/rescue/os.img" ] || exit 0
# never the dock's NVMe drive (an older Phosphor put its repair copy there)
n="$(basename "$(readlink -f "/sys/class/block/${d##*/}/.." 2> /dev/null)")"
grep -qi jmicron "/sys/block/$n/device/vendor" 2> /dev/null && exit 0
# Phosphor's copy names the drive it was made on: one that came along on another drive isn't a repair drive
if [ -f "$m/phosphor/rescue/os.id" ] && [ ! -f "$m/ss1tool/rescue/os.img" ]; then
	w="$(sed -n 's/^drive=//p' "$m/phosphor/rescue/os.id" | head -1)"
	[ -n "$w" ] && [ "$w" != "$(blkid -s PARTUUID -o value "$d" 2> /dev/null)" ] && exit 0
fi
if grep -q " $R " /proc/mounts; then
	# MiSTer mounts a drive twice now and then: the second one goes
	awk -v d="$d" -v r="$R" '$1 == d && $2 == r {f=1} END {exit !f}' /proc/mounts && umount "$m" 2> /dev/null
	exit 0
fi
mkdir -p "$R" && mount --move "$m" "$R" && mount -o remount,ro "$R" || exit 0
# MiSTer's next hook names the drive in /var/run/usbmount (Vendor_Model_N -> its mount): named here first,
# pointing at /media/repair, so that one keeps it (it skips a name that's there) instead of the old usbN
v="$(echo "$UM_VENDOR" | sed 's/ /_/g; s/[^0-9A-Za-z._-]//g')"; o="$(echo "$UM_MODEL" | sed 's/ /_/g; s/[^0-9A-Za-z._-]//g')"
n="${v}${v:+${o:+_}}$o"; p="$(echo "$d" | sed 's/^.*[^0-9]\([0-9]*\)/\1/')"
[ -n "$n" ] && mkdir -p /var/run/usbmount && ln -sfn "$R" "/var/run/usbmount/$n${p:+_$p}"
echo "$(date '+%Y-%m-%d %H:%M:%S')  the repair drive ($d) at $R, read-only (not $m)" >> /media/fat/phosphor/logs/drives.log
exit 0
EOF
}
hook_rm_body() {
	cat << 'EOF'
#!/bin/sh
# Phosphor: the repair drive unplugged: /media/repair let go (MiSTer's usbmount only unmounts /media/usbN)
awk -v d="$DEVNAME" '$1 == d && $2 == "/media/repair" {f=1} END {exit !f}' /proc/mounts || exit 0
umount -l /media/repair
for l in /var/run/usbmount/*; do [ "$(readlink "$l")" = /media/repair ] && rm -f "$l"; done
exit 0
EOF
}
repair_hook() {
	[ -d "$HOOK_DIR/mount.d" ] || return 0               # (not MiSTer's Linux)
	local ro=0 want_rule changed=0
	want_rule="KERNEL==\"sd*\", ACTION==\"remove\", RUN+=\"$HOOK_RM\""
	if [ "$(hook_body)" != "$(cat "$HOOK" 2> /dev/null)" ] || [ "$(hook_rm_body)" != "$(cat "$HOOK_RM" 2> /dev/null)" ] ||
		[ "$want_rule" != "$(cat "$HOOK_RULE" 2> /dev/null)" ] || [ ! -d "$REPAIR_MNT" ]; then
		[ -z "${PHOS_USBMOUNT:-}" ] && grep -q " / [^ ]* ro[, ]" /proc/mounts && ro=1 && mount -o remount,rw / 2> /dev/null
		hook_body > "$HOOK" && chmod 755 "$HOOK"
		hook_rm_body > "$HOOK_RM" && chmod 755 "$HOOK_RM"
		echo "$want_rule" > "$HOOK_RULE" && chmod 644 "$HOOK_RULE"
		mkdir -p "${PHOS_REPAIR_MNT:-$REPAIR_MNT}"
		sync; changed=1
		[ "$ro" = 1 ] && mount -o remount,ro / 2> /dev/null
		[ -z "${PHOS_USBMOUNT:-}" ] && udevadm control --reload > /dev/null 2>&1
		echo "$(date '+%Y-%m-%d %H:%M:%S')  the repair drive's hook put in (/media/repair)" >> "$ROOT/logs/drives.log"
	fi
	# a repair drive mounted before the hook was there: moved now (the NVMe drive keeps the number it got)
	local dev mnt
	while read -r dev mnt _; do
		case "$mnt" in /media/usb[0-9]*) UM_MOUNTPOINT="$mnt" UM_DEVICE="$dev" sh "$HOOK" ;; esac
	done < <(grep ' /media/usb[0-9]' /proc/mounts)
	return 0
}

case "${1:-status}" in
	boot)
		# a line a boot (with the uptime), to tell restarts apart from Main's (logs/main-reboots.log)
		echo "$(date '+%Y-%m-%d %H:%M:%S')  boot  up $(cut -d' ' -f1 /proc/uptime 2>/dev/null)s" >> "$ROOT/logs/boots.log"
		# running from the NVMe / a USB drive (nvmeboot/): the restart word, the drive's second mount gone
		[ -f "$ROOT/nvmeboot/nvmeboot.sh" ] && sh "$ROOT/nvmeboot/nvmeboot.sh" boot
		# the repair drive off /media/usbN (the hook back after a MiSTer Linux update, a drive already moved)
		repair_hook
		# before anything of Phosphor's opens files on them
		command -v fsck.exfat > /dev/null && check_dirty_drives
		# restarted from the offline SD card repair (storage module): its result, left on the repair drive
		# (in the background: just after a repair the dock's NVMe can need waking first)
		[ -x "$ROOT/modules/storage/storage.sh" ] && { "$ROOT/modules/storage/storage.sh" sdrescue result boot < /dev/null > /dev/null 2>&1 & }
		# started from the SD card to check the drive it runs from: back to the drive (a restart) once it's clean
		[ -f "$ROOT/nvmeboot/nvmeboot.sh" ] && sh "$ROOT/nvmeboot/nvmeboot.sh" after-check
		# MiSTer's downloader may have put its own menu.rbf back: Phosphor's again (for the next boot)
		[ -x "$BIN/MiSTer_phosphor" ] && put_menu_core "$(dirname "$ROOT")" > /dev/null
		# running from the drive: a new kernel or menu core (an update, the line above) to the SD card,
		# which the SuperStation starts from
		[ -f "$ROOT/nvmeboot/nvmeboot.sh" ] && sh "$ROOT/nvmeboot/nvmeboot.sh" sync > /dev/null
		# Phosphor first, so it's on screen within seconds of MiSTer starting
		start_service
		pidof MiSTer_phosphor > /dev/null || switch_main      # main= in MiSTer.ini normally does this
		"$0" start
		# a new card: the modules, once, in the background from the Phosphor release kept next to this
		# script, so a first boot isn't held up by them
		if [ -f "$ROOT/firstboot" ]; then
			nice -n 10 "$0" firstboot < /dev/null > /dev/null 2>&1 &
		fi
		;;
	firstboot)
		[ -f "$ROOT/firstboot" ] || exit 0
		mkdir "$TMP/firstboot.lock" 2>/dev/null || exit 0
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
		rm -rf "$w"; rmdir "$TMP/firstboot.lock"
		;;
	safe-stop)
		# before switching off or restarting (the power window, Settings, the PC app): stop Phosphor's
		# modules the way a shutdown does (their stop steps, the clean-stop mark, the menu core back), flush
		# everything, then make every drive read-only, which is when exFAT clears its "dirty" flag (the one
		# Windows asks to scan for). A plain remount is refused while anything has a file open for writing
		# (Linux itself runs from linux.img on the card), so then the kernel's emergency remount does it.
		# Prints one line per drive, path|clean, path|DIRTY, path|n/a; then "done". Nothing is written after.
		# (PHOS_SAFE_STOP: the stop step's own "shutdown" below doesn't run this again)
		PHOS_SAFE_STOP=1 /media/fat/linux/user-startup.sh stop > /dev/null 2>&1
		sync
		mounts=""
		for m in /media/usb[0-9]* $REPAIR_MNT /media/fat/cifs /media/fat; do mountpoint -q "$m" 2>/dev/null && mounts="$mounts $m"; done
		for m in $mounts; do mount -o remount,ro "$m" 2> /dev/null; done
		sync
		if grep -E ' /media/(fat|usb[0-9]+) ' /proc/mounts | grep -q ' rw,'; then
			echo 1 > /proc/sys/kernel/sysrq 2> /dev/null
			echo s > /proc/sysrq-trigger 2> /dev/null; sleep 1
			echo u > /proc/sysrq-trigger 2> /dev/null; sleep 2
		fi
		echo 3 > /proc/sys/vm/drop_caches 2> /dev/null     # the flags as they are on the card, not cached
		for m in $mounts; do
			[ "$m" = /media/fat/cifs ] && continue
			dev="$(awk -v m="$m" '$2 == m {print $1; exit}' /proc/mounts)"
			if [ "$dev" = /dev/root ]; then         # the SD card: Linux names it /dev/root; its real device
				d="$(stat -c %d "$m" 2> /dev/null)"; dev=""
				if [ -n "$d" ]; then
					n="$(basename "$(readlink -f "/sys/dev/block/$(( (d >> 8) & 0xfff )):$(( (d & 0xff) | ((d >> 12) & 0xfff00) ))")")"
					[ -b "/dev/$n" ] && dev="/dev/$n"
				fi
			fi
			st="n/a"
			if [ -b "$dev" ] && [ "$(dd if="$dev" bs=1 skip=3 count=5 2> /dev/null)" = EXFAT ]; then
				f="$(dd if="$dev" bs=1 skip=106 count=1 2> /dev/null | od -An -tu1 | tr -d ' ')"
				st=clean; [ $(( ${f:-0} & 2 )) -ne 0 ] && st=DIRTY; [ $(( ${f:-0} & 4 )) -ne 0 ] && st="MEDIA FAILURE"
			fi
			ro=rw; grep -q " $m [^ ]* ro[, ]" /proc/mounts && ro=ro
			echo "$m|$st|$ro"
		done
		usb_quiesce
		: > "$TMP/safe-stopped"
		echo done
		;;
	shutdown)
		# Linux shutting down (S99user runs user-startup.sh stop): whatever started it (Phosphor's power
		# window already did safe-stop; a restart from MiSTer's menu, Update All, the downloader or a
		# terminal didn't), the drives end read-only and the USB devices let go, so the SD card isn't left
		# "not shut down safely" and the dock's NVMe drive starts clean
		"$0" menu-core
		[ -n "${PHOS_SAFE_STOP:-}" ] || [ -f "$TMP/safe-stopped" ] || "$0" safe-stop > /dev/null 2>&1
		;;
	repair-hook)
		repair_hook
		grep " $REPAIR_MNT " /proc/mounts || echo "no repair drive at $REPAIR_MNT"
		;;
	menu-core)
		# Phosphor's menu core as menu.rbf again (MiSTer's downloader puts its own back on updates);
		# at shutdown and after Update / Update All. MiSTer's is kept in backup/menu.rbf.mister.
		[ -x "$BIN/MiSTer_phosphor" ] && put_menu_core "$(dirname "$ROOT")" && sync
		[ -f "$ROOT/nvmeboot/nvmeboot.sh" ] && sh "$ROOT/nvmeboot/nvmeboot.sh" sync > /dev/null
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
	reload)
		for p in $(pids phosphor); do kill "$p" 2>/dev/null; done
		for _ in 1 2 3 4 5 6 7 8 9 10; do running phosphor || break; sleep 0.3; done
		for p in $(pids phosphor); do kill -9 "$p" 2>/dev/null; done
		echo "$(date '+%Y-%m-%d %H:%M:%S')  reload ui" >> "$ROOT/logs/boots.log"
		"$0" start
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
