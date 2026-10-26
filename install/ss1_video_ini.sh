#!/bin/sh
# SS1 video ini files set to Taki's settings (SuperStation-Documentation/INI_Files, 2026-09-29).
#
# WHY: the SuperStation One has its own analog video hardware: a Sony CXA encoder for S-Video and
# composite, chosen with DIP switch 3 (down = Sony, the way it ships; up = the FPGA's own encoder).
# The Sony chip needs vga_mode=subcarrier in MiSTer.ini (RGB plus a colour subcarrier; MiSTer's
# video.cpp: "subcarrier for CXA2075 encoders"); yc.txt, when there, overrides its value per core.
# MiSTer's own defaults (vga_mode=rgb) give no colour over S-Video/composite on an SS1. Taki ships
# MiSTer.ini plus four profiles; these are the video lines his files set, so only these are
# edited, in place; every other line (main=, HDMI settings, per-core sections) stays as it is:
#
#   file              vga_mode    composite_sync vga_sog video_info logo
#   MiSTer.ini        subcarrier  1              0       2          0     HDMI, and S-Video/composite through the Sony chip
#   MiSTer_RGHV.ini   rgb         0              0       2          0     RGB with separate H/V sync (VGA monitors)
#   MiSTer_RGsB.ini   rgb         1              0       2          0     RGB with composite sync (SCART); DIP 4 adds sync on green
#   MiSTer_SVID.ini   svideo      1              1       2          0     the FPGA's own S-Video/composite (DIP 3 up)
#   MiSTer_YPbP.ini   ypbpr       1              0       2          0     component
#
# A missing profile is made from MiSTer.ini (so it keeps main= and the user's settings) with its
# values. MiSTer_example.ini (MiSTer's reference copy) is renamed MiSTer_example.ini.txt: Main counts
# every MiSTer_*.ini as a profile and offers only the first three it finds on the card (cfg.cpp
# cfg_get_name), so the example file pushed Taki's profiles out. A copy of every file is kept first.
# Phosphor's Main offers four profiles (phosphor-main cfg.cpp CFG_ALT_MAX); MiSTer's offers three.
#
# Where this runs (keep them the same; see docs/ss1-video.md):
#   - Phosphor: install.sh runs this file once per card (fresh install, every update, the SD image),
#     marker phosphor/backup/video-ini-taki.done; after that the user's own changes stay.
#   - SS1 Tool (Windows and Android): ss1video.go does the same edit on a Console Mode SuperStation
#     when it connects, once per card (marker config/ss1tool/video-ini-taki.done), and says so.
#   Change a value here and it must change there too (and in the docs table).
#
# usage: ss1_video_ini.sh <card root, e.g. /media/fat> <backup folder>
# prints one "changed <file> <key> <old> -> <new>" line per edit, "made <file>", "renamed <file>"
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
set -e
FAT="$1"; BK="$2"
[ -f "$FAT/MiSTer.ini" ] || { echo "no MiSTer.ini in $FAT"; exit 1; }
mkdir -p "$BK"

# the value of key in [MiSTer] (active lines only), "" when not set
getkey() { # file key
	awk -v k="$2" '
	{ t = $0; sub(/\r$/, "", t); gsub(/^[ \t]+/, "", t)
	  if (t ~ /^\[/) { s = tolower(t); sub(/\].*/, "]", s); sec = s; next }
	  if (sec != "[mister]" && sec != "") next
	  if (t ~ "^" k "[ \t]*=") { v = t; sub(/^[^=]*=[ \t]*/, "", v); sub(/[ \t]*;.*$/, "", v); sub(/[ \t]+$/, "", v); print v; exit } }' "$1"
}

# set key=value in the [MiSTer] section: the active line, else the first commented one, else added
# after the [MiSTer] line; an inline comment after the value and the line ending (CRLF) are kept
setkey() { # file key value
	awk -v k="$2" -v v="$3" '
	{ lines[NR] = $0 }
	END {
		sec = ""; act = 0; com = 0; hdr = 0
		for (i = 1; i <= NR; i++) {
			l = lines[i]; sub(/\r$/, "", l); t = l; gsub(/^[ \t]+/, "", t)
			if (t ~ /^\[/) { s = tolower(t); sub(/\].*/, "]", s); sec = s; if (sec == "[mister]" && !hdr) hdr = i; continue }
			if (sec != "[mister]" && sec != "") continue
			if (!act && t ~ "^" k "[ \t]*=") act = i
			else if (!com && t ~ "^;[ \t]*" k "[ \t]*=") com = i
		}
		at = act ? act : com
		for (i = 1; i <= NR; i++) {
			cr = (lines[i] ~ /\r$/) ? "\r" : ""
			if (i == at) {
				l = lines[i]; sub(/\r$/, "", l); sub(/^[ \t]*;?[ \t]*/, "", l)
				rest = ""
				if (match(l, /[ \t]*;.*/)) rest = substr(l, RSTART)
				newl = k "=" v
				if (rest != "") { pad = 23 - length(newl); if (pad < 1) pad = 1; gsub(/^[ \t]*/, "", rest); newl = newl sprintf("%" pad "s", "") rest }
				print newl cr
			} else print lines[i]
			if (!at && i == hdr) print k "=" v cr
		}
		if (!at && !hdr) { print "[MiSTer]"; print k "=" v }
	}' "$1" > "$1.new" && mv -f "$1.new" "$1"
}

apply() { # file vga_mode composite_sync vga_sog video_info logo
	f="$1"; shift
	for kv in "vga_mode=$1" "composite_sync=$2" "vga_sog=$3" "video_info=$4" "logo=$5"; do
		k="${kv%%=*}"; v="${kv#*=}"
		was="$(getkey "$f" "$k")"
		if [ "$was" != "$v" ]; then
			setkey "$f" "$k" "$v"
			echo "changed ${f##*/} $k ${was:-(not set)} -> $v"
		fi
	done
}

for f in MiSTer.ini MiSTer_RGHV.ini MiSTer_RGsB.ini MiSTer_SVID.ini MiSTer_YPbP.ini MiSTer_example.ini; do
	if [ -f "$FAT/$f" ]; then cp -p "$FAT/$f" "$BK/$f"; fi
done
apply "$FAT/MiSTer.ini" subcarrier 1 0 2 0
for p in "RGHV rgb 0 0" "RGsB rgb 1 0" "SVID svideo 1 1" "YPbP ypbpr 1 0"; do
	set -- $p
	f="$FAT/MiSTer_$1.ini"
	if [ ! -f "$f" ]; then cp "$FAT/MiSTer.ini" "$f"; echo "made ${f##*/}"; fi
	apply "$f" "$2" "$3" "$4" 2 0
done
if [ -f "$FAT/MiSTer_example.ini" ]; then
	mv -f "$FAT/MiSTer_example.ini" "$FAT/MiSTer_example.ini.txt"
	echo "renamed MiSTer_example.ini -> MiSTer_example.ini.txt"
fi
sync
