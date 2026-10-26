/* Phosphor fonts: Inter (SIL OFL 1.1) and DejaVu Sans Mono (Bitstream Vera licence), converted to
 * LVGL bitmap fonts by tools/make_fonts.sh; the fonts keep their own licences (THIRD_PARTY_NOTICES.md).
 * This header: Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_FONTS_H
#define PHOS_FONTS_H

#define LV_FONT_CUSTOM_DECLARE \
	LV_FONT_DECLARE(phos_inter_14) LV_FONT_DECLARE(phos_inter_16) LV_FONT_DECLARE(phos_inter_med_18) \
	LV_FONT_DECLARE(phos_inter_semi_22) LV_FONT_DECLARE(phos_inter_semi_32) \
	LV_FONT_DECLARE(phos_mono_13) LV_FONT_DECLARE(phos_mono_18) LV_FONT_DECLARE(phos_mono_40) \
	LV_FONT_DECLARE(phos_inter_14_l) LV_FONT_DECLARE(phos_inter_16_l) LV_FONT_DECLARE(phos_inter_med_18_l) \
	LV_FONT_DECLARE(phos_inter_semi_22_l) LV_FONT_DECLARE(phos_inter_semi_32_l) \
	LV_FONT_DECLARE(phos_mono_13_l) LV_FONT_DECLARE(phos_mono_18_l) LV_FONT_DECLARE(phos_mono_40_l)

#undef LV_FONT_DEFAULT
#define LV_FONT_DEFAULT &phos_inter_16

#endif
