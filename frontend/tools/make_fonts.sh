#!/bin/bash
# Regenerate the LVGL bitmap fonts in fonts/ (needs: cd tools && npm install lv_font_conv@1.5.2).
# Inter: SIL Open Font License 1.1. DejaVu Sans Mono: Bitstream Vera / DejaVu licence.
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
