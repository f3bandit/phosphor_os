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
#define FONT_SMALL   (&phos_inter_14)
#define FONT_BODY    (&phos_inter_16)
#define FONT_NAME    (&phos_inter_med_18)
#define FONT_TITLE   (&phos_inter_semi_22)
#define FONT_HEADING (&phos_inter_semi_32)
#define FONT_LABEL   (&phos_mono_13)   /* spaced capitals: LIBRARY, PICK UP WHERE YOU LEFT OFF */
#define FONT_WORD    (&phos_mono_18)   /* PHOSPHOR wordmark */
#define FONT_LOGO    (&phos_mono_40)

/* layout (designed at 1280x720; scaled with the screen) */
int S(int px);
void theme_set_scale(int screen_w, int screen_h);

/* helpers */
lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);
lv_obj_t *mk_box(lv_obj_t *parent, int w, int h, lv_color_t bg, int radius);
void spaced_caps(char *dst, size_t n, const char *src);   /* "Library" -> "L I B R A R Y" style tracking */

#endif
