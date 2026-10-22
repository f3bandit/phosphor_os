/* Phosphor frontend: colours, fonts and shared styles.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "theme.h"
#include <ctype.h>
#include <string.h>

palette_t PAL;
static accent_t cur_accent = ACCENT_AMBER;
static int scale_pct = 100;

static const uint32_t ACCENTS[ACCENT_COUNT] = {
	0xffb347, /* amber: the phosphor glow */
	0xff6b5e, /* red */
	0x5cf08a, /* green: P1 phosphor */
	0x6cb6ff, /* blue */
	0xc792ff, /* purple */
};
static const char *ACCENT_NAMES[ACCENT_COUNT] = { "Amber", "Red", "Green", "Blue", "Purple" };

void theme_set_accent(accent_t a)
{
	if (a >= ACCENT_COUNT) a = ACCENT_AMBER;
	cur_accent = a;
	PAL.acc = lv_color_hex(ACCENTS[a]);
	/* the focused card's fill is the panel tinted a little towards the accent */
	PAL.panel_on = lv_color_mix(PAL.acc, lv_color_hex(0x141416), 20);
}

void theme_init(accent_t a)
{
	PAL.bg = lv_color_hex(0x0e0e10);
	PAL.panel = lv_color_hex(0x17171b);
	PAL.line = lv_color_hex(0x2a2a31);
	PAL.text = lv_color_hex(0xece8e1);
	PAL.dim = lv_color_hex(0x8d8a85);
	PAL.faint = lv_color_hex(0x5c5a57);
	PAL.icon_off = lv_color_hex(0x6f6c68);
	theme_set_accent(a);
}

accent_t theme_accent(void) { return cur_accent; }
const char *theme_accent_name(accent_t a) { return a < ACCENT_COUNT ? ACCENT_NAMES[a] : "?"; }

int theme_big;

void theme_set_scale(int w, int h)
{
	int sx = w * 100 / 1280, sy = h * 100 / 720;
	scale_pct = sx < sy ? sx : sy;
	if (scale_pct < 50) scale_pct = 50;
	theme_big = scale_pct >= 125;
}

int S(int px) { return px * scale_pct / 100; }

lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
	lv_obj_t *l = lv_label_create(parent);
	lv_obj_set_style_text_font(l, font, 0);
	lv_obj_set_style_text_color(l, color, 0);
	lv_label_set_text(l, text ? text : "");
	return l;
}

lv_obj_t *mk_box(lv_obj_t *parent, int w, int h, lv_color_t bg, int radius)
{
	lv_obj_t *o = lv_obj_create(parent);
	lv_obj_remove_style_all(o);
	lv_obj_set_size(o, w, h);
	lv_obj_set_style_bg_color(o, bg, 0);
	lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(o, radius, 0);
	lv_obj_set_scrollable(o, false);
	return o;
}

void spaced_caps(char *dst, size_t n, const char *src)
{
	size_t j = 0;
	for (size_t i = 0; src[i] && j + 1 < n; i++) {
		dst[j++] = (char)toupper((unsigned char)src[i]);
	}
	dst[j] = 0;
}
