/* Phosphor frontend: colours, fonts and shared styles.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#ifndef PHOS_THEME_H
#define PHOS_THEME_H

#include "lvgl.h"

typedef enum { ACCENT_AMBER, ACCENT_RED, ACCENT_GREEN, ACCENT_BLUE, ACCENT_PURPLE, ACCENT_COUNT } accent_t;

typedef struct {
	lv_color_t bg, panel, panel_on, line, text, dim, faint, icon_off, acc;
} palette_t;

extern palette_t PAL;

void theme_init(accent_t accent);
void theme_set_accent(accent_t accent);
accent_t theme_accent(void);
const char *theme_accent_name(accent_t a);

/* fonts */
/* each in three sizes: the 1280x720 design, 1.5 times that for a 1920x1080 screen, and about 0.72
   times for a small one (640x480: a 4:3 CRT). theme_tier picks: 2 big, 1 the design's, 0 small */
extern int theme_big, theme_tier;
/* and a fourth, tier 3, for a CRT at 240 or 288 lines (CRT mode 240p / 288p): text made for that few
   lines, in the look Settings > Video > Text picks (theme_crt_font: 0 Smooth, 1 Sharp, 2 Legible) */
extern int theme_crt_font;
#define PHOS_F(n)    (theme_tier == 3 ? (theme_crt_font == 1 ? &n##_cs : theme_crt_font == 2 ? &n##_cl : &n##_c) \
                      : theme_tier == 2 ? &n##_l : theme_tier == 0 ? &n##_s : &n)
#define FONT_SMALL   PHOS_F(phos_inter_14)
#define FONT_BODY    PHOS_F(phos_inter_16)
#define FONT_NAME    PHOS_F(phos_inter_med_18)
#define FONT_TITLE   PHOS_F(phos_inter_semi_22)
#define FONT_HEADING PHOS_F(phos_inter_semi_32)
#define FONT_LABEL   PHOS_F(phos_mono_13)   /* spaced capitals: LIBRARY, PICK UP WHERE YOU LEFT OFF */
#define FONT_WORD    PHOS_F(phos_mono_18)   /* PHOSPHOR wordmark */
#define FONT_LOGO    PHOS_F(phos_mono_40)

/* layout (designed at 1280x720; scaled with the screen). A screen narrower than 16:9 (4:3, 5:4) is
   scaled by its height, so the design keeps its full 720 height and gets narrower: CW is the canvas
   width in design units (1280 at 16:9, 960 at 4:3). Anything placed from the right or sized to the
   width uses CW, not 1280. */
int S(int px);
extern int CW;
void theme_set_scale(int screen_w, int screen_h);
#define NARROW (CW < 1200)          /* 4:3 and the like: fewer columns, narrower panels */
/* horizontal positions and widths in the 1280-wide design, fitted to the canvas: the 56-unit margins
   stay, what's between them is squeezed (at 16:9 these are exactly S()) */
int SX(int px);
int SW(int px);
/* 240 or 288 lines (LR): the main screens have layouts of their own, designed at 320x240 (L(): those
   units to pixels, 1 at 240p, 1.2 at 288p; LRW: the screen's width in them). Other screens are the
   design's, scaled down */
extern int theme_lr, LRW;
int L(int px);

/* helpers */
lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);
lv_obj_t *mk_box(lv_obj_t *parent, int w, int h, lv_color_t bg, int radius);
void spaced_caps(char *dst, size_t n, const char *src);   /* "Library" -> "L I B R A R Y" style tracking */

#endif
