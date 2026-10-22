/* Phosphor frontend: screens and navigation.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Layout follows mockups/phosphor.html at 1280x720 and scales with the screen:
 *   status bar (wordmark, share/ethernet/WiFi/Bluetooth, clock)
 *   Home: category shelf + "pick up where you left off"
 *   Systems in a category -> games of a system (virtual list + details)
 *   Settings (Menu button)
 *   footer: what the buttons do here */
#include "ui.h"
#include "icons.h"
#include "library.h"
#include "log.h"
#include "meta.h"
#include "settings.h"
#include "sysinfo.h"
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

typedef enum { SCR_HOME, SCR_SYSTEMS, SCR_GAMES, SCR_SETTINGS } screen_t;

static int W, H;
static lv_obj_t *root, *body, *footer;
static screen_t cur = SCR_HOME;

/* ---------- status bar ---------- */

static lv_obj_t *sb_share, *sb_eth, *sb_wifi, *sb_bt, *sb_btn, *sb_clock;

static void status_bar(void)
{
	lv_obj_t *bar = mk_box(root, W, S(72), PAL.bg, 0);
	lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
	lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);

	lv_obj_t *word = mk_label(bar, FONT_WORD, PAL.acc, "PHOSPHOR");
	lv_obj_set_style_text_letter_space(word, S(3), 0);
	lv_obj_align(word, LV_ALIGN_LEFT_MID, S(56), 0);

	lv_obj_t *right = mk_box(bar, LV_SIZE_CONTENT, S(30), PAL.bg, 0);
	lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
	lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_column(right, S(22), 0);
	lv_obj_align(right, LV_ALIGN_RIGHT_MID, -S(56), 0);

	int is = S(22);
	sb_share = icon_create(right, ICON_SHARE, is);
	sb_eth = icon_create(right, ICON_ETHERNET, is);
	sb_wifi = icon_create(right, ICON_WIFI, is);
	lv_obj_t *btg = mk_box(right, LV_SIZE_CONTENT, is, PAL.bg, 0);
	lv_obj_set_style_bg_opa(btg, LV_OPA_TRANSP, 0);
	lv_obj_set_flex_flow(btg, LV_FLEX_FLOW_ROW);
	sb_bt = icon_create(btg, ICON_BLUETOOTH, is);
	sb_btn = mk_label(btg, FONT_LABEL, PAL.acc, "");   /* superscript count: top-aligned in the row */
	lv_obj_set_flex_align(btg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
	lv_obj_set_style_pad_column(btg, S(2), 0);
	sb_clock = mk_label(right, FONT_BODY, PAL.text, "");
	lv_obj_set_style_margin_left(sb_clock, S(6), 0);
}

static void status_update(void)
{
	sysinfo_t s;
	sysinfo_read(&s);
	lv_obj_set_style_text_color(sb_share, s.share ? PAL.text : PAL.icon_off, 0);
	lv_obj_set_style_text_color(sb_eth, s.ethernet ? PAL.text : PAL.icon_off, 0);
	if (s.wifi < 0) lv_obj_set_hidden(sb_wifi, true);
	else {
		lv_obj_set_hidden(sb_wifi, false);
		lv_obj_set_style_text_color(sb_wifi, s.wifi > 0 ? PAL.text : PAL.icon_off, 0);
		icon_set_level(sb_wifi, s.wifi);
	}
	lv_obj_t *btg = lv_obj_get_parent(sb_bt);
	if (s.bt < 0) lv_obj_set_hidden(btg, true);
	else {
		lv_obj_set_hidden(btg, false);
		lv_obj_set_style_text_color(sb_bt, s.bt > 0 ? PAL.text : PAL.icon_off, 0);
		char n[16] = "";
		if (s.bt > 0) snprintf(n, sizeof(n), "%d", s.bt);
		lv_label_set_text(sb_btn, n);
	}
	char clk[16];
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t, &tm);
	if (SETTINGS.clock24) strftime(clk, sizeof(clk), "%H:%M", &tm);
	else {
		strftime(clk, sizeof(clk), "%I:%M %p", &tm);
		if (clk[0] == '0') memmove(clk, clk + 1, strlen(clk));
	}
	const char *fake = getenv("PHOS_FAKE_CLOCK");
	lv_label_set_text(sb_clock, fake ? fake : clk);
}

/* ---------- footer: button hints ---------- */

static void footer_set(const char *const *hints)
{
	lv_obj_clean(footer);
	for (int i = 0; hints[i]; i += 2) {
		lv_obj_t *item = mk_box(footer, LV_SIZE_CONTENT, S(28), PAL.bg, 0);
		lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
		lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
		lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_style_pad_column(item, S(8), 0);
		lv_obj_t *btn = mk_box(item, S(24), S(24), PAL.bg, LV_RADIUS_CIRCLE);
		lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
		lv_obj_set_style_border_width(btn, LV_MAX(1, S(1)), 0);
		lv_obj_set_style_border_color(btn, PAL.dim, 0);
		lv_obj_t *g = mk_label(btn, FONT_LABEL, PAL.text, hints[i]);
		lv_obj_center(g);
		mk_label(item, FONT_SMALL, PAL.dim, hints[i + 1]);
	}
}

/* ---------- toast ---------- */

static lv_obj_t *toast;
static void toast_hide(lv_timer_t *t) { (void)t; lv_obj_set_hidden(toast, true); }

static void toast_show(const char *msg)
{
	lv_label_set_text(lv_obj_get_child(toast, 0), msg);
	lv_obj_set_hidden(toast, false);
	lv_obj_move_foreground(toast);
	lv_timer_t *t = lv_timer_create(toast_hide, 3500, NULL);
	lv_timer_set_repeat_count(t, 1);
	plog("toast: %s", msg);
}

/* ---------- virtual list (used for systems, games, settings) ---------- */

#define VL_MAX_ROWS 24
typedef struct {
	lv_obj_t *box, *rows[VL_MAX_ROWS], *lab[VL_MAX_ROWS], *val[VL_MAX_ROWS], *bar[VL_MAX_ROWS], *thumb;
	int vis, count, sel, top, row_h;
	void (*fill)(int i, char *text, size_t n, char *value, size_t vn, int *dim);
} vlist_t;

static void vlist_create(vlist_t *v, lv_obj_t *parent, int x, int y, int w, int h)
{
	v->row_h = S(46);
	v->vis = LV_MIN(h / v->row_h, VL_MAX_ROWS);
	v->box = mk_box(parent, w, v->vis * v->row_h, PAL.bg, 0);
	lv_obj_set_style_bg_opa(v->box, LV_OPA_TRANSP, 0);
	lv_obj_set_pos(v->box, x, y);
	for (int r = 0; r < v->vis; r++) {
		lv_obj_t *row = mk_box(v->box, w - S(10), v->row_h - S(4), PAL.panel, S(10));
		lv_obj_set_pos(row, 0, r * v->row_h);
		lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
		v->bar[r] = mk_box(row, S(3), v->row_h - S(18), PAL.acc, S(2));
		lv_obj_align(v->bar[r], LV_ALIGN_LEFT_MID, S(8), 0);
		v->lab[r] = mk_label(row, FONT_BODY, PAL.dim, "");
		lv_label_set_long_mode(v->lab[r], LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_width(v->lab[r], w - S(200));
		lv_obj_align(v->lab[r], LV_ALIGN_LEFT_MID, S(24), 0);
		v->val[r] = mk_label(row, FONT_SMALL, PAL.faint, "");
		lv_obj_align(v->val[r], LV_ALIGN_RIGHT_MID, -S(18), 0);
		v->rows[r] = row;
	}
	v->thumb = mk_box(v->box, S(3), S(40), PAL.line, S(2));
	lv_obj_set_x(v->thumb, w - S(4));
}

static void vlist_refresh(vlist_t *v)
{
	if (v->sel >= v->count) v->sel = v->count ? v->count - 1 : 0;
	if (v->sel < v->top) v->top = v->sel;
	if (v->sel >= v->top + v->vis) v->top = v->sel - v->vis + 1;
	for (int r = 0; r < v->vis; r++) {
		int i = v->top + r;
		if (i >= v->count) { lv_obj_set_hidden(v->rows[r], true); continue; }
		lv_obj_set_hidden(v->rows[r], false);
		char t[256], val[64] = "";
		int dim = 0;
		v->fill(i, t, sizeof(t), val, sizeof(val), &dim);
		int on = i == v->sel;
		lv_label_set_text(v->lab[r], t);
		lv_label_set_text(v->val[r], val);
		lv_obj_set_style_bg_opa(v->rows[r], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
		lv_obj_set_style_bg_color(v->rows[r], PAL.panel_on, 0);
		lv_obj_set_style_text_color(v->lab[r], on ? PAL.text : dim ? PAL.faint : PAL.dim, 0);
		lv_obj_set_style_text_color(v->val[r], on ? PAL.acc : PAL.faint, 0);
		if (on) lv_obj_set_hidden(v->bar[r], false);
		else lv_obj_set_hidden(v->bar[r], true);
	}
	/* scroll position */
	if (v->count > v->vis) {
		int track = v->vis * v->row_h, th = LV_MAX(S(24), track * v->vis / v->count);
		lv_obj_set_height(v->thumb, th);
		lv_obj_set_y(v->thumb, (track - th) * v->top / (v->count - v->vis));
		lv_obj_set_hidden(v->thumb, false);
	} else {
		lv_obj_set_hidden(v->thumb, true);
	}
}

static int vlist_key(vlist_t *v, pkey_t k)
{
	int old = v->sel;
	if (k == K_UP && v->sel > 0) v->sel--;
	else if (k == K_DOWN && v->sel + 1 < v->count) v->sel++;
	else if (k == K_L) v->sel = LV_MAX(0, v->sel - v->vis);
	else if (k == K_R) v->sel = LV_MIN(v->count - 1, v->sel + v->vis);
	else return 0;
	if (v->sel != old) vlist_refresh(v);
	return 1;
}

/* ---------- Home: category shelf ---------- */

static const icon_t CAT_ICON[CAT_COUNT] = { ICON_DISC, ICON_CARTRIDGE, ICON_HANDHELD, ICON_ARCADE, ICON_COMPUTER };

static struct {
	lv_obj_t *card[CAT_COUNT], *ic[CAT_COUNT], *nm[CAT_COUNT], *ct[CAT_COUNT];
	int sel;
	float pos;          /* animated selection position */
	lv_obj_t *cont, *cont_title, *cont_sub;
} home;

static void shelf_layout(float pos)
{
	home.pos = pos;
	int cx = W / 2, step = S(236);
	for (int i = 0; i < CAT_COUNT; i++) {
		float d = i - pos, ad = d < 0 ? -d : d;
		float grow = ad < 1 ? 1 - ad : 0;                  /* 1 at the focused card */
		int cw = S(200) + (int)(S(36) * grow), ch = S(250) + (int)(S(45) * grow);
		lv_obj_set_size(home.card[i], cw, ch);
		lv_obj_set_pos(home.card[i], cx + (int)(d * step) - cw / 2, S(140) + (S(250) - ch) / 2);
		int opa = 255 - (int)(LV_MIN(ad, 2.5f) * 60);
		lv_obj_set_style_opa(home.card[i], (lv_opa_t)LV_MAX(opa, 90), 0);
		int on = ad < 0.5f;
		lv_obj_set_style_border_color(home.card[i], on ? PAL.acc : PAL.line, 0);
		lv_obj_set_style_bg_color(home.card[i], on ? PAL.panel_on : PAL.panel, 0);
		lv_obj_set_style_text_color(home.ic[i], on ? PAL.acc : PAL.dim, 0);
		lv_obj_set_style_text_color(home.nm[i], on ? PAL.text : PAL.dim, 0);
	}
}

static void shelf_anim_cb(void *var, int32_t v) { (void)var; shelf_layout(v / 1000.0f); }

static void shelf_go(int sel)
{
	if (sel < 0 || sel >= CAT_COUNT) return;
	lv_anim_delete(&home, shelf_anim_cb);
	lv_anim_t a;
	lv_anim_init(&a);
	lv_anim_set_var(&a, &home);
	lv_anim_set_exec_cb(&a, shelf_anim_cb);
	lv_anim_set_values(&a, (int32_t)(home.pos * 1000), sel * 1000);
	lv_anim_set_duration(&a, 170);
	lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
	lv_anim_start(&a);
	home.sel = sel;
}

static void home_counts(void)
{
	for (int c = 0; c < CAT_COUNT; c++) {
		int n = library_category_count((category_t)c);
		char t[48];
		if (n < 0) snprintf(t, sizeof(t), "Looking...");
		else snprintf(t, sizeof(t), "%d game%s", n, n == 1 ? "" : "s");
		lv_label_set_text(home.ct[c], t);
	}
}

static void home_continue(void)
{
	if (SETTINGS.last_title[0]) {
		lv_label_set_text(home.cont_title, SETTINGS.last_title);
		char sub[128];
		snprintf(sub, sizeof(sub), "%s", SETTINGS.last_system_name);
		lv_label_set_text(home.cont_sub, sub);
	} else {
		lv_label_set_text(home.cont_title, "Nothing yet");
		lv_label_set_text(home.cont_sub, "The last game you play will be here");
	}
}

static void show_home(void)
{
	cur = SCR_HOME;
	lv_obj_clean(body);
	lv_obj_t *sec = mk_label(body, FONT_LABEL, PAL.dim, "LIBRARY");
	lv_obj_set_style_text_letter_space(sec, S(3), 0);
	lv_obj_set_pos(sec, S(56), S(104));

	for (int i = 0; i < CAT_COUNT; i++) {
		lv_obj_t *c = mk_box(body, S(200), S(250), PAL.panel, S(18));
		lv_obj_set_style_border_width(c, LV_MAX(1, S(1)), 0);
		lv_obj_set_style_border_color(c, PAL.line, 0);
		lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_style_pad_row(c, S(14), 0);
		home.card[i] = c;
		home.ic[i] = icon_create(c, CAT_ICON[i], S(72));
		home.nm[i] = mk_label(c, FONT_NAME, PAL.dim, category_name((category_t)i));
		home.ct[i] = mk_label(c, FONT_SMALL, PAL.faint, "");
	}
	shelf_layout((float)home.sel);
	home_counts();

	/* pick up where you left off */
	lv_obj_t *cont = mk_box(body, W - S(112), S(120), PAL.panel, S(16));
	lv_obj_set_style_border_width(cont, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(cont, PAL.line, 0);
	lv_obj_set_pos(cont, S(56), S(470));
	lv_obj_t *art = mk_box(cont, S(72), S(88), lv_color_hex(0x2a2119), S(8));
	lv_obj_set_style_border_width(art, 1, 0);
	lv_obj_set_style_border_color(art, lv_color_hex(0x3a3530), 0);
	lv_obj_align(art, LV_ALIGN_LEFT_MID, S(28), 0);
	icon_t last_icon = ICON_DISC;
	for (int i = 0; i < NSYSTEMS; i++)
		if (!strcmp(SYSTEMS[i].id, SETTINGS.last_system)) last_icon = CAT_ICON[SYSTEMS[i].cat];
	lv_obj_t *aic = icon_create(art, last_icon, S(34));
	lv_obj_set_style_text_color(aic, PAL.acc, 0);
	lv_obj_center(aic);
	lv_obj_t *l1 = mk_label(cont, FONT_LABEL, PAL.dim, "PICK UP WHERE YOU LEFT OFF");
	lv_obj_set_style_text_letter_space(l1, S(3), 0);
	lv_obj_set_pos(l1, S(124), S(28));
	home.cont_title = mk_label(cont, FONT_TITLE, PAL.text, "");
	lv_obj_set_pos(home.cont_title, S(124), S(48));
	home.cont_sub = mk_label(cont, FONT_SMALL, PAL.dim, "");
	lv_obj_set_pos(home.cont_sub, S(124), S(80));
	home.cont = cont;
	if (SETTINGS.last_title[0]) {
		lv_obj_t *pill = mk_box(cont, LV_SIZE_CONTENT, S(36), PAL.panel, LV_RADIUS_CIRCLE);
		lv_obj_set_style_border_width(pill, LV_MAX(1, S(1)), 0);
		lv_obj_set_style_border_color(pill, PAL.acc, 0);
		lv_obj_set_style_pad_hor(pill, S(18), 0);
		lv_obj_t *pl = mk_label(pill, FONT_SMALL, PAL.acc, "Play");
		lv_obj_center(pl);
		lv_obj_align(pill, LV_ALIGN_RIGHT_MID, -S(28), 0);
	}
	home_continue();

	static const char *const hints_last[] = { "A", "Open", "X", "Play last", "\xE2\x89\xA1", "Settings", NULL };
	static const char *const hints[] = { "A", "Open", "\xE2\x89\xA1", "Settings", NULL };
	footer_set(SETTINGS.last_title[0] ? hints_last : hints);
}

/* ---------- Systems in a category ---------- */

static int sys_idx[64], nsys;
static category_t sys_cat;
static vlist_t sys_list;
static lv_obj_t *sys_detail_name, *sys_detail_count, *sys_detail_core;

static void sys_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	game_system_t *s = &SYSTEMS[sys_idx[i]];
	snprintf(t, n, "%s", s->name);
	if (s->count < 0) snprintf(val, vn, "...");
	else snprintf(val, vn, "%d", s->count);
	*dim = s->count == 0;
}

static void sys_detail(void)
{
	if (!nsys) {
		lv_label_set_text(sys_detail_name, "No games yet");
		lv_label_set_text(sys_detail_count, "Put games in games/<system> on the SD card, a USB drive or your network share");
		lv_label_set_text(sys_detail_core, "");
		return;
	}
	game_system_t *s = &SYSTEMS[sys_idx[sys_list.sel]];
	lv_label_set_text(sys_detail_name, s->name);
	char t[64];
	if (s->count < 0) snprintf(t, sizeof(t), "Looking for games...");
	else snprintf(t, sizeof(t), "%d game%s", s->count, s->count == 1 ? "" : "s");
	lv_label_set_text(sys_detail_count, t);
	lv_label_set_text(sys_detail_core, s->core_found ? "" : "Core not installed");
}

static void screen_title(const char *label, const char *title)
{
	lv_obj_t *sec = mk_label(body, FONT_LABEL, PAL.dim, label);
	lv_obj_set_style_text_letter_space(sec, S(3), 0);
	lv_obj_set_pos(sec, S(56), S(96));
	lv_obj_t *tl = mk_label(body, FONT_HEADING, PAL.text, title);
	lv_obj_set_pos(tl, S(56), S(118));
}

static void show_systems(category_t c)
{
	cur = SCR_SYSTEMS;
	sys_cat = c;
	lv_obj_clean(body);
	screen_title("LIBRARY", category_name(c));
	nsys = library_category_systems(c, sys_idx, 64);
	vlist_create(&sys_list, body, S(56), S(176), S(640), S(440));
	sys_list.count = nsys;
	sys_list.fill = sys_fill;
	sys_list.sel = 0;
	sys_list.top = 0;

	lv_obj_t *panel = mk_box(body, S(470), S(300), PAL.panel, S(16));
	lv_obj_set_style_border_width(panel, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(panel, PAL.line, 0);
	lv_obj_set_pos(panel, W - S(56) - S(470), S(176));
	lv_obj_t *ic = icon_create(panel, CAT_ICON[c], S(64));
	lv_obj_set_style_text_color(ic, PAL.acc, 0);
	lv_obj_set_pos(ic, S(28), S(28));
	sys_detail_name = mk_label(panel, FONT_TITLE, PAL.text, "");
	lv_obj_set_pos(sys_detail_name, S(28), S(116));
	sys_detail_count = mk_label(panel, FONT_BODY, PAL.dim, "");
	lv_label_set_long_mode(sys_detail_count, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(sys_detail_count, S(414));
	lv_obj_set_pos(sys_detail_count, S(28), S(152));
	sys_detail_core = mk_label(panel, FONT_SMALL, PAL.acc, "");
	lv_obj_set_pos(sys_detail_core, S(28), S(184));

	vlist_refresh(&sys_list);
	sys_detail();
	static const char *const hints[] = { "A", "Open", "B", "Back", NULL };
	footer_set(hints);
}

/* ---------- Games of a system ---------- */

static game_system_t *games_sys;
static game_t *games;
static int ngames;
static vlist_t game_list;
static lv_obj_t *g_art, *g_noart, *g_title, *g_info, *g_desc, *g_where;
static lv_timer_t *art_timer;
static char art_src[1100];

static void game_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	(void)val; (void)vn; (void)dim;
	snprintf(t, n, "%s", games[i].title);
}

static const char *where_label(const char *path)
{
	if (!strncmp(path, "/media/usb", 10)) return "USB / NVMe drive";
	if (strstr(path, "/cifs/")) return "Network share";
	return "SD card";
}

/* box art loads a moment after the selection settles, so scrolling through a list stays quick */
static void art_load(lv_timer_t *t)
{
	(void)t;
	art_timer = NULL;
	if (cur != SCR_GAMES || !ngames) return;
	const meta_t *m = meta_for(games[game_list.sel].path);
	if (m->image) {
		snprintf(art_src, sizeof(art_src), "A:%s", m->image);   /* LVGL's file driver letter */
		lv_image_set_src(g_art, art_src);
		lv_obj_set_hidden(g_art, false);
		lv_obj_set_hidden(g_noart, true);
	} else {
		lv_obj_set_hidden(g_art, true);
		lv_obj_set_hidden(g_noart, false);
	}
}

static void game_detail(void)
{
	if (art_timer) { lv_timer_delete(art_timer); art_timer = NULL; }
	if (!ngames) {
		lv_label_set_text(g_title, "No games found");
		lv_label_set_text(g_info, "");
		lv_label_set_text(g_desc, "Put them in games/ on the SD card, a USB drive or your network share.");
		lv_label_set_text(g_where, "");
		lv_obj_set_hidden(g_art, true);
		lv_obj_set_hidden(g_noart, false);
		return;
	}
	game_t *g = &games[game_list.sel];
	const meta_t *m = meta_for(g->path);
	lv_label_set_text(g_title, m->name ? m->name : g->title);

	/* 1997 · Publisher · 1-2 players · Genre */
	char info[256] = "";
	const char *parts[] = { m->date, m->publisher ? m->publisher : m->developer, NULL, m->genre };
	char players[32] = "";
	if (m->players) { snprintf(players, sizeof(players), "%s player%s", m->players, strcmp(m->players, "1") ? "s" : ""); parts[2] = players; }
	for (int i = 0; i < 4; i++) {
		if (!parts[i] || !*parts[i]) continue;
		if (info[0]) strncat(info, "  \xC2\xB7  ", sizeof(info) - strlen(info) - 1);
		strncat(info, parts[i], sizeof(info) - strlen(info) - 1);
	}
	lv_label_set_text(g_info, info);
	lv_label_set_text(g_desc, m->desc ? m->desc : "");

	const char *base = strrchr(g->path, '/');
	char where[600];
	snprintf(where, sizeof(where), "%s  \xC2\xB7  %s", where_label(g->path), base ? base + 1 : g->path);
	lv_label_set_text(g_where, where);

	/* keep the old art up until the new one is ready, unless this game has none */
	if (!m->image) { lv_obj_set_hidden(g_art, true); lv_obj_set_hidden(g_noart, false); }
	art_timer = lv_timer_create(art_load, 120, NULL);
	lv_timer_set_repeat_count(art_timer, 1);
}

static void show_games(game_system_t *s)
{
	cur = SCR_GAMES;
	games_sys = s;
	if (art_timer) { lv_timer_delete(art_timer); art_timer = NULL; }
	if (games) library_free_games(games, ngames);
	games = library_games(s, &ngames);
	char dirs[16][512];
	int nd = library_system_dirs(s, dirs, 16);
	meta_load(s->id, dirs, nd);
	lv_obj_clean(body);
	char label[96];
	spaced_caps(label, sizeof(label), category_name(s->cat));
	screen_title(label, s->name);
	vlist_create(&game_list, body, S(56), S(176), S(560), S(440));
	game_list.count = ngames;
	game_list.fill = game_fill;
	game_list.sel = 0;
	game_list.top = 0;

	/* details: box art on top, then title, details and description */
	int pw = S(584), ph = S(456), pad = S(26), iw = pw - 2 * pad;
	lv_obj_t *panel = mk_box(body, pw, ph, PAL.panel, S(16));
	lv_obj_set_style_border_width(panel, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(panel, PAL.line, 0);
	lv_obj_set_pos(panel, W - S(56) - pw, S(170));

	int ah = S(196);
	g_art = lv_image_create(panel);
	lv_obj_set_size(g_art, iw, ah);
	lv_image_set_inner_align(g_art, LV_IMAGE_ALIGN_CONTAIN);
	lv_obj_set_pos(g_art, pad, pad);
	lv_obj_set_hidden(g_art, true);
	g_noart = mk_box(panel, iw, ah, lv_color_mix(PAL.panel, PAL.bg, 128), S(10));
	lv_obj_set_pos(g_noart, pad, pad);
	lv_obj_t *nic = icon_create(g_noart, CAT_ICON[s->cat], S(64));
	lv_obj_set_style_text_color(nic, PAL.line, 0);
	lv_obj_center(nic);

	int y = pad + ah + S(16);
	g_title = mk_label(panel, FONT_TITLE, PAL.text, "");
	lv_label_set_long_mode(g_title, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(g_title, iw, S(30));
	lv_obj_set_pos(g_title, pad, y);
	y += S(32);
	g_info = mk_label(panel, FONT_SMALL, PAL.acc, "");
	lv_label_set_long_mode(g_info, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(g_info, iw, S(20));
	lv_obj_set_pos(g_info, pad, y);
	y += S(28);
	int bottom = S(34);
	g_desc = mk_label(panel, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(g_desc, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_style_text_line_space(g_desc, S(3), 0);
	lv_obj_set_size(g_desc, iw, ph - y - bottom - S(8));
	lv_obj_set_pos(g_desc, pad, y);
	g_where = mk_label(panel, FONT_SMALL, PAL.faint, "");
	lv_label_set_long_mode(g_where, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_width(g_where, iw);
	lv_obj_align(g_where, LV_ALIGN_BOTTOM_LEFT, pad, -S(14));

	char cnt[48];
	snprintf(cnt, sizeof(cnt), "%d game%s", ngames, ngames == 1 ? "" : "s");
	lv_obj_t *c = mk_label(body, FONT_SMALL, PAL.dim, cnt);
	lv_obj_align(c, LV_ALIGN_TOP_LEFT, S(56) + S(560) - S(10) - lv_obj_get_width(c), S(134));
	lv_obj_update_layout(c);
	lv_obj_set_x(c, S(56) + S(560) - S(10) - lv_obj_get_width(c));

	vlist_refresh(&game_list);
	game_detail();
	static const char *const hints[] = { "A", "Play", "B", "Back", "L", "Page up", "R", "Page down", NULL };
	footer_set(hints);
}

static void play(game_system_t *s, game_t *g)
{
	char err[256];
	if (library_launch(s, g, err, sizeof(err)) != 0) {
		toast_show(err);
		return;
	}
	settings_set_last(s, g);
	toast_show("Starting...");
}

/* ---------- Settings ---------- */

static vlist_t set_list;
enum { SET_ACCENT, SET_CLOCK, SET_RESCAN, SET_ABOUT, SET_COUNT };

static void set_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	(void)dim;
	switch (i) {
	case SET_ACCENT: snprintf(t, n, "Accent colour"); snprintf(val, vn, "<  %s  >", theme_accent_name(theme_accent())); break;
	case SET_CLOCK: snprintf(t, n, "Clock"); snprintf(val, vn, "<  %s  >", SETTINGS.clock24 ? "24-hour" : "12-hour"); break;
	case SET_RESCAN: snprintf(t, n, "Look for games again"); break;
	case SET_ABOUT: snprintf(t, n, "Phosphor"); snprintf(val, vn, "%s", PHOS_VERSION); break;
	}
}

static void show_settings(void);

static void settings_change(int dir)
{
	if (set_list.sel == SET_ACCENT) {
		int a = ((int)theme_accent() + dir + ACCENT_COUNT) % ACCENT_COUNT;
		theme_set_accent((accent_t)a);
		SETTINGS.accent = a;
		settings_save();
		/* everything picks up the new accent */
		int keep = set_list.sel;
		lv_obj_clean(root);
		ui_init(W, H);
		show_settings();
		set_list.sel = keep;
		vlist_refresh(&set_list);
	} else if (set_list.sel == SET_CLOCK) {
		SETTINGS.clock24 = !SETTINGS.clock24;
		settings_save();
		status_update();
		vlist_refresh(&set_list);
	}
}

static void show_settings(void)
{
	cur = SCR_SETTINGS;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Settings");
	vlist_create(&set_list, body, S(56), S(176), S(760), S(440));
	set_list.count = SET_COUNT;
	set_list.fill = set_fill;
	set_list.sel = 0;
	set_list.top = 0;
	vlist_refresh(&set_list);
	static const char *const hints[] = { "A", "Select", "\xE2\x86\x94", "Change", "B", "Back", NULL };
	footer_set(hints);
}

/* ---------- navigation ---------- */

void ui_rescan(void)
{
	for (int i = 0; i < NSYSTEMS; i++) SYSTEMS[i].count = -1;
	library_scan_start();
}

void ui_key(pkey_t k)
{
	switch (cur) {
	case SCR_HOME:
		if (k == K_LEFT) shelf_go(home.sel - 1);
		else if (k == K_RIGHT) shelf_go(home.sel + 1);
		else if (k == K_A) show_systems((category_t)home.sel);
		else if (k == K_MENU) show_settings();
		else if (k == K_X && SETTINGS.last_title[0]) {
			game_system_t *s = NULL;
			for (int i = 0; i < NSYSTEMS; i++) if (!strcmp(SYSTEMS[i].id, SETTINGS.last_system)) s = &SYSTEMS[i];
			game_t g = { SETTINGS.last_path, SETTINGS.last_title };
			if (s) play(s, &g);
		}
		break;
	case SCR_SYSTEMS:
		if (vlist_key(&sys_list, k)) sys_detail();
		else if (k == K_A && nsys) show_games(&SYSTEMS[sys_idx[sys_list.sel]]);
		else if (k == K_B) show_home();
		break;
	case SCR_GAMES:
		if (vlist_key(&game_list, k)) game_detail();
		else if (k == K_A && ngames) play(games_sys, &games[game_list.sel]);
		else if (k == K_B) { int keep = 0; for (int i = 0; i < nsys; i++) if (&SYSTEMS[sys_idx[i]] == games_sys) keep = i;
			show_systems(sys_cat); sys_list.sel = keep; vlist_refresh(&sys_list); sys_detail(); }
		break;
	case SCR_SETTINGS:
		if (vlist_key(&set_list, k)) break;
		if (k == K_LEFT) settings_change(-1);
		else if (k == K_RIGHT || k == K_A) {
			if (set_list.sel == SET_RESCAN && k == K_A) { ui_rescan(); toast_show("Looking for games..."); }
			else settings_change(1);
		}
		else if (k == K_B || k == K_MENU) show_home();
		break;
	}
}

static int last_gen = -1;
static uint32_t last_status;

void ui_tick(void)
{
	uint32_t now = lv_tick_get();
	if (now - last_status > 2000 || last_status == 0) { status_update(); last_status = now ? now : 1; }
	int g = library_scan_generation();
	if (g != last_gen) {
		last_gen = g;
		if (cur == SCR_HOME) home_counts();
		else if (cur == SCR_SYSTEMS) {
			nsys = library_category_systems(sys_cat, sys_idx, 64);
			sys_list.count = nsys;
			vlist_refresh(&sys_list);
			sys_detail();
		}
	}
	if (mounts_changed()) {
		plog("mounts changed: looking for games again");
		ui_rescan();
	}
}

void ui_init(int w, int h)
{
	W = w;
	H = h;
	theme_set_scale(w, h);
	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, PAL.bg, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	if (!root) {
		root = mk_box(scr, w, h, PAL.bg, 0);
	}
	lv_obj_set_style_bg_color(root, PAL.bg, 0);
	status_bar();
	body = mk_box(root, w, h, PAL.bg, 0);
	lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
	lv_obj_move_background(body);
	footer = mk_box(root, w - S(112), S(64), PAL.bg, 0);
	lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
	lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_column(footer, S(28), 0);
	lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, S(56), 0);

	toast = mk_box(root, LV_SIZE_CONTENT, S(44), lv_color_hex(0x1f1f25), S(12));
	lv_obj_set_style_border_width(toast, 1, 0);
	lv_obj_set_style_border_color(toast, PAL.acc, 0);
	lv_obj_set_style_pad_hor(toast, S(20), 0);
	lv_obj_t *tl = mk_label(toast, FONT_SMALL, PAL.text, "");
	lv_obj_center(tl);
	lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -S(80));
	lv_obj_set_hidden(toast, true);

	status_update();
	show_home();
}
