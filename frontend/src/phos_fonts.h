/* Phosphor fonts: Inter (SIL OFL 1.1) and DejaVu Sans Mono (Bitstream Vera licence),
 * converted to LVGL bitmap fonts by tools/make_fonts.sh. */
#ifndef PHOS_FONTS_H
#define PHOS_FONTS_H

#define LV_FONT_CUSTOM_DECLARE \
	LV_FONT_DECLARE(phos_inter_14) LV_FONT_DECLARE(phos_inter_16) LV_FONT_DECLARE(phos_inter_med_18) \
	LV_FONT_DECLARE(phos_inter_semi_22) LV_FONT_DECLARE(phos_inter_semi_32) \
	LV_FONT_DECLARE(phos_mono_13) LV_FONT_DECLARE(phos_mono_18) LV_FONT_DECLARE(phos_mono_40)

#undef LV_FONT_DEFAULT
#define LV_FONT_DEFAULT &phos_inter_16

#endif
