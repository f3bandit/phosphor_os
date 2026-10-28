#!/bin/bash
# Regenerate the LVGL bitmap fonts in fonts/ (needs: cd tools && npm install lv_font_conv@1.5.2).
# Inter: SIL Open Font License 1.1. DejaVu Sans Mono: Bitstream Vera / DejaVu licence.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
set -e
cd "$(dirname "$0")/.."
C=tools/node_modules/.bin/lv_font_conv
INTER="${INTER_DIR:-/usr/share/fonts/opentype/inter}"
MONO="${MONO_FONT:-/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf}"
TEXT="0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026,0x203A,0x2190-0x2193,0x2713"
MONO_RANGE="0x20-0x7E,0xA0-0xFF,0x2014,0x2022,0x2026,0x2190-0x2195,0x2261"
gen() { # name size font range
	$C --no-compress --no-prefilter --bpp 4 --size "$2" --font "$3" -r "$4" \
		--format lvgl --lv-include lvgl.h -o "fonts/$1.c"
}
gen phos_inter_14      14 "$INTER/Inter-Regular.otf"  "$TEXT"
gen phos_inter_16      16 "$INTER/Inter-Regular.otf"  "$TEXT"
gen phos_inter_med_18  18 "$INTER/Inter-Medium.otf"   "$TEXT"
gen phos_inter_semi_22 22 "$INTER/Inter-SemiBold.otf" "$TEXT"
gen phos_inter_semi_32 32 "$INTER/Inter-SemiBold.otf" "$TEXT"
gen phos_mono_13       13 "$MONO" "$MONO_RANGE"
gen phos_mono_18       18 "$MONO" "$MONO_RANGE"
gen phos_mono_40       40 "$MONO" "$MONO_RANGE"
# the same set 1.5 times bigger, for a 1920x1080 screen (Phosphor fills the screen at its own size)
gen phos_inter_14_l    21 "$INTER/Inter-Regular.otf"  "$TEXT"
gen phos_inter_16_l    24 "$INTER/Inter-Regular.otf"  "$TEXT"
gen phos_inter_med_18_l 27 "$INTER/Inter-Medium.otf"  "$TEXT"
gen phos_inter_semi_22_l 33 "$INTER/Inter-SemiBold.otf" "$TEXT"
gen phos_inter_semi_32_l 48 "$INTER/Inter-SemiBold.otf" "$TEXT"
gen phos_mono_13_l     20 "$MONO" "$MONO_RANGE"
gen phos_mono_18_l     27 "$MONO" "$MONO_RANGE"
gen phos_mono_40_l     60 "$MONO" "$MONO_RANGE"
# and about 0.72 times, for small screens (640x480, a 4:3 CRT at 480 lines: Phosphor scales by height there)
small() {
gen phos_inter_14_s    10 "$INTER/Inter-Regular.otf"  "$TEXT"
gen phos_inter_16_s    12 "$INTER/Inter-Regular.otf"  "$TEXT"
gen phos_inter_med_18_s 13 "$INTER/Inter-Medium.otf"  "$TEXT"
gen phos_inter_semi_22_s 16 "$INTER/Inter-SemiBold.otf" "$TEXT"
gen phos_inter_semi_32_s 23 "$INTER/Inter-SemiBold.otf" "$TEXT"
gen phos_mono_13_s     10 "$MONO" "$MONO_RANGE"
gen phos_mono_18_s     13 "$MONO" "$MONO_RANGE"
gen phos_mono_40_s     29 "$MONO" "$MONO_RANGE"
}
small
# a CRT at 240 or 288 lines (Settings > Video > CRT mode 240p / 288p): bigger for the screen, in three looks
# (Text: Smooth, Sharp: no anti-aliasing, Legible: DejaVu Sans Bold), as Console Mode offers
crt() { # suffix bpp sans-regular sans-bold
gen_b() { $C --no-compress --no-prefilter --bpp "$2" --size "$3" --font "$4" -r "$5" --format lvgl --lv-include lvgl.h -o "fonts/$1.c"; }
gen_b phos_inter_14$1      $2 10 "$3" "$TEXT"
gen_b phos_inter_16$1      $2 12 "$3" "$TEXT"
gen_b phos_inter_med_18$1  $2 12 "$4" "$TEXT"
gen_b phos_inter_semi_22$1 $2 14 "$4" "$TEXT"
gen_b phos_inter_semi_32$1 $2 17 "$4" "$TEXT"
gen_b phos_mono_13$1       $2 8  "$MONO" "$MONO_RANGE"
gen_b phos_mono_18$1       $2 10 "$MONO" "$MONO_RANGE"
gen_b phos_mono_40$1       $2 24 "$MONO" "$MONO_RANGE"
}
DJ="${DEJAVU_DIR:-/usr/share/fonts/truetype/dejavu}"
crt _c  4 "$INTER/Inter-SemiBold.otf" "$INTER/Inter-Bold.otf"
crt _cs 1 "$INTER/Inter-SemiBold.otf" "$INTER/Inter-Bold.otf"
crt _cl 4 "$DJ/DejaVuSans-Bold.ttf"   "$DJ/DejaVuSans-Bold.ttf"
# LVGL 9 has no LV_VERSION_CHECK (so it reads as false). That's right for the old glyph cache,
# but the public descriptor must be const to match LV_FONT_DECLARE.
for f in fonts/*.c; do
	awk '{ lines[NR] = $0 } END {
		for (i = 1; i <= NR; i++) {
			if (lines[i] == "#if LV_VERSION_CHECK(8, 0, 0)" && lines[i + 1] ~ /^const lv_font_t /) print "#if 1"
			else print lines[i]
		}
	}' "$f" > "$f.tmp" && mv "$f.tmp" "$f"
done
