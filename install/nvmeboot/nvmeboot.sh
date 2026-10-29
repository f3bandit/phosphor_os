#!/bin/sh
# Phosphor: run from the NVMe (or a USB drive) instead of the SD card.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
#
# The SuperStation can only start from the SD card: its bootloader (MiSTer's u-boot), the Linux kernel
# (linux/zImage_dtb) and the menu core (menu.rbf) stay there. Everything else runs from the drive:
# linux.img, MiSTer's Main, the cores, saves, settings, Phosphor. No custom u-boot or kernel is needed:
#   - MiSTer's u-boot reads /linux/u-boot.txt from the SD card before it starts Linux (its "scrtest"),
#     so Phosphor's lines there (u-boot.phosphor.txt) replace its "mmcboot" with one that starts Linux
#     with root=PARTUUID=<the drive's partition> instead of the SD card.
#   - MiSTer's kernel mounts that partition (exFAT) as /media/fat and linux/linux.img on it as / (its
#     loop= patch), the same as it does with the SD card. rootwait=20: if the drive isn't there after 20 s
#     the kernel stops and restarts (panic=3).
#   - A word in the memory MiSTer keeps across a restart (0x1FFFFF10, the page Main and u-boot share)
#     says it was tried: u-boot sets "trying" before starting from the drive, Phosphor clears it once it's
#     up. Still "trying" at the next start-up = it didn't come up: u-boot starts from the SD card and
#     leaves "fell back" for Phosphor to tell the user. /phosphor_boot_sd on the SD card = start from it.
#   - The drive can't be checked while it's the system's own (/ and /media/fat are on it), so a drive
#     Linux found marked dirty at start-up (not shut down safely) gets "check": u-boot starts once from the
#     SD card, where the drive is only storage and Phosphor's dirty-drive check repairs it (fsck.exfat -p),
#     then it restarts and runs from the drive again.
# The boot drive is found by its partition's PARTUUID, never by its Linux name (sda / sdb change with
# what's plugged in). Two partitions with the same PARTUUID (a cloned drive) are refused.
#
#   nvmeboot.sh status            key=value lines (for the shell and the apps)
#   nvmeboot.sh check [drive]     can it run from that drive (default /media/usb0): ok / -- / FAIL lines
#   nvmeboot.sh copy [drive]      the SD card copied to the drive (games added, nothing on it replaced)
#   nvmeboot.sh enable [drive]    the boot ID and the start-up lines on the SD card; downloader.ini
#                                 storage_priority to 'prefer_sd' on the drive (the old value kept)
#   nvmeboot.sh disable           the start-up lines off the SD card (it starts from it again)
#   nvmeboot.sh copyback          running from the drive: what changed there copied back to the SD card
#   nvmeboot.sh boot              at boot, from phosphor.sh boot: the restart word, the second mount; the
#                                 drive marked dirty at start-up: once from the SD card to check it
#   nvmeboot.sh after-check       at boot, after the dirty-drive check: back to the drive once it's clean
#   nvmeboot.sh sync              running from the drive: the kernel, menu.rbf and u-boot.txt's own lines
#                                 copied to the SD card when they changed (after updates)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
FAT="${NVB_FAT:-/media/fat}"
SDMNT="${NVB_SDMNT:-/media/sd}"                         # the SD card, while running from the drive
SDDEV="${NVB_SDDEV:-/dev/mmcblk0p1}"
CMDLINE="${NVB_CMDLINE:-/proc/cmdline}"
DEVMEM="${NVB_DEVMEM:-devmem}"
MARK="${NVB_MARK:-0x1FFFFF10}"
TMPD="${PHOS_TMP:-/tmp/phosphor}"
BLOCK="$HERE/u-boot.phosphor.txt"
M_TRY=0x504E5452 M_FELL=0x504E4642 M_FORCED=0x504E4653  # "RTNP", "BFNP", "SFNP" in memory
M_CHECK=0x504E4643                                      # "CFNP": start from the SD card once, to check the drive

mkdir -p "$TMPD" 2> /dev/null
log() { echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$FAT/phosphor/logs/nvmeboot.log" 2> /dev/null; }

boot_word() { sed -n 's/.*phosphor\.boot=\([a-z]*\).*/\1/p' "$CMDLINE" 2> /dev/null; }   # nvme | fallback | forced | ""
from_drive() { [ "$(boot_word)" = nvme ]; }

# the SD card's root: itself when running from it, mounted at $SDMNT when running from the drive
sd_root() {
	if ! from_drive; then echo "$FAT"; return 0; fi
	if ! mountpoint -q "$SDMNT" 2> /dev/null; then
		[ -n "${NVB_NOMOUNT:-}" ] && { echo "$SDMNT"; return 0; }
		mkdir -p "$SDMNT" && mount -t exfat -o noatime "$SDDEV" "$SDMNT" || return 1
		touch "$TMPD/nvmeboot.sdmounted"
	fi
	echo "$SDMNT"
}
sd_done() {     # unmounted again when this mounted it, so it's clean (never left marked dirty)
	[ -f "$TMPD/nvmeboot.sdmounted" ] || return 0
	sync; umount "$SDMNT" 2> /dev/null && rm -f "$TMPD/nvmeboot.sdmounted"
}

dev_of() { awk -v m="$1" '$2 == m {print $1; exit}' "${NVB_MOUNTS:-/proc/mounts}"; }
partuuid_of() { blkid -s PARTUUID -o value "$1" 2> /dev/null; }
fstype_of() { blkid -s TYPE -o value "$1" 2> /dev/null; }
# how many partitions carry that PARTUUID (a cloned drive: 2)
partuuid_count() { blkid 2> /dev/null | grep -ci "PARTUUID=\"$1\""; }

kb_used() { df -kP "$1" 2> /dev/null | awk 'NR == 2 {print $3}'; }
kb_free() { df -kP "$1" 2> /dev/null | awk 'NR == 2 {print $4}'; }

# the start-up lines in an u-boot.txt: ours are phos_* and our mmcboot
has_block() { grep -q '^phos_partuuid=' "$1" 2> /dev/null; }
strip_block() { grep -v -e '^phos_' -e '^mmcboot=if run phos_force' "$1" 2> /dev/null; }

# the drive Linux runs from was marked dirty when it was mounted (this boot's kernel log)
root_was_dirty() {
	d="$(stat -c %d "$FAT" 2> /dev/null)"; [ -n "$d" ] || return 1
	n="$(basename "$(readlink -f "/sys/dev/block/$(( (d >> 8) & 0xfff )):$(( (d & 0xff) | ((d >> 12) & 0xfff00) ))" 2> /dev/null)")"
	[ -n "${NVB_ROOTDEV:-}" ] && n="$NVB_ROOTDEV"
	[ -n "$n" ] && ${NVB_DMESG:-dmesg} 2> /dev/null | grep -q "exFAT-fs ($n): Volume was not properly unmounted"
}

# started from the SD card to check the drive: back to it once Phosphor's dirty-drive check (run just
# before this) has cleaned it
cmd_after_check() {
	[ "$(boot_word)" = check ] || return 0
	p="$(sed -n 's/^phos_partuuid=//p' "$FAT/linux/u-boot.txt" 2> /dev/null | head -1)"
	dev="$(blkid 2> /dev/null | grep -i "PARTUUID=\"$p\"" | head -1 | cut -d: -f1)"
	if [ -z "$dev" ]; then log "check: the drive isn't here"; echo check-failed > "$TMPD/boot-from"; return 0; fi
	mnt="$(awk -v d="$dev" '$1 == d {print $2; exit}' "${NVB_MOUNTS:-/proc/mounts}")"
	# the dirty-drive check's own line for it, this boot (it's mounted again after the check, and Linux
	# marks a drive in use while it's mounted, so its flag now means nothing)
	l="$(grep "  $dev " "$FAT/phosphor/logs/drives.log" 2> /dev/null | tail -1)"
	up="$(cut -d. -f1 /proc/uptime 2> /dev/null)"; t="$(date -d "${l%%  *}" +%s 2> /dev/null)"
	[ -n "$t" ] && [ -n "$up" ] && [ "$t" -lt $(( $(date +%s) - up - 5 )) ] && l=""      # an older boot's line
	case "$l" in
		*"still dirty"*|*"COULDN'T MOUNT"*|*"not checked"*)
			log "check: the drive wasn't cleaned (${l#*  }): staying on the SD card"
			echo check-failed > "$TMPD/boot-from"; return 0 ;;
	esac
	log "check: the drive ($dev${mnt:+ at $mnt}) is clean: restarting from it"
	$DEVMEM "$MARK" 32 0 2> /dev/null
	[ -n "${NVB_NOREBOOT:-}" ] && { echo back-to-drive > "$TMPD/boot-from"; return 0; }
	sh "$FAT/phosphor/phosphor.sh" safe-stop > /dev/null 2>&1; sync; reboot
	sleep 30; return 0
}

cmd_status() {
	w="$(boot_word)"
	echo "running_from=$([ "$w" = nvme ] && echo drive || echo sd)"
	echo "why=${w:-normal}"
	r="$(sd_root)" || { echo "sd=unreadable"; return 1; }
	if has_block "$r/linux/u-boot.txt"; then
		p="$(sed -n 's/^phos_partuuid=//p' "$r/linux/u-boot.txt" | head -1)"
		echo "enabled=1"; echo "partuuid=$p"
		echo "partitions_with_it=$(partuuid_count "$p")"
	else
		echo "enabled=0"
	fi
	sd_done
	[ -f "$TMPD/boot-from" ] && echo "this_boot=$(cat "$TMPD/boot-from")"
	return 0
}

cmd_check() {
	t="${1:-/media/usb0}" bad=0
	d="$(dev_of "$t")"
	[ -n "$d" ] && echo "ok   drive: $t ($d)" || { echo "FAIL $t isn't mounted"; return 1; }
	[ "$(fstype_of "$d")" = exfat ] && echo "ok   exFAT" || { echo "FAIL not exFAT (MiSTer's kernel mounts the start-up drive as exFAT)"; bad=1; }
	p="$(partuuid_of "$d")"
	if [ -z "$p" ]; then echo "FAIL the partition has no PARTUUID (no partition table?)"; bad=1
	elif [ "$(partuuid_count "$p")" -gt 1 ]; then echo "FAIL another partition has the same PARTUUID $p (a cloned drive): unplug it"; bad=1
	else echo "ok   boot ID (PARTUUID) $p"; fi
	need=$(( $(kb_used "$FAT") + 1048576 )) have="$(kb_free "$t")"
	[ "${have:-0}" -gt "$need" ] && echo "ok   room: $((have / 1024)) MB free, $((need / 1024)) MB needed" || { echo "FAIL not enough room: $((${have:-0} / 1024)) MB free, $((need / 1024)) MB needed"; bad=1; }
	k="$(uname -r | cut -d- -f1)"
	case "$k" in [6-9].[5-9]*|[6-9].[1-9][0-9]*|[7-9].*) echo "ok   kernel $k (rootwait= with a time limit)" ;;
		*) echo "FAIL kernel $k: rootwait=<seconds> needs Linux 6.5 or newer"; bad=1 ;; esac
	[ -f "$FAT/linux/zImage_dtb" ] && [ -f "$FAT/menu.rbf" ] && echo "ok   the SD card's start-up files are there" || { echo "FAIL linux/zImage_dtb or menu.rbf missing on the SD card"; bad=1; }
	return $bad
}

# rsync the SD card's files onto the drive: everything, games and other big folders without replacing
# what's on the drive already. exFAT: no owners or permissions, 2 s times.
rs() { rsync -rt --modify-window=2 --exclude='/System Volume Information' --exclude=/lost+found --exclude=/phosphor_boot_sd "$@"; }
cmd_copy() {
	t="${1:-/media/usb0}"
	cmd_check "$t" > /dev/null || { cmd_check "$t"; return 1; }
	log "copy to $t"
	rs --exclude=/games --exclude=/_Arcade/mame --exclude=/cheats "$FAT/" "$t/" || return 1
	for big in games _Arcade/mame cheats; do
		[ -d "$FAT/$big" ] && { mkdir -p "$t/$big"; rs --ignore-existing "$FAT/$big/" "$t/$big/" || return 1; }
	done
	sync
	# checked: what's on the SD card (outside the merged folders) is the same on the drive. Files that
	# changed while it copied (a log, a settings file Phosphor saved) are copied again, up to 3 times
	# (the whole check once; after that only the files that differed)
	diff="$(rs -c -n --out-format='%n' --exclude=/games --exclude=/_Arcade/mame --exclude=/cheats "$FAT/" "$t/" | grep -v '/$')"
	n=0
	while [ -n "$diff" ]; do
		n=$((n + 1))
		[ "$n" -gt 3 ] && { diff="$(echo "$diff" | head -5 | tr '\n' ' ')"; echo "FAIL these differ after the copy: $diff"; log "copy check failed: $diff"; return 1; }
		log "copy check: $(echo "$diff" | wc -l) file(s) changed while copying, copied again"
		echo "$diff" | rs -c --files-from=- "$FAT/" "$t/" || return 1
		sleep 2
		diff="$(echo "$diff" | rs -c -n --out-format='%n' --files-from=- "$FAT/" "$t/" | grep -v '/$')"
	done
	echo "ok   copied and checked"
}

set_priority() {   # downloader.ini storage_priority = $2 in file $1 (added in [MiSTer] when missing)
	f="$1" v="$2"
	[ -f "$f" ] || return 0
	if grep -q '^[[:space:]]*storage_priority[[:space:]]*=' "$f"; then
		sed -i "s/^[[:space:]]*storage_priority[[:space:]]*=.*/storage_priority = $v/" "$f"
	else
		sed -i "/^\[[Mm][Ii][Ss][Tt][Ee][Rr]\]/a storage_priority = $v" "$f"
	fi
}
get_priority() { sed -n 's/^[[:space:]]*storage_priority[[:space:]]*=[[:space:]]*//p' "$1" 2> /dev/null | head -1; }

cmd_enable() {
	t="${1:-/media/usb0}"
	cmd_check "$t" > /dev/null || { cmd_check "$t"; return 1; }
	p="$(partuuid_of "$(dev_of "$t")")"
	[ -f "$t/linux/linux.img" ] && [ -f "$t/MiSTer" ] || { echo "FAIL $t has no copy of the system: run copy first"; return 1; }
	# Update All on the drive: it is /media/fat there, so no "external" storage to prefer
	mkdir -p "$t/phosphor/state"
	old="$(get_priority "$t/downloader.ini")"
	[ -f "$t/phosphor/state/nvmeboot.state" ] || printf 'storage_priority=%s\npartuuid=%s\nenabled_at=%s\n' "$old" "$p" "$(date '+%Y-%m-%d %H:%M:%S')" > "$t/phosphor/state/nvmeboot.state"
	set_priority "$t/downloader.ini" "'prefer_sd'"
	r="$(sd_root)" || { echo "FAIL the SD card can't be mounted"; return 1; }
	u="$r/linux/u-boot.txt"
	{ strip_block "$u"; sed "s/@PARTUUID@/$p/" "$BLOCK"; } > "$u.new" && mv -f "$u.new" "$u" || { sd_done; return 1; }
	sd_done
	log "enabled: partuuid $p (downloader.ini storage_priority was ${old:-unset})"
	echo "ok   next start-up from $t (boot ID $p); the SD card if it doesn't come up within 20 s"
}

cmd_disable() {
	r="$(sd_root)" || { echo "FAIL the SD card can't be mounted"; return 1; }
	u="$r/linux/u-boot.txt"
	if has_block "$u"; then strip_block "$u" > "$u.new" && mv -f "$u.new" "$u"; fi
	rm -f "$r/phosphor_boot_sd"
	# the SD card's own downloader.ini was never changed; a copyback may have brought the drive's: put
	# its value back
	st="$FAT/phosphor/state/nvmeboot.state"
	if [ -f "$st" ]; then
		old="$(sed -n 's/^storage_priority=//p' "$st")"
		[ -n "$old" ] && set_priority "$r/downloader.ini" "$old"
	fi
	sd_done
	log "disabled"
	echo "ok   the next start-up is from the SD card"
}

# running from the drive: copied back to the SD card, newer files only (saves, settings, updated cores
# and Linux). The drive's own folders that were never the SD card's (its games) stay on it.
cmd_copyback() {
	from_drive || { echo "FAIL not running from the drive"; return 1; }
	r="$(sd_root)" || { echo "FAIL the SD card can't be mounted"; return 1; }
	need="$(rsync -rt --modify-window=2 --update -n --stats --exclude=/games --exclude=/_Arcade/mame --exclude=/cheats --exclude=/phosphor_boot_sd "$FAT/" "$r/" 2> /dev/null | sed -n 's/^Total transferred file size: \([0-9,]*\).*/\1/p' | tr -d ,)"
	free=$(( $(kb_free "$r") * 1024 ))
	[ "${need:-0}" -lt "$free" ] || { echo "FAIL the SD card needs $((need / 1048576)) MB, has $((free / 1048576)) MB"; sd_done; return 1; }
	rsync -rt --modify-window=2 --update --exclude=/games --exclude=/_Arcade/mame --exclude=/cheats --exclude=/phosphor_boot_sd --exclude=/linux/u-boot.txt "$FAT/" "$r/" || { sd_done; return 1; }
	sync; sd_done
	log "copied back to the SD card: $((${need:-0} / 1048576)) MB"
	echo "ok   copied back: $((${need:-0} / 1048576)) MB"
}

cmd_sync() {
	from_drive || return 0
	r="$(sd_root)" || return 1
	n=0
	for f in linux/zImage_dtb menu.rbf; do
		[ -f "$FAT/$f" ] || continue
		cmp -s "$FAT/$f" "$r/$f" && continue
		cp "$FAT/$f" "$r/$f.new" && mv -f "$r/$f.new" "$r/$f" && n=$((n + 1)) && log "synced $f to the SD card"
	done
	# u-boot.txt: the lines that aren't Phosphor's (USB polling, ethaddr...) from the drive's copy, ours kept
	if [ -f "$FAT/linux/u-boot.txt" ] && has_block "$r/linux/u-boot.txt"; then
		strip_block "$FAT/linux/u-boot.txt" > "$TMPD/ub.mine"; strip_block "$r/linux/u-boot.txt" > "$TMPD/ub.sd"
		if ! cmp -s "$TMPD/ub.mine" "$TMPD/ub.sd"; then
			{ cat "$TMPD/ub.mine"; grep -e '^phos_' -e '^mmcboot=if run phos_force' "$r/linux/u-boot.txt"; } > "$r/linux/u-boot.txt.new" && mv -f "$r/linux/u-boot.txt.new" "$r/linux/u-boot.txt" && n=$((n + 1)) && log "synced u-boot.txt's own lines"
		fi
		rm -f "$TMPD/ub.mine" "$TMPD/ub.sd"
	fi
	sd_done
	echo "synced=$n"
}

cmd_boot() {
	w="$(boot_word)"
	m="$($DEVMEM "$MARK" 32 2> /dev/null)"
	case "$w" in
		nvme)     echo drive > "$TMPD/boot-from" ;;
		fallback) echo fallback > "$TMPD/boot-from"; log "started from the SD card: the drive didn't come up" ;;
		forced)   echo forced > "$TMPD/boot-from" ;;
		check)    echo check > "$TMPD/boot-from"; log "started from the SD card to check the drive" ;;
		*)        rm -f "$TMPD/boot-from"; return 0 ;;     # not set up: nothing to do
	esac
	$DEVMEM "$MARK" 32 0 2> /dev/null                          # up: the next restart tries the drive again
	if [ "$w" = nvme ] && root_was_dirty; then
		# marked dirty when Linux mounted it: it can't be checked while it's in use, so once from the SD card
		# (at most once in 10 minutes: a drive the check can't clean is left to run as it is)
		st="$FAT/phosphor/state/nvmeboot.checked"
		last="$(cat "$st" 2> /dev/null)"; now="$(date +%s)"
		if [ -z "$last" ] || [ $((now - last)) -gt 600 ]; then
			mkdir -p "${st%/*}"; echo "$now" > "$st"
			log "the drive was marked dirty at start-up: restarting from the SD card to check it"
			$DEVMEM "$MARK" 32 "$M_CHECK" 2> /dev/null
			[ -n "${NVB_NOREBOOT:-}" ] && { echo check > "$TMPD/boot-from"; return 0; }
			sh "$FAT/phosphor/phosphor.sh" safe-stop > /dev/null 2>&1; sync; reboot
			sleep 30; return 0
		fi
		log "the drive was marked dirty at start-up, but it was checked less than 10 minutes ago: left as it is"
	fi
	if [ "$w" = nvme ]; then
		# usbmount found the drive again and mounted it a second time (/media/usbN): the same file system
		# twice, so the library would list every game twice
		fd="$(stat -c %d "$FAT" 2> /dev/null)"
		for u in /media/usb[0-9]*; do
			mountpoint -q "$u" 2> /dev/null && [ "$(stat -c %d "$u" 2> /dev/null)" = "$fd" ] && umount "$u" 2> /dev/null && log "unmounted $u (the start-up drive again)"
		done
	fi
	log "boot: $w (word was ${m:-?})"
	return 0
}

case "${1:-status}" in
	status) cmd_status ;;
	check) shift; cmd_check "$@" ;;
	copy) shift; cmd_copy "$@" ;;
	enable) shift; cmd_enable "$@" ;;
	disable) cmd_disable ;;
	copyback) cmd_copyback ;;
	sync) cmd_sync ;;
	boot) cmd_boot ;;
	after-check) cmd_after_check ;;
	*) sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
