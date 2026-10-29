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

int theme_big, theme_tier = 1, theme_crt_font, theme_lr, LRW = 320;
static int lr_pct = 100;
int L(int px) { return px * lr_pct / 100; }
int CW = 1280;

void theme_set_scale(int w, int h)
{
	/* by the height: at 16:9 that's the width too; narrower (4:3) keeps the design's 720 rows and gets a
	   narrower canvas, down to 900 design units wide (5:4); wider gets a wider one */
	scale_pct = h * 100 / 720;
	if (scale_pct < 30) scale_pct = 30;
	CW = w * 100 / scale_pct;
	if (CW < 900) {                                     /* (taller than 5:4: by the width again) */
		scale_pct = w * 100 / 900;
		CW = 900;
	}
	theme_big = scale_pct >= 125;
	theme_tier = scale_pct >= 125 ? 2 : scale_pct < 85 ? 0 : 1;
	theme_lr = h <= 300;                                 /* 240p / 288p (with any overscan taken off) */
	if (theme_lr) {
		theme_tier = 3;
		lr_pct = h * 100 / 240 < 100 ? 100 : h * 100 / 240;
		LRW = w * 100 / lr_pct;
	}
}

int S(int px) { return px * scale_pct / 100; }
int SX(int px) { return CW >= 1280 ? S(px) : S(56 + (px - 56) * (CW - 112) / 1168); }
int SW(int px) { return CW >= 1280 ? S(px) : S(px * (CW - 112) / 1168); }

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
