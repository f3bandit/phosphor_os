/* Phosphor shell: a controller's button layout (see padview.h).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "padview.h"
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "padlayouts.h"

#define MAXEL 40

typedef struct {
	char t;
	const char *id;                /* "a", or alternatives "c|r1" */
	lv_obj_t *shape, *lab, *knob, *fill, *arm[4], *sym;
	int kx, ky, kr, full;          /* stick / lever knob centre and travel; trigger fill width */
} el_t;

struct padview {
	lv_obj_t *box;
	int w, h, layout, n;
	el_t el[MAXEL];
};

static int sc(const padview_t *p, int v) { return v * p->w / 600; }

int padview_count(void) { return PL_COUNT; }
const char *padview_key(int i) { return i >= 0 && i < PL_COUNT ? PLAYOUTS[i].key : "generic"; }
int padview_index(const char *key)
{
	for (int i = 0; key && i < PL_COUNT; i++) if (!strcmp(PLAYOUTS[i].key, key)) return i;
	return -1;
}
const char *padview_name(const char *key) { int i = padview_index(key); return i < 0 ? "" : PLAYOUTS[i].name; }

/* a button's name on this layout ("LB" on Xbox-style, "L1" on PlayStation), for text */
const char *padview_label(const char *key, const char *id)
{
	static const struct { const char *id, *name; } D[] = { { "up", "Up" }, { "down", "Down" }, { "left", "Left" }, { "right", "Right" } };
	for (unsigned i = 0; i < sizeof(D) / sizeof(D[0]); i++) if (!strcmp(id, D[i].id)) return D[i].name;
	int li = padview_index(key);
	if (li < 0) li = padview_index("generic");
	const char *lab = NULL;
	for (int i = 0; li >= 0 && i < PLAYOUTS[li].n; i++) if (!strcmp(PLAYOUTS[li].it[i].id, id)) lab = PLAYOUTS[li].it[i].lab;
	if (lab && (unsigned char)lab[0] >= 0x80) {          /* PlayStation's symbols: the font has no such glyphs */
		if (!strcmp(id, "y")) return "Triangle";
		if (!strcmp(id, "b")) return "Circle";
		if (!strcmp(id, "a")) return "Cross";
		if (!strcmp(id, "x")) return "Square";
	}
	return lab && *lab ? lab : id;
}

static lv_obj_t *shape(padview_t *p, int x, int y, int w, int h, int radius)
{
	lv_obj_t *o = mk_box(p->box, w, h, PAL.panel, radius);
	lv_obj_set_pos(o, x, y);
	lv_obj_set_style_border_width(o, LV_MAX(1, S(2)), 0);
	lv_obj_set_style_border_color(o, PAL.line, 0);
	return o;
}

/* PlayStation's symbols aren't in the fonts: drawn */
static lv_obj_t *ps_symbol(padview_t *p, const char *lab, int cx, int cy, int r)
{
	static lv_point_precise_t pts[4][5];
	int which = !strcmp(lab, "\xE2\x96\xB3") ? 0 : !strcmp(lab, "\xE2\x97\x8B") ? 1 : !strcmp(lab, "\xE2\x9C\x95") ? 2 :
	            !strcmp(lab, "\xE2\x96\xA1") ? 3 : -1;
	if (which < 0) return NULL;
	int s = r * 9 / 20;
	lv_obj_t *o;
	if (which == 1 || which == 3) {          /* circle, square: a ring */
		o = mk_box(p->box, 2 * s, 2 * s, PAL.panel, which == 1 ? LV_RADIUS_CIRCLE : LV_MAX(1, S(2)));
		lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
		lv_obj_set_style_border_width(o, LV_MAX(1, S(2)), 0);
		lv_obj_set_style_border_color(o, PAL.text, 0);
		lv_obj_set_pos(o, cx - s, cy - s);
		return o;
	}
	o = lv_line_create(p->box);
	lv_point_precise_t *q = pts[which];
	if (which == 0) {                        /* triangle */
		q[0] = (lv_point_precise_t){ s, 0 }; q[1] = (lv_point_precise_t){ 2 * s, 2 * s * 87 / 100 };
		q[2] = (lv_point_precise_t){ 0, 2 * s * 87 / 100 }; q[3] = q[0];
		lv_line_set_points(o, q, 4);
		lv_obj_set_pos(o, cx - s, cy - s * 87 / 100);
	} else {                                 /* a cross: one line through the middle and back out */
		q[0] = (lv_point_precise_t){ 0, 0 }; q[1] = (lv_point_precise_t){ 2 * s, 2 * s };
		q[2] = (lv_point_precise_t){ s, s }; q[3] = (lv_point_precise_t){ 2 * s, 0 }; q[4] = (lv_point_precise_t){ 0, 2 * s };
		lv_line_set_points(o, q, 5);
		lv_obj_set_pos(o, cx - s, cy - s);
	}
	lv_obj_set_style_line_width(o, LV_MAX(1, S(2)), 0);
	lv_obj_set_style_line_color(o, PAL.text, 0);
	lv_obj_set_style_line_rounded(o, true, 0);
	return o;
}

static lv_obj_t *label(padview_t *p, const char *text, int cx, int cy, int small)
{
	const char *t = !strcmp(text, "\xE2\x88\x92") ? "-" : !strcmp(text, "\xE2\x8C\x82") ? "Home" : text;
	lv_obj_t *l = mk_label(p->box, small || p->w < S(600) ? FONT_SMALL : FONT_BODY, PAL.text, t);
	lv_obj_update_layout(l);
	lv_obj_set_pos(l, cx - lv_obj_get_width(l) / 2, cy - lv_obj_get_height(l) / 2);
	return l;
}

padview_t *padview_create(lv_obj_t *parent, int x, int y, int w)
{
	padview_t *p = calloc(1, sizeof(*p));
	p->w = w;
	p->h = w * 345 / 600;
	p->box = mk_box(parent, p->w, p->h, PAL.bg, 0);
	lv_obj_set_style_bg_opa(p->box, LV_OPA_TRANSP, 0);
	lv_obj_set_pos(p->box, x, y);
	p->layout = -1;
	padview_layout(p, "generic");
	return p;
}

lv_obj_t *padview_obj(padview_t *p) { return p->box; }

void padview_delete(padview_t *p) { if (p) { lv_obj_delete(p->box); free(p); } }

void padview_layout(padview_t *p, const char *key)
{
	int li = padview_index(key);
	if (li < 0) li = 0;
	if (li == p->layout) return;
	p->layout = li;
	lv_obj_clean(p->box);
	memset(p->el, 0, sizeof(p->el));
	const pl_layout_t *L = &PLAYOUTS[li];
	p->n = 0;
	for (int i = 0; i < L->n && p->n < MAXEL; i++) {
		const pl_item_t *it = &L->it[i];
		el_t *e = &p->el[p->n++];
		e->t = it->t;
		e->id = it->id;
		int x = sc(p, it->x), y = sc(p, it->y), a = sc(p, it->a), b = sc(p, it->b);
		int small = strlen(it->lab) > 2;
		switch (it->t) {
		case 'c':
			e->shape = shape(p, x - a, y - a, 2 * a, 2 * a, LV_RADIUS_CIRCLE);
			if ((e->sym = ps_symbol(p, it->lab, x, y, a))) break;
			if (it->lab[0]) e->lab = label(p, it->lab, x, it->a < 14 ? y + a + sc(p, 12) : y, small || it->a < 14);
			break;
		case 'r':
			e->shape = shape(p, x, y, a, b, LV_MIN(b / 2, sc(p, 9)));
			if (it->lab[0]) e->lab = label(p, it->lab, x + a / 2, y + b / 2, small || it->b < 18);
			break;
		case 't':
			e->shape = shape(p, x, y, a, b, sc(p, 8));
			e->full = a - sc(p, 6);
			e->fill = mk_box(p->box, 0, b - sc(p, 6), PAL.acc, sc(p, 5));
			lv_obj_set_pos(e->fill, x + sc(p, 3), y + sc(p, 3));
			if (it->lab[0]) e->lab = label(p, it->lab, x + a / 2, y + b / 2, small);
			break;
		case 's': case 'l': {
			int knob = it->t == 's' ? a * 42 / 100 : a * 38 / 100;
			e->shape = shape(p, x - a, y - a, 2 * a, 2 * a, LV_RADIUS_CIRCLE);
			e->knob = mk_box(p->box, 2 * knob, 2 * knob, PAL.faint, LV_RADIUS_CIRCLE);
			e->kx = x; e->ky = y; e->kr = it->t == 's' ? a * 55 / 100 : a / 2;
			lv_obj_set_pos(e->knob, x - knob, y - knob);
			if (it->lab[0]) e->lab = label(p, it->lab, x, y, 1);
			if (it->t == 'l') e->id = "lever";
			break;
		}
		case 'd': {
			int w = a * 9 / 10, h = w / 2, ex = a + h;
			lv_obj_t *v = shape(p, x - h, y - ex, w, 2 * ex, sc(p, 4)), *hz = shape(p, x - ex, y - h, 2 * ex, w, sc(p, 4));
			lv_obj_t *mid = mk_box(p->box, w - 2 * LV_MAX(1, S(2)), w + 2 * LV_MAX(1, S(2)), PAL.panel, 0);
			lv_obj_set_pos(mid, x - h + LV_MAX(1, S(2)), y - h - LV_MAX(1, S(2)));
			(void)v; (void)hz;
			e->shape = v;
			e->id = "dpad";
			/* the four arms: up, down, left, right */
			int ax[4] = { x - h, x - h, x - ex, x + h }, ay[4] = { y - ex, y + h, y - h, y - h };
			int aw[4] = { w, w, ex - h, ex - h }, ah[4] = { ex - h, ex - h, w, w };
			for (int k = 0; k < 4; k++) {
				e->arm[k] = mk_box(p->box, aw[k] - 2 * S(3), ah[k] - 2 * S(3), PAL.acc, S(3));
				lv_obj_set_pos(e->arm[k], ax[k] + S(3), ay[k] + S(3));
				lv_obj_set_hidden(e->arm[k], true);
			}
			break;
		}
		}
	}
}

static int in(const char *const *list, int n, const char *id)
{
	for (int i = 0; i < n; i++) if (list[i] && !strcmp(list[i], id)) return 1;
	return 0;
}

/* any of the alternatives ("c|r1") in the list */
static int any(const char *const *list, int n, const char *ids)
{
	char b[32];
	const char *s = ids;
	while (*s) {
		size_t l = strcspn(s, "|");
		snprintf(b, sizeof(b), "%.*s", (int)l, s);
		if (in(list, n, b)) return 1;
		s += l + (s[l] == '|');
	}
	return 0;
}

void padview_state(padview_t *p, const padstate_t *s)
{
	const char *dirs[4] = { "up", "down", "left", "right" };
	for (int i = 0; i < p->n; i++) {
		el_t *e = &p->el[i];
		int has = 1, on = 0, tgt = 0, done = 0;
		if (!strcmp(e->id, "dpad")) {
			if (s->nhas >= 0) has = in(s->has, s->nhas, "up") || in(s->has, s->nhas, "left");
			/* each arm by itself: pressed (full), the one to press (half), mapped already (faint); never the
			   whole pad's outline */
			for (int k = 0; k < 4; k++) {
				int o = in(s->on, s->non, dirs[k]), t = s->target && !strcmp(s->target, dirs[k]), dn = in(s->done, s->ndone, dirs[k]);
				lv_obj_set_hidden(e->arm[k], !o && !t && !dn);
				lv_obj_set_style_bg_opa(e->arm[k], o ? LV_OPA_COVER : t ? LV_OPA_50 : LV_OPA_20, 0);
			}
		} else if (!strcmp(e->id, "lever")) {
			float x = (in(s->on, s->non, "right") ? 1.f : 0) - (in(s->on, s->non, "left") ? 1.f : 0);
			float y = (in(s->on, s->non, "down") ? 1.f : 0) - (in(s->on, s->non, "up") ? 1.f : 0);
			if (!x && !y) { x = s->lx; y = s->ly; }
			on = x * x + y * y > 0.09f;
			lv_obj_set_pos(e->knob, e->kx + (int)(x * e->kr) - lv_obj_get_width(e->knob) / 2, e->ky + (int)(y * e->kr) - lv_obj_get_height(e->knob) / 2);
			tgt = s->target && (!strcmp(s->target, "up") || !strcmp(s->target, "down") || !strcmp(s->target, "left") || !strcmp(s->target, "right"));
		} else if (e->t == 's') {
			int right = !strcmp(e->id, "rs");
			float x = right ? s->rx : s->lx, y = right ? s->ry : s->ly;
			if (s->nhas >= 0) has = in(s->has, s->nhas, right ? "rsx" : "lsx");
			on = any(s->on, s->non, right ? "r3" : "l3") || x * x + y * y > 0.09f;
			lv_obj_set_pos(e->knob, e->kx + (int)(x * e->kr) - lv_obj_get_width(e->knob) / 2, e->ky + (int)(y * e->kr) - lv_obj_get_height(e->knob) / 2);
			tgt = s->target && !strcmp(s->target, e->id);
		} else {
			/* N64's C buttons: the right stick's directions */
			if (!strncmp(e->id, "rs", 2) && strlen(e->id) == 3) {
				char d = e->id[2];
				if (s->nhas >= 0) has = in(s->has, s->nhas, "rsx");
				on = (d == 'u' && s->ry < -.5f) || (d == 'd' && s->ry > .5f) || (d == 'l' && s->rx < -.5f) || (d == 'r' && s->rx > .5f);
			} else {
				if (s->nhas >= 0) has = any(s->has, s->nhas, e->id);
				on = any(s->on, s->non, e->id);
			}
			tgt = s->target && any(&s->target, 1, e->id);
			done = any(s->done, s->ndone, e->id);
			if (e->fill) {
				float v = !strcmp(e->id, "l2") ? s->l2 : !strcmp(e->id, "r2") ? s->r2 : 0;
				if (on && v < 1) v = v > 0 ? v : 1;
				lv_obj_set_width(e->fill, (int)(e->full * (v < 0 ? 0 : v > 1 ? 1 : v)));
				on = 0;                          /* the fill shows it */
			}
		}
		lv_obj_t *o = e->shape;
		int knob_on = 0;
		if (e->knob) { knob_on = on; on = 0; }       /* a stick or lever: the knob lights, not the ring */
		lv_obj_set_style_opa(o, has ? LV_OPA_COVER : LV_OPA_40, 0);
		lv_obj_set_style_bg_color(o, on ? PAL.acc : tgt ? PAL.panel_on : PAL.panel, 0);
		lv_obj_set_style_border_color(o, on || tgt ? PAL.acc : done ? lv_color_mix(PAL.acc, PAL.line, 110) : PAL.line, 0);
		lv_obj_set_style_border_width(o, tgt ? LV_MAX(2, S(3)) : LV_MAX(1, S(2)), 0);
		lv_obj_set_style_shadow_width(o, tgt ? S(18) : 0, 0);
		lv_obj_set_style_shadow_color(o, PAL.acc, 0);
		lv_obj_set_style_shadow_opa(o, tgt ? LV_OPA_50 : LV_OPA_TRANSP, 0);
		if (e->knob) lv_obj_set_style_bg_color(e->knob, knob_on ? PAL.acc : PAL.faint, 0);
		lv_color_t tc = on ? PAL.bg : tgt ? PAL.acc : has ? PAL.text : PAL.faint;
		if (e->lab) lv_obj_set_style_text_color(e->lab, tc, 0);
		if (e->sym) { lv_obj_set_style_line_color(e->sym, tc, 0); lv_obj_set_style_border_color(e->sym, tc, 0); }
		if (e->lab) lv_obj_move_foreground(e->lab);
		if (e->sym) lv_obj_move_foreground(e->sym);
	}
}
