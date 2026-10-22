/* Phosphor frontend: screens and navigation.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Layout follows mockups/phosphor.html at 1280x720 and scales with the screen:
 *   status bar (wordmark, share/ethernet/WiFi/Bluetooth, clock)
 *   Home: category shelf + "pick up where you left off"
 *   Systems in a category -> games of a system (virtual list + details)
 *     Y: "Based on" picker, for a hack/mod/translation to use its original's art and description
 *   Settings (Menu button)
 *   footer: what the buttons do here */
#include "ui.h"
#include "icons.h"
#include "api.h"
#include "log.h"
#include "settings.h"
#include "sysinfo.h"
#include "texture.h"
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { SCR_HOME, SCR_SYSTEMS, SCR_GAMES, SCR_SETTINGS, SCR_SOURCES, SCR_DIAG, SCR_WIFI, SCR_VIDEO, SCR_VIDEO_SYS, SCR_PICK, SCR_REMOTE, SCR_CLOUD, SCR_WELCOME, SCR_BACKUP, SCR_PORTS, SCR_BT } screen_t;

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
		/* one character (or symbol): a circle; a word like SELECT: a pill */
		int chars = 0;
		for (const unsigned char *c = (const unsigned char *)hints[i]; *c; c++) if ((*c & 0xC0) != 0x80) chars++;
		lv_obj_t *btn = mk_box(item, chars > 1 ? LV_SIZE_CONTENT : S(24), S(24), PAL.bg, LV_RADIUS_CIRCLE);
		if (chars > 1) lv_obj_set_style_pad_hor(btn, S(8), 0);
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
		lv_obj_set_size(v->lab[r], w - S(200), S(26));     /* one line: long text ends in ... */
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
/* the shelf: the categories, then Favorites and Recently played */
enum { SH_FAV = CAT_COUNT, SH_RECENT, SHELF_COUNT };
static icon_t shelf_icon(int i) { return i == SH_FAV ? ICON_STAR : i == SH_RECENT ? ICON_CLOCK : CAT_ICON[i]; }
static const char *shelf_name(int i) { return i == SH_FAV ? "Favorites" : i == SH_RECENT ? "Recently played" : category_name((category_t)i); }

static struct {
	lv_obj_t *card[SHELF_COUNT], *ic[SHELF_COUNT], *nm[SHELF_COUNT], *ct[SHELF_COUNT];
	int sel;
	float pos;          /* animated selection position */
	lv_obj_t *cont, *cont_title, *cont_sub;
} home;

static void shelf_layout(float pos)
{
	home.pos = pos;
	int cx = W / 2, step = S(236);
	for (int i = 0; i < SHELF_COUNT; i++) {
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
	if (sel < 0 || sel >= SHELF_COUNT) return;
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

/* ---------- data from phosphord ---------- */

static int cat_count[SHELF_COUNT];
static cJSON *last_game;          /* {"id","title","system_name","category",...} or NULL */

static void load_home_data(void)
{
	for (int c = 0; c < SHELF_COUNT; c++) cat_count[c] = -1;
	cJSON *r = api_call("{\"cmd\":\"games\",\"filter\":\"favorites\",\"limit\":0}");
	if (api_ok(r)) cat_count[SH_FAV] = (int)api_num(r, "total", 0);
	cJSON_Delete(r);
	r = api_call("{\"cmd\":\"games\",\"filter\":\"recent\",\"limit\":0}");
	if (api_ok(r)) cat_count[SH_RECENT] = (int)api_num(r, "total", 0);
	cJSON_Delete(r);
	r = api_call("{\"cmd\":\"categories\"}");
	if (api_ok(r)) {
		cJSON *it;
		cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "categories")) {
			int id = (int)api_num(it, "id", -1);
			if (id >= 0 && id < CAT_COUNT) cat_count[id] = (int)api_num(it, "games", 0);
		}
	}
	cJSON_Delete(r);
	cJSON_Delete(last_game);
	last_game = NULL;
	r = api_call("{\"cmd\":\"last\"}");
	if (api_ok(r) && cJSON_IsObject(cJSON_GetObjectItem(r, "game")))
		last_game = cJSON_DetachItemFromObject(r, "game");
	cJSON_Delete(r);
}

static void play_id(long long id)
{
	cJSON *r = api_call("{\"cmd\":\"launch\",\"id\":%lld}", id);
	if (!r) toast_show("The system service isn't responding");
	else if (!api_ok(r)) toast_show(api_str(r, "error") ? api_str(r, "error") : "Couldn't start the game");
	else toast_show("Starting...");
	cJSON_Delete(r);
}

static void home_counts(void)
{
	for (int c = 0; c < SHELF_COUNT; c++) {
		int n = cat_count[c];
		char t[48];
		if (n < 0) snprintf(t, sizeof(t), "Looking...");
		else snprintf(t, sizeof(t), "%d game%s", n, n == 1 ? "" : "s");
		lv_label_set_text(home.ct[c], t);
	}
}

static void home_continue(void)
{
	if (last_game) {
		lv_label_set_text(home.cont_title, api_str(last_game, "title") ? api_str(last_game, "title") : "");
		char sub[160];
		long secs = (long)api_num(last_game, "play_seconds", 0);
		if (secs >= 60) snprintf(sub, sizeof(sub), "%s  \xC2\xB7  %ld h %ld m played", api_str(last_game, "system_name"), secs / 3600, secs / 60 % 60);
		else snprintf(sub, sizeof(sub), "%s", api_str(last_game, "system_name"));
		lv_label_set_text(home.cont_sub, sub);
	} else {
		lv_label_set_text(home.cont_title, "Nothing yet");
		lv_label_set_text(home.cont_sub, "The last game you play will be here");
	}
}

static void show_home(void)
{
	cur = SCR_HOME;
	load_home_data();
	lv_obj_clean(body);
	lv_obj_t *sec = mk_label(body, FONT_LABEL, PAL.dim, "LIBRARY");
	lv_obj_set_style_text_letter_space(sec, S(3), 0);
	lv_obj_set_pos(sec, S(56), S(104));

	for (int i = 0; i < SHELF_COUNT; i++) {
		lv_obj_t *c = mk_box(body, S(200), S(250), PAL.panel, S(18));
		lv_obj_set_style_border_width(c, LV_MAX(1, S(1)), 0);
		lv_obj_set_style_border_color(c, PAL.line, 0);
		lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_style_pad_row(c, S(14), 0);
		home.card[i] = c;
		home.ic[i] = icon_create(c, shelf_icon(i), S(72));
		home.nm[i] = mk_label(c, FONT_NAME, PAL.dim, shelf_name(i));
		home.ct[i] = mk_label(c, FONT_SMALL, PAL.faint, "");
	}
	shelf_layout((float)home.sel);
	home_counts();

	/* pick up where you left off */
	lv_obj_t *cont = mk_box(body, W - S(112), S(120), PAL.panel, S(16));
	lv_obj_set_style_border_width(cont, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(cont, PAL.line, 0);
	lv_obj_set_pos(cont, S(56), S(470));
	int aw = S(72), ah = S(88);
	lv_obj_t *art = mk_box(cont, aw, ah, lv_color_hex(0x2a2119), S(8));
	lv_obj_set_style_border_width(art, 1, 0);
	lv_obj_set_style_border_color(art, lv_color_hex(0x3a3530), 0);
	lv_obj_set_style_clip_corner(art, true, 0);
	lv_obj_align(art, LV_ALIGN_LEFT_MID, S(28), 0);
	const char *last_art = last_game ? api_str(last_game, "art") : NULL;
	if (last_art) {
		static char src[1100];
		snprintf(src, sizeof(src), "A:%s", last_art);
		lv_obj_t *img = lv_image_create(art);
		lv_obj_set_size(img, aw, ah);
		lv_image_set_inner_align(img, LV_IMAGE_ALIGN_COVER);
		lv_image_set_src(img, src);
		lv_obj_center(img);
	} else {
		int cat = last_game ? (int)api_num(last_game, "category", 0) : 0;
		lv_obj_t *aic = icon_create(art, CAT_ICON[cat >= 0 && cat < CAT_COUNT ? cat : 0], S(34));
		lv_obj_set_style_text_color(aic, PAL.acc, 0);
		lv_obj_center(aic);
	}
	lv_obj_t *l1 = mk_label(cont, FONT_LABEL, PAL.dim, "PICK UP WHERE YOU LEFT OFF");
	lv_obj_set_style_text_letter_space(l1, S(3), 0);
	lv_obj_set_pos(l1, S(124), S(28));
	home.cont_title = mk_label(cont, FONT_TITLE, PAL.text, "");
	lv_obj_set_pos(home.cont_title, S(124), S(48));
	home.cont_sub = mk_label(cont, FONT_SMALL, PAL.dim, "");
	lv_obj_set_pos(home.cont_sub, S(124), S(80));
	home.cont = cont;
	if (last_game) {
		lv_obj_t *pill = mk_box(cont, LV_SIZE_CONTENT, S(36), PAL.panel, LV_RADIUS_CIRCLE);
		lv_obj_set_style_border_width(pill, LV_MAX(1, S(1)), 0);
		lv_obj_set_style_border_color(pill, PAL.acc, 0);
		lv_obj_set_style_pad_hor(pill, S(18), 0);
		lv_obj_t *pl = mk_label(pill, FONT_SMALL, PAL.acc, "Play");
		lv_obj_center(pl);
		lv_obj_align(pill, LV_ALIGN_RIGHT_MID, -S(28), 0);
	}
	home_continue();

	static const char *const hints_last[] = { "A", "Open", "X", "Play last", "Y", "Search", "\xE2\x89\xA1", "Settings", NULL };
	static const char *const hints[] = { "A", "Open", "Y", "Search", "\xE2\x89\xA1", "Settings", NULL };
	footer_set(last_game ? hints_last : hints);
}

/* ---------- Systems in a category ---------- */

typedef struct { char id[24], name[48]; int games, core; } sys_item_t;
static sys_item_t sys_items[64];
static int nsys;
static category_t sys_cat;
static vlist_t sys_list;
static lv_obj_t *sys_detail_name, *sys_detail_count, *sys_detail_core;

static void load_systems(category_t c)
{
	nsys = 0;
	cJSON *r = api_call("{\"cmd\":\"systems\",\"category\":%d}", (int)c);
	if (api_ok(r)) {
		cJSON *it;
		cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "systems")) {
			if (nsys >= 64) break;
			sys_item_t *s = &sys_items[nsys++];
			snprintf(s->id, sizeof(s->id), "%s", api_str(it, "id") ? api_str(it, "id") : "");
			snprintf(s->name, sizeof(s->name), "%s", api_str(it, "name") ? api_str(it, "name") : "");
			s->games = (int)api_num(it, "games", 0);
			s->core = cJSON_IsTrue(cJSON_GetObjectItem(it, "core"));
		}
	}
	cJSON_Delete(r);
}

static void sys_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	snprintf(t, n, "%s", sys_items[i].name);
	snprintf(val, vn, "%d", sys_items[i].games);
	*dim = !sys_items[i].core;
}

static void sys_detail(void)
{
	if (!nsys) {
		lv_label_set_text(sys_detail_name, "No games yet");
		lv_label_set_text(sys_detail_count, "Put games in games/<system> on the SD card, a USB drive or your network share");
		lv_label_set_text(sys_detail_core, "");
		return;
	}
	sys_item_t *s = &sys_items[sys_list.sel];
	lv_label_set_text(sys_detail_name, s->name);
	char t[64];
	snprintf(t, sizeof(t), "%d game%s", s->games, s->games == 1 ? "" : "s");
	lv_label_set_text(sys_detail_count, t);
	lv_label_set_text(sys_detail_core, s->core ? "" : "Core not installed");
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
	load_systems(c);
	lv_obj_clean(body);
	screen_title("LIBRARY", category_name(c));
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

typedef struct { long long id; char *title; int linked, fav; char sys[24]; } game_item_t;
/* what the game list shows: a system's games, or (across systems) favorites, recently played, a search */
enum { GL_SYSTEM, GL_FAV, GL_RECENT, GL_SEARCH };
static int games_mode;
static char games_q[64];
static game_item_t *games;
static int ngames;
static char games_sys[24];
static int games_cat;
static vlist_t game_list;
static lv_obj_t *g_art, *g_noart, *g_title, *g_info, *g_desc, *g_where;
static lv_timer_t *art_timer;
static char art_src[1100];
static cJSON *detail;
static int info_y, desc_h;          /* the description moves up when there are no details */             /* the selected game's details */

static void free_games(void)
{
	for (int i = 0; i < ngames; i++) free(games[i].title);
	free(games);
	games = NULL;
	ngames = 0;
}

static void load_games(const char *sys)
{
	free_games();
	cJSON *r;
	if (games_mode == GL_FAV) r = api_call("{\"cmd\":\"games\",\"filter\":\"favorites\"}");
	else if (games_mode == GL_RECENT) r = api_call("{\"cmd\":\"games\",\"filter\":\"recent\",\"limit\":200}");
	else if (games_mode == GL_SEARCH) {
		cJSON *q = cJSON_CreateObject();
		cJSON_AddStringToObject(q, "cmd", "games");
		cJSON_AddStringToObject(q, "q", games_q);
		cJSON_AddNumberToObject(q, "limit", 500);
		char *b = cJSON_PrintUnformatted(q);
		cJSON_Delete(q);
		r = api_call("%s", b);
		free(b);
	}
	else r = api_call("{\"cmd\":\"games\",\"system\":\"%s\"}", sys);
	if (api_ok(r)) {
		cJSON *arr = cJSON_GetObjectItem(r, "games");
		int n = cJSON_GetArraySize(arr);
		games = calloc((size_t)(n ? n : 1), sizeof(*games));
		cJSON *it;
		cJSON_ArrayForEach(it, arr) {
			games[ngames].id = (long long)api_num(it, "id", -1);
			games[ngames].title = strdup(api_str(it, "title") ? api_str(it, "title") : "");
			games[ngames].linked = cJSON_IsTrue(cJSON_GetObjectItem(it, "linked"));
			games[ngames].fav = cJSON_IsTrue(cJSON_GetObjectItem(it, "favorite"));
			snprintf(games[ngames].sys, sizeof(games[0].sys), "%s", api_str(it, "system") ? api_str(it, "system") : "");
			ngames++;
		}
	}
	cJSON_Delete(r);
}

/* system names for lists across systems */
static struct { char id[24], name[48]; } sysname[96];
static int nsysname;
static const char *system_name(const char *id)
{
	if (!nsysname) {
		cJSON *r = api_call("{\"cmd\":\"systems\",\"all\":true}"), *it;
		cJSON *arr = api_ok(r) ? cJSON_GetObjectItem(r, "systems") : NULL;
		cJSON_ArrayForEach(it, arr) {
			if (nsysname >= 96) break;
			snprintf(sysname[nsysname].id, sizeof(sysname[0].id), "%s", api_str(it, "id") ? api_str(it, "id") : "");
			snprintf(sysname[nsysname].name, sizeof(sysname[0].name), "%s", api_str(it, "name") ? api_str(it, "name") : "");
			nsysname++;
		}
		cJSON_Delete(r);
	}
	for (int i = 0; i < nsysname; i++) if (!strcmp(sysname[i].id, id)) return sysname[i].name;
	return id;
}

static void game_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	(void)dim;
	snprintf(t, n, "%s", games[i].title);
	if (games_mode == GL_SYSTEM) { if (games[i].fav) snprintf(val, vn, "Favorite"); }
	else snprintf(val, vn, "%s%s", games[i].fav && games_mode != GL_FAV ? "Favorite  \xC2\xB7  " : "", system_name(games[i].sys));
}

static void games_toggle_fav(void)
{
	if (!ngames) return;
	game_item_t *g = &games[game_list.sel];
	cJSON *r = api_call("{\"cmd\":\"favorite\",\"id\":%lld,\"on\":%s}", g->id, g->fav ? "false" : "true");
	if (api_ok(r)) {
		g->fav = !g->fav;
		toast_show(g->fav ? "Added to Favorites" : "Removed from Favorites");
		vlist_refresh(&game_list);
	} else toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't change it");
	cJSON_Delete(r);
}

/* box art loads a moment after the selection settles, so scrolling through a list stays quick */
static void art_load(lv_timer_t *t)
{
	(void)t;
	art_timer = NULL;
	if (cur != SCR_GAMES || !detail) return;
	const char *art = api_str(detail, "art");
	if (art) {
		snprintf(art_src, sizeof(art_src), "A:%s", art);   /* LVGL's file driver letter */
		lv_image_set_src(g_art, art_src);
		lv_obj_set_hidden(g_art, false);
		lv_obj_set_hidden(g_noart, true);
	}
}

static void game_detail(void)
{
	if (art_timer) { lv_timer_delete(art_timer); art_timer = NULL; }
	cJSON_Delete(detail);
	detail = NULL;
	if (!ngames) {
		lv_label_set_text(g_title, games_mode == GL_FAV ? "No favorites yet" : games_mode == GL_RECENT ? "Nothing played yet"
			: games_mode == GL_SEARCH ? "Nothing with that name" : "No games found");
		lv_label_set_text(g_info, "");
		lv_label_set_text(g_desc, games_mode == GL_FAV ? "In any list of games, X marks the selected one as a favorite."
			: games_mode == GL_RECENT ? "Games you play show up here, the last one first."
			: games_mode == GL_SEARCH ? "Search looks for part of a game's name: try a shorter word."
			: "Put them in games/ on the SD card, a USB drive or your network share.");
		lv_label_set_text(g_where, "");
		lv_obj_set_hidden(g_art, true);
		lv_obj_set_hidden(g_noart, false);
		return;
	}
	game_item_t *g = &games[game_list.sel];
	cJSON *r = api_call("{\"cmd\":\"game\",\"id\":%lld}", g->id);
	if (api_ok(r)) detail = cJSON_DetachItemFromObject(r, "game");
	cJSON_Delete(r);
	const cJSON *d = detail;
	lv_label_set_text(g_title, d && api_str(d, "title") ? api_str(d, "title") : g->title);

	/* Based on X · 1997 · Publisher · 1-2 players · Genre */
	char info[400] = "", players[32] = "", based[300] = "";
	const char *parts[5] = { NULL };
	cJSON *bo = d ? cJSON_GetObjectItem(d, "based_on") : NULL;
	if (cJSON_IsObject(bo) && api_str(bo, "title")) {
		snprintf(based, sizeof(based), "Based on %s", api_str(bo, "title"));
		parts[4] = based;
	}
	if (d) {
		parts[0] = api_str(d, "year");
		parts[1] = api_str(d, "publisher") ? api_str(d, "publisher") : api_str(d, "developer");
		const char *pl = api_str(d, "players");
		if (pl) { snprintf(players, sizeof(players), "%s player%s", pl, strcmp(pl, "1") ? "s" : ""); parts[2] = players; }
		parts[3] = api_str(d, "genre");
	}
	static const int order[5] = { 4, 0, 1, 2, 3 };
	for (int o = 0; o < 5; o++) {
		int i = order[o];
		if (!parts[i] || !*parts[i]) continue;
		if (info[0]) strncat(info, "  \xC2\xB7  ", sizeof(info) - strlen(info) - 1);
		strncat(info, parts[i], sizeof(info) - strlen(info) - 1);
	}
	lv_label_set_text(g_info, info);
	lv_obj_set_hidden(g_info, !info[0]);
	lv_obj_set_y(g_desc, info[0] ? info_y + S(28) : info_y);
	lv_obj_set_height(g_desc, info[0] ? desc_h : desc_h + S(28));
	const char *desc = d ? api_str(d, "description") : NULL;
	lv_label_set_text(g_desc, desc ? desc : "");

	/* where the file is: SD card · name.ext */
	char where[600] = "";
	cJSON *files = d ? cJSON_GetObjectItem(d, "files") : NULL;
	cJSON *f0 = cJSON_GetArrayItem(files, 0);
	if (f0) {
		const char *p = api_str(f0, "path"), *base = p ? strrchr(p, '/') : NULL;
		snprintf(where, sizeof(where), "%s  \xC2\xB7  %s%s", api_str(f0, "where"), base ? base + 1 : (p ? p : ""),
		         cJSON_IsTrue(cJSON_GetObjectItem(f0, "online")) ? "" : "  (not connected)");
	}
	lv_label_set_text(g_where, where);

	/* keep the old art up until the new one is ready, unless this game has none */
	if (!d || !api_str(d, "art")) {
		lv_obj_set_hidden(g_art, true);
		lv_obj_set_hidden(g_noart, false);
		return;
	}
	art_timer = lv_timer_create(art_load, 120, NULL);
	lv_timer_set_repeat_count(art_timer, 1);
}

static void games_hints(void);

static void show_games(const char *sys_id, const char *sys_name, int cat)
{
	cur = SCR_GAMES;
	if (sys_id[0]) games_mode = GL_SYSTEM;
	snprintf(games_sys, sizeof(games_sys), "%s", sys_id);
	games_cat = cat;
	if (art_timer) { lv_timer_delete(art_timer); art_timer = NULL; }
	load_games(sys_id);
	lv_obj_clean(body);
	char label[96];
	spaced_caps(label, sizeof(label), games_mode == GL_SYSTEM ? category_name((category_t)cat) : "Library");
	screen_title(label, sys_name);
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
	lv_obj_t *nic = icon_create(g_noart, CAT_ICON[cat >= 0 && cat < CAT_COUNT ? cat : 0], S(64));
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
	info_y = y;
	y += S(28);
	int bottom = S(34);
	desc_h = ph - y - bottom - S(8);
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
	lv_obj_update_layout(c);
	lv_obj_set_pos(c, S(56) + S(560) - S(10) - lv_obj_get_width(c), S(134));

	vlist_refresh(&game_list);
	game_detail();
	games_hints();
}

static void games_hints(void)
{
	static const char *const hints[] = { "A", "Play", "B", "Back", "X", "Favorite", "Y", "Based on", "L", "Page up", "R", "Page down", NULL };
	footer_set(hints);
}

/* ---------- "Based on" picker ----------
 * A hack, mod or translation can point at the game it was made from, and then shows that game's
 * box art and description under its own name. Most alike titles come first. */

typedef struct { long long id; char *title; } pick_item_t;
static pick_item_t *picks;
static int npicks, pick_unlink;         /* pick_unlink: row 0 is "Not based on another game" */
static long long pick_current = -1;
static lv_obj_t *pick_panel;
static vlist_t pick_list;

static void picker_free(void)
{
	for (int i = 0; i < npicks; i++) free(picks[i].title);
	free(picks);
	picks = NULL;
	npicks = 0;
}

static void pick_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	if (pick_unlink && i == 0) { snprintf(t, n, "Not based on another game"); *dim = 1; return; }
	pick_item_t *p = &picks[i - pick_unlink];
	snprintf(t, n, "%s", p->title);
	if (p->id == pick_current) snprintf(val, vn, "current");
}

static int picker_open(void) { return pick_panel != NULL; }

static void picker_close(void)
{
	if (pick_panel) lv_obj_delete(pick_panel);
	pick_panel = NULL;
	picker_free();
	games_hints();
}

static void picker_show(void)
{
	if (!ngames) return;
	game_item_t *g = &games[game_list.sel];
	cJSON *r = api_call("{\"cmd\":\"candidates\",\"id\":%lld,\"limit\":500}", g->id);
	if (!api_ok(r)) { cJSON_Delete(r); toast_show("Couldn't load the game list"); return; }
	picker_free();
	cJSON *arr = cJSON_GetObjectItem(r, "games");
	int n = cJSON_GetArraySize(arr);
	picks = calloc((size_t)(n ? n : 1), sizeof(*picks));
	cJSON *it;
	cJSON_ArrayForEach(it, arr) {
		picks[npicks].id = (long long)api_num(it, "id", -1);
		picks[npicks].title = strdup(api_str(it, "title") ? api_str(it, "title") : "");
		npicks++;
	}
	pick_current = (long long)api_num(r, "current", -1);
	pick_unlink = pick_current >= 0;
	cJSON_Delete(r);
	if (!npicks && !pick_unlink) { picker_free(); toast_show("No other games for this system yet"); return; }

	int pw = S(760), ph = S(560), pad = S(30);
	pick_panel = mk_box(root, pw, ph, PAL.panel, S(18));
	lv_obj_set_style_border_width(pick_panel, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(pick_panel, PAL.acc, 0);
	lv_obj_set_style_shadow_width(pick_panel, S(40), 0);
	lv_obj_set_style_shadow_color(pick_panel, lv_color_black(), 0);
	lv_obj_set_style_shadow_opa(pick_panel, LV_OPA_60, 0);
	lv_obj_align(pick_panel, LV_ALIGN_CENTER, 0, -S(10));
	lv_obj_t *h = mk_label(pick_panel, FONT_TITLE, PAL.text, "Based on");
	lv_obj_set_pos(h, pad, pad);
	char sub[400];
	snprintf(sub, sizeof(sub), "Pick the game \"%s\" was made from. It will use that game's box art and description, and keep its own name.", g->title);
	lv_obj_t *sl = mk_label(pick_panel, FONT_SMALL, PAL.dim, sub);
	lv_label_set_long_mode(sl, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(sl, pw - 2 * pad);
	lv_obj_set_pos(sl, pad, pad + S(40));
	vlist_create(&pick_list, pick_panel, pad - S(6), pad + S(104), pw - 2 * pad + S(12), ph - pad - S(104) - S(20));
	pick_list.count = npicks + pick_unlink;
	pick_list.fill = pick_fill;
	pick_list.top = 0;
	pick_list.sel = pick_unlink ? 1 : 0;              /* start on the most alike game */
	if (pick_unlink && npicks)
		for (int i = 0; i < npicks; i++) if (picks[i].id == pick_current) pick_list.sel = i + 1;
	if (pick_list.sel >= pick_list.count) pick_list.sel = 0;
	vlist_refresh(&pick_list);
	lv_obj_move_foreground(pick_panel);
	static const char *const hints[] = { "A", "Use this game", "B", "Cancel", "L", "Page up", "R", "Page down", NULL };
	footer_set(hints);
}

static void picker_choose(void)
{
	long long id = games[game_list.sel].id;
	cJSON *r;
	char msg[400];
	if (pick_unlink && pick_list.sel == 0) {
		r = api_call("{\"cmd\":\"unlink\",\"id\":%lld}", id);
		snprintf(msg, sizeof(msg), "Not based on another game any more");
	} else {
		pick_item_t *p = &picks[pick_list.sel - pick_unlink];
		r = api_call("{\"cmd\":\"link\",\"id\":%lld,\"base\":%lld}", id, p->id);
		snprintf(msg, sizeof(msg), "Uses the art and description of %s", p->title);
	}
	if (!api_ok(r)) snprintf(msg, sizeof(msg), "%s", r && api_str(r, "error") ? api_str(r, "error") : "Couldn't change it");
	cJSON_Delete(r);
	picker_close();
	/* the list marks linked games: reload it, staying on this game */
	int keep = game_list.sel;
	load_games(games_sys);
	game_list.count = ngames;
	game_list.sel = keep;
	vlist_refresh(&game_list);
	game_detail();
	toast_show(msg);
}

static void picker_key(pkey_t k)
{
	if (vlist_key(&pick_list, k)) return;
	if (k == K_A) picker_choose();
	else if (k == K_B || k == K_Y) picker_close();
}

/* ---------- Settings ---------- */

static vlist_t set_list;
enum { SET_WIFI, SET_BT, SET_REMOTE, SET_CLOUD, SET_BACKUP, SET_PORTS, SET_VIDEO, SET_ACCENT, SET_TEXTURE, SET_CLOCK, SET_SCRAPE, SET_SCRAPE_INFO, SET_SOURCES, SET_RESCAN, SET_DIAG, SET_ABOUT, SET_COUNT };
static cJSON *scrape_state;      /* phosphord's scrape reply, refreshed while Settings is open */

static void scrape_refresh(void)
{
	cJSON_Delete(scrape_state);
	scrape_state = api_call("{\"cmd\":\"scrape\"}");
}

static void set_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	(void)dim;
	switch (i) {
	case SET_ACCENT: snprintf(t, n, "Accent colour"); snprintf(val, vn, "<  %s  >", theme_accent_name(theme_accent())); break;
	case SET_TEXTURE: snprintf(t, n, "Background texture"); snprintf(val, vn, "<  %s  >", texture_name((texture_t)SETTINGS.texture)); break;
	case SET_CLOCK: snprintf(t, n, "Clock"); snprintf(val, vn, "<  %s  >", SETTINGS.clock24 ? "24-hour" : "12-hour"); break;
	case SET_SCRAPE: {
		int on = scrape_state && cJSON_IsTrue(cJSON_GetObjectItem(scrape_state, "on"));
		snprintf(t, n, "Get box art and details");
		snprintf(val, vn, "<  %s  >", on ? "On" : "Off");
		break;
	}
	case SET_SCRAPE_INFO: {
		*dim = 1;
		if (!api_ok(scrape_state)) { snprintf(t, n, "Box art: phosphord isn't answering"); break; }
		int found = (int)api_num(scrape_state, "found", 0), nf = (int)api_num(scrape_state, "not_found", 0);
		int pend = (int)api_num(scrape_state, "pending", 0);
		const char *st = api_str(scrape_state, "status");
		snprintf(t, n, "%s", st ? st : "");
		if (pend) snprintf(val, vn, "%d found  \xC2\xB7  %d to go", found, pend);
		else snprintf(val, vn, "%d found  \xC2\xB7  %d not found", found, nf);
		break;
	}
	case SET_SOURCES: snprintf(t, n, "Box art sources"); snprintf(val, vn, ">"); break;
	case SET_WIFI: snprintf(t, n, "WiFi"); snprintf(val, vn, ">"); break;
	case SET_BT: snprintf(t, n, "Bluetooth"); snprintf(val, vn, ">"); break;
	case SET_REMOTE: snprintf(t, n, "Remote access"); snprintf(val, vn, ">"); break;
	case SET_CLOUD: snprintf(t, n, "Cloud saves"); snprintf(val, vn, ">"); break;
	case SET_BACKUP: snprintf(t, n, "Backups"); snprintf(val, vn, ">"); break;
	case SET_PORTS: snprintf(t, n, "Controllers & memory cards"); snprintf(val, vn, ">"); break;
	case SET_VIDEO: snprintf(t, n, "Screen filters"); snprintf(val, vn, ">"); break;
	case SET_RESCAN: snprintf(t, n, "Look for games again"); break;
	case SET_DIAG: snprintf(t, n, "Diagnostics"); snprintf(val, vn, ">"); break;
	case SET_ABOUT: snprintf(t, n, "Phosphor"); snprintf(val, vn, "%s", PHOS_VERSION); break;
	}
}

static void show_settings(void);
static void show_welcome(void);
static int in_setup;              /* the sub-screens go back to the welcome screen, not Settings */

static void back_to_settings(int sel)
{
	if (in_setup) { show_welcome(); return; }
	show_settings();
	set_list.sel = sel;
	vlist_refresh(&set_list);
}

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
	} else if (set_list.sel == SET_TEXTURE) {
		SETTINGS.texture = (SETTINGS.texture + dir + TEX_COUNT) % TEX_COUNT;
		settings_save();
		texture_apply(root, (texture_t)SETTINGS.texture);
		vlist_refresh(&set_list);
	} else if (set_list.sel == SET_SCRAPE) {
		int on = scrape_state && cJSON_IsTrue(cJSON_GetObjectItem(scrape_state, "on"));
		cJSON_Delete(scrape_state);
		scrape_state = api_call("{\"cmd\":\"scrape\",\"on\":%s}", on ? "false" : "true");
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
	scrape_refresh();
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

/* ---------- Box art sources ----------
 * Which sources the scraper asks, in what order (the first answer for each field wins).
 * Sources that need a key say so; keys go in phosphor/config/scraper.ini. */

typedef struct { char id[24], name[40], needs[120]; int on, ready; } src_item_t;
static src_item_t srcs[16];
static int nsrcs;
static vlist_t src_list;

static void show_settings(void);

static void sources_load(cJSON *r)
{
	nsrcs = 0;
	cJSON *it;
	if (api_ok(r)) cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "sources")) {
		if (nsrcs >= 16) break;
		src_item_t *x = &srcs[nsrcs++];
		snprintf(x->id, sizeof(x->id), "%s", api_str(it, "id") ? api_str(it, "id") : "");
		snprintf(x->name, sizeof(x->name), "%s", api_str(it, "name") ? api_str(it, "name") : "");
		snprintf(x->needs, sizeof(x->needs), "%s", api_str(it, "needs") ? api_str(it, "needs") : "");
		x->on = cJSON_IsTrue(cJSON_GetObjectItem(it, "on"));
		x->ready = cJSON_IsTrue(cJSON_GetObjectItem(it, "ready"));
	}
	cJSON_Delete(r);
}

static void src_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	src_item_t *x = &srcs[i];
	snprintf(t, n, "%d.  %s", i + 1, x->name);
	if (!x->on) { snprintf(val, vn, "Off"); *dim = 1; }
	else if (!x->ready) { snprintf(val, vn, "On  \xC2\xB7  needs a key"); *dim = 1; }
	else snprintf(val, vn, "On");
}

/* send the whole list back: order and which are off */
static void sources_save(void)
{
	char body[1200];
	int j = snprintf(body, sizeof(body), "{\"cmd\":\"scrape_sources\",\"order\":[");
	for (int i = 0; i < nsrcs; i++) j += snprintf(body + j, sizeof(body) - (size_t)j, "%s\"%s\"", i ? "," : "", srcs[i].id);
	j += snprintf(body + j, sizeof(body) - (size_t)j, "],\"off\":[");
	int k = 0;
	for (int i = 0; i < nsrcs; i++) if (!srcs[i].on) j += snprintf(body + j, sizeof(body) - (size_t)j, "%s\"%s\"", k++ ? "," : "", srcs[i].id);
	snprintf(body + j, sizeof(body) - (size_t)j, "]}");
	sources_load(api_call("%s", body));
	vlist_refresh(&src_list);
}

static void show_sources(void)
{
	cur = SCR_SOURCES;
	sources_load(api_call("{\"cmd\":\"scrape_sources\"}"));
	lv_obj_clean(body);
	screen_title("SETTINGS", "Box art sources");
	vlist_create(&src_list, body, S(56), S(176), S(760), S(330));
	src_list.count = nsrcs;
	src_list.fill = src_fill;
	src_list.sel = 0;
	src_list.top = 0;
	vlist_refresh(&src_list);
	lv_obj_t *note = mk_label(body, FONT_SMALL, PAL.dim,
	        "Asked in this order; for each piece (box art, description, year...) the first source that has it wins. "
	        "Games are matched by their contents, never guessed from file names.\n"
	        "TheGamesDB, ScreenScraper and IGDB need a key or login: put them in phosphor/config/scraper.ini on the SD card.");
	lv_label_set_long_mode(note, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(note, S(760));
	lv_obj_set_pos(note, S(56), S(176) + src_list.vis * src_list.row_h + S(14));
	static const char *const hints[] = { "A", "On / Off", "Y", "Move up", "X", "Move down", "B", "Back", NULL };
	footer_set(hints);
}

static void sources_key(pkey_t k)
{
	if (k == K_UP || k == K_DOWN) { vlist_key(&src_list, k); return; }
	int i = src_list.sel;
	if (!nsrcs) { if (k == K_B) { show_settings(); } return; }
	if (k == K_A || k == K_LEFT || k == K_RIGHT) { srcs[i].on = !srcs[i].on; sources_save(); }
	else if (k == K_Y && i > 0) {
		src_item_t t = srcs[i]; srcs[i] = srcs[i - 1]; srcs[i - 1] = t;
		src_list.sel--; sources_save();
	} else if (k == K_X && i + 1 < nsrcs) {
		src_item_t t = srcs[i]; srcs[i] = srcs[i + 1]; srcs[i + 1] = t;
		src_list.sel++; sources_save();
	} else if (k == K_B || k == K_MENU) {
		show_settings();
		set_list.sel = SET_SOURCES;
		vlist_refresh(&set_list);
	}
}

/* ---------- Diagnostics ----------
 * The diag module's checks of the whole system (board, cores, OS, storage, video, sound, input,
 * network, USB, Phosphor), run by phosphord in the background. Problems and warnings first. */

typedef struct { char label[96], msg[220]; int level; } diag_item_t;   /* level: 3 fail, 2 warn, 1 info, 0 ok */
static diag_item_t *ditems;
static int nditems;
static vlist_t diag_list;
static lv_obj_t *diag_head, *diag_sub;
static char diag_busy[16];

static int level_of(const char *l)
{
	return !l ? 0 : !strcmp(l, "fail") ? 3 : !strcmp(l, "warn") ? 2 : !strcmp(l, "info") ? 1 : 0;
}

static const char *AREA_NAMES[][2] = {
	{ "board", "Board" }, { "cores", "Cores" }, { "os", "System" }, { "storage", "Storage" }, { "video", "Video" },
	{ "sound", "Sound" }, { "input", "Controllers" }, { "network", "Network" }, { "usb", "USB" }, { "phosphor", "Phosphor" },
};

static void diag_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	diag_item_t *d = &ditems[i];
	snprintf(t, n, "%s  \xC2\xB7  %s", d->label, d->msg);
	static const char *WORD[] = { "OK", "Info", "Check", "Problem" };
	snprintf(val, vn, "%s", WORD[d->level]);
	*dim = d->level < 2;
}

static void diag_apply(cJSON *r)
{
	free(ditems);
	ditems = NULL;
	nditems = 0;
	snprintf(diag_busy, sizeof(diag_busy), "%s", api_str(r, "busy") ? api_str(r, "busy") : "");
	cJSON *arr = cJSON_GetObjectItem(r, "results");
	int n = cJSON_GetArraySize(arr);
	ditems = calloc((size_t)(n ? n : 1), sizeof(*ditems));
	cJSON *it;
	cJSON_ArrayForEach(it, arr) {
		diag_item_t *d = &ditems[nditems++];
		const char *area = api_str(it, "area"), *key = api_str(it, "key");
		const char *an = area ? area : "";
		for (size_t k = 0; k < sizeof(AREA_NAMES) / sizeof(AREA_NAMES[0]); k++) if (area && !strcmp(area, AREA_NAMES[k][0])) an = AREA_NAMES[k][1];
		char kk[64];
		snprintf(kk, sizeof(kk), "%s", key ? key : "");
		for (char *c = kk; *c; c++) if (*c == '_') *c = ' ';
		snprintf(d->label, sizeof(d->label), "%s %s", an, kk);
		snprintf(d->msg, sizeof(d->msg), "%s", api_str(it, "message") ? api_str(it, "message") : "");
		d->level = level_of(api_str(it, "level"));
	}
	/* problems first, then warnings, otherwise in the module's order */
	diag_item_t *sorted = calloc((size_t)(nditems ? nditems : 1), sizeof(*sorted));
	int k = 0;
	for (int pass = 3; pass >= 1; pass--)       /* problems, then warnings, then the rest */
		for (int i = 0; i < nditems; i++)
			if (pass == 1 ? ditems[i].level <= 1 : ditems[i].level == pass) sorted[k++] = ditems[i];
	free(ditems);
	ditems = sorted;

	cJSON *sm = cJSON_GetObjectItem(r, "summary");
	if (!api_ok(r)) {
		lv_label_set_text(diag_head, "phosphord isn't answering");
		lv_label_set_text(diag_sub, "");
	} else if (!cJSON_IsTrue(cJSON_GetObjectItem(r, "installed"))) {
		lv_label_set_text(diag_head, "The diagnostics module isn't installed");
		lv_label_set_text(diag_sub, "Install phosphor-modules/diag to /media/fat/phosphor/modules/diag.");
	} else if (!strcmp(diag_busy, "checking")) {
		lv_label_set_text(diag_head, "Checking the whole system...");
		lv_label_set_text(diag_sub, "Board, cores, system, storage, video, sound, controllers, network, USB. About 20 seconds.");
	} else if (sm) {
		int lv = level_of(api_str(sm, "level"));
		lv_label_set_text(diag_head, lv == 0 ? "Everything looks fine" : lv == 2 ? "A few things worth a look" : "Some things need fixing");
		char sub[300];
		const char *b = api_str(r, "bundle"), *e = api_str(r, "error");
		if (!strcmp(diag_busy, "bundle")) snprintf(sub, sizeof(sub), "%s  \xC2\xB7  making a report for support...", api_str(sm, "text"));
		else if (e) snprintf(sub, sizeof(sub), "%s  \xC2\xB7  %s", api_str(sm, "text"), e);
		else if (b) snprintf(sub, sizeof(sub), "%s  \xC2\xB7  report saved: %s", api_str(sm, "text"), b);
		else snprintf(sub, sizeof(sub), "%s", api_str(sm, "text"));
		lv_label_set_text(diag_sub, sub);
	} else {
		lv_label_set_text(diag_head, api_str(r, "error") ? api_str(r, "error") : "Not checked yet");
		lv_label_set_text(diag_sub, "");
	}
	cJSON_Delete(r);
	diag_list.count = nditems;
	vlist_refresh(&diag_list);
}

static void show_diag(void)
{
	cur = SCR_DIAG;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Diagnostics");
	diag_head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_obj_set_pos(diag_head, S(56), S(178));
	diag_sub = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(diag_sub, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(diag_sub, W - S(112), S(22));
	lv_obj_set_pos(diag_sub, S(56), S(208));
	vlist_create(&diag_list, body, S(56), S(244), W - S(112), S(380));
	diag_list.fill = diag_fill;
	diag_list.sel = 0;
	diag_list.top = 0;
	diag_list.count = 0;
	/* first visit: run the checks; later: show the last results */
	cJSON *r = api_call("{\"cmd\":\"diag\"}");
	if (api_ok(r) && cJSON_IsNull(cJSON_GetObjectItem(r, "at")) && !*(api_str(r, "busy") ? api_str(r, "busy") : "")) {
		cJSON_Delete(r);
		r = api_call("{\"cmd\":\"diag\",\"run\":true}");
	}
	diag_apply(r);
	static const char *const hints[] = { "Y", "Check again", "X", "Save a report for support", "B", "Back", NULL };
	footer_set(hints);
}

static void diag_key(pkey_t k)
{
	if (vlist_key(&diag_list, k)) return;
	if (k == K_Y && !diag_busy[0]) diag_apply(api_call("{\"cmd\":\"diag\",\"run\":true}"));
	else if (k == K_X && !diag_busy[0]) diag_apply(api_call("{\"cmd\":\"diag\",\"bundle\":true}"));
	else if (k == K_B || k == K_MENU) {
		show_settings();
		set_list.sel = SET_DIAG;
		vlist_refresh(&set_list);
	}
}

/* ---------- on-screen keyboard ----------
 * For WiFi passwords and names, with a controller: D-pad moves, A types, X deletes, Y switches
 * letters / capitals / symbols, Start is done, B cancels. Works over any screen. */

#define OSK_COLS 10
#define OSK_ROWS 5          /* 4 rows of characters + the action row */
static const char *const OSK_LAYOUT[3][4] = {
	{ "1234567890", "qwertyuiop", "asdfghjkl'", "zxcvbnm,.-" },
	{ "1234567890", "QWERTYUIOP", "ASDFGHJKL\"", "ZXCVBNM;:_" },
	{ "!@#$%^&*()", "-_=+[]{}\\|", ";:'\",.<>/?", "~`  " },
};
/* action row: Shift, Space, Delete, Done (each spans several columns) */
enum { OA_SHIFT, OA_SPACE, OA_DEL, OA_DONE, OA_COUNT };
static const int OA_START[OA_COUNT] = { 0, 2, 7, 9 }, OA_SPAN[OA_COUNT] = { 2, 5, 2, 1 };
static const char *const OA_NAME[OA_COUNT] = { "Shift", "Space", "Delete", "Done" };

static struct {
	lv_obj_t *panel, *field, *cell[4][OSK_COLS], *act[OA_COUNT], *hint;
	int open, layer, row, col, masked, max;
	char text[80];
	void (*done)(const char *text);    /* NULL text: cancelled */
	const char *const *prev_hints;
} osk;

static void osk_draw(void)
{
	for (int r = 0; r < 4; r++)
		for (int c = 0; c < OSK_COLS; c++) {
			const char *row = OSK_LAYOUT[osk.layer][r];
			char ch[2] = { c < (int)strlen(row) ? row[c] : ' ', 0 };
			lv_label_set_text(lv_obj_get_child(osk.cell[r][c], 0), ch);
			int on = osk.row == r && osk.col == c;
			lv_obj_set_style_bg_color(osk.cell[r][c], on ? PAL.acc : PAL.panel_on, 0);
			lv_obj_set_style_text_color(lv_obj_get_child(osk.cell[r][c], 0), on ? PAL.bg : PAL.text, 0);
		}
	for (int a = 0; a < OA_COUNT; a++) {
		int on = osk.row == 4 && osk.col >= OA_START[a] && osk.col < OA_START[a] + OA_SPAN[a];
		lv_obj_set_style_bg_color(osk.act[a], on ? PAL.acc : PAL.panel_on, 0);
		lv_obj_set_style_text_color(lv_obj_get_child(osk.act[a], 0), on ? PAL.bg : PAL.text, 0);
	}
	lv_label_set_text(lv_obj_get_child(osk.act[OA_SHIFT], 0), osk.layer == 0 ? "ABC" : osk.layer == 1 ? "#+=" : "abc");
	char shown[100];
	size_t n = strlen(osk.text);
	if (osk.masked) { memset(shown, '*', n); shown[n] = 0; }
	else snprintf(shown, sizeof(shown), "%s", osk.text);
	strncat(shown, "_", sizeof(shown) - strlen(shown) - 1);
	lv_label_set_text(osk.field, shown);
}

static void osk_close(const char *result)
{
	osk.open = 0;
	if (osk.panel) lv_obj_delete(osk.panel);
	osk.panel = NULL;
	if (osk.prev_hints) footer_set(osk.prev_hints);
	void (*cb)(const char *) = osk.done;
	osk.done = NULL;
	if (cb) cb(result);
}

static void osk_open(const char *title, const char *hint, int masked, int max, void (*done)(const char *), const char *const *back_hints)
{
	memset(osk.text, 0, sizeof(osk.text));
	osk.open = 1; osk.layer = 0; osk.row = 1; osk.col = 0; osk.masked = masked;
	osk.max = max < (int)sizeof(osk.text) - 1 ? max : (int)sizeof(osk.text) - 1;
	osk.done = done;
	osk.prev_hints = back_hints;
	int cw = S(62), ch = S(52), gap = S(8), pad = S(30);
	int pw = pad * 2 + OSK_COLS * cw + (OSK_COLS - 1) * gap, ph = S(150) + OSK_ROWS * (ch + gap) + S(20);
	osk.panel = mk_box(root, pw, ph, PAL.panel, S(18));
	lv_obj_set_style_border_width(osk.panel, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(osk.panel, PAL.acc, 0);
	lv_obj_align(osk.panel, LV_ALIGN_CENTER, 0, -S(10));
	lv_obj_t *h = mk_label(osk.panel, FONT_TITLE, PAL.text, title);
	lv_obj_set_pos(h, pad, pad - S(6));
	osk.hint = mk_label(osk.panel, FONT_SMALL, PAL.dim, hint);
	lv_obj_set_pos(osk.hint, pad, pad + S(28));
	lv_obj_t *fb = mk_box(osk.panel, pw - 2 * pad, S(46), PAL.bg, S(10));
	lv_obj_set_pos(fb, pad, pad + S(56));
	osk.field = mk_label(fb, FONT_NAME, PAL.text, "_");
	lv_obj_align(osk.field, LV_ALIGN_LEFT_MID, S(14), 0);
	int y0 = pad + S(118);
	for (int r = 0; r < 4; r++)
		for (int c = 0; c < OSK_COLS; c++) {
			lv_obj_t *b = mk_box(osk.panel, cw, ch, PAL.panel_on, S(8));
			lv_obj_set_pos(b, pad + c * (cw + gap), y0 + r * (ch + gap));
			lv_obj_t *l = mk_label(b, FONT_NAME, PAL.text, "");
			lv_obj_center(l);
			osk.cell[r][c] = b;
		}
	for (int a = 0; a < OA_COUNT; a++) {
		lv_obj_t *b = mk_box(osk.panel, OA_SPAN[a] * cw + (OA_SPAN[a] - 1) * gap, ch, PAL.panel_on, S(8));
		lv_obj_set_pos(b, pad + OA_START[a] * (cw + gap), y0 + 4 * (ch + gap));
		lv_obj_t *l = mk_label(b, FONT_BODY, PAL.text, OA_NAME[a]);
		lv_obj_center(l);
		osk.act[a] = b;
	}
	lv_obj_move_foreground(osk.panel);
	static const char *const hints[] = { "A", "Type", "X", "Delete", "Y", "ABC / #+=", "\xE2\x89\xA1", "Done", "B", "Cancel", NULL };
	footer_set(hints);
	osk_draw();
}

static void osk_type(char c)
{
	size_t n = strlen(osk.text);
	if ((int)n < osk.max) { osk.text[n] = c; osk.text[n + 1] = 0; }
}

static int osk_key(pkey_t k)
{
	if (!osk.open) return 0;
	switch (k) {
	case K_UP: osk.row = (osk.row + OSK_ROWS - 1) % OSK_ROWS; break;
	case K_DOWN: osk.row = (osk.row + 1) % OSK_ROWS; break;
	case K_LEFT:
		if (osk.row == 4) { int a = 0; for (int i = 0; i < OA_COUNT; i++) if (osk.col >= OA_START[i]) a = i; osk.col = OA_START[(a + OA_COUNT - 1) % OA_COUNT]; }
		else osk.col = (osk.col + OSK_COLS - 1) % OSK_COLS;
		break;
	case K_RIGHT:
		if (osk.row == 4) { int a = 0; for (int i = 0; i < OA_COUNT; i++) if (osk.col >= OA_START[i]) a = i; osk.col = OA_START[(a + 1) % OA_COUNT]; }
		else osk.col = (osk.col + 1) % OSK_COLS;
		break;
	case K_A:
		if (osk.row < 4) {
			const char *row = OSK_LAYOUT[osk.layer][osk.row];
			if (osk.col < (int)strlen(row) && row[osk.col] != ' ') osk_type(row[osk.col]);
		} else {
			int a = 0;
			for (int i = 0; i < OA_COUNT; i++) if (osk.col >= OA_START[i]) a = i;
			if (a == OA_SHIFT) osk.layer = (osk.layer + 1) % 3;
			else if (a == OA_SPACE) osk_type(' ');
			else if (a == OA_DEL) { size_t n = strlen(osk.text); if (n) osk.text[n - 1] = 0; }
			else { char t[80]; snprintf(t, sizeof(t), "%s", osk.text); osk_close(t); return 1; }
		}
		break;
	case K_X: { size_t n = strlen(osk.text); if (n) osk.text[n - 1] = 0; break; }
	case K_Y: osk.layer = (osk.layer + 1) % 3; break;
	case K_MENU: { char t[80]; snprintf(t, sizeof(t), "%s", osk.text); osk_close(t); return 1; }
	case K_B: osk_close(NULL); return 1;
	default: break;
	}
	osk_draw();
	return 1;
}

/* ---------- WiFi ----------
 * Networks in range (strongest first), saved ones marked; A joins (asks for the password when
 * needed), X forgets, Y scans again, Select undoes the last change. phosphord writes MiSTer's
 * wpa_supplicant.conf (backed up first) and makes wpa_supplicant read it again. */

typedef struct { char ssid[40]; int signal, bands, saved, current, inrange; char security[16]; } wnet_t;
static wnet_t wnets[64];
static int nwnets, wscanning;
static vlist_t wifi_list;
static lv_obj_t *wifi_head, *wifi_sub;
static char wifi_pending[40];        /* network waiting for its password */
static int wifi_other;               /* "Other network": name typed first */
static const char *const WIFI_HINTS[] = { "A", "Join", "X", "Forget", "Y", "Scan", "SELECT", "Undo", "B", "Back", NULL };

static const char *signal_word(int dbm) { return dbm >= -55 ? "Strong" : dbm >= -67 ? "Good" : dbm >= -75 ? "Fair" : "Weak"; }

static void wifi_apply(cJSON *r)
{
	if (!api_ok(r)) {
		lv_label_set_text(wifi_head, r && api_str(r, "error") ? api_str(r, "error") : "phosphord isn't answering");
		cJSON_Delete(r);
		return;
	}
	cJSON *st = cJSON_GetObjectItem(r, "status");
	char head[160], sub[200];
	if (!cJSON_IsTrue(cJSON_GetObjectItem(st, "present"))) {
		snprintf(head, sizeof(head), "No WiFi adapter found");
		snprintf(sub, sizeof(sub), "Plug in a USB WiFi adapter, or use Ethernet.");
	} else if (cJSON_IsTrue(cJSON_GetObjectItem(st, "connected"))) {
		int sig = (int)api_num(st, "signal", 0), fq = (int)api_num(st, "freq", 0);
		snprintf(head, sizeof(head), "Connected to %s", api_str(st, "ssid") ? api_str(st, "ssid") : "");
		snprintf(sub, sizeof(sub), "%s signal (%d dBm)  \xC2\xB7  %s GHz%s%s", signal_word(sig), sig, fq >= 5000 ? "5" : "2.4",
		         api_str(st, "ip") && *api_str(st, "ip") ? "  \xC2\xB7  " : "", api_str(st, "ip") ? api_str(st, "ip") : "");
	} else {
		snprintf(head, sizeof(head), "Not connected");
		snprintf(sub, sizeof(sub), "Pick a network and press A.");
	}
	wscanning = cJSON_IsTrue(cJSON_GetObjectItem(r, "scanning"));
	if (wscanning) snprintf(sub + strlen(sub), sizeof(sub) - strlen(sub), "%sLooking for networks...", sub[0] ? "  \xC2\xB7  " : "");
	lv_label_set_text(wifi_head, head);
	lv_label_set_text(wifi_sub, sub);
	nwnets = 0;
	cJSON *it;
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "networks")) {
		if (nwnets >= 63) break;
		wnet_t *w = &wnets[nwnets++];
		memset(w, 0, sizeof(*w));
		snprintf(w->ssid, sizeof(w->ssid), "%s", api_str(it, "ssid") ? api_str(it, "ssid") : "");
		snprintf(w->security, sizeof(w->security), "%s", api_str(it, "security") ? api_str(it, "security") : "");
		w->signal = (int)api_num(it, "signal", -100);
		w->bands = (int)api_num(it, "bands", 0);
		w->saved = cJSON_IsTrue(cJSON_GetObjectItem(it, "saved"));
		w->current = cJSON_IsTrue(cJSON_GetObjectItem(it, "current"));
		w->inrange = 1;
	}
	/* saved networks that aren't in range right now */
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "saved")) {
		const char *ss = api_str(it, "ssid");
		int seen = 0;
		for (int i = 0; i < nwnets; i++) if (ss && !strcmp(wnets[i].ssid, ss)) seen = 1;
		if (seen || !ss || nwnets >= 63) continue;
		wnet_t *w = &wnets[nwnets++];
		memset(w, 0, sizeof(*w));
		snprintf(w->ssid, sizeof(w->ssid), "%s", ss);
		w->saved = 1;
	}
	cJSON_Delete(r);
	wifi_list.count = nwnets + 1;                 /* + "Other network..." */
	vlist_refresh(&wifi_list);
}

static void wifi_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	if (i == nwnets) { snprintf(t, n, "Other network..."); snprintf(val, vn, "hidden name"); *dim = 1; return; }
	wnet_t *w = &wnets[i];
	snprintf(t, n, "%s", w->ssid);
	if (w->current) snprintf(val, vn, "Connected  \xC2\xB7  %s", signal_word(w->signal));
	else if (!w->inrange) { snprintf(val, vn, "Saved  \xC2\xB7  not in range"); *dim = 1; }
	else snprintf(val, vn, "%s%s  \xC2\xB7  %s%s", w->saved ? "Saved  \xC2\xB7  " : "",
	              !strcmp(w->security, "open") ? "Open" : !strcmp(w->security, "wpa3") ? "WPA3" : !strcmp(w->security, "enterprise") ? "Enterprise" : "Secured",
	              signal_word(w->signal), w->bands == 3 ? "  \xC2\xB7  2.4 + 5 GHz" : w->bands == 2 ? "  \xC2\xB7  5 GHz" : "");
}

static void wifi_send(const char *ssid, const char *password, int hidden)
{
	cJSON *req = cJSON_CreateObject();
	cJSON_AddStringToObject(req, "cmd", "wifi");
	cJSON_AddStringToObject(req, "do", "connect");
	cJSON_AddStringToObject(req, "ssid", ssid);
	if (password) cJSON_AddStringToObject(req, "password", password);
	if (hidden) cJSON_AddBoolToObject(req, "hidden", 1);
	char *body = cJSON_PrintUnformatted(req);
	cJSON_Delete(req);
	cJSON *r = api_call("%s", body);
	free(body);
	if (api_ok(r)) { char m[120]; snprintf(m, sizeof(m), "Joining %s...", ssid); toast_show(m); }
	else toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't join");
	wifi_apply(r);
}

static void wifi_password_done(const char *pw)
{
	if (cur != SCR_WIFI) return;
	if (!pw) { footer_set(WIFI_HINTS); return; }
	wifi_send(wifi_pending, pw, wifi_other);
	wifi_other = 0;
}

static void wifi_name_done(const char *name)
{
	if (!name || !*name) { footer_set(WIFI_HINTS); wifi_other = 0; return; }
	snprintf(wifi_pending, sizeof(wifi_pending), "%s", name);
	osk_open("Password", "Leave it empty for an open network, then Done.", 0, 63, wifi_password_done, WIFI_HINTS);
}

static void show_wifi(void)
{
	cur = SCR_WIFI;
	lv_obj_clean(body);
	screen_title("SETTINGS", "WiFi");
	wifi_head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_obj_set_pos(wifi_head, S(56), S(178));
	wifi_sub = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(wifi_sub, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(wifi_sub, W - S(112), S(22));
	lv_obj_set_pos(wifi_sub, S(56), S(208));
	vlist_create(&wifi_list, body, S(56), S(244), S(900), S(380));
	wifi_list.fill = wifi_fill;
	wifi_list.sel = 0;
	wifi_list.top = 0;
	wifi_list.count = 0;
	wifi_apply(api_call("{\"cmd\":\"wifi\",\"do\":\"scan\"}"));
	footer_set(WIFI_HINTS);
}

static void wifi_key(pkey_t k)
{
	if (vlist_key(&wifi_list, k)) return;
	int i = wifi_list.sel;
	if (k == K_A) {
		if (i == nwnets) {                        /* Other network... */
			wifi_other = 1;
			osk_open("Network name", "Type the network's name exactly, then Done.", 0, 32, wifi_name_done, WIFI_HINTS);
			return;
		}
		if (i > nwnets) return;
		wnet_t *w = &wnets[i];
		if (!strcmp(w->security, "wpa3") || !strcmp(w->security, "enterprise")) {
			toast_show(!strcmp(w->security, "wpa3") ? "WPA3-only networks aren't supported yet" : "Enterprise (802.1X) networks aren't supported");
			return;
		}
		if (w->saved || !strcmp(w->security, "open")) { wifi_send(w->ssid, NULL, 0); return; }
		snprintf(wifi_pending, sizeof(wifi_pending), "%s", w->ssid);
		wifi_other = 0;
		char hint[120];
		snprintf(hint, sizeof(hint), "Password for %s", w->ssid);
		osk_open("Password", hint, 0, 63, wifi_password_done, WIFI_HINTS);
	} else if (k == K_X && i < nwnets && wnets[i].saved) {
		cJSON *req = cJSON_CreateObject();
		cJSON_AddStringToObject(req, "cmd", "wifi");
		cJSON_AddStringToObject(req, "do", "forget");
		cJSON_AddStringToObject(req, "ssid", wnets[i].ssid);
		char *b = cJSON_PrintUnformatted(req);
		cJSON_Delete(req);
		cJSON *r = api_call("%s", b);
		free(b);
		toast_show(api_ok(r) ? "Forgotten (SELECT undoes it)" : (r && api_str(r, "error") ? api_str(r, "error") : "Couldn't forget it"));
		wifi_apply(r);
	} else if (k == K_Y) {
		wifi_apply(api_call("{\"cmd\":\"wifi\",\"do\":\"scan\"}"));
	} else if (k == K_SELECT) {
		cJSON *r = api_call("{\"cmd\":\"wifi\",\"do\":\"undo\"}");
		toast_show(api_ok(r) ? "Last WiFi change undone" : (r && api_str(r, "error") ? api_str(r, "error") : "Couldn't undo"));
		wifi_apply(r);
	} else if (k == K_B || k == K_MENU) {
		back_to_settings(SET_WIFI);
	}
}

/* ---------- Remote access ----------
 * Lets the phone app / PC app / Claude plugin manage the library over the home network. Off until
 * turned on here; each device pairs once with a 6-digit code shown on this screen. */

typedef struct { char id[17], name[64]; long created, last_seen; } rdev_t;
static rdev_t rdevs[16];
static int nrdevs, r_on, r_listening, r_port;
static char r_addr[48], r_err[128], r_code[8], r_paired[64];
static int r_left;
static vlist_t rem_list;
static lv_obj_t *rem_head, *rem_sub, *pair_box, *pair_code, *pair_info, *pair_left;
static const char *const REM_HINTS[] = { "A", "Change", "X", "Forget device", "B", "Back", NULL };
static const char *const PAIR_HINTS[] = { "B", "Close", NULL };

static void when(long t, char *out, size_t n)
{
	if (t <= 0) { snprintf(out, n, "never"); return; }
	time_t now = time(NULL), tt = (time_t)t;
	if (now - tt < 120) { snprintf(out, n, "just now"); return; }
	struct tm a, b;
	localtime_r(&tt, &a);
	localtime_r(&now, &b);
	if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday) snprintf(out, n, "today");
	else strftime(out, n, "%b %d", &a);
}

static void pair_close(void)
{
	if (pair_box) { lv_obj_delete(pair_box); pair_box = NULL; }
	footer_set(REM_HINTS);
}

static void pair_draw(void)
{
	if (!pair_box) return;
	char c[16];
	snprintf(c, sizeof(c), "%.3s %.3s", r_code, r_code + 3);
	lv_label_set_text(pair_code, c);
	char info[200];
	snprintf(info, sizeof(info), "In the Phosphor app, enter this code and the address\n%s:%d", r_addr[0] ? r_addr : "(no network)", r_port);
	lv_label_set_text(pair_info, info);
	char l[48];
	snprintf(l, sizeof(l), "Closes in %d:%02d", r_left / 60, r_left % 60);
	lv_label_set_text(pair_left, l);
}

static void remote_apply(cJSON *r)
{
	if (!api_ok(r)) {
		if (rem_head) lv_label_set_text(rem_head, r && api_str(r, "error") ? api_str(r, "error") : "phosphord isn't answering");
		cJSON_Delete(r);
		return;
	}
	int had_code = r_code[0] != 0;
	char old_paired[64];
	snprintf(old_paired, sizeof(old_paired), "%s", r_paired);
	r_on = cJSON_IsTrue(cJSON_GetObjectItem(r, "on"));
	r_listening = cJSON_IsTrue(cJSON_GetObjectItem(r, "listening"));
	r_port = (int)api_num(r, "port", 8767);
	snprintf(r_err, sizeof(r_err), "%s", api_str(r, "error") ? api_str(r, "error") : "");
	cJSON *a = cJSON_GetObjectItem(r, "addresses");
	snprintf(r_addr, sizeof(r_addr), "%s", cJSON_GetArraySize(a) > 0 && cJSON_IsString(cJSON_GetArrayItem(a, 0)) ? cJSON_GetArrayItem(a, 0)->valuestring : "");
	cJSON *p = cJSON_GetObjectItem(r, "pairing");
	snprintf(r_code, sizeof(r_code), "%s", cJSON_IsObject(p) && api_str(p, "code") ? api_str(p, "code") : "");
	r_left = cJSON_IsObject(p) ? (int)api_num(p, "seconds", 0) : 0;
	snprintf(r_paired, sizeof(r_paired), "%s", api_str(r, "paired") ? api_str(r, "paired") : "");
	nrdevs = 0;
	cJSON *it;
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "devices")) {
		if (nrdevs >= 16) break;
		rdev_t *d = &rdevs[nrdevs++];
		snprintf(d->id, sizeof(d->id), "%s", api_str(it, "id") ? api_str(it, "id") : "");
		snprintf(d->name, sizeof(d->name), "%s", api_str(it, "name") ? api_str(it, "name") : "");
		d->created = (long)api_num(it, "created", 0);
		d->last_seen = (long)api_num(it, "last_seen", 0);
	}
	cJSON_Delete(r);
	if (rem_head) {
		char h[160], sub[200];
		if (!r_on) {
			snprintf(h, sizeof(h), "Off");
			snprintf(sub, sizeof(sub), "Turn it on to manage your library from the Phosphor app on a phone or PC, on your home network.");
		} else if (!r_listening) {
			snprintf(h, sizeof(h), "On, but not reachable");
			snprintf(sub, sizeof(sub), "%s", r_err[0] ? r_err : "Starting...");
		} else {
			snprintf(h, sizeof(h), "On  \xC2\xB7  %s:%d", r_addr[0] ? r_addr : "no network yet", r_port);
			snprintf(sub, sizeof(sub), "Paired devices can see and edit the library, box art and screen filters. WiFi and pairing stay here.");
		}
		lv_label_set_text(rem_head, h);
		lv_label_set_text(rem_sub, sub);
		rem_list.count = 2 + nrdevs;
		vlist_refresh(&rem_list);
	}
	if (pair_box) {
		if (!r_code[0]) {
			if (r_paired[0] && strcmp(r_paired, old_paired)) { char m[120]; snprintf(m, sizeof(m), "Paired: %s", r_paired); toast_show(m); }
			else if (had_code) toast_show("Pairing closed");
			pair_close();
		} else pair_draw();
	}
}

static void rem_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	if (i == 0) { snprintf(t, n, "Remote access"); snprintf(val, vn, "<  %s  >", r_on ? "On" : "Off"); *dim = !r_on; return; }
	if (i == 1) { snprintf(t, n, "Pair a device"); snprintf(val, vn, r_on ? ">" : "turn it on first"); *dim = !r_on; return; }
	rdev_t *d = &rdevs[i - 2];
	char a[32], b[32];
	when(d->created, a, sizeof(a));
	when(d->last_seen, b, sizeof(b));
	snprintf(t, n, "%s", d->name);
	snprintf(val, vn, "paired %s  \xC2\xB7  last used %s", a, b);
}

static void pair_open(void)
{
	cJSON *r = api_call("{\"cmd\":\"remote\",\"do\":\"pair\"}");
	if (!api_ok(r)) { toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't start pairing"); cJSON_Delete(r); return; }
	r_paired[0] = 0;
	pair_box = mk_box(body, S(720), S(300), PAL.panel, S(16));
	lv_obj_set_style_border_width(pair_box, 1, 0);
	lv_obj_set_style_border_color(pair_box, PAL.acc, 0);
	lv_obj_center(pair_box);
	lv_obj_t *l = mk_label(pair_box, FONT_LABEL, PAL.dim, "PAIR A DEVICE");
	lv_obj_set_style_text_letter_space(l, S(3), 0);
	lv_obj_set_pos(l, S(36), S(30));
	pair_code = mk_label(pair_box, FONT_LOGO, PAL.acc, "");
	lv_obj_set_style_text_letter_space(pair_code, S(6), 0);
	lv_obj_set_pos(pair_code, S(36), S(64));
	pair_info = mk_label(pair_box, FONT_BODY, PAL.text, "");
	lv_obj_set_pos(pair_info, S(36), S(140));
	pair_left = mk_label(pair_box, FONT_SMALL, PAL.dim, "");
	lv_obj_set_pos(pair_left, S(36), S(236));
	footer_set(PAIR_HINTS);
	remote_apply(r);
}

static void show_remote(void)
{
	cur = SCR_REMOTE;
	pair_box = NULL;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Remote access");
	rem_head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_obj_set_pos(rem_head, S(56), S(178));
	rem_sub = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(rem_sub, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(rem_sub, W - S(112), S(22));
	lv_obj_set_pos(rem_sub, S(56), S(208));
	vlist_create(&rem_list, body, S(56), S(244), S(900), S(380));
	rem_list.fill = rem_fill;
	rem_list.sel = 0;
	rem_list.top = 0;
	rem_list.count = 2;
	remote_apply(api_call("{\"cmd\":\"remote\"}"));
	footer_set(REM_HINTS);
}

static void remote_key(pkey_t k)
{
	if (pair_box) {
		if (k == K_B || k == K_MENU) {
			cJSON_Delete(api_call("{\"cmd\":\"remote\",\"do\":\"stop\"}"));
			r_code[0] = 0;
			pair_close();
		}
		return;
	}
	if (vlist_key(&rem_list, k)) return;
	int i = rem_list.sel;
	if (i == 0 && (k == K_A || k == K_LEFT || k == K_RIGHT)) {
		remote_apply(api_call("{\"cmd\":\"remote\",\"on\":%s}", r_on ? "false" : "true"));
		toast_show(r_on ? "Remote access is on" : "Remote access is off");
	} else if (i == 1 && k == K_A) {
		if (!r_on) toast_show("Turn remote access on first");
		else pair_open();
	} else if (i >= 2 && k == K_X && i - 2 < nrdevs) {
		char name[64];
		snprintf(name, sizeof(name), "%s", rdevs[i - 2].name);
		cJSON *r = api_call("{\"cmd\":\"remote\",\"do\":\"forget\",\"id\":\"%s\"}", rdevs[i - 2].id);
		if (api_ok(r)) { char m[120]; snprintf(m, sizeof(m), "Forgot %s", name); toast_show(m); }
		else toast_show("Couldn't forget it");
		remote_apply(r);
		if (rem_list.sel >= rem_list.count) rem_list.sel = rem_list.count - 1;
		vlist_refresh(&rem_list);
	} else if (k == K_B || k == K_MENU) {
		back_to_settings(SET_REMOTE);
	}
}

/* ---------- Cloud saves ----------
 * The cloud-sync module: saves sent to the user's own storage after each game, with history; getting
 * them back only on request, with a preview first. WiFi, Bluetooth pairings and passwords are never sent
 * (the module's fixed list). Sign-in services (Google Drive, Dropbox, OneDrive) are added from the phone
 * app, which can paste the sign-in token; WebDAV, SFTP and SMB are added here. */

enum { CL_MAIN, CL_DEVICES, CL_PREVIEW, CL_PLACES, CL_TYPES };
static struct {
	int mode, installed, ready, busy, job_ok;
	char remote[80], device[64], last_push[32], last_error[200], pending[4], job[24];
	char places[16][48], place_types[16][24];
	int nplaces;
	char lines[120][120];          /* devices, or the preview */
	int nlines;
	char from[64];                 /* the MiSTer a restore comes from */
	char add_type[16], add_addr[200], add_user[80], add_folder[80];
	long long seen_at;
} cl;
static vlist_t cl_list;
static lv_obj_t *cl_head, *cl_sub;
static const char *const CL_HINTS[] = { "A", "Choose", "B", "Back", NULL };
static const char *const CL_PREVIEW_HINTS[] = { "A", "Get them back", "B", "Cancel", NULL };
static const char *const CL_NOTHING_HINTS[] = { "A", "OK", NULL };
static const char *const CL_PLACES_HINTS[] = { "A", "Use", "B", "Back", NULL };
static const char *const CL_KINDS[][2] = { { "webdav", "WebDAV (NAS, Nextcloud)" }, { "sftp", "SFTP (a PC, NAS or server)" },
                                          { "smb", "Windows share / NAS (SMB)" }, { "drive", "Google Drive, Dropbox, OneDrive" } };
#define CL_NTYPES 4

static void cl_split_output(const char *out)
{
	cl.nlines = 0;
	const char *p = out;
	while (p && *p && cl.nlines < 120) {
		const char *e = strchr(p, '\n');
		size_t n = e ? (size_t)(e - p) : strlen(p);
		if (n) {
			const char *q = p;
			while (n && *q == ' ') { q++; n--; }
			snprintf(cl.lines[cl.nlines++], sizeof(cl.lines[0]), "%.*s", (int)(n < 119 ? n : 119), q);
		}
		p = e ? e + 1 : NULL;
	}
}

static void cl_apply(cJSON *r)
{
	if (!api_ok(r)) {
		if (cl_head) lv_label_set_text(cl_head, r && api_str(r, "error") ? api_str(r, "error") : "phosphord isn't answering");
		cJSON_Delete(r);
		return;
	}
	cJSON *st = cJSON_GetObjectItem(r, "status"), *j = cJSON_GetObjectItem(r, "job");
	cl.installed = cJSON_IsTrue(cJSON_GetObjectItem(r, "installed"));
	const char *rcl = api_str(st, "rclone");
	cl.ready = rcl && *rcl;
	snprintf(cl.remote, sizeof(cl.remote), "%s", api_str(st, "remote") ? api_str(st, "remote") : "");
	snprintf(cl.device, sizeof(cl.device), "%s", api_str(st, "device") ? api_str(st, "device") : "");
	snprintf(cl.last_push, sizeof(cl.last_push), "%s", api_str(st, "last_push") ? api_str(st, "last_push") : "");
	snprintf(cl.last_error, sizeof(cl.last_error), "%s", api_str(st, "last_error") ? api_str(st, "last_error") : "");
	snprintf(cl.pending, sizeof(cl.pending), "%s", api_str(st, "pending") ? api_str(st, "pending") : "0");
	cl.nplaces = 0;
	cJSON *it;
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "remotes")) {
		if (cl.nplaces >= 16) break;
		snprintf(cl.places[cl.nplaces], sizeof(cl.places[0]), "%s", api_str(it, "name") ? api_str(it, "name") : "");
		snprintf(cl.place_types[cl.nplaces], sizeof(cl.place_types[0]), "%s", api_str(it, "type") ? api_str(it, "type") : "");
		cl.nplaces++;
	}
	int was_busy = cl.busy;
	cl.busy = cJSON_IsTrue(cJSON_GetObjectItem(j, "busy"));
	cl.job_ok = cJSON_IsTrue(cJSON_GetObjectItem(j, "ok"));
	snprintf(cl.job, sizeof(cl.job), "%s", api_str(j, "name") ? api_str(j, "name") : "");
	long long at = (long long)api_num(j, "at", 0);
	const char *out = api_str(j, "output") ? api_str(j, "output") : "";
	/* a job just finished: say how it went, and show what it found */
	if (was_busy && !cl.busy && at != cl.seen_at) {
		cl.seen_at = at;
		char first[160] = "";
		const char *nl = strchr(out, '\n');
		snprintf(first, sizeof(first), "%.*s", (int)(nl ? (size_t)(nl - out) : strlen(out)), out);
		if (!strcmp(cl.job, "devices") && cl.job_ok) { cl_split_output(out); cl.mode = CL_DEVICES; cl_list.sel = 0; }
		else if (!strcmp(cl.job, "preview")) { cl_split_output(out); cl.mode = CL_PREVIEW; cl_list.sel = 0; }
		else if (!strcmp(cl.job, "push")) toast_show(cl.job_ok ? "Saves sent" : "Sending failed");
		else if (!strcmp(cl.job, "pull")) toast_show(cl.job_ok ? "Saves are back" : "Couldn't get them back");
		else if (!strcmp(cl.job, "test")) toast_show(cl.job_ok ? "Connection OK" : first);
		else if (!strcmp(cl.job, "setup")) toast_show(cl.job_ok ? "rclone is ready" : first);
		else if (!strcmp(cl.job, "use")) toast_show(cl.job_ok ? "Storage place chosen" : first);
		else if (!strcmp(cl.job, "add")) {
			if (cl.job_ok) {           /* added: use it straight away */
				cJSON *q = cJSON_CreateObject();
				cJSON_AddStringToObject(q, "cmd", "cloud");
				cJSON_AddStringToObject(q, "do", "use");
				cJSON_AddStringToObject(q, "name", cl.add_type);
				if (cl.add_folder[0]) cJSON_AddStringToObject(q, "folder", cl.add_folder);
				char *b = cJSON_PrintUnformatted(q);
				cJSON_Delete(q);
				cJSON_Delete(r);
				cJSON *u = api_call("%s", b);
				free(b);
				cl.busy = 1;
				cl_apply(u);
				return;
			}
			toast_show(first[0] ? first : "Couldn't add it");
		}
	}
	cJSON_Delete(r);
	if (!cl_head) return;
	char h[200], sub[260];
	if (!cl.installed) {
		snprintf(h, sizeof(h), "Not installed");
		snprintf(sub, sizeof(sub), "Install the cloud-sync module: /media/fat/phosphor/modules/cloud-sync/install.sh (in the Phosphor release).");
	} else if (!cl.ready) {
		snprintf(h, sizeof(h), "Needs rclone");
		snprintf(sub, sizeof(sub), "Download it once (about 25 MB, checked against rclone's checksums).");
	} else if (!cl.remote[0]) {
		snprintf(h, sizeof(h), "No storage place yet");
		snprintf(sub, sizeof(sub), "Choose where saves go: your NAS, a PC, or a cloud drive (from the phone app).");
	} else {
		snprintf(h, sizeof(h), "Sending to %s  \xC2\xB7  %s", cl.remote, cl.device);
		if (cl.last_error[0]) snprintf(sub, sizeof(sub), "%s", cl.last_error);
		else snprintf(sub, sizeof(sub), "%s%s  \xC2\xB7  WiFi, Bluetooth pairings and passwords are never sent",
		              cl.last_push[0] ? "Last sent " : "Not sent yet", cl.last_push);
	}
	if (cl.busy) {
		static const char *const W[][2] = { { "setup", "Downloading rclone..." }, { "push", "Sending..." }, { "pull", "Getting saves back..." },
		                                    { "preview", "Checking what would come back..." }, { "test", "Checking the connection..." },
		                                    { "devices", "Looking in the cloud..." }, { "add", "Adding..." }, { "use", "Choosing..." } };
		for (size_t i = 0; i < sizeof(W) / sizeof(W[0]); i++) if (!strcmp(cl.job, W[i][0])) snprintf(sub, sizeof(sub), "%s", W[i][1]);
	}
	if (!cl.busy) switch (cl.mode) {          /* sub-pages say what they're for */
	case CL_DEVICES: snprintf(sub, sizeof(sub), "Whose saves? Each MiSTer has its own copy in the cloud."); break;
	case CL_PREVIEW: snprintf(sub, sizeof(sub), "From %s. Newer files win; anything replaced here is kept in phosphor/backup/cloud-sync.", cl.from); break;
	case CL_PLACES: snprintf(sub, sizeof(sub), "Where saves go."); break;
	case CL_TYPES: snprintf(sub, sizeof(sub), "Add a storage place. Passwords are kept on this MiSTer only."); break;
	default: break;
	}
	lv_label_set_text(cl_head, h);
	lv_label_set_text(cl_sub, sub);
	switch (cl.mode) {
	case CL_DEVICES: case CL_PREVIEW: cl_list.count = cl.nlines; break;
	case CL_PLACES: cl_list.count = cl.nplaces + 1; break;
	case CL_TYPES: cl_list.count = CL_NTYPES; break;
	default: cl_list.count = !cl.installed ? 0 : !cl.ready ? 1 : 4; break;
	}
	footer_set(cl.mode == CL_PREVIEW ? (cl.nlines && strncmp(cl.lines[0], "Nothing", 7) ? CL_PREVIEW_HINTS : CL_NOTHING_HINTS) : cl.mode == CL_PLACES ? CL_PLACES_HINTS : CL_HINTS);
	vlist_refresh(&cl_list);
}

static void cl_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	switch (cl.mode) {
	case CL_DEVICES: {
		char *mark = strstr(cl.lines[i], "  (this MiSTer)");
		snprintf(t, n, "%.*s", mark ? (int)(mark - cl.lines[i]) : (int)strlen(cl.lines[i]), cl.lines[i]);
		snprintf(val, vn, "%s", mark ? "this MiSTer" : "another MiSTer");
		return;
	}
	case CL_PREVIEW:
		snprintf(t, n, "%s", cl.lines[i]);
		*dim = cl.lines[i][0] != 0 && strchr(cl.lines[i], ':') == NULL;
		return;
	case CL_PLACES:
		if (i == cl.nplaces) { snprintf(t, n, "Add a storage place..."); snprintf(val, vn, ">"); return; }
		snprintf(t, n, "%s", cl.places[i]);
		snprintf(val, vn, "%s%s", cl.place_types[i], !strncmp(cl.remote, cl.places[i], strlen(cl.places[i])) && cl.remote[strlen(cl.places[i])] == ':' ? "  \xC2\xB7  in use" : "");
		return;
	case CL_TYPES:
		snprintf(t, n, "%s", CL_KINDS[i][1]);
		snprintf(val, vn, i == 3 ? "from the phone app" : ">");
		*dim = i == 3;
		return;
	}
	if (!cl.ready) { snprintf(t, n, "Download rclone"); snprintf(val, vn, ">"); return; }
	*dim = !cl.remote[0] && i != 2;
	switch (i) {
	case 0: snprintf(t, n, "Send now"); snprintf(val, vn, "%s", cl.pending[0] == '1' ? "waiting to send" : ""); break;
	case 1: snprintf(t, n, "Get saves back"); snprintf(val, vn, ">"); break;
	case 2: snprintf(t, n, "Storage place"); snprintf(val, vn, "%s", cl.remote[0] ? cl.remote : "Choose >"); break;
	default: snprintf(t, n, "Check the connection"); snprintf(val, vn, ">"); break;
	}
}

static void cl_do(const char *what, const char *from)
{
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "cloud");
	cJSON_AddStringToObject(q, "do", what);
	if (from && *from) cJSON_AddStringToObject(q, "from", from);
	char *b = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = api_call("%s", b);
	free(b);
	if (!api_ok(r)) { toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't start it"); cJSON_Delete(r); return; }
	cl.busy = 1;
	cl_apply(r);
}

static void show_cloud(void);

static void cl_add_send(const char *pass)
{
	if (cur != SCR_CLOUD) return;
	if (!pass) { footer_set(CL_HINTS); return; }
	cJSON *q = cJSON_CreateObject(), *set;
	cJSON_AddStringToObject(q, "cmd", "cloud");
	cJSON_AddStringToObject(q, "do", "add");
	cJSON_AddStringToObject(q, "name", cl.add_type);
	cJSON_AddStringToObject(q, "type", cl.add_type);
	set = cJSON_AddObjectToObject(q, "settings");
	if (!strcmp(cl.add_type, "webdav")) { cJSON_AddStringToObject(set, "url", cl.add_addr); cJSON_AddStringToObject(set, "vendor", "other"); }
	else cJSON_AddStringToObject(set, "host", cl.add_addr);
	if (cl.add_user[0]) cJSON_AddStringToObject(set, "user", cl.add_user);
	if (*pass) cJSON_AddStringToObject(set, "pass", pass);
	char *b = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = api_call("%s", b);
	memset(b, 0, strlen(b));          /* the password */
	free(b);
	cl.mode = CL_MAIN;
	cl_list.sel = 2;
	if (!api_ok(r)) { toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't add it"); cJSON_Delete(r); footer_set(CL_HINTS); return; }
	cl.busy = 1;
	cl_apply(r);
}

static void cl_add_user(const char *u)
{
	if (cur != SCR_CLOUD || !u) { footer_set(CL_HINTS); return; }
	snprintf(cl.add_user, sizeof(cl.add_user), "%s", u);
	osk_open("Password", "Its password (leave empty if there's none), then Done.", 1, 64, cl_add_send, CL_HINTS);
}

static void cl_add_folder(const char *f)
{
	if (cur != SCR_CLOUD || !f || !*f) { footer_set(CL_HINTS); return; }
	snprintf(cl.add_folder, sizeof(cl.add_folder), "%s/Phosphor", f);
	osk_open("User name", "The user name for the share.", 0, 64, cl_add_user, CL_HINTS);
}

static void cl_add_addr(const char *a)
{
	if (cur != SCR_CLOUD || !a || !*a) { footer_set(CL_HINTS); return; }
	snprintf(cl.add_addr, sizeof(cl.add_addr), "%s", a);
	if (!strcmp(cl.add_type, "smb")) osk_open("Share name", "The shared folder's name, e.g. Backups", 0, 64, cl_add_folder, CL_HINTS);
	else osk_open("User name", "The user name for it.", 0, 64, cl_add_user, CL_HINTS);
}

static void show_cloud(void)
{
	cur = SCR_CLOUD;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Cloud saves");
	cl_head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_obj_set_pos(cl_head, S(56), S(178));
	cl_sub = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(cl_sub, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(cl_sub, W - S(112), S(22));
	lv_obj_set_pos(cl_sub, S(56), S(208));
	vlist_create(&cl_list, body, S(56), S(244), S(900), S(380));
	cl_list.fill = cl_fill;
	cl_list.sel = 0;
	cl_list.top = 0;
	cl_list.count = 0;
	cl.mode = CL_MAIN;
	cl_apply(api_call("{\"cmd\":\"cloud\"}"));
}

static void cloud_key(pkey_t k)
{
	if (vlist_key(&cl_list, k)) return;
	int i = cl_list.sel;
	if (k == K_B || k == K_MENU) {
		if (cl.mode != CL_MAIN) {
			cl.mode = cl.mode == CL_TYPES ? CL_PLACES : CL_MAIN;
			cl_list.sel = cl.mode == CL_PLACES ? cl.nplaces : 0;
			cl_list.top = 0;
			cl_apply(api_call("{\"cmd\":\"cloud\"}"));
			return;
		}
		back_to_settings(SET_CLOUD);
		return;
	}
	if (k != K_A) return;
	if (cl.busy) { toast_show("Busy: wait for it to finish"); return; }
	switch (cl.mode) {
	case CL_DEVICES:
		if (i < cl.nlines) {
			char *mark = strstr(cl.lines[i], "  (this MiSTer)");
			snprintf(cl.from, sizeof(cl.from), "%.*s", mark ? (int)(mark - cl.lines[i]) : (int)strlen(cl.lines[i]), cl.lines[i]);
			cl_do("preview", cl.from);
		}
		return;
	case CL_PREVIEW:
		if (cl.nlines && strncmp(cl.lines[0], "Nothing", 7)) { cl.mode = CL_MAIN; cl_list.sel = 1; cl_do("pull", cl.from); }
		else { cl.mode = CL_MAIN; cl_apply(api_call("{\"cmd\":\"cloud\"}")); }
		return;
	case CL_PLACES:
		if (i == cl.nplaces) { cl.mode = CL_TYPES; cl_list.sel = 0; cl_apply(api_call("{\"cmd\":\"cloud\"}")); return; }
		{
			cJSON *r = api_call("{\"cmd\":\"cloud\",\"do\":\"use\",\"name\":\"%s\"}", cl.places[i]);
			cl.mode = CL_MAIN;
			cl_list.sel = 2;
			if (api_ok(r)) cl.busy = 1;
			cl_apply(r);
		}
		return;
	case CL_TYPES:
		if (i == 3) { toast_show("Add Google Drive, Dropbox or OneDrive from the Phosphor app (it needs a sign-in)"); return; }
		snprintf(cl.add_type, sizeof(cl.add_type), "%s", CL_KINDS[i][0]);
		cl.add_folder[0] = 0;
		cl.add_user[0] = 0;
		osk_open(i == 0 ? "WebDAV address" : "Address", i == 0 ? "e.g. https://nas.local/remote.php/dav/files/me" : "The computer or NAS, e.g. 192.168.1.5",
		         0, 190, cl_add_addr, CL_HINTS);
		return;
	}
	if (!cl.installed) return;
	if (!cl.ready) { cl_do("setup", NULL); return; }
	switch (i) {
	case 0: if (cl.remote[0]) cl_do("push", NULL); else toast_show("Choose a storage place first"); break;
	case 1: if (cl.remote[0]) cl_do("devices", NULL); else toast_show("Choose a storage place first"); break;
	case 2: cl.mode = CL_PLACES; cl_list.sel = 0; cl_list.top = 0; cl_apply(api_call("{\"cmd\":\"cloud\"}")); break;
	default: if (cl.remote[0]) cl_do("test", NULL); else toast_show("Choose a storage place first"); break;
	}
}

/* ---------- Backups ----------
 * The backup-agent module's snapshots (saves and settings, kept on the SD card, NVMe or NAS). A snapshot
 * shows what it would put back; A restores only saves and save states, X everything in it (settings
 * too). Every restore takes a safety snapshot first, so it can be undone from this list. */

enum { BK_MAIN, BK_PREVIEW };
static struct {
	int mode, installed, busy, job_ok;
	char job[24], dest[200], last_error[200];
	char names[60][64], when[60][20], reason[60][40];
	int kb[60], nsnaps;
	char lines[150][120];
	int nlines;
	char pick[64], pick_label[80];
	long long seen_at;
} bk;
static vlist_t bk_list;
static lv_obj_t *bk_head, *bk_sub;
static const char *const BK_HINTS[] = { "A", "Choose", "B", "Back", NULL };
static const char *const BK_PREVIEW_HINTS[] = { "A", "Restore saves", "X", "Everything", "B", "Cancel", NULL };

static void bk_reason(const char *r, char *out, size_t n)
{
	if (!strcmp(r, "manual")) snprintf(out, n, "Backed up by hand");
	else if (!strcmp(r, "daily")) snprintf(out, n, "Daily");
	else if (!strcmp(r, "pre-restore")) snprintf(out, n, "Before a restore");
	else if (!strncmp(r, "exit-", 5)) snprintf(out, n, "After %s", r + 5);
	else snprintf(out, n, "%s", r);
}

static void bk_apply(cJSON *r)
{
	if (!api_ok(r)) {
		if (bk_head) lv_label_set_text(bk_head, r && api_str(r, "error") ? api_str(r, "error") : "phosphord isn't answering");
		cJSON_Delete(r);
		return;
	}
	cJSON *st = cJSON_GetObjectItem(r, "status"), *j = cJSON_GetObjectItem(r, "job"), *it;
	bk.installed = cJSON_IsTrue(cJSON_GetObjectItem(r, "installed"));
	snprintf(bk.dest, sizeof(bk.dest), "%s", api_str(st, "dest") ? api_str(st, "dest") : "");
	snprintf(bk.last_error, sizeof(bk.last_error), "%s", api_str(st, "last_error") ? api_str(st, "last_error") : "");
	bk.nsnaps = 0;
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "snapshots")) {
		if (bk.nsnaps >= 60) break;
		int i = bk.nsnaps++;
		snprintf(bk.names[i], sizeof(bk.names[0]), "%s", api_str(it, "name") ? api_str(it, "name") : "");
		snprintf(bk.when[i], sizeof(bk.when[0]), "%s", api_str(it, "when") ? api_str(it, "when") : "");
		bk_reason(api_str(it, "reason") ? api_str(it, "reason") : "", bk.reason[i], sizeof(bk.reason[0]));
		bk.kb[i] = (int)api_num(it, "kb", 0);
	}
	int was = bk.busy;
	bk.busy = cJSON_IsTrue(cJSON_GetObjectItem(j, "busy"));
	bk.job_ok = cJSON_IsTrue(cJSON_GetObjectItem(j, "ok"));
	snprintf(bk.job, sizeof(bk.job), "%s", api_str(j, "name") ? api_str(j, "name") : "");
	long long at = (long long)api_num(j, "at", 0);
	const char *out = api_str(j, "output") ? api_str(j, "output") : "";
	if (was && !bk.busy && at != bk.seen_at) {
		bk.seen_at = at;
		if (!strcmp(bk.job, "preview")) {
			bk.nlines = 0;
			for (const char *p = out; p && *p && bk.nlines < 150;) {
				const char *e = strchr(p, '\n');
				size_t n = e ? (size_t)(e - p) : strlen(p);
				/* files only (folders end in /), and not the "Would restore into ..." line */
				if (n && p[n - 1] != '/' && strncmp(p, "Would restore", 13))
					snprintf(bk.lines[bk.nlines++], sizeof(bk.lines[0]), "%.*s", (int)(n < 119 ? n : 119), p);
				p = e ? e + 1 : NULL;
			}
			if (!bk.job_ok && bk.nlines == 0) snprintf(bk.lines[bk.nlines++], sizeof(bk.lines[0]), "No saves in this backup");
			bk.mode = BK_PREVIEW;
			bk_list.sel = 0;
			bk_list.top = 0;
		} else if (!strcmp(bk.job, "now")) toast_show(bk.job_ok ? "Backed up" : "Backup failed");
		else if (!strcmp(bk.job, "restore")) toast_show(bk.job_ok ? "Restored (the backup made just before it can undo it)" : "Restore failed: nothing changed");
	}
	cJSON_Delete(r);
	if (!bk_head) return;
	char h[240], sub[260];
	if (!bk.installed) {
		snprintf(h, sizeof(h), "Not installed");
		snprintf(sub, sizeof(sub), "Install the backup-agent module (modules/backup-agent/install.sh in the Phosphor release).");
	} else {
		snprintf(h, sizeof(h), "%d backup%s  \xC2\xB7  %s", bk.nsnaps, bk.nsnaps == 1 ? "" : "s", bk.dest);
		if (bk.last_error[0]) snprintf(sub, sizeof(sub), "%s", bk.last_error);
		else snprintf(sub, sizeof(sub), "Taken when you leave a game and once a day. They stay on this drive, never in the cloud.");
	}
	if (bk.mode == BK_PREVIEW) snprintf(sub, sizeof(sub), "%s: A puts back the saves only; X everything in it, settings too.", bk.pick_label);
	if (bk.busy) snprintf(sub, sizeof(sub), "%s", !strcmp(bk.job, "now") ? "Backing up..." : !strcmp(bk.job, "restore") ? "Restoring..." : "Looking inside...");
	lv_label_set_text(bk_head, h);
	lv_label_set_text(bk_sub, sub);
	bk_list.count = bk.mode == BK_PREVIEW ? bk.nlines : bk.installed ? bk.nsnaps + 1 : 0;
	footer_set(bk.mode == BK_PREVIEW ? BK_PREVIEW_HINTS : BK_HINTS);
	vlist_refresh(&bk_list);
}

static void bk_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	if (bk.mode == BK_PREVIEW) { snprintf(t, n, "%s", bk.lines[i]); return; }
	if (i == 0) { snprintf(t, n, "Back up now"); snprintf(val, vn, ">"); return; }
	i--;
	snprintf(t, n, "%s", bk.when[i]);
	snprintf(val, vn, "%s  \xC2\xB7  %d KB", bk.reason[i], bk.kb[i]);
	*dim = !strcmp(bk.reason[i], "Before a restore");
}

static void bk_do(const char *what, const char *name, const char *only)
{
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "backup");
	cJSON_AddStringToObject(q, "do", what);
	if (name) cJSON_AddStringToObject(q, "name", name);
	if (only) cJSON_AddStringToObject(q, "only", only);
	char *b = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = api_call("%s", b);
	free(b);
	if (!api_ok(r)) { toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't start it"); cJSON_Delete(r); return; }
	bk.busy = 1;
	bk_apply(r);
}

static void show_backup(void)
{
	cur = SCR_BACKUP;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Backups");
	bk_head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_label_set_long_mode(bk_head, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(bk_head, W - S(112), S(24));
	lv_obj_set_pos(bk_head, S(56), S(178));
	bk_sub = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(bk_sub, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(bk_sub, W - S(112), S(22));
	lv_obj_set_pos(bk_sub, S(56), S(208));
	vlist_create(&bk_list, body, S(56), S(244), S(900), S(380));
	bk_list.fill = bk_fill;
	bk_list.sel = 0;
	bk_list.top = 0;
	bk_list.count = 0;
	bk.mode = BK_MAIN;
	bk_apply(api_call("{\"cmd\":\"backup\"}"));
}

static void backup_key(pkey_t k)
{
	if (vlist_key(&bk_list, k)) return;
	if (k == K_B || k == K_MENU) {
		if (bk.mode == BK_PREVIEW) { bk.mode = BK_MAIN; bk_list.sel = 0; bk_apply(api_call("{\"cmd\":\"backup\"}")); return; }
		back_to_settings(SET_BACKUP);
		return;
	}
	if (bk.busy) { if (k == K_A || k == K_X) toast_show("Busy: wait for it to finish"); return; }
	if (bk.mode == BK_PREVIEW) {
		if (k == K_A) { bk.mode = BK_MAIN; bk_list.sel = 0; bk_do("restore", bk.pick, "saves|savestates"); }
		else if (k == K_X) { bk.mode = BK_MAIN; bk_list.sel = 0; bk_do("restore", bk.pick, NULL); }
		return;
	}
	if (k != K_A || !bk.installed) return;
	if (bk_list.sel == 0) { bk_do("now", NULL, NULL); return; }
	int i = bk_list.sel - 1;
	if (i < bk.nsnaps) {
		snprintf(bk.pick, sizeof(bk.pick), "%s", bk.names[i]);
		snprintf(bk.pick_label, sizeof(bk.pick_label), "%s (%s)", bk.when[i], bk.reason[i]);
		bk_do("preview", bk.pick, "saves|savestates");
	}
}

/* ---------- Controllers & memory cards ----------
 * The front ports through phosphord (phosphor-main + the Phosphor menu core): both controllers live
 * (buttons, sticks, digital/analog), memory card backups (whole card to phosphor/memcards) and
 * putting a backup back on a card (phosphord backs the card up first). Opening the screen loads
 * the Phosphor menu core if the stock one is loaded (Main restarts: a few seconds). */

enum { PT_MAIN, PT_CONFIRM };
enum { PT_BACKUP1, PT_BACKUP2, PT_ANALOG, PT_INPUT, PT_FIRST_FILE };
static struct {
	int mode, busy, job_ok, available, can_load, main_api, in_game, done, total, loading;
	char job[16], message[200];
	char names[80][64];
	long long at[80];
	int nfiles;
	char pick[64];
	char pad[2][160];
	int pad_id[2], pad_conn[2];
} pt;
static vlist_t pt_list;
static lv_obj_t *pt_head, *pt_padl[2], *pt_sub;
static const char *const PT_HINTS[] = { "A", "Choose", "B", "Back", NULL };
static const char *const PT_CONFIRM_HINTS[] = { "A", "Port 1", "X", "Port 2", "B", "Cancel", NULL };

static const char *pt_type(int id)
{
	switch (id) {
	case 0x41: return "Digital controller";
	case 0x73: return "DualShock (analog)";
	case 0x53: return "Analog joystick";
	case 0x23: return "neGcon";
	case 0x12: return "Mouse";
	case 0x63: return "GunCon";
	case 0xF3: return "Controller (setting up)";
	default: return "Controller";
	}
}

static void pt_pads(void)
{
	cJSON *r = api_call("{\"cmd\":\"pads\"}"), *it;
	static const char *const BTN[16] = { "Select", "L3", "R3", "Start", "Up", "Right", "Down", "Left",
		"L2", "R2", "L1", "R1", "Triangle", "Circle", "Cross", "Square" };
	if (!api_ok(r)) {
		for (int p = 0; p < 2; p++) { pt.pad_conn[p] = 0; snprintf(pt.pad[p], sizeof(pt.pad[0]), "Port %d", p + 1); }
		cJSON_Delete(r);
	} else {
		cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "pads")) {
			int p = (int)api_num(it, "port", 0) - 1;
			if (p < 0 || p > 1) continue;
			pt.pad_conn[p] = cJSON_IsTrue(cJSON_GetObjectItem(it, "connected"));
			pt.pad_id[p] = (int)api_num(it, "id", 0);
			char *o = pt.pad[p];
			size_t n = sizeof(pt.pad[0]), k;
			if (!pt.pad_conn[p]) { snprintf(o, n, "Port %d  \xC2\xB7  nothing plugged in", p + 1); continue; }
			k = (size_t)snprintf(o, n, "Port %d  \xC2\xB7  %s", p + 1, pt_type(pt.pad_id[p]));
			int b = (int)api_num(it, "buttons", 0), any = 0;
			for (int i = 0; i < 16 && k < n - 12; i++)
				if (b & (1 << i)) k += (size_t)snprintf(o + k, n - k, "%s%s", any++ ? " " : "   ", BTN[i]);
			if (pt.pad_id[p] == 0x73 || pt.pad_id[p] == 0x53)
				snprintf(o + k, n - k, "   L %d,%d  R %d,%d", (int)api_num(it, "lx", 0), (int)api_num(it, "ly", 0),
					(int)api_num(it, "rx", 0), (int)api_num(it, "ry", 0));
		}
		cJSON_Delete(r);
	}
	for (int p = 0; p < 2; p++) if (pt_padl[p]) {
		lv_label_set_text(pt_padl[p], pt.pad[p]);
		lv_obj_set_style_text_color(pt_padl[p], pt.pad_conn[p] ? PAL.text : PAL.dim, 0);
	}
}

static void pt_label_for(const char *name, char *out, size_t n)
{
	int port, y, mo, d, h, mi;
	if (sscanf(name, "port%d-%4d%2d%2d-%2d%2d", &port, &y, &mo, &d, &h, &mi) == 6)
		snprintf(out, n, "Port %d card  \xC2\xB7  %04d-%02d-%02d %02d:%02d", port, y, mo, d, h, mi);
	else snprintf(out, n, "%s", name);
}

static void pt_apply(cJSON *r)
{
	if (!api_ok(r)) {
		if (pt_head) lv_label_set_text(pt_head, r && api_str(r, "error") ? api_str(r, "error") : "phosphord isn't answering");
		cJSON_Delete(r);
		return;
	}
	cJSON *p = cJSON_GetObjectItem(r, "ports"), *j = cJSON_GetObjectItem(r, "job"), *it;
	pt.main_api = cJSON_IsTrue(cJSON_GetObjectItem(p, "main_api"));
	pt.available = cJSON_IsTrue(cJSON_GetObjectItem(p, "available"));
	pt.can_load = cJSON_IsTrue(cJSON_GetObjectItem(p, "can_load"));
	pt.in_game = cJSON_IsTrue(cJSON_GetObjectItem(p, "in_game"));
	int was = pt.busy;
	pt.busy = cJSON_IsTrue(cJSON_GetObjectItem(j, "busy"));
	pt.job_ok = cJSON_IsTrue(cJSON_GetObjectItem(j, "ok"));
	pt.done = (int)api_num(j, "done", 0);
	pt.total = (int)api_num(j, "total", 1);
	snprintf(pt.job, sizeof(pt.job), "%s", api_str(j, "name") ? api_str(j, "name") : "");
	snprintf(pt.message, sizeof(pt.message), "%s", api_str(j, "message") ? api_str(j, "message") : "");
	pt.nfiles = 0;
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "backups")) {
		if (pt.nfiles >= 80) break;
		snprintf(pt.names[pt.nfiles], sizeof(pt.names[0]), "%s", api_str(it, "name") ? api_str(it, "name") : "");
		pt.at[pt.nfiles++] = (long long)api_num(it, "at", 0);
	}
	cJSON_Delete(r);
	if (was && !pt.busy) {
		if (!strcmp(pt.job, "prepare")) pt.loading = 0;
		if (!strcmp(pt.job, "prepare") && pt.job_ok) pt.available = 1;
		if (strcmp(pt.job, "prepare") || !pt.job_ok) toast_show(pt.message[0] ? pt.message : pt.job_ok ? "Done" : "That didn't work");
	}
	if (!pt_head) return;
	char h[240], sub[260];
	if (!pt.main_api) {
		snprintf(h, sizeof(h), "Needs Phosphor's Main");
		snprintf(sub, sizeof(sub), "The front ports are read through phosphor-main, which isn't running.");
	} else if (pt.in_game) {
		snprintf(h, sizeof(h), "In a game");
		snprintf(sub, sizeof(sub), "The front ports are read here only from the menu.");
	} else if (!pt.available && !pt.busy) {
		snprintf(h, sizeof(h), pt.can_load ? "Front ports off" : "The Phosphor menu core isn't installed");
		snprintf(sub, sizeof(sub), pt.can_load ? "A on any line loads the Phosphor menu core (a few seconds)." : "It comes with the Phosphor release (phosphor/cores/menu_phosphor.rbf).");
	} else {
		snprintf(h, sizeof(h), "Front ports  \xC2\xB7  %d memory card backup%s", pt.nfiles, pt.nfiles == 1 ? "" : "s");
		snprintf(sub, sizeof(sub), "Backups stay in phosphor/memcards on this SD card. MT32-pi and MIDI are off while the front ports are in use.");
	}
	if (pt.mode == PT_CONFIRM) {
		char lab[100];
		pt_label_for(pt.pick, lab, sizeof(lab));
		snprintf(sub, sizeof(sub), "Put %s on the card in port 1 (A) or port 2 (X)? The card is backed up first.", lab);
	}
	if (pt.busy) {
		int pc = pt.total > 0 ? pt.done * 100 / pt.total : 0;
		if (!strcmp(pt.job, "prepare")) snprintf(sub, sizeof(sub), "Loading the Phosphor menu core...");
		else if (!strcmp(pt.job, "backup")) snprintf(sub, sizeof(sub), "Backing up the memory card... %d%%", pc);
		else if (!strcmp(pt.job, "restore")) snprintf(sub, sizeof(sub), "%s... %d%%", pt.done < pt.total / 2 ? "Backing up the card first" : "Writing", pc);
		else snprintf(sub, sizeof(sub), "Working...");
	}
	lv_label_set_text(pt_head, h);
	lv_label_set_text(pt_sub, sub);
	pt_list.count = PT_FIRST_FILE + pt.nfiles;
	footer_set(pt.mode == PT_CONFIRM ? PT_CONFIRM_HINTS : PT_HINTS);
	vlist_refresh(&pt_list);
}

static void pt_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	switch (i) {
	case PT_BACKUP1: case PT_BACKUP2:
		snprintf(t, n, "Back up the memory card in port %d", i == PT_BACKUP1 ? 1 : 2);
		snprintf(val, vn, ">");
		return;
	case PT_INPUT:
		snprintf(t, n, "Use them to move around Phosphor");
		snprintf(val, vn, "<  %s  >", SETTINGS.front_pads ? "On" : "Off");
		return;
	case PT_ANALOG:
		snprintf(t, n, "Switch DualShocks to analog");
		*dim = !((pt.pad_conn[0] && pt.pad_id[0] == 0x41) || (pt.pad_conn[1] && pt.pad_id[1] == 0x41));
		return;
	}
	i -= PT_FIRST_FILE;
	pt_label_for(pt.names[i], t, n);
	snprintf(val, vn, "Put back >");
	*dim = strncmp(pt.names[i], "port", 4) != 0;
}

static void pt_do(const char *what, int port, const char *name)
{
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "memcard");
	cJSON_AddStringToObject(q, "do", what);
	if (port) cJSON_AddNumberToObject(q, "port", port);
	if (name) cJSON_AddStringToObject(q, "name", name);
	char *b = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = api_call("%s", b);
	free(b);
	if (!api_ok(r)) { toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't start it"); cJSON_Delete(r); return; }
	pt.busy = 1;
	pt_apply(r);
}

static void show_ports(void)
{
	cur = SCR_PORTS;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Controllers & memory cards");
	pt_head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_label_set_long_mode(pt_head, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(pt_head, W - S(112), S(24));
	lv_obj_set_pos(pt_head, S(56), S(178));
	for (int p = 0; p < 2; p++) {
		pt_padl[p] = mk_label(body, FONT_BODY, PAL.dim, "");
		lv_label_set_long_mode(pt_padl[p], LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_size(pt_padl[p], W - S(112), S(24));
		lv_obj_set_pos(pt_padl[p], S(56), S(216 + 30 * p));
	}
	pt_sub = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(pt_sub, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(pt_sub, W - S(112), S(22));
	lv_obj_set_pos(pt_sub, S(56), S(282));
	vlist_create(&pt_list, body, S(56), S(316), S(900), S(320));
	pt_list.fill = pt_fill;
	pt_list.sel = 0;
	pt_list.top = 0;
	pt_list.count = 0;
	pt.mode = PT_MAIN;
	pt.loading = 0;
	for (int p = 0; p < 2; p++) { pt.pad_conn[p] = 0; snprintf(pt.pad[p], sizeof(pt.pad[0]), "Port %d", p + 1); }
	pt_apply(api_call("{\"cmd\":\"memcard\"}"));
	if (pt.available) pt_pads();
	else if (pt.can_load && !pt.busy) { pt.loading = 1; pt_do("prepare", 0, NULL); }
	else pt_pads();
}

static void ports_key(pkey_t k)
{
	if (vlist_key(&pt_list, k)) return;
	if (k == K_B || k == K_MENU) {
		if (pt.mode == PT_CONFIRM) { pt.mode = PT_MAIN; pt_apply(api_call("{\"cmd\":\"memcard\"}")); return; }
		back_to_settings(SET_PORTS);
		return;
	}
	if (pt.busy) { if (k == K_A || k == K_X) toast_show("Busy: wait for it to finish"); return; }
	if (pt.mode == PT_CONFIRM) {
		if (k == K_A || k == K_X) { pt.mode = PT_MAIN; pt_do("restore", k == K_A ? 1 : 2, pt.pick); }
		return;
	}
	if (pt_list.sel == PT_INPUT && (k == K_A || k == K_LEFT || k == K_RIGHT)) {
		SETTINGS.front_pads = !SETTINGS.front_pads;
		settings_save();
		vlist_refresh(&pt_list);
		return;
	}
	if (k != K_A) return;
	if (!pt.available) {
		if (pt.can_load) pt_do("prepare", 0, NULL);
		return;
	}
	switch (pt_list.sel) {
	case PT_BACKUP1: pt_do("backup", 1, NULL); return;
	case PT_BACKUP2: pt_do("backup", 2, NULL); return;
	case PT_ANALOG: {
		int any = 0;
		for (int p = 0; p < 2; p++) if (pt.pad_conn[p] && pt.pad_id[p] == 0x41) {
			char q[64];
			snprintf(q, sizeof(q), "{\"cmd\":\"pad_analog\",\"port\":%d}", p + 1);
			cJSON_Delete(api_call("%s", q));
			any = 1;
		}
		toast_show(any ? "Switched (a controller without analog stays digital)" : "No digital DualShock plugged in");
		return;
	}
	}
	int i = pt_list.sel - PT_FIRST_FILE;
	if (i >= 0 && i < pt.nfiles) {
		snprintf(pt.pick, sizeof(pt.pick), "%s", pt.names[i]);
		pt.mode = PT_CONFIRM;
		pt_apply(api_call("{\"cmd\":\"memcard\"}"));
	}
}

/* ---------- Bluetooth ----------
 * Controllers paired with MiSTer's own Bluetooth (phosphord runs MiSTer's btctl): pair a new one
 * (it looks for a controller in pairing mode for up to 90 s), and for each paired one disconnect or
 * forget. Pairings stay on this SD card. */

enum { BT_MAIN, BT_ACTION };
static struct {
	int mode, present, busy, ok, ndev;
	long long at, seen_at;
	char message[160], found[64];
	char mac[24][18], name[24][64];
	int conn[24], input[24];
	int pick;
} bt;
static vlist_t bt_list;
static lv_obj_t *bt_head, *bt_sub;
static const char *const BT_HINTS[] = { "A", "Choose", "B", "Back", NULL };
static const char *const BT_ACTION_HINTS[] = { "A", "Disconnect", "X", "Forget", "B", "Cancel", NULL };

static void bt_apply(cJSON *r)
{
	if (!api_ok(r)) {
		if (bt_head) lv_label_set_text(bt_head, r && api_str(r, "error") ? api_str(r, "error") : "phosphord isn't answering");
		cJSON_Delete(r);
		return;
	}
	cJSON *p = cJSON_GetObjectItem(r, "pairing"), *it;
	bt.present = cJSON_IsTrue(cJSON_GetObjectItem(r, "present"));
	int was = bt.busy;
	bt.busy = cJSON_IsTrue(cJSON_GetObjectItem(p, "busy"));
	bt.ok = cJSON_IsTrue(cJSON_GetObjectItem(p, "ok"));
	bt.at = (long long)api_num(p, "at", 0);
	snprintf(bt.message, sizeof(bt.message), "%s", api_str(p, "message") ? api_str(p, "message") : "");
	snprintf(bt.found, sizeof(bt.found), "%s", api_str(p, "found") ? api_str(p, "found") : "");
	bt.ndev = 0;
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "devices")) {
		if (bt.ndev >= 24) break;
		int i = bt.ndev++;
		snprintf(bt.mac[i], sizeof(bt.mac[0]), "%s", api_str(it, "mac") ? api_str(it, "mac") : "");
		snprintf(bt.name[i], sizeof(bt.name[0]), "%s", api_str(it, "name") ? api_str(it, "name") : "");
		bt.conn[i] = cJSON_IsTrue(cJSON_GetObjectItem(it, "connected"));
		bt.input[i] = cJSON_IsTrue(cJSON_GetObjectItem(it, "input"));
	}
	cJSON_Delete(r);
	if (was && !bt.busy && bt.at != bt.seen_at) { bt.seen_at = bt.at; toast_show(bt.message); }
	if (!bt_head) return;
	int nc = 0;
	for (int i = 0; i < bt.ndev; i++) nc += bt.conn[i];
	char h[200], sub[260];
	if (!bt.present) {
		snprintf(h, sizeof(h), "No Bluetooth adapter");
		snprintf(sub, sizeof(sub), "Plug in a Bluetooth adapter; it shows up here by itself.");
	} else {
		snprintf(h, sizeof(h), "%d paired  \xC2\xB7  %d connected", bt.ndev, nc);
		snprintf(sub, sizeof(sub), "Pairings stay on this SD card.");
	}
	if (bt.busy) {
		size_t ml = strlen(bt.message);
		int dots = ml >= 3 && !strcmp(bt.message + ml - 3, "...");
		if (bt.found[0] && dots) snprintf(sub, sizeof(sub), "%.*s: %s...", (int)(ml - 3), bt.message, bt.found);
		else snprintf(sub, sizeof(sub), "%s", bt.message);
	}
	if (bt.mode == BT_ACTION && bt.pick < bt.ndev)
		snprintf(sub, sizeof(sub), "%s: A disconnects it (it reconnects when you turn it on), X forgets it.", bt.name[bt.pick]);
	lv_label_set_text(bt_head, h);
	lv_label_set_text(bt_sub, sub);
	bt_list.count = bt.present ? 1 + bt.ndev : 0;
	footer_set(bt.mode == BT_ACTION ? BT_ACTION_HINTS : BT_HINTS);
	vlist_refresh(&bt_list);
}

static void bt_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	if (i == 0) {
		snprintf(t, n, "%s", bt.busy ? "Stop looking" : "Pair a controller");
		snprintf(val, vn, "%s", bt.busy ? "Looking..." : ">");
		return;
	}
	i--;
	snprintf(t, n, "%s", bt.name[i]);
	snprintf(val, vn, "%s", bt.conn[i] ? "Connected" : "Not connected");
	*dim = !bt.conn[i];
}

static void bt_do(const char *what, const char *mac)
{
	char q[128];
	if (mac) snprintf(q, sizeof(q), "{\"cmd\":\"bluetooth\",\"do\":\"%s\",\"mac\":\"%s\"}", what, mac);
	else snprintf(q, sizeof(q), "{\"cmd\":\"bluetooth\",\"do\":\"%s\"}", what);
	cJSON *r = api_call("%s", q);
	if (!api_ok(r)) { toast_show(r && api_str(r, "error") ? api_str(r, "error") : "That didn't work"); cJSON_Delete(r); return; }
	if (!strcmp(what, "pair")) bt.busy = 1;
	bt_apply(r);
}

static void show_bt(void)
{
	cur = SCR_BT;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Bluetooth");
	bt_head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_label_set_long_mode(bt_head, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(bt_head, W - S(112), S(24));
	lv_obj_set_pos(bt_head, S(56), S(178));
	bt_sub = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(bt_sub, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(bt_sub, W - S(112), S(22));
	lv_obj_set_pos(bt_sub, S(56), S(208));
	vlist_create(&bt_list, body, S(56), S(244), S(900), S(380));
	bt_list.fill = bt_fill;
	bt_list.sel = 0;
	bt_list.top = 0;
	bt_list.count = 0;
	bt.mode = BT_MAIN;
	bt_apply(api_call("{\"cmd\":\"bluetooth\"}"));
}

static void bt_key(pkey_t k)
{
	if (vlist_key(&bt_list, k)) { if (bt.mode == BT_ACTION) { bt.mode = BT_MAIN; bt_apply(api_call("{\"cmd\":\"bluetooth\"}")); } return; }
	if (k == K_B || k == K_MENU) {
		if (bt.mode == BT_ACTION) { bt.mode = BT_MAIN; bt_apply(api_call("{\"cmd\":\"bluetooth\"}")); return; }
		back_to_settings(SET_BT);
		return;
	}
	if (bt.mode == BT_ACTION) {
		if (bt.pick < bt.ndev && (k == K_A || k == K_X)) {
			char mac[18];
			snprintf(mac, sizeof(mac), "%s", bt.mac[bt.pick]);
			bt.mode = BT_MAIN;
			bt_do(k == K_A ? "disconnect" : "forget", mac);
			if (k == K_X) toast_show("Forgotten: pair it again to use it");
		}
		return;
	}
	if (k != K_A || !bt.present) return;
	if (bt_list.sel == 0) { bt_do(bt.busy ? "cancel" : "pair", NULL); return; }
	if (bt.busy) { toast_show("Busy: wait for pairing or stop it"); return; }
	bt.pick = bt_list.sel - 1;
	bt.mode = BT_ACTION;
	bt_apply(api_call("{\"cmd\":\"bluetooth\"}"));
}

/* ---------- Welcome (first start) ----------
 * Shown once on a new install (no frontend.ini yet): network, box art lookup, accent colour, the phone
 * app and cloud saves, each optional; "Start" opens the library. Settings has all of these later too. */

enum { WL_NET, WL_SCRAPE, WL_ACCENT, WL_PHONE, WL_CLOUD, WL_START, WL_COUNT };
static vlist_t wl_list;
static char wl_net[120];
static int wl_scrape = -1;
static const char *const WL_HINTS[] = { "A", "Choose", "\xE2\x86\x94", "Change", NULL };

static void wl_refresh_net(void)
{
	sysinfo_t si;
	sysinfo_read(&si);
	cJSON *w = api_call("{\"cmd\":\"wifi\"}");
	cJSON *st = w ? cJSON_GetObjectItem(w, "status") : NULL;
	if (st && cJSON_IsTrue(cJSON_GetObjectItem(st, "connected")))
		snprintf(wl_net, sizeof(wl_net), "WiFi: %s", api_str(st, "ssid") ? api_str(st, "ssid") : "connected");
	else if (si.ethernet) snprintf(wl_net, sizeof(wl_net), "Ethernet connected");
	else snprintf(wl_net, sizeof(wl_net), "Not connected");
	cJSON_Delete(w);
	cJSON *sc = api_call("{\"cmd\":\"scrape\"}");
	wl_scrape = sc && api_ok(sc) ? cJSON_IsTrue(cJSON_GetObjectItem(sc, "on")) : -1;
	cJSON_Delete(sc);
}

static void wl_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	switch (i) {
	case WL_NET: snprintf(t, n, "Network"); snprintf(val, vn, "%s  >", wl_net); *dim = 0; break;
	case WL_SCRAPE: snprintf(t, n, "Get box art and details from the internet"); snprintf(val, vn, "<  %s  >", wl_scrape > 0 ? "On" : "Off"); break;
	case WL_ACCENT: snprintf(t, n, "Accent colour"); snprintf(val, vn, "<  %s  >", theme_accent_name(theme_accent())); break;
	case WL_PHONE: snprintf(t, n, "Phosphor app on your phone"); snprintf(val, vn, "optional  >"); break;
	case WL_CLOUD: snprintf(t, n, "Cloud saves"); snprintf(val, vn, "optional  >"); break;
	default: snprintf(t, n, "Start"); snprintf(val, vn, ">"); break;
	}
}

static void show_welcome(void)
{
	cur = SCR_WELCOME;
	in_setup = 1;
	lv_obj_clean(body);
	screen_title("WELCOME", "Let's set up Phosphor");
	lv_obj_t *sub = mk_label(body, FONT_SMALL, PAL.dim, "A few things to choose; all of them can be changed later in Settings.");
	lv_obj_set_pos(sub, S(56), S(186));
	wl_refresh_net();
	int keep = wl_list.sel;
	vlist_create(&wl_list, body, S(56), S(224), S(900), S(380));
	wl_list.fill = wl_fill;
	wl_list.count = WL_COUNT;
	wl_list.sel = keep < WL_COUNT ? keep : 0;
	wl_list.top = 0;
	vlist_refresh(&wl_list);
	footer_set(WL_HINTS);
}

static void welcome_key(pkey_t k)
{
	if (vlist_key(&wl_list, k)) return;
	int i = wl_list.sel;
	int change = k == K_LEFT ? -1 : k == K_RIGHT ? 1 : k == K_A ? 1 : 0;
	if (!change) return;
	switch (i) {
	case WL_NET: if (k == K_A) show_wifi(); break;
	case WL_SCRAPE:
		cJSON_Delete(api_call("{\"cmd\":\"scrape\",\"on\":%s}", wl_scrape > 0 ? "false" : "true"));
		wl_refresh_net();
		vlist_refresh(&wl_list);
		break;
	case WL_ACCENT: {
		int a = ((int)theme_accent() + change + ACCENT_COUNT) % ACCENT_COUNT;
		theme_set_accent((accent_t)a);
		SETTINGS.accent = a;
		settings_save();
		lv_obj_clean(root);
		ui_init(W, H);              /* everything picks up the new accent; ui_init shows the welcome again */
		break;
	}
	case WL_PHONE: if (k == K_A) show_remote(); break;
	case WL_CLOUD: if (k == K_A) show_cloud(); break;
	default:
		if (k != K_A) break;
		SETTINGS.setup_done = 1;
		settings_save();
		in_setup = 0;
		show_home();
		break;
	}
}

/* ---------- file picker ----------
 * For long lists of files in folders (300 filters, 1200 shadow masks): shows one folder at a time,
 * A opens a folder or picks a file, B goes up a folder (or cancels at the top). "Off" comes first. */

static struct {
	char **all;                /* every file, relative path */
	int nall;
	char folder[400];          /* current folder, "" = top */
	char **rows;               /* shown: "Off", folders ("name/"), files */
	int nrows, has_off;
	vlist_t list;
	char title[80];
	void (*done)(const char *path);   /* NULL = cancelled, "" = off */
	screen_t back;
} pk;

static void fp_free_rows(void)
{
	for (int i = 0; i < pk.nrows; i++) free(pk.rows[i]);
	free(pk.rows);
	pk.rows = NULL;
	pk.nrows = 0;
}

static void fp_build(void)
{
	fp_free_rows();
	pk.rows = calloc((size_t)pk.nall + 2, sizeof(char *));
	size_t fl = strlen(pk.folder);
	if (pk.has_off && !fl) pk.rows[pk.nrows++] = strdup("Off");
	/* folders first (each once), then files, both in the list's order */
	for (int pass = 0; pass < 2; pass++)
		for (int i = 0; i < pk.nall; i++) {
			const char *p = pk.all[i];
			if (fl && (strncmp(p, pk.folder, fl) || p[fl] != '/')) continue;
			const char *rest = fl ? p + fl + 1 : p, *slash = strchr(rest, '/');
			if (pass == 0 && slash) {
				char name[300];
				snprintf(name, sizeof(name), "%.*s/", (int)(slash - rest), rest);
				int dup = 0;
				for (int k = 0; k < pk.nrows; k++) if (!strcmp(pk.rows[k], name)) dup = 1;
				if (!dup) pk.rows[pk.nrows++] = strdup(name);
			} else if (pass == 1 && !slash) {
				pk.rows[pk.nrows++] = strdup(rest);
			}
		}
}

static void fp_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	const char *r = pk.rows[i];
	size_t l = strlen(r);
	if (l && r[l - 1] == '/') { snprintf(t, n, "%.*s", (int)(l - 1), r); snprintf(val, vn, ">"); }
	else {
		snprintf(t, n, "%s", r);
		char *dot = strrchr(t, '.');
		if (dot && (!strcmp(dot, ".txt") || !strcmp(dot, ".ini"))) *dot = 0;
		if (pk.has_off && !pk.folder[0] && i == 0) *dim = 1;
	}
}

static void fp_draw(void)
{
	lv_obj_clean(body);
	char label[440];
	snprintf(label, sizeof(label), "%s", pk.folder[0] ? pk.folder : "");
	screen_title(pk.folder[0] ? "FOLDER" : "CHOOSE", pk.title);
	lv_obj_t *f = mk_label(body, FONT_SMALL, PAL.dim, label);
	lv_obj_set_pos(f, S(56), S(178));
	vlist_create(&pk.list, body, S(56), S(208), W - S(112), S(420));
	pk.list.fill = fp_fill;
	pk.list.count = pk.nrows;
	pk.list.sel = 0;
	pk.list.top = 0;
	vlist_refresh(&pk.list);
	static const char *const hints[] = { "A", "Choose", "B", "Back", "L", "Page up", "R", "Page down", NULL };
	footer_set(hints);
}

static void fp_close(const char *result)
{
	for (int i = 0; i < pk.nall; i++) free(pk.all[i]);
	free(pk.all);
	pk.all = NULL;
	pk.nall = 0;
	fp_free_rows();
	void (*cb)(const char *) = pk.done;
	pk.done = NULL;
	if (cb) cb(result);
}

/* kind: filters, masks, gamma, presets (from phosphord) */
static void fp_open(const char *kind, const char *title, int has_off, void (*done)(const char *))
{
	cJSON *r = api_call("{\"cmd\":\"video_files\",\"kind\":\"%s\"}", kind);
	cJSON *arr = cJSON_GetObjectItem(r, "files"), *it;
	int n = cJSON_GetArraySize(arr);
	if (!api_ok(r) || (!n && !has_off)) { cJSON_Delete(r); toast_show("Nothing to choose from on the SD card"); return; }
	pk.all = calloc((size_t)(n ? n : 1), sizeof(char *));
	pk.nall = 0;
	cJSON_ArrayForEach(it, arr) if (cJSON_IsString(it)) pk.all[pk.nall++] = strdup(it->valuestring);
	cJSON_Delete(r);
	snprintf(pk.title, sizeof(pk.title), "%s", title);
	pk.folder[0] = 0;
	pk.has_off = has_off;
	pk.done = done;
	cur = SCR_PICK;
	fp_build();
	fp_draw();
}

static void fp_key(pkey_t k)
{
	if (vlist_key(&pk.list, k)) return;
	if (k == K_A && pk.nrows) {
		const char *r = pk.rows[pk.list.sel];
		size_t l = strlen(r);
		if (pk.has_off && !pk.folder[0] && pk.list.sel == 0) { fp_close(""); return; }
		if (l && r[l - 1] == '/') {
			char nf[400];
			snprintf(nf, sizeof(nf), "%s%s%.*s", pk.folder, pk.folder[0] ? "/" : "", (int)(l - 1), r);
			snprintf(pk.folder, sizeof(pk.folder), "%s", nf);
			fp_build();
			fp_draw();
		} else {
			char full[800];
			snprintf(full, sizeof(full), "%s%s%s", pk.folder, pk.folder[0] ? "/" : "", r);
			fp_close(full);
		}
	} else if (k == K_B) {
		if (!pk.folder[0]) { fp_close(NULL); return; }
		char *sl = strrchr(pk.folder, '/');
		if (sl) *sl = 0;
		else pk.folder[0] = 0;
		fp_build();
		fp_draw();
	}
}

/* ---------- Screen filters ----------
 * Per system: a preset, or the video filter, scanlines, shadow mask and gamma one by one. phosphord
 * writes the files MiSTer's own OSD writes (backed up first); it shows the next time the core starts. */

typedef struct { char sys[24], name[48], summary[200]; } vsys_t;
static vsys_t vsys[40];
static int nvsys;
static vlist_t vsys_list;
static cJSON *vcur;                 /* the selected system's settings */
static char vsel_sys[24];
static vlist_t vrow_list;
enum { VR_PRESET, VR_FILTER, VR_VERT, VR_SCAN, VR_ILACE, VR_MASK, VR_MASKMODE, VR_GAMMA, VR_COUNT };
static const char *VR_KEY[VR_COUNT] = { "", "filter", "vertical", "scanlines", "interlace", "mask", "maskmode", "gamma" };
static const char *VR_NAME[VR_COUNT] = { "Apply a preset", "Video filter", "Vertical filter", "Scanlines filter",
                                         "Interlaced filter", "Shadow mask", "Shadow mask size", "Gamma" };

static const char *short_name(const char *path)
{
	static char b[200];
	const char *s = strrchr(path, '/');
	snprintf(b, sizeof(b), "%s", s ? s + 1 : path);
	char *dot = strrchr(b, '.');
	if (dot) *dot = 0;
	return b;
}

static void video_summary(cJSON *x, char *out, size_t n)
{
	out[0] = 0;
	if (!cJSON_IsTrue(cJSON_GetObjectItem(x, "custom"))) { snprintf(out, n, "MiSTer.ini defaults"); return; }
	const char *keys[] = { "filter", "scanlines", "mask", "gamma" };
	for (int i = 0; i < 4; i++) {
		const char *v = api_str(x, keys[i]);
		if (!v) continue;
		size_t l = strlen(out);
		snprintf(out + l, n - l, "%s%s", l ? "  \xC2\xB7  " : "", short_name(v));
	}
	if (!out[0]) snprintf(out, n, "All off");
}

static void vsys_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	snprintf(t, n, "%s", vsys[i].name);
	snprintf(val, vn, "%s", vsys[i].summary);
	*dim = !strcmp(vsys[i].summary, "MiSTer.ini defaults");
}

static void show_video(void)
{
	cur = SCR_VIDEO;
	cJSON *r = api_call("{\"cmd\":\"video\"}"), *it;
	nvsys = 0;
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "systems")) {
		if (nvsys >= 40) break;
		vsys_t *v = &vsys[nvsys++];
		snprintf(v->sys, sizeof(v->sys), "%s", api_str(it, "system") ? api_str(it, "system") : "");
		snprintf(v->name, sizeof(v->name), "%s", api_str(it, "name") ? api_str(it, "name") : "");
		video_summary(it, v->summary, sizeof(v->summary));
	}
	cJSON_Delete(r);
	lv_obj_clean(body);
	screen_title("SETTINGS", "Screen filters");
	lv_obj_t *note = mk_label(body, FONT_SMALL, PAL.dim, nvsys ? "Per system. Changes show the next time the system's core starts."
	                                                             : "No cores found for your systems.");
	lv_obj_set_pos(note, S(56), S(178));
	vlist_create(&vsys_list, body, S(56), S(208), W - S(112), S(420));
	vsys_list.fill = vsys_fill;
	vsys_list.count = nvsys;
	vsys_list.sel = 0;
	vsys_list.top = 0;
	vlist_refresh(&vsys_list);
	static const char *const hints[] = { "A", "Open", "B", "Back", NULL };
	footer_set(hints);
}

static void vrow_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	snprintf(t, n, "%s", VR_NAME[i]);
	if (i == VR_PRESET) { snprintf(val, vn, ">"); return; }
	const char *v = vcur ? api_str(vcur, VR_KEY[i]) : NULL;
	if (i == VR_MASKMODE) {
		int off = !v || !strcmp(v, "off");
		snprintf(val, vn, "<  %s  >", off ? "Off" : v);
		*dim = off;
		return;
	}
	snprintf(val, vn, "%s", v ? short_name(v) : "Off");
	*dim = !v;
}

static void show_video_sys(void);

static void video_result(cJSON *r)
{
	if (api_ok(r)) {
		cJSON_Delete(vcur);
		vcur = cJSON_DetachItemFromObject(r, "video");
	} else {
		toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't change it");
	}
	cJSON_Delete(r);
}

static void video_send(const char *key, const char *value)
{
	cJSON *req = cJSON_CreateObject(), *set;
	cJSON_AddStringToObject(req, "cmd", "video");
	cJSON_AddStringToObject(req, "system", vsel_sys);
	if (!key) cJSON_AddStringToObject(req, "preset", value);
	else { set = cJSON_AddObjectToObject(req, "set"); cJSON_AddStringToObject(set, key, value && *value ? value : "off"); }
	char *b = cJSON_PrintUnformatted(req);
	cJSON_Delete(req);
	video_result(api_call("%s", b));
	free(b);
}

static int vpick_row;
static void video_picked(const char *path)
{
	if (path) {
		video_send(vpick_row == VR_PRESET ? NULL : VR_KEY[vpick_row], path);
		toast_show("Saved: shows the next time this system starts");
	}
	show_video_sys();
	vrow_list.sel = vpick_row;
	vlist_refresh(&vrow_list);
}

static void show_video_sys(void)
{
	cur = SCR_VIDEO_SYS;
	if (!vcur) video_result(api_call("{\"cmd\":\"video\",\"system\":\"%s\"}", vsel_sys));
	lv_obj_clean(body);
	char name[64] = "";
	for (int i = 0; i < nvsys; i++) if (!strcmp(vsys[i].sys, vsel_sys)) snprintf(name, sizeof(name), "%s", vsys[i].name);
	screen_title("SCREEN FILTERS", name);
	char core[120];
	snprintf(core, sizeof(core), "Core: %s  \xC2\xB7  shows the next time it starts", vcur && api_str(vcur, "core") ? api_str(vcur, "core") : "?");
	lv_obj_t *note = mk_label(body, FONT_SMALL, PAL.dim, core);
	lv_obj_set_pos(note, S(56), S(178));
	vlist_create(&vrow_list, body, S(56), S(208), S(900), S(420));
	vrow_list.fill = vrow_fill;
	vrow_list.count = VR_COUNT;
	vrow_list.sel = 0;
	vrow_list.top = 0;
	vlist_refresh(&vrow_list);
	static const char *const hints[] = { "A", "Change", "SELECT", "Undo", "B", "Back", NULL };
	footer_set(hints);
}

static void video_key(pkey_t k)
{
	if (cur == SCR_VIDEO) {
		if (vlist_key(&vsys_list, k)) return;
		if (k == K_A && nvsys) {
			snprintf(vsel_sys, sizeof(vsel_sys), "%s", vsys[vsys_list.sel].sys);
			cJSON_Delete(vcur);
			vcur = NULL;
			show_video_sys();
		} else if (k == K_B || k == K_MENU) {
			show_settings();
			set_list.sel = SET_VIDEO;
			vlist_refresh(&set_list);
		}
		return;
	}
	if (vlist_key(&vrow_list, k)) return;
	int i = vrow_list.sel;
	if (i == VR_MASKMODE && (k == K_LEFT || k == K_RIGHT || k == K_A)) {
		static const char *MM[] = { "off", "1x", "2x", "1x rotated", "2x rotated" };
		const char *v = vcur ? api_str(vcur, "maskmode") : "off";
		int m = 0;
		for (int j = 0; j < 5; j++) if (v && !strcmp(v, MM[j])) m = j;
		m = (m + (k == K_LEFT ? 4 : 1)) % 5;
		video_send("maskmode", MM[m]);
		vlist_refresh(&vrow_list);
	} else if (k == K_A) {
		vpick_row = i;
		static const char *KIND[VR_COUNT] = { "presets", "filters", "filters", "filters", "filters", "masks", "", "gamma" };
		fp_open(KIND[i], VR_NAME[i], i != VR_PRESET, video_picked);
	} else if (k == K_SELECT) {
		cJSON *r = api_call("{\"cmd\":\"video\",\"system\":\"%s\",\"undo\":true}", vsel_sys);
		toast_show(api_ok(r) ? "Last change undone" : (r && api_str(r, "error") ? api_str(r, "error") : "Couldn't undo"));
		video_result(r);
		vlist_refresh(&vrow_list);
	} else if (k == K_B || k == K_MENU) {
		int keep = 0;
		for (int j = 0; j < nvsys; j++) if (!strcmp(vsys[j].sys, vsel_sys)) keep = j;
		show_video();
		vsys_list.sel = keep;
		vlist_refresh(&vsys_list);
	}
}

/* ---------- navigation ---------- */

void ui_rescan(void)
{
	cJSON_Delete(api_call("{\"cmd\":\"rescan\"}"));
}

/* ---------- lists across systems: favorites, recently played, search ---------- */

static void show_list(int mode)
{
	games_mode = mode;
	char t[96];
	if (mode == GL_SEARCH) snprintf(t, sizeof(t), "\xE2\x80\x9C%s\xE2\x80\x9D", games_q);
	else snprintf(t, sizeof(t), "%s", mode == GL_FAV ? "Favorites" : "Recently played");
	show_games("", t, 0);
}

static const char *const SEARCH_BACK_HINTS[] = { "A", "Open", "Y", "Search", "\xE2\x89\xA1", "Settings", NULL };

static void search_done(const char *text)
{
	if (!text) return;                       /* B: cancelled */
	while (*text == ' ') text++;
	if (!*text) { toast_show("Type part of a game's name"); return; }
	snprintf(games_q, sizeof(games_q), "%s", text);
	show_list(GL_SEARCH);
}

static void search_open(void)
{
	osk_open("Search", "Part of a game's name, then Done.", 0, 40, search_done, SEARCH_BACK_HINTS);
}

void ui_key(pkey_t k)
{
	if (osk_key(k)) return;
	switch (cur) {
	case SCR_HOME:
		if (k == K_LEFT) shelf_go(home.sel - 1);
		else if (k == K_RIGHT) shelf_go(home.sel + 1);
		else if (k == K_A && home.sel == SH_FAV) show_list(GL_FAV);
		else if (k == K_A && home.sel == SH_RECENT) show_list(GL_RECENT);
		else if (k == K_A) show_systems((category_t)home.sel);
		else if (k == K_Y) search_open();
		else if (k == K_MENU) show_settings();
		else if (k == K_X && last_game) play_id((long long)api_num(last_game, "id", -1));
		break;
	case SCR_SYSTEMS:
		if (vlist_key(&sys_list, k)) sys_detail();
		else if (k == K_A && nsys) show_games(sys_items[sys_list.sel].id, sys_items[sys_list.sel].name, sys_cat);
		else if (k == K_B) show_home();
		break;
	case SCR_GAMES:
		if (picker_open()) { picker_key(k); break; }
		if (vlist_key(&game_list, k)) game_detail();
		else if (k == K_Y && ngames) picker_show();
		else if (k == K_X && ngames) games_toggle_fav();
		else if (k == K_A && ngames) play_id(games[game_list.sel].id);
		else if (k == K_B && games_mode != GL_SYSTEM) show_home();
		else if (k == K_B) {
			int keep = 0;
			show_systems(sys_cat);
			for (int i = 0; i < nsys; i++) if (!strcmp(sys_items[i].id, games_sys)) keep = i;
			sys_list.sel = keep;
			vlist_refresh(&sys_list);
			sys_detail();
		}
		break;
	case SCR_SOURCES:
		sources_key(k);
		break;
	case SCR_DIAG:
		diag_key(k);
		break;
	case SCR_WIFI:
		wifi_key(k);
		break;
	case SCR_VIDEO:
	case SCR_VIDEO_SYS:
		video_key(k);
		break;
	case SCR_PICK:
		fp_key(k);
		break;
	case SCR_REMOTE:
		remote_key(k);
		break;
	case SCR_CLOUD:
		cloud_key(k);
		break;
	case SCR_WELCOME:
		welcome_key(k);
		break;
	case SCR_BACKUP:
		backup_key(k);
		break;
	case SCR_PORTS:
		ports_key(k);
		break;
	case SCR_BT:
		bt_key(k);
		break;
	case SCR_SETTINGS:
		if (vlist_key(&set_list, k)) break;
		if (k == K_LEFT) settings_change(-1);
		else if (k == K_RIGHT || k == K_A) {
			if (set_list.sel == SET_RESCAN && k == K_A) { ui_rescan(); toast_show("Looking for games..."); }
			else if (set_list.sel == SET_SOURCES) show_sources();
			else if (set_list.sel == SET_DIAG) show_diag();
			else if (set_list.sel == SET_WIFI) show_wifi();
			else if (set_list.sel == SET_BT) show_bt();
			else if (set_list.sel == SET_REMOTE) show_remote();
			else if (set_list.sel == SET_CLOUD) show_cloud();
			else if (set_list.sel == SET_BACKUP) show_backup();
			else if (set_list.sel == SET_PORTS) show_ports();
			else if (set_list.sel == SET_VIDEO) show_video();
			else settings_change(1);
		}
		else if (k == K_B || k == K_MENU) show_home();
		break;
	}
}

static int last_gen = -1;
static uint32_t last_status, last_poll;

void ui_tick(void)
{
	uint32_t now = lv_tick_get();
	if (now - last_status > 2000 || last_status == 0) { status_update(); last_status = now ? now : 1; }
	/* the controllers on the front ports, live */
	static uint32_t last_pads;
	if (cur == SCR_PORTS && pt.available && now - last_pads >= 100) { last_pads = now; pt_pads(); }
	/* the library changed (a scan finished): refresh what's on screen */
	if (now - last_poll >= 1000 || last_poll == 0) {
		last_poll = now ? now : 1;
		if (cur == SCR_SETTINGS) { scrape_refresh(); vlist_refresh(&set_list); }
		if (cur == SCR_BACKUP && bk.busy) bk_apply(api_call("{\"cmd\":\"backup\"}"));
		if (cur == SCR_BT) { static int bt_tick; if (bt.busy || ++bt_tick % 3 == 0) bt_apply(api_call("{\"cmd\":\"bluetooth\"}")); }
		if (cur == SCR_PORTS && pt.busy) pt_apply(api_call("{\"cmd\":\"memcard\"}"));
		if (cur == SCR_CLOUD && cl.busy && !osk.open) cl_apply(api_call("{\"cmd\":\"cloud\"}"));
		if (cur == SCR_REMOTE && pair_box) remote_apply(api_call("{\"cmd\":\"remote\"}"));
		if (cur == SCR_DIAG && diag_busy[0]) diag_apply(api_call("{\"cmd\":\"diag\"}"));
		if (cur == SCR_WIFI && !osk.open) { static int tick; if (wscanning || ++tick % 5 == 0) wifi_apply(api_call("{\"cmd\":\"wifi\"}")); }
		cJSON *r = api_call("{\"cmd\":\"status\"}");
		int g = r ? (int)api_num(r, "generation", -1) : -1;
		cJSON_Delete(r);
		if (g != last_gen) {
			int first = last_gen < 0;
			last_gen = g;
			if (!first && cur == SCR_HOME) { load_home_data(); home_counts(); home_continue(); }
			else if (!first && cur == SCR_SYSTEMS) {
				load_systems(sys_cat);
				sys_list.count = nsys;
				vlist_refresh(&sys_list);
				sys_detail();
			} else if (!first && cur == SCR_GAMES && !picker_open()) {
				/* copies of a game found on two drives may have become one: stay on the same game */
				long long keep = ngames ? games[game_list.sel].id : -1;
				load_games(games_sys);
				game_list.count = ngames;
				for (int i = 0; i < ngames; i++) if (games[i].id == keep) game_list.sel = i;
				vlist_refresh(&game_list);
				game_detail();
			}
		}
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
	texture_apply(root, (texture_t)SETTINGS.texture);    /* redrawn in the accent colour */
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
	if (SETTINGS.setup_done) show_home();
	else show_welcome();
}
