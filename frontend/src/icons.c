/* Phosphor frontend: outline icons drawn as vectors.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "icons.h"
#include "theme.h"

typedef struct {
	icon_t icon;
	int level;
} icon_data_t;

typedef struct {
	lv_layer_t *layer;
	int x0, y0;     /* top-left of the icon on screen */
	float k;        /* pixels per grid unit */
	lv_color_t col, off;
	int w;          /* stroke width in pixels */
} pen_t;

static lv_point_precise_t P(pen_t *p, float x, float y)
{
	lv_point_precise_t q = { (lv_value_precise_t)(p->x0 + x * p->k), (lv_value_precise_t)(p->y0 + y * p->k) };
	return q;
}

static void line(pen_t *p, float x1, float y1, float x2, float y2)
{
	lv_draw_line_dsc_t d;
	lv_draw_line_dsc_init(&d);
	d.color = p->col;
	d.width = p->w;
	d.round_start = 1;
	d.round_end = 1;
	d.p1 = P(p, x1, y1);
	d.p2 = P(p, x2, y2);
	lv_draw_line(p->layer, &d);
}

static void poly(pen_t *p, const float *xy, int n, int closed)
{
	for (int i = 0; i + 1 < n; i++) line(p, xy[2 * i], xy[2 * i + 1], xy[2 * i + 2], xy[2 * i + 3]);
	if (closed) line(p, xy[2 * n - 2], xy[2 * n - 1], xy[0], xy[1]);
}

static void rect(pen_t *p, float x, float y, float w, float h, float r)
{
	lv_draw_rect_dsc_t d;
	lv_draw_rect_dsc_init(&d);
	d.bg_opa = LV_OPA_TRANSP;
	d.border_color = p->col;
	d.border_width = p->w;
	d.border_opa = LV_OPA_COVER;
	d.radius = (int32_t)(r * p->k);
	lv_area_t a = { (int32_t)(p->x0 + x * p->k), (int32_t)(p->y0 + y * p->k),
	                (int32_t)(p->x0 + (x + w) * p->k), (int32_t)(p->y0 + (y + h) * p->k) };
	lv_draw_rect(p->layer, &d, &a);
}

static void circle(pen_t *p, float cx, float cy, float r, int filled)
{
	lv_draw_rect_dsc_t d;
	lv_draw_rect_dsc_init(&d);
	d.radius = LV_RADIUS_CIRCLE;
	if (filled) {
		d.bg_color = p->col;
		d.bg_opa = LV_OPA_COVER;
	} else {
		d.bg_opa = LV_OPA_TRANSP;
		d.border_color = p->col;
		d.border_width = p->w;
		d.border_opa = LV_OPA_COVER;
	}
	lv_area_t a = { (int32_t)(p->x0 + (cx - r) * p->k), (int32_t)(p->y0 + (cy - r) * p->k),
	                (int32_t)(p->x0 + (cx + r) * p->k), (int32_t)(p->y0 + (cy + r) * p->k) };
	lv_draw_rect(p->layer, &d, &a);
}

static void arc(pen_t *p, float cx, float cy, float r, int a0, int a1, lv_color_t c)
{
	lv_draw_arc_dsc_t d;
	lv_draw_arc_dsc_init(&d);
	d.color = c;
	d.width = p->w;
	d.rounded = 1;
	d.center.x = (int32_t)(p->x0 + cx * p->k);
	d.center.y = (int32_t)(p->y0 + cy * p->k);
	d.radius = (uint16_t)(r * p->k);
	d.start_angle = a0;
	d.end_angle = a1;
	lv_draw_arc(p->layer, &d);
}

static void draw_cb(lv_event_t *e)
{
	lv_obj_t *obj = lv_event_get_target_obj(e);
	icon_data_t *id = lv_obj_get_user_data(obj);
	if (!id) return;
	lv_area_t c;
	lv_obj_get_coords(obj, &c);
	int size = lv_area_get_width(&c);
	int grid = id->icon <= ICON_BLUETOOTH ? 24 : 72;
	pen_t p = {
		.layer = lv_event_get_layer(e), .x0 = c.x1, .y0 = c.y1, .k = (float)size / grid,
		.col = lv_obj_get_style_text_color(obj, 0), .off = PAL.icon_off,
	};
	p.w = grid == 24 ? (size >= 22 ? 2 : 1) : LV_MAX(2, size / 30);

	switch (id->icon) {
	case ICON_SHARE: { /* folder */
		const float f[] = { 4, 7, 11, 7, 13, 9, 20, 9, 20, 18, 4, 18 };
		poly(&p, f, 6, 1);
		break;
	}
	case ICON_ETHERNET:
		rect(&p, 6, 5, 12, 9, 1);
		line(&p, 9, 14, 9, 17); line(&p, 15, 14, 15, 17); line(&p, 12, 14, 12, 19); line(&p, 8, 19, 16, 19);
		break;
	case ICON_WIFI: {
		int lvl = id->level < 0 ? 3 : id->level;
		arc(&p, 12, 22, 15, 228, 312, lvl >= 3 ? p.col : p.off);
		arc(&p, 12, 22, 10.5f, 226, 314, lvl >= 2 ? p.col : p.off);
		arc(&p, 12, 22, 6, 222, 318, lvl >= 1 ? p.col : p.off);
		circle(&p, 12, 19, 1.3f, 1);
		break;
	}
	case ICON_BLUETOOTH: {
		const float b[] = { 7, 7, 17, 17, 12, 21, 12, 3, 17, 7, 7, 17 };
		poly(&p, b, 6, 0);
		break;
	}
	case ICON_DISC:
		circle(&p, 36, 36, 26, 0); circle(&p, 36, 36, 6, 0);
		break;
	case ICON_CARTRIDGE:
		rect(&p, 16, 10, 40, 52, 4); rect(&p, 24, 18, 24, 16, 1); line(&p, 22, 54, 50, 54);
		break;
	case ICON_HANDHELD:
		rect(&p, 20, 6, 32, 60, 6); rect(&p, 26, 13, 20, 18, 1);
		circle(&p, 31, 46, 3, 0); circle(&p, 43, 44, 3, 0);
		break;
	case ICON_ARCADE: {
		const float a[] = { 20, 64, 20, 20, 28, 8, 44, 8, 52, 20, 52, 64 };
		poly(&p, a, 6, 1);
		rect(&p, 26, 20, 20, 16, 1); circle(&p, 30, 48, 3, 0); line(&p, 42, 44, 42, 52);
		break;
	}
	case ICON_COMPUTER:
		rect(&p, 10, 12, 52, 34, 3); line(&p, 26, 58, 46, 58); line(&p, 36, 46, 36, 58);
		break;
	case ICON_PORTS:
		rect(&p, 18, 26, 36, 28, 2);
		line(&p, 28, 26, 28, 16); line(&p, 28, 16, 44, 16); line(&p, 44, 16, 44, 26); line(&p, 28, 40, 44, 40);
		break;
	case ICON_GEAR:
		circle(&p, 36, 36, 9, 0);
		circle(&p, 36, 36, 22, 0);
		for (int i = 0; i < 8; i++) {
			int32_t s = lv_trigo_sin(i * 45), co = lv_trigo_cos(i * 45);
			float dx = (float)co / LV_TRIGO_SIN_MAX, dy = (float)s / LV_TRIGO_SIN_MAX;
			line(&p, 36 + dx * 22, 36 + dy * 22, 36 + dx * 29, 36 + dy * 29);
		}
		break;
	case ICON_STAR: {
		/* five points, outline */
		float s[20];
		for (int i = 0; i < 10; i++) {
			int a = -90 + i * 36;
			float r = (i & 1) ? 12.5f : 28.0f;
			s[2 * i] = 36 + r * (float)lv_trigo_cos(a) / LV_TRIGO_SIN_MAX;
			s[2 * i + 1] = 38 + r * (float)lv_trigo_sin(a) / LV_TRIGO_SIN_MAX;
		}
		poly(&p, s, 10, 1);
		break;
	}
	case ICON_CLOCK:
		circle(&p, 36, 36, 26, 0);
		line(&p, 36, 36, 36, 20); line(&p, 36, 36, 47, 42);
		break;
	case ICON_SEARCH:
		circle(&p, 31, 31, 18, 0);
		line(&p, 44, 44, 58, 58);
		break;
	case ICON_POWER:                                     /* the power symbol */
		arc(&p, 36, 39, 22, 300, 240, p.col);
		line(&p, 36, 11, 36, 34);
		break;
	case ICON_RESTART:                                   /* a circle arrow */
		arc(&p, 36, 37, 22, 20, 300, p.col);
		line(&p, 47, 18, 58, 16); line(&p, 47, 18, 50, 28);
		break;
	case ICON_SCREEN:                                    /* a screen with a stand */
		rect(&p, 9, 13, 54, 36, 5);
		line(&p, 36, 49, 36, 58); line(&p, 24, 59, 48, 59);
		break;
	case ICON_CHECK:
		{ const float k[] = { 20, 38, 31, 49, 53, 25 }; poly(&p, k, 3, 0); }
		break;
	default:
		break;
	}
}

static void delete_cb(lv_event_t *e)
{
	lv_free(lv_obj_get_user_data(lv_event_get_target_obj(e)));
}

lv_obj_t *icon_create(lv_obj_t *parent, icon_t icon, int size)
{
	lv_obj_t *o = lv_obj_create(parent);
	lv_obj_remove_style_all(o);
	lv_obj_set_size(o, size, size);
	lv_obj_set_scrollable(o, false);
	lv_obj_set_clickable(o, false);
	icon_data_t *d = lv_malloc(sizeof(*d));
	d->icon = icon;
	d->level = -1;
	lv_obj_set_user_data(o, d);
	lv_obj_add_event_cb(o, draw_cb, LV_EVENT_DRAW_MAIN, NULL);
	lv_obj_add_event_cb(o, delete_cb, LV_EVENT_DELETE, NULL);
	return o;
}

void icon_set(lv_obj_t *o, icon_t icon)
{
	icon_data_t *d = lv_obj_get_user_data(o);
	if (d && d->icon != icon) { d->icon = icon; lv_obj_invalidate(o); }
}

void icon_set_level(lv_obj_t *o, int level)
{
	icon_data_t *d = lv_obj_get_user_data(o);
	if (d && d->level != level) { d->level = level; lv_obj_invalidate(o); }
}
