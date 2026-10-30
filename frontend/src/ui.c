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
#include <unistd.h>
#include "mainlink.h"
#include "crt.h"
#include "src/misc/lv_text_private.h"
#include "input.h"
#include "icons.h"
#include "api.h"
#include "log.h"
#include "settings.h"
#include "sysinfo.h"
#include "texture.h"
#include "jpeg.h"
#include "padview.h"
#include "theme.h"
#include "video_opts.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>

typedef enum { SCR_HOME, SCR_SYSTEMS, SCR_GAMES, SCR_SETTINGS, SCR_SOURCES, SCR_DIAG, SCR_WIFI, SCR_REMOTE, SCR_CLOUD, SCR_WELCOME, SCR_BACKUP, SCR_PORTS, SCR_BT, SCR_ABOUT, SCR_LOG, SCR_SCRIPTS, SCR_SCAN, SCR_GUARD, SCR_ART, SCR_CTRL, SCR_MAP, SCR_PUPD, SCR_MCARDS, SCR_PADTEST, SCR_STORAGE, SCR_CTEST, SCR_VIDEO, SCR_DISCS, SCR_INGAME, SCR_RA, SCR_RAVIEW, SCR_MUSIC, SCR_NOWPLAY, SCR_SHOTS, SCR_SHOTGRID, SCR_SHOTVIEW, SCR_FOLDER, SCR_WALL, SCR_DCOPY } screen_t;

static int W, H;
static lv_obj_t *root, *body, *footer;
static screen_t cur = SCR_HOME;

/* ---------- the background: the texture, or the wallpaper (Settings > Wallpapers) ----------
 * phosphord makes the chosen picture cover the screen and darkens it a little (wallpaper_screen), so
 * text over it stays readable; it's drawn as the object's background image. */
static lv_draw_buf_t *bg_wall_buf;                    /* the wallpaper, decoded (bg_apply) */
static void bg_apply(lv_obj_t *obj)
{
	if (SETTINGS.wallpaper_on && SETTINGS.wallpaper[0] && SETTINGS.wall_osd != 2) {
		cJSON *q = cJSON_CreateObject();
		cJSON_AddStringToObject(q, "cmd", "wallpaper_screen");
		cJSON_AddStringToObject(q, "file", SETTINGS.wallpaper);
		cJSON_AddNumberToObject(q, "w", W);
		cJSON_AddNumberToObject(q, "h", H);
		char *b = cJSON_PrintUnformatted(q);
		cJSON_Delete(q);
		cJSON *r = api_call_slow(20000, "%s", b);
		free(b);
		const char *sp = api_ok(r) ? api_str(r, "screen") : NULL;
		/* decoded once into its own buffer: in the image cache it got pushed out by box art, and every
		   redraw then decoded the whole picture again (a second on the SS1), leaving the menu half drawn */
		char src[720];
		snprintf(src, sizeof(src), "A:%s", sp ? sp : "");
		lv_draw_buf_t *nb = sp ? jpeg_load_buf(src) : NULL;
		cJSON_Delete(r);
		if (nb) {
			texture_apply(obj, 0);                       /* (frees the texture tile) */
			lv_obj_set_style_bg_image_src(obj, nb, 0);
			lv_obj_set_style_bg_image_tiled(obj, false, 0);
			lv_obj_set_style_bg_image_opa(obj, LV_OPA_COVER, 0);
			if (bg_wall_buf) lv_draw_buf_destroy(bg_wall_buf);
			bg_wall_buf = nb;
			lv_obj_invalidate(lv_screen_active());
			return;
		}
	}
	texture_apply(obj, (texture_t)SETTINGS.texture);
	if (bg_wall_buf) { lv_draw_buf_destroy(bg_wall_buf); bg_wall_buf = NULL; }
	lv_obj_invalidate(lv_screen_active());
}

/* ---------- status bar ---------- */

static lv_obj_t *sb_share, *sb_eth, *sb_wifi, *sb_bt, *sb_btn, *sb_clock, *sb_alert, *sb_alert_text, *sb_disc, *sb_upd;

static void status_bar(void)
{
	int lr = theme_lr;                                  /* 240p / 288p: a thin bar, 20 lines */
	lv_obj_t *bar = mk_box(root, W, lr ? L(22) : S(72), PAL.bg, 0);
	lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
	lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);

	/* the wordmark and the status icons each on a card (see-through: theme_card_opa) */
	lv_obj_t *wcard = mk_box(bar, LV_SIZE_CONTENT, LV_SIZE_CONTENT, PAL.panel, lr ? L(4) : S(12));
	lv_obj_set_style_pad_hor(wcard, lr ? L(4) : S(14), 0);
	lv_obj_set_style_pad_ver(wcard, lr ? L(1) : S(10), 0);
	lv_obj_align(wcard, LV_ALIGN_LEFT_MID, lr ? L(12) : S(42), lr ? L(2) : 0);
	lv_obj_t *word = mk_label(wcard, FONT_WORD, PAL.acc, "PHOSPHOR");
	lv_obj_set_style_text_letter_space(word, lr ? 2 : S(3), 0);

	lv_obj_t *right = mk_box(bar, LV_SIZE_CONTENT, lr ? L(14) : S(30), PAL.panel, lr ? L(4) : S(12));
	lv_obj_set_height(right, LV_SIZE_CONTENT);
	lv_obj_set_style_pad_hor(right, lr ? L(4) : S(14), 0);
	lv_obj_set_style_pad_ver(right, lr ? L(1) : S(10), 0);
	lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_column(right, lr ? L(6) : S(22), 0);
	lv_obj_align(right, LV_ALIGN_RIGHT_MID, lr ? -L(12) : -S(42), lr ? L(2) : 0);

	/* the notification area: shown only when there's something to look at (Settings > Updates) */
	sb_alert = mk_box(right, LV_SIZE_CONTENT, S(30), PAL.panel_on, S(15));
	lv_obj_set_style_border_color(sb_alert, PAL.acc, 0);
	lv_obj_set_style_border_width(sb_alert, S(1) > 1 ? S(1) : 1, 0);
	lv_obj_set_style_pad_hor(sb_alert, S(12), 0);
	lv_obj_set_flex_flow(sb_alert, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(sb_alert, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_column(sb_alert, S(8), 0);
	lv_obj_t *dot = mk_box(sb_alert, S(10), S(10), PAL.acc, S(5));
	(void)dot;
	sb_alert_text = mk_label(sb_alert, FONT_SMALL, PAL.text, "");
	lv_obj_set_hidden(sb_alert, true);

	int is = lr ? L(10) : S(22);
	sb_upd = icon_create(right, ICON_UPDATE, is);         /* a newer Phosphor on GitHub (Settings > Update Phosphor) */
	lv_obj_set_style_text_color(sb_upd, lv_color_hex(0xf5c542), 0);
	lv_obj_set_hidden(sb_upd, true);
	sb_disc = icon_create(right, ICON_DISC, is);          /* a disc in the dock's drive */
	lv_obj_set_style_text_color(sb_disc, PAL.acc, 0);
	lv_obj_set_hidden(sb_disc, true);
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

/* the notification pill: the power warning first (it can cost a save), else update-guard's alert */
static int guard_resets = -1;
static struct {
	int known, count, warn_at, warn;
	struct { long at; char what[64]; } ev[10];
	int nev;
} pw = { .warn_at = 3 };
/* the dock's NVMe still missing after phosphord reset its port (<tmp>/dock-stuck, phantom.c): only a power
   cut starts it over */
static int dock_stuck;
static void alert_refresh(void)
{
	if (!sb_alert) return;
	char t[64] = "";
	if (dock_stuck) snprintf(t, sizeof(t), "NVMe not responding: unplug the power 10 s");
	else if (pw.warn) snprintf(t, sizeof(t), "Power supply problem?");
	else if (guard_resets > 0) snprintf(t, sizeof(t), "%d setting%s reset by an update", guard_resets, guard_resets == 1 ? "" : "s");
	if (t[0]) lv_label_set_text(sb_alert_text, t);
	lv_obj_set_hidden(sb_alert, !t[0]);
}
static void guard_alert_set(int n)
{
	guard_resets = n;
	alert_refresh();
}
/* health-watch's power check: heavy work cut short with no trace, counted since the last Dismiss.
 * Asked at start-up, after a script and when Diagnostics opens; never on a timer. */
static void power_apply(cJSON *r)
{
	if (!api_ok(r) || !cJSON_IsTrue(cJSON_GetObjectItem(r, "installed"))) { cJSON_Delete(r); return; }
	pw.known = 1;
	pw.count = (int)api_num(r, "count", 0);
	pw.warn_at = (int)api_num(r, "warn_at", 3);
	pw.warn = cJSON_IsTrue(cJSON_GetObjectItem(r, "warn"));
	pw.nev = 0;
	cJSON *e;
	cJSON_ArrayForEach(e, cJSON_GetObjectItem(r, "events")) {
		if (pw.nev >= 10) break;
		pw.ev[pw.nev].at = (long)api_num(e, "at", 0);
		snprintf(pw.ev[pw.nev].what, sizeof(pw.ev[0].what), "%s", api_str(e, "what") ? api_str(e, "what") : "?");
		pw.nev++;
	}
	cJSON_Delete(r);
	alert_refresh();
}
static void power_check(void) { power_apply(api_call("{\"cmd\":\"power\"}")); }
static int guard_count(cJSON *g)
{
	if (!g || !cJSON_IsTrue(cJSON_GetObjectItem(g, "awaiting"))) return 0;
	int n = 0;
	cJSON *f;
	cJSON_ArrayForEach(f, cJSON_GetObjectItem(g, "files")) if (cJSON_IsTrue(cJSON_GetObjectItem(f, "reset"))) n++;
	return n;
}
/* asked at start-up, after a script, and from the Updates screen: never on a timer */
static void guard_check(void)
{
	cJSON *g = api_call("{\"cmd\":\"guard\"}");
	if (g && api_ok(g)) guard_alert_set(guard_count(g));
	cJSON_Delete(g);
}

static void status_update(void)
{
	sysinfo_t s;
	sysinfo_read(&s);
	{
		char f[300];
		const char *tp = getenv("PHOS_TMP");
		snprintf(f, sizeof(f), "%s/dock-stuck", tp && *tp ? tp : "/tmp/phosphor");
		int st = access(f, F_OK) == 0;
		if (st != dock_stuck) { dock_stuck = st; alert_refresh(); }
	}
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

static const char *const *footer_now;
static void footer_set(const char *const *hints)
{
	if (hints == footer_now && lv_obj_get_child_count(footer)) return;     /* the same: nothing to redraw */
	footer_now = hints;
	lv_obj_clean(footer);
	lv_obj_set_hidden(footer, !hints || !hints[0]);     /* (a card: none when there's nothing on it) */
	if (!hints || !hints[0]) return;
	if (theme_lr) {                                     /* 240p / 288p: the button's letter in bold, then what it does */
		for (int i = 0; hints[i]; i += 2) {
			lv_obj_t *item = mk_box(footer, LV_SIZE_CONTENT, L(14), PAL.bg, 0);
			lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
			lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
			lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
			lv_obj_set_style_pad_column(item, L(3), 0);
			lv_obj_set_user_data(item, (void *)(intptr_t)i);   /* (the hint it shows: what may go, below) */
			mk_label(item, (unsigned char)hints[i][0] >= 0x80 ? FONT_WORD : FONT_NAME, PAL.text, hints[i]);   /* (arrows, \xE2\x89\xA1: the mono font has them) */
			mk_label(item, FONT_SMALL, PAL.dim, hints[i + 1]);
		}
		lv_obj_update_layout(footer);
		for (;;) {                                      /* what doesn't fit goes (never B), as on a narrow screen */
			int n = (int)lv_obj_get_child_count(footer);
			lv_obj_t *last = n ? lv_obj_get_child(footer, n - 1) : NULL;
			if (!last || lv_obj_get_x(last) + lv_obj_get_width(last) <= lv_obj_get_width(footer) || n <= 1) break;
			int drop = -1;
			for (int k = n - 1; k >= 1 && drop < 0; k--) if (strcmp(hints[(intptr_t)lv_obj_get_user_data(lv_obj_get_child(footer, k))], "B")) drop = k;
			if (drop < 0) break;
			lv_obj_delete(lv_obj_get_child(footer, drop));
			lv_obj_update_layout(footer);
		}
		return;
	}
	for (int i = 0; hints[i]; i += 2) {
		lv_obj_t *item = mk_box(footer, LV_SIZE_CONTENT, S(28), PAL.bg, 0);
		lv_obj_set_user_data(item, (void *)(intptr_t)i);
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
	/* a narrow screen (4:3, 5:4) can't fit a long row: the last hints go (never B, Back), the first ones
	   being the ones that matter */
	if (!NARROW) return;
	lv_obj_update_layout(footer);
	for (;;) {
		int n = (int)lv_obj_get_child_count(footer);
		lv_obj_t *last = n ? lv_obj_get_child(footer, n - 1) : NULL;
		if (!last || lv_obj_get_x(last) + lv_obj_get_width(last) <= lv_obj_get_width(footer) || n <= 2) break;
		int drop = -1;
		for (int k = n - 1; k >= 1 && drop < 0; k--)
			if (strcmp(hints[(intptr_t)lv_obj_get_user_data(lv_obj_get_child(footer, k))], "B")) drop = k;
		if (drop < 0) break;
		lv_obj_delete(lv_obj_get_child(footer, drop));
		lv_obj_update_layout(footer);
	}
}

/* ---------- toast ---------- */

static lv_obj_t *toast;
static lv_timer_t *toast_timer;
static void toast_hide(lv_timer_t *t) { (void)t; toast_timer = NULL; lv_obj_set_hidden(toast, true); }

static char ig_note[120];              /* CRT mode in a game: the last toast, shown in the OSD menu */
static uint32_t ig_note_at;
static void toast_show(const char *msg)
{
	snprintf(ig_note, sizeof(ig_note), "%s", msg);
	ig_note_at = lv_tick_get();
	lv_label_set_text(lv_obj_get_child(toast, 0), msg);
	lv_obj_set_hidden(toast, false);
	lv_obj_move_foreground(toast);
	if (toast_timer) lv_timer_delete(toast_timer);   /* the new one's full time, not what's left of the last */
	toast_timer = lv_timer_create(toast_hide, 3500, NULL);
	lv_timer_set_repeat_count(toast_timer, 1);
	plog("toast: %s", msg);
}

/* ---------- virtual list (used for systems, games, settings) ---------- */

#define VL_MAX_ROWS 24
typedef struct {
	lv_obj_t *box, *rows[VL_MAX_ROWS], *lab[VL_MAX_ROWS], *val[VL_MAX_ROWS], *bar[VL_MAX_ROWS], *thumb;
	int vis, count, sel, top, row_h, row_w;
	int wrap;                           /* Up on the first row goes to the last, and Down on the last to the first */
	signed char shown[VL_MAX_ROWS];     /* what each row last looked like: -1 unknown, else on*2+dim */
	void (*fill)(int i, char *text, size_t n, char *value, size_t vn, int *dim);
} vlist_t;

/* 240p / 288p: every list takes the screen's width under the title (the design's side panels don't fit
   there), unless the screen lays itself out for 240p (vlist_lr_own, for the next list made) */
static int vlist_lr_own, vlist_lr_y;
/* 240p: a status line and a line under it above the list (Bluetooth, Backups, Cloud saves...) */
static void lr_head_sub(lv_obj_t *head, lv_obj_t *sub)
{
	if (!theme_lr) return;
	lv_label_set_long_mode(head, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(head, W - L(32), lv_font_get_line_height(FONT_BODY));
	lv_obj_set_pos(head, L(16), L(58));
	if (sub) {
		lv_label_set_long_mode(sub, LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_size(sub, W - L(32), lv_font_get_line_height(FONT_SMALL));
		lv_obj_set_pos(sub, L(16), L(73));
	}
	vlist_lr_y = L(88);
}
/* 240p: put a thing at x, y (320x240 units); w > 0: that wide, one line, ... at the end */
static void lr_at(lv_obj_t *o, int x, int y, int w)
{
	if (!o) return;
	lv_obj_set_pos(o, L(x), L(y));
	if (w > 0) {
		lv_obj_set_width(o, L(w));
		if (lv_obj_check_type(o, &lv_label_class)) lv_label_set_long_mode(o, LV_LABEL_LONG_MODE_DOTS);
	}
}
/* 240p: a progress screen's "Everything else waits" box, one line, and the side list hidden */
static void lr_progress(lv_obj_t *bar, lv_obj_t *eta, lv_obj_t *note, lv_obj_t *nt, lv_obj_t *nb, lv_obj_t *sl, lv_obj_t *list, int bar_y, int note_y)
{
	int w = LRW - 32;
	lv_obj_set_size(bar, L(w), L(5));
	lv_obj_set_pos(bar, L(16), L(bar_y));
	lv_obj_set_width(eta, L(150));
	lv_obj_set_pos(eta, L(16 + w - 150), L(bar_y + 9));
	lv_obj_set_size(note, L(w), LV_SIZE_CONTENT);
	lv_obj_set_pos(note, L(16), L(note_y));
	lv_obj_set_style_pad_bottom(note, L(5), 0);
	lv_obj_set_style_radius(note, L(4), 0);
	lr_at(nt, 8, 5, w - 16);
	lv_obj_set_hidden(nb, true);
	lv_obj_set_hidden(sl, true);
	lv_obj_set_hidden(list, true);
}
#define LR_HIDE(o) do { if (theme_lr) lv_obj_set_hidden((o), true); } while (0)   /* 240p: a side panel that doesn't fit */
static void vlist_create(vlist_t *v, lv_obj_t *parent, int x, int y, int w, int h)
{
	if (theme_lr && !vlist_lr_own && parent == body) {
		x = L(12); y = vlist_lr_y ? vlist_lr_y : L(60); w = W - L(24); h = H - y - L(24);
	}
	vlist_lr_own = 0;
	vlist_lr_y = 0;
	v->row_h = theme_lr ? L(19) : S(46);
	v->row_w = w - (theme_lr ? L(4) : S(10));
	v->vis = LV_MIN(h / v->row_h, VL_MAX_ROWS);
	v->box = mk_box(parent, w, v->vis * v->row_h, PAL.bg, 0);   /* (on its column's card: body_cards) */
	lv_obj_set_style_bg_opa(v->box, LV_OPA_TRANSP, 0);
	lv_obj_set_pos(v->box, x, y);
	for (int r = 0; r < v->vis; r++) {
		int lr = theme_lr;
		lv_obj_t *row = mk_box(v->box, w - (lr ? L(4) : S(10)), v->row_h - (lr ? L(1) : S(4)), PAL.panel, lr ? L(3) : S(10));
		lv_obj_set_style_radius(row, lr ? L(3) : S(10), 0);   /* (a row: its own, smaller rounding) */
		lv_obj_set_pos(row, 0, r * v->row_h);
		lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
		v->bar[r] = mk_box(row, lr ? L(2) : S(3), v->row_h - (lr ? L(5) : S(18)), PAL.acc, S(2));
		lv_obj_align(v->bar[r], LV_ALIGN_LEFT_MID, lr ? 0 : S(8), 0);
		v->lab[r] = mk_label(row, FONT_BODY, PAL.dim, "");
		lv_label_set_long_mode(v->lab[r], LV_LABEL_LONG_MODE_DOTS);
		/* one line: long text ends in ... (240p: what the value leaves, the value's own width found when filled) */
		lv_obj_set_size(v->lab[r], lr ? w - L(60) : w - S(200), lr ? lv_font_get_line_height(FONT_BODY) : S(26));
		lv_obj_align(v->lab[r], LV_ALIGN_LEFT_MID, lr ? L(7) : S(24), 0);
		v->val[r] = mk_label(row, FONT_SMALL, PAL.dim, "");
		lv_obj_align(v->val[r], LV_ALIGN_RIGHT_MID, lr ? -L(5) : -S(18), 0);
		v->rows[r] = row;
		v->shown[r] = -1;
	}
	v->thumb = mk_box(v->box, theme_lr ? L(2) : S(3), S(40), PAL.line, S(2));
	lv_obj_set_x(v->thumb, w - (theme_lr ? L(2) : S(4)));
}

static void vlist_refresh(vlist_t *v)
{
	if (v->sel >= v->count) v->sel = v->count ? v->count - 1 : 0;
	if (v->sel < 0) v->sel = 0;
	if (v->top < 0) v->top = 0;
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
		/* only what changed: every change redraws that part of the screen (a 1080p screen is a lot of
		   pixels for the MiSTer's ARM, and this runs every second while some screens are open) */
		if (strcmp(lv_label_get_text(v->lab[r]), t)) lv_label_set_text(v->lab[r], t);
		if (strcmp(lv_label_get_text(v->val[r]), val)) {
			lv_label_set_text(v->val[r], val);
			/* the name gets what the value leaves, so a long value (a status) never runs over it */
			lv_text_attributes_t ta;
			lv_text_attributes_init(&ta);
			int vw = val[0] ? lv_text_get_width(val, strlen(val), FONT_SMALL, &ta) : 0;
			if (theme_lr) lv_obj_set_width(v->lab[r], LV_MAX(L(40), v->row_w - L(12) - (vw ? vw + L(10) : 0)));
			else lv_obj_set_width(v->lab[r], LV_MAX(S(120), LV_MIN(v->row_w - S(190), v->row_w - S(24) - S(18) - (vw ? vw + S(20) : 0))));
		}
		int look = on * 2 + (dim ? 1 : 0);
		if (v->shown[r] != look) {
			v->shown[r] = (signed char)look;
			lv_obj_set_style_bg_opa(v->rows[r], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
			lv_obj_set_style_bg_color(v->rows[r], PAL.panel_on, 0);
			lv_obj_set_style_text_color(v->lab[r], on ? PAL.text : dim ? PAL.faint : PAL.dim, 0);
			lv_obj_set_style_text_color(v->val[r], on ? PAL.acc : PAL.dim, 0);
			lv_obj_set_hidden(v->bar[r], !on);
		}
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
	else if (k == K_UP && v->wrap && v->count > 1) v->sel = v->count - 1;
	else if (k == K_DOWN && v->wrap && v->count > 1) v->sel = 0;
	else if (k == K_L) v->sel = LV_MAX(0, v->sel - v->vis);
	else if (k == K_R) v->sel = LV_MAX(0, LV_MIN(v->count - 1, v->sel + v->vis));   /* (an empty list: stays 0) */
	else return 0;
	if (v->sel != old) vlist_refresh(v);
	return 1;
}

/* ---------- Home: category shelf ---------- */

static const icon_t CAT_ICON[CAT_COUNT] = { ICON_DISC, ICON_CARTRIDGE, ICON_HANDHELD, ICON_ARCADE, ICON_COMPUTER };
/* the shelf: the categories, Other and Utility (the cores in _Other and _Utility), Music, then Favorites and
   Recently played */
enum { SH_OTHER = CAT_COUNT, SH_UTIL, SH_MUSIC, SH_FAV, SH_RECENT, SHELF_COUNT };
static icon_t shelf_icon(int i) { return i == SH_OTHER ? ICON_OTHER : i == SH_UTIL ? ICON_UTILITY : i == SH_MUSIC ? ICON_MUSIC : i == SH_FAV ? ICON_STAR : i == SH_RECENT ? ICON_CLOCK : CAT_ICON[i]; }
static const char *shelf_name(int i) { return i == SH_OTHER ? "Other" : i == SH_UTIL ? "Utility" : i == SH_MUSIC ? "Music" : i == SH_FAV ? "Favorites" : i == SH_RECENT ? "Recently played" : category_name((category_t)i); }

static struct {
	lv_obj_t *card[SHELF_COUNT], *ic[SHELF_COUNT], *nm[SHELF_COUNT], *ct[SHELF_COUNT];
	int sel, first;     /* the highlighted card; the first of the five shown */
	float pos;          /* animated selection position */
	lv_obj_t *cont, *cont_title, *cont_sub;
} home;

/* the shelf, laid out like Phosphor for Windows: five whole cards in a row from the left, the
 * highlight moving along them, the highlighted card a little bigger. Moving past the fifth, the first
 * one goes and the next one comes in (and back the same way); nothing is ever cut off at the edge.
 * pos is the animated selection (0..SHELF_COUNT-1); home.first the first card shown. */
static int on_border(int on) { return on ? L(2) : 0; }       /* 240p: the highlighted card's outline */
#define SHELF_SHOWN (theme_lr ? 3 : 5)                     /* 240p / 288p: three bigger cards */
static void shelf_layout(float pos)
{
	home.pos = pos;
	int left = theme_lr ? L(16) : S(56), gap = theme_lr ? L(8) : S(18), vis_w = W - 2 * left;
	int cw0 = (vis_w - (SHELF_SHOWN - 1) * gap) / SHELF_SHOWN, ch0 = theme_lr ? L(88) : S(250), step = cw0 + gap;
	if (home.sel < home.first) home.first = home.sel;
	if (home.sel > home.first + SHELF_SHOWN - 1) home.first = home.sel - (SHELF_SHOWN - 1);
	if (home.first > SHELF_COUNT - SHELF_SHOWN) home.first = LV_MAX(0, SHELF_COUNT - SHELF_SHOWN);
	if (home.first < 0) home.first = 0;
	for (int i = 0; i < SHELF_COUNT; i++) {
		int shown = i >= home.first && i < home.first + SHELF_SHOWN;
		lv_obj_set_hidden(home.card[i], !shown);
		if (!shown) continue;
		float d = i - pos, ad = d < 0 ? -d : d;
		float grow = ad < 1 ? 1 - ad : 0;                  /* 1 at the highlighted card */
		int cw = cw0 + (theme_lr ? 0 : (int)(S(22) * grow)), ch = ch0 + (theme_lr ? 0 : (int)(S(32) * grow));
		int cx = left + (i - home.first) * step + cw0 / 2;
		lv_obj_set_size(home.card[i], cw, ch);
		lv_obj_set_pos(home.card[i], cx - cw / 2, (theme_lr ? L(44) : S(140)) + (ch0 - ch) / 2);
		if (theme_lr) lv_obj_set_style_border_width(home.card[i], on_border(ad < 0.5f), 0);
		lv_obj_set_style_opa(home.card[i], LV_OPA_COVER, 0);
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
	r = api_call("{\"cmd\":\"folder_cores\",\"folder\":\"_Other\"}");
	if (api_ok(r)) cat_count[SH_OTHER] = cJSON_GetArraySize(cJSON_GetObjectItem(r, "cores"));
	cJSON_Delete(r);
	r = api_call("{\"cmd\":\"folder_cores\",\"folder\":\"_Utility\"}");
	if (api_ok(r)) cat_count[SH_UTIL] = cJSON_GetArraySize(cJSON_GetObjectItem(r, "cores")) + (cJSON_IsTrue(cJSON_GetObjectItem(r, "update_all")) ? 1 : 0);
	cJSON_Delete(r);
	r = api_call("{\"cmd\":\"music_albums\"}");
	if (api_ok(r)) cat_count[SH_MUSIC] = cJSON_GetArraySize(cJSON_GetObjectItem(r, "albums")) + (cJSON_GetObjectItem(r, "cd") ? 1 : 0);
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
	cJSON *ld = api_ok(r) ? cJSON_GetObjectItem(r, "disc") : NULL;
	if (cJSON_IsObject(ld)) {
		/* a disc played after the last game from storage, still in the drive: Play last plays it */
		last_game = cJSON_CreateObject();
		cJSON_AddStringToObject(last_game, "title", api_str(ld, "title") ? api_str(ld, "title") : "Disc");
		char sub[96];
		snprintf(sub, sizeof(sub), "%s  \xC2\xB7  in the drive", api_str(ld, "system") ? api_str(ld, "system") : "Disc");
		cJSON_AddStringToObject(last_game, "system_name", sub);
		if (api_str(ld, "art")) cJSON_AddStringToObject(last_game, "art", api_str(ld, "art"));
		cJSON_AddNumberToObject(last_game, "category", CAT_DISC);
		cJSON_AddTrueToObject(last_game, "disc");
	} else if (api_ok(r) && cJSON_IsObject(cJSON_GetObjectItem(r, "game")))
		last_game = cJSON_DetachItemFromObject(r, "game");
	cJSON_Delete(r);
}

static char ig_title[160];         /* the game just started (the in-game menu's title) */
static int ig_from_disc;           /* ... and whether it plays from the dock's drive */
static cJSON *ig_game;             /* ... its details: box art and description beside the in-game menu */
static char ig_file[1024];         /* the file Phosphor's Main last said was loaded (status "file") */
static char ig_core[64];           /* the core the in-game menu last opened over */
/* after a launch: what's running now (the in-game menu's title and details) */
static void play_id_quiet(long long id)
{
	toast_show("Starting...");
	cJSON *g = api_call("{\"cmd\":\"game\",\"id\":%lld}", id);
	cJSON *gg = api_ok(g) ? cJSON_DetachItemFromObject(g, "game") : NULL;
	snprintf(ig_title, sizeof(ig_title), "%s", gg && api_str(gg, "title") ? api_str(gg, "title") : "");
	ig_from_disc = 0;
	ig_file[0] = ig_core[0] = 0;
	cJSON_Delete(ig_game);
	ig_game = cJSON_IsObject(gg) ? gg : (cJSON_Delete(gg), NULL);
	cJSON_Delete(g);
}
/* ---------- Start with: the core a game starts on, when it has more than one ----------
 * The MiSTer core, the RetroAchievements core, an older version kept on the card, a test build from
 * _Unstable, a dual RAM build, another build in _Other: only the game's own system (phosphord game_cores). Shown only when there's a
 * choice and none was set for the system; X sets one (Always start with this: core_pref), so after that
 * games start straight away. */
static void play_id_core(long long id, int core);
static struct { lv_obj_t *dim, *row[8], *nm[8], *ds[8]; cJSON *cores; long long id; int n, sel, open; char sys[64]; } cc;
static void cc_draw(void)
{
	for (int i = 0; i < cc.n; i++) {
		int on = i == cc.sel;
		lv_obj_set_style_bg_color(cc.row[i], on ? PAL.panel_on : PAL.panel, 0);
		lv_obj_set_style_border_color(cc.row[i], on ? PAL.acc : PAL.line, 0);
		lv_obj_set_style_border_width(cc.row[i], on ? S(2) : LV_MAX(1, S(1)), 0);
		lv_obj_set_style_text_color(cc.nm[i], on ? PAL.text : PAL.dim, 0);
	}
}
static void cc_close(void)
{
	if (!cc.open) return;
	lv_obj_delete(cc.dim);
	cJSON_Delete(cc.cores);
	memset(&cc, 0, sizeof(cc));
}
static int cc_open(long long id, cJSON *cores, const char *title, const char *sysname)
{
	cc_close();
	cc.open = 1;
	cc.id = id;
	cc.cores = cores;
	cc.n = LV_MIN(8, cJSON_GetArraySize(cores));
	snprintf(cc.sys, sizeof(cc.sys), "%s", sysname ? sysname : "");
	cc.dim = lv_obj_create(lv_layer_top());
	lv_obj_remove_style_all(cc.dim);
	lv_obj_set_size(cc.dim, W, H);
	lv_obj_set_style_bg_color(cc.dim, PAL.bg, 0);
	lv_obj_set_style_bg_opa(cc.dim, LV_OPA_COVER, 0);
	char cap[120];
	spaced_caps(cap, sizeof(cap), sysname && *sysname ? sysname : "START WITH");
	lv_obj_t *l = mk_label(cc.dim, FONT_LABEL, PAL.dim, cap);
	lv_obj_set_style_text_letter_space(l, S(3), 0);
	lv_obj_set_pos(l, S(56), S(96));
	lv_obj_t *t = mk_label(cc.dim, FONT_HEADING, PAL.text, title && *title ? title : "Start with");
	lv_obj_set_pos(t, S(56), S(122));
	lv_obj_t *st = mk_label(cc.dim, FONT_LABEL, PAL.dim, "START WITH");
	lv_obj_set_style_text_letter_space(st, S(3), 0);
	lv_obj_set_pos(st, S(56), S(186));
	for (int i = 0; i < cc.n; i++) {
		cJSON *x = cJSON_GetArrayItem(cores, i);
		lv_obj_t *r = mk_box(cc.dim, SW(1168), S(80), PAL.panel, S(12));
		lv_obj_set_pos(r, S(56), S(214) + i * S(88));
		lv_obj_set_style_border_width(r, LV_MAX(1, S(1)), 0);
		lv_obj_set_style_pad_hor(r, S(22), 0);
		cc.row[i] = r;
		char nm[220];
		const char *d = api_str(x, "date");
		snprintf(nm, sizeof(nm), "%s%s%s", api_str(x, "name") ? api_str(x, "name") : "", d ? "  \xC2\xB7  " : "", d ? d : "");
		cc.nm[i] = mk_label(r, FONT_NAME, PAL.dim, nm);
		lv_obj_align(cc.nm[i], LV_ALIGN_TOP_LEFT, 0, S(14));
		cc.ds[i] = mk_label(r, FONT_SMALL, PAL.dim, api_str(x, "desc") ? api_str(x, "desc") : "");
		lv_obj_align(cc.ds[i], LV_ALIGN_TOP_LEFT, 0, S(46));
		if (cJSON_IsTrue(cJSON_GetObjectItem(x, "preferred"))) {
			lv_obj_t *p = mk_label(r, FONT_SMALL, PAL.acc, "Always");
			lv_obj_align(p, LV_ALIGN_RIGHT_MID, 0, 0);
			cc.sel = i;
		}
	}
	char xh[96];
	snprintf(xh, sizeof(xh), "Always start %s games with this", sysname && *sysname ? sysname : "these");
	static char xhint[96];
	snprintf(xhint, sizeof(xhint), "%s", xh);
	const char *hints[] = { "A", "Start", "X", xhint, "B", "Back", NULL };
	lv_obj_t *hb = mk_box(cc.dim, W, S(40), PAL.bg, 0);
	lv_obj_set_style_bg_opa(hb, LV_OPA_TRANSP, 0);
	lv_obj_set_flex_flow(hb, LV_FLEX_FLOW_ROW);
	lv_obj_set_style_pad_column(hb, S(28), 0);
	lv_obj_set_pos(hb, S(56), H - S(56));
	for (int i = 0; hints[i]; i += 2) {
		char h[140];
		snprintf(h, sizeof(h), "(%s)  %s", hints[i], hints[i + 1]);
		mk_label(hb, FONT_SMALL, PAL.dim, h);
	}
	cc_draw();
	plog("start with: %d cores", cc.n);
	return 1;
}
/* the chooser's part of a key press: 1 when it ate it */
static int cc_key(pkey_t k)
{
	if (!cc.open) return 0;
	if (k == K_UP && cc.n) cc.sel = (cc.sel + cc.n - 1) % cc.n;
	else if (k == K_DOWN && cc.n) cc.sel = (cc.sel + 1) % cc.n;
	else if (k == K_B || k == K_MENU) { cc_close(); return 1; }
	else if (k == K_A || k == K_X) {
		cJSON *x = cJSON_GetArrayItem(cc.cores, cc.sel);
		int core = x ? (int)api_num(x, "index", 0) : 0;
		long long id = cc.id;
		if (k == K_X) {
			cJSON *r = api_call("{\"cmd\":\"core_pref\",\"id\":%lld,\"core\":%d}", id, core);
			if (!api_ok(r)) toast_show(api_str(r, "error") ? api_str(r, "error") : "Couldn't keep that choice");
			cJSON_Delete(r);
		}
		cc_close();
		play_id_core(id, core);
		return 1;
	}
	cc_draw();
	return 1;
}

/* Start with... (Select on a game): the chooser even when a default is set, to change it */
static void play_choose(long long id)
{
	cJSON *c = api_call("{\"cmd\":\"game_cores\",\"id\":%lld}", id);
	cJSON *cores = api_ok(c) ? cJSON_DetachItemFromObject(c, "cores") : NULL;
	cJSON_Delete(c);
	if (cJSON_GetArraySize(cores) < 2) { cJSON_Delete(cores); toast_show("This game has just the one core"); return; }
	cJSON *g = api_call("{\"cmd\":\"game\",\"id\":%lld}", id);
	cJSON *gg = api_ok(g) ? cJSON_GetObjectItem(g, "game") : NULL;
	cc_open(id, cores, gg ? api_str(gg, "title") : NULL, gg ? api_str(gg, "system_name") : NULL);
	cJSON_Delete(g);
}
static void play_id_core(long long id, int core);
static void play_id(long long id)
{
	/* more than one core, and none set for the system: ask which (Start with) */
	cJSON *c = api_call("{\"cmd\":\"game_cores\",\"id\":%lld}", id);
	cJSON *cores = api_ok(c) ? cJSON_DetachItemFromObject(c, "cores") : NULL;
	cJSON_Delete(c);
	int n = cJSON_GetArraySize(cores), pref = 0;
	cJSON *x;
	cJSON_ArrayForEach(x, cores) if (cJSON_IsTrue(cJSON_GetObjectItem(x, "preferred"))) pref = 1;
	if (n > 1 && !pref) {
		cJSON *g = api_call("{\"cmd\":\"game\",\"id\":%lld}", id);
		cJSON *gg = api_ok(g) ? cJSON_GetObjectItem(g, "game") : NULL;
		cc_open(id, cores, gg ? api_str(gg, "title") : NULL, gg ? api_str(gg, "system_name") : NULL);
		cJSON_Delete(g);
		return;
	}
	cJSON_Delete(cores);
	play_id_core(id, -1);
}
static void play_id_core(long long id, int core)
{
	cJSON *r = core >= 0 ? api_call("{\"cmd\":\"launch\",\"id\":%lld,\"core\":%d}", id, core)
	                     : api_call("{\"cmd\":\"launch\",\"id\":%lld}", id);
	if (!r) toast_show("The system service isn't responding");
	else if (!api_ok(r)) toast_show(api_str(r, "error") ? api_str(r, "error") : "Couldn't start the game");
	else {
		toast_show("Starting...");
		cJSON *g = api_call("{\"cmd\":\"game\",\"id\":%lld}", id);
		cJSON *gg = api_ok(g) ? cJSON_DetachItemFromObject(g, "game") : NULL;
		snprintf(ig_title, sizeof(ig_title), "%s", gg && api_str(gg, "title") ? api_str(gg, "title") : "");
		ig_from_disc = 0;
		ig_file[0] = ig_core[0] = 0;
		cJSON_Delete(ig_game);
		ig_game = cJSON_IsObject(gg) ? gg : (cJSON_Delete(gg), NULL);
		cJSON_Delete(g);
	}
	cJSON_Delete(r);
}

/* ---------- the dock's CD/DVD drive ----------
 * phosphord names what's in it ("disc": from the library or libretro's lists, with box art);
 * Phosphor's Main tells us when it changes. Shown only while a disc is in: the disc icon in the status
 * bar and "In the drive" at the top of Disc. Nothing starts by itself. Music CDs and DVDs aren't played
 * yet. Disc support: Main_MiSTer_Physical_Disc by Anime0t4ku and bonna79. (docs/discs.md) */
static struct {
	int known, enabled, in, playable, tries, copyable;   /* copyable: Save a copy works for it (dump.c) */
	long long game_id;                 /* the same game in the library (its details), else 0 */
	char state[16], title[200], system[64], serial[32], reason[96], art[700];
} dk = { .enabled = 1 };
static void systems_disc_update(void);
static void discs_screen_update(void);
static void show_discs(void);
static void show_ra(void);
static void show_music(void);
static void music_key(int k);
static void nowplay_key(int k);
static void music_tick(void);
static void show_nowplay(void);
static void ra_key(int k);
static void raview_key(int k);
static void ra_tick(void);
static void discs_key(int k);
static void show_shots(void);
static void shots_key(int k);
static void shotgrid_key(int k);
static void shotview_key(int k);
static void take_shot(void);
static void ingame_key(int k);
static void ingame_open(void);
static void ingame_close(int to_game);
static void disc_refresh(void)
{
	cJSON *r = api_call("{\"cmd\":\"disc\"}");
	cJSON *d = api_ok(r) ? cJSON_GetObjectItem(r, "disc") : NULL;
	int en = dk.enabled, tries = dk.tries;
	char key[64];
	snprintf(key, sizeof(key), "%s|%s", dk.state, dk.serial);
	memset(&dk, 0, sizeof(dk));
	dk.enabled = en;
	if (d) {
		dk.known = 1;
		dk.enabled = !cJSON_IsFalse(cJSON_GetObjectItem(d, "enabled"));
		const char *v;
#define DKS(f, k) if ((v = api_str(d, k))) snprintf(dk.f, sizeof(dk.f), "%s", v)
		DKS(state, "state"); DKS(title, "title"); DKS(system, "system"); DKS(serial, "serial"); DKS(reason, "reason"); DKS(art, "art");
#undef DKS
		dk.playable = cJSON_IsTrue(cJSON_GetObjectItem(d, "playable"));
		dk.copyable = cJSON_IsTrue(cJSON_GetObjectItem(d, "copyable"));
		dk.game_id = (long long)api_num(d, "game_id", 0);
		dk.in = dk.enabled && (!strcmp(dk.state, "reading") || !strcmp(dk.state, "ready") || !strcmp(dk.state, "unreadable"));
	}
	cJSON_Delete(r);
	char nk[64];
	snprintf(nk, sizeof(nk), "%s|%s", dk.state, dk.serial);
	dk.tries = strcmp(key, nk) ? 0 : tries;          /* box art may still be on its way: ask a few more times */
	static int pushed = -1;                          /* Main follows Settings › Discs (kept in phosphord) */
	if (dk.known && dk.enabled != pushed) { mainlink_disc_enable(dk.enabled); pushed = dk.enabled; }
	if (sb_disc) lv_obj_set_hidden(sb_disc, !dk.in);
	systems_disc_update();
	discs_screen_update();
}
static void disc_play_now(void)
{
	cJSON *r = api_call("{\"cmd\":\"disc_play\"}");
	if (!r) toast_show("The system service isn't responding");
	else if (!api_ok(r)) toast_show(api_str(r, "error") ? api_str(r, "error") : "Couldn't start the disc");
	else {
		toast_show("Starting...");
		snprintf(ig_title, sizeof(ig_title), "%.159s", dk.title);
		ig_file[0] = ig_core[0] = 0;
		ig_from_disc = 1;
		/* the in-game menu's details: the library game with the same disc code, else what the drive says */
		cJSON_Delete(ig_game);
		ig_game = NULL;
		if (dk.game_id > 0) {
			cJSON *g = api_call("{\"cmd\":\"game\",\"id\":%lld}", dk.game_id);
			cJSON *gg = api_ok(g) ? cJSON_DetachItemFromObject(g, "game") : NULL;
			if (cJSON_IsObject(gg)) ig_game = gg; else cJSON_Delete(gg);
			cJSON_Delete(g);
		}
		if (!ig_game) {
			ig_game = cJSON_CreateObject();
			cJSON_AddStringToObject(ig_game, "title", dk.title);
			if (dk.art[0]) cJSON_AddStringToObject(ig_game, "art", dk.art);
		}
	}
	cJSON_Delete(r);
}
static void disc_eject_now(void)
{
	cJSON *r = api_call("{\"cmd\":\"disc_eject\"}");
	if (!api_ok(r)) toast_show(r && api_str(r, "error") ? api_str(r, "error") : "The tray didn't open");
	cJSON_Delete(r);
}
/* the drive row's words (the Disc list, Settings › Discs) */
static const char *disc_line(void)
{
	if (!strcmp(dk.state, "reading")) return "reading the disc\xE2\x80\xA6";
	if (!strcmp(dk.state, "unreadable")) return "a disc that can't be read";
	return dk.title[0] ? dk.title : "a disc";
}

static void home_counts(void)
{
	for (int c = 0; c < SHELF_COUNT; c++) {
		int n = cat_count[c];
		char t[48];
		if (n < 0) snprintf(t, sizeof(t), "Looking...");
		else if (c == SH_MUSIC) snprintf(t, sizeof(t), "%d album%s", n, n == 1 ? "" : "s");
		else if (c == SH_OTHER) snprintf(t, sizeof(t), n ? "%d core%s" : "None yet", n, n == 1 ? "" : "s");
		else if (c == SH_UTIL) snprintf(t, sizeof(t), n ? "%d tool%s" : "None yet", n, n == 1 ? "" : "s");
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
	int lr = theme_lr;
	lv_obj_t *sec = mk_label(body, FONT_LABEL, PAL.dim, "LIBRARY");
	lv_obj_set_style_text_letter_space(sec, lr ? 2 : S(3), 0);
	lv_obj_set_pos(sec, lr ? L(16) : S(56), lr ? L(30) : S(104));

	for (int i = 0; i < SHELF_COUNT; i++) {
		lv_obj_t *c = mk_box(body, S(200), S(250), PAL.panel, lr ? L(6) : S(18));
		lv_obj_set_style_border_width(c, lr ? 0 : LV_MAX(1, S(1)), 0);
		lv_obj_set_style_border_color(c, PAL.line, 0);
		lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_style_pad_row(c, lr ? L(3) : S(14), 0);
		home.card[i] = c;
		home.ic[i] = icon_create(c, shelf_icon(i), lr ? L(28) : S(72));
		home.nm[i] = mk_label(c, FONT_NAME, PAL.dim, shelf_name(i));
		home.ct[i] = mk_label(c, FONT_SMALL, PAL.faint, "");
	}
	shelf_layout((float)home.sel);
	home_counts();

	/* pick up where you left off */
	lv_obj_t *cont = mk_box(body, lr ? W - L(32) : W - S(112), lr ? L(52) : S(120), PAL.panel, lr ? L(6) : S(16));
	lv_obj_set_style_border_width(cont, lr ? 0 : LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(cont, PAL.line, 0);
	lv_obj_set_pos(cont, lr ? L(16) : S(56), lr ? LV_MIN(L(142), H - L(24) - L(54)) : S(470));   /* (240p with overscan: above the hints) */
	int aw = lr ? L(32) : S(72), ah = lr ? L(36) : S(88);
	lv_obj_t *art = mk_box(cont, aw, ah, lv_color_hex(0x2a2119), S(8));
	lv_obj_set_style_border_width(art, 1, 0);
	lv_obj_set_style_border_color(art, lv_color_hex(0x3a3530), 0);
	lv_obj_set_style_clip_corner(art, true, 0);
	lv_obj_align(art, LV_ALIGN_LEFT_MID, lr ? L(9) : S(28), 0);
	/* music playing: the bar shows it (Music › Now playing) instead of the last game */
	cJSON *mr = api_call("{\"cmd\":\"music\"}");
	cJSON *mtrack = api_ok(mr) ? cJSON_GetObjectItem(cJSON_GetObjectItem(mr, "music"), "track") : NULL;
	const char *last_art = mtrack ? api_str(mtrack, "cover") : last_game ? api_str(last_game, "art") : NULL;
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
		lv_obj_t *aic = icon_create(art, mtrack ? ICON_MUSIC : CAT_ICON[cat >= 0 && cat < CAT_COUNT ? cat : 0], S(34));
		lv_obj_set_style_text_color(aic, PAL.acc, 0);
		lv_obj_center(aic);
	}
	lv_obj_t *l1 = mk_label(cont, FONT_LABEL, lr ? PAL.acc : PAL.dim, mtrack ? "NOW PLAYING" : lr ? "CONTINUE" : "PICK UP WHERE YOU LEFT OFF");
	lv_obj_set_style_text_letter_space(l1, lr ? 1 : S(3), 0);
	lv_obj_set_pos(l1, lr ? L(50) : S(124), lr ? L(7) : S(28));
	home.cont_title = mk_label(cont, lr ? FONT_NAME : FONT_TITLE, PAL.text, "");
	lv_obj_set_pos(home.cont_title, lr ? L(50) : S(124), lr ? L(17) : S(48));
	home.cont_sub = mk_label(cont, FONT_SMALL, PAL.dim, "");
	lv_obj_set_pos(home.cont_sub, lr ? L(50) : S(124), lr ? L(32) : S(80));
	if (lr) {                                           /* one line each, ... at the end */
		lv_label_set_long_mode(home.cont_title, LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_width(home.cont_title, W - L(32) - L(60));
		lv_label_set_long_mode(home.cont_sub, LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_width(home.cont_sub, W - L(32) - L(60));
	}
	home.cont = cont;
	if (last_game && !lr) {                             /* (240p: X Play last says it) */
		lv_obj_t *pill = mk_box(cont, LV_SIZE_CONTENT, S(36), PAL.panel, LV_RADIUS_CIRCLE);
		lv_obj_set_style_border_width(pill, LV_MAX(1, S(1)), 0);
		lv_obj_set_style_border_color(pill, PAL.acc, 0);
		lv_obj_set_style_pad_hor(pill, S(18), 0);
		lv_obj_t *pl = mk_label(pill, FONT_SMALL, PAL.acc, "Play");
		lv_obj_center(pl);
		lv_obj_align(pill, LV_ALIGN_RIGHT_MID, -S(28), 0);
	}
	home_continue();
	if (mtrack) {
		lv_label_set_text(home.cont_title, api_str(mtrack, "title") ? api_str(mtrack, "title") : "");
		char sub[300];
		snprintf(sub, sizeof(sub), "%s%s%s", api_str(mtrack, "album") ? api_str(mtrack, "album") : "",
		         api_str(mtrack, "artist") && *api_str(mtrack, "artist") ? "  \xC2\xB7  " : "", api_str(mtrack, "artist") ? api_str(mtrack, "artist") : "");
		lv_label_set_text(home.cont_sub, sub);
	}
	cJSON_Delete(mr);

	static const char *const hints_last[] = { "A", "Open", "X", "Play last", "Y", "Search", "\xE2\x89\xA1", "Settings", NULL };
	static const char *const hints[] = { "A", "Open", "Y", "Search", "\xE2\x89\xA1", "Settings", NULL };
	footer_set(last_game ? hints_last : hints);
}

/* ---------- Systems in a category ---------- */

typedef struct { char id[24], name[48]; int games, core, drive; } sys_item_t;   /* drive: "In the drive" */
static sys_item_t sys_items[64];
static int nsys;
static category_t sys_cat;
static vlist_t sys_list;
static lv_obj_t *sys_detail_name, *sys_detail_count, *sys_detail_core, *sys_detail_ic, *sys_detail_art, *sys_detail_about, *sys_detail_fact;

/* a few lines about each system for the Library's side: who made it, when, how many sold, and one thing
   it's remembered for (sales: the makers' lifetime figures as usually cited) */
static const struct { const char *id, *about, *fact; } SYS_ABOUT[] = {
	{ "PSX", "Sony  \xC2\xB7  1994  \xC2\xB7  102 million sold", "The first console to sell over 100 million. CD games made 3D worlds and full-motion video the norm." },
	{ "Saturn", "Sega  \xC2\xB7  1994  \xC2\xB7  9 million sold", "Two Hitachi SH-2 processors. A hit in Japan, and home to some of the best 2D arcade ports." },
	{ "MegaCD", "Sega  \xC2\xB7  1991  \xC2\xB7  2.2 million sold", "A CD drive under the Mega Drive: CD-quality music, full-motion video and Sonic CD." },
	{ "TGFX16CD", "NEC / Hudson  \xC2\xB7  1988", "The first CD-ROM drive for a home console, two years before anyone else's." },
	{ "NES", "Nintendo  \xC2\xB7  1983  \xC2\xB7  62 million sold", "The Famicom brought home video games back in North America after the 1983 crash." },
	{ "SNES", "Nintendo  \xC2\xB7  1990  \xC2\xB7  49 million sold", "Mode 7 scaling and rotation, and a library from Super Mario World to Chrono Trigger." },
	{ "Genesis", "Sega  \xC2\xB7  1988  \xC2\xB7  31 million sold", "Sega's 16-bit console: blast processing, Sonic, and the console war with Nintendo." },
	{ "N64", "Nintendo  \xC2\xB7  1996  \xC2\xB7  33 million sold", "Its analog stick set how 3D games are played, starting with Super Mario 64." },
	{ "SMS", "Sega  \xC2\xB7  1985  \xC2\xB7  13 million sold", "Second to the NES in America, but a long-time favourite in Europe and Brazil." },
	{ "TGFX16", "NEC / Hudson  \xC2\xB7  1987  \xC2\xB7  10 million sold", "Games on HuCards the size of a credit card. Known in America as the TurboGrafx-16." },
	{ "ATARI7800", "Atari  \xC2\xB7  2600: 1977, 7800: 1986", "The 2600 (30 million sold) made cartridges the norm. The 7800 plays its games too." },
	{ "GAMEBOY", "Nintendo  \xC2\xB7  1989  \xC2\xB7  119 million sold", "Game Boy and Game Boy Color together, with Tetris and Pok\xC3\xA9mon among their best sellers." },
	{ "GBA", "Nintendo  \xC2\xB7  2001  \xC2\xB7  82 million sold", "A pocket SNES and then some, and it plays Game Boy and Game Boy Color games too." },
	{ "GameGear", "Sega  \xC2\xB7  1990  \xC2\xB7  11 million sold", "A backlit colour screen against the Game Boy, with a Master System inside." },
	{ "WonderSwan", "Bandai  \xC2\xB7  1999  \xC2\xB7  3.5 million sold", "Japan only. Designed with Gunpei Yokoi, the creator of the Game Boy." },
	{ "AtariLynx", "Atari  \xC2\xB7  1989", "The first handheld with a colour screen, designed by Epyx. It can be turned upside down for left-handed play." },
	{ "Arcade", "Arcade boards", "The arcade games themselves, each core rebuilding the original board in the FPGA." },
	{ "C64", "Commodore  \xC2\xB7  1982  \xC2\xB7  12 to 17 million sold", "The best-selling single computer model ever, and its SID chip is still loved for its music." },
	{ "MSX", "Microsoft / ASCII  \xC2\xB7  1983", "A standard built by Sony, Panasonic, Philips and many more. Metal Gear started on the MSX2." },
	{ "Atari800", "Atari  \xC2\xB7  1979", "Designed by the team behind the 2600, with graphics chips that later inspired the Amiga. Star Raiders." },
};
static char sys_art_src[720];

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
			s->drive = 0;
		}
	}
	cJSON_Delete(r);
	/* a disc in the dock's drive: first, only while it's in */
	if (c == CAT_DISC && dk.in) {
		if (nsys >= 64) nsys = 63;
		memmove(&sys_items[1], &sys_items[0], sizeof(sys_items[0]) * (size_t)nsys);
		sys_item_t *d = &sys_items[0];
		memset(d, 0, sizeof(*d));
		d->drive = 1;
		d->core = dk.playable;
		nsys++;
	}
}

static void sys_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	if (sys_items[i].drive) {
		snprintf(t, n, "In the drive  \xC2\xB7  %s", disc_line());
		snprintf(val, vn, "%s", dk.playable ? dk.system : !strcmp(dk.state, "reading") ? "wait a moment" : dk.reason);
		*dim = !dk.playable && strcmp(dk.state, "reading");
		return;
	}
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
		static const char *const hints_none[] = { "B", "Back", NULL };
		footer_set(hints_none);
		return;
	}
	sys_item_t *s = &sys_items[sys_list.sel];
	if (sys_detail_ic) lv_obj_set_hidden(sys_detail_ic, s->drive && dk.art[0]);
	if (sys_detail_art) lv_obj_set_hidden(sys_detail_art, !(s->drive && dk.art[0]));
	static const char *const hints_sys[] = { "A", "Open", "B", "Back", NULL };
	static const char *const hints_drive[] = { "A", "Play", "Y", "Eject", "B", "Back", NULL };
	static const char *const hints_drive_off[] = { "Y", "Eject", "B", "Back", NULL };
	static const char *const hints_drive_copy[] = { "A", "Play", "X", "Save a copy", "Y", "Eject", "B", "Back", NULL };
	static const char *const hints_drive_off_copy[] = { "X", "Save a copy", "Y", "Eject", "B", "Back", NULL };
	footer_set(!s->drive ? hints_sys : dk.playable ? (dk.copyable ? hints_drive_copy : hints_drive)
	                                               : (dk.copyable ? hints_drive_off_copy : hints_drive_off));
	if (sys_detail_about) lv_obj_set_hidden(sys_detail_about, true);
	if (sys_detail_fact) lv_obj_set_hidden(sys_detail_fact, true);
	lv_obj_set_hidden(sys_detail_core, false);
	if (s->drive) {
		lv_label_set_text(sys_detail_name, disc_line());
		char t[160];
		if (dk.system[0] && dk.serial[0]) snprintf(t, sizeof(t), "%s  \xC2\xB7  %s", dk.system, dk.serial);
		else snprintf(t, sizeof(t), "%s", dk.system[0] ? dk.system : "In the dock's drive");
		lv_label_set_text(sys_detail_count, t);
		lv_label_set_text(sys_detail_core, dk.playable ? "In the dock's drive" : dk.reason);
		if (dk.art[0] && sys_detail_art) {
			snprintf(sys_art_src, sizeof(sys_art_src), "A:%s", dk.art);
			lv_image_set_src(sys_detail_art, sys_art_src);
		}
		return;
	}
	lv_label_set_text(sys_detail_name, s->name);
	char t[64];
	snprintf(t, sizeof(t), "%d game%s", s->games, s->games == 1 ? "" : "s");
	lv_label_set_text(sys_detail_count, t);
	lv_label_set_text(sys_detail_core, s->core ? "" : "Core not installed");
	lv_obj_set_hidden(sys_detail_core, s->core);
	const char *ab = "", *fa = "";
	for (size_t i = 0; i < sizeof(SYS_ABOUT) / sizeof(SYS_ABOUT[0]); i++)
		if (!strcmp(SYS_ABOUT[i].id, s->id)) { ab = SYS_ABOUT[i].about; fa = SYS_ABOUT[i].fact; }
	if (sys_detail_about) { lv_label_set_text(sys_detail_about, ab); lv_obj_set_hidden(sys_detail_about, !ab[0]); }
	if (sys_detail_fact) { lv_label_set_text(sys_detail_fact, fa); lv_obj_set_hidden(sys_detail_fact, !fa[0]); }
}

static void screen_title(const char *label, const char *title)
{
	if (!theme_lr) {                                    /* a card behind the title (see-through) */
		lv_obj_t *c = mk_box(body, LV_SIZE_CONTENT, LV_SIZE_CONTENT, PAL.panel, S(14));
		lv_obj_set_style_pad_hor(c, S(14), 0);
		lv_obj_set_style_pad_top(c, S(12), 0);
		lv_obj_set_style_pad_bottom(c, S(10), 0);         /* (the heading's line has room under its letters) */
		lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_row(c, S(2), 0);
		lv_obj_set_pos(c, S(42), S(76));                  /* (ends a little above the cards under it) */
		lv_obj_add_flag(c, LV_OBJ_FLAG_USER_1);        /* (not part of a column: body_cards) */
		lv_obj_t *sec = mk_label(c, FONT_LABEL, PAL.dim, label);
		lv_obj_set_style_text_letter_space(sec, S(3), 0);
		mk_label(c, FONT_HEADING, PAL.text, title);
		return;
	}
	lv_obj_t *sec = mk_label(body, FONT_LABEL, PAL.dim, label);
	lv_obj_set_style_text_letter_space(sec, theme_lr ? 2 : S(3), 0);
	lv_obj_set_pos(sec, theme_lr ? L(16) : S(56), theme_lr ? L(26) : S(96));
	lv_obj_t *tl = mk_label(body, FONT_HEADING, PAL.text, title);
	lv_obj_set_pos(tl, theme_lr ? L(16) : S(56), theme_lr ? L(36) : S(118));
	if (theme_lr) {                                     /* one line: a long name ends in ... */
		lv_label_set_long_mode(tl, LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_width(tl, W - L(32));
	}
}

static void show_systems(category_t c)
{
	cur = SCR_SYSTEMS;
	sys_cat = c;
	load_systems(c);
	lv_obj_clean(body);
	screen_title("LIBRARY", category_name(c));
	vlist_create(&sys_list, body, S(56), S(176), SW(640), S(440));
	sys_list.count = nsys;
	sys_list.fill = sys_fill;
	sys_list.sel = 0;
	sys_list.top = 0;

	/* the side: the system's icon (or the disc's box art), name and games, big and in the middle of the
	   room right of the list (the screen's card under it: body_cards) */
	int px = S(56) + SW(640) + S(24), pw = W - S(56) - px;
	lv_obj_t *panel = mk_box(body, pw, H - S(82) - S(176), PAL.bg, 0);
	lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
	lv_obj_set_pos(panel, px, S(176));
	lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_row(panel, S(10), 0);
	if (theme_lr) lv_obj_set_hidden(panel, true);      /* 240p: the list says it (the counts on the right) */
	lv_obj_t *ic = icon_create(panel, CAT_ICON[c], S(128));
	lv_obj_set_style_text_color(ic, PAL.acc, 0);
	lv_obj_set_style_margin_bottom(ic, S(12), 0);
	sys_detail_ic = ic;
	sys_detail_art = lv_image_create(panel);           /* the disc's box art, in the icon's place */
	lv_obj_set_size(sys_detail_art, S(180), S(180));
	lv_image_set_inner_align(sys_detail_art, LV_IMAGE_ALIGN_CONTAIN);
	lv_obj_set_hidden(sys_detail_art, true);
	sys_detail_name = mk_label(panel, FONT_HEADING, PAL.text, "");
	lv_obj_set_width(sys_detail_name, pw - S(24));
	lv_obj_set_style_text_align(sys_detail_name, LV_TEXT_ALIGN_CENTER, 0);
	lv_label_set_long_mode(sys_detail_name, LV_LABEL_LONG_MODE_WRAP);
	sys_detail_count = mk_label(panel, FONT_NAME, PAL.dim, "");
	lv_label_set_long_mode(sys_detail_count, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(sys_detail_count, pw - S(24));
	lv_obj_set_style_text_align(sys_detail_count, LV_TEXT_ALIGN_CENTER, 0);
	sys_detail_core = mk_label(panel, FONT_BODY, PAL.acc, "");
	lv_obj_set_style_text_align(sys_detail_core, LV_TEXT_ALIGN_CENTER, 0);
	sys_detail_about = mk_label(panel, FONT_BODY, PAL.acc, "");
	lv_obj_set_width(sys_detail_about, pw - S(24));
	lv_obj_set_style_text_align(sys_detail_about, LV_TEXT_ALIGN_CENTER, 0);
	lv_label_set_long_mode(sys_detail_about, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_style_margin_top(sys_detail_about, S(8), 0);
	sys_detail_fact = mk_label(panel, FONT_BODY, PAL.dim, "");
	lv_obj_set_width(sys_detail_fact, LV_MIN(pw - S(24), S(420)));
	lv_obj_set_style_text_align(sys_detail_fact, LV_TEXT_ALIGN_CENTER, 0);
	lv_label_set_long_mode(sys_detail_fact, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_hidden(sys_detail_about, true);
	lv_obj_set_hidden(sys_detail_fact, true);

	vlist_refresh(&sys_list);
	sys_detail();
}

/* the drive changed while Disc is open: the list again, on the same row */
static void systems_disc_update(void)
{
	if (cur != SCR_SYSTEMS || sys_cat != CAT_DISC) return;
	char keep[24];
	int was_drive = nsys && sys_items[sys_list.sel].drive;
	snprintf(keep, sizeof(keep), "%s", nsys ? sys_items[sys_list.sel].id : "");
	load_systems(sys_cat);
	sys_list.count = nsys;
	sys_list.sel = 0;
	for (int i = 0; i < nsys; i++)
		if (was_drive ? sys_items[i].drive : (!sys_items[i].drive && !strcmp(sys_items[i].id, keep))) sys_list.sel = i;
	if (sys_list.sel >= nsys) sys_list.sel = nsys ? nsys - 1 : 0;
	if (sys_list.top > sys_list.sel) sys_list.top = sys_list.sel;
	vlist_refresh(&sys_list);
	sys_detail();
}

/* ---------- Disc > In the drive > Save a copy (phosphord dump.c) ----------
 * The game disc in the drive copied into the library (.cue and .bin tracks, Redump's layout), checked
 * against Redump's list. Progress while it runs (nothing else happens meanwhile), then the result. */
static void show_games(const char *sys_id, const char *sys_name, int cat);
static struct {
	lv_obj_t *art, *title, *where, *step, *bar, *fill, *line, *left, *note, *badge, *res;
	char art_src[720], sys[48];
	int stop_armed, finished;
	uint32_t armed_at;
} dc;
static const char *const DC_HINTS_RUN[] = { "B", "Stop", NULL };
static const char *const DC_HINTS_DONE[] = { "A", "Show it", "Y", "Eject", "B", "Done", NULL };
static const char *const DC_HINTS_FAIL[] = { "X", "Try again", "B", "Back", NULL };

static void dcopy_update(void)
{
	cJSON *r = api_call("{\"cmd\":\"disc_copy\"}");
	cJSON *c = api_ok(r) ? cJSON_GetObjectItem(r, "copy") : NULL;
	if (!c) { cJSON_Delete(r); return; }
	const char *st = api_str(c, "state") ? api_str(c, "state") : "";
	int busy = cJSON_IsTrue(cJSON_GetObjectItem(c, "busy")), done = !strcmp(st, "done");
	snprintf(dc.sys, sizeof(dc.sys), "%s", api_str(c, "system") ? api_str(c, "system") : "");
	lv_label_set_text(dc.title, api_str(c, "title") ? api_str(c, "title") : "Disc");
	char t[400];
	const char *dest = api_str(c, "dest");
	snprintf(t, sizeof(t), "%s%s%s", dc.sys, dest ? "  \xC2\xB7  " : "", dest ? dest : "");
	lv_label_set_text(dc.where, t);
	int pct = (int)api_num(c, "percent", 0);
	lv_obj_set_width(dc.fill, LV_MAX(1, lv_obj_get_width(dc.bar) * pct / 100));
	lv_obj_set_hidden(dc.bar, !busy);
	if (busy) {
		snprintf(t, sizeof(t), "TRACK %d OF %d  \xC2\xB7  %s", (int)api_num(c, "track", 0), (int)api_num(c, "of", 0),
		         cJSON_IsTrue(cJSON_GetObjectItem(c, "data")) ? "DATA" : "MUSIC");
		lv_label_set_text(dc.step, (int)api_num(c, "of", 0) ? t : "READING THE DISC");
		int rr = (int)api_num(c, "rereads", 0);
		char re[48] = "";
		if (rr) snprintf(re, sizeof(re), "  \xC2\xB7  %d spot%s read again", rr, rr == 1 ? "" : "s");
		double sp = api_num(c, "speed", 0);
		char spd[24] = "";
		if (sp > 0) snprintf(spd, sizeof(spd), "  \xC2\xB7  %.0f\xC3\x97", sp);
		snprintf(t, sizeof(t), "%d of %d MB%s%s", (int)api_num(c, "mb_done", 0), (int)api_num(c, "mb_total", 0), spd, re);
		lv_label_set_text(dc.line, t);
		int left = (int)api_num(c, "seconds_left", -1);
		if (left >= 0) snprintf(t, sizeof(t), left >= 90 ? "about %d minutes left" : left > 10 ? "under %d minute left" : "almost done",
		                        left >= 90 ? (left + 30) / 60 : 2);
		else t[0] = 0;
		if (left > 10 && left < 90) snprintf(t, sizeof(t), "about a minute left");
		lv_label_set_text(dc.left, t);
		lv_label_set_text(dc.res, api_str(c, "msg") ? api_str(c, "msg") : "");
	} else {
		lv_label_set_text(dc.step, done ? "SAVED" : !strcmp(st, "stopped") ? "STOPPED" : "DIDN'T FINISH");
		lv_label_set_text(dc.line, "");
		lv_label_set_text(dc.left, "");
		const char *cue = api_str(c, "cue");
		snprintf(t, sizeof(t), "%s%s%s", api_str(c, "msg") ? api_str(c, "msg") : "", cue ? "\n" : "", cue ? cue : "");
		lv_label_set_text(dc.res, t);
	}
	int verified = cJSON_IsTrue(cJSON_GetObjectItem(c, "verified"));
	lv_obj_set_hidden(dc.badge, !(done && verified));
	lv_obj_set_hidden(dc.note, !busy);
	if (!busy && !dc.finished) {
		dc.finished = 1;
		footer_set(done ? DC_HINTS_DONE : DC_HINTS_FAIL);
		if (done) toast_show(verified ? "Copy saved: an exact copy of the disc" : "Copy saved");
	} else if (busy) {
		dc.finished = 0;
		footer_set(DC_HINTS_RUN);
	}
	if (dc.stop_armed && lv_tick_get() - dc.armed_at > 4000) dc.stop_armed = 0;
	cJSON_Delete(r);
}

static void show_dcopy(int start)
{
	if (start) {
		cJSON *r = api_call("{\"cmd\":\"disc_copy\",\"do\":\"start\"}");
		/* one already running (started from the phone, or this screen was left): that one's screen */
		if (!api_ok(r) && api_str(r, "error") && strstr(api_str(r, "error"), "already being saved")) { cJSON_Delete(r); r = NULL; show_dcopy(0); return; }
		if (!api_ok(r)) {
			toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't start the copy");
			cJSON_Delete(r);
			return;
		}
		cJSON_Delete(r);
	}
	cur = SCR_DCOPY;
	memset(&dc, 0, sizeof(dc));
	lv_obj_clean(body);
	screen_title("DISC", "Saving a copy");
	int aw = S(220);
	lv_obj_t *frame = mk_box(body, aw, aw, PAL.panel, S(10));
	lv_obj_set_pos(frame, S(56), S(196));
	lv_obj_set_style_clip_corner(frame, true, 0);
	dc.art = lv_image_create(frame);
	lv_obj_set_size(dc.art, aw, aw);
	lv_image_set_inner_align(dc.art, LV_IMAGE_ALIGN_CONTAIN);
	lv_obj_center(dc.art);
	if (dk.art[0]) { snprintf(dc.art_src, sizeof(dc.art_src), "A:%s", dk.art); lv_image_set_src(dc.art, dc.art_src); }
	else { lv_obj_t *ic = icon_create(frame, ICON_DISC, S(72)); lv_obj_set_style_text_color(ic, PAL.dim, 0); lv_obj_center(ic); }
	int x = S(56) + aw + S(36), w = W - S(56) - x;
	dc.title = mk_label(body, FONT_TITLE, PAL.text, "");
	lv_label_set_long_mode(dc.title, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_width(dc.title, w);
	lv_obj_set_pos(dc.title, x, S(200));
	dc.where = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(dc.where, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_width(dc.where, w);
	lv_obj_set_pos(dc.where, x, S(240));
	dc.badge = mk_box(body, LV_SIZE_CONTENT, S(32), lv_color_hex(0x1d2b22), S(16));
	lv_obj_set_style_border_width(dc.badge, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(dc.badge, lv_color_hex(0x5cf08a), 0);
	lv_obj_set_style_pad_hor(dc.badge, S(14), 0);
	lv_obj_t *bl = mk_label(dc.badge, FONT_BODY, lv_color_hex(0xc8f5d6), "\xE2\x9C\x93  Matches Redump");
	lv_obj_center(bl);
	lv_obj_set_pos(dc.badge, x, S(272));
	lv_obj_set_hidden(dc.badge, true);
	dc.step = mk_label(body, FONT_LABEL, PAL.acc, "READING THE DISC");
	lv_obj_set_style_text_letter_space(dc.step, S(3), 0);
	lv_obj_set_pos(dc.step, x, S(318));
	dc.bar = mk_box(body, w, S(16), PAL.line, S(8));
	lv_obj_set_pos(dc.bar, x, S(344));
	dc.fill = mk_box(dc.bar, 1, S(16), PAL.acc, S(8));
	lv_obj_set_pos(dc.fill, 0, 0);
	dc.line = mk_label(body, FONT_BODY, PAL.text, "");
	lv_obj_set_pos(dc.line, x, S(372));
	dc.left = mk_label(body, FONT_BODY, PAL.dim, "");
	lv_obj_set_style_text_align(dc.left, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_width(dc.left, SW(300));
	lv_obj_set_pos(dc.left, W - S(56) - SW(300), S(372));
	dc.res = mk_label(body, FONT_BODY, PAL.text, "");
	lv_label_set_long_mode(dc.res, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(dc.res, w);
	lv_obj_set_pos(dc.res, x, S(410));
	dc.note = mk_box(body, w, LV_SIZE_CONTENT, PAL.bg, S(10));
	lv_obj_set_style_bg_opa(dc.note, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_color(dc.note, PAL.line, 0);
	lv_obj_set_style_border_width(dc.note, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_pad_all(dc.note, S(18), 0);
	lv_obj_set_flex_flow(dc.note, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(dc.note, S(8), 0);
	lv_obj_set_pos(dc.note, x, S(470));
	mk_label(dc.note, FONT_NAME, PAL.text, "Everything else waits until this is done.");
	lv_obj_t *nb = mk_label(dc.note, FONT_SMALL, PAL.dim, "Stopping it throws the unfinished copy away; the disc isn't changed.");
	lv_label_set_long_mode(nb, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(nb, w - 2 * S(18));
	footer_set(DC_HINTS_RUN);
	dcopy_update();
}

static void dcopy_key(int k)
{
	cJSON *r = api_call("{\"cmd\":\"disc_copy\"}");
	cJSON *c = api_ok(r) ? cJSON_GetObjectItem(r, "copy") : NULL;
	int busy = c && cJSON_IsTrue(cJSON_GetObjectItem(c, "busy")), done = c && api_str(c, "state") && !strcmp(api_str(c, "state"), "done");
	cJSON_Delete(r);
	if (busy) {
		if (k != K_B) return;
		if (!dc.stop_armed) {                                /* twice: it throws the copy away */
			dc.stop_armed = 1;
			dc.armed_at = lv_tick_get();
			toast_show("Press B again to stop: the unfinished copy is thrown away");
			return;
		}
		cJSON_Delete(api_call("{\"cmd\":\"disc_copy\",\"do\":\"stop\"}"));
		dc.stop_armed = 0;
		return;
	}
	if (k == K_B) { show_systems(CAT_DISC); return; }
	if (k == K_Y) { disc_eject_now(); return; }
	if (k == K_X && !done) { show_dcopy(1); return; }
	if (k == K_A && done) {                                  /* its system's games, where it is now */
		show_systems(CAT_DISC);
		for (int i = 0; i < nsys; i++)
			if (!sys_items[i].drive && !strcmp(sys_items[i].name, dc.sys)) { show_games(sys_items[i].id, sys_items[i].name, CAT_DISC); return; }
	}
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
	/* RetroAchievements: this game's progress, when the library is matched (Settings › RetroAchievements) */
	cJSON *rab = d ? cJSON_GetObjectItem(d, "ra") : NULL;
	if (cJSON_IsObject(rab)) {
		char ra[64];
		snprintf(ra, sizeof(ra), "%sAchievements %d / %d", info[0] ? "  \xC2\xB7  " : "", (int)api_num(rab, "unlocked", 0), (int)api_num(rab, "total", 0));
		strncat(info, ra, sizeof(info) - strlen(info) - 1);
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
	int lr = theme_lr, lpw = L(84);                     /* 240p: the list, and the box art beside it */
	if (lr) { vlist_lr_own = 1; vlist_create(&game_list, body, L(12), L(60), W - L(28) - lpw - L(8), H - L(60) - L(24)); }
	else vlist_create(&game_list, body, S(56), S(206), SW(560), S(412));   /* (the count above it, on the card) */
	game_list.count = ngames;
	game_list.fill = game_fill;
	game_list.sel = 0;
	game_list.top = 0;

	/* details: box art on top, then title, details and description */
	int pw = lr ? lpw : SW(584), ph = lr ? H - L(64) - L(24) : S(456), pad = lr ? 0 : S(26), iw = pw - 2 * pad;
	lv_obj_t *panel = mk_box(body, pw, ph, PAL.panel, S(16));
	lv_obj_set_style_border_width(panel, lr ? 0 : LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(panel, PAL.line, 0);
	lv_obj_set_pos(panel, lr ? W - L(16) - pw : W - S(56) - pw, lr ? L(64) : S(170));
	if (lr) { lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(panel, 0, 0); }

	int ah = lr ? L(108) : S(196);
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

	int y = pad + ah + (lr ? L(4) : S(16));
	g_title = mk_label(panel, FONT_TITLE, PAL.text, "");
	if (lr) lv_obj_set_hidden(g_title, true);           /* (240p: the highlighted row says it) */
	lv_label_set_long_mode(g_title, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(g_title, iw, S(30));
	lv_obj_set_pos(g_title, pad, y);
	if (!lr) y += S(32);
	g_info = mk_label(panel, FONT_SMALL, PAL.acc, "");
	lv_label_set_long_mode(g_info, lr ? LV_LABEL_LONG_MODE_WRAP : LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(g_info, iw, lr ? L(28) : S(20));
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
	if (lr) { lv_obj_set_hidden(g_desc, true); lv_obj_set_hidden(g_where, true); }

	char cnt[48];
	snprintf(cnt, sizeof(cnt), "%d game%s", ngames, ngames == 1 ? "" : "s");

	lv_obj_t *c = mk_label(body, FONT_SMALL, PAL.dim, cnt);
	lv_obj_update_layout(c);
	if (lr) lv_obj_set_pos(c, W - L(16) - lv_obj_get_width(c), L(26));
	else lv_obj_set_pos(c, S(56) + SW(560) - S(10) - lv_obj_get_width(c), S(176));

	vlist_refresh(&game_list);
	game_detail();
	games_hints();
}

static void games_hints(void)
{
	static const char *const hints[] = { "A", "Play", "B", "Back", "X", "Favorite", "Y", "Based on", "SELECT", "Start with", "L", "Page up", "R", "Page down", NULL };
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

	int pw = SW(760), ph = S(560), pad = S(30);
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

/* MiSTer's downloader and Update All: scripts on the SD card, run the way MiSTer's Scripts menu runs them */
static const char *fat_dir(void) { const char *f = getenv("PHOS_FAT"); return f && *f ? f : "/media/fat"; }
static const char *fat_path(char *buf, size_t n, const char *rel) { snprintf(buf, n, "%s/%s", fat_dir(), rel); return buf; }
static char scripts_dir_buf[320], script_log_buf[320], upd_buf[340], upd_all_buf[340];
#define SCRIPTS_DIR fat_path(scripts_dir_buf, sizeof(scripts_dir_buf), "Scripts")
#define SCRIPT_LOG fat_path(script_log_buf, sizeof(script_log_buf), "Scripts/.config/downloader/downloader.log")   /* the downloader's own (Update All runs it too) */
#define UPDATE_SH fat_path(upd_buf, sizeof(upd_buf), "Scripts/update.sh")
#define UPDATE_ALL_SH fat_path(upd_all_buf, sizeof(upd_all_buf), "Scripts/update_all.sh")
static struct { int available, all_available; } cr;
static void cores_refresh(void)
{
	cr.available = access(UPDATE_SH, R_OK) == 0;
	cr.all_available = access(UPDATE_ALL_SH, R_OK) == 0;
}
static void cores_value(char *val, size_t vn)
{
	if (!cr.available) snprintf(val, vn, "MiSTer's updater not found");
	else if (mainlink_script_running()) snprintf(val, vn, "Running...");
	else snprintf(val, vn, ">");
}
static void run_script(const char *path);
static int cores_start_job(int have_net, int all)
{
	if (!have_net) { toast_show("Connect to a network first"); return 0; }
	/* through Phosphor's wrapper when it's there: a restore point first, then the updater */
	char w[400];
	fat_path(w, sizeof(w), all ? "phosphor/scripts/update_all.sh" : "phosphor/scripts/update.sh");
	run_script(access(w, X_OK) == 0 ? w : all ? UPDATE_ALL_SH : UPDATE_SH);
	return 1;
}
static void cores_start(int have_net) { cores_start_job(have_net, 0); }
/* Utility's Register this SuperStation: Update All (its settings during the countdown), network first */
static void cores_start_job_fwd(int all)
{
	sysinfo_t si;
	sysinfo_read(&si);
	cJSON *w = api_call("{\"cmd\":\"wifi\"}");
	cJSON *ws = w ? cJSON_GetObjectItem(w, "status") : NULL;
	int net = si.ethernet || cJSON_IsTrue(cJSON_GetObjectItem(ws, "connected"));
	cJSON_Delete(w);
	cores_start_job(net, all);
}

/* ---------- Restart (A twice) ---------- */

static uint32_t restart_asked, shutdown_asked;

static void power_run(int restart);
static void restart_ask(void)
{
	uint32_t now = lv_tick_get();
	if (!restart_asked || now - restart_asked > 4000) {
		restart_asked = now ? now : 1;
		toast_show("Press A again to restart");
		return;
	}
	restart_asked = 0;
	plog("restart from Settings");
	power_run(1);                                    /* drives made clean first, like the power window */
}

static void shutdown_ask(void)
{
	uint32_t now = lv_tick_get();
	if (!shutdown_asked || now - shutdown_asked > 4000) {
		shutdown_asked = now ? now : 1;
		toast_show("Press A again to shut down");
		return;
	}
	shutdown_asked = 0;
	plog("shut down from Settings");
	power_run(0);                                    /* the power window's safe stop, then "safe to switch off" */
}

/* ---------- Settings ---------- */

static vlist_t set_list;
enum { SET_WIFI, SET_BT, SET_CTRL, SET_REMOTE, SET_SAMBA, SET_CLOUD, SET_BACKUP, SET_PORTS, SET_STORAGE, SET_DISCS, SET_RA, SET_SHOTS, SET_VIDEO, SET_ACCENT, SET_TEXTURE, SET_WALLPAPER, SET_WALL_PICK, SET_CLOCK, SET_SAVER, SET_RESCAN, SET_SCRAPE, SET_SCRAPE_INFO, SET_SOURCES, SET_CORES, SET_UPDATE_ALL, SET_PHOSUPD, SET_GUARD, SET_SCRIPTS, SET_UPDLOG, SET_DIAG, SET_MISTER, SET_RESTART, SET_SHUTDOWN, SET_ABOUT, SET_COUNT };
/* the rows shown: every setting, but Choose wallpaper only while Wallpapers is on */
static int set_ids[SET_COUNT], set_n;
static void set_build(void)
{
	set_n = 0;
	for (int id = 0; id < SET_COUNT; id++) {
		if (id == SET_WALL_PICK && !SETTINGS.wallpaper_on) continue;
		set_ids[set_n++] = id;
	}
}
static int set_id(int row) { return row >= 0 && row < set_n ? set_ids[row] : -1; }
static int set_row(int id) { for (int i = 0; i < set_n; i++) if (set_ids[i] == id) return i; return 0; }
static void si_update(void);
static int si_pending(void);
static cJSON *scrape_state;      /* phosphord's scrape reply, refreshed while Settings is open */
static cJSON *samba_st;          /* phosphord's samba reply, likewise */

static int samba_b(const char *k) { return samba_st && cJSON_IsTrue(cJSON_GetObjectItem(samba_st, k)); }
static void my_ip(char *out, size_t n);
static void samba_take(cJSON *x)
{
	int was = samba_b("busy");
	cJSON_Delete(samba_st);
	samba_st = x;
	if (!was || samba_b("busy") || !api_ok(samba_st)) return;
	/* just finished: say what happened */
	char msg[200], ip[64];
	if (!samba_b("worked")) snprintf(msg, sizeof(msg), "Sharing: %s", api_str(samba_st, "error") && *api_str(samba_st, "error") ? api_str(samba_st, "error") : "that didn't work");
	else if (samba_b("want")) { my_ip(ip, sizeof(ip)); snprintf(msg, sizeof(msg), "Sharing is on: \\\\%s  (user root, password 1)", ip); }
	else snprintf(msg, sizeof(msg), "Sharing is off");
	toast_show(msg);
}

/* this system's address on the network (first IPv4 that isn't loopback), for "\\\\address" */
static void my_ip(char *out, size_t n)
{
	struct ifaddrs *ifs, *i;
	snprintf(out, n, "MiSTer");
	if (getifaddrs(&ifs) != 0) return;
	for (i = ifs; i; i = i->ifa_next) {
		if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK)) continue;
		inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, out, (socklen_t)n);
		break;
	}
	freeifaddrs(ifs);
}

/* (once a second while Settings is open: in the background, the replies taken as they come) */
static void scrape_refresh(void)
{
	api_bg_ask("scrape", "{\"cmd\":\"scrape\"}");
	api_bg_ask("samba", "{\"cmd\":\"samba\"}");
	cJSON *x = api_bg_take("scrape");
	if (x) { cJSON_Delete(scrape_state); scrape_state = x; }
	x = api_bg_take("samba");
	if (x) samba_take(x);
}

static void set_fill(int row, char *t, size_t n, char *val, size_t vn, int *dim)
{
	(void)dim;
	switch (set_id(row)) {
	case SET_ACCENT: snprintf(t, n, "Accent colour"); snprintf(val, vn, "<  %s  >", theme_accent_name(theme_accent())); break;
	case SET_TEXTURE: snprintf(t, n, "Background texture");
		if (SETTINGS.wallpaper_on) { snprintf(val, vn, "Off while Wallpapers is on"); *dim = 1; }
		else snprintf(val, vn, "<  %s  >", texture_name((texture_t)SETTINGS.texture));
		break;
	case SET_WALLPAPER: snprintf(t, n, "Wallpapers"); snprintf(val, vn, "<  %s  >", SETTINGS.wallpaper_on ? "On" : "Off"); break;
	case SET_WALL_PICK: {
		const char *b = strrchr(SETTINGS.wallpaper, '/');
		char nm[64] = "None yet";
		if (b) {
			snprintf(nm, sizeof(nm), "%s", b + 1);
			char *d = strrchr(nm, '.');
			if (d) *d = 0;
			const char *tags[] = { "MiSTer 16X9 ", "MiSTer 4X3 ", "MiSTer ", "16X9 ", "4X3 ", "16x9_", "4x3_" };
			for (size_t i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
				size_t l = strlen(tags[i]);
				if (!strncasecmp(nm, tags[i], l)) { memmove(nm, nm + l, strlen(nm + l) + 1); break; }
			}
			for (char *c = nm; *c; c++) if (*c == '_') *c = ' ';
			/* "MiSTer - Anime0t4ku - Metal Gear": the dashes and the creator's name (as phosphord's wp_name) */
			static const char *const creators[] = { "Ranny Snice", "Anime0t4ku", "PCN Challenge", "RGarciaLago" };
			for (int again = 1; again;) {
				again = 0;
				size_t l = strspn(nm, " -");
				if (l) { memmove(nm, nm + l, strlen(nm + l) + 1); again = 1; }
				for (size_t i = 0; i < sizeof(creators) / sizeof(creators[0]); i++) {
					size_t cl = strlen(creators[i]);
					if (!strncasecmp(nm, creators[i], cl) && (nm[cl] == ' ' || nm[cl] == '-')) {
						memmove(nm, nm + cl, strlen(nm + cl) + 1);
						again = 1;
					}
				}
			}
		}
		snprintf(t, n, "Choose wallpaper...");
		snprintf(val, vn, "%.40s  >", nm);
		break;
	}
	case SET_SAVER:
		snprintf(t, n, "Screensaver");
		if (SETTINGS.saver_min) snprintf(val, vn, "<  After %d minutes  >", SETTINGS.saver_min);
		else snprintf(val, vn, "<  Never  >");
		break;
	case SET_CLOCK: snprintf(t, n, "Clock"); snprintf(val, vn, "<  %s  >", SETTINGS.clock24 ? "24-hour" : "12-hour"); break;
	case SET_SCRAPE: {
		int on = scrape_state && cJSON_IsTrue(cJSON_GetObjectItem(scrape_state, "on"));
		snprintf(t, n, "Get box art and details");
		snprintf(val, vn, "%s", on ? "Working...  >" : "Start  >");
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
	case SET_CTRL: snprintf(t, n, "Controllers"); snprintf(val, vn, ">"); break;
	case SET_REMOTE: snprintf(t, n, "Remote access"); snprintf(val, vn, ">"); break;
	case SET_SAMBA:
		snprintf(t, n, "Network share (Samba)");
		if (!api_ok(samba_st) || !samba_b("available")) { *dim = 1; snprintf(val, vn, "Not on this system"); }
		else if (samba_b("busy")) {
			static const char *const D[] = { ".", "..", "..." };
			snprintf(val, vn, "Turning %s%s", samba_b("want") ? "on" : "off", D[(lv_tick_get() / 1000) % 3]);
		}
		else if (samba_b("on") && samba_b("running")) {
			char ip[64];
			my_ip(ip, sizeof(ip));
			snprintf(val, vn, "<  On: \\\\%s  >", ip);
		}
		else if (samba_b("on")) snprintf(val, vn, "<  On, not running  >");
		else snprintf(val, vn, "<  Off  >");
		break;
	case SET_CLOUD: snprintf(t, n, "Cloud saves"); snprintf(val, vn, ">"); break;
	case SET_BACKUP: snprintf(t, n, "Backups"); snprintf(val, vn, ">"); break;
	case SET_PORTS: snprintf(t, n, "Front ports & memory cards"); snprintf(val, vn, ">"); break;
	case SET_STORAGE: snprintf(t, n, "Storage"); snprintf(val, vn, ">"); break;
	case SET_DISCS:
		snprintf(t, n, "Discs");
		if (!dk.known) { *dim = 1; snprintf(val, vn, "Needs Phosphor's Main with disc support"); }
		else snprintf(val, vn, "%s  >", dk.enabled ? "On" : "Off");
		break;
	case SET_RA: snprintf(t, n, "RetroAchievements"); snprintf(val, vn, ">"); break;
	case SET_SHOTS: snprintf(t, n, "Screenshots"); snprintf(val, vn, "%s  >", SETTINGS.shot_off ? "Off" : "L3 + R3"); break;
	case SET_VIDEO: snprintf(t, n, "Video"); snprintf(val, vn, ">"); break;
	case SET_RESCAN: snprintf(t, n, "Look for games"); snprintf(val, vn, "Start  >"); break;
	case SET_CORES: snprintf(t, n, "Update"); cores_value(val, vn); break;
	case SET_UPDATE_ALL:
		snprintf(t, n, "Update All");
		if (!cr.all_available) { *dim = 1; snprintf(val, vn, "Not on the SD card"); }
		else snprintf(val, vn, ">");
		break;
	case SET_PHOSUPD: snprintf(t, n, "Update Phosphor"); snprintf(val, vn, "%s  >", PHOS_VERSION); break;
	case SET_GUARD:
		snprintf(t, n, "Updates & restore points");
		if (guard_resets > 0) snprintf(val, vn, "%d setting%s reset by an update  >", guard_resets, guard_resets == 1 ? "" : "s");
		else snprintf(val, vn, ">");
		break;
	case SET_SCRIPTS: snprintf(t, n, "Scripts"); snprintf(val, vn, ">"); break;
	case SET_UPDLOG: snprintf(t, n, "Last update's log"); snprintf(val, vn, ">"); break;
	case SET_DIAG: snprintf(t, n, "Diagnostics"); snprintf(val, vn, ">"); break;
	case SET_MISTER: snprintf(t, n, "MiSTer menu (OSD)"); snprintf(val, vn, ">"); break;
	case SET_RESTART: snprintf(t, n, "Restart"); break;
	case SET_SHUTDOWN: snprintf(t, n, "Shut down safely"); break;
	case SET_ABOUT: snprintf(t, n, "About this system"); snprintf(val, vn, "Phosphor %s  >", PHOS_VERSION); break;
	}
}

static void show_settings(void);
static void show_welcome(void);
static int in_setup;              /* the sub-screens go back to the welcome screen, not Settings */

/* ---------- Running a script ----------
 * Update All, MiSTer's downloader and anything in Scripts run the way MiSTer's Scripts menu runs them:
 * Phosphor's Main shows MiSTer's own terminal (controllers as its keys, Update All's settings work
 * the same), and gives the screen back after "Press any key to continue". Phosphor draws nothing
 * and reads nothing meanwhile (main.c). */
static void back_to_settings(int sel);
static void show_art(void);
static void show_scripts(void);
static void run_script(const char *path)
{
	if (mainlink_script_running()) { toast_show("A script is already running"); return; }
	console_text();
	if (mainlink_script(path)) { console_quiet(); toast_show("Scripts need Phosphor's Main (phosphor-main)"); }
}

void ui_script_done(int status)
{
	cores_refresh();
	guard_check();                 /* a restore point was saved; and the alert may have changed */
	power_check();
	if (cur == SCR_SETTINGS) vlist_refresh(&set_list);
	toast_show(status ? "The script ended with an error (Settings > Last update's log)" : "Done");
}

/* ---------- Updates & restore points (update-guard) ----------
 * Left: what the last Linux update reset, each setting explained, ticked if it was reset; Restore
 * ticked / Ignore / Roll back Linux. Right: the restore points (saved when an update is started),
 * tick one to roll back, or several to delete. Left/Right switch sides, A ticks or presses. Asked
 * for when the screen opens and while a job runs, never otherwise. */
#define GD_MAXF 8
#define GD_MAXP 12
static struct {
	cJSON *st;
	int nf, np, side, sel[2], tick_f[GD_MAXF], tick_p[GD_MAXP], busy_seen, confirm_btn;
	uint32_t confirm_at;
	char job_last[24];
	lv_obj_t *left, *right, *note;
} gd;
static const char *const GD_HINTS[] = { "A", "Tick / press", "\xE2\x86\x90\xE2\x86\x92", "Other side", "B", "Back", NULL };

static const struct { const char *path, *name, *what, *who; } GD_FILES[] = {
	{ "/etc/dhcpcd.conf", "Network address", "A fixed address for the SS1 on your network instead of an automatic one.",
	  "Only if you set one, e.g. so SS1 Tool or the phone app always finds it." },
	{ "/root/.ssh/authorized_keys", "SSH keys", "Lets your PC log in to the SS1 without typing a password.",
	  "Only if you added keys yourself (advanced; SS1 Tool works without them)." },
	{ "/etc/hostname", "Name on the network", "What the SS1 is called on your network (\"MiSTer\" by default).",
	  "Only if you renamed it, e.g. to tell two MiSTers apart." },
	{ "/etc/ssh/sshd_config", "SSH settings", "How remote login works: its port, whether passwords are allowed.",
	  "Rarely changed. Leave it alone unless you know you edited it." },
	{ "/etc/hosts", "Known names", "Names the SS1 uses for other machines on your network.",
	  "Only if you added entries yourself." },
};

static cJSON *gd_file(int i) { return cJSON_GetArrayItem(cJSON_GetObjectItem(gd.st, "files"), i); }
static cJSON *gd_point(int i) { return cJSON_GetArrayItem(cJSON_GetObjectItem(gd.st, "points"), i); }

static const char *gd_label(const char *l)
{
	if (!l) return "";
	if (!strcmp(l, "before-update-all")) return "Before Update All";
	if (!strcmp(l, "before-get-cores")) return "Before Get cores";
	if (!strcmp(l, "pre-rollback")) return "Before a roll back";
	if (!strcmp(l, "system")) return "Saved at start-up";
	if (!strcmp(l, "manual")) return "Saved by hand";
	return l;
}

/* "2026-10-23 02:02:43" -> "Oct 23, 2:02 AM" */
static void gd_when(const char *c, char *o, size_t n)
{
	static const char *const M[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	int y, mo, d, h, mi;
	if (!c || sscanf(c, "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) != 5 || mo < 1 || mo > 12) { snprintf(o, n, "%s", c ? c : ""); return; }
	if (SETTINGS.clock24) snprintf(o, n, "%s %d, %02d:%02d", M[mo - 1], d, h, mi);
	else snprintf(o, n, "%s %d, %d:%02d %s", M[mo - 1], d, h % 12 ? h % 12 : 12, mi, h < 12 ? "AM" : "PM");
}

/* left side: the files, then 3 buttons; right: the points, then 2 buttons */
static int gd_count(int side) { return side == 0 ? gd.nf + 3 : gd.np + 2; }

static void gd_check(lv_obj_t *par, int x, int y, int on, int focus)
{
	lv_obj_t *b = mk_box(par, S(20), S(20), on ? PAL.acc : PAL.bg, S(4));
	lv_obj_set_style_border_color(b, on || focus ? PAL.acc : PAL.dim, 0);
	lv_obj_set_style_border_width(b, S(2) > 1 ? S(2) : 1, 0);
	lv_obj_set_pos(b, x, y);
	if (on) { lv_obj_t *m = mk_label(b, FONT_SMALL, PAL.bg, "\xE2\x9C\x93"); lv_obj_center(m); }
}

static void gd_button(lv_obj_t *par, int x, int y, int w, const char *t, int primary, int focus)
{
	lv_obj_t *b = mk_box(par, w, S(36), primary ? PAL.acc : PAL.bg, S(7));
	lv_obj_set_style_border_color(b, focus ? PAL.text : primary ? PAL.acc : PAL.line, 0);
	lv_obj_set_style_border_width(b, focus ? (S(2) > 1 ? S(2) : 1) : 1, 0);
	lv_obj_set_pos(b, x, y);
	lv_obj_t *l = mk_label(b, FONT_NAME, primary ? PAL.bg : PAL.text, t);
	lv_obj_center(l);
}

static void gd_draw(void)
{
	lv_obj_clean(gd.left);
	lv_obj_clean(gd.right);
	cJSON *st = gd.st;
	int awaiting = cJSON_IsTrue(cJSON_GetObjectItem(st, "awaiting"));
	char t[200];
	/* left */
	lv_obj_t *h = mk_label(gd.left, FONT_LABEL, awaiting ? PAL.acc : PAL.dim, "");
	if (awaiting) {
		char when[40];
		gd_when(api_str(st, "updated_at"), when, sizeof(when));
		snprintf(t, sizeof(t), "THE LAST LINUX UPDATE  \xC2\xB7  %s \xE2\x86\x92 %s  \xC2\xB7  %s", api_str(st, "updated_from") ? api_str(st, "updated_from") : "?",
		         api_str(st, "linux") ? api_str(st, "linux") : "?", when);
	} else snprintf(t, sizeof(t), "YOUR SETTINGS  \xC2\xB7  LINUX %s", api_str(st, "linux") ? api_str(st, "linux") : "?");
	lv_label_set_text(h, t);
	lv_obj_t *intro = mk_label(gd.left, FONT_SMALL, PAL.dim, awaiting
		? "A Linux update replaces MiSTer's system files, so settings stored there go back to stock. Most people never change these: restore only what you set up yourself. Phosphor kept your copies."
		: "Nothing was reset. Phosphor keeps copies of these settings, in case a Linux update puts them back to stock. Most people never change them.");
	lv_obj_set_width(intro, SW(560));
	lv_label_set_long_mode(intro, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_pos(intro, 0, S(22));
	lv_obj_update_layout(intro);
	int y = S(22) + lv_obj_get_height(intro) + S(16);
	for (int i = 0; i < gd.nf; i++) {
		cJSON *f = gd_file(i);
		const char *path = api_str(f, "path");
		int reset = cJSON_IsTrue(cJSON_GetObjectItem(f, "reset")), k = -1;
		for (size_t j = 0; j < sizeof(GD_FILES) / sizeof(GD_FILES[0]); j++) if (path && !strcmp(path, GD_FILES[j].path)) k = (int)j;
		int focus = gd.side == 0 && gd.sel[0] == i;
		if (focus) {
			lv_obj_t *hl = mk_box(gd.left, SW(560), S(72), PAL.panel_on, S(7));
			lv_obj_set_pos(hl, -S(8), y - S(6));
		}
		gd_check(gd.left, 0, y, gd.tick_f[i], focus);
		lv_obj_t *nm = mk_label(gd.left, FONT_NAME, reset ? PAL.text : PAL.dim, k >= 0 ? GD_FILES[k].name : path);
		lv_obj_set_pos(nm, S(32), y - S(2));
		lv_obj_t *pl = mk_label(gd.left, FONT_LABEL, PAL.faint, path ? (strrchr(path, '/') + 1) : "");
		lv_obj_align_to(pl, nm, LV_ALIGN_OUT_RIGHT_MID, S(10), 0);
		lv_obj_t *stl = mk_label(gd.left, FONT_SMALL, reset ? PAL.acc : PAL.faint, reset ? "Reset by the update" : "Unchanged");
		lv_obj_set_pos(stl, SX(400), y);
		if (k >= 0) {
			lv_obj_t *w = mk_label(gd.left, FONT_SMALL, focus ? PAL.text : PAL.dim, GD_FILES[k].what);
			lv_obj_set_pos(w, S(32), y + S(22));
			lv_obj_t *wh = mk_label(gd.left, FONT_SMALL, focus ? PAL.dim : PAL.faint, GD_FILES[k].who);
			lv_obj_set_pos(wh, S(32), y + S(40));
		}
		y += S(72);
	}
	int by = y + S(6), b = gd.side == 0 ? gd.sel[0] - gd.nf : -1;
	gd_button(gd.left, 0, by, S(180), "Restore ticked", 1, b == 0);
	gd_button(gd.left, S(192), by, S(100), "Ignore", 0, b == 1);
	gd_button(gd.left, S(304), by, S(190), "Roll back Linux\xE2\x80\xA6", 0, b == 2);
	lv_obj_t *ig = mk_label(gd.left, FONT_SMALL, PAL.dim, "Ignore keeps the stock files and clears the alert; you can still roll back later.\nNetwork and SSH changes apply after a restart.");
	lv_obj_set_pos(ig, 0, by + S(46));
	/* right */
	lv_obj_t *rh = mk_label(gd.right, FONT_LABEL, PAL.dim, "RESTORE POINTS");
	(void)rh;
	lv_obj_t *ri = mk_label(gd.right, FONT_SMALL, PAL.dim, gd.np
		? "Saved each time you start Get cores or Update All. Use one if an\nupdate broke something: a core won't start, the menu misbehaves."
		: "None yet: one is saved each time you start Get cores or Update All.");
	lv_obj_set_pos(ri, 0, S(22));
	y = S(74);
	int show = gd.np < 5 ? gd.np : 5, from = 0;
	if (gd.side == 1 && gd.sel[1] < gd.np && gd.sel[1] >= show) from = gd.sel[1] - show + 1;
	for (int i = from; i < from + show; i++) {
		cJSON *p = gd_point(i);
		int focus = gd.side == 1 && gd.sel[1] == i;
		lv_obj_t *row = mk_box(gd.right, S(540), S(50), focus ? PAL.panel_on : PAL.panel, S(7));
		if (!focus) lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);   /* (only the chosen one picked out, as in lists) */
		lv_obj_set_pos(row, -S(8), y - S(6));
		gd_check(gd.right, 0, y + S(6), gd.tick_p[i], focus);
		char when[40];
		gd_when(api_str(p, "created"), when, sizeof(when));
		snprintf(t, sizeof(t), "%s  \xC2\xB7  %s", when, gd_label(api_str(p, "label")));
		lv_obj_t *l1 = mk_label(gd.right, FONT_NAME, PAL.text, t);
		lv_obj_set_pos(l1, S(32), y - S(3));
		snprintf(t, sizeof(t), "Linux %s", api_str(p, "linux") ? api_str(p, "linux") : "?");
		lv_obj_t *l2 = mk_label(gd.right, FONT_SMALL, PAL.dim, t);
		lv_obj_set_pos(l2, S(32), y + S(21));
		y += S(56);
	}
	by = y + S(6);
	b = gd.side == 1 ? gd.sel[1] - gd.np : -1;
	gd_button(gd.right, 0, by, S(190), "Roll back to ticked", 1, b == 0);
	gd_button(gd.right, S(202), by, S(140), "Delete ticked", 0, b == 1);
	lv_obj_t *rn = mk_label(gd.right, FONT_SMALL, PAL.dim, "Rolling back puts back MiSTer's Main, menu core and kernel from the\npoint and downloads that Linux version from MiSTer-devel (checked first).\nYour current set is saved as a point first, so it can be undone.");
	lv_obj_set_pos(rn, 0, by + S(46));
	/* the last job */
	cJSON *j = cJSON_GetObjectItem(st, "job");
	if (cJSON_IsTrue(cJSON_GetObjectItem(j, "busy"))) lv_label_set_text(gd.note, "Working\xE2\x80\xA6 (a roll back may be downloading Linux: a few minutes)");
	else lv_label_set_text(gd.note, "");
}

static void gd_load(cJSON *st)
{
	cJSON_Delete(gd.st);
	gd.st = st;
	gd.nf = cJSON_GetArraySize(cJSON_GetObjectItem(st, "files"));
	gd.np = cJSON_GetArraySize(cJSON_GetObjectItem(st, "points"));
	if (gd.nf > GD_MAXF) gd.nf = GD_MAXF;
	if (gd.np > GD_MAXP) gd.np = GD_MAXP;
	for (int i = 0; i < gd.nf; i++) gd.tick_f[i] = cJSON_IsTrue(cJSON_GetObjectItem(gd_file(i), "reset"));
	for (int i = 0; i < gd.np; i++) gd.tick_p[i] = 0;
	for (int s2 = 0; s2 < 2; s2++) if (gd.sel[s2] >= gd_count(s2)) gd.sel[s2] = gd_count(s2) - 1;
	guard_alert_set(guard_count(st));
}

static void show_guard(void)
{
	cur = SCR_GUARD;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Updates & restore points");
	gd.left = mk_box(body, SW(580), S(500), PAL.bg, 0);
	lv_obj_set_pos(gd.left, S(56), S(170));
	gd.right = mk_box(body, SW(560), S(500), PAL.bg, 0);
	lv_obj_set_pos(gd.right, SX(668), S(170));
	lv_obj_t *div = mk_box(body, S(1) > 0 ? S(1) : 1, S(460), PAL.line, 0);
	lv_obj_set_pos(div, SX(646), S(172));
	gd.note = mk_label(body, FONT_SMALL, PAL.acc, "");
	lv_obj_set_pos(gd.note, SX(668), S(140));
	gd.side = 0;
	gd.sel[0] = gd.sel[1] = 0;
	gd.st = NULL;
	gd_load(api_call("{\"cmd\":\"guard\"}"));
	if (theme_lr) {                                     /* two columns of explanations: not at 240 lines */
		lv_obj_set_hidden(div, true);
		lv_obj_set_hidden(gd.right, true);
		lv_obj_clean(gd.left);
		lv_obj_set_pos(gd.left, L(16), L(60));
		lv_obj_set_size(gd.left, W - L(32), H - L(90));
		lv_obj_t *m = mk_label(gd.left, FONT_BODY, PAL.dim, "Restore points need more room than 240p or 288p: use CRT mode 480i (Settings > Video), or Phosphor for Windows or Android.");
		lv_label_set_long_mode(m, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_set_width(m, W - L(32));
		static const char *const back[] = { "B", "Back", NULL };
		footer_set(back);
		return;
	}
	if (!gd.st || !api_ok(gd.st) || !cJSON_IsTrue(cJSON_GetObjectItem(gd.st, "installed"))) {
		lv_obj_clean(gd.left);
		mk_label(gd.left, FONT_BODY, PAL.dim, "The update-guard module isn't installed.");
	} else gd_draw();
	footer_set(GD_HINTS);
}

static void gd_do(const char *what, int side)
{
	char body2[2048];
	int k = snprintf(body2, sizeof(body2), "{\"cmd\":\"guard\",\"do\":\"%s\",\"items\":[", what);
	int n = 0;
	if (side == 0) {
		for (int i = 0; i < gd.nf; i++) if (gd.tick_f[i]) { const char *p = api_str(gd_file(i), "path"); if (p) k += snprintf(body2 + k, sizeof(body2) - k, "%s\"%s\"", n++ ? "," : "", p); }
	} else if (side == 1) {
		for (int i = 0; i < gd.np; i++) if (gd.tick_p[i]) { const char *p = api_str(gd_point(i), "name"); if (p) k += snprintf(body2 + k, sizeof(body2) - k, "%s\"%s\"", n++ ? "," : "", p); }
	}
	snprintf(body2 + k, sizeof(body2) - k, "]}");
	cJSON *r = api_call("%s", body2);
	if (!api_ok(r)) { toast_show(api_str(r, "error") ? api_str(r, "error") : "phosphord isn't answering"); cJSON_Delete(r); return; }
	snprintf(gd.job_last, sizeof(gd.job_last), "%s", what);
	gd.busy_seen = 1;
	gd_load(r);
	gd_draw();
}

/* while a job runs (only then): its result once it's done */
static void gd_tick(void)
{
	if (!gd.busy_seen) return;
	cJSON *r = api_call("{\"cmd\":\"guard\"}");
	if (!api_ok(r)) { cJSON_Delete(r); return; }
	cJSON *j = cJSON_GetObjectItem(r, "job");
	if (cJSON_IsTrue(cJSON_GetObjectItem(j, "busy"))) { cJSON_Delete(r); return; }
	gd.busy_seen = 0;
	int okj = cJSON_IsTrue(cJSON_GetObjectItem(j, "ok"));
	const char *out = api_str(j, "output");
	char first[160] = "";
	if (out) {
		/* the line that says what happened: the last non-empty one that isn't a hint */
		const char *p = out, *best = NULL;
		while (*p) {
			const char *e = strchr(p, '\n');
			size_t len = e ? (size_t)(e - p) : strlen(p);
			if (len && strncmp(p, "Undo with", 9) && strncmp(p, "Note:", 5) && strncmp(p, "To undo", 7)) best = p;
			if (!e) break;
			p = e + 1;
		}
		if (best) { size_t len = strcspn(best, "\n"); snprintf(first, sizeof(first), "%.*s", (int)(len < 150 ? len : 150), best); }
	}
	if (!strcmp(gd.job_last, "rollback") && okj) toast_show("Rolled back. Restart to start it (Settings > Restart)");
	else toast_show(first[0] ? first : okj ? "Done" : "That didn't work");
	if (cur == SCR_GUARD && !theme_lr) { gd_load(r); gd_draw(); } else { guard_alert_set(guard_count(r)); cJSON_Delete(r); }
}

static int gd_confirm(int btn, const char *msg)
{
	uint32_t now = lv_tick_get();
	if (gd.confirm_btn == btn && now - gd.confirm_at < 4000) { gd.confirm_btn = 0; return 1; }
	gd.confirm_btn = btn;
	gd.confirm_at = now ? now : 1;
	toast_show(msg);
	return 0;
}

static void guard_key(pkey_t k)
{
	if (!gd.st || !api_ok(gd.st) || theme_lr) { if (k == K_B || k == K_MENU) back_to_settings(SET_GUARD); return; }
	int *sel = &gd.sel[gd.side], n = gd_count(gd.side);
	switch (k) {
	case K_UP: if (*sel > 0) (*sel)--; break;
	case K_DOWN: if (*sel < n - 1) (*sel)++; break;
	case K_LEFT: case K_RIGHT: gd.side = !gd.side; if (gd.sel[gd.side] >= gd_count(gd.side)) gd.sel[gd.side] = 0; break;
	case K_B: case K_MENU: back_to_settings(SET_GUARD); return;
	case K_A:
		if (cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(gd.st, "job"), "busy")) || gd.busy_seen) { toast_show("Busy: wait for it to finish"); return; }
		if (gd.side == 0) {
			if (*sel < gd.nf) { gd.tick_f[*sel] = !gd.tick_f[*sel]; break; }
			int b = *sel - gd.nf, any = 0;
			for (int i = 0; i < gd.nf; i++) any |= gd.tick_f[i];
			if (b == 0) { if (!any) { toast_show("Tick the settings to bring back first"); return; } gd_do("restore", 0); return; }
			if (b == 1) { gd_do("keep", -1); return; }
			gd.side = 1; gd.sel[1] = 0; break;           /* Roll back Linux…: the restore points */
		} else {
			if (*sel < gd.np) {
				gd.tick_p[*sel] = !gd.tick_p[*sel];
				break;
			}
			int b = *sel - gd.np, cnt = 0;
			for (int i = 0; i < gd.np; i++) cnt += gd.tick_p[i];
			if (b == 0) {
				if (cnt != 1) { toast_show(cnt ? "Tick just one point to roll back to" : "Tick the point to roll back to first"); return; }
				if (!gd_confirm(1, "Press A again to roll back (a restart is needed after)")) return;
				gd_do("rollback", 1);
				return;
			}
			if (!cnt) { toast_show("Tick the points to delete first"); return; }
			if (!gd_confirm(2, "Press A again to delete them")) return;
			gd_do("delete", 1);
			return;
		}
		break;
	default: return;
	}
	gd_draw();
}

/* ---------- Other and Utility: the cores in _Other and _Utility (phosphord folder_cores) ----------
 * A list on the left, what the highlighted one is on the right; A starts it on its own (its menu picks
 * a file, as in MiSTer's menu). Utility starts with Register this SuperStation when Update All is there:
 * Update All's own settings on the TV (its account and Link unlock supporter cores: Coin-Op Collection,
 * JOTEGO...), through Phosphor's wrapper so a restore point comes first. */
static void cores_start_job_fwd(int all);
static struct { int util, reg, linked; cJSON *cores; vlist_t list; lv_obj_t *dtitle, *dtext, *dfile; } fo;
static cJSON *fo_core(int i) { return cJSON_GetArrayItem(fo.cores, i - fo.reg); }
static void fo_detail(void)
{
	int i = fo.list.sel;
	if (fo.reg && i == 0) {
		lv_label_set_text(fo.dtitle, "REGISTER THIS SUPERSTATION");
		lv_label_set_text(fo.dtext, "Opens Update All's own settings on the TV. Press a button during its countdown for the settings, "
		                  "sign in to your account there, then use Link so your supporter cores (Coin-Op Collection, JOTEGO and others) "
		                  "download on this SuperStation. Phosphor comes back by itself when Update All finishes.");
		lv_label_set_text(fo.dfile, fo.linked ? "Status: linked" : "Status: not linked yet");
		return;
	}
	cJSON *x = fo_core(i);
	char t[120];
	spaced_caps(t, sizeof(t), x && api_str(x, "name") ? api_str(x, "name") : "");
	lv_label_set_text(fo.dtitle, t);
	lv_label_set_text(fo.dtext, fo.util ? "A test or setup tool. It starts on its own, as from MiSTer's menu."
	                                    : "Starts on its own, as from MiSTer's menu; its own menu (the menu button) picks a file when it uses one.");
	const char *f = x ? api_str(x, "file") : NULL;
	const char *rel = f ? strstr(f, fo.util ? "_Utility/" : "_Other/") : NULL;
	lv_label_set_text(fo.dfile, rel ? rel : "");
}
static void fo_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	*dim = 0;
	if (fo.reg && i == 0) {
		snprintf(t, n, "Register this SuperStation");
		snprintf(val, vn, "%s", fo.linked ? "Linked" : "Update All");
		if (i == fo.list.sel) fo_detail();
		return;
	}
	cJSON *x = fo_core(i);
	snprintf(t, n, "%s", x && api_str(x, "name") ? api_str(x, "name") : "");
	snprintf(val, vn, "%s", x && api_str(x, "date") ? api_str(x, "date") : "");
	if (i == fo.list.sel) fo_detail();
}
static void show_folder(int util)
{
	cur = SCR_FOLDER;
	lv_obj_clean(body);
	fo.util = util;
	cJSON_Delete(fo.cores);
	cJSON *r = api_call("{\"cmd\":\"folder_cores\",\"folder\":\"%s\"}", util ? "_Utility" : "_Other");
	fo.cores = r ? cJSON_DetachItemFromObject(r, "cores") : NULL;
	fo.reg = util && r && cJSON_IsTrue(cJSON_GetObjectItem(r, "update_all"));
	fo.linked = r && cJSON_IsTrue(cJSON_GetObjectItem(r, "linked"));
	cJSON_Delete(r);
	screen_title("LIBRARY", util ? "Utility" : "Other");
	int n = cJSON_GetArraySize(fo.cores) + fo.reg;
	lv_obj_t *sub = mk_label(body, FONT_SMALL, PAL.dim, n ? (util ? "Test and setup tools from _Utility, and registering for supporter cores."
	                                                                 : "Everything MiSTer or Update All puts in _Other: small games, toys, music players and cores that use the ARM side.")
	                                                     : (util ? "There's nothing in _Utility yet." : "There's nothing in _Other yet: Update All puts cores there."));
	lv_obj_set_pos(sub, S(56), S(176));
	vlist_create(&fo.list, body, S(56), S(214), SW(700), S(420));
	fo.list.fill = fo_fill;
	fo.list.count = n;
	fo.list.sel = fo.list.top = 0;
	lv_obj_t *pan = mk_box(body, SW(424), LV_SIZE_CONTENT, PAL.panel, S(16));
	LR_HIDE(pan);
	lv_obj_set_style_border_width(pan, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(pan, PAL.line, 0);
	lv_obj_set_style_pad_all(pan, S(24), 0);
	lv_obj_set_flex_flow(pan, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(pan, S(12), 0);
	lv_obj_set_pos(pan, SX(800), S(214));
	lv_obj_set_hidden(pan, !n);
	fo.dtitle = mk_label(pan, FONT_LABEL, PAL.dim, "");
	lv_obj_set_style_text_letter_space(fo.dtitle, S(3), 0);
	fo.dtext = mk_label(pan, FONT_BODY, PAL.text, "");
	lv_label_set_long_mode(fo.dtext, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(fo.dtext, S(376));
	fo.dfile = mk_label(pan, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(fo.dfile, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(fo.dfile, S(376));
	static const char *const hints[] = { "A", "Start", "B", "Back", NULL };
	static const char *const uhints[] = { "A", "Open", "B", "Back", NULL };
	footer_set(util ? uhints : hints);
	vlist_refresh(&fo.list);
}
static void folder_key(pkey_t k)
{
	if (vlist_key(&fo.list, k)) return;
	if (k == K_B || k == K_MENU) { show_home(); return; }
	if (k != K_A || !fo.list.count) return;
	if (fo.reg && fo.list.sel == 0) { cores_start_job_fwd(1); return; }
	cJSON *x = fo_core(fo.list.sel);
	const char *f = x ? api_str(x, "file") : NULL;
	if (!f) return;
	char *q = NULL;
	cJSON *rq = cJSON_CreateObject();
	cJSON_AddStringToObject(rq, "cmd", "run_core");
	cJSON_AddStringToObject(rq, "file", f);
	q = cJSON_PrintUnformatted(rq);
	cJSON_Delete(rq);
	cJSON *r = api_call("%s", q);
	free(q);
	if (!api_ok(r)) toast_show(api_str(r, "error") ? api_str(r, "error") : "Couldn't start it");
	cJSON_Delete(r);
}

/* ---------- Wallpapers: the picker (phosphord wallpaper.c) ----------
 * Categories along the top (your own, then each Update All creator's, only those with pictures in the
 * aspect shown), L/R to switch; a grid of thumbnails (made by phosphord, one per tick as they show);
 * Y: 16:9 / 4:3 / All; X: what it's for (both menus, Phosphor's only, MiSTer's only); A uses it. */
#define WG_COLS 4
#define WG_ROWS 2
static void show_settings(void);
static struct {
	cJSON *cats, *items;
	int cat, sel, top, aspect, pending;            /* aspect 0 16:9, 1 4:3, 2 all; pending: thumbs still to come */
	uint32_t poll_at;
	int drawn_top;                                 /* the page whose thumbnails are up (-1: none) */
	int opening;                                   /* the first load since the picker opened */
	lv_obj_t *tabs, *grid, *cell[WG_COLS * WG_ROWS], *img[WG_COLS * WG_ROWS], *cap[WG_COLS * WG_ROWS], *use, *asp, *empty;
	char thumbs[WG_COLS * WG_ROWS][720];
} wg;
static const char *WG_ASP[3] = { "16:9", "4:3", "all" };
static const char *WG_USE[3] = { "Both menus", "Phosphor's menu", "MiSTer's menu (OSD)" };

static void wall_osd_apply(void)
{
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "wallpaper_osd");
	cJSON_AddStringToObject(q, "file", SETTINGS.wallpaper_on && SETTINGS.wall_osd != 1 ? SETTINGS.wallpaper : "");
	char *b = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON_Delete(api_call("%s", b));
	free(b);
}
static void wall_toggle(void)
{
	SETTINGS.wallpaper_on = !SETTINGS.wallpaper_on;
	settings_save();
	wall_osd_apply();
	bg_apply(root);
	int keep = set_row(SET_WALLPAPER);
	set_build();
	set_list.count = set_n;
	set_list.sel = keep;
	vlist_refresh(&set_list);
	si_update();
	if (SETTINGS.wallpaper_on && !SETTINGS.wallpaper[0]) toast_show("Choose a wallpaper below");
}
static void wg_draw(void);
static void wg_load(void)
{
	cJSON_Delete(wg.cats); cJSON_Delete(wg.items);
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "wallpapers");
	cJSON_AddStringToObject(q, "aspect", WG_ASP[wg.aspect]);
	cJSON_AddStringToObject(q, "current", SETTINGS.wallpaper);
	char *qs = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = qs ? api_call_slow(20000, "%s", qs) : NULL;
	free(qs);
	wg.cats = r ? cJSON_DetachItemFromObject(r, "categories") : NULL;
	int nc = cJSON_GetArraySize(wg.cats);
	if (wg.opening) {                                  /* opened: on the picture in use's collection, or */
		wg.opening = 0;                                /* the first with something in it */
		const char *cc = r ? api_str(r, "current_category") : NULL;
		int first = -1;
		for (int i = 0; i < nc; i++) {
			cJSON *c = cJSON_GetArrayItem(wg.cats, i);
			const char *id = api_str(c, "id");
			if (cc && id && !strcmp(id, cc)) { wg.cat = i; first = -2; break; }
			if (first == -1 && cJSON_GetNumberValue(cJSON_GetObjectItem(c, "count")) > 0) first = i;
		}
		if (first >= 0) wg.cat = first;
	}
	cJSON_Delete(r);
	if (wg.cat >= nc) wg.cat = nc ? nc - 1 : 0;
	cJSON *c = cJSON_GetArrayItem(wg.cats, wg.cat);
	r = c ? api_call_slow(20000, "{\"cmd\":\"wallpapers\",\"aspect\":\"%s\",\"category\":\"%s\"}", WG_ASP[wg.aspect], api_str(c, "id")) : NULL;
	wg.items = r ? cJSON_DetachItemFromObject(r, "items") : NULL;
	cJSON_Delete(r);
	wg.sel = wg.top = 0;
	wg.drawn_top = -1;
	int n = cJSON_GetArraySize(wg.items);
	for (int i = 0; i < n; i++) {                   /* start on the one in use */
		const char *f = api_str(cJSON_GetArrayItem(wg.items, i), "file");
		if (f && !strcmp(f, SETTINGS.wallpaper)) { wg.sel = i; wg.top = i / WG_COLS * WG_COLS; }
	}
	wg_draw();
}
static void wg_draw(void)
{
	lv_obj_clean(wg.tabs);
	int nc = cJSON_GetArraySize(wg.cats);
	if (nc > 1) mk_label(wg.tabs, FONT_SMALL, PAL.dim, "L");
	for (int i = 0; i < nc; i++) {
		cJSON *c = cJSON_GetArrayItem(wg.cats, i);
		lv_obj_t *t = mk_box(wg.tabs, LV_SIZE_CONTENT, S(36), i == wg.cat ? PAL.panel_on : PAL.bg, S(18));
		lv_obj_set_style_bg_opa(t, i == wg.cat ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
		lv_obj_set_style_border_width(t, i == wg.cat ? S(2) : 0, 0);
		lv_obj_set_style_border_color(t, PAL.acc, 0);
		lv_obj_set_style_pad_hor(t, S(16), 0);
		lv_obj_t *l = mk_label(t, FONT_BODY, i == wg.cat ? PAL.text : PAL.dim, api_str(c, "name") ? api_str(c, "name") : "");
		lv_obj_center(l);
	}
	if (nc > 1) mk_label(wg.tabs, FONT_SMALL, PAL.dim, "R");
	char a[48];
	snprintf(a, sizeof(a), "%s  (Y)", wg.aspect == 2 ? "All" : WG_ASP[wg.aspect]);
	lv_label_set_text(wg.asp, a);
	char u[80];
	snprintf(u, sizeof(u), "Use for:  %s  (X)", WG_USE[SETTINGS.wall_osd % 3]);
	lv_label_set_text(wg.use, u);
	int n = cJSON_GetArraySize(wg.items), newpage = wg.drawn_top != wg.top;
	wg.drawn_top = wg.top;
	for (int i = 0; i < WG_COLS * WG_ROWS; i++) {
		int k = wg.top + i;
		cJSON *x = k < n ? cJSON_GetArrayItem(wg.items, k) : NULL;
		lv_obj_set_hidden(wg.cell[i], !x);
		if (!x) continue;
		int on = k == wg.sel;
		const char *f = api_str(x, "file");
		int inuse = f && !strcmp(f, SETTINGS.wallpaper);
		lv_obj_set_style_border_width(wg.cell[i], on ? S(3) : inuse ? S(1) : 0, 0);
		lv_obj_set_style_border_color(wg.cell[i], PAL.acc, 0);
		char c[160];
		snprintf(c, sizeof(c), "%s%s", inuse ? "\xE2\x9C\x93  " : "", api_str(x, "name") ? api_str(x, "name") : "");
		lv_label_set_text(wg.cap[i], c);
		if (newpage) {
			lv_image_set_src(wg.img[i], NULL);
			wg.thumbs[i][0] = 0;
		}
	}
	if (!n) lv_label_set_text(wg.cap[0], "");
	cJSON *cc = cJSON_GetArrayItem(wg.cats, wg.cat);
	const char *cid = cc ? api_str(cc, "id") : NULL;
	lv_label_set_text(wg.empty, n ? ""
		: nc <= 1 && wg.aspect == 2 ? "No wallpapers yet. Put your own in the wallpapers_user folder on the SD card, or turn on a wallpaper collection in Update All's settings (Utility > Register this SuperStation opens it) and run it."
		: nc <= 1 ? "None in this shape yet: Y shows the others. To get some, put your own in the wallpapers_user folder on the SD card, or turn on a wallpaper collection in Update All's settings and run it."
		: cid && !strcmp(cid, "user") ? "None of your own in this shape: put pictures in the wallpapers_user folder on the SD card. L and R show the collections."
		: "None in this shape: Y shows the others.");
	/* thumbnails: phosphord's worker makes them (about a second each for a big picture), this page
	   first and then the next; wall_tick puts each in as it's ready */
	cJSON *q = cJSON_CreateObject(), *fl = cJSON_AddArrayToObject(q, "files");
	cJSON_AddStringToObject(q, "cmd", "wallpaper_prefetch");
	for (int k = wg.top; k < n && k < wg.top + 2 * WG_COLS * WG_ROWS; k++) {
		const char *f = api_str(cJSON_GetArrayItem(wg.items, k), "file");
		if (f) cJSON_AddItemToArray(fl, cJSON_CreateString(f));
	}
	char *b = n && newpage ? cJSON_PrintUnformatted(q) : NULL;
	cJSON_Delete(q);
	if (b) { cJSON_Delete(api_call("%s", b)); free(b); }
	if (newpage) { wg.pending = 1; wg.poll_at = 0; }
}
/* the page's thumbnails that are ready (wall_tick): a quick check every 300 ms, never a wait */
static void wall_tick(void)
{
	if (cur != SCR_WALL || !wg.pending) return;
	uint32_t now = lv_tick_get();
	if (wg.poll_at && (int32_t)(now - wg.poll_at) < 0) return;
	wg.poll_at = now + 300;
	int left = 0;
	for (int i = 0; i < WG_COLS * WG_ROWS; i++) {
		cJSON *x = cJSON_GetArrayItem(wg.items, wg.top + i);
		if (!x || wg.thumbs[i][0]) continue;
		cJSON *q = cJSON_CreateObject();
		cJSON_AddStringToObject(q, "cmd", "wallpaper_thumb");
		cJSON_AddStringToObject(q, "file", api_str(x, "file"));
		char *b = cJSON_PrintUnformatted(q);
		cJSON_Delete(q);
		cJSON *r = api_call("%s", b);
		free(b);
		const char *t = api_ok(r) ? api_str(r, "thumb") : NULL;
		if (t) {
			snprintf(wg.thumbs[i], sizeof(wg.thumbs[i]), "A:%s", t);
			lv_image_set_src(wg.img[i], wg.thumbs[i]);
		} else if (api_ok(r)) left++;              /* still being made (an unreadable one stays empty) */
		cJSON_Delete(r);
	}
	if (!left) wg.pending = 0;
}
static void show_wallpapers(void)
{
	cur = SCR_WALL;
	wg.opening = 1;
	lv_obj_clean(body);
	screen_title("SETTINGS \xC2\xB7 WALLPAPERS", "Choose a wallpaper");
	wg.tabs = mk_box(body, SW(900), S(40), PAL.bg, 0);
	lv_obj_set_style_bg_opa(wg.tabs, LV_OPA_TRANSP, 0);
	lv_obj_set_flex_flow(wg.tabs, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(wg.tabs, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_column(wg.tabs, S(10), 0);
	lv_obj_set_pos(wg.tabs, S(56), S(170));
	wg.asp = mk_label(body, FONT_BODY, PAL.text, "");
	lv_obj_set_pos(wg.asp, SX(1080), S(178));
	int cw = (SW(1168) - S(16) * (WG_COLS - 1)) / WG_COLS, ch = cw * 9 / 16;
	for (int i = 0; i < WG_COLS * WG_ROWS; i++) {
		lv_obj_t *c = mk_box(body, cw, ch + S(28), PAL.bg, S(10));
		lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
		lv_obj_set_pos(c, S(56) + (i % WG_COLS) * (cw + S(16)), S(226) + (i / WG_COLS) * (ch + S(44)));
		lv_obj_t *fr = mk_box(c, cw, ch, PAL.panel, S(10));
		lv_obj_set_style_clip_corner(fr, true, 0);
		wg.img[i] = lv_image_create(fr);
		lv_obj_set_size(wg.img[i], cw, ch);
		lv_image_set_inner_align(wg.img[i], LV_IMAGE_ALIGN_COVER);
		lv_obj_center(wg.img[i]);
		wg.cap[i] = mk_label(c, FONT_SMALL, PAL.dim, "");
		lv_label_set_long_mode(wg.cap[i], LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_width(wg.cap[i], cw);
		lv_obj_set_pos(wg.cap[i], S(4), ch + S(6));
		wg.cell[i] = c;
		lv_obj_set_style_border_side(c, LV_BORDER_SIDE_FULL, 0);
	}
	wg.empty = mk_label(body, FONT_BODY, PAL.dim, "");
	lv_label_set_long_mode(wg.empty, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(wg.empty, SW(900));
	lv_obj_set_pos(wg.empty, S(56), S(240));
	wg.use = mk_label(body, FONT_BODY, PAL.text, "");
	lv_obj_set_pos(wg.use, S(56), S(634));
	static const char *const hints[] = { "A", "Use this one", "L", "R", "R", "Category", "Y", "16:9 / 4:3 / All", "X", "Use for", "B", "Back", NULL };
	(void)hints;
	static const char *const h2[] = { "A", "Use this one", "Y", "16:9 / 4:3 / All", "X", "Use for", "B", "Back", NULL };
	footer_set(h2);
	wg_load();
}
static void wall_key(pkey_t k)
{
	int n = cJSON_GetArraySize(wg.items), nc = cJSON_GetArraySize(wg.cats), old = wg.sel;
	if (k == K_B || k == K_MENU) { back_to_settings(SET_WALL_PICK); return; }
	if (k == K_L && nc) { wg.cat = (wg.cat + nc - 1) % nc; wg_load(); return; }
	if (k == K_R && nc) { wg.cat = (wg.cat + 1) % nc; wg_load(); return; }
	if (k == K_Y) { wg.aspect = (wg.aspect + 1) % 3; wg_load(); return; }
	if (k == K_X) {
		SETTINGS.wall_osd = (SETTINGS.wall_osd + 1) % 3;
		settings_save();
		wall_osd_apply();
		bg_apply(root);
		wg_draw();
		return;
	}
	if (k == K_A && n) {
		const char *f = api_str(cJSON_GetArrayItem(wg.items, wg.sel), "file");
		if (!f) return;
		snprintf(SETTINGS.wallpaper, sizeof(SETTINGS.wallpaper), "%s", f);
		SETTINGS.wallpaper_on = 1;
		settings_save();
		wall_osd_apply();
		bg_apply(root);
		toast_show(SETTINGS.wall_osd == 2 ? "Behind MiSTer's menu from the next time it shows" : "Wallpaper set");
		wg_draw();
		return;
	}
	if (!n) return;
	if (k == K_LEFT && wg.sel > 0) wg.sel--;
	else if (k == K_RIGHT && wg.sel + 1 < n) wg.sel++;
	else if (k == K_UP && wg.sel >= WG_COLS) wg.sel -= WG_COLS;
	else if (k == K_DOWN && wg.sel + WG_COLS < n) wg.sel += WG_COLS;
	else if (k == K_DOWN && wg.sel / WG_COLS < (n - 1) / WG_COLS) wg.sel = n - 1;
	if (wg.sel == old) return;
	int ntop = wg.top;
	while (wg.sel < ntop) ntop -= WG_COLS;
	while (wg.sel >= ntop + WG_COLS * WG_ROWS) ntop += WG_COLS;
	if (ntop != wg.top) { wg.top = ntop; wg_draw(); }
	else {                                            /* just the highlight */
		for (int i = 0; i < WG_COLS * WG_ROWS; i++) {
			int kk = wg.top + i;
			const char *f = api_str(cJSON_GetArrayItem(wg.items, kk), "file");
			int inuse = f && !strcmp(f, SETTINGS.wallpaper);
			lv_obj_set_style_border_width(wg.cell[i], kk == wg.sel ? S(3) : inuse ? S(1) : 0, 0);
		}
	}
}

/* ---------- Scripts (everything in /media/fat/Scripts) ---------- */
static vlist_t sc_list;
static char sc_names[200][96];
static int sc_n;
static int sc_cmp(const void *a, const void *b) { return strcasecmp((const char *)a, (const char *)b); }
static void sc_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	(void)dim;
	snprintf(t, n, "%.*s", (int)(strlen(sc_names[i]) - 3), sc_names[i]);
	snprintf(val, vn, "Run >");
}
static void show_scripts(void)
{
	cur = SCR_SCRIPTS;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Scripts");
	sc_n = 0;
	DIR *d = opendir(SCRIPTS_DIR);
	struct dirent *e;
	while (d && (e = readdir(d)) && sc_n < 200) {
		size_t n = strlen(e->d_name);
		if (n > 3 && n < sizeof(sc_names[0]) && e->d_name[0] != '.' && !strcasecmp(e->d_name + n - 3, ".sh") && strcmp(e->d_name, "phosphor.sh"))
			snprintf(sc_names[sc_n++], sizeof(sc_names[0]), "%s", e->d_name);
	}
	if (d) closedir(d);
	qsort(sc_names, (size_t)sc_n, sizeof(sc_names[0]), sc_cmp);
	lv_obj_t *sub = mk_label(body, FONT_SMALL, PAL.dim, sc_n ? "The scripts in the SD card's Scripts folder. They run in MiSTer's terminal, as from MiSTer's Scripts menu." : "There are no scripts in the SD card's Scripts folder.");
	lv_obj_set_pos(sub, S(56), S(176));
	vlist_create(&sc_list, body, S(56), S(210), SW(1000), S(420));
	sc_list.count = sc_n;
	sc_list.fill = sc_fill;
	sc_list.sel = 0;
	sc_list.top = 0;
	static const char *const hints[] = { "A", "Run", "B", "Back", NULL };
	footer_set(hints);
	vlist_refresh(&sc_list);
}
static void scripts_key(pkey_t k)
{
	if (vlist_key(&sc_list, k)) return;
	if (k == K_B || k == K_MENU) { back_to_settings(SET_SCRIPTS); return; }
	if (k == K_A && sc_n) {
		char path[460];
		snprintf(path, sizeof(path), "%s/%s", SCRIPTS_DIR, sc_names[sc_list.sel]);
		run_script(path);
	}
}

/* ---------- Video: MiSTer's own video settings (MiSTer.ini and its profiles), tried with Keep ----------
 * The same list and words as the apps (video_opts.h, from apps/windows/web/video-options.js). Left/Right
 * pick a value, A tries it: phosphord keeps the old file, restarts Main with the change, and puts it back
 * by itself unless Keep comes in 35 s. L/R switch the profile shown; one not in use can be tried as a
 * whole. X tries the copy from before the last change. The Keep window (below) shows over any screen,
 * whoever started the try (this menu, the phone, the PC).
 * Profiles: up to four (Phosphor's Main, phosphord VT_ALT_MAX). Taki's values for MiSTer.ini and the four
 * SS1 profiles, and why (the SS1's Sony video encoder): docs/ss1-video.md, install/ss1_video_ini.sh. */
static vlist_t vd_list;
static void vk_check(int now);
static struct {
	cJSON *st;                         /* phosphord's vset reply */
	int nrows, row2opt[NVOPTS + 5];        /* the list: options in group order */
	int edited[NVOPTS];                /* a value picked but not tried yet: its index in vals */
	char file[64];
	lv_obj_t *chips, *panel, *pname, *prisk, *ptext, *pwarn, *pdip, *pfoot, *empty;
	int ncrt;                          /* Phosphor's CRT rows first (row2opt -1, -2, -3) */
	crt_cfg_t crt;                     /* as saved */
	int crt_pick;                      /* the mode picked (CRT_OFF: off), not tried yet; -2: none */
	int crt_dirty;                     /* Text or Overscan changed: the menu starts again on the way out */
} vd;
static const char *const VD_HINTS_CRT[] = { "\xE2\x86\x90\xE2\x86\x92", "Change", "A", "Try", "B", "Back", NULL };
static const char *const VD_HINTS_CRTCH[] = { "\xE2\x86\x90\xE2\x86\x92", "Change", "B", "Back (the menu starts again)", NULL };
static const char *const VD_HINTS_CRTPOS[] = { "\xE2\x86\x90\xE2\x86\x92", "Move the picture", "B", "Back", NULL };
static const int CRT_MODES[] = { CRT_OFF, CRT_240P, CRT_288P, CRT_480I, CRT_576I, CRT_480P };
#define NCRT_MODES 6
enum { CR_MODE, CR_TEXT, CR_OVER, CR_H, CR_V };
static int vd_crt_row(int i) { return i < vd.ncrt ? i : -1; }
static const char *const CRT_FONT_NAMES[] = { "Smooth", "Sharp", "Legible" };
static int vd_crt_shown(void) { return vd.crt_pick != -2 ? vd.crt_pick : vd.crt.on ? vd.crt.mode : CRT_OFF; }
static const char *const VD_HINTS[] = { "\xE2\x86\x90\xE2\x86\x92", "Change", "A", "Try", "X", "Earlier settings", "L/R", "Profile", "B", "Back", NULL };
static const char *const VD_HINTS_PROF[] = { "A", "Try this profile", "L/R", "Profile", "B", "Back", NULL };

static cJSON *vd_setting(const char *key) { return cJSON_GetObjectItem(cJSON_GetObjectItem(vd.st, "settings"), key); }
static const char *vd_now(int o)
{
	cJSON *s = vd_setting(VOPTS[o].key);
	return s && cJSON_IsTrue(cJSON_GetObjectItem(s, "set")) && api_str(s, "value") ? api_str(s, "value") : "";
}
static int vd_index(int o, const char *v)
{
	for (int i = 0; i < VOPTS[o].nv; i++) if (!strcmp(VOPTS[o].vals[i].v, v)) return i;
	return -1;
}
static const char *vd_label(int o, const char *v, char *buf, size_t n)
{
	int i = vd_index(o, v);
	if (i >= 0) return VOPTS[o].vals[i].label;
	if (!*v) { snprintf(buf, n, "Not set (%s)", VOPTS[o].def); return buf; }
	return v;
}
/* the value a row shows: the one picked, else the one in the file */
static const char *vd_shown(int o) { return vd.edited[o] >= 0 ? VOPTS[o].vals[vd.edited[o]].v : vd_now(o); }

static int vd_profile_count(void)
{
	int n = 0;
	cJSON *f;
	cJSON_ArrayForEach(f, cJSON_GetObjectItem(vd.st, "files")) if (api_num(f, "profile", -1) >= 0) n++;
	return n;
}
static cJSON *vd_profile(int k)
{
	int n = 0;
	cJSON *f;
	cJSON_ArrayForEach(f, cJSON_GetObjectItem(vd.st, "files")) if (api_num(f, "profile", -1) >= 0 && n++ == k) return f;
	return NULL;
}
static cJSON *vd_shown_profile(void)
{
	for (int k = 0; k < vd_profile_count(); k++) { cJSON *f = vd_profile(k); if (api_str(f, "name") && !strcmp(api_str(f, "name"), vd.file)) return f; }
	return vd_profile(0);
}

/* the DIP switches on the side of the SS1 a set of analog settings needs (dipFor in video-options.js) */
static int vd_dip(const char *mode, const char *csync, const char *pal, const char *label, char out[][64], int max)
{
	const char *reg = pal && !strcmp(pal, "1") ? "Up (PAL)" : "Down (NTSC)";
	int n = 0;
#define DIP(S, P) do { if (n < max) snprintf(out[n++], 64, "DIP %s   %s", S, P); } while (0)
	if (!*mode) mode = "rgb";
	if (!strcmp(mode, "subcarrier")) { DIP("3", "Down (Sony encoder)"); DIP("1", reg); DIP("2", reg); DIP("4", "Up (off)"); }
	else if (!strcmp(mode, "svideo") || !strcmp(mode, "cvbs")) { DIP("3", "Up (FPGA encoder)"); DIP("1", reg); DIP("2", reg); DIP("4", "Up (off)"); }
	else if (!strcmp(mode, "ypbpr")) DIP("4", "Down (sync on Y)");
	else if (csync && !strcmp(csync, "0")) DIP("4", "Up (off)");
	else if (label && !strcasecmp(label, "rgsb")) DIP("4", "Down (sync on green)");
	else DIP("4", "Up for SCART / separate sync, Down for sync on green");
#undef DIP
	return n;
}
static int vd_is_analog(const char *k) { return !strcmp(k, "vga_mode") || !strcmp(k, "composite_sync") || !strcmp(k, "vga_sog") || !strcmp(k, "menu_pal"); }
static int vd_opt(const char *k) { for (int o = 0; o < NVOPTS; o++) if (!strcmp(VOPTS[o].key, k)) return o; return -1; }

static void vd_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	int cr = vd_crt_row(i);
	if (cr >= 0) {
		*dim = 0;
		if (cr == 0) {
			snprintf(t, n, "CRT mode (analog out)");
			const char *l = crt_mode_name(vd_crt_shown());
			if (vd.crt_pick != -2) snprintf(val, vn, "<  %s  >", l);
			else snprintf(val, vn, "%s", l);
		} else if (cr == CR_TEXT) {
			snprintf(t, n, "CRT text (240p, 288p)");
			snprintf(val, vn, "<  %s  >", CRT_FONT_NAMES[SETTINGS.crt_font]);
			*dim = !vd.crt.on || (vd.crt.mode != CRT_240P && vd.crt.mode != CRT_288P);
		} else if (cr == CR_OVER) {
			snprintf(t, n, "CRT overscan");
			if (SETTINGS.crt_overscan) snprintf(val, vn, "<  %d%% per edge  >", SETTINGS.crt_overscan);
			else snprintf(val, vn, "<  Off  >");
			*dim = !vd.crt.on;
		} else {
			int x = cr == CR_H ? vd.crt.h : vd.crt.v;
			snprintf(t, n, cr == CR_H ? "CRT picture: left / right" : "CRT picture: up / down");
			if (!vd.crt.on) { snprintf(val, vn, "%+d", x); *dim = 1; }
			else snprintf(val, vn, "<  %+d  >", x);
		}
		return;
	}
	int o = vd.row2opt[i];
	char buf[64];
	const char *g = "";
	for (int k = 0; k < NVGROUPS; k++) if (!strcmp(VGROUPS_[k][0], VOPTS[o].group)) g = VGROUPS_[k][1];
	snprintf(t, n, "%s", VOPTS[o].name);
	(void)g;
	const char *l = vd_label(o, vd_shown(o), buf, sizeof(buf));
	int mx = NARROW ? 16 : 30;                    /* (a 4:3 screen: the name needs the room) */
	if (vd.edited[o] >= 0) snprintf(val, vn, "<  %.*s%s  >", mx - 4, l, (int)strlen(l) > mx - 4 ? "..." : "");
	else snprintf(val, vn, "%.*s%s", mx, l, (int)strlen(l) > mx ? "..." : "");
	*dim = 0;
}

static void vd_panel(void)
{
	cJSON *pf = vd_shown_profile();
	int using = !pf || cJSON_IsTrue(cJSON_GetObjectItem(pf, "active"));
	char dips[4][64];
	int nd = 0;
	lv_obj_set_hidden(vd_list.box, !using);
	lv_obj_set_hidden(vd.empty, using);
	if (!using) {
		char t[200];
		cJSON *act = NULL;
		for (int k = 0; k < vd_profile_count(); k++) if (cJSON_IsTrue(cJSON_GetObjectItem(vd_profile(k), "active"))) act = vd_profile(k);
		snprintf(t, sizeof(t), "%s isn't in use: Main uses %s now.", pf ? api_str(pf, "label") : "?", act ? api_str(act, "label") : "Main");
		lv_label_set_text(vd.empty, t);
		snprintf(t, sizeof(t), "Use %s", pf ? api_str(pf, "label") : "?");
		lv_label_set_text(vd.pname, t);
		lv_label_set_text(vd.prisk, "");
		snprintf(t, sizeof(t), "%s. Trying it restarts the menu with this profile; it goes back by itself in %d seconds unless you keep it.",
		         pf ? api_str(pf, "name") : "", (int)api_num(vd.st, "secs", 35));
		lv_label_set_text(vd.ptext, t);
		lv_label_set_text(vd.pwarn, "");
		cJSON *s = cJSON_GetObjectItem(vd.st, "settings");
		nd = vd_dip(api_str(cJSON_GetObjectItem(s, "vga_mode"), "value") ? api_str(cJSON_GetObjectItem(s, "vga_mode"), "value") : "",
		            api_str(cJSON_GetObjectItem(s, "composite_sync"), "value"), api_str(cJSON_GetObjectItem(s, "menu_pal"), "value"), pf ? api_str(pf, "label") : "", dips, 4);
		footer_set(VD_HINTS_PROF);
	} else if (vd_crt_row(vd_list.sel) >= 0) {
		int cr = vd_crt_row(vd_list.sel);
		lv_label_set_text(vd.pname, cr == 0 ? "CRT mode" : cr == CR_TEXT ? "CRT text" : cr == CR_OVER ? "CRT overscan" : "CRT picture position");
		lv_label_set_text(vd.prisk, cr == 0 ? "Phosphor's own setting (not MiSTer.ini)" : cr == CR_TEXT || cr == CR_OVER ? "The menu starts again with it" : "Changes right away");
		lv_obj_set_style_text_color(vd.prisk, PAL.dim, 0);
		lv_label_set_text(vd.ptext, cr == 0
			? "Phosphor's menu on a CRT, at the TV's own timing, out of the analog port (VGA, SCART, component). "
			  "240p, 288p, 480i and 576i are 15 kHz for a TV (288p and 576i for PAL sets); 480p is 31 kHz for a VGA monitor or a PVM / BVM that takes it. "
			  "HDMI shows the same picture. Games still use their own video and MiSTer.ini's settings. "
			  "In a game, Phosphor's in-game menu shows in MiSTer's menu window on the CRT."
			: cr == CR_TEXT ? "How the text looks at 240p and 288p. Smooth: soft edges. Sharp: no soft edges, crisp on a sharp monitor. Legible: a plainer, heavier typeface."
			: cr == CR_OVER ? "Most TVs cut the edges of the picture off. This keeps Phosphor's menu inside: a few per cent off each edge."
			: "Moves Phosphor's picture on the CRT so it's in the middle of the screen. Each step is one pixel across, or one line up or down.");
		lv_label_set_text(vd.pwarn, cr == 0 ? "A TV that can't show a mode shows nothing: the old mode comes back by itself unless you keep it."
		                                   : vd.crt.on ? "" : "Turn CRT mode on first.");
		if (!mainlink_has_crt()) lv_label_set_text(vd.pwarn, "Needs Phosphor's Main (0.27.42 or later) and Phosphor's menu core.");
		footer_set(cr == 0 ? VD_HINTS_CRT : cr == CR_TEXT || cr == CR_OVER ? VD_HINTS_CRTCH : VD_HINTS_CRTPOS);
	} else {
		int o = vd.row2opt[vd_list.sel];
		lv_label_set_text(vd.pname, VOPTS[o].name);
		const char *rw = "";
		int warn = 0;
		for (int k = 0; k < NVRISKS; k++) if (!strcmp(VRISKS[k].id, VOPTS[o].risk)) { rw = VRISKS[k].words; warn = VRISKS[k].warn; }
		lv_label_set_text(vd.prisk, rw);
		lv_obj_set_style_text_color(vd.prisk, warn ? PAL.acc : PAL.dim, 0);
		lv_label_set_text(vd.ptext, VOPTS[o].text);
		char w[400] = "";
		if (VOPTS[o].warn) snprintf(w, sizeof(w), "%s", VOPTS[o].warn);
		cJSON *ov = cJSON_GetObjectItem(vd_setting(VOPTS[o].key), "overrides");
		if (cJSON_GetArraySize(ov)) {
			size_t l = strlen(w);
			snprintf(w + l, sizeof(w) - l, "%sSome cores set their own:", l ? "  " : "");
			cJSON *x;
			cJSON_ArrayForEach(x, ov) { l = strlen(w); snprintf(w + l, sizeof(w) - l, " %s", x->valuestring); }
		}
		lv_label_set_text(vd.pwarn, w);
		if (vd_is_analog(VOPTS[o].key) && vd.edited[o] >= 0) {
			int m = vd_opt("vga_mode"), c = vd_opt("composite_sync"), p = vd_opt("menu_pal");
			nd = vd_dip(vd_shown(m), vd_shown(c), vd_shown(p), pf ? api_str(pf, "label") : "", dips, 4);
		}
		footer_set(VD_HINTS);
	}
	char d[400] = "";
	if (nd) {
		snprintf(d, sizeof(d), "Set on the side of the SS1 first:");
		for (int i = 0; i < nd; i++) { size_t l = strlen(d); snprintf(d + l, sizeof(d) - l, "\n%s", dips[i]); }
		size_t l = strlen(d);
		snprintf(d + l, sizeof(d) - l, "\nSS1 units made after December 2025. The ini and the switches have to agree.");
	}
	lv_label_set_text(vd.pdip, d);
	lv_obj_set_hidden(vd.pdip, !nd);
	char f[120];
	if (vd_crt_row(vd_list.sel) >= 0 && using)
		snprintf(f, sizeof(f), vd_crt_row(vd_list.sel) == 0 ? "Goes back by itself in %d seconds unless you keep it." : "", CRT_KEEP_SECS);
	else if (cJSON_IsTrue(cJSON_GetObjectItem(vd.st, "game_running"))) snprintf(f, sizeof(f), "A game is running: close it first.");
	else snprintf(f, sizeof(f), "Goes back by itself in %d seconds unless you keep it.", (int)api_num(vd.st, "secs", 35));
	lv_label_set_text(vd.pfoot, f);
}

static void vd_chips(void)
{
	lv_obj_clean(vd.chips);
	for (int k = 0; k < vd_profile_count(); k++) {
		cJSON *f = vd_profile(k);
		int shown = api_str(f, "name") && !strcmp(api_str(f, "name"), vd.file), act = cJSON_IsTrue(cJSON_GetObjectItem(f, "active"));
		lv_obj_t *c = mk_box(vd.chips, LV_SIZE_CONTENT, S(32), shown ? PAL.panel_on : PAL.bg, S(16));
		lv_obj_set_style_bg_opa(c, shown ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
		lv_obj_set_style_border_width(c, LV_MAX(1, S(1)), 0);
		lv_obj_set_style_border_color(c, shown ? PAL.acc : PAL.line, 0);
		lv_obj_set_style_pad_hor(c, S(16), 0);
		char t[48];
		snprintf(t, sizeof(t), "%s%s", api_str(f, "label") ? api_str(f, "label") : "?", act ? "  \xE2\x9C\x93" : "");
		lv_obj_t *l = mk_label(c, FONT_BODY, shown ? PAL.text : PAL.dim, t);
		lv_obj_center(l);
	}
}

static void vd_load(void)
{
	char req[120];
	if (vd.file[0]) snprintf(req, sizeof(req), "{\"cmd\":\"vset\",\"file\":\"%s\"}", vd.file);
	else snprintf(req, sizeof(req), "{\"cmd\":\"vset\"}");
	cJSON *r = api_call("%s", req);
	if (!api_ok(r)) {
		toast_show(api_str(r, "error") ? "Video settings need phosphord 0.27.25 or later" : "phosphord isn't answering");
		cJSON_Delete(r);
		return;
	}
	cJSON_Delete(vd.st);
	vd.st = r;
	snprintf(vd.file, sizeof(vd.file), "%s", api_str(r, "file") ? api_str(r, "file") : "MiSTer.ini");
	vd_chips();
	vlist_refresh(&vd_list);
	vd_panel();
}

static void show_video(void)
{
	cur = SCR_VIDEO;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Video");
	memset(vd.edited, 0xff, sizeof(vd.edited));
	vd.file[0] = 0;
	vd.nrows = 0;
	vd.ncrt = 5;
	crt_load(&vd.crt);
	vd.crt_pick = -2;
	for (int k = 0; k < vd.ncrt; k++) vd.row2opt[vd.nrows++] = -1 - k;
	for (int g = 0; g < NVGROUPS; g++)
		for (int o = 0; o < NVOPTS; o++) if (!strcmp(VOPTS[o].group, VGROUPS_[g][0])) vd.row2opt[vd.nrows++] = o;
	vd.chips = mk_box(body, W - S(112), S(36), PAL.bg, 0);
	lv_obj_set_style_bg_opa(vd.chips, LV_OPA_TRANSP, 0);
	lv_obj_set_flex_flow(vd.chips, LV_FLEX_FLOW_ROW);
	lv_obj_set_style_pad_column(vd.chips, S(10), 0);
	lv_obj_set_pos(vd.chips, S(56), S(170));
	int lw = W - S(112) - SW(500);
	vlist_create(&vd_list, body, S(56), S(220), lw, S(410));
	vd_list.fill = vd_fill;
	vd_list.count = vd.nrows;
	vd_list.sel = 0;
	vd_list.top = 0;
	vd.empty = mk_label(body, FONT_BODY, PAL.dim, "");
	lv_obj_set_width(vd.empty, lw - S(40));
	lv_label_set_long_mode(vd.empty, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_pos(vd.empty, S(80), S(232));
	if (theme_lr) { lv_obj_set_pos(vd.empty, L(16), L(64)); lv_obj_set_width(vd.empty, W - L(32)); }
	int px = S(56) + lw + S(30), pw2 = W - S(56) - px;
	vd.panel = mk_box(body, pw2, S(410), PAL.panel, S(14));
	if (theme_lr) { lv_obj_set_hidden(vd.panel, true); lv_obj_set_hidden(vd.chips, true); }
	lv_obj_set_pos(vd.panel, px, S(220));
	lv_obj_set_style_border_width(vd.panel, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(vd.panel, PAL.line, 0);
	lv_obj_set_flex_flow(vd.panel, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_all(vd.panel, S(22), 0);
	lv_obj_set_style_pad_row(vd.panel, S(10), 0);
	vd.pname = mk_label(vd.panel, FONT_TITLE, PAL.text, "");
	vd.prisk = mk_label(vd.panel, FONT_SMALL, PAL.dim, "");
	vd.ptext = mk_label(vd.panel, FONT_SMALL, PAL.dim, "");
	vd.pwarn = mk_label(vd.panel, FONT_SMALL, PAL.acc, "");
	vd.pdip = mk_label(vd.panel, FONT_SMALL, PAL.text, "");
	vd.pfoot = mk_label(vd.panel, FONT_SMALL, PAL.faint, "");
	lv_obj_t *ls[] = { vd.pname, vd.ptext, vd.pwarn, vd.pdip, vd.pfoot };
	for (size_t i = 0; i < sizeof(ls) / sizeof(ls[0]); i++) { lv_obj_set_width(ls[i], pw2 - S(44)); lv_label_set_long_mode(ls[i], LV_LABEL_LONG_MODE_WRAP); }
	lv_obj_set_style_border_width(vd.pdip, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(vd.pdip, PAL.acc, 0);
	lv_obj_set_style_border_side(vd.pdip, LV_BORDER_SIDE_LEFT, 0);
	lv_obj_set_style_pad_left(vd.pdip, S(12), 0);
	lv_obj_set_width(vd.pdip, pw2 - S(44));
	vd_load();
}

static void vd_try(const char *what)
{
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "vset");
	cJSON_AddStringToObject(q, "file", vd.file);
	if (!strcmp(what, "undo")) cJSON_AddBoolToObject(q, "undo", 1);
	else if (!strcmp(what, "profile")) cJSON_AddNumberToObject(q, "profile", api_num(vd_shown_profile(), "profile", 0));
	else {
		int o = vd.row2opt[vd_list.sel];
		cJSON *t = cJSON_AddObjectToObject(q, "try");
		cJSON_AddStringToObject(t, VOPTS[o].key, VOPTS[o].vals[vd.edited[o]].v);
		if (vd_is_analog(VOPTS[o].key))           /* analog settings go together */
			for (int x = 0; x < NVOPTS; x++) if (x != o && vd.edited[x] >= 0 && vd_is_analog(VOPTS[x].key)) cJSON_AddStringToObject(t, VOPTS[x].key, VOPTS[x].vals[vd.edited[x]].v);
	}
	char *s = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = api_call("%s", s);
	free(s);
	if (!api_ok(r)) { toast_show(api_str(r, "error") ? api_str(r, "error") : "phosphord isn't answering"); cJSON_Delete(r); return; }
	cJSON_Delete(r);
	for (int o = 0; o < NVOPTS; o++) if (strcmp(what, "value") || vd_is_analog(VOPTS[vd.row2opt[vd_list.sel]].key) ? vd_is_analog(VOPTS[o].key) || o == vd.row2opt[vd_list.sel] : o == vd.row2opt[vd_list.sel]) vd.edited[o] = -1;
	vk_check(1);                                      /* the Keep window, now */
}

static void video_key(pkey_t k)
{
	cJSON *pf = vd_shown_profile();
	int using = !pf || cJSON_IsTrue(cJSON_GetObjectItem(pf, "active"));   /* (no profiles known: the list, CRT rows and all) */
	if (k == K_L || k == K_R) {                      /* the profile shown */
		int n = vd_profile_count(), at = 0;
		for (int i = 0; i < n; i++) if (vd_profile(i) == pf) at = i;
		if (n < 2) return;
		at = (at + (k == K_R ? 1 : n - 1)) % n;
		snprintf(vd.file, sizeof(vd.file), "%s", api_str(vd_profile(at), "name"));
		memset(vd.edited, 0xff, sizeof(vd.edited));
		vd_load();
		return;
	}
	if (k == K_B || k == K_MENU) {
		int any = 0;
		for (int o = 0; o < NVOPTS; o++) if (vd.edited[o] >= 0) any = 1;
		if (vd.crt_pick != -2 && k == K_B) { vd.crt_pick = -2; vlist_refresh(&vd_list); vd_panel(); return; }
		if (vd.crt_dirty) { vd.crt_dirty = 0; setenv("PHOS_REOPEN", "video", 1); crt_request_restart(); return; }
		if (any && k == K_B) { memset(vd.edited, 0xff, sizeof(vd.edited)); vlist_refresh(&vd_list); vd_panel(); return; }
		back_to_settings(SET_VIDEO);
		return;
	}
	if (!using) { if (k == K_A) vd_try("profile"); return; }
	if (vlist_key(&vd_list, k)) { vd_panel(); return; }
	int cr = vd_crt_row(vd_list.sel);
	if (cr >= 0) {
		if (!mainlink_has_crt()) { if (k == K_LEFT || k == K_RIGHT || k == K_A) toast_show("CRT mode needs Phosphor's Main and menu core"); return; }
		if ((cr == CR_TEXT || cr == CR_OVER) && (k == K_LEFT || k == K_RIGHT)) {
			int d = k == K_RIGHT ? 1 : -1;
			if (cr == CR_TEXT) SETTINGS.crt_font = (SETTINGS.crt_font + 3 + d) % 3;
			else SETTINGS.crt_overscan = LV_CLAMP(0, SETTINGS.crt_overscan + d, 10);
			settings_save();
			vlist_refresh(&vd_list);
			vd_panel();
			if (vd.crt.on && (cr == CR_OVER || vd.crt.mode == CRT_240P || vd.crt.mode == CRT_288P)) vd.crt_dirty = 1;
			return;
		}
		if (cr == 0 && (k == K_LEFT || k == K_RIGHT)) {
			int at = 0, now = vd.crt.on ? vd.crt.mode : CRT_OFF;
			for (int i = 0; i < NCRT_MODES; i++) if (CRT_MODES[i] == vd_crt_shown()) at = i;
			at = (at + (k == K_RIGHT ? 1 : NCRT_MODES - 1)) % NCRT_MODES;
			vd.crt_pick = CRT_MODES[at] == now ? -2 : CRT_MODES[at];
			vlist_refresh(&vd_list);
			vd_panel();
		} else if (cr == 0 && k == K_A) {
			if (vd.crt_pick == -2) { toast_show("Pick another mode with left and right first"); return; }
			crt_cfg_t c = vd.crt;
			c.on = vd.crt_pick != CRT_OFF;
			if (c.on) c.mode = vd.crt_pick;
			setenv("PHOS_REOPEN", "video", 1);
			if (crt_try(&c)) { unsetenv("PHOS_REOPEN"); toast_show("Couldn't save the CRT setting"); return; }
			toast_show("Starting the menu again in the new mode");
		} else if (cr > 0 && (k == K_LEFT || k == K_RIGHT)) {
			if (!vd.crt.on) { toast_show("Turn CRT mode on first"); return; }
			int d = k == K_RIGHT ? 1 : -1;
			if (cr == CR_H) vd.crt.h = LV_CLAMP(-31, vd.crt.h + d, 31);
			else vd.crt.v = LV_CLAMP(-31, vd.crt.v + d, 31);
			crt_move(vd.crt.h, vd.crt.v);
			vlist_refresh(&vd_list);
		}
		return;
	}
	int o = vd.row2opt[vd_list.sel];
	if (k == K_LEFT || k == K_RIGHT) {
		int i = vd.edited[o] >= 0 ? vd.edited[o] : vd_index(o, vd_now(o));
		int nv = VOPTS[o].nv;
		i = i < 0 ? 0 : (i + (k == K_RIGHT ? 1 : nv - 1)) % nv;
		vd.edited[o] = strcmp(VOPTS[o].vals[i].v, vd_now(o)) ? i : -1;
		vlist_refresh(&vd_list);
		vd_panel();
	} else if (k == K_A) {
		if (vd.edited[o] < 0) { toast_show("Pick another value with left and right first"); return; }
		vd_try("value");
	} else if (k == K_X) vd_try("undo");
}

/* ---------- the Keep window: a video try waits for Keep (from here, the phone or the PC) ---------- */
static struct {
	lv_obj_t *box, *what, *secs, *bar;
	int open, total, crt;             /* crt: a CRT mode try (Phosphor's own, crt.c), not phosphord's */
	uint32_t until;                   /* lv_tick when it goes back by itself */
} vk;
static void vk_box(const char *what, int total, int left);

static void vk_close(void)
{
	if (vk.box) lv_obj_delete(vk.box);
	vk.box = NULL;
	vk.open = 0;
	vk.crt = 0;
}

static void vk_show(cJSON *p)
{
	vk_close();
	char what[200] = "";
	cJSON *ch = cJSON_GetObjectItem(p, "changes");
	const char *kind = api_str(p, "kind");
	if (kind && !strcmp(kind, "profile")) snprintf(what, sizeof(what), "Profile %d", (int)api_num(ch, "profile", 0));
	else if (cJSON_IsTrue(cJSON_GetObjectItem(ch, "undo"))) snprintf(what, sizeof(what), "The settings from before");
	else {
		cJSON *x;
		cJSON_ArrayForEach(x, ch) {
			int o = vd_opt(x->string);
			char buf[64];
			size_t l = strlen(what);
			if (o >= 0) snprintf(what + l, sizeof(what) - l, "%s%s: %s", l ? ", " : "", VOPTS[o].name, vd_label(o, x->valuestring ? x->valuestring : "", buf, sizeof(buf)));
		}
	}
	vk_box(what, (int)api_num(p, "secs", 35), (int)api_num(p, "secs_left", 35));
}

static void vk_box(const char *what, int total, int left)
{
	vk_close();
	int lr = theme_lr;                                  /* 240p / 288p: the same, in its own units */
#define VK(a, b) (lr ? L(b) : S(a))
	int w = lr ? W - L(32) : LV_MIN(S(620), W - S(40)), h = VK(270, 132);
	vk.box = mk_box(root, w, h, PAL.panel, VK(18, 6));
	lv_obj_set_style_border_width(vk.box, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(vk.box, PAL.acc, 0);
	lv_obj_center(vk.box);
	lv_obj_t *l = mk_label(vk.box, FONT_LABEL, PAL.dim, "T R Y I N G");
	lv_obj_set_pos(l, VK(30, 10), VK(24, 8));
	vk.what = mk_label(vk.box, FONT_TITLE, PAL.text, what);
	lv_obj_set_width(vk.what, w - VK(60, 20));
	lv_label_set_long_mode(vk.what, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_pos(vk.what, VK(30, 10), VK(48, 20));
	l = mk_label(vk.box, lr ? FONT_SMALL : FONT_BODY, PAL.dim, "Picture looks right? Keep it. Otherwise the old setting comes back by itself.");
	lv_obj_set_width(l, w - VK(60, 20));
	lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_pos(l, VK(30, 10), VK(88, 40));
	lv_obj_t *track = mk_box(vk.box, w - VK(160, 60), VK(6, 4), PAL.line, VK(3, 2));
	lv_obj_set_pos(track, VK(30, 10), VK(156, 74));
	vk.bar = mk_box(track, w - VK(160, 60), VK(6, 4), PAL.acc, VK(3, 2));
	vk.secs = mk_label(vk.box, FONT_NAME, PAL.acc, "");
	lv_obj_set_pos(vk.secs, w - VK(110, 44), VK(144, 68));
	int bw = (w - VK(60, 20) - VK(20, 8)) / 2, bh = VK(48, 22);
	lv_obj_t *b1 = mk_box(vk.box, bw, bh, PAL.acc, VK(24, 11));
	lv_obj_set_pos(b1, VK(30, 10), h - bh - VK(26, 10));
	l = mk_label(b1, FONT_NAME, PAL.bg, "A   Keep");
	lv_obj_center(l);
	lv_obj_t *b2 = mk_box(vk.box, bw, bh, PAL.panel, VK(24, 11));
	lv_obj_set_style_border_width(b2, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(b2, PAL.line, 0);
	lv_obj_set_pos(b2, w - VK(30, 10) - bw, h - bh - VK(26, 10));
	l = mk_label(b2, FONT_NAME, PAL.text, lr ? "B   Put back" : "B   Put back now");
	lv_obj_center(l);
#undef VK
	vk.total = total;
	vk.until = lv_tick_get() + (uint32_t)left * 1000;
	vk.open = 1;
	lv_obj_move_foreground(vk.box);
	ui_activity();
}

static void vk_tick(void)
{
	if (!vk.open) return;
	int32_t ms = (int32_t)(vk.until - lv_tick_get());
	int left = ms > 0 ? (ms + 999) / 1000 : 0;
	char t[16];
	snprintf(t, sizeof(t), "%d s", left);
	if (strcmp(lv_label_get_text(vk.secs), t)) lv_label_set_text(vk.secs, t);
	int tw = lv_obj_get_width(lv_obj_get_parent(vk.bar));
	lv_obj_set_width(vk.bar, LV_MAX(1, tw * left / (vk.total > 0 ? vk.total : 35)));
}

/* is a try waiting? (the marker phosphord keeps on the SD card: a stat, so it can be looked at often) */
static void vk_check(int now)
{
	static uint32_t last;
	uint32_t t = lv_tick_get();
	if (vk.crt) {                                   /* a CRT mode try: the old one back by itself */
		if ((int32_t)(vk.until - t) <= 0) { vk_close(); crt_revert(); }
		return;
	}
	if (!now && t - last < 1000) return;
	last = t;
	char p[300];
	snprintf(p, sizeof(p), "%s/phosphor/data/video_try", fat_dir());
	int waiting = access(p, F_OK) == 0;
	if (!waiting) {
		if (vk.open) {
			vk_close();
			if ((int32_t)(vk.until - t) <= 1000) toast_show("Put back: the old video setting is back");
			if (cur == SCR_VIDEO) vd_load();
		}
		return;
	}
	if (vk.open) return;
	cJSON *r = api_call("{\"cmd\":\"vset\"}");
	cJSON *pd = cJSON_GetObjectItem(r, "pending");
	if (api_ok(r) && cJSON_IsObject(pd)) {
		cJSON_AddNumberToObject(pd, "secs", api_num(r, "secs", 35));
		vk_show(pd);
	}
	cJSON_Delete(r);
}

static int vk_key(pkey_t k)
{
	if (!vk.open) return 0;
	if (vk.crt && (k == K_A || k == K_B)) {
		vk_close();
		if (k == K_A) { crt_keep(); toast_show("Kept"); }
		else crt_revert();                            /* (the shell starts again in the old mode) */
		return 1;
	}
	if (k == K_A || k == K_B) {
		cJSON *r = api_call(k == K_A ? "{\"cmd\":\"vset\",\"keep\":true}" : "{\"cmd\":\"vset\",\"revert\":true}");
		vk_close();
		toast_show(!api_ok(r) ? (api_str(r, "error") ? api_str(r, "error") : "phosphord isn't answering") : k == K_A ? "Kept" : "Put back");
		cJSON_Delete(r);
		if (cur == SCR_VIDEO) vd_load();
	}
	return 1;                                       /* nothing else while it's open */
}

/* ---------- Last update's log ----------
 * The downloader's own log (Update All runs the downloader too), cleaned up. Up/Down scroll, L/R a page, B back. */
static lv_obj_t *log_box, *log_lab;
static const char *const LOG_HINTS[] = { "\xE2\x86\x91\xE2\x86\x93", "Scroll", "L/R", "Page", "B", "Back", NULL };

/* the end of the log (up to max bytes), colour codes gone, carriage-return redraws applied */
static char *log_clean(size_t max)
{
	FILE *f = fopen(SCRIPT_LOG, "rb");
	if (!f) return strdup("(no update has run yet)");
	fseek(f, 0, SEEK_END);
	long size = ftell(f), from = size > (long)max ? size - (long)max : 0;
	fseek(f, from, SEEK_SET);
	char *raw = malloc((size_t)(size - from) + 1), *out = malloc((size_t)(size - from) + 2);
	size_t n = raw ? fread(raw, 1, (size_t)(size - from), f) : 0;
	fclose(f);
	if (!raw || !out) { free(raw); free(out); return strdup(""); }
	size_t i = 0, k = 0, line0 = 0;
	if (from) { while (i < n && raw[i] != '\n') i++; if (i < n) i++; }
	for (; i < n; i++) {
		unsigned char c = (unsigned char)raw[i];
		if (c == 0x1b) {
			i++;
			if (i < n && raw[i] == '[') { i++; while (i < n && !(raw[i] >= 0x40 && raw[i] <= 0x7E)) i++; }
			else if (i < n && raw[i] == ']') { while (i < n && raw[i] != 7) i++; }
			continue;
		}
		if (c == '\r') { if (i + 1 < n && raw[i + 1] == '\n') continue; k = line0; continue; }
		if (c == '\n') { out[k++] = '\n'; line0 = k; continue; }
		if (c >= 0x20 || c == '\t') out[k++] = (char)c;
	}
	out[k] = 0;
	free(raw);
	if (!k) { free(out); return strdup("(the log is empty)"); }
	return out;
}

/* The whole log, however long (Update All's runs to thousands of lines): only the lines on screen are
 * drawn, so it stays quick on the MiSTer's ARM. Long lines wrap at a fixed width. */
#define LOG_WRAP 150
static char *log_text;
static int *log_line, log_n, log_top, log_vis;
static lv_obj_t *log_pos;

static void log_draw(void)
{
	if (log_top > log_n - log_vis) log_top = log_n - log_vis;
	if (log_top < 0) log_top = 0;
	size_t cap = (size_t)log_vis * (LOG_WRAP + 2) + 1, k = 0;
	char *b = malloc(cap);
	if (!b) return;
	for (int i = log_top; i < log_n && i < log_top + log_vis; i++) {
		int a = log_line[i], e = log_line[i + 1];
		while (e > a && (log_text[e - 1] == '\n')) e--;
		if (e - a > LOG_WRAP) e = a + LOG_WRAP;
		memcpy(b + k, log_text + a, (size_t)(e - a));
		k += (size_t)(e - a);
		b[k++] = '\n';
	}
	if (k) k--;
	b[k] = 0;
	lv_label_set_text(log_lab, b);
	free(b);
	char p[64];
	snprintf(p, sizeof(p), "lines %d\xE2\x80\x93%d of %d", log_n ? log_top + 1 : 0, LV_MIN(log_top + log_vis, log_n), log_n);
	lv_label_set_text(log_pos, p);
}

static void show_log(void)
{
	cur = SCR_LOG;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Last update's log");
	log_box = mk_box(body, W - S(112), H - S(180) - S(84), lv_color_hex(0x000000), S(10));
	lv_obj_set_pos(log_box, S(56), S(176));
	lv_obj_set_style_pad_all(log_box, S(16), 0);
	log_lab = mk_label(log_box, FONT_SMALL, PAL.text, "");
	lv_label_set_long_mode(log_lab, LV_LABEL_LONG_MODE_CLIP);
	lv_obj_set_size(log_lab, W - S(112) - S(32), H - S(180) - S(84) - S(32));
	log_pos = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_obj_align(log_pos, LV_ALIGN_TOP_RIGHT, -S(56), S(136));
	free(log_text);
	free(log_line);
	log_text = log_clean(4u * 1024 * 1024);
	/* line starts, with long lines cut into LOG_WRAP pieces */
	int cap = 1024;
	log_line = malloc(sizeof(int) * (size_t)cap);
	log_n = 0;
	for (int a = 0, n = (int)strlen(log_text); a < n; ) {
		int e = a;
		while (e < n && log_text[e] != '\n' && e - a < LOG_WRAP) e++;
		if (e < n && log_text[e] == '\n') e++;
		if (log_n + 2 >= cap) { cap *= 2; log_line = realloc(log_line, sizeof(int) * (size_t)cap); }
		log_line[log_n++] = a;
		a = e;
	}
	log_line[log_n] = (int)strlen(log_text);
	log_vis = (H - S(180) - S(84) - S(32)) / lv_font_get_line_height(FONT_SMALL);
	if (log_vis < 1) log_vis = 1;
	log_top = log_n - log_vis;                 /* the end: what happened last */
	log_draw();
	footer_set(LOG_HINTS);
}

static void log_key(pkey_t k)
{
	switch (k) {
	case K_UP: log_top -= 3; log_draw(); break;
	case K_DOWN: log_top += 3; log_draw(); break;
	case K_L: case K_LEFT: log_top -= log_vis - 2; log_draw(); break;
	case K_R: case K_RIGHT: log_top += log_vis - 2; log_draw(); break;
	case K_B: case K_MENU: log_box = NULL; back_to_settings(SET_UPDLOG); return;
	default: break;
	}
}

static void back_to_settings(int sel)
{
	if (in_setup) { show_welcome(); return; }
	show_settings();
	set_list.sel = set_row(sel);
	vlist_refresh(&set_list);
	si_update();
}

static void settings_change(int dir)
{
	if (set_id(set_list.sel) == SET_ACCENT) {
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
		si_update();
	} else if (set_id(set_list.sel) == SET_TEXTURE && SETTINGS.wallpaper_on) {
		toast_show("Turn Wallpapers off to use a texture");
	} else if (set_id(set_list.sel) == SET_WALLPAPER) {
		wall_toggle();
	} else if (set_id(set_list.sel) == SET_TEXTURE) {
		SETTINGS.texture = texture_step(SETTINGS.texture, dir ? dir : 1);
		settings_save();
		bg_apply(root);
		vlist_refresh(&set_list);
		si_update();                          /* (the swatch holds the old tile until then) */
	} else if (set_id(set_list.sel) == SET_SAVER) {
		static const int opts[] = { 0, 5, 10, 20, 30 };
		int i = 0;
		while (i < 5 && opts[i] != SETTINGS.saver_min) i++;
		i = ((i < 5 ? i : 2) + dir + 5) % 5;
		SETTINGS.saver_min = opts[i];
		settings_save();
		vlist_refresh(&set_list);
	} else if (set_id(set_list.sel) == SET_SAMBA) {
		if (!api_ok(samba_st) || !samba_b("available")) { toast_show("MiSTer's Samba isn't on this system"); return; }
		if (samba_b("busy")) { toast_show("Wait a moment, it's still changing"); return; }
		int on = samba_b("on") && samba_b("running");
		cJSON *r = api_call("{\"cmd\":\"samba\",\"on\":%s}", on ? "false" : "true");
		if (!api_ok(r)) toast_show(r && api_str(r, "error") ? api_str(r, "error") : "phosphord isn't answering");
		else toast_show(on ? "Turning sharing off..." : "Turning sharing on: a few seconds...");
		cJSON_Delete(samba_st);
		samba_st = r;
		vlist_refresh(&set_list);
	} else if (set_id(set_list.sel) == SET_CLOCK) {
		SETTINGS.clock24 = !SETTINGS.clock24;
		settings_save();
		status_update();
		vlist_refresh(&set_list);
	}
}

/* ---------- Settings: the info panel ----------
 * Beside the list: what the highlighted setting is set to now, and what it's for. Built from what the
 * shell already has; rows with a screen of their own (WiFi, Bluetooth, Remote access, Cloud saves,
 * Backups, Look for games) ask phosphord once, after the cursor has rested on them, never on a timer.
 * Controllers & memory cards asks nothing, so the card reader stays asleep until its screen opens. */

#define SI_ROWS 5
#define SI_REST_MS 300
static struct {
	lv_obj_t *box, *title, *pair[SI_ROWS], *k[SI_ROWS], *v[SI_ROWS], *swatch, *rule, *about, *tip;
	int row;                  /* the row on show (-1: none yet) */
	cJSON *ext;               /* phosphord's answer for ext_row */
	int ext_row, ext_asked;
	uint32_t moved_at;
} si = { .row = -1, .ext_row = -1 };

static const char *const SI_TEXT[SET_COUNT][2] = {
	[SET_WIFI] = { "Join a WiFi network, or switch between saved ones. Networks you've joined are remembered and stay on this MiSTer.",
	               "Needed for updates, box art and cloud saves. Ethernet on the dock works too." },
	[SET_BT] = { "Pair Bluetooth controllers and keyboards, and forget ones you no longer use.",
	             "Put the controller in pairing mode first, then choose Pair." },
	[SET_REMOTE] = { "Lets the Phosphor phone app and Phosphor Link on a PC reach this MiSTer over your home network, after you pair them with a code shown here.",
	                 "Only paired devices get in. Nothing goes over the internet." },
	[SET_SAMBA] = { "Shares the SD card and the games drive as a network folder, so you can copy games and files from a PC.",
	                "Leave it off when you don't need it: anyone on your network can open the share." },
	[SET_CLOUD] = { "Sends your saves, save states and core settings to your own storage after each game, with older versions kept. Getting them back is only on request, with a preview first.",
	                "WiFi, Bluetooth pairings and passwords are never sent." },
	[SET_BACKUP] = { "Snapshots of your saves and settings, taken when you leave a game and once a day. Restore just the saves, or everything, from any snapshot.",
	                 "Every restore takes a snapshot first, so it can be undone." },
	[SET_STORAGE] = { "The SD card and the NVMe / USB drives: space, checks, a speed test, repairs, and putting back a drive that dropped off.",
	                  "Only a repair changes a drive, and only after you say so twice." },
	[SET_DISCS] = { "Play games from the dock's CD/DVD drive: PlayStation, Saturn, Mega-CD, PC Engine CD, Neo Geo CD, CD-i and 3DO, and MSU-1 / MD+ discs. A disc shows at the top of Disc while it's in.",
	                "Disc support by Anime0t4ku and bonna79 (Main_MiSTer_Physical_Disc)." },
	[SET_RA] = { "Achievements in your games, from retroachievements.org: log in once, then games on NES, SNES, Mega Drive, N64, PlayStation, Game Boy, GBA and more start on the RetroAchievements cores. Your progress and each game's achievements are here too.",
	             "RetroAchievements support by odelot (Main_MiSTer, RA cores); rcheevos by the RetroAchievements team." },
	[SET_SHOTS] = { "Press both sticks in (L3 + R3) to save a picture of the screen: the game, or Phosphor's own screens. They're on the SD card in screenshots, by system.",
	                "Off: the combo does nothing." },
	[SET_PORTS] = { "The controllers and memory cards in the front ports: see what's plugged in, and back up or copy memory card saves.",
	                "The ports are only read while this screen is open." },
	[SET_VIDEO] = { "MiSTer's own video settings: HDMI resolution, refresh and lag, scaling, HDR, the TV remote over HDMI, and the analog outputs, each explained. Try a change: it goes back by itself in 35 seconds unless you keep it.",
	                 "Analog settings say which DIP switches on the side of the SS1 they need." },
	[SET_ACCENT] = { "The colour used for highlights, the wordmark and the background texture. Left and right change it.", NULL },
	[SET_TEXTURE] = { "A faint pattern behind the menus, in the accent colour. Left and right step through them; it changes as you go.",
	                  "Drawn once when you pick it, so it costs nothing while you use the menu." },
	[SET_WALLPAPER] = { "A picture behind the menus instead of the texture: Update All's wallpaper collections (Ranny Snice, Anime0t4ku and others), or your own in wallpapers_user.",
	                    "The same picture can go behind MiSTer's own menu too, or a different one there." },
	[SET_WALL_PICK] = { "Choose the wallpaper: L and R switch between your own and each creator's, Y between 16:9, 4:3 and all, X what it's for (both menus, Phosphor's or MiSTer's).", NULL },
	[SET_CLOCK] = { "How the clock in the top corner shows the time.", NULL },
	[SET_SAVER] = { "Dims the screen after a while with no buttons pressed, to save the TV from burn-in. Any button wakes it.", NULL },
	[SET_RESCAN] = { "Looks through the SD card and the games drive for new or removed games and works out what each one is. Run it after adding games.",
	                 "The menu waits while it works; B twice stops it." },
	[SET_SCRAPE] = { "Fetches box art, descriptions and release details for your games from the sources in Box art sources.",
	                 "Needs a network. Runs only when you start it, on its own screen; B twice there stops it." },
	[SET_SCRAPE_INFO] = { "How far box art has got: games found, still to do, and ones no source knew.", NULL },
	[SET_SOURCES] = { "Which sites box art and details come from, and in what order. The first answer for each game wins.",
	                  "Some sources need a free account key, in phosphor/config/scraper.ini." },
	[SET_CORES] = { "Runs MiSTer's own downloader to get the cores, or update them. A restore point is saved first.",
	                "Takes a while on a fresh card. Cores go to the games drive when there is one." },
	[SET_UPDATE_ALL] = { "Runs Update All: cores, plus extras such as arcade ROMs, BIOS files and other databases you've chosen in it. A restore point is saved first.",
	                     "Use a single-port power adapter: shared multi-port chargers can cut out under the load." },
	[SET_PHOSUPD] = { "Installs a new version of Phosphor from a release package, or goes back to the version before. Your library, box art and settings stay.",
	                  "The installed version is backed up first; MiSTer's own files aren't touched." },
	[SET_GUARD] = { "Linux updates can put some settings back to stock (network address, SSH keys). This shows which, puts them back, or rolls back to a restore point.", NULL },
	[SET_SCRIPTS] = { "Runs the scripts in /media/fat/Scripts, the way MiSTer's own Scripts menu does.",
	                  "Controllers work as keys while a script runs." },
	[SET_UPDLOG] = { "The downloader's log from the last update, to see what changed or what went wrong.", NULL },
	[SET_DIAG] = { "Checks the network, drives, temperature and services, and makes a report to share when something isn't working.",
	               "Also counts times the power went in the middle of an update or other heavy work: three means the power adapter needs a look." },
	[SET_MISTER] = { "Leaves Phosphor for MiSTer's own menu, the OSD, and MiSTer's own settings. Launch Phosphor in its System page to come back here.", NULL },
	[SET_CTRL] = { "Your USB and Bluetooth controllers: map their buttons for MiSTer's menu and every core, see them work live, or put them back to automatic.",
	               "Every change is backed up first, and can be put back from Backups there." },
	[SET_RESTART] = { "Restarts the MiSTer.", "Saves are written as they happen, so nothing is lost." },
	[SET_SHUTDOWN] = { "Turns the MiSTer off safely: every drive is finished with first, so the SD card and USB drives come out clean. When the screen says it's safe, switch the power off.",
	                   "Also: Select + Start together, anywhere in Phosphor." },
	[SET_ABOUT] = { "Versions, licences and credits for Phosphor and the parts it's built on.", NULL },
};

static int si_wants_ext(int row)
{
	return row == SET_WIFI || row == SET_BT || row == SET_REMOTE || row == SET_CLOUD || row == SET_BACKUP || row == SET_RESCAN;
}

static void si_when(long t, char *out, size_t n)
{
	if (t <= 0) { snprintf(out, n, "Never"); return; }
	time_t now = time(NULL), tt = (time_t)t;
	struct tm a, b;
	localtime_r(&tt, &a);
	localtime_r(&now, &b);
	char clk[16];
	strftime(clk, sizeof(clk), SETTINGS.clock24 ? "%H:%M" : "%I:%M %p", &a);
	const char *c = clk[0] == '0' && !SETTINGS.clock24 ? clk + 1 : clk;
	if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday) snprintf(out, n, "Today, %s", c);
	else if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday - 1) snprintf(out, n, "Yesterday, %s", c);
	else { char d[16]; strftime(d, sizeof(d), a.tm_year == b.tm_year ? "%b %d" : "%b %d %Y", &a); snprintf(out, n, "%s, %s", d, c); }
}

/* the state lines for a row: returns how many; *hi marks a line drawn in the accent colour */
static int si_state(int row, char k[][40], char v[][96], int *hi)
{
	int n = 0;
	*hi = -1;
	cJSON *e = si.ext_row == row ? si.ext : NULL;
#define LINE(K, ...) do { if (n < SI_ROWS) { snprintf(k[n], 40, "%s", K); snprintf(v[n], 96, __VA_ARGS__); n++; } } while (0)
	switch (row) {
	case SET_WIFI: {
		cJSON *s = cJSON_GetObjectItem(e, "status");
		if (!e) break;
		if (!cJSON_IsTrue(cJSON_GetObjectItem(s, "present"))) { LINE("WiFi", "No adapter"); break; }
		if (cJSON_IsTrue(cJSON_GetObjectItem(s, "connected"))) {
			int sig = (int)api_num(s, "signal", 0);
			*hi = 0;
			LINE("Network", "%s", api_str(s, "ssid") ? api_str(s, "ssid") : "?");
			if (sig) LINE("Signal", "%s", sig >= -55 ? "Strong" : sig >= -67 ? "Good" : sig >= -75 ? "Fair" : "Weak");
			LINE("Address", "%s", api_str(s, "ip") && *api_str(s, "ip") ? api_str(s, "ip") : "Waiting for one");
		} else LINE("Network", "Not connected");
		LINE("Saved networks", "%d", cJSON_GetArraySize(cJSON_GetObjectItem(e, "saved")));
		break;
	}
	case SET_BT: {
		if (!e) break;
		if (!cJSON_IsTrue(cJSON_GetObjectItem(e, "present"))) { LINE("Bluetooth", "No adapter"); break; }
		cJSON *d, *ds = cJSON_GetObjectItem(e, "devices");
		int paired = cJSON_GetArraySize(ds), conn = 0;
		LINE("Paired", "%d", paired);
		cJSON_ArrayForEach(d, ds) if (cJSON_IsTrue(cJSON_GetObjectItem(d, "connected"))) {
			if (conn++ < 3) LINE(conn == 1 ? "Connected" : "", "%s", api_str(d, "name") ? api_str(d, "name") : "?");
		}
		if (!conn) LINE("Connected", "None");
		break;
	}
	case SET_REMOTE: {
		if (!e) break;
		int on = cJSON_IsTrue(cJSON_GetObjectItem(e, "on"));
		*hi = 0;
		LINE("Status", "%s", !on ? "Off" : cJSON_IsTrue(cJSON_GetObjectItem(e, "listening")) ? "On" : "On, not listening");
		cJSON *a = cJSON_GetArrayItem(cJSON_GetObjectItem(e, "addresses"), 0);
		if (on && cJSON_IsString(a)) LINE("Address", "%s:%d", a->valuestring, (int)api_num(e, "port", 0));
		LINE("Paired devices", "%d", cJSON_GetArraySize(cJSON_GetObjectItem(e, "devices")));
		break;
	}
	case SET_SAMBA:
		if (!api_ok(samba_st) || !samba_b("available")) { LINE("Status", "Not on this system"); break; }
		*hi = 0;
		LINE("Status", "%s", samba_b("on") && samba_b("running") ? "On" : samba_b("on") ? "On, not running" : "Off");
		if (samba_b("on") && samba_b("running")) {
			char ip[64];
			my_ip(ip, sizeof(ip));
			LINE("Address", "\\\\%s", ip);
			LINE("Sign in", "root  /  1");
		}
		break;
	case SET_CLOUD: {
		if (!e) break;
		cJSON *s = cJSON_GetObjectItem(e, "status");
		const char *rc = api_str(s, "rclone"), *rem = api_str(s, "remote"), *err = api_str(s, "last_error");
		if (!cJSON_IsTrue(cJSON_GetObjectItem(e, "installed"))) { LINE("Status", "Not installed"); break; }
		if (!rc || !*rc) { LINE("Status", "Needs setting up"); break; }
		if (!rem || !*rem) { LINE("Storage place", "Not chosen yet"); break; }
		char w[40];
		{   /* cloud-sync gives "YYYY-MM-DD HH:MM:SS" (local time), or nothing */
			const char *lp = api_str(s, "last_push");
			struct tm tm;
			memset(&tm, 0, sizeof(tm));
			if (lp && sscanf(lp, "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
				tm.tm_year -= 1900;
				tm.tm_mon -= 1;
				tm.tm_isdst = -1;
				si_when((long)mktime(&tm), w, sizeof(w));
			}
			else snprintf(w, sizeof(w), "Not yet");
		}
		LINE("Storage place", "%s", rem);
		LINE("Last sent", "%s", w);
		LINE("Waiting to send", "%s", api_str(s, "pending") && !strcmp(api_str(s, "pending"), "1") ? "Yes" : "Nothing");
		if (err && *err) { *hi = n; LINE("Last problem", "%.90s", err); }
		break;
	}
	case SET_BACKUP: {
		if (!e) break;
		cJSON *s = cJSON_GetObjectItem(e, "status");
		if (!cJSON_IsTrue(cJSON_GetObjectItem(e, "installed"))) { LINE("Status", "Not installed"); break; }
		const char *at = api_str(s, "last_at"), *dest = api_str(s, "dest"), *err = api_str(s, "last_error");
		LINE("Snapshots", "%d", cJSON_GetArraySize(cJSON_GetObjectItem(e, "snapshots")));
		LINE("Newest", "%s", at && *at ? at : "None yet");
		if (dest && *dest) LINE("Kept in", "%s", dest);
		if (api_str(s, "free_mb")) LINE("Free space there", "%s MB", api_str(s, "free_mb"));
		if (err && *err) { *hi = n; LINE("Last problem", "%.90s", err); }
		break;
	}
	case SET_ACCENT: *hi = 0; LINE("Set to", "%s", theme_accent_name(theme_accent())); break;
	case SET_TEXTURE:
		*hi = 0;
		LINE("Set to", "%s", texture_name((texture_t)SETTINGS.texture));
		if (SETTINGS.texture) LINE("Drawn in", "%s (the accent colour)", theme_accent_name(theme_accent()));
		break;
	case SET_CLOCK: *hi = 0; LINE("Set to", "%s", SETTINGS.clock24 ? "24-hour" : "12-hour"); break;
	case SET_SAVER:
		*hi = 0;
		if (SETTINGS.saver_min) LINE("Set to", "After %d minutes", SETTINGS.saver_min);
		else LINE("Set to", "Never");
		break;
	case SET_RESCAN:
		if (e) LINE("Games in the library", "%d", (int)api_num(e, "games", 0));
		break;
	case SET_SCRAPE: case SET_SCRAPE_INFO:
		if (!api_ok(scrape_state)) break;
		*hi = 0;
		LINE("Status", "%s", cJSON_IsTrue(cJSON_GetObjectItem(scrape_state, "on")) ? "Working" : api_str(scrape_state, "status") ? api_str(scrape_state, "status") : "");
		LINE("Found", "%d", (int)api_num(scrape_state, "found", 0));
		LINE("Still to do", "%d", (int)api_num(scrape_state, "pending", 0));
		LINE("Not found", "%d", (int)api_num(scrape_state, "not_found", 0));
		break;
	case SET_CORES:
		LINE("MiSTer's updater", "%s", cr.available ? "On the SD card" : "Not found");
		break;
	case SET_UPDATE_ALL:
		LINE("Update All", "%s", cr.all_available ? "On the SD card" : "Not on the SD card");
		break;
	case SET_GUARD:
		if (guard_resets > 0) { *hi = 0; LINE("Reset by an update", "%d setting%s", guard_resets, guard_resets == 1 ? "" : "s"); }
		else if (guard_resets == 0) LINE("Settings", "As you left them");
		break;
	case SET_DIAG:
		if (!pw.known) break;
		if (pw.warn) *hi = 0;
		LINE("Power events", "%d  (warns at %d)", pw.count, pw.warn_at);
		if (pw.nev) {
			char w[40];
			si_when(pw.ev[0].at, w, sizeof(w));
			LINE("Last one", "%.40s, %.40s", pw.ev[0].what, pw.ev[0].at < 1000000000L ? "time unknown" : w);
		}
		break;
	case SET_ABOUT: {
		char ip[64];
		my_ip(ip, sizeof(ip));
		LINE("Phosphor", "%s", PHOS_VERSION);
		LINE("Address", "%s", ip);
		break;
	}
	default: break;
	}
#undef LINE
	return n;
}

/* a row is waiting for its status: the loop wakes for it */
static int si_pending(void) { return si.box && cur == SCR_SETTINGS && si_wants_ext(si.row) && !si.ext_asked; }

static void si_set(lv_obj_t *l, const char *t)
{
	const char *was = lv_label_get_text(l);
	if (!was || strcmp(was, t)) lv_label_set_text(l, t);
}

static void si_update(void)
{
	if (!si.box || cur != SCR_SETTINGS) return;
	int row = set_id(set_list.sel);
	if (row < 0 || row >= SET_COUNT) return;
	uint32_t now = lv_tick_get();
	if (row != si.row) {
		si.row = row;
		si.moved_at = now ? now : 1;
		si.ext_asked = 0;
	}
	/* rested on a row with a screen of its own: ask phosphord once */
	if (si_wants_ext(row) && !si.ext_asked && now - si.moved_at >= SI_REST_MS) {
		si.ext_asked = 1;
		cJSON_Delete(si.ext);
		si.ext = api_call(row == SET_WIFI ? "{\"cmd\":\"wifi\"}" : row == SET_BT ? "{\"cmd\":\"bluetooth\"}" :
		                  row == SET_REMOTE ? "{\"cmd\":\"remote\"}" : row == SET_CLOUD ? "{\"cmd\":\"cloud\"}" :
		                  row == SET_BACKUP ? "{\"cmd\":\"backup\"}" : "{\"cmd\":\"status\"}");
		si.ext_row = api_ok(si.ext) ? row : -1;
	}
	char t[96], val[96];
	int dim = 0;
	set_fill(set_list.sel, t, sizeof(t), val, sizeof(val), &dim);
	if (row == SET_SCRAPE_INFO) snprintf(t, sizeof(t), "Box art progress");
	si_set(si.title, t);
	char k[SI_ROWS][40], v[SI_ROWS][96];
	int hi, n = si_state(row, k, v, &hi);
	if (!n && si_wants_ext(row)) { snprintf(k[0], 40, "Status"); snprintf(v[0], 96, "%s", si.ext_asked ? "phosphord isn't answering" : "..."); n = 1; hi = -1; }
	for (int i = 0; i < SI_ROWS; i++) {
		lv_obj_set_hidden(si.pair[i], i >= n);
		if (i >= n) continue;
		si_set(si.k[i], k[i]);
		si_set(si.v[i], v[i]);
		lv_obj_set_style_text_color(si.v[i], PAL.text, 0);   /* (every value the same: none picked out) */
		(void)hi;
		/* the value one line, in what the name leaves (a long one ends in ..., never wraps) */
		lv_text_attributes_t ta;
		lv_text_attributes_init(&ta);
		int kw = lv_text_get_width(k[i], strlen(k[i]), FONT_BODY, &ta), iw = lv_obj_get_width(si.pair[i]);
		lv_obj_set_size(si.v[i], LV_MAX(S(60), iw - kw - S(16)), lv_font_get_line_height(FONT_BODY));
	}
	/* the texture: a piece of the real tile */
	int sw = row == SET_ACCENT;                        /* (the accent's colour bar; no texture box) */
	lv_obj_set_hidden(si.swatch, !sw);
	if (sw) {
		const void *tile = row == SET_TEXTURE ? texture_tile() : NULL;
		lv_obj_set_style_bg_color(si.swatch, row == SET_ACCENT ? PAL.acc : PAL.bg, 0);
		lv_obj_set_style_bg_image_src(si.swatch, tile, 0);
		lv_obj_set_style_bg_image_tiled(si.swatch, tile != NULL, 0);
		lv_obj_set_height(si.swatch, row == SET_ACCENT ? S(14) : S(84));
	}
	lv_obj_set_hidden(si.rule, false);                 /* (every row the same: name, details, a line, then what it does) */
	si_set(si.about, SI_TEXT[row][0] ? SI_TEXT[row][0] : "");
	si_set(si.tip, SI_TEXT[row][1] ? SI_TEXT[row][1] : "");
	lv_obj_set_hidden(si.tip, !SI_TEXT[row][1]);
}

static void si_create(lv_obj_t *parent, int x, int y, int w, int h)
{
	si.box = mk_box(parent, w, h, PAL.panel, S(10));
	if (theme_lr) lv_obj_set_hidden(si.box, true);     /* 240p: no room beside the list */
	lv_obj_set_pos(si.box, x, y);
	lv_obj_set_style_border_color(si.box, PAL.line, 0);
	lv_obj_set_style_border_width(si.box, S(1) > 0 ? S(1) : 1, 0);
	lv_obj_set_style_pad_all(si.box, S(24), 0);
	lv_obj_set_style_pad_row(si.box, S(8), 0);
	lv_obj_set_flex_flow(si.box, LV_FLEX_FLOW_COLUMN);
	int iw = w - 2 * S(24);
	si.title = mk_label(si.box, FONT_TITLE, PAL.text, "");
	lv_obj_set_width(si.title, iw);
	lv_label_set_long_mode(si.title, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_style_pad_bottom(si.title, S(8), 0);
	for (int i = 0; i < SI_ROWS; i++) {
		si.pair[i] = lv_obj_create(si.box);
		lv_obj_remove_style_all(si.pair[i]);
		lv_obj_set_size(si.pair[i], iw, LV_SIZE_CONTENT);
		si.k[i] = mk_label(si.pair[i], FONT_BODY, PAL.dim, "");
		si.v[i] = mk_label(si.pair[i], FONT_BODY, PAL.text, "");
		lv_obj_set_width(si.v[i], iw * 62 / 100);
		lv_obj_set_style_text_align(si.v[i], LV_TEXT_ALIGN_RIGHT, 0);
		lv_label_set_long_mode(si.v[i], LV_LABEL_LONG_MODE_DOTS);
		lv_obj_align(si.v[i], LV_ALIGN_TOP_RIGHT, 0, 0);
		lv_obj_set_hidden(si.pair[i], true);
	}
	si.swatch = mk_box(si.box, iw, S(84), PAL.bg, S(4));
	lv_obj_set_style_border_color(si.swatch, PAL.line, 0);
	lv_obj_set_style_border_width(si.swatch, 1, 0);
	lv_obj_set_hidden(si.swatch, true);
	si.rule = mk_box(si.box, iw, 1, PAL.line, 0);
	lv_obj_set_style_margin_top(si.rule, S(8), 0);
	lv_obj_set_style_margin_bottom(si.rule, S(8), 0);
	si.about = mk_label(si.box, FONT_BODY, PAL.text, "");
	lv_obj_set_width(si.about, iw);
	lv_label_set_long_mode(si.about, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_style_text_line_space(si.about, S(4), 0);
	si.tip = mk_label(si.box, FONT_SMALL, PAL.dim, "");
	lv_obj_set_width(si.tip, iw);
	lv_label_set_long_mode(si.tip, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_style_margin_top(si.tip, S(6), 0);
	si.row = -1;
	si.ext_row = -1;
	cJSON_Delete(si.ext);
	si.ext = NULL;
}

static void show_settings(void)
{
	cur = SCR_SETTINGS;
	scrape_refresh();
	lv_obj_clean(body);
	screen_title("SETTINGS", "Settings");
	vlist_create(&set_list, body, S(56), S(176), SW(700), S(440));
	set_build();
	set_list.count = set_n;
	set_list.fill = set_fill;
	set_list.wrap = 1;
	set_list.sel = 0;
	set_list.top = 0;
	vlist_refresh(&set_list);
	si_create(body, SX(780), S(170), SW(444), S(452));
	si_update();
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
	src_list.count = nsrcs;
	vlist_refresh(&src_list);
}

static void show_sources(void)
{
	cur = SCR_SOURCES;
	sources_load(api_call("{\"cmd\":\"scrape_sources\"}"));
	lv_obj_clean(body);
	screen_title("SETTINGS", "Box art sources");
	vlist_create(&src_list, body, S(56), S(176), SW(760), S(330));
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
	lv_obj_set_width(note, SW(760));
	lv_obj_set_pos(note, S(56), S(176) + src_list.vis * src_list.row_h + S(14));
	LR_HIDE(note);
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
		set_list.sel = set_row(SET_SOURCES);
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

/* the Power section at the top of Diagnostics, while there are power events since the last Dismiss.
 * Returns its height (0: none). */
static int diag_power(int y)
{
	if (!pw.known || pw.count <= 0) return 0;
	int x = S(56), w = W - S(112), h = S(232), pad = S(22);
	lv_obj_t *box = mk_box(body, w, h, PAL.panel, S(10));
	lv_obj_set_pos(box, x, y);
	lv_obj_set_style_border_color(box, pw.warn ? PAL.acc : PAL.line, 0);
	lv_obj_set_style_border_width(box, S(1) > 1 ? S(1) : 1, 0);
	char t[160];
	char cap[16];
	spaced_caps(cap, sizeof(cap), "POWER");
	lv_obj_t *l = mk_label(box, FONT_LABEL, PAL.acc, cap);
	lv_obj_set_pos(l, pad, S(14));
	if (pw.warn) snprintf(t, sizeof(t), "The MiSTer lost power %d times in the middle of heavy work", pw.count);
	else snprintf(t, sizeof(t), "The MiSTer lost power %d time%s in the middle of heavy work (a warning comes at %d)",
	              pw.count, pw.count == 1 ? "" : "s", pw.warn_at);
	l = mk_label(box, FONT_TITLE, PAL.text, t);
	lv_obj_set_width(l, w - 2 * pad);
	lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_pos(l, pad, S(36));
	l = mk_label(box, FONT_SMALL, PAL.dim, "No crash, no error, no log: it just restarted. That's what a power adapter that can't keep up looks like.");
	lv_obj_set_width(l, w - 2 * pad);
	lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_pos(l, pad, S(68));
	int col = (w - 2 * pad) * 55 / 100;
	for (int i = 0; i < pw.nev && i < 3; i++) {
		char when[40];
		si_when(pw.ev[i].at, when, sizeof(when));
		if (pw.ev[i].at < 1000000000L) snprintf(when, sizeof(when), "Time unknown");   /* no clock yet when it started */
		l = mk_label(box, FONT_BODY, PAL.dim, when);
		lv_obj_set_pos(l, pad, S(100) + i * S(24));
		snprintf(t, sizeof(t), "Lost power during %s", pw.ev[i].what);
		l = mk_label(box, FONT_BODY, PAL.text, t);
		lv_obj_set_width(l, col - S(170));
		lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_pos(l, pad + S(170), S(100) + i * S(24));
	}
	lv_obj_t *rule = mk_box(box, 1, S(84), PAL.line, 0);
	lv_obj_set_pos(rule, pad + col, S(100));
	l = mk_label(box, FONT_NAME, PAL.text, "What helps");
	lv_obj_set_pos(l, pad + col + S(22), S(98));
	l = mk_label(box, FONT_SMALL, PAL.dim, "A single-port USB-C PD adapter, 9 V / 3 A or more (the Steam Deck adapter works well). "
	                                       "Not a shared multi-port charger: it moves power between ports and can dip under load.");
	lv_obj_set_width(l, w - 2 * pad - col - S(22));
	lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_pos(l, pad + col + S(22), S(124));
	lv_obj_t *btn = mk_box(box, S(190), S(34), PAL.acc, S(8));
	lv_obj_set_pos(btn, pad, h - S(34) - S(16));
	l = mk_label(btn, FONT_NAME, PAL.bg, "Dismiss (fixed it)");
	lv_obj_center(l);
	l = mk_label(box, FONT_SMALL, PAL.faint, "The count starts again. Switching off in the middle of an update counts once too.");
	lv_obj_set_pos(l, pad + S(210), h - S(34) - S(16) + S(9));
	return h + S(16);
}

static void show_diag(void)
{
	cur = SCR_DIAG;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Diagnostics");
	power_check();
	diag_head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_obj_set_pos(diag_head, S(56), S(178));
	diag_sub = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(diag_sub, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(diag_sub, W - S(112), S(22));
	lv_obj_set_pos(diag_sub, S(56), S(208));
	lr_head_sub(diag_head, diag_sub);
	int ph = diag_power(S(240));
	vlist_create(&diag_list, body, S(56), S(244) + ph, W - S(112), S(380) - ph);
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
	static const char *const phints[] = { "A", "Dismiss", "Y", "Check again", "X", "Save a report for support", "B", "Back", NULL };
	footer_set(ph ? phints : hints);
}

static void diag_key(pkey_t k)
{
	if (vlist_key(&diag_list, k)) return;
	if (k == K_A && pw.count > 0) {
		power_apply(api_call("{\"cmd\":\"power\",\"do\":\"ack\"}"));
		toast_show("Dismissed: the count starts again");
		show_diag();
		return;
	}
	if (k == K_Y && !diag_busy[0]) diag_apply(api_call("{\"cmd\":\"diag\",\"run\":true}"));
	else if (k == K_X && !diag_busy[0]) diag_apply(api_call("{\"cmd\":\"diag\",\"bundle\":true}"));
	else if (k == K_B || k == K_MENU) {
		show_settings();
		set_list.sel = set_row(SET_DIAG);
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
	vlist_create(&wifi_list, body, S(56), S(244), SW(900), S(380));
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
enum { RR_ON, RR_PAIR, RR_COUNT };
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
			snprintf(sub, sizeof(sub), "Paired devices can see and edit the library, box art and video settings. WiFi and pairing stay here.");
		}
		lv_label_set_text(rem_head, h);
		lv_label_set_text(rem_sub, sub);
		rem_list.count = RR_COUNT + nrdevs;
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
	if (i == RR_ON) { snprintf(t, n, "Remote access"); snprintf(val, vn, "<  %s  >", r_on ? "On" : "Off"); *dim = !r_on; return; }
	if (i == RR_PAIR) { snprintf(t, n, "Pair a device"); snprintf(val, vn, r_on ? ">" : "turn it on first"); *dim = !r_on; return; }
	rdev_t *d = &rdevs[i - RR_COUNT];
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
	pair_box = mk_box(body, SW(720), S(300), PAL.panel, S(16));
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
	vlist_create(&rem_list, body, S(56), S(244), SW(900), S(380));
	rem_list.fill = rem_fill;
	rem_list.sel = 0;
	rem_list.top = 0;
	rem_list.count = RR_COUNT;
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
	if (i == RR_ON && (k == K_A || k == K_LEFT || k == K_RIGHT)) {
		remote_apply(api_call("{\"cmd\":\"remote\",\"on\":%s}", r_on ? "false" : "true"));
		toast_show(r_on ? "Remote access is on" : "Remote access is off");
	} else if (i == RR_PAIR && k == K_A) {
		if (!r_on) toast_show("Turn remote access on first");
		else pair_open();
	} else if (i >= RR_COUNT && k == K_X && i - RR_COUNT < nrdevs) {
		char name[64];
		snprintf(name, sizeof(name), "%s", rdevs[i - RR_COUNT].name);
		cJSON *r = api_call("{\"cmd\":\"remote\",\"do\":\"forget\",\"id\":\"%s\"}", rdevs[i - RR_COUNT].id);
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
		if (!cl.job_ok) {                 /* a failure: the reason is the last line, not "Downloading..." */
			const char *e = out + strlen(out), *b;
			while (e > out && (e[-1] == '\n' || e[-1] == ' ')) e--;
			for (b = e; b > out && b[-1] != '\n'; b--) ;
			if (e > b) snprintf(first, sizeof(first), "%.*s", (int)(e - b), b);
		}
		if (!strcmp(cl.job, "devices") && cl.job_ok) { cl_split_output(out); cl.mode = CL_DEVICES; cl_list.sel = 0; }
		else if (!strcmp(cl.job, "preview")) { cl_split_output(out); cl.mode = CL_PREVIEW; cl_list.sel = 0; }
		else if (!strcmp(cl.job, "push")) toast_show(cl.job_ok ? "Saves sent" : "Sending failed");
		else if (!strcmp(cl.job, "pull")) toast_show(cl.job_ok ? "Saves are back" : "Couldn't get them back");
		else if (!strcmp(cl.job, "test")) toast_show(cl.job_ok ? "Connection OK" : first);
		else if (!strcmp(cl.job, "setup")) {
			toast_show(cl.job_ok ? "rclone is ready: now choose where saves go" : first);
			if (cl.job_ok && !cl.remote[0]) {      /* straight on to the next step */
				cl.mode = cl.nplaces ? CL_PLACES : CL_TYPES;
				cl_list.sel = 0;
				cl_list.top = 0;
			}
		}
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
		snprintf(sub, sizeof(sub), "Next: Storage place, then Add (Google Drive, Dropbox, OneDrive: from the phone app).");
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
	/* nowhere to send yet: the cursor goes to the one row that does something */
	if (cl.mode == CL_MAIN && cl.ready && !cl.remote[0] && !cl.busy) cl_list.sel = 2;
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
	lr_head_sub(cl_head, cl_sub);
	vlist_create(&cl_list, body, S(56), S(244), SW(900), S(380));
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
	lr_head_sub(bk_head, bk_sub);
	vlist_create(&bk_list, body, S(56), S(244), SW(900), S(380));
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
enum { PT_CARDS, PT_TEST, PT_BACKUP1, PT_BACKUP2, PT_FIRST_FILE };   /* the front ports are for games (SNAC), not the menu */
static struct {
	int mode, busy, job_ok, available, can_load, main_api, in_game, done, total, loading;
	char job[16], message[200];
	char names[80][200];
	long long at[80];
	int nfiles;
	char pick[200];
	int ask_name, asked;       /* a new card waits for a name; the keyboard was opened for it */
	char suggest[64];
	char pad[2][160];
	int pad_id[2], pad_conn[2];
} pt;
static vlist_t pt_list;
static lv_obj_t *pt_head, *pt_padl[2], *pt_sub;

/* the card map, like SS1 Tool's: 32 x 32 blocks, one a sector, filling in as the card is read or written */
static lv_obj_t *mc_box, *mc_canvas, *mc_title, *mc_count;
static lv_draw_buf_t *mc_buf;
static char mc_shown[1025];
static int mc_cell;
static const uint32_t MC_COL[6] = { 0x343a50, 0x38c058, 0xe8c030, 0xe84040, 0xd070e8, 0x4a9cf0 };
static const char *const MC_LEG[] = { "1read OK", "2needed a retry", "3failed", "5written and checked", "0not done yet", NULL };

static void mc_cell_fill(int i, lv_color_t c)
{
	int x0 = (i % 32) * mc_cell, y0 = (i / 32) * mc_cell, n = mc_cell - LV_MAX(1, mc_cell / 7);
	for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) lv_canvas_set_px(mc_canvas, x0 + x, y0 + y, c, LV_OPA_COVER);
}

static void mc_create(lv_obj_t *parent)
{
	mc_cell = LV_MAX(4, S(7));
	int side = 32 * mc_cell;
	mc_box = mk_box(parent, side, S(26) + side + S(42) + 5 * S(22) + S(24), PAL.bg, 0);
	lv_obj_set_style_bg_opa(mc_box, LV_OPA_TRANSP, 0);
	lv_obj_set_pos(mc_box, W - S(56) - side, S(150));
	LR_HIDE(mc_box);                                    /* 240p: the card map doesn't fit beside the list */
	mc_title = mk_label(mc_box, FONT_LABEL, PAL.dim, "MEMORY CARD");
	lv_obj_set_style_text_letter_space(mc_title, S(2), 0);
	lv_obj_set_pos(mc_title, 0, 0);
	if (!mc_buf || (int)mc_buf->header.w != side) {
		if (mc_buf) lv_draw_buf_destroy(mc_buf);
		mc_buf = lv_draw_buf_create(side, side, LV_COLOR_FORMAT_RGB565, 0);
	}
	mc_canvas = lv_canvas_create(mc_box);
	lv_canvas_set_draw_buf(mc_canvas, mc_buf);
	lv_canvas_fill_bg(mc_canvas, PAL.bg, LV_OPA_COVER);
	lv_obj_set_pos(mc_canvas, 0, S(26));
	mc_shown[0] = 0;
	for (int i = 0; i < 1024; i++) mc_cell_fill(i, lv_color_hex(MC_COL[0]));
	mc_count = mk_label(mc_box, FONT_SMALL, PAL.dim, "No card read yet");
	lv_obj_set_pos(mc_count, 0, S(26) + side + S(10));
	for (int k = 0; MC_LEG[k]; k++) {
		lv_obj_t *sq = mk_box(mc_box, S(10), S(10), lv_color_hex(MC_COL[MC_LEG[k][0] - '0']), S(2));
		lv_obj_set_pos(sq, 0, S(26) + side + S(42) + k * S(22) + S(4));
		lv_obj_t *l = mk_label(mc_box, FONT_SMALL, PAL.faint, MC_LEG[k] + 1);
		lv_obj_set_pos(l, S(18), S(26) + side + S(42) + k * S(22));
	}
}

/* only the blocks that changed; the one being worked on blinks in the accent colour */
static void mc_update(const char *map, int busy)
{
	if (!mc_canvas || !map || strlen(map) != 1024) return;
	int changed = 0, cur = -1, ok = 0, retried = 0, failed = 0, written = 0;
	if (busy) for (int i = 0; i < 1024; i++) if (map[i] == '0') { cur = i; break; }
	int blink = (lv_tick_get() / 300) % 2;
	for (int i = 0; i < 1024; i++) {
		char v = map[i];
		ok += v == '1'; retried += v == '2'; failed += v == '3'; written += v == '5';
		char show = (i == cur) ? (blink ? 'c' : 'C') : v;
		if (mc_shown[0] && mc_shown[i] == show) continue;
		lv_color_t c = show == 'c' ? PAL.acc : show == 'C' ? PAL.panel_on : lv_color_hex(MC_COL[(v >= '0' && v <= '5') ? v - '0' : 0]);
		mc_cell_fill(i, c);
		changed = 1;
		if (mc_shown[0]) mc_shown[i] = show;
	}
	if (!mc_shown[0]) {
		for (int i = 0; i < 1024; i++) mc_shown[i] = (i == cur) ? (blink ? 'c' : 'C') : map[i];
		mc_shown[1024] = 0;
	}
	if (changed) lv_obj_invalidate(mc_canvas);
	char t[120];
	int done = ok + retried + failed + written;
	if (!done && !busy) snprintf(t, sizeof(t), "No card read yet");
	else if (written) snprintf(t, sizeof(t), "%d written%s", written, failed ? "  \xC2\xB7  1 failed" : "");
	else if (failed) snprintf(t, sizeof(t), "%d / 1024  \xC2\xB7  %d failed", done, failed);
	else snprintf(t, sizeof(t), "%d / 1024  \xC2\xB7  %d retried", done, retried);
	lv_label_set_text(mc_count, t);
}
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

/* "Card name/2026-10-22_203723.mcd" -> "Card name  ·  Oct 22 2026, 8:37 PM" (old loose files: their own way) */
static void pt_label_for(const char *name, char *out, size_t n)
{
	static const char *const MON[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	int port, y, mo, d, h, mi;
	const char *sl = strchr(name, '/');
	if (sl && sscanf(sl + 1, "%4d-%2d-%2d_%2d%2d", &y, &mo, &d, &h, &mi) == 5 && mo >= 1 && mo <= 12) {
		char t[16];
		if (SETTINGS.clock24) snprintf(t, sizeof(t), "%02d:%02d", h, mi);
		else snprintf(t, sizeof(t), "%d:%02d %s", h % 12 ? h % 12 : 12, mi, h < 12 ? "AM" : "PM");
		snprintf(out, n, "%.*s  \xC2\xB7  %s %d %d, %s", (int)(sl - name), name, MON[mo - 1], d, y, t);
	}
	else if (sscanf(name, "port%d-%4d%2d%2d-%2d%2d", &port, &y, &mo, &d, &h, &mi) == 6)
		snprintf(out, n, "Port %d card (older backup)  \xC2\xB7  %04d-%02d-%02d %02d:%02d", port, y, mo, d, h, mi);
	else snprintf(out, n, "%s", name);
}

static void pt_apply(cJSON *r);
static void show_mcards(void);
static void show_padtest(void);

/* a card Phosphor hasn't seen: its name, typed on the keyboard (B keeps the suggested one) */
static void mc_name_done(const char *text)
{
	char req[400], esc[200];
	const char *t = (text && *text) ? text : pt.suggest;
	size_t k = 0;
	for (const char *p = t; *p && k + 2 < sizeof(esc); p++) { if (*p == '"' || *p == '\\') esc[k++] = '\\'; esc[k++] = *p; }
	esc[k] = 0;
	snprintf(req, sizeof(req), "{\"cmd\":\"memcard\",\"do\":\"name\",\"name\":\"%s\"}", esc);
	cJSON *r = api_call("%s", req);
	if (!api_ok(r)) { toast_show(r && api_str(r, "error") ? api_str(r, "error") : "phosphord isn't answering"); cJSON_Delete(r); return; }
	cJSON *j = cJSON_GetObjectItem(r, "job");
	toast_show(api_str(j, "message") ? api_str(j, "message") : "Backed up");
	pt.asked = 0;
	if (cur == SCR_PORTS) pt_apply(r);
	else cJSON_Delete(r);
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
	pt.ask_name = cJSON_IsTrue(cJSON_GetObjectItem(j, "ask_name"));
	snprintf(pt.suggest, sizeof(pt.suggest), "%s", api_str(j, "suggest") ? api_str(j, "suggest") : "Memory card");
	snprintf(pt.message, sizeof(pt.message), "%s", api_str(j, "message") ? api_str(j, "message") : "");
	if (cur == SCR_PORTS) mc_update(api_str(j, "map"), pt.busy && strcmp(pt.job, "prepare"));
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
		if ((strcmp(pt.job, "prepare") || !pt.job_ok) && !pt.ask_name) toast_show(pt.message[0] ? pt.message : pt.job_ok ? "Done" : "That didn't work");
	}
	/* a new card: name it (the backup is kept until it has one) */
	if (pt.ask_name && !pt.asked && !osk.open && cur == SCR_PORTS) {
		pt.asked = 1;
		osk_open("Name this memory card", "Phosphor hasn't seen this card before. Next time it knows it by its saves.", 0, 40, mc_name_done, PT_HINTS);
		snprintf(osk.text, sizeof(osk.text), "%s", pt.suggest);
		osk_draw();
	}
	if (!pt.ask_name) pt.asked = 0;
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
		snprintf(sub, sizeof(sub), "Restore %s to the card in port 1 (A) or port 2 (X)? The card is backed up first.", lab);
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
	if (!osk.open) footer_set(pt.mode == PT_CONFIRM ? PT_CONFIRM_HINTS : PT_HINTS);     /* the keyboard has its own */
	vlist_refresh(&pt_list);
}

static void pt_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	switch (i) {
	case PT_CARDS: snprintf(t, n, "Memory cards: both ports, save by save"); snprintf(val, vn, ">"); return;
	case PT_TEST: snprintf(t, n, "Controller test"); snprintf(val, vn, ">"); return;
	case PT_BACKUP1: case PT_BACKUP2:
		snprintf(t, n, "Back up the memory card in port %d", i == PT_BACKUP1 ? 1 : 2);
		snprintf(val, vn, ">");
		return;
	}
	i -= PT_FIRST_FILE;
	pt_label_for(pt.names[i], t, n);
	snprintf(val, vn, "Restore >");
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
	/* the list, with the memory card picture (mc_create) on its right */
	int ptw = LV_MIN(S(880), W - S(112) - 32 * LV_MAX(4, S(7)) - S(24));
	cur = SCR_PORTS;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Front ports & memory cards");
	pt_head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_label_set_long_mode(pt_head, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(pt_head, ptw, S(24));
	lv_obj_set_pos(pt_head, S(56), S(178));
	for (int p = 0; p < 2; p++) {
		pt_padl[p] = mk_label(body, FONT_BODY, PAL.dim, "");
		lv_label_set_long_mode(pt_padl[p], LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_size(pt_padl[p], ptw, S(24));
		lv_obj_set_pos(pt_padl[p], S(56), S(216 + 30 * p));
	}
	pt_sub = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(pt_sub, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(pt_sub, ptw, S(22));
	lv_obj_set_pos(pt_sub, S(56), S(282));
	vlist_create(&pt_list, body, S(56), S(316), ptw, S(320));
	mc_create(body);
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
	if (k != K_A) return;
	if (pt_list.sel == PT_CARDS) { show_mcards(); return; }      /* the saved cards work without the ports */
	if (pt_list.sel == PT_TEST && !pt.available && pt.can_load) { pt_do("prepare", 0, NULL); return; }
	if (pt_list.sel == PT_TEST) { show_padtest(); return; }
	if (!pt.available) {
		if (pt.can_load) pt_do("prepare", 0, NULL);
		return;
	}
	switch (pt_list.sel) {
	case PT_CARDS: show_mcards(); return;
	case PT_TEST: show_padtest(); return;
	case PT_BACKUP1: pt_do("backup", 1, NULL); return;
	case PT_BACKUP2: pt_do("backup", 2, NULL); return;
	}
	int i = pt_list.sel - PT_FIRST_FILE;
	if (i >= 0 && i < pt.nfiles) {
		snprintf(pt.pick, sizeof(pt.pick), "%s", pt.names[i]);
		pt.mode = PT_CONFIRM;
		pt_apply(api_call("{\"cmd\":\"memcard\"}"));
	}
}

/* ---------- Memory cards, save by save: SS1 Tool's memory card manager on the TV ----------
 * Both front ports side by side. A side shows the card read from its port (A reads it: a backup into
 * the library, phosphor/memcards) or a card opened from the library. A save is copied to the other
 * side (a new version of that card; if it's the card in a port, it's written back to it, after a
 * fresh backup, as every write). Remove makes a new version too: nothing is overwritten. */
typedef struct { int slot, blocks; char title[96], product[16]; unsigned char icon[1024]; int has_icon; } mcs_save_t;
#define MCS_ROWS 7
static struct {
	char card[2][128], ver[2][64];
	int read[2];                    /* the card shown is the one in that port (read this session) */
	mcs_save_t saves[2][15];
	int nsaves[2], freeb[2], formatted[2], side, sel[2], top[2];
	int pending, job_side;          /* a job runs for job_side: 1 read, 2 write */
	char lib[40][128];
	int nlib;
	int menu, menu_sel, menu_n, menu_act[6];
	char menu_items[6][96];
	int armed;                      /* X pressed once: press again to copy */
	lv_obj_t *col[2], *head[2], *sub[2], *row[2][MCS_ROWS], *ric[2][MCS_ROWS], *rti[2][MCS_ROWS], *rbl[2][MCS_ROWS], *status, *menu_box;
	lv_draw_buf_t *ibuf[2][MCS_ROWS];
} ms;
static const char *const MS_HINTS[] = { "A", "Save", "X", "Copy to the other side", "Y", "Card", "\xE2\x86\x90\xE2\x86\x92", "Port", "B", "Back", NULL };
static const char *const MS_MENU_HINTS[] = { "A", "Choose", "B", "Cancel", NULL };

static int b64v(int c) { return c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' ? 62 : c == '/' ? 63 : -1; }
static int unb64(const char *s, unsigned char *out, int max)
{
	unsigned v = 0;
	int bits = 0, n = 0;
	for (; s && *s && n < max; s++) {
		int d = b64v((unsigned char)*s);
		if (d < 0) continue;
		v = v << 6 | (unsigned)d;
		bits += 6;
		if (bits >= 8) { bits -= 8; out[n++] = (unsigned char)(v >> bits & 255); }
	}
	return n;
}

static void ms_load(int side)
{
	ms.nsaves[side] = 0;
	ms.formatted[side] = ms.freeb[side] = 0;
	if (!ms.card[side][0]) return;
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "memcard");
	cJSON_AddStringToObject(q, "do", "card");
	cJSON_AddStringToObject(q, "name", ms.card[side]);
	char *b = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = api_call("%s", b);
	free(b);
	if (!api_ok(r)) { toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't open the card"); ms.card[side][0] = 0; cJSON_Delete(r); return; }
	cJSON *c = cJSON_GetObjectItem(r, "card"), *it;
	snprintf(ms.ver[side], sizeof(ms.ver[0]), "%s", api_str(r, "version") ? api_str(r, "version") : "");
	ms.formatted[side] = cJSON_IsTrue(cJSON_GetObjectItem(c, "formatted"));
	ms.freeb[side] = (int)api_num(c, "free", 0);
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(c, "saves")) {
		if (ms.nsaves[side] >= 15) break;
		mcs_save_t *sv = &ms.saves[side][ms.nsaves[side]++];
		sv->slot = (int)api_num(it, "slot", 0);
		sv->blocks = cJSON_GetArraySize(cJSON_GetObjectItem(it, "blocks"));
		snprintf(sv->title, sizeof(sv->title), "%s", api_str(it, "title") ? api_str(it, "title") : "");
		snprintf(sv->product, sizeof(sv->product), "%s", api_str(it, "product") ? api_str(it, "product") : "");
		sv->has_icon = unb64(api_str(it, "icon"), sv->icon, 1024) == 1024;
	}
	cJSON_Delete(r);
	if (ms.sel[side] >= ms.nsaves[side]) ms.sel[side] = ms.nsaves[side] ? ms.nsaves[side] - 1 : 0;
}

static void ms_draw(void)
{
	if (cur != SCR_MCARDS) return;
	for (int s = 0; s < 2; s++) {
		char h[200], sub[200];
		if (ms.card[s][0]) snprintf(h, sizeof(h), "Port %d  \xC2\xB7  \"%s\"%s", s + 1, ms.card[s], ms.read[s] ? "" : "  (saved card)");
		else snprintf(h, sizeof(h), "Port %d", s + 1);
		if (!ms.card[s][0]) snprintf(sub, sizeof(sub), "A: read the card in port %d.  Y: open a saved card here.", s + 1);
		else if (!ms.formatted[s]) snprintf(sub, sizeof(sub), "Not formatted");
		else snprintf(sub, sizeof(sub), "%d of 15 blocks used", 15 - ms.freeb[s]);
		lv_label_set_text(ms.head[s], h);
		lv_label_set_text(ms.sub[s], sub);
		lv_obj_set_style_border_color(ms.col[s], s == ms.side ? PAL.acc : PAL.line, 0);
		if (ms.sel[s] < ms.top[s]) ms.top[s] = ms.sel[s];
		if (ms.sel[s] >= ms.top[s] + MCS_ROWS) ms.top[s] = ms.sel[s] - MCS_ROWS + 1;
		for (int r = 0; r < MCS_ROWS; r++) {
			int i = ms.top[s] + r;
			if (i >= ms.nsaves[s]) { lv_obj_set_hidden(ms.row[s][r], true); continue; }
			lv_obj_set_hidden(ms.row[s][r], false);
			mcs_save_t *sv = &ms.saves[s][i];
			int on = s == ms.side && i == ms.sel[s];
			lv_obj_set_style_bg_opa(ms.row[s][r], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
			lv_label_set_text(ms.rti[s][r], sv->title);
			lv_obj_set_style_text_color(ms.rti[s][r], on ? PAL.text : PAL.dim, 0);
			char bl[24];
			snprintf(bl, sizeof(bl), "%d block%s", sv->blocks, sv->blocks == 1 ? "" : "s");
			lv_label_set_text(ms.rbl[s][r], bl);
			/* the save's own icon, 16x16 drawn at twice the size */
			lv_draw_buf_t *db = ms.ibuf[s][r];
			for (int y = 0; y < 32; y++) {
				uint8_t *row = db->data + (size_t)y * db->header.stride;
				for (int x = 0; x < 32; x++) {
					const unsigned char *px = sv->icon + ((y / 2) * 16 + x / 2) * 4;
					row[x * 4 + 0] = px[2]; row[x * 4 + 1] = px[1]; row[x * 4 + 2] = px[0];
					row[x * 4 + 3] = sv->has_icon ? px[3] : 0;
				}
			}
			lv_obj_invalidate(ms.ric[s][r]);
		}
	}
	if (!ms.menu) footer_set(MS_HINTS);
}

static void ms_menu_close(void)
{
	if (ms.menu_box) lv_obj_delete(ms.menu_box);
	ms.menu_box = NULL;
	ms.menu = 0;
	footer_set(MS_HINTS);
}

static void ms_menu_draw(void)
{
	if (!ms.menu_box) return;
	for (int i = 0; i < ms.menu_n; i++) {
		lv_obj_t *r = lv_obj_get_child(ms.menu_box, i + 1);
		lv_obj_set_style_bg_opa(r, i == ms.menu_sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
		lv_obj_set_style_text_color(lv_obj_get_child(r, 0), i == ms.menu_sel ? PAL.text : PAL.dim, 0);
	}
}

/* a small list over the screen; menu: 1 save options, 2 card options, 3 open a saved card */
static void ms_menu_open(int kind, const char *title)
{
	ms.menu = kind;
	ms.menu_sel = 0;
	ms.menu_box = mk_box(lv_layer_top(), S(560), LV_SIZE_CONTENT, PAL.panel, S(14));
	lv_obj_set_style_border_width(ms.menu_box, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(ms.menu_box, PAL.acc, 0);
	lv_obj_set_style_pad_all(ms.menu_box, S(18), 0);
	lv_obj_set_flex_flow(ms.menu_box, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(ms.menu_box, S(4), 0);
	lv_obj_center(ms.menu_box);
	lv_obj_t *t = mk_label(ms.menu_box, FONT_NAME, PAL.text, title);
	lv_obj_set_style_pad_bottom(t, S(8), 0);
	for (int i = 0; i < ms.menu_n; i++) {
		lv_obj_t *r = mk_box(ms.menu_box, LV_PCT(100), S(40), PAL.panel_on, S(8));
		lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
		lv_obj_t *l = mk_label(r, FONT_BODY, PAL.dim, ms.menu_items[i]);
		lv_obj_align(l, LV_ALIGN_LEFT_MID, S(14), 0);
	}
	ms_menu_draw();
	footer_set(MS_MENU_HINTS);
}

static void ms_job(const char *what, int port, const char *name, int pending)
{
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "memcard");
	cJSON_AddStringToObject(q, "do", what);
	cJSON_AddNumberToObject(q, "port", port);
	if (name) cJSON_AddStringToObject(q, "name", name);
	char *b = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = api_call("%s", b);
	free(b);
	if (!api_ok(r)) { toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't start it"); cJSON_Delete(r); return; }
	cJSON_Delete(r);
	ms.pending = pending;
	ms.job_side = port - 1;
	lv_label_set_text(ms.status, pending == 1 ? "Reading the card..." : "Backing up the card, then writing...");
}

static void ms_name_done(const char *text);

/* the job's progress; when it's done: the side shows the card that was read or written */
static void ms_poll(void)
{
	cJSON *r = api_call("{\"cmd\":\"memcard\"}");
	if (!api_ok(r)) { cJSON_Delete(r); return; }
	cJSON *j = cJSON_GetObjectItem(r, "job");
	int busy = cJSON_IsTrue(cJSON_GetObjectItem(j, "busy")), ok = cJSON_IsTrue(cJSON_GetObjectItem(j, "ok"));
	int done = (int)api_num(j, "done", 0), total = (int)api_num(j, "total", 1);
	char msg[220], file[160];
	snprintf(msg, sizeof(msg), "%s", api_str(j, "message") ? api_str(j, "message") : "");
	snprintf(file, sizeof(file), "%s", api_str(j, "file") ? api_str(j, "file") : "");
	int ask = cJSON_IsTrue(cJSON_GetObjectItem(j, "ask_name"));
	char sugg[64];
	snprintf(sugg, sizeof(sugg), "%s", api_str(j, "suggest") ? api_str(j, "suggest") : "Memory card");
	cJSON_Delete(r);
	if (busy) {
		char t[120];
		snprintf(t, sizeof(t), "%s... %d%%", ms.pending == 1 ? "Reading the card" : done < total / 2 ? "Backing up the card first" : "Writing", total ? done * 100 / total : 0);
		lv_label_set_text(ms.status, t);
		return;
	}
	if (ask && !osk.open) {
		lv_label_set_text(ms.status, "A card Phosphor hasn't seen before: give it a name");
		osk_open("Name this memory card", "Next time Phosphor knows it by its saves.", 0, 40, ms_name_done, MS_HINTS);
		snprintf(osk.text, sizeof(osk.text), "%s", sugg);
		osk_draw();
		return;
	}
	if (ask) return;
	int side = ms.job_side, pending = ms.pending;
	ms.pending = 0;
	lv_label_set_text(ms.status, msg);
	if (!ok) { toast_show(msg[0] ? msg : "That didn't work"); return; }
	char *sl = strchr(file, '/');
	if (pending == 1 && sl) {                       /* read: the card it was kept as */
		*sl = 0;
		snprintf(ms.card[side], sizeof(ms.card[0]), "%.127s", file);
		ms.read[side] = 1;
	}
	ms_load(0);
	ms_load(1);
	ms_draw();
	toast_show(pending == 1 ? "Card read" : "Written to the card");
}

static void ms_name_done(const char *text)
{
	char req[400], esc[200];
	size_t k = 0;
	const char *t = text && *text ? text : "Memory card";
	for (const char *p = t; *p && k + 2 < sizeof(esc); p++) { if (*p == '"' || *p == '\\') esc[k++] = '\\'; esc[k++] = *p; }
	esc[k] = 0;
	snprintf(req, sizeof(req), "{\"cmd\":\"memcard\",\"do\":\"name\",\"name\":\"%s\"}", esc);
	cJSON_Delete(api_call("%s", req));
	footer_set(MS_HINTS);
}

static int ms_edit(const char *op, int from, int slot)
{
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "memcard");
	cJSON_AddStringToObject(q, "do", "edit");
	cJSON_AddStringToObject(q, "op", op);
	cJSON_AddStringToObject(q, "name", ms.card[from]);
	cJSON_AddStringToObject(q, "version", ms.ver[from]);
	cJSON *sl = cJSON_AddArrayToObject(q, "slots");
	cJSON_AddItemToArray(sl, cJSON_CreateNumber(slot));
	if (!strcmp(op, "copy")) cJSON_AddStringToObject(q, "to", ms.card[!from]);
	char *b = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = api_call("%s", b);
	free(b);
	int ok = api_ok(r);
	if (!ok) toast_show(r && api_str(r, "error") ? api_str(r, "error") : "That didn't work");
	cJSON_Delete(r);
	return ok;
}

/* the selected save to the other side; written to the card in that port when that's where it is */
static void ms_copy(void)
{
	int s = ms.side, o = !s;
	if (!ms.nsaves[s]) return;
	if (!ms.card[o][0]) { toast_show("Nothing on the other side: read its card (A) or open a saved one (Y)"); return; }
	if (!strcmp(ms.card[o], ms.card[s])) { toast_show("Both sides are the same card"); return; }
	if (!ms_edit("copy", s, ms.saves[s][ms.sel[s]].slot)) return;
	ms_load(o);
	ms_draw();
	if (ms.read[o]) {
		char rel[220];
		snprintf(rel, sizeof(rel), "%s/%s.mcd", ms.card[o], ms.ver[o]);
		ms_job("restore", o + 1, rel, 2);
	} else toast_show("Copied (a new version of that card)");
}

static void show_mcards(void)
{
	cur = SCR_MCARDS;
	lv_obj_clean(body);
	screen_title("FRONT PORTS", "Memory cards");
	int cw = (W - S(112) - S(24)) / 2, ch = S(400);
	for (int s = 0; s < 2; s++) {
		ms.col[s] = mk_box(body, cw, ch, PAL.panel, S(12));
		lv_obj_set_pos(ms.col[s], S(56) + s * (cw + S(24)), S(176));
		lv_obj_set_style_border_width(ms.col[s], LV_MAX(1, S(1)), 0);
		ms.head[s] = mk_label(ms.col[s], FONT_NAME, PAL.text, "");
		lv_label_set_long_mode(ms.head[s], LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_size(ms.head[s], cw - S(36), S(26));
		lv_obj_set_pos(ms.head[s], S(18), S(16));
		ms.sub[s] = mk_label(ms.col[s], FONT_SMALL, PAL.dim, "");
		lv_obj_set_pos(ms.sub[s], S(18), S(46));
		for (int r = 0; r < MCS_ROWS; r++) {
			lv_obj_t *row = mk_box(ms.col[s], cw - S(28), S(44), PAL.panel_on, S(8));
			lv_obj_set_pos(row, S(14), S(80) + r * S(44));
			ms.row[s][r] = row;
			if (!ms.ibuf[s][r]) ms.ibuf[s][r] = lv_draw_buf_create(32, 32, LV_COLOR_FORMAT_ARGB8888, 0);
			ms.ric[s][r] = lv_image_create(row);
			lv_image_set_src(ms.ric[s][r], ms.ibuf[s][r]);
			lv_obj_align(ms.ric[s][r], LV_ALIGN_LEFT_MID, S(8), 0);
			ms.rti[s][r] = mk_label(row, FONT_BODY, PAL.dim, "");
			lv_label_set_long_mode(ms.rti[s][r], LV_LABEL_LONG_MODE_DOTS);
			lv_obj_set_width(ms.rti[s][r], cw - S(28) - S(150));
			lv_obj_align(ms.rti[s][r], LV_ALIGN_LEFT_MID, S(52), 0);
			ms.rbl[s][r] = mk_label(row, FONT_SMALL, PAL.faint, "");
			lv_obj_align(ms.rbl[s][r], LV_ALIGN_RIGHT_MID, -S(12), 0);
		}
	}
	ms.status = mk_label(body, FONT_SMALL, PAL.dim, "Copies and changes make a new version of a card in the library (phosphor/memcards): nothing is overwritten.");
	lv_obj_set_pos(ms.status, S(56), S(176) + ch + S(14));
	ms.menu = 0;
	ms.armed = 0;
	for (int s = 0; s < 2; s++) ms_load(s);
	ms_draw();
}

static void mcards_key(pkey_t k)
{
	if (ms.menu) {
		if (k == K_UP && ms.menu_sel > 0) { ms.menu_sel--; ms_menu_draw(); return; }
		if (k == K_DOWN && ms.menu_sel + 1 < ms.menu_n) { ms.menu_sel++; ms_menu_draw(); return; }
		if (k == K_B || k == K_MENU) { ms_menu_close(); return; }
		if (k != K_A) return;
		int act = ms.menu_act[ms.menu_sel], kind = ms.menu, s = ms.side;
		ms_menu_close();
		if (kind == 3) {                             /* a saved card on this side */
			memcpy(ms.card[s], ms.lib[act], sizeof(ms.card[0]));
			ms.read[s] = 0;
			ms.sel[s] = ms.top[s] = 0;
			ms_load(s);
			ms_draw();
			return;
		}
		switch (act) {
		case 1: ms_copy(); break;
		case 2:
			if (ms_edit("remove", s, ms.saves[s][ms.sel[s]].slot)) {
				ms_load(s);
				ms_draw();
				if (ms.read[s]) { char rel[220]; snprintf(rel, sizeof(rel), "%s/%s.mcd", ms.card[s], ms.ver[s]); ms_job("restore", s + 1, rel, 2); }
				else toast_show("Removed (a new version: the old one stays)");
			}
			break;
		case 3: ms_job("backup", s + 1, NULL, 1); break;
		case 4: {
			cJSON *r = api_call("{\"cmd\":\"memcard\",\"do\":\"library\"}"), *it, *lib = r ? cJSON_GetObjectItem(r, "library") : NULL;
			ms.nlib = 0;
			cJSON_ArrayForEach(it, lib) if (ms.nlib < 40 && api_str(it, "name")) snprintf(ms.lib[ms.nlib++], sizeof(ms.lib[0]), "%s", api_str(it, "name"));
			cJSON_Delete(r);
			if (!ms.nlib) { toast_show("No saved cards yet: read a card first"); break; }
			ms.menu_n = LV_MIN(ms.nlib, 6);
			for (int i = 0; i < ms.menu_n; i++) { snprintf(ms.menu_items[i], sizeof(ms.menu_items[0]), "%s", ms.lib[i]); ms.menu_act[i] = i; }
			ms_menu_open(3, "Open a saved card");
			break;
		}
		case 5: {
			char rel[220];
			snprintf(rel, sizeof(rel), "%s/%s.mcd", ms.card[s], ms.ver[s]);
			ms_job("restore", s + 1, rel, 2);
			break;
		}
		}
		return;
	}
	if (ms.pending && (k == K_A || k == K_X || k == K_Y)) { toast_show("Busy: wait for it to finish"); return; }
	int s = ms.side;
	if (k != K_X) ms.armed = 0;
	switch (k) {
	case K_LEFT: case K_L: ms.side = 0; ms_draw(); break;
	case K_RIGHT: case K_R: ms.side = 1; ms_draw(); break;
	case K_UP: if (ms.sel[s] > 0) { ms.sel[s]--; ms_draw(); } break;
	case K_DOWN: if (ms.sel[s] + 1 < ms.nsaves[s]) { ms.sel[s]++; ms_draw(); } break;
	case K_A:
		if (!ms.card[s][0] || !ms.nsaves[s]) { ms_job("backup", s + 1, NULL, 1); break; }
		ms.menu_n = 2;
		snprintf(ms.menu_items[0], sizeof(ms.menu_items[0]), "Copy to the other side"); ms.menu_act[0] = 1;
		snprintf(ms.menu_items[1], sizeof(ms.menu_items[1]), "Remove from this card"); ms.menu_act[1] = 2;
		ms_menu_open(1, ms.saves[s][ms.sel[s]].title);
		break;
	case K_X:
		if (!ms.nsaves[s]) break;
		if (!ms.armed) { ms.armed = 1; toast_show(ms.read[!s] ? "Press X again: the card in that port is backed up first, then written" : "Press X again to copy"); break; }
		ms.armed = 0;
		ms_copy();
		break;
	case K_Y: {
		int n = 0;
		snprintf(ms.menu_items[n], sizeof(ms.menu_items[0]), "Read the card in port %d", s + 1); ms.menu_act[n++] = 3;
		snprintf(ms.menu_items[n], sizeof(ms.menu_items[0]), "Open a saved card here"); ms.menu_act[n++] = 4;
		if (ms.card[s][0] && !ms.read[s]) { snprintf(ms.menu_items[n], sizeof(ms.menu_items[0]), "Put this card on the card in port %d", s + 1); ms.menu_act[n++] = 5; }
		ms.menu_n = n;
		ms_menu_open(2, "Card");
		break;
	}
	case K_B: case K_MENU: show_ports(); break;
	default: break;
	}
}

/* ---------- Storage: the SD card and the NVMe / USB drives (the storage module) ----------
 * One row per drive (space used, the last check on it), then the checks. A on a drive: its checks;
 * X: a quick check of it. A drive that dropped off (unmounted) is mounted again with A. Only a
 * repair changes a drive, and it asks twice. The SD card can't be repaired while Phosphor runs from it:
 * "Repair all drives" restarts into a copy of MiSTer's Linux on a USB drive (the repair drive; never the
 * NVMe drive) that repairs the SD card and every other drive, and restarts again; the repair drive itself
 * is repaired after that (the storage module's sdrescue.sh). Back up: the backup-agent's snapshot of saves
 * and settings, now. */
#define SG_MAX 8
typedef struct { char path[64], kind[8], fs[16], model[64], gen[16]; double size_kb, free_kb; int mounted; } sg_drive_t;
typedef struct { char target[64], job[24], summary[120]; int ok; long at; } sg_result_t;
enum { SG_QUICKALL, SG_SPEED, SG_REPAIR, SG_SDREPAIR, SG_BACKUP, SG_ACTIONS };
/* the offline SD card repair (storage API "sdrescue") */
typedef struct {
	int drive, present, armed, dirty, fresh, ncand, nvme, nother, nlastd, self_running, nextra;
	char extra[120];                       /* on the repair drive, not a repair copy (a downloader's folders) */
	char mount[64], model[64], last[16], last_at[24], last_summary[120], newp[64], newmodel[64], self_res[16];
	char cand[SG_MAX][64], cand_model[SG_MAX][64];
	char lastd[3][48], lastd_res[3][16];   /* the other drives the last repair repaired */
} sg_rescue_t;
static struct {
	int installed, busy, bk_busy, bk_installed, n, menu, menu_sel, menu_n, confirm;
	sg_drive_t d[SG_MAX];
	sg_result_t res[SG_MAX];
	int nres;
	char job[24], target[64], output[1200], bk_last[40], bk_dest[120];
	long last_at;
	char queue[SG_MAX][64];
	int nqueue;
	char menu_title[80], menu_items[6][80], menu_job[6][24], menu_tgt[6][64];
	lv_obj_t *menu_box;
	sg_rescue_t sr;
	int sr_restart;            /* this screen asked for a repair: restart once the copy is ready */
} sg;
static vlist_t sg_list;
static lv_obj_t *sg_ptitle, *sg_bar, *sg_fill, *sg_k[5], *sg_v[5], *sg_note, *sg_out;
static const char *const SG_HINTS[] = { "A", "Open", "X", "Check now", "B", "Back", NULL };
static const char *const SG_MENU_HINTS[] = { "A", "Choose", "B", "Cancel", NULL };

static const char *sg_jobname(const char *j)
{
	static const struct { const char *k, *v; } N[] = {
		{ "quick", "Quick check" }, { "fscheck", "File system check" }, { "fsrepair", "Repair" }, { "speed", "Speed test" },
		{ "health", "Drive health" }, { "drive", "Drive check" }, { "mount", "Mount again" },
		{ "sdrescue-setup", "Setting up the repair drive" }, { "sdrescue-arm", "Getting the repair ready" },
		{ "sdrescue-disarm", "Repair at the next restart off" }, { "sdrescue-remove", "Drive repair off" },
		{ "sdrescue-tidy", "Moving files off the repair drive" }, { NULL, NULL } };
	for (int i = 0; N[i].k; i++) if (!strcmp(N[i].k, j)) return N[i].v;
	return j;
}

static void sg_drive_name(const sg_drive_t *d, char *out, size_t n)
{
	const char *b = strrchr(d->path, '/');
	b = b ? b + 1 : d->path;
	if (!strcmp(d->kind, "sd")) snprintf(out, n, "SD card");
	else snprintf(out, n, "%s drive (%s)", !strcmp(d->kind, "nvme") ? "NVMe" : "USB", b);
}

static void sg_gb(double kb, char *out, size_t n)
{
	double g = kb / 1048576.0;
	if (g >= 1000) snprintf(out, n, "%.1f TB", g / 1024);
	else snprintf(out, n, g >= 100 ? "%.0f GB" : "%.1f GB", g);
}

static sg_result_t *sg_result(const char *target)
{
	for (int i = 0; i < sg.nres; i++) if (!strcmp(sg.res[i].target, target)) return &sg.res[i];
	return NULL;
}

/* the check's verdict: its last "RESULT: PASS - ..." line */
static void sg_verdict(const char *out, int *ok, char *sum, size_t n)
{
	const char *r = NULL, *p = out;
	while ((p = strstr(p, "RESULT:"))) { r = p; p += 7; }
	*sum = 0;
	if (!r) return;
	r += 7;
	while (*r == ' ') r++;
	if (!strncmp(r, "PASS", 4)) *ok = 1;
	else if (!strncmp(r, "FAIL", 4) || !strncmp(r, "WARN", 4)) *ok = 0;
	const char *dash = strstr(r, " - ");
	const char *s = dash && dash - r < 8 ? dash + 3 : r;
	size_t l = strcspn(s, "\r\n");
	snprintf(sum, n, "%.*s", (int)(l < n ? l : n - 1), s);
}

static void sg_start(const char *job, const char *target);
static void sg_panel(void);
static void power_run(int restart);

static void sg_rescue_parse(cJSON *o, sg_rescue_t *x)
{
	memset(x, 0, sizeof(*x));
	if (!o) return;
	const char *d = api_str(o, "drive");
	x->drive = d && *d;
	x->present = cJSON_IsTrue(cJSON_GetObjectItem(o, "drive_present"));
	x->armed = cJSON_IsTrue(cJSON_GetObjectItem(o, "armed"));
	x->dirty = cJSON_IsTrue(cJSON_GetObjectItem(o, "sd_dirty_at_boot"));
	x->fresh = cJSON_IsTrue(cJSON_GetObjectItem(o, "fresh"));
	snprintf(x->mount, sizeof(x->mount), "%s", api_str(o, "drive_mount") ? api_str(o, "drive_mount") : "");
	snprintf(x->model, sizeof(x->model), "%s", api_str(o, "drive_model") ? api_str(o, "drive_model") : "");
	snprintf(x->last, sizeof(x->last), "%s", api_str(o, "last_result") ? api_str(o, "last_result") : "");
	snprintf(x->last_at, sizeof(x->last_at), "%s", api_str(o, "last_at") ? api_str(o, "last_at") : "");
	snprintf(x->last_summary, sizeof(x->last_summary), "%s", api_str(o, "last_summary") ? api_str(o, "last_summary") : "");
	cJSON *n = cJSON_GetObjectItem(o, "new_drive"), *it;
	if (n) {
		snprintf(x->newp, sizeof(x->newp), "%s", api_str(n, "path") ? api_str(n, "path") : "");
		snprintf(x->newmodel, sizeof(x->newmodel), "%s", api_str(n, "model") ? api_str(n, "model") : "");
	}
	x->nvme = cJSON_IsTrue(cJSON_GetObjectItem(o, "drive_nvme"));
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(o, "extra")) {
		const char *e = cJSON_GetStringValue(it);
		if (!e) continue;
		size_t l = strlen(x->extra);
		if (x->nextra < 4) snprintf(x->extra + l, sizeof(x->extra) - l, "%s%s", x->nextra ? ", " : "", e);
		else if (x->nextra == 4) snprintf(x->extra + l, sizeof(x->extra) - l, "...");
		x->nextra++;
	}
	const char *ss = api_str(o, "self_state"), *sres = api_str(o, "self_result");
	x->self_running = ss && !strcmp(ss, "running");
	snprintf(x->self_res, sizeof(x->self_res), "%s", sres ? sres : "");
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(o, "plugged")) {
		const char *pu = api_str(it, "partuuid");
		if (!pu || !d || strcmp(pu, d)) x->nother++;
	}
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(o, "last_drives")) {
		if (x->nlastd >= 3) break;
		const char *lb = api_str(it, "label"), *md = api_str(it, "model"), *rs = api_str(it, "result");
		int nv = md && !strcmp(md, "NVMe drive");
		snprintf(x->lastd[x->nlastd], sizeof(x->lastd[0]), "%s", nv ? "NVMe drive" : lb && *lb ? lb : md && *md ? md : "USB drive");
		snprintf(x->lastd_res[x->nlastd], sizeof(x->lastd_res[0]), "%s", rs ? rs : "");
		x->nlastd++;
	}
	cJSON_ArrayForEach(it, cJSON_GetObjectItem(o, "candidates")) {
		if (x->ncand >= SG_MAX) break;
		snprintf(x->cand[x->ncand], sizeof(x->cand[0]), "%s", api_str(it, "path") ? api_str(it, "path") : "");
		snprintf(x->cand_model[x->ncand], sizeof(x->cand_model[0]), "%s", api_str(it, "model") ? api_str(it, "model") : "");
		x->ncand++;
	}
}

/* once after start-up: a repair that just ran, or a reason to look at it (the SD card wasn't shut down
   safely; the repair drive swapped for another) */
static void sdrescue_check(void)
{
	cJSON *r = api_call("{\"cmd\":\"storage\"}");
	if (api_ok(r)) {
		sg_rescue_t x;
		sg_rescue_parse(cJSON_GetObjectItem(r, "sdrescue"), &x);
		char t[200];
		if (x.fresh && x.last[0]) {
			int bad = !strcmp(x.last, "errors_left") || !strcmp(x.last, "failed");
			for (int i = 0; i < x.nlastd; i++) if (!strcmp(x.lastd_res[i], "errors_left") || !strcmp(x.lastd_res[i], "failed")) bad = 1;
			snprintf(t, sizeof(t), "Drives repaired offline: %s", bad ? "some need a look (Settings > Storage)" : "all fine");
			toast_show(t);
		} else if (x.dirty && x.drive && x.present && !x.nvme)
			toast_show("The SD card wasn't shut down safely: Settings > Storage > Repair all drives");
		else if (x.dirty && (!x.drive || x.nvme))
			toast_show(x.ncand ? "The SD card wasn't shut down safely: Settings > Storage can repair it offline"
				: "The SD card wasn't shut down safely: plug in a spare USB drive to repair it offline (Settings > Storage)");
		else if (x.drive && !x.present && x.newp[0])
			toast_show("A new USB drive can be the repair drive: Settings > Storage > Repair all drives");
		else if (x.present && x.nextra)
			toast_show("Something put files on the repair drive: Settings > Storage > Repair all drives");
	}
	cJSON_Delete(r);
}

/* "USB drive (usb1, SanDisk Ultra)", "NVMe drive (usb0)" */
static void sg_rescue_name(const char *path, const char *model, char *out, size_t n)
{
	const char *b = strrchr(path, '/');
	b = b ? b + 1 : path;
	int nvme = model && !strcmp(model, "NVMe drive");
	if (!strcmp(path, "/media/repair"))                 /* (hidden from the /media/usbN drives) */
		snprintf(out, n, "USB drive%s%s%s", model && *model ? " (" : "", model && *model ? model : "", model && *model ? ")" : "");
	else if (nvme || !model || !*model) snprintf(out, n, "%s drive (%s)", nvme ? "NVMe" : "USB", b);
	else snprintf(out, n, "USB drive (%s, %s)", b, model);
}

static const char *sg_rescue_res(const char *r)
{
	return !strcmp(r, "clean") ? "No errors found" : !strcmp(r, "repaired") ? "Repaired" :
		!strcmp(r, "errors_left") ? "Errors left: repair it on a PC" : !strcmp(r, "skipped") ? "Not checked here" : "Didn't run";
}

/* sdrescue.sh's answer: its last line, without the ok / FAIL mark */
static void sg_rescue_line(const char *out, char *sum, size_t n)
{
	const char *e = out + strlen(out);
	while (e > out && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) e--;
	const char *b = e;
	while (b > out && b[-1] != '\n') b--;
	if (!strncmp(b, "ok ", 3)) b += 3;
	else if (!strncmp(b, "FAIL ", 5)) b += 5;
	while (b < e && *b == ' ') b++;
	snprintf(sum, n, "%.*s", (int)(e - b), b);
	if (*sum >= 'a' && *sum <= 'z') *sum -= 32;
}

static void sg_refresh(void)
{
	cJSON *r = api_call("{\"cmd\":\"storage\"}"), *it;
	if (api_ok(r)) {
		int was = sg.busy;
		sg.installed = cJSON_IsTrue(cJSON_GetObjectItem(r, "installed"));
		sg.n = 0;
		cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "drives")) {
			if (sg.n >= SG_MAX) break;
			sg_drive_t *d = &sg.d[sg.n++];
			snprintf(d->path, sizeof(d->path), "%s", api_str(it, "path") ? api_str(it, "path") : "");
			snprintf(d->kind, sizeof(d->kind), "%s", api_str(it, "kind") ? api_str(it, "kind") : "usb");
			snprintf(d->fs, sizeof(d->fs), "%s", api_str(it, "fs") ? api_str(it, "fs") : "");
			snprintf(d->model, sizeof(d->model), "%s", api_str(it, "model") ? api_str(it, "model") : "");
			snprintf(d->gen, sizeof(d->gen), "%s", api_str(it, "gen") ? api_str(it, "gen") : "");
			d->size_kb = api_num(it, "size_kb", 0);
			d->free_kb = api_num(it, "free_kb", 0);
			d->mounted = cJSON_IsTrue(cJSON_GetObjectItem(it, "mounted"));
		}
		sg_rescue_parse(cJSON_GetObjectItem(r, "sdrescue"), &sg.sr);
		cJSON *l = cJSON_GetObjectItem(r, "last");
		sg.busy = cJSON_IsTrue(cJSON_GetObjectItem(l, "busy"));
		snprintf(sg.job, sizeof(sg.job), "%s", api_str(l, "job") ? api_str(l, "job") : "");
		snprintf(sg.target, sizeof(sg.target), "%s", api_str(l, "target") ? api_str(l, "target") : "");
		const char *o = api_str(l, "output") ? api_str(l, "output") : "";
		size_t ol = strlen(o);
		snprintf(sg.output, sizeof(sg.output), "%s", ol > sizeof(sg.output) - 1 ? o + ol - (sizeof(sg.output) - 1) : o);
		long at = (long)api_num(l, "at", 0);
		if (!sg.busy && sg.job[0] && sg.target[0] && strcmp(sg.job, "mount") && strncmp(sg.job, "sdrescue-", 9) && (at != sg.last_at || !sg_result(sg.target))) {
			sg_result_t *x = sg_result(sg.target);
			if (!x && sg.nres < SG_MAX) x = &sg.res[sg.nres++];
			if (x) {
				snprintf(x->target, sizeof(x->target), "%s", sg.target);
				snprintf(x->job, sizeof(x->job), "%s", sg.job);
				x->ok = cJSON_IsTrue(cJSON_GetObjectItem(l, "ok"));
				x->at = at;
				int v = x->ok;
				sg_verdict(sg.output, &v, x->summary, sizeof(x->summary));
				x->ok = v && x->ok;
			}
		}
		if (was && !sg.busy && !strncmp(sg.job, "sdrescue-", 9)) {
			int ok = cJSON_IsTrue(cJSON_GetObjectItem(l, "ok"));
			char sum[160];
			sg_rescue_line(sg.output, sum, sizeof(sum));
			if (ok && sg.sr_restart && !strcmp(sg.job, "sdrescue-arm")) {
				sg.sr_restart = 0;
				cJSON_Delete(r);
				toast_show("Restarting to repair all drives");
				power_run(1);
				return;
			}
			sg.sr_restart = 0;
			toast_show(sum[0] ? sum : ok ? "Done" : "That didn't work");
		} else if (was && !sg.busy && sg.job[0]) {
			int ok = cJSON_IsTrue(cJSON_GetObjectItem(l, "ok"));
			char sum[120];
			sg_verdict(sg.output, &ok, sum, sizeof(sum));
			char t[200];
			snprintf(t, sizeof(t), "%s: %s", sg_jobname(sg.job), sum[0] ? sum : ok ? "passed" : "didn't pass");
			if (!sg.nqueue) toast_show(t);
		}
		sg.last_at = at;
	}
	cJSON_Delete(r);
	cJSON *b = api_call("{\"cmd\":\"backup\"}");
	if (api_ok(b)) {
		int was = sg.bk_busy;
		cJSON *st = cJSON_GetObjectItem(b, "status");
		sg.bk_installed = cJSON_IsTrue(cJSON_GetObjectItem(b, "installed"));
		sg.bk_busy = cJSON_IsTrue(cJSON_GetObjectItem(b, "busy")) || (api_str(st, "busy") && !strcmp(api_str(st, "busy"), "1"));
		snprintf(sg.bk_last, sizeof(sg.bk_last), "%s", api_str(st, "last_at") ? api_str(st, "last_at") : "");
		snprintf(sg.bk_dest, sizeof(sg.bk_dest), "%s", api_str(st, "dest") ? api_str(st, "dest") : "");
		if (was && !sg.bk_busy) toast_show("Saves & settings backed up");
	}
	cJSON_Delete(b);
	/* Quick check of everything: one drive after another */
	if (!sg.busy && sg.nqueue) {
		char t[64];
		snprintf(t, sizeof(t), "%s", sg.queue[0]);
		memmove(sg.queue[0], sg.queue[1], sizeof(sg.queue[0]) * (SG_MAX - 1));
		sg.nqueue--;
		sg_start("quick", t);
		if (!sg.nqueue) {}
	}
	sg_list.count = sg.installed ? sg.n + SG_ACTIONS : 1;
	if (sg_list.sel >= sg_list.count) sg_list.sel = sg_list.count - 1;
	if (cur == SCR_STORAGE) { vlist_refresh(&sg_list); sg_panel(); }
}

static void sg_fill_row(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	*val = 0;
	if (!sg.installed) { snprintf(t, n, "The storage module isn't installed"); *dim = 1; return; }
	if (i < sg.n) {
		sg_drive_t *d = &sg.d[i];
		char nm[64], sz[24];
		sg_drive_name(d, nm, sizeof(nm));
		sg_gb(d->size_kb, sz, sizeof(sz));
		if (!d->mounted) { snprintf(t, n, "%s \xC2\xB7 not mounted", nm); snprintf(val, vn, "Mount it again  >"); return; }
		char fs[16];
		snprintf(fs, sizeof(fs), "%s", d->fs);
		if (!strcmp(fs, "exfat")) snprintf(fs, sizeof(fs), "exFAT");
		else if (!strcmp(fs, "vfat")) snprintf(fs, sizeof(fs), "FAT32");
		else if (!strcmp(fs, "ntfs")) snprintf(fs, sizeof(fs), "NTFS");
		snprintf(t, n, "%s \xC2\xB7 %s %s", nm, sz, fs);
		int used = d->size_kb > 0 ? (int)(100 - d->free_kb * 100 / d->size_kb + 0.5) : 0;
		sg_result_t *x = sg_result(d->path);
		if (sg.busy && !strcmp(sg.target, d->path)) snprintf(val, vn, "%d%% used \xC2\xB7 checking...  >", used);
		else if (x) snprintf(val, vn, "%d%% used \xC2\xB7 %s  >", used, x->ok ? "healthy" : "check it");
		else snprintf(val, vn, "%d%% used  >", used);
		return;
	}
	switch (i - sg.n) {
	case SG_QUICKALL: snprintf(t, n, "Quick check of everything"); snprintf(val, vn, sg.nqueue ? "%d to go  >" : ">", sg.nqueue + 1); break;
	case SG_SPEED: snprintf(t, n, "Speed test"); snprintf(val, vn, ">"); break;
	case SG_REPAIR: snprintf(t, n, "Repair a drive"); snprintf(val, vn, "NVMe / USB  >"); break;
	case SG_SDREPAIR: {
		const sg_rescue_t *x = &sg.sr;
		snprintf(t, n, "Repair all drives");
		if (sg.busy && !strncmp(sg.job, "sdrescue-", 9)) snprintf(val, vn, "Preparing...");
		else if (x->self_running) snprintf(val, vn, "Repairing the repair drive...");
		else if (x->armed) snprintf(val, vn, "At the next restart  >");
		else if (!x->drive || x->nvme) snprintf(val, vn, x->ncand ? "Set up  >" : "Needs a USB drive  >");
		else if (!x->present) snprintf(val, vn, x->newp[0] ? "New drive found  >" : "Drive not plugged in  >");
		else snprintf(val, vn, x->dirty ? "Recommended  >" : "Ready  >");
		break;
	}
	case SG_BACKUP:
		snprintf(t, n, "Back up saves & settings");
		if (!sg.bk_installed) { *dim = 1; snprintf(val, vn, "Needs backup-agent"); }
		else if (sg.bk_busy) snprintf(val, vn, "Backing up...");
		else if (sg.bk_last[0]) snprintf(val, vn, "last: %.16s  >", sg.bk_last);
		else snprintf(val, vn, "Never  >");
		break;
	}
}

static void sg_kv(int i, const char *k, const char *v)
{
	lv_label_set_text(sg_k[i], k);
	lv_label_set_text(sg_v[i], v);
	lv_obj_set_hidden(lv_obj_get_parent(sg_k[i]), !*k);
}

static void sg_panel(void)
{
	int i = sg_list.sel;
	char a[160], b[160];
	for (int r = 0; r < 5; r++) sg_kv(r, "", "");
	lv_obj_set_hidden(sg_bar, true);
	lv_label_set_text(sg_out, "");
	if (!sg.installed) {
		lv_label_set_text(sg_ptitle, "Storage");
		lv_label_set_text(sg_note, "Install the storage module (modules/storage/install.sh in the Phosphor release).");
		return;
	}
	if (i < sg.n) {
		sg_drive_t *d = &sg.d[i];
		sg_drive_name(d, a, sizeof(a));
		lv_label_set_text(sg_ptitle, a);
		int r = 0;
		if (d->mounted && d->size_kb > 0) {
			lv_obj_set_hidden(sg_bar, false);
			int w = lv_obj_get_width(sg_bar);
			lv_obj_set_width(sg_fill, LV_MAX(S(6), (int)(w * (1 - d->free_kb / d->size_kb))));
			char s1[24], s2[24];
			sg_gb(d->size_kb, s1, sizeof(s1));
			sg_gb(d->free_kb, s2, sizeof(s2));
			snprintf(b, sizeof(b), "%s, %s free", s1, s2);
			sg_kv(r++, "Size", b);
		} else if (!d->mounted) { sg_kv(r++, "State", "Not mounted"); }
		snprintf(b, sizeof(b), "%s", !strcmp(d->fs, "exfat") ? "exFAT" : !strcmp(d->fs, "vfat") ? "FAT32" : !strcmp(d->fs, "ntfs") ? "NTFS" : d->fs[0] ? d->fs : "unknown");
		sg_kv(r++, "File system", b);
		if (d->model[0] && strcmp(d->kind, "sd")) sg_kv(r++, "Model", d->model);
		if (!strcmp(d->kind, "nvme") && (!strcmp(d->gen, "gen3") || !strcmp(d->gen, "gen4"))) sg_kv(r++, "NVMe", !strcmp(d->gen, "gen4") ? "PCIe Gen 4" : "PCIe Gen 3");
		sg_result_t *x = sg_result(d->path);
		if (x) {
			char w[40];
			si_when(x->at, w, sizeof(w));
			snprintf(b, sizeof(b), "%s %s \xC2\xB7 %s", sg_jobname(x->job), x->ok ? "passed" : "didn't pass", w);
			sg_kv(r++, "Last check", b);
		} else sg_kv(r++, "Last check", "Not since Phosphor started");
		if (!d->mounted) lv_label_set_text(sg_note, "A: mount it again, as MiSTer does at start-up. Nothing on it is changed.");
		else if (!strcmp(d->kind, "sd")) lv_label_set_text(sg_note, "A: checks for this drive: quick, file system, speed, health, and repair offline (Repair all drives, a restart). X: a quick check now.");
		else lv_label_set_text(sg_note, "A: checks for this drive: quick, file system, speed, health, and repair. X: a quick check now.");
	} else {
		static const char *const T[] = { "Quick check of everything", "Speed test", "Repair a drive", "Repair all drives", "Back up saves & settings" };
		static const char *const N[] = {
			"A quick check of every drive, one after another: the file system's state, the partition and the boot files. Nothing is changed.",
			"Writes one temporary file to a drive and reads it back, then removes it. Shows how fast the drive reads and writes.",
			"Unmounts an NVMe or USB drive, asks the file system checker to repair it, and mounts it again. Asks twice first. For the SD card (and every drive at once, offline), see Repair all drives.",
			"",
			"A snapshot of your saves, save states and settings now, the same as after a game. Restore it from Settings > Backups." };
		int k = i - sg.n;
		lv_label_set_text(sg_ptitle, T[k]);
		lv_label_set_text(sg_note, N[k]);
		if (k == SG_SDREPAIR) {
			const sg_rescue_t *x = &sg.sr;
			int r = 0;
			if (x->drive && !x->nvme) {
				if (x->present) sg_rescue_name(x->mount, x->model, b, sizeof(b));
				else snprintf(b, sizeof(b), "%s \xC2\xB7 not plugged in", x->model[0] ? x->model : "The repair drive");
				sg_kv(r++, "Repair drive", b);
			} else sg_kv(r++, "Repair drive", x->nvme ? "The NVMe drive: choose a USB drive" : "None yet");
			if (x->armed) sg_kv(r++, "Next restart", "Repairs all drives");
			if (x->present && x->nextra) sg_kv(r++, "Also on it", x->extra);
			if (x->last[0]) {
				snprintf(b, sizeof(b), "%s \xC2\xB7 %.16s", sg_rescue_res(x->last), x->last_at);
				sg_kv(r++, "SD card", b);
				for (int j = 0; j < x->nlastd && r < 4; j++) sg_kv(r++, x->lastd[j], sg_rescue_res(x->lastd_res[j]));
				if (r < 5) sg_kv(r++, "Repair drive after", x->self_running ? "Repairing now..." : x->self_res[0] ? sg_rescue_res(x->self_res) : "Not checked");
			} else sg_kv(r++, "SD card at start-up", x->dirty ? "Wasn't shut down safely" : "Clean");
			if ((!x->drive || x->nvme) && !x->ncand)
				lv_label_set_text(sg_note, "Plug a spare USB drive into the SuperStation (exFAT, 1 GB free) to repair all drives without a PC: Phosphor puts a copy of MiSTer's Linux on it, and a restart repairs the SD card, the NVMe drive and any other USB drives from there. Why not the NVMe drive: the drive the repair runs from can't be repaired in the same pass, and the NVMe drive holds your games.");
			else if (!x->drive || x->nvme)
				lv_label_set_text(sg_note, "A: choose the USB drive the repair starts from. Phosphor puts a copy of MiSTer's Linux on it (about 400 MB, nothing else changes). Then a restart repairs the SD card, the NVMe drive and any other USB drives while nothing is using them, and starts Phosphor again.");
			else if (!x->present && x->newp[0])
				lv_label_set_text(sg_note, "The repair drive isn't plugged in, but another USB drive is. A: make it the repair drive (a copy of MiSTer's Linux goes on it, about 400 MB).");
			else if (!x->present)
				lv_label_set_text(sg_note, "Plug the repair drive back in, or another spare USB drive (exFAT, 1 GB free) to use instead.");
			else if (x->nextra)
				lv_label_set_text(sg_note, "Something else put files on the repair drive (MiSTer's downloader puts games folders on the first USB drive it finds). It should only hold repair copies: A, then Move the other files to the games drive.");
			else {
				char nb[320];
				snprintf(nb, sizeof(nb), "A: restart and repair the SD card%s while nothing is using them. The TV shows the repair as it runs, then Phosphor starts again; the repair drive itself is repaired after that.",
					x->nother ? " and the other drives plugged in" : "");
				lv_label_set_text(sg_note, nb);
			}
		} else if (k == SG_BACKUP) {
			sg_kv(0, "Last backup", sg.bk_last[0] ? sg.bk_last : "None yet");
			if (sg.bk_dest[0]) sg_kv(1, "Kept in", sg.bk_dest);
		} else {
			int r = 0;
			for (int j = 0; j < sg.nres && r < 5; j++) {
				const sg_result_t *x = &sg.res[j];
				if (k == SG_SPEED && strcmp(x->job, "speed")) continue;
				if (k == SG_REPAIR && strcmp(x->job, "fsrepair")) continue;
				if (k == SG_QUICKALL && strcmp(x->job, "quick")) continue;
				for (int q = 0; q < sg.n; q++) if (!strcmp(sg.d[q].path, x->target)) sg_drive_name(&sg.d[q], a, sizeof(a));
				snprintf(b, sizeof(b), "%s", x->summary[0] ? x->summary : x->ok ? "Passed" : "Didn't pass");
				sg_kv(r++, a, b);
			}
		}
	}
	if (sg.busy) {
		/* (a module's output arrives when it's done) */
		char nm[64] = "";
		for (int q = 0; q < sg.n; q++) if (!strcmp(sg.d[q].path, sg.target)) sg_drive_name(&sg.d[q], nm, sizeof(nm));
		static const char *const D[] = { ".", "..", "..." };
		snprintf(b, sizeof(b), "%s on the %s%s", sg_jobname(sg.job), nm[0] ? nm : sg.target, D[(lv_tick_get() / 1000) % 3]);
		if (sg.nqueue) snprintf(b + strlen(b), sizeof(b) - strlen(b), "\n%d more drive%s after this", sg.nqueue, sg.nqueue == 1 ? "" : "s");
		lv_label_set_text(sg_out, b);
	}
}

static void sg_start(const char *job, const char *target)
{
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "storage");
	if (!strcmp(job, "mount") || !strncmp(job, "sdrescue-", 9)) cJSON_AddStringToObject(q, "do", job);
	else { cJSON_AddStringToObject(q, "do", "check"); cJSON_AddStringToObject(q, "test", job); }
	if (target && *target) cJSON_AddStringToObject(q, "target", target);
	char *s = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = api_call("%s", s);
	free(s);
	if (!api_ok(r)) { toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't start it"); sg.nqueue = 0; }
	else sg.busy = 1;
	cJSON_Delete(r);
	sg_refresh();
}

static void sg_menu_close(void)
{
	if (sg.menu_box) lv_obj_delete(sg.menu_box);
	sg.menu_box = NULL;
	sg.menu = 0;
	sg.confirm = 0;
	footer_set(SG_HINTS);
}

static void sg_menu_draw(void)
{
	if (!sg.menu_box) return;
	for (int i = 0; i < sg.menu_n; i++) {
		lv_obj_t *r = lv_obj_get_child(sg.menu_box, i + 1);
		lv_obj_set_style_bg_opa(r, i == sg.menu_sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
		lv_obj_set_style_text_color(lv_obj_get_child(r, 0), i == sg.menu_sel ? PAL.text : PAL.dim, 0);
	}
}

static void sg_menu_add(const char *label, const char *job, const char *target)
{
	if (sg.menu_n >= 6) return;
	snprintf(sg.menu_items[sg.menu_n], sizeof(sg.menu_items[0]), "%s", label);
	snprintf(sg.menu_job[sg.menu_n], sizeof(sg.menu_job[0]), "%s", job);
	snprintf(sg.menu_tgt[sg.menu_n], sizeof(sg.menu_tgt[0]), "%s", target);
	sg.menu_n++;
}

static void sg_menu_open(const char *title)
{
	if (!sg.menu_n) { toast_show("No drive for that"); return; }
	if (sg.menu_box) lv_obj_delete(sg.menu_box);
	sg.menu = 1;
	sg.menu_sel = 0;
	sg.menu_box = mk_box(lv_layer_top(), S(620), LV_SIZE_CONTENT, PAL.panel, S(14));
	lv_obj_set_style_border_width(sg.menu_box, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(sg.menu_box, PAL.acc, 0);
	lv_obj_set_style_pad_all(sg.menu_box, S(18), 0);
	lv_obj_set_flex_flow(sg.menu_box, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(sg.menu_box, S(4), 0);
	lv_obj_center(sg.menu_box);
	lv_obj_t *t = mk_label(sg.menu_box, FONT_NAME, PAL.text, title);
	lv_obj_set_width(t, LV_PCT(100));
	lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_style_pad_bottom(t, S(8), 0);
	for (int i = 0; i < sg.menu_n; i++) {
		lv_obj_t *r = mk_box(sg.menu_box, LV_PCT(100), S(40), PAL.panel_on, S(8));
		lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
		lv_obj_t *l = mk_label(r, FONT_BODY, PAL.dim, sg.menu_items[i]);
		lv_obj_align(l, LV_ALIGN_LEFT_MID, S(14), 0);
	}
	sg_menu_draw();
	footer_set(SG_MENU_HINTS);
}

static void sg_rescue_action(void);

static void sg_menu_pick(void)
{
	char job[24], tgt[64], lab[80];
	snprintf(job, sizeof(job), "%s", sg.menu_job[sg.menu_sel]);
	snprintf(tgt, sizeof(tgt), "%s", sg.menu_tgt[sg.menu_sel]);
	snprintf(lab, sizeof(lab), "%s", sg.menu_items[sg.menu_sel]);
	if (!job[0]) { sg_menu_close(); return; }                      /* Cancel */
	if (!strcmp(job, "sdrescue-open")) { sg_menu_close(); sg_rescue_action(); return; }
	if (!strcmp(job, "sdrescue-go") && !sg.confirm) {
		sg.menu_n = 0;
		sg_menu_add("Yes, restart and repair", job, "");
		sg_menu_add("Cancel", "", "");
		sg_menu_open("Restart now and repair all drives? The TV shows the repair, then Phosphor starts again.");
		sg.confirm = 1;
		return;
	}
	if (!strcmp(job, "sdrescue-remove") && !sg.confirm) {
		sg.menu_n = 0;
		sg_menu_add("Yes, turn it off", job, "");
		sg_menu_add("Cancel", "", "");
		sg_menu_open("Turn off drive repair? Its lines come off the SD card and its copy of Linux off the repair drive.");
		sg.confirm = 1;
		return;
	}
	if (!strcmp(job, "sdrescue-go")) { sg_menu_close(); sg.sr_restart = 1; sg_start("sdrescue-arm", ""); return; }
	if (!strcmp(job, "fsrepair") && !sg.confirm) {
		/* the second ask */
		char t[160];
		snprintf(t, sizeof(t), "Repair %s? It's unmounted while the checker works, then mounted again.", lab);
		sg.menu_n = 0;
		sg_menu_add("Yes, repair it", job, tgt);
		sg_menu_add("Cancel", "", "");
		sg_menu_open(t);
		sg.confirm = 1;
		return;
	}
	sg_menu_close();
	sg_start(job, tgt);
}

/* Repair all drives: set up a USB drive, restart and repair, a new drive instead of one that's gone */
static void sg_rescue_action(void)
{
	const sg_rescue_t *x = &sg.sr;
	char nm[96], lab[80];
	if (sg.busy) { toast_show("A check is running: wait for it to finish"); return; }
	sg.menu_n = 0;
	if (!x->drive || x->nvme) {
		if (!x->ncand) { toast_show("Plug in a spare USB drive (exFAT, 1 GB free) first"); return; }
		for (int i = 0; i < x->ncand && sg.menu_n < 5; i++) {
			sg_rescue_name(x->cand[i], x->cand_model[i], nm, sizeof(nm));
			snprintf(lab, sizeof(lab), "Use the %s", nm);
			sg_menu_add(lab, "sdrescue-setup", x->cand[i]);
		}
		sg_menu_add("Cancel", "", "");
		sg_menu_open("Which USB drive does the repair start from? A copy of MiSTer's Linux goes on it (about 400 MB); nothing else on it changes.");
		return;
	}
	if (!x->present) {
		if (!x->newp[0]) { toast_show("Plug the repair drive back in, or another spare USB drive"); return; }
		sg_rescue_name(x->newp, x->newmodel, nm, sizeof(nm));
		snprintf(lab, sizeof(lab), "Use the %s", nm);
		sg_menu_add(lab, "sdrescue-setup", x->newp);
		sg_menu_add("Turn off drive repair", "sdrescue-remove", "");
		sg_menu_add("Cancel", "", "");
		sg_menu_open("The repair drive isn't plugged in. Use this USB drive instead?");
		return;
	}
	sg_menu_add("Restart and repair all drives", "sdrescue-go", "");
	if (x->armed) sg_menu_add("Don't repair at the next restart", "sdrescue-disarm", "");
	if (x->nextra) {
		/* the games drive: the NVMe drive, else the first USB drive, else the SD card */
		const char *to = "/media/fat";
		for (int i = 0; i < sg.n; i++) if (sg.d[i].mounted && !strcmp(sg.d[i].kind, "nvme")) { to = sg.d[i].path; break; }
		if (!strcmp(to, "/media/fat")) for (int i = 0; i < sg.n; i++) if (sg.d[i].mounted && strcmp(sg.d[i].kind, "sd")) { to = sg.d[i].path; break; }
		sg_menu_add("Move the other files to the games drive", "sdrescue-tidy", to);
	}
	for (int i = 0; i < x->ncand && sg.menu_n < 4; i++) {
		if (!strcmp(x->cand[i], x->mount)) continue;
		sg_rescue_name(x->cand[i], x->cand_model[i], nm, sizeof(nm));
		snprintf(lab, sizeof(lab), "Use the %s instead", nm);
		sg_menu_add(lab, "sdrescue-setup", x->cand[i]);
	}
	sg_menu_add("Turn off drive repair", "sdrescue-remove", "");
	sg_menu_add("Cancel", "", "");
	sg_menu_open("Repair all drives");
}

static void storage_key(pkey_t k)
{
	if (sg.menu) {
		if (k == K_UP) { sg.menu_sel = (sg.menu_sel + sg.menu_n - 1) % sg.menu_n; sg_menu_draw(); }
		else if (k == K_DOWN) { sg.menu_sel = (sg.menu_sel + 1) % sg.menu_n; sg_menu_draw(); }
		else if (k == K_A) sg_menu_pick();
		else if (k == K_B || k == K_MENU) sg_menu_close();
		return;
	}
	if (vlist_key(&sg_list, k)) { sg_panel(); return; }
	if (k == K_B || k == K_MENU) { sg.nqueue = 0; back_to_settings(SET_STORAGE); return; }
	if (!sg.installed || (k != K_A && k != K_X)) return;
	int i = sg_list.sel;
	if ((sg.busy || sg.nqueue) && !(i == sg.n + SG_BACKUP)) { toast_show("A check is running: wait for it to finish"); return; }
	char nm[64], t[120];
	if (i < sg.n) {
		sg_drive_t *d = &sg.d[i];
		if (!d->mounted) { sg_start("mount", d->path); return; }
		if (k == K_X) { sg_start("quick", d->path); return; }
		sg_drive_name(d, nm, sizeof(nm));
		sg.menu_n = 0;
		sg_menu_add("Quick check", "quick", d->path);
		sg_menu_add("File system check", "fscheck", d->path);
		sg_menu_add("Speed test", "speed", d->path);
		sg_menu_add("Drive health", "health", d->path);
		if (strcmp(d->kind, "sd")) sg_menu_add("Repair the file system", "fsrepair", d->path);
		else sg_menu_add("Repair offline (a restart)", "sdrescue-open", "");
		snprintf(t, sizeof(t), "%s", nm);
		sg_menu_open(t);
		return;
	}
	switch (i - sg.n) {
	case SG_QUICKALL:
		sg.nqueue = 0;
		for (int q = 0; q < sg.n; q++) if (sg.d[q].mounted) snprintf(sg.queue[sg.nqueue++], sizeof(sg.queue[0]), "%s", sg.d[q].path);
		sg_refresh();
		break;
	case SG_SPEED: case SG_REPAIR:
		sg.menu_n = 0;
		for (int q = 0; q < sg.n; q++) {
			if (!sg.d[q].mounted || (i - sg.n == SG_REPAIR && !strcmp(sg.d[q].kind, "sd"))) continue;
			sg_drive_name(&sg.d[q], nm, sizeof(nm));
			sg_menu_add(nm, i - sg.n == SG_SPEED ? "speed" : "fsrepair", sg.d[q].path);
		}
		sg_menu_open(i - sg.n == SG_SPEED ? "Speed test: which drive?" : "Repair which drive?");
		break;
	case SG_SDREPAIR: sg_rescue_action(); break;
	case SG_BACKUP: {
		if (!sg.bk_installed) { toast_show("Install the backup-agent module first"); break; }
		if (sg.bk_busy) { toast_show("Already backing up"); break; }
		cJSON *r = api_call("{\"cmd\":\"backup\",\"do\":\"now\"}");
		if (!api_ok(r)) toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't start it");
		else { sg.bk_busy = 1; toast_show("Backing up saves & settings..."); }
		cJSON_Delete(r);
		vlist_refresh(&sg_list);
		break;
	}
	}
}

static void show_storage(void)
{
	cur = SCR_STORAGE;
	lv_obj_clean(body);
	sg.menu_box = NULL;
	sg.menu = 0;
	screen_title("SETTINGS", "Storage");
	vlist_create(&sg_list, body, S(56), S(170), SW(660), S(452));
	sg_list.fill = sg_fill_row;
	sg_list.sel = 0;
	sg_list.top = 0;
	sg_list.count = 0;
	lv_obj_t *box = mk_box(body, SW(464), S(430), PAL.panel, S(10));
	lv_obj_set_pos(box, SX(760), S(170));
	LR_HIDE(box);
	lv_obj_set_style_border_color(box, PAL.line, 0);
	lv_obj_set_style_border_width(box, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_pad_all(box, S(22), 0);
	lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(box, S(6), 0);
	int iw = SW(464) - 2 * S(22);
	sg_ptitle = mk_label(box, FONT_NAME, PAL.text, "");
	lv_obj_set_width(sg_ptitle, iw);
	lv_label_set_long_mode(sg_ptitle, LV_LABEL_LONG_MODE_DOTS);
	sg_bar = mk_box(box, iw, S(8), PAL.line, S(4));
	lv_obj_set_style_margin_bottom(sg_bar, S(6), 0);
	sg_fill = mk_box(sg_bar, S(6), S(8), PAL.acc, S(4));
	lv_obj_set_width(sg_bar, iw);
	for (int r = 0; r < 5; r++) {
		lv_obj_t *pr = lv_obj_create(box);
		lv_obj_remove_style_all(pr);
		lv_obj_set_size(pr, iw, LV_SIZE_CONTENT);
		sg_k[r] = mk_label(pr, FONT_SMALL, PAL.dim, "");
		sg_v[r] = mk_label(pr, FONT_SMALL, PAL.text, "");
		lv_obj_set_width(sg_v[r], iw * 64 / 100);
		lv_label_set_long_mode(sg_v[r], LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_pos(sg_v[r], iw * 36 / 100, 0);
	}
	sg_note = mk_label(box, FONT_SMALL, PAL.dim, "");
	lv_obj_set_width(sg_note, iw);
	lv_label_set_long_mode(sg_note, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_style_margin_top(sg_note, S(8), 0);
	sg_out = mk_label(box, FONT_SMALL, PAL.acc, "");
	lv_obj_set_width(sg_out, iw);
	lv_label_set_long_mode(sg_out, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_style_margin_top(sg_out, S(8), 0);
	footer_set(SG_HINTS);
	lv_obj_update_layout(box);
	sg_refresh();
}

/* ---------- Controller test: a front port's PlayStation controller, live, in SS1 Tool's drawing ---------- */
static struct {
	int port, id, conn, lx, ly, rx, ry, layout;
	unsigned buttons;
	padview_t *pv;
	lv_obj_t *head, *axis;
} ptst;
static const char *const PADT_HINTS[] = { "L/R", "Port", "B", "Back", NULL };

static void padtest_poll(void)
{
	cJSON *r = api_call("{\"cmd\":\"pads\"}"), *it, *pads = r ? cJSON_GetObjectItem(r, "pads") : NULL;
	ptst.conn = 0;
	cJSON_ArrayForEach(it, pads) {
		if ((int)api_num(it, "port", 0) != ptst.port) continue;
		ptst.conn = cJSON_IsTrue(cJSON_GetObjectItem(it, "connected"));
		ptst.id = (int)api_num(it, "id", 0);
		ptst.buttons = (unsigned)api_num(it, "buttons", 0);
		ptst.lx = (int)api_num(it, "lx", 128); ptst.ly = (int)api_num(it, "ly", 128);
		ptst.rx = (int)api_num(it, "rx", 128); ptst.ry = (int)api_num(it, "ry", 128);
	}
	cJSON_Delete(r);
	if (cur != SCR_PADTEST || !ptst.pv) return;
	int analog = ptst.conn && (ptst.id == 0x73 || ptst.id == 0x53);
	if (analog != ptst.layout) { ptst.layout = analog; padview_layout(ptst.pv, analog ? "dualshock" : "psx"); }
	char h[120];
	snprintf(h, sizeof(h), ptst.conn ? "Port %d  \xC2\xB7  %s" : "Port %d  \xC2\xB7  nothing plugged in", ptst.port, pt_type(ptst.id));
	lv_label_set_text(ptst.head, h);
	/* the controller's bits: Select L3 R3 Start Up Right Down Left L2 R2 L1 R1 Triangle Circle Cross Square */
	static const char *const ID[16] = { "back", "ls", "rs", "start", "up", "right", "down", "left", "l2", "r2", "l1", "r1", "y", "b", "a", "x" };
	padstate_t st;
	memset(&st, 0, sizeof(st));
	st.nhas = -1;
	for (int i = 0; i < 16; i++) if (ptst.conn && (ptst.buttons & (1u << i))) st.on[st.non++] = ID[i];
	if (analog) {
		st.lx = (ptst.lx - 128) / 128.f; st.ly = (ptst.ly - 128) / 128.f;
		st.rx = (ptst.rx - 128) / 128.f; st.ry = (ptst.ry - 128) / 128.f;
	}
	st.l2 = (ptst.buttons & (1u << 8)) ? 1.f : 0;
	st.r2 = (ptst.buttons & (1u << 9)) ? 1.f : 0;
	padview_state(ptst.pv, &st);
	char a[120];
	if (analog) snprintf(a, sizeof(a), "L  x %+.2f  y %+.2f        R  x %+.2f  y %+.2f", st.lx, st.ly, st.rx, st.ry);
	else snprintf(a, sizeof(a), ptst.conn ? "Digital controller: no sticks" : "Plug a controller into port %d", ptst.port);
	lv_label_set_text(ptst.axis, a);
}

static void show_padtest(void)
{
	cur = SCR_PADTEST;
	lv_obj_clean(body);
	if (ptst.port < 1 || ptst.port > 2) ptst.port = 1;
	screen_title("FRONT PORTS", "Controller test");
	ptst.head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_obj_set_pos(ptst.head, S(56), S(178));
	ptst.pv = padview_create(body, (W - S(680)) / 2, S(214), S(680));
	ptst.layout = 0;
	padview_layout(ptst.pv, "psx");
	ptst.axis = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_obj_set_pos(ptst.axis, S(56), S(214) + S(680) * 345 / 600 + S(10));
	footer_set(PADT_HINTS);
	padtest_poll();
}

static void padtest_key(pkey_t k)
{
	if (k == K_L || k == K_LEFT) { ptst.port = 1; padtest_poll(); }
	else if (k == K_R || k == K_RIGHT) { ptst.port = 2; padtest_poll(); }
	else if (k == K_B || k == K_MENU) { ptst.pv = NULL; show_ports(); }
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
/* the side: a big Bluetooth sign and what's happening, centred; how to put controllers in pairing mode */
static lv_obj_t *bt_ring, *bt_ic, *bt_title, *bt_note, *bt_tips;
static uint32_t bt_started;                     /* when Pair was pressed (the 90 s it looks for) */
static char bt_paired[64];                      /* a controller that just paired: shown until the next try */
#define BT_LOOK_S 90
static const char *const BT_HINTS[] = { "A", "Choose", "B", "Back", NULL };
static const char *const BT_ACTION_HINTS[] = { "A", "Disconnect", "X", "Delete", "B", "Cancel", NULL };
static const char *const BT_DEV_HINTS[] = { "A", "Options", "X", "Delete", "B", "Back", NULL };
static uint32_t bt_confirm_at;                  /* X (or Delete all) pressed once: again within 4 s deletes */
static int bt_confirm_row = -1;

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
	if (was && !bt.busy && bt.at != bt.seen_at) {
		bt.seen_at = bt.at;
		if (bt.ok) snprintf(bt_paired, sizeof(bt_paired), "%s", bt.found[0] ? bt.found : "The controller");
		else toast_show(bt.message);
	}
	if (!bt_head) return;
	if (bt_title) {                                    /* the side */
		lv_color_t c = bt.busy ? PAL.acc : bt_paired[0] ? lv_color_hex(0x5cf08a) : !bt.present ? PAL.dim : PAL.acc;
		lv_obj_set_style_border_color(bt_ring, c, 0);
		lv_obj_set_style_text_color(bt_ic, c, 0);
		if (!bt.present) {
			lv_label_set_text(bt_title, "No Bluetooth adapter");
			lv_label_set_text(bt_note, "Plug one in: it shows up here by itself.");
		} else if (bt.busy) {
			lv_label_set_text(bt_title, "Looking for controllers");
			lv_label_set_text(bt_note, "Put the controller in pairing mode now.");
		} else if (bt_paired[0]) {
			char t[96];
			snprintf(t, sizeof(t), "%s paired", bt_paired);
			lv_label_set_text(bt_title, t);
			lv_label_set_text(bt_note, "It connects by itself from now on.\nCheck its buttons in Settings \xE2\x80\xBA Controllers.");
		} else if (bt.seen_at && !bt.ok && bt.message[0]) {
			lv_label_set_text(bt_title, "Pairing didn't work");
			lv_label_set_text(bt_note, "Put it in pairing mode again, then Pair a controller.");
		} else {
			lv_label_set_text(bt_title, "Pair a controller");
			lv_label_set_text(bt_note, "Put the controller in pairing mode, then press A.");
		}
		lv_obj_set_hidden(bt_tips, !bt.present || (bt_paired[0] && !bt.busy));
	}
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
		snprintf(sub, sizeof(sub), "%s: A disconnects it (it reconnects when you turn it on), X deletes it.", bt.name[bt.pick]);
	lv_label_set_text(bt_head, h);
	lv_label_set_text(bt_sub, sub);
	bt_list.count = bt.present ? 1 + bt.ndev + (bt.ndev ? 1 : 0) : 0;    /* (Delete all pairings last) */
	footer_set(bt.mode == BT_ACTION ? BT_ACTION_HINTS : bt_list.sel >= 1 && bt_list.sel <= bt.ndev ? BT_DEV_HINTS : BT_HINTS);
	vlist_refresh(&bt_list);
}

static void bt_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	if (i == 0) {
		snprintf(t, n, "%s", bt.busy ? "Stop looking" : "Pair a controller");
		int left = BT_LOOK_S - (int)((lv_tick_get() - bt_started) / 1000);
		if (bt.busy && bt_started && left > 0) snprintf(val, vn, "Searching...  %d s", left);
		else snprintf(val, vn, "%s", bt.busy ? "Searching..." : ">");
		return;
	}
	int confirming = bt_confirm_row == i && lv_tick_get() - bt_confirm_at < 4000;
	if (i == bt.ndev + 1) {
		snprintf(t, n, "Delete all pairings");
		snprintf(val, vn, "%s", confirming ? "Press A again to delete them all" : "");
		return;
	}
	i--;
	snprintf(t, n, "%s", bt.name[i]);
	snprintf(val, vn, "%s", confirming ? "Press X again to delete" : bt.conn[i] ? "Connected" : "Not connected");
	*dim = !bt.conn[i];
}

static void bt_do(const char *what, const char *mac)
{
	char q[128];
	if (mac) snprintf(q, sizeof(q), "{\"cmd\":\"bluetooth\",\"do\":\"%s\",\"mac\":\"%s\"}", what, mac);
	else snprintf(q, sizeof(q), "{\"cmd\":\"bluetooth\",\"do\":\"%s\"}", what);
	cJSON *r = api_call("%s", q);
	if (!api_ok(r)) { toast_show(r && api_str(r, "error") ? api_str(r, "error") : "That didn't work"); cJSON_Delete(r); return; }
	if (!strcmp(what, "pair")) { bt.busy = 1; bt_started = lv_tick_get(); bt_paired[0] = 0; }
	bt_apply(r);
}

static void show_bt(void)
{
	cur = SCR_BT;
	lv_obj_clean(body);
	bt_paired[0] = 0;
	screen_title("SETTINGS", "Bluetooth");
	bt_head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_label_set_long_mode(bt_head, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(bt_head, W - S(112), S(24));
	lv_obj_set_pos(bt_head, S(56), S(178));
	bt_sub = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(bt_sub, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(bt_sub, W - S(112), S(22));
	lv_obj_set_pos(bt_sub, S(56), S(208));
	lr_head_sub(bt_head, bt_sub);
	vlist_create(&bt_list, body, S(56), S(244), SW(600), S(380));
	bt_list.fill = bt_fill;
	{
		int px = S(56) + SW(600) + S(24), pw = W - S(56) - px;
		lv_obj_t *side = mk_box(body, pw, H - S(82) - S(176), PAL.bg, 0);
		lv_obj_set_style_bg_opa(side, LV_OPA_TRANSP, 0);
		lv_obj_set_pos(side, px, S(176));
		lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_flex_align(side, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_style_pad_top(side, S(28), 0);
		lv_obj_set_style_pad_row(side, S(8), 0);
		if (theme_lr) lv_obj_set_hidden(side, true);
		bt_ring = mk_box(side, S(108), S(108), PAL.bg, LV_RADIUS_CIRCLE);
		lv_obj_set_style_bg_opa(bt_ring, LV_OPA_TRANSP, 0);
		lv_obj_set_style_border_width(bt_ring, LV_MAX(2, S(4)), 0);
		lv_obj_set_style_margin_bottom(bt_ring, S(16), 0);
		bt_ic = icon_create(bt_ring, ICON_BLUETOOTH, S(68));
		lv_obj_center(bt_ic);
		bt_title = mk_label(side, FONT_HEADING, PAL.text, "");
		lv_obj_set_width(bt_title, pw - S(24));
		lv_obj_set_style_text_align(bt_title, LV_TEXT_ALIGN_CENTER, 0);
		lv_label_set_long_mode(bt_title, LV_LABEL_LONG_MODE_WRAP);
		bt_note = mk_label(side, FONT_BODY, PAL.dim, "");
		lv_obj_set_width(bt_note, pw - S(24));
		lv_obj_set_style_text_align(bt_note, LV_TEXT_ALIGN_CENTER, 0);
		lv_label_set_long_mode(bt_note, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_set_style_text_line_space(bt_note, S(6), 0);
		bt_tips = mk_box(side, pw - S(24), LV_SIZE_CONTENT, PAL.bg, 0);
		lv_obj_set_style_bg_opa(bt_tips, LV_OPA_TRANSP, 0);
		lv_obj_set_flex_flow(bt_tips, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_flex_align(bt_tips, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_style_pad_top(bt_tips, S(20), 0);
		lv_obj_set_style_pad_row(bt_tips, S(2), 0);
		static const char *const TIPS[][2] = {
			{ "8BitDo", "Start + Y (Switch mode) or Start + X (X-input)" },
			{ "Xbox", "hold the pair button on top" },
			{ "PlayStation", "hold Share + PS until the light flashes" },
			{ "Switch Pro", "hold the sync button on top" },
		};
		for (int i = 0; i < 4; i++) {
			lv_obj_t *a = mk_label(bt_tips, FONT_NAME, PAL.text, TIPS[i][0]);
			lv_obj_set_style_margin_top(a, i ? S(12) : 0, 0);
			mk_label(bt_tips, FONT_SMALL, PAL.dim, TIPS[i][1]);
		}
	}
	bt_list.sel = 0;
	bt_list.top = 0;
	bt_list.count = 0;
	bt.mode = BT_MAIN;
	bt_apply(api_call("{\"cmd\":\"bluetooth\"}"));
}

static void bt_key(pkey_t k)
{
	if (vlist_key(&bt_list, k)) {
		bt_confirm_row = -1;
		if (bt.mode == BT_ACTION) { bt.mode = BT_MAIN; bt_apply(api_call("{\"cmd\":\"bluetooth\"}")); }
		else footer_set(bt_list.sel >= 1 && bt_list.sel <= bt.ndev ? BT_DEV_HINTS : BT_HINTS);
		return;
	}
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
			if (k == K_X) toast_show("Deleted: pair it again to use it");
		}
		return;
	}
	if (!bt.present) return;
	int sel = bt_list.sel, dev = sel >= 1 && sel <= bt.ndev;
	if (k == K_X && dev) {                             /* delete (forget) this one: X twice */
		if (bt_confirm_row == sel && lv_tick_get() - bt_confirm_at < 4000) {
			char mac[18];
			snprintf(mac, sizeof(mac), "%s", bt.mac[sel - 1]);
			bt_confirm_row = -1;
			bt_do("forget", mac);
			toast_show("Deleted: pair it again to use it");
		} else { bt_confirm_row = sel; bt_confirm_at = lv_tick_get(); vlist_refresh(&bt_list); }
		return;
	}
	if (k != K_A) return;
	if (sel == bt.ndev + 1 && bt.ndev) {               /* Delete all pairings: A twice */
		if (bt_confirm_row == sel && lv_tick_get() - bt_confirm_at < 4000) {
			bt_confirm_row = -1;
			char macs[24][18];
			int n = bt.ndev;
			for (int i = 0; i < n; i++) snprintf(macs[i], sizeof(macs[0]), "%s", bt.mac[i]);
			for (int i = 0; i < n; i++) bt_do("forget", macs[i]);
			bt_list.sel = 0;
			toast_show("All pairings deleted");
		} else { bt_confirm_row = sel; bt_confirm_at = lv_tick_get(); vlist_refresh(&bt_list); }
		return;
	}
	if (bt_list.sel == 0) { bt_do(bt.busy ? "cancel" : "pair", NULL); return; }
	if (bt.busy) { toast_show("Busy: wait for pairing or stop it"); return; }
	bt.pick = bt_list.sel - 1;
	bt.mode = BT_ACTION;
	bt_apply(api_call("{\"cmd\":\"bluetooth\"}"));
}

/* ---------- Controllers ----------
 * USB and Bluetooth controllers, the way SS1 Tool shows them: its button layouts, picked automatically
 * (or by hand, kept per controller), lit up live. Map buttons opens a locked window where MiSTer's own
 * mapping (run by Phosphor's Main) records the buttons; Reset to automatic, backups and restore.
 * phosphord reads the controllers and does the files; nothing here writes a mapping itself. */

#define CT_MAX 12
static struct {
	int n, nbk, mode;                       /* mode: 0 the controllers, 1 the backups */
	char ev[CT_MAX][16], vp[CT_MAX][16], name[CT_MAX][96], style[CT_MAX][24], uses[CT_MAX][12], where[CT_MAX][16];
	int style_set[CT_MAX], ncores[CT_MAX];
	char has[CT_MAX][24][8];
	int nhas[CT_MAX];
	char bk[24][64];
	int bkfiles[24];
	padview_t *pv;
	lv_obj_t *style_lab, *desc, *empty;
	uint32_t confirm_at;                    /* a second press confirms (reset, restore) */
	int confirm_row;
	char on[40][8];                         /* what's pressed now (live) */
} ct;
static vlist_t ct_list;
enum { CT_BACKUP_NOW, CT_BACKUPS, CT_ACTIONS };

static const char *ct_where(const char *w)
{
	return !strcmp(w, "bluetooth") ? "Bluetooth" : !strcmp(w, "dock") ? "Dock USB" : !strcmp(w, "superstation") ? "SS1 USB" : "USB";
}

static void ct_apply(cJSON *r)
{
	if (!api_ok(r)) { cJSON_Delete(r); return; }
	cJSON *a = cJSON_GetObjectItem(r, "controllers"), *x;
	ct.n = 0;
	cJSON_ArrayForEach(x, a) {
		if (ct.n >= CT_MAX) break;
		int i = ct.n++;
		snprintf(ct.ev[i], sizeof(ct.ev[i]), "%s", api_str(x, "id") ? api_str(x, "id") : "");
		snprintf(ct.vp[i], sizeof(ct.vp[i]), "%s", api_str(x, "vidpid") ? api_str(x, "vidpid") : "");
		snprintf(ct.name[i], sizeof(ct.name[i]), "%s", api_str(x, "name") ? api_str(x, "name") : "Controller");
		snprintf(ct.style[i], sizeof(ct.style[i]), "%s", api_str(x, "style") ? api_str(x, "style") : "generic");
		snprintf(ct.where[i], sizeof(ct.where[i]), "%s", api_str(x, "where") ? api_str(x, "where") : "");
		ct.style_set[i] = cJSON_IsTrue(cJSON_GetObjectItem(x, "style_set"));
		cJSON *m = cJSON_GetObjectItem(x, "mapping");
		snprintf(ct.uses[i], sizeof(ct.uses[i]), "%s", api_str(m, "uses") ? api_str(m, "uses") : "none");
		ct.ncores[i] = cJSON_GetArraySize(cJSON_GetObjectItem(m, "cores"));
		ct.nhas[i] = 0;
		cJSON *h;
		cJSON_ArrayForEach(h, cJSON_GetObjectItem(x, "has"))
			if (ct.nhas[i] < 24 && cJSON_IsString(h)) snprintf(ct.has[i][ct.nhas[i]++], 8, "%s", h->valuestring);
	}
	ct.nbk = 0;
	cJSON_ArrayForEach(x, cJSON_GetObjectItem(r, "backups")) {
		if (ct.nbk >= 24) break;
		snprintf(ct.bk[ct.nbk], sizeof(ct.bk[0]), "%s", api_str(x, "name") ? api_str(x, "name") : "");
		ct.bkfiles[ct.nbk++] = (int)api_num(x, "files", 0);
	}
	cJSON_Delete(r);
}

/* "20261023-181512_before-reset" -> "Oct 23, 6:15 PM · before reset" */
static void ct_bk_label(const char *b, char *out, size_t n)
{
	struct tm tm = { 0 };
	const char *rest = strchr(b, '_');
	if (sscanf(b, "%4d%2d%2d-%2d%2d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min) == 5) {
		tm.tm_year -= 1900; tm.tm_mon -= 1;
		char d[40];
		strftime(d, sizeof(d), SETTINGS.clock24 ? "%b %d, %H:%M" : "%b %d, %I:%M %p", &tm);
		char lab[48] = "";
		if (rest) { snprintf(lab, sizeof(lab), "%s", rest + 1); for (char *c = lab; *c; c++) if (*c == '_' || *c == '-') *c = ' '; }
		snprintf(out, n, "%s%s%s", d, lab[0] ? "  \xC2\xB7  " : "", lab);
	} else snprintf(out, n, "%s", b);
}

static void ct_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	if (ct.mode == 1) {
		if (!ct.nbk) { *dim = 1; snprintf(t, n, "No backups yet"); return; }
		ct_bk_label(ct.bk[i], t, n);
		if (ct.confirm_row == i && lv_tick_get() - ct.confirm_at < 4000) snprintf(val, vn, "Press A again to restore");
		else snprintf(val, vn, "%d file%s", ct.bkfiles[i], ct.bkfiles[i] == 1 ? "" : "s");
		return;
	}
	if (i < ct.n) {
		snprintf(t, n, "%s   %s", ct.name[i], ct_where(ct.where[i]));
		if (ct.confirm_row == i && lv_tick_get() - ct.confirm_at < 4000) snprintf(val, vn, "Press X again to reset");
		else if (!strcmp(ct.uses[i], "custom")) snprintf(val, vn, "Custom mapping  >");
		else if (!strcmp(ct.uses[i], "database")) snprintf(val, vn, "Automatic  >");
		else snprintf(val, vn, "Not in MiSTer's list  >");
		return;
	}
	switch (i - ct.n) {
	case CT_BACKUP_NOW: snprintf(t, n, "Back up mappings now"); break;
	case CT_BACKUPS: snprintf(t, n, "Backups"); snprintf(val, vn, "%d  >", ct.nbk); break;
	}
}

static const char *const CT_HINTS[] = { "A", "Map buttons", "Y", "Test it", "X", "Reset to automatic", "\xE2\x86\x94", "Drawing", "B", "Back", NULL };
static const char *const CT_HINTS2[] = { "A", "Choose", "B", "Back", NULL };
static const char *const CT_HINTS3[] = { "A", "Restore", "B", "Back", NULL };

static void ct_side(void)
{
	int s = ct_list.sel, pad = ct.mode == 0 && s < ct.n;
	lv_obj_set_hidden(padview_obj(ct.pv), !pad || theme_lr);
	lv_obj_set_hidden(ct.style_lab, !pad || theme_lr);
	footer_set(ct.mode == 1 ? CT_HINTS3 : pad ? CT_HINTS : CT_HINTS2);
	if (pad) {
		padview_layout(ct.pv, ct.style[s]);
		char b[96];
		snprintf(b, sizeof(b), "<  %s%s  >", padview_name(ct.style[s]), ct.style_set[s] ? "" : " (automatic)");
		lv_label_set_text(ct.style_lab, b);
		lv_label_set_text(ct.desc, !strcmp(ct.uses[s], "custom") ?
			"Uses its own button mapping in MiSTer's menu and every core. Map buttons to change it; Reset to automatic goes back to MiSTer's controller list." :
			!strcmp(ct.uses[s], "database") ? "MiSTer knows this controller and maps it by itself. Map buttons to set it up your way." :
			"MiSTer doesn't know this controller yet: map its buttons once and it works in the menu and every core.");
		/* (the drawing also says which face buttons are A, B, X and Y in Phosphor's menu) */
	} else if (ct.mode == 1) {
		lv_label_set_text(ct.desc, "Every backup holds all the mapping files and your controller profiles as they were. Restoring one backs up the current ones first.");
	} else {
		int a = s - ct.n;
		lv_label_set_text(ct.desc, a == CT_BACKUP_NOW ? "Saves every controller's mapping and your controller profiles in phosphor/backups on the SD card." :
			"Put back the mappings from an earlier backup.");
	}
	lv_obj_set_hidden(ct.empty, ct.n > 0 || ct.mode == 1);
}

static void ct_refresh(cJSON *r)
{
	int keep = ct_list.sel;
	ct_apply(r);
	ct_list.count = ct.mode == 1 ? LV_MAX(1, ct.nbk) : ct.n + CT_ACTIONS;
	ct_list.sel = LV_MIN(keep, ct_list.count - 1);
	vlist_refresh(&ct_list);
	ct_side();
}

static void show_ctrl(void)
{
	cur = SCR_CTRL;
	lv_obj_clean(body);
	free(ct.pv);                            /* (its objects went with the screen) */
	screen_title("SETTINGS", "Controllers");
	vlist_create(&ct_list, body, S(56), S(176), SW(660), S(440));
	ct_list.fill = ct_fill;
	ct_list.sel = ct_list.top = 0;
	ct_list.count = 0;
	ct.mode = 0;
	ct.confirm_row = -1;
	/* the right side: the drawing's name at the top, the drawing as big as the side allows and in its
	   middle, what the mapping does at the bottom of the card (the card ends 68 above the screen's bottom) */
	int rx = SX(740), rw = W - S(56) - rx, ph = rw * 345 / 600;
	ct.style_lab = mk_label(body, FONT_SMALL, PAL.text, "");
	lv_obj_set_pos(ct.style_lab, rx, S(176));
	ct.desc = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_obj_set_width(ct.desc, rw);
	lv_label_set_long_mode(ct.desc, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_align(ct.desc, LV_ALIGN_BOTTOM_LEFT, rx, -S(82));
	int top = S(206), bot = H - S(82) - S(72);            /* (room for three lines of it) */
	ct.pv = padview_create(body, rx, top + LV_MAX(0, (bot - top - ph) / 2), rw);
	LR_HIDE(ct.desc); LR_HIDE(ct.style_lab); LR_HIDE(padview_obj(ct.pv));
	ct.empty = mk_label(body, FONT_BODY, PAL.dim, "No USB or Bluetooth controller found. Plug one in or switch it on: it shows up here by itself.");
	lv_obj_set_width(ct.empty, SW(660));
	lv_label_set_long_mode(ct.empty, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_pos(ct.empty, S(80), S(136) + S(46) * 4);
	ct_refresh(api_call("{\"cmd\":\"controllers\"}"));
}

/* the live state of the highlighted controller, into the drawing */
static char ct_live_on[40][8];
static void ct_live(padview_t *pv, const char *ev, int idx, const char *target, char done[][8], int ndone)
{
	/* in the background (api_bg_ask): the newest reply that came, drawn; none yet, the drawing stays */
	char key[40];
	snprintf(key, sizeof(key), "live:%.30s", ev);
	api_bg_ask(key, "{\"cmd\":\"controllers\",\"live\":\"%s\"}", ev);
	cJSON *r = api_bg_take(key);
	if (!r) return;
	cJSON *l = cJSON_GetObjectItem(r, "live");
	padstate_t s;
	memset(&s, 0, sizeof(s));
	s.nhas = idx >= 0 ? ct.nhas[idx] : -1;
	for (int i = 0; i < s.nhas; i++) s.has[i] = ct.has[idx][i];
	cJSON *x;
	cJSON_ArrayForEach(x, cJSON_GetObjectItem(l, "on"))
		if (s.non < 40 && cJSON_IsString(x)) { snprintf(ct_live_on[s.non], 8, "%.7s", x->valuestring); s.on[s.non] = ct_live_on[s.non]; s.non++; }
	cJSON *ls = cJSON_GetObjectItem(l, "ls"), *rs = cJSON_GetObjectItem(l, "rs");
	if (ls) { s.lx = (float)cJSON_GetArrayItem(ls, 0)->valuedouble; s.ly = (float)cJSON_GetArrayItem(ls, 1)->valuedouble; }
	if (rs) { s.rx = (float)cJSON_GetArrayItem(rs, 0)->valuedouble; s.ry = (float)cJSON_GetArrayItem(rs, 1)->valuedouble; }
	s.l2 = (float)api_num(l, "l2", 0);
	s.r2 = (float)api_num(l, "r2", 0);
	s.target = target;
	for (int i = 0; i < ndone && i < 40; i++) s.done[s.ndone++] = done[i];
	padview_state(pv, &s);
	cJSON_Delete(r);
}

/* ---------- Controller test: one controller, live (Settings > Controllers > Y) ----------
 * The controller's own drawing (the layout chosen for it), lit as it's pressed; the sticks and
 * triggers as numbers; and what MiSTer's Main passes on after its mapping. Locked like the mapping
 * window: every button is being tested, so only Select + Start together closes it. */
static struct {
	int idx;
	char ev[16], name[96], style[24], uses[12];
	padview_t *pv;
	lv_obj_t *what, *ask, *hint, *k[5], *v[5];
	char got[8][12];
	int ngot;
} tw;
static const char *const TW_HINTS[] = { "Select + Start", "Close (or Esc on a keyboard, B on the app's Remote)", NULL };

static void tw_poll(void)
{
	char key[40];
	snprintf(key, sizeof(key), "live:%.30s", tw.ev);
	api_bg_ask(key, "{\"cmd\":\"controllers\",\"live\":\"%s\"}", tw.ev);
	cJSON *r = api_bg_take(key);              /* (in the background: none yet, the screen stays as it is) */
	if (!r) return;
	cJSON *l = cJSON_GetObjectItem(r, "live");
	padstate_t s;
	memset(&s, 0, sizeof(s));
	s.nhas = tw.idx >= 0 && tw.idx < ct.n ? ct.nhas[tw.idx] : -1;
	for (int i = 0; i < s.nhas; i++) s.has[i] = ct.has[tw.idx][i];
	cJSON *x;
	cJSON_ArrayForEach(x, cJSON_GetObjectItem(l, "on"))
		if (s.non < 40 && cJSON_IsString(x)) { snprintf(ct_live_on[s.non], 8, "%.7s", x->valuestring); s.on[s.non] = ct_live_on[s.non]; s.non++; }
	cJSON *ls = cJSON_GetObjectItem(l, "ls"), *rs = cJSON_GetObjectItem(l, "rs");
	if (cJSON_GetArraySize(ls) == 2) { s.lx = (float)cJSON_GetArrayItem(ls, 0)->valuedouble; s.ly = (float)cJSON_GetArrayItem(ls, 1)->valuedouble; }
	if (cJSON_GetArraySize(rs) == 2) { s.rx = (float)cJSON_GetArrayItem(rs, 0)->valuedouble; s.ry = (float)cJSON_GetArrayItem(rs, 1)->valuedouble; }
	s.l2 = (float)api_num(l, "l2", 0);
	s.r2 = (float)api_num(l, "r2", 0);
	padview_state(tw.pv, &s);
	/* what's held, by this layout's names */
	char t[200] = "";
	size_t n = 0;
	for (int i = 0; i < s.non && n < sizeof(t) - 40; i++)
		n += (size_t)snprintf(t + n, sizeof(t) - n, "%s%s", n ? " + " : "", padview_label(tw.style, s.on[i]));
	char a[240];
	if (!r) snprintf(a, sizeof(a), "phosphord isn't answering");
	else if (!l) snprintf(a, sizeof(a), "Not connected");
	else if (n) snprintf(a, sizeof(a), "Pressing %s", t);
	else snprintf(a, sizeof(a), "Press anything");
	lv_label_set_text(tw.ask, a);
	/* the sticks and triggers */
	char v[64];
	float sx[2] = { s.lx, s.rx }, sy[2] = { s.ly, s.ry };
	for (int k = 0; k < 2; k++) {
		int px = (int)(sx[k] * 100 + (sx[k] < 0 ? -0.5f : 0.5f)), py = (int)(sy[k] * 100 + (sy[k] < 0 ? -0.5f : 0.5f));
		if (abs(px) < 3 && abs(py) < 3) snprintf(v, sizeof(v), "centred");
		else snprintf(v, sizeof(v), "%s %d%%, %s %d%%", px < 0 ? "left" : "right", abs(px), py < 0 ? "up" : "down", abs(py));
		lv_label_set_text(tw.v[k], v);
	}
	snprintf(v, sizeof(v), "%d%%", (int)(s.l2 * 100 + 0.5f)); lv_label_set_text(tw.v[2], v);
	snprintf(v, sizeof(v), "%d%%", (int)(s.r2 * 100 + 0.5f)); lv_label_set_text(tw.v[3], v);
	cJSON_Delete(r);
}

/* a key the shell got (MiSTer's Main, after its mapping): into "MiSTer gets" */
static void tw_got(pkey_t k)
{
	static const char *const N[] = { "", "Up", "Down", "Left", "Right", "A", "B", "X", "Y", "Menu", "Select", "L", "R", "Select+Start" };
	if (k <= KEY_NONE_ || k > K_POWER) return;
	if (tw.ngot == 8) { memmove(tw.got[0], tw.got[1], sizeof(tw.got[0]) * 7); tw.ngot = 7; }
	snprintf(tw.got[tw.ngot++], sizeof(tw.got[0]), "%s", N[k]);
	char t[120] = "";
	size_t n = 0;
	for (int i = 0; i < tw.ngot; i++) n += (size_t)snprintf(t + n, sizeof(t) - n, "%s%s", i ? ", " : "", tw.got[i]);
	lv_label_set_text(tw.v[4], t);
}

static void show_ctrl(void);
static void tw_key(pkey_t k)
{
	/* Select + Start; or B from a keyboard or a remote (the phone or PC app's Remote control): a controller
	   whose buttons come through wrong can't always press Select + Start together */
	if (k == K_POWER || (k == K_B && input_not_controller())) { padview_delete(tw.pv); tw.pv = NULL; show_ctrl(); return; }
	tw_got(k);
}

static void show_ctest(int idx)
{
	memset(&tw, 0, sizeof(tw));
	tw.idx = idx;
	snprintf(tw.ev, sizeof(tw.ev), "%s", ct.ev[idx]);
	snprintf(tw.name, sizeof(tw.name), "%s", ct.name[idx]);
	snprintf(tw.style, sizeof(tw.style), "%s", ct.style[idx]);
	snprintf(tw.uses, sizeof(tw.uses), "%s", ct.uses[idx]);
	cur = SCR_CTEST;
	lv_obj_clean(body);
	free(ct.pv);
	ct.pv = NULL;
	char t[160];
	snprintf(t, sizeof(t), "Testing %s", tw.name);
	screen_title("CONTROLLERS", t);
	snprintf(t, sizeof(t), "LIVE \xC2\xB7 %s \xC2\xB7 %s", padview_name(tw.style), !strcmp(tw.uses, "custom") ? "CUSTOM MAPPING" : "AUTOMATIC MAPPING");
	for (char *c = t; *c; c++) *c = (char)toupper((unsigned char)*c);
	tw.what = mk_label(body, FONT_LABEL, PAL.acc, t);
	lv_obj_set_style_text_letter_space(tw.what, S(1), 0);
	lv_obj_set_pos(tw.what, S(56), S(200));
	tw.ask = mk_label(body, FONT_HEADING, PAL.text, "Press anything");
	lv_obj_set_width(tw.ask, SW(440));
	lv_label_set_long_mode(tw.ask, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_pos(tw.ask, S(56), S(226));
	tw.hint = mk_label(body, FONT_BODY, PAL.dim, "Everything lights as you press it. Sticks and triggers show how far they go.");
	lv_obj_set_width(tw.hint, SW(430));
	lv_label_set_long_mode(tw.hint, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_pos(tw.hint, S(56), S(282));
	const char *K[5] = { "Left stick", "Right stick", padview_label(tw.style, "l2"), padview_label(tw.style, "r2"), "MiSTer gets" };
	for (int i = 0; i < 5; i++) {
		tw.k[i] = mk_label(body, FONT_BODY, PAL.dim, K[i]);
		lv_obj_set_pos(tw.k[i], S(56), S(360 + 30 * i));
		tw.v[i] = mk_label(body, FONT_BODY, PAL.text, i == 4 ? "nothing yet" : "");
		lv_obj_set_width(tw.v[i], S(300));
		lv_label_set_long_mode(tw.v[i], LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_pos(tw.v[i], S(190), S(360 + 30 * i));
	}
	/* the drawing in the middle of the card's right half (the card: under the title to above the hints,
	   the screen's margins at the sides; body_cards), as big as fits there: measured by what's drawn,
	   not its box (the box has room round the drawing) */
	{
		int cx1 = W / 2, cx2 = W - S(42), cy1 = S(162), cy2 = H - S(68), pad = S(24);
		int pw = LV_MIN(cx2 - cx1 - 2 * pad, (cy2 - cy1 - 2 * pad) * 600 / 345);
		lv_area_t u = { 0 };
		/* sized again until what's drawn fits (labels don't grow with the drawing: a bigger one can switch
		   to the bigger font), then centred */
		for (int pass = 0, fitted = 0; pass < 6; pass++) {
			if (tw.pv) { padview_delete(tw.pv); tw.pv = NULL; }
			tw.pv = padview_create(body, cx1, cy1, pw);
			padview_layout(tw.pv, tw.style);
			lv_obj_t *box = padview_obj(tw.pv);
			lv_obj_update_layout(box);
			lv_area_t a;
			int have = 0;
			for (uint32_t i = 0; i < lv_obj_get_child_count(box); i++) {
				lv_obj_t *c = lv_obj_get_child(box, (int32_t)i);
				if (lv_obj_is_hidden(c)) continue;
				lv_obj_get_coords(c, &a);
				if (!have) { u = a; have = 1; continue; }
				u.x1 = LV_MIN(u.x1, a.x1); u.y1 = LV_MIN(u.y1, a.y1); u.x2 = LV_MAX(u.x2, a.x2); u.y2 = LV_MAX(u.y2, a.y2);
			}
			if (!have) break;
			if (!fitted) {                                 /* as big as the drawing itself fits */
				int dw = lv_area_get_width(&u), dh = lv_area_get_height(&u);
				int fw = (cx2 - cx1 - 2 * pad) * 1000 / LV_MAX(1, dw), fh = (cy2 - cy1 - 2 * pad) * 1000 / LV_MAX(1, dh);
				int f = LV_MIN(fw, fh);
				if (f >= 980 && f <= 1020) fitted = 1;      /* (within 2 %: this size) */
				else { pw = pw * f / 1000; if (pass == 4) fitted = 1; continue; }
			}
			{
				/* across: in the middle of the room right of the info's text (to the card's right edge);
				   up and down: in the middle of the card */
				lv_area_t bc;
				lv_obj_get_coords(body, &bc);
				int ir = S(56);
				/* (not the LIVE · drawing · mapping line above: a long drawing name runs far right, over
				   the drawing's top, which is lower) */
				lv_obj_t *info[] = { tw.ask, tw.hint, tw.k[0], tw.k[1], tw.k[2], tw.k[3], tw.k[4], tw.v[0], tw.v[1], tw.v[2], tw.v[3], tw.v[4] };
				for (size_t i = 0; i < sizeof(info) / sizeof(info[0]); i++) {
					if (!info[i]) continue;
					lv_obj_update_layout(info[i]);
					const char *t = lv_label_get_text(info[i]);
					lv_point_t sz;
					int ow = lv_obj_get_width(info[i]);
					lv_text_get_size(&sz, t, lv_obj_get_style_text_font(info[i], 0), lv_obj_get_style_text_letter_space(info[i], 0), 0,
					                 lv_label_get_long_mode(info[i]) == LV_LABEL_LONG_MODE_WRAP ? ow : LV_COORD_MAX, LV_TEXT_FLAG_NONE);
					ir = LV_MAX(ir, lv_obj_get_x(info[i]) + LV_MIN(sz.x, ow));
				}
				int dx = (ir + cx2) / 2 - ((u.x1 + u.x2) / 2 - bc.x1), dy = (cy1 + cy2) / 2 - ((u.y1 + u.y2) / 2 - bc.y1);
				lv_obj_set_pos(box, lv_obj_get_x(box) + dx, lv_obj_get_y(box) + dy);
				break;
			}
		}
	}
	footer_set(TW_HINTS);
	tw_poll();
	plog("controllers: testing %s", tw.name);
}

static void show_map(int idx);

static void ct_key(pkey_t k)
{
	int s = ct_list.sel;
	if (vlist_key(&ct_list, k)) { ct.confirm_row = -1; ct_side(); return; }
	if (k == K_B || k == K_MENU) {
		if (ct.mode == 1) { ct.mode = 0; ct_list.sel = ct.n + CT_BACKUPS; ct_refresh(api_call("{\"cmd\":\"controllers\"}")); return; }
		back_to_settings(SET_CTRL);
		return;
	}
	if (ct.mode == 1) {
		if (k != K_A || !ct.nbk) return;
		if (ct.confirm_row != s || lv_tick_get() - ct.confirm_at > 4000) { ct.confirm_row = s; ct.confirm_at = lv_tick_get(); vlist_refresh(&ct_list); return; }
		ct.confirm_row = -1;
		cJSON *r = api_call("{\"cmd\":\"controllers\",\"do\":\"restore\",\"name\":\"%s\"}", ct.bk[s]);
		toast_show(api_ok(r) ? "Mappings put back: they work from the next game or menu" : (api_str(r, "error") ? api_str(r, "error") : "Couldn't restore"));
		cJSON_Delete(r);
		ct.mode = 0;
		ct_list.sel = 0;
		ct_refresh(api_call("{\"cmd\":\"controllers\"}"));
		return;
	}
	if (s < ct.n) {
		if (k == K_LEFT || k == K_RIGHT) {                  /* the drawing: Automatic, then every layout */
			int i = ct.style_set[s] ? padview_index(ct.style[s]) : -1;
			i += k == K_RIGHT ? 1 : -1;
			if (i >= padview_count()) i = -1;
			if (i < -1) i = padview_count() - 1;
			cJSON_Delete(api_call("{\"cmd\":\"controllers\",\"do\":\"style\",\"vidpid\":\"%s\",\"style\":\"%s\"}", ct.vp[s], i < 0 ? "auto" : padview_key(i)));
			input_relayout();                               /* (Nintendo drawings: A right, B bottom in the menu) */
			ct_refresh(api_call("{\"cmd\":\"controllers\"}"));
		} else if (k == K_A) {
			show_map(s);
		} else if (k == K_Y) {
			show_ctest(s);
		} else if (k == K_X) {
			if (strcmp(ct.uses[s], "custom")) { toast_show("This controller already uses MiSTer's automatic mapping"); return; }
			if (ct.confirm_row != s || lv_tick_get() - ct.confirm_at > 4000) { ct.confirm_row = s; ct.confirm_at = lv_tick_get(); vlist_refresh(&ct_list); return; }
			ct.confirm_row = -1;
			cJSON *r = api_call("{\"cmd\":\"controllers\",\"do\":\"reset\",\"vidpid\":\"%s\"}", ct.vp[s]);
			toast_show(api_ok(r) ? "Back to automatic. The old mapping is in Backups" : (api_str(r, "error") ? api_str(r, "error") : "Couldn't reset"));
			cJSON_Delete(r);
			ct_refresh(api_call("{\"cmd\":\"controllers\"}"));
		}
		return;
	}
	if (k != K_A) return;
	switch (s - ct.n) {
	case CT_BACKUP_NOW: {
		cJSON *r = api_call("{\"cmd\":\"controllers\",\"do\":\"backup\"}");
		char m[96];
		snprintf(m, sizeof(m), "Backed up %d file%s", (int)api_num(r, "saved", 0), api_num(r, "saved", 0) == 1 ? "" : "s");
		toast_show(api_ok(r) ? m : "Couldn't make the backup");
		cJSON_Delete(r);
		ct_refresh(api_call("{\"cmd\":\"controllers\"}"));
		break;
	}
	case CT_BACKUPS:
		ct.mode = 1;
		ct_list.sel = ct_list.top = 0;
		ct_refresh(api_call("{\"cmd\":\"controllers\"}"));
		break;
	}
}

/* ---------- the mapping window ----------
 * Locked, like Looking for games: MiSTer's own mapping runs in Main and decides what counts; this shows
 * the step and the controller. Select + Start cancels (nothing saved). Asked for from the TV or, through
 * phosphord, from the app (<tmp>/map-request); its state goes to <tmp>/map-state for the app. */

static const struct { int step; const char *what, *ask, *target, *hint; } MSTEPS[] = {
	{ -6, "D-pad test", "Press right on the D-pad", "right", "MiSTer checks how the D-pad talks first." },
	{ -5, "D-pad test", "Press down on the D-pad", "down", NULL },
	{ -4, "Left stick", "Tilt the left stick right", "ls", "No left stick? Press any button." },
	{ -3, "Left stick", "Tilt the left stick down", "ls", "No left stick? Press any button." },
	{ -2, "Right stick", "Tilt the right stick right", "rs", "No right stick? Press any button." },
	{ -1, "Right stick", "Tilt the right stick down", "rs", "No right stick? Press any button." },
	{ 0, "D-pad", "Press RIGHT", "right", NULL }, { 1, "D-pad", "Press LEFT", "left", NULL },
	{ 2, "D-pad", "Press DOWN", "down", NULL }, { 3, "D-pad", "Press UP", "up", NULL },
	{ 4, "Buttons", "Press A", "b", "The ring shows where A usually goes; any button works." },
	{ 5, "Buttons", "Press B", "a", NULL },
	{ 6, "Buttons", "Press X", "y", "Not on this controller? Press a button you've already used." },
	{ 7, "Buttons", "Press Y", "x", "Not on this controller? Press a button you've already used." },
	{ 8, "Buttons", "Press L", "l1", "Not on this controller? Press a button you've already used." },
	{ 9, "Buttons", "Press R", "r1", "Not on this controller? Press a button you've already used." },
	{ 10, "Buttons", "Press SELECT", "back", "Not on this controller? Press a button you've already used." },
	{ 11, "Buttons", "Press START", "start", "Not on this controller? Press a button you've already used." },
	{ 20, "Menu", "Press the MENU button", "guide", "Opens MiSTer's menu in a game. Two buttons together work too." },
	{ 21, "Menu", "Press OK", "b", "OK in MiSTer's menu: usually the same button as A." },
	{ 22, "Menu", "Press BACK", "a", "Back in MiSTer's menu: usually the same button as B." },
	{ 23, "Menu", "Tilt a stick right", "ls", "For moving in MiSTer's menu. No stick? Press any button." },
	{ 24, "Menu", "Tilt a stick down", "ls", "For moving in MiSTer's menu. No stick? Press any button." },
};
#define MSTEP_N ((int)(sizeof(MSTEPS) / sizeof(MSTEPS[0])))

static struct {
	int idx, step, finished, from_app;
	uint32_t end_at, started;
	char vp[16], ev[16], name[96], style[24];
	char done[40][8], seen[40][8], ask_now[64];
	int ndone, nseen;
	lv_obj_t *what, *ask, *hint, *bar, *note;
	padview_t *pv;
} mw;

static void map_state_file(const char *extra)
{
	char p[300], t[310];
	const char *tmp = getenv("PHOS_TMP");
	snprintf(p, sizeof(p), "%s/map-state", tmp && *tmp ? tmp : "/tmp/phosphor");
	snprintf(t, sizeof(t), "%s.new", p);
	FILE *f = fopen(t, "w");
	if (!f) return;
	int i = -1;
	for (int k = 0; k < MSTEP_N; k++) if (MSTEPS[k].step == mw.step) i = k;
	fprintf(f, "{\"on\":%s,\"dev\":\"%s\",\"name\":\"%s\",\"step\":%d,\"n\":%d,\"of\":%d,\"ask\":\"%s\",\"target\":\"%s\"%s}\n",
	        mw.finished ? "false" : "true", mw.vp, mw.name, mw.step, i + 1, MSTEP_N, i >= 0 ? (mw.ask_now[0] ? mw.ask_now : MSTEPS[i].ask) : "", i >= 0 ? MSTEPS[i].target : "", extra ? extra : "");
	fclose(f);
	rename(t, p);
}

static void map_show_step(void)
{
	int i = -1;
	for (int k = 0; k < MSTEP_N; k++) if (MSTEPS[k].step == mw.step) i = k;
	if (i < 0) return;
	char b[64];
	snprintf(b, sizeof(b), "%s  \xC2\xB7  %d OF %d", MSTEPS[i].what, i + 1, MSTEP_N);
	for (char *c = b; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
	lv_label_set_text(mw.what, b);
	/* MiSTer's A, B, X and Y are a SNES pad's: A is the right-hand face button. Asked by the name this
	   controller has on the button the ring shows, so "Press A" never points at a button printed B */
	const char *mn = mw.step == 4 ? "A" : mw.step == 5 ? "B" : mw.step == 6 ? "X" : mw.step == 7 ? "Y" : NULL;
	const char *lab = mn ? padview_label(mw.style, MSTEPS[i].target) : NULL;
	if (mn && lab && *lab && strcmp(lab, mn)) {
		char ask[64], hint[200];
		snprintf(ask, sizeof(ask), "Press %s", lab);
		snprintf(hint, sizeof(hint), "MiSTer's %s button: MiSTer names them as on a SNES pad, so on this controller that's %s (the ring shows it).%s",
			mn, lab, mw.step >= 6 ? " Not on this controller? Press a button you've already used." : " Any button works.");
		lv_label_set_text(mw.ask, ask);
		lv_label_set_text(mw.hint, hint);
		snprintf(mw.ask_now, sizeof(mw.ask_now), "%s", ask);
	} else {
		mw.ask_now[0] = 0;
		lv_label_set_text(mw.ask, MSTEPS[i].ask);
		lv_label_set_text(mw.hint, MSTEPS[i].hint ? MSTEPS[i].hint : "");
	}
	lv_bar_set_value(mw.bar, (i * 100) / MSTEP_N, LV_ANIM_OFF);
	map_state_file(NULL);
}

static const char *const MAP_HINTS[] = { "Select + Start", "Cancel, nothing saved (or Esc on a keyboard, B on the app's Remote)", NULL };

static void show_map(int idx)
{
	if (!mainlink_can_map()) { toast_show("Mapping needs Phosphor's Main (phosphor-main 0.27 or newer)"); return; }
	char what[32];
	snprintf(what, sizeof(what), "start %s", ct.vp[idx]);
	if (mainlink_map(what)) { toast_show("Couldn't start the mapping"); return; }
	memset(&mw, 0, sizeof(mw));
	mw.idx = idx;
	mw.step = -100;
	mw.started = lv_tick_get();
	snprintf(mw.vp, sizeof(mw.vp), "%s", ct.vp[idx]);
	snprintf(mw.ev, sizeof(mw.ev), "%s", ct.ev[idx]);
	snprintf(mw.name, sizeof(mw.name), "%s", ct.name[idx]);
	snprintf(mw.style, sizeof(mw.style), "%s", ct.style[idx]);
	cur = SCR_MAP;
	lv_obj_clean(body);
	free(ct.pv);
	ct.pv = NULL;
	char t[128];
	snprintf(t, sizeof(t), "Mapping %s", mw.name);
	screen_title("CONTROLLERS", t);
	mw.what = mk_label(body, FONT_LABEL, PAL.acc, "STARTING");
	lv_obj_set_style_text_letter_space(mw.what, S(1), 0);
	lv_obj_set_pos(mw.what, S(56), S(200));
	mw.ask = mk_label(body, FONT_HEADING, PAL.text, "");
	lv_obj_set_pos(mw.ask, S(56), S(226));
	mw.hint = mk_label(body, FONT_BODY, PAL.dim, "");
	lv_obj_set_width(mw.hint, SW(430));
	lv_label_set_long_mode(mw.hint, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_pos(mw.hint, S(56), S(282));
	mw.bar = lv_bar_create(body);
	lv_obj_set_size(mw.bar, SW(430), S(10));
	lv_obj_set_pos(mw.bar, S(56), S(560));
	lv_obj_set_style_bg_color(mw.bar, PAL.line, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(mw.bar, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(mw.bar, PAL.acc, LV_PART_INDICATOR);
	lv_bar_set_range(mw.bar, 0, 100);
	mw.note = mk_label(body, FONT_SMALL, PAL.dim, "Each button goes into MiSTer's mapping for this controller: the menu, Phosphor and every core.");
	lv_obj_set_width(mw.note, SW(430));
	lv_label_set_long_mode(mw.note, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_pos(mw.note, S(56), S(586));
	mw.pv = padview_create(body, SX(520), S(196), SW(704));
	padview_layout(mw.pv, mw.style);
	footer_set(MAP_HINTS);
	plog("controllers: mapping %s (%s)", mw.name, mw.vp);
}

static void map_finish(int saved, const char *err)
{
	mw.finished = 1;
	mw.end_at = lv_tick_get();
	lv_label_set_text(mw.what, saved ? "SAVED" : "CANCELLED");
	lv_label_set_text(mw.ask, saved ? "All set" : "Nothing changed");
	lv_label_set_text(mw.hint, saved ? "This controller now works this way in MiSTer's menu, Phosphor and every core. The old mapping is in Backups." :
	                  err && *err ? err : "The controller keeps the mapping it had.");
	lv_bar_set_value(mw.bar, saved ? 100 : 0, LV_ANIM_OFF);
	char ex[160];
	snprintf(ex, sizeof(ex), ",\"done\":true,\"saved\":%s,\"error\":\"%s\"", saved ? "true" : "false", err ? err : "");
	map_state_file(ex);
	plog("controllers: mapping %s %s%s%s", mw.vp, saved ? "saved" : "cancelled", err && *err ? ": " : "", err ? err : "");
}

static void map_tick(void)
{
	if (mw.finished) {
		if (lv_tick_get() - mw.end_at > 3000) { show_ctrl(); }
		return;
	}
	int step, done, saved;
	char dev[16], err[96];
	int on = mainlink_map_state(&step, &done, &saved, dev, sizeof(dev), err, sizeof(err));
	if (!on && done) { map_finish(saved, err); return; }
	if (!on && !done && lv_tick_get() - mw.started > 3000) { map_finish(0, "Phosphor's Main didn't start the mapping"); return; }
	if (step != mw.step && step != -100) {
		/* what was pressed for the step that just ended: lit as mapped from here on */
		/* (not before the first step: what was held as the window opened, A or Start, isn't mapped) */
		if (mw.step != -100) for (int i = 0; i < mw.nseen && mw.ndone < 40; i++) snprintf(mw.done[mw.ndone++], 8, "%s", mw.seen[i]);
		mw.nseen = 0;
		mw.step = step;
		map_show_step();
	}
	int i = -1;
	for (int k = 0; k < MSTEP_N; k++) if (MSTEPS[k].step == mw.step) i = k;
	ct_live(mw.pv, mw.ev, mw.idx, i >= 0 ? MSTEPS[i].target : NULL, mw.done, mw.ndone);
	for (int k = 0; k < 40 && ct_live_on[k][0]; k++) {
		int have = 0;
		for (int j = 0; j < mw.nseen; j++) have |= !strcmp(mw.seen[j], ct_live_on[k]);
		if (!have && mw.nseen < 40) snprintf(mw.seen[mw.nseen++], 8, "%.7s", ct_live_on[k]);
	}
	memset(ct_live_on, 0, sizeof(ct_live_on));
}

static void map_key(pkey_t k)
{
	if (mw.finished) { if (k == K_A || k == K_B) show_ctrl(); return; }
	if (k == K_POWER || (k == K_B && input_not_controller())) mainlink_map("cancel");   /* (B: a keyboard or a remote) */
}

/* the app asks through phosphord: <tmp>/map-request ("start VVVV:PPPP", "skip", "cancel") */
static void script_requests(void)
{
	char p[300], path[600] = "";
	const char *t = getenv("PHOS_TMP");
	snprintf(p, sizeof(p), "%s/script-request", t && *t ? t : "/tmp/phosphor");
	FILE *f = fopen(p, "r");
	if (!f) return;
	if (!fgets(path, sizeof(path), f)) path[0] = 0;
	fclose(f);
	unlink(p);
	path[strcspn(path, "\r\n")] = 0;
	if (!path[0] || strstr(path, "/../") || !strstr(path, "/Scripts/")) return;
	plog("scripts: %s asked from the app", path);
	run_script(path);
}

static void map_requests(void)
{
	script_requests();
	char p[300], what[48] = "";
	const char *t = getenv("PHOS_TMP");
	snprintf(p, sizeof(p), "%s/map-request", t && *t ? t : "/tmp/phosphor");
	FILE *f = fopen(p, "r");
	if (!f) return;
	if (!fgets(what, sizeof(what), f)) what[0] = 0;
	fclose(f);
	unlink(p);
	what[strcspn(what, "\r\n")] = 0;
	plog("controllers: asked from another device (%s)", what);
	if (!strncmp(what, "start ", 6)) {
		if (cur == SCR_MAP && !mw.finished) return;
		if (cur != SCR_CTRL) show_ctrl();
		for (int i = 0; i < ct.n; i++) if (!strcmp(ct.vp[i], what + 6)) { show_map(i); mw.from_app = 1; return; }
		toast_show("That controller isn't connected");
	} else if (cur == SCR_MAP && !mw.finished && (!strcmp(what, "skip") || !strcmp(what, "cancel"))) {
		mainlink_map(what);
	}
}

/* ---------- Update Phosphor ----------
 * Phosphor's own updates, from a release package (phosphor-<version>-armhf.tar.gz) put in
 * phosphor/updates (over the network share, or sent by the app), at the top of the SD card or of a USB
 * drive; and rolling back to the version before. phosphor/update.sh does the work: it checks the
 * package, backs up the installed version, installs, and starts Phosphor again (this screen goes
 * away while it does). */

/* Phosphor's releases on GitHub (phosphord upnet.c): looked at a minute after start-up and every 30 min
 * while no game runs; a newer one shows a yellow arrow in the status bar and a Download row here */
static struct { int newer, downloaded; long checked; char latest[24], state[16], msg[200], err[120]; } upo;
static void up_recount(void);
static void upo_apply(cJSON *r)
{
	cJSON *on = r ? cJSON_GetObjectItem(r, "online") : NULL;
	if (!cJSON_IsObject(on)) return;
	upo.checked = (long)api_num(on, "checked", 0);
	snprintf(upo.err, sizeof(upo.err), "%s", api_str(on, "error") ? api_str(on, "error") : "");
	/* no release in the answer (GitHub didn't answer and nothing was known yet): what was known stays */
	if (!api_str(on, "latest")) { up_recount(); return; }
	upo.newer = cJSON_IsTrue(cJSON_GetObjectItem(on, "newer"));
	upo.downloaded = cJSON_IsTrue(cJSON_GetObjectItem(on, "downloaded"));
	snprintf(upo.latest, sizeof(upo.latest), "%s", api_str(on, "latest") ? api_str(on, "latest") : "");
	snprintf(upo.state, sizeof(upo.state), "%s", api_str(on, "state") ? api_str(on, "state") : "");
	snprintf(upo.msg, sizeof(upo.msg), "%s", api_str(on, "msg") ? api_str(on, "msg") : "");
	if (sb_upd) lv_obj_set_hidden(sb_upd, !upo.newer);
	up_recount();                                     /* (the Update screen, when it's open) */
}
/* the Download row on top: until it's downloaded and listed as a package (then its Install row takes the
   same place; never a moment with neither) */
static int upo_listed(void);
static int upo_row(void) { return upo.newer && (!upo.downloaded || !upo_listed()) ? 1 : 0; }

static struct {
	int npk, nrb, busy, old_files;
	double old_kb;
	char ver[24], pk_file[8][300], pk_ver[8][24], rb_ver[6][24], rb_date[6][24], msg[200];
	uint32_t confirm_at;
	int confirm_row;
	lv_obj_t *head, *note;
} up;
static vlist_t up_list;
static int upo_listed(void) { for (int i = 0; i < up.npk; i++) if (!strcmp(up.pk_ver[i], upo.latest)) return 1; return 0; }
/* the Update screen's rows follow every answer (also the half-hourly one): a row count that's off would
   put a press on the wrong row; and a confirm ("Press A again") is for the row as it was, so it goes */
static void up_recount(void)
{
	if (cur != SCR_PUPD) return;
	int n = 1 + upo_row() + up.npk + up.nrb + (up.old_files > 0);
	if (n != up_list.count) {
		if (up_list.count) up.confirm_row = -1;
		up_list.count = n;
		if (up_list.sel >= n) up_list.sel = n - 1;
	}
	vlist_refresh(&up_list);
}

/* 0.27.21 against 0.27.6: number by number */
static int ver_cmp(const char *a, const char *b)
{
	while (*a || *b) {
		if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
			long x = strtol(a, (char **)&a, 10), y = strtol(b, (char **)&b, 10);
			if (x != y) return x < y ? -1 : 1;
		} else {
			if (*a != *b) return (unsigned char)*a < (unsigned char)*b ? -1 : 1;
			a++; b++;
		}
	}
	return 0;
}

static void up_apply(cJSON *r)
{
	if (!api_ok(r)) { cJSON_Delete(r); return; }
	upo_apply(r);
	snprintf(up.ver, sizeof(up.ver), "%s", api_str(r, "version") ? api_str(r, "version") : "?");
	cJSON *x;
	up.npk = up.nrb = 0;
	/* the newest 8, newest first (phosphord lists them by file name: 0.27.6 after 0.27.21) */
	{
		cJSON *pa = cJSON_GetObjectItem(r, "packages");
		int na = cJSON_GetArraySize(pa);
		cJSON **v = na > 0 ? malloc(sizeof(*v) * (size_t)na) : NULL;
		for (int i = 0; v && i < na; i++) v[i] = cJSON_GetArrayItem(pa, i);
		for (int i = 1; v && i < na; i++)                /* (a handful: insertion sort, newest first) */
			for (int j = i; j > 0; j--) {
				const char *a1 = api_str(v[j - 1], "version"), *b1 = api_str(v[j], "version");
				if (ver_cmp(a1 ? a1 : "", b1 ? b1 : "") >= 0) break;
				cJSON *t = v[j]; v[j] = v[j - 1]; v[j - 1] = t;
			}
		for (int i = 0; v && i < na && up.npk < 8; i++) {
			snprintf(up.pk_file[up.npk], sizeof(up.pk_file[0]), "%s", api_str(v[i], "file") ? api_str(v[i], "file") : "");
			snprintf(up.pk_ver[up.npk++], sizeof(up.pk_ver[0]), "%s", api_str(v[i], "version") ? api_str(v[i], "version") : "");
		}
		free(v);
	}
	cJSON_ArrayForEach(x, cJSON_GetObjectItem(r, "rollbacks")) if (up.nrb < 6) {
		const char *v = api_str(x, "version");
		if (v && !strcmp(v, up.ver)) continue;          /* (a copy of what's installed) */
		snprintf(up.rb_ver[up.nrb], sizeof(up.rb_ver[0]), "%s", v ? v : "");
		snprintf(up.rb_date[up.nrb++], sizeof(up.rb_date[0]), "%s", api_str(x, "date") ? api_str(x, "date") : "");
	}
	up.old_files = (int)api_num(r, "old_files", 0);
	up.old_kb = api_num(r, "old_kb", 0);
	cJSON *st = cJSON_GetObjectItem(r, "state");
	up.busy = st && api_str(st, "state") && !strcmp(api_str(st, "state"), "working");
	snprintf(up.msg, sizeof(up.msg), "%s", st && api_str(st, "msg") ? api_str(st, "msg") : "");
	cJSON_Delete(r);
	char h[160];
	snprintf(h, sizeof(h), "Installed: Phosphor %s", up.ver);
	if (up.head) lv_label_set_text(up.head, h);
	if (up.note) lv_label_set_text(up.note, up.busy ? up.msg : !strcmp(upo.state, "working") || !strcmp(upo.state, "failed") ? upo.msg :
		"Newer versions come from Phosphor's releases on GitHub (looked at every 30 min while no game runs, or right now with Check GitHub for updates, at the top). "
		"A release package (phosphor-<version>-armhf.tar.gz) put in phosphor/updates over the network share, sent from the Phosphor app or on a USB drive works too. "
		"The newest 3 packages are kept after an update.");
	up_recount();                                     /* (Check GitHub for updates always first) */
}

/* Check GitHub for updates: phosphord asks GitHub in the background. Its answer is read every 2 s until
 * it's one from after the press (checked), for up to 20 s: an older answer, or none, is never taken for
 * "up to date" */
static uint32_t up_checking;
static long up_check_t0;
static int up_check_tries;
static void up_check_done(lv_timer_t *t)
{
	cJSON *r = api_call("{\"cmd\":\"update\"}");
	int ok = api_ok(r);
	if (ok) { if (cur == SCR_PUPD) up_apply(r); else { upo_apply(r); cJSON_Delete(r); } } else cJSON_Delete(r);
	int fresh = ok && upo.checked >= up_check_t0;
	if (!fresh && ++up_check_tries < 10) return;      /* (the timer asks again in 2 s) */
	lv_timer_delete(t);
	up_checking = 0;
	if (cur != SCR_PUPD) return;
	if (fresh && upo.newer && upo_row() && up_list.sel == 0) up_list.sel = 1 + (up.old_files > 0);   /* onto its Download row (under Remove old update files when that shows) */
	up_recount();
	if (!ok) toast_show("No answer from Phosphor's service: try again");
	else if (!fresh) toast_show("GitHub didn't answer in time: try again");
	else if (upo.err[0]) toast_show(upo.err);
	else toast_show(upo.newer ? "A newer Phosphor is on GitHub" : "Phosphor is up to date");
}

static void up_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	int sure = up.confirm_row == i && lv_tick_get() - up.confirm_at < 4000;
	if (i == 0) {                                     /* always on top */
		snprintf(t, n, "Check GitHub for updates");
		snprintf(val, vn, "%s", up_checking ? "Asking GitHub..." : upo.newer ? "Newer one found" : upo.latest[0] ? "Up to date" : "GitHub");
		*dim = up_checking != 0;
		return;
	}
	i--;
	if (up.old_files > 0) {                           /* (right under it) the packages earlier updates left */
		if (i == 0) {
			snprintf(t, n, "Remove old update files");
			if (sure) snprintf(val, vn, "Press A again to remove");
			else snprintf(val, vn, "%d file%s, %.0f MB", up.old_files, up.old_files == 1 ? "" : "s", up.old_kb / 1024 < 1 ? 1 : up.old_kb / 1024);
			return;
		}
		i--;
	}
	if (upo_row()) {
		if (i == 0) {
			int dl = !strcmp(upo.state, "working"), ready = upo.downloaded;
			snprintf(t, n, "%s Phosphor %s", ready ? "Install" : "Download", upo.latest);
			snprintf(val, vn, "%s", ready ? "Getting it ready..." : dl ? "Downloading..." : "From GitHub");
			*dim = dl || ready;
			return;
		}
		i--;
	}
	if (i < up.npk) {
		const char *f = strrchr(up.pk_file[i], '/');
		char where[64];
		snprintf(where, sizeof(where), "%s", strstr(up.pk_file[i], "/media/usb") ? "USB drive" : strstr(up.pk_file[i], "/phosphor/updates/") ? "phosphor/updates" : "SD card");
		snprintf(t, n, "Install Phosphor %s", up.pk_ver[i]);
		snprintf(val, vn, "%s", sure ? "Press A again to install" : where);
		(void)f;
		return;
	}
	i -= up.npk;
	snprintf(t, n, "Roll back to Phosphor %s", up.rb_ver[i]);
	snprintf(val, vn, "%s", sure ? "Press A again to roll back" : up.rb_date[i]);
}

static const char *const UP_HINTS[] = { "A", "Choose", "B", "Back", NULL };

static void show_update(void)
{
	cur = SCR_PUPD;
	lv_obj_clean(body);
	screen_title("SETTINGS", "Update Phosphor");
	up.head = mk_label(body, FONT_BODY, PAL.text, "");
	lv_obj_set_pos(up.head, S(56), S(178));
	vlist_create(&up_list, body, S(56), S(214), SW(900), S(320));
	up_list.fill = up_fill;
	up_list.sel = up_list.top = 0;
	up_list.count = 0;
	up.confirm_row = -1;
	up.note = mk_label(body, FONT_SMALL, PAL.dim, "");
	LR_HIDE(up.note);
	lv_obj_set_width(up.note, SW(900));
	lv_label_set_long_mode(up.note, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_pos(up.note, S(56), S(560));
	footer_set(UP_HINTS);
	up_apply(api_call("{\"cmd\":\"update\"}"));
}

static void up_key(pkey_t k)
{
	if (vlist_key(&up_list, k)) { up.confirm_row = -1; return; }
	if (k == K_B || k == K_MENU) { back_to_settings(SET_PHOSUPD); return; }
	if (k != K_A || up.busy) return;
	int s = up_list.sel;
	if (s == 0) {                                     /* Check GitHub for updates (always on top) */
		if (up_checking) return;
		cJSON *r = api_call("{\"cmd\":\"update\",\"do\":\"check\"}");
		if (!api_ok(r)) { toast_show(api_str(r, "error") ? api_str(r, "error") : "Couldn't ask GitHub"); cJSON_Delete(r); return; }
		cJSON_Delete(r);
		up_checking = lv_tick_get() | 1;
		up_check_t0 = (long)time(NULL);
		up_check_tries = 0;
		up.confirm_row = -1;
		toast_show("Asking GitHub...");
		vlist_refresh(&up_list);
		lv_timer_create(up_check_done, 2000, NULL);  /* (deletes itself when it has the answer) */
		return;
	}
	s--;
	if (up.old_files > 0) {
		if (s == 0) {                               /* Remove old update files: not an update, nothing restarts */
			if (up.confirm_row != up_list.sel || lv_tick_get() - up.confirm_at > 4000) { up.confirm_row = up_list.sel; up.confirm_at = lv_tick_get(); vlist_refresh(&up_list); return; }
			up.confirm_row = -1;
			cJSON *r = api_call_slow(30000, "{\"cmd\":\"update\",\"do\":\"clean\"}");
			int ok = api_ok(r);
			toast_show(ok ? "Old update files removed" : r && api_str(r, "error") ? api_str(r, "error") : "Couldn't remove them");
			if (ok) up_apply(r); else cJSON_Delete(r);
			return;
		}
		s--;
	}
	if (upo_row()) {
		if (s == 0) {                               /* the newer release, into phosphor/updates (checked) */
			if (!strcmp(upo.state, "working") || upo.downloaded) return;
			cJSON *r = api_call("{\"cmd\":\"update\",\"do\":\"download\"}");
			int started = api_ok(r);
			toast_show(started ? "Downloading..." : api_str(r, "error") ? api_str(r, "error") : "The download didn't start");
			cJSON_Delete(r);
			if (started) snprintf(upo.state, sizeof(upo.state), "working");
			up.confirm_row = -1;
			vlist_refresh(&up_list);
			return;
		}
		s--;
	}
	if (up.confirm_row != up_list.sel || lv_tick_get() - up.confirm_at > 4000) { up.confirm_row = up_list.sel; up.confirm_at = lv_tick_get(); vlist_refresh(&up_list); return; }
	up.confirm_row = -1;
	/* busy from here: a second press never starts a second update (the updater turns one down too) */
	up.busy = 1;
	cJSON *r = s < up.npk ? api_call_slow(30000, "{\"cmd\":\"update\",\"do\":\"install\",\"file\":\"%s\"}", up.pk_file[s])
	                      : api_call_slow(30000, "{\"cmd\":\"update\",\"do\":\"rollback\",\"version\":\"%s\"}", up.rb_ver[s - up.npk]);
	if (!r) { toast_show("Updating..."); return; }    /* no answer in time: it may well have started (the screen follows its state) */
	if (!api_ok(r)) { up.busy = 0; toast_show(api_str(r, "error") ? api_str(r, "error") : "The updater didn't start"); cJSON_Delete(r); return; }
	cJSON_Delete(r);
	lv_label_set_text(up.note, "Updating: Phosphor closes, then the SuperStation restarts by itself to finish.");
	toast_show("Updating...");
}

static void power_run(int restart);
static void pm_close(void);
static void saver_hide(void);
static void update_restart(lv_timer_t *t) { (void)t; if (!getenv("PHOS_NO_REBOOT")) power_run(1); }

/* ---------- Updating: the screen while Phosphor updates itself (or rolls back) ----------
 * The updater (update.sh) keeps Phosphor's screen up when it knows the screen can show this (the
 * note written at start-up): it only stops phosphord, and writes each step to <tmp>/update-state,
 * read here straight from the file (phosphord is away meanwhile). Done: the SuperStation restarts.
 * Failed: what went wrong, and back to Phosphor (nothing was changed or it was put back). */
static struct {
	int shown, failed, restarting;
	char state[16], step[16], version[32], from[32], job[16], msg[200];
	int n, total;
	long at;
	lv_obj_t *box, *title, *sub, *bar, *fill, *rows[5], *rk[5], *rv[5], *hint;
} us;

static const char *upd_tmp(void) { const char *t = getenv("PHOS_TMP"); return t && *t ? t : "/tmp/phosphor"; }

static int upd_read(void)
{
	char p[300], line[300];
	snprintf(p, sizeof(p), "%s/update-state", upd_tmp());
	FILE *f = fopen(p, "r");
	if (!f) return 0;
	us.state[0] = us.step[0] = us.version[0] = us.from[0] = us.job[0] = us.msg[0] = 0;
	us.n = us.total = 0;
	us.at = 0;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = 0;
		const char *k = line, *v = eq + 1;
		if (!strcmp(k, "state")) snprintf(us.state, sizeof(us.state), "%s", v);
		else if (!strcmp(k, "step")) snprintf(us.step, sizeof(us.step), "%s", v);
		else if (!strcmp(k, "version")) snprintf(us.version, sizeof(us.version), "%s", v);
		else if (!strcmp(k, "from")) snprintf(us.from, sizeof(us.from), "%s", v);
		else if (!strcmp(k, "job")) snprintf(us.job, sizeof(us.job), "%s", v);
		else if (!strcmp(k, "msg")) snprintf(us.msg, sizeof(us.msg), "%s", v);
		else if (!strcmp(k, "n")) us.n = atoi(v);
		else if (!strcmp(k, "total")) us.total = atoi(v);
		else if (!strcmp(k, "at")) us.at = atol(v);
	}
	fclose(f);
	return us.state[0] != 0;
}

static void upd_build(void)
{
	us.box = lv_obj_create(lv_layer_top());
	lv_obj_remove_style_all(us.box);
	lv_obj_set_size(us.box, W, H);
	lv_obj_set_style_bg_color(us.box, PAL.bg, 0);
	lv_obj_set_style_bg_opa(us.box, LV_OPA_COVER, 0);
	texture_apply(us.box, (texture_t)SETTINGS.texture);
	lv_obj_t *word = mk_label(us.box, FONT_WORD, PAL.acc, "PHOSPHOR");
	lv_obj_set_style_text_letter_space(word, S(3), 0);
	lv_obj_set_pos(word, S(56), S(38));
	lv_obj_t *card = mk_box(us.box, S(560), LV_SIZE_CONTENT, PAL.panel, S(18));
	lv_obj_set_style_border_width(card, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(card, PAL.line, 0);
	lv_obj_set_style_pad_all(card, S(32), 0);
	lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(card, S(8), 0);
	lv_obj_center(card);
	lv_obj_t *ic = icon_create(card, ICON_RESTART, S(52));
	lv_obj_set_style_text_color(ic, PAL.acc, 0);
	us.title = mk_label(card, FONT_HEADING, PAL.text, "");
	us.sub = mk_label(card, FONT_BODY, PAL.dim, "");
	lv_obj_set_width(us.sub, S(496));
	lv_label_set_long_mode(us.sub, LV_LABEL_LONG_WRAP);
	us.bar = mk_box(card, S(496), S(8), PAL.line, S(4));
	lv_obj_set_style_margin_top(us.bar, S(14), 0);
	lv_obj_set_style_margin_bottom(us.bar, S(10), 0);
	us.fill = mk_box(us.bar, S(8), S(8), PAL.acc, S(4));
	for (int i = 0; i < 5; i++) {
		us.rows[i] = mk_box(card, S(496), S(30), PAL.panel, 0);
		lv_obj_set_style_bg_opa(us.rows[i], LV_OPA_TRANSP, 0);
		us.rk[i] = mk_label(us.rows[i], FONT_NAME, PAL.text, "");
		lv_obj_align(us.rk[i], LV_ALIGN_LEFT_MID, 0, 0);
		us.rv[i] = mk_label(us.rows[i], FONT_NAME, PAL.dim, "");
		lv_obj_align(us.rv[i], LV_ALIGN_RIGHT_MID, 0, 0);
	}
	us.hint = mk_label(card, FONT_BODY, PAL.acc, "");
	lv_obj_set_style_margin_top(us.hint, S(10), 0);
	lv_obj_set_hidden(us.hint, true);
}

static void upd_draw(void)
{
	static const char *const STEPS[5] = { "check", "backup", "install", "modules", "restart" };
	int rb = !strcmp(us.job, "rollback"), cur_i = 0, done = !strcmp(us.state, "done");
	for (int i = 0; i < 5; i++) if (!strcmp(us.step, STEPS[i])) cur_i = i;
	if (done) cur_i = 4;
	char t[200];
	if (us.failed) snprintf(t, sizeof(t), "%s didn't finish", rb ? "Going back" : "The update");
	else if (done) snprintf(t, sizeof(t), "%s", us.msg[0] ? us.msg : "Updated");
	else snprintf(t, sizeof(t), rb ? "Going back to Phosphor %s" : "Updating Phosphor", us.version);
	lv_label_set_text(us.title, t);
	if (us.failed) snprintf(t, sizeof(t), "%s", us.msg);
	else if (us.from[0] && us.version[0])
		snprintf(t, sizeof(t), "%s \xE2\x86\x92 %s. Your library, box art and settings stay as they are. Don't switch off: the SuperStation restarts by itself when it's done.", us.from, us.version);
	else snprintf(t, sizeof(t), "Your library, box art and settings stay as they are. Don't switch off: the SuperStation restarts by itself when it's done.");
	lv_label_set_text(us.sub, t);
	/* the bar: each step's share, the modules counted */
	static const int AT[5] = { 3, 12, 30, 60, 96 };
	int pct = done ? 100 : AT[cur_i];
	if (!done && cur_i == 3 && us.total > 0) pct = 60 + 34 * us.n / us.total;
	lv_obj_set_width(us.fill, LV_MAX(S(8), S(496) * pct / 100));
	lv_obj_set_hidden(us.bar, us.failed);
	char k[5][80];
	snprintf(k[0], 80, "Checking the package");
	snprintf(k[1], 80, "Keeping a copy of %s", us.from[0] ? us.from : "this version");
	snprintf(k[2], 80, rb ? "Putting back Phosphor %s" : "Installing Phosphor %s", us.version);
	snprintf(k[3], 80, "Updating the modules");
	snprintf(k[4], 80, "Restarting the SuperStation");
	for (int i = 0; i < 5; i++) {
		lv_obj_set_hidden(us.rows[i], (rb && (i == 0 || i == 3)));
		lv_label_set_text(us.rk[i], k[i]);
		char v[40];
		lv_color_t c;
		if (i < cur_i || (done && i < 4)) { snprintf(v, sizeof(v), "Done"); c = lv_color_hex(0x5cf08a); }
		else if (i == cur_i && us.failed) { snprintf(v, sizeof(v), "Stopped"); c = lv_color_hex(0xff6b5e); }
		else if (i == cur_i) {
			if (i == 3 && us.total > 0) snprintf(v, sizeof(v), "%d of %d", us.n < us.total ? us.n + 1 : us.total, us.total);
			else snprintf(v, sizeof(v), i == 4 ? "Now" : "Working");
			c = PAL.acc;
		} else { snprintf(v, sizeof(v), us.failed ? "" : "Next"); c = PAL.faint; }
		lv_label_set_text(us.rv[i], v);
		lv_obj_set_style_text_color(us.rv[i], c, 0);
	}
	lv_label_set_text(us.hint, "A  Back to Phosphor");
	lv_obj_set_hidden(us.hint, !us.failed);
}

static void upd_restart_now(lv_timer_t *t)
{
	(void)t;
	if (us.box) { lv_obj_delete(us.box); us.box = NULL; }
	us.shown = 0;
	api_reset();                                     /* phosphord was restarted by the update */
	if (!getenv("PHOS_NO_REBOOT")) power_run(1);
}

/* every half second: an update running (from the TV, the app or the Scripts menu) takes the screen */
static void upd_poll(void)
{
	if (us.restarting || !upd_read()) return;
	long age = (long)time(NULL) - us.at;
	if (!us.shown) {
		if (strcmp(us.state, "working") || age > 900) return;
		us.shown = 1;
		us.failed = 0;
		saver_hide();
		pm_close();
		upd_build();
		plog("update: the Updating screen (%s %s)", us.job, us.version);
	}
	if (!strcmp(us.state, "failed")) us.failed = 1;
	upd_draw();
	if (!strcmp(us.state, "done") && !us.restarting) {
		us.restarting = 1;
		/* (once: start-up's check won't restart again) */
		char mk[300];
		snprintf(mk, sizeof(mk), "%s/update-restarted", upd_tmp());
		FILE *f = fopen(mk, "w");
		if (f) fclose(f);
		lv_timer_t *tm = lv_timer_create(upd_restart_now, 3000, NULL);
		lv_timer_set_repeat_count(tm, 1);
	}
}

/* the Updating screen's part of a key press: 1 when it ate it */
static int upd_key(pkey_t k)
{
	if (!us.shown) return 0;
	if (us.failed && k == K_A) {
		lv_obj_delete(us.box);
		us.box = NULL;
		us.shown = us.failed = 0;
		/* (phosphord is started again by the next request) */
		toast_show("Back to Phosphor");
	}
	return 1;
}

/* tells the updater this screen can stay up during an update (see upd_poll) */
static void upd_note(void)
{
	char p[300];
	snprintf(p, sizeof(p), "%s/shell-update-screen", upd_tmp());
	FILE *f = fopen(p, "w");
	if (f) { fprintf(f, "%d\n", (int)getpid()); fclose(f); }
}

/* after Phosphor started again: what the update did (once) */
static void update_check(void)
{
	cJSON *r = api_call("{\"cmd\":\"update\"}");
	cJSON *st = r ? cJSON_GetObjectItem(r, "state") : NULL;
	const char *s = st ? api_str(st, "state") : NULL;
	if (s && (!strcmp(s, "done") || !strcmp(s, "failed")) && time(NULL) - (time_t)api_num(st, "at", 0) < 300) {
		/* a finished update (or roll back) ends with a full restart, so MiSTer's Main, the drivers and
		   every module start fresh with it: once, marked in tmp (cleared by the restart itself) */
		char mk[300];
		const char *t = getenv("PHOS_TMP");
		snprintf(mk, sizeof(mk), "%s/update-restarted", t && *t ? t : "/tmp/phosphor");
		if (!strcmp(s, "done") && access(mk, F_OK) != 0) {
			FILE *f = fopen(mk, "w");
			if (f) fclose(f);
			char m[200];
			snprintf(m, sizeof(m), "%s. Restarting the SuperStation to finish...", api_str(st, "msg") ? api_str(st, "msg") : "Updated");
			toast_show(m);
			lv_timer_t *tm = lv_timer_create(update_restart, 4000, NULL);
			lv_timer_set_repeat_count(tm, 1);
		} else toast_show(api_str(st, "msg") ? api_str(st, "msg") : "Updated");
	}
	cJSON_Delete(r);
}

/* ---------- About this system ----------
 * Versions (Phosphor, its service, MiSTer's Linux, which Main), storage with free space, network
 * addresses and uptime: what support needs, on one page. */

static vlist_t ab_list;
static char ab_t[32][64], ab_v[32][96];
static int ab_n;

static void ab_add(const char *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void ab_add(const char *t, const char *fmt, ...)
{
	if (ab_n >= 32) return;
	snprintf(ab_t[ab_n], sizeof(ab_t[0]), "%s", t);
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(ab_v[ab_n], sizeof(ab_v[0]), fmt, ap);
	va_end(ap);
	ab_n++;
}

static void ab_size(char *out, size_t n, unsigned long long b)
{
	if (b >= 1000ull * 1000 * 1000 * 1000) snprintf(out, n, "%.1f TB", b / 1e12);
	else if (b >= 1000ull * 1000 * 1000) snprintf(out, n, "%.1f GB", b / 1e9);
	else snprintf(out, n, "%llu MB", b / 1000000ull);
}

static void ab_disk(const char *label, const char *path)
{
	struct statvfs v;
	if (statvfs(path, &v)) return;
	char fr[24], tot[24];
	ab_size(fr, sizeof(fr), (unsigned long long)v.f_bavail * v.f_frsize);
	ab_size(tot, sizeof(tot), (unsigned long long)v.f_blocks * v.f_frsize);
	ab_add(label, "%s free of %s", fr, tot);
}

static void ab_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	(void)dim;
	snprintf(t, n, "%s", ab_t[i]);
	snprintf(val, vn, "%s", ab_v[i]);
}

static void show_about(void)
{
	cur = SCR_ABOUT;
	lv_obj_clean(body);
	screen_title("SETTINGS", "About this system");
	ab_n = 0;
	ab_add("Phosphor", "%s", PHOS_VERSION);
	cJSON *r = api_call("{\"cmd\":\"status\"}");
	ab_add("Library service", "%s", api_ok(r) && api_str(r, "version") ? api_str(r, "version") : "not answering");
	if (api_ok(r)) ab_add("Games", "%d", (int)api_num(r, "games", 0));
	cJSON_Delete(r);
	ab_add("MiSTer's Main", "%s", mainlink_connected() ? "Phosphor's (with its API)" : "MiSTer's own");
	r = api_call("{\"cmd\":\"ports\"}");
	cJSON *pp = api_ok(r) ? cJSON_GetObjectItem(r, "ports") : NULL;
	ab_add("Menu core", "%s", cJSON_IsTrue(cJSON_GetObjectItem(pp, "available")) ? "Phosphor's (front ports)" : "MiSTer's own");
	cJSON_Delete(r);
	char ver[32] = "";
	FILE *f = fopen("/MiSTer.version", "r");
	if (f) { if (!fgets(ver, sizeof(ver), f)) ver[0] = 0; fclose(f); ver[strcspn(ver, "\r\n")] = 0; }
	ab_add("MiSTer Linux", "%s", ver[0] ? ver : "unknown");
	struct utsname u;
	if (!uname(&u)) ab_add("Kernel", "%s", u.release);

	const char *fat = getenv("PHOS_FAT");
	ab_disk("SD card", fat && *fat ? fat : "/media/fat");
	f = fopen("/proc/mounts", "r");
	if (f) {
		char line[512], dev[160], mnt[200];
		while (fgets(line, sizeof(line), f)) {
			if (sscanf(line, "%159s %199s", dev, mnt) != 2 || strncmp(mnt, "/media/", 7) || !strcmp(mnt, "/media/fat")) continue;
			char lab[64];
			if (!strncmp(mnt, "/media/usb", 10)) snprintf(lab, sizeof(lab), "%s %.40s", strstr(dev, "nvme") ? "NVMe" : "USB drive", mnt + 10);
			else if (strstr(mnt, "cifs")) snprintf(lab, sizeof(lab), "Network share");
			else snprintf(lab, sizeof(lab), "%.60s", mnt + 7);
			ab_disk(lab, mnt);
		}
		fclose(f);
	}
	r = api_call("{\"cmd\":\"remote\"}");
	cJSON *a = api_ok(r) ? cJSON_GetObjectItem(r, "addresses") : NULL, *it;
	char ips[96] = "";
	cJSON_ArrayForEach(it, a) if (cJSON_IsString(it)) {
		if (ips[0]) strncat(ips, ", ", sizeof(ips) - strlen(ips) - 1);
		strncat(ips, it->valuestring, sizeof(ips) - strlen(ips) - 1);
	}
	ab_add("Network", "%s", ips[0] ? ips : "not connected");
	cJSON_Delete(r);
	double up = 0;
	f = fopen("/proc/uptime", "r");
	if (f) { if (fscanf(f, "%lf", &up) != 1) up = 0; fclose(f); }
	long m = (long)up / 60;
	if (m >= 60 * 24) ab_add("Up for", "%ld d %ld h", m / 1440, m / 60 % 24);
	else ab_add("Up for", "%ld h %ld m", m / 60, m % 60);
	/* thanks: the hardware and the work Phosphor is built on */
	ab_add("The SuperStation One", "Taki and Retro Remake: thank you for such excellent hardware");
	ab_add("Disc support", "Anime0t4ku and bonna79 (Main_MiSTer_Physical_Disc)");
	ab_add("RetroAchievements", "odelot (RA Main and cores); rcheevos by the RA team");
	ab_add("Music CD names", "MusicBrainz and the Cover Art Archive");
	ab_add("Music decoding", "dr_libs by David Reid, stb_vorbis by Sean Barrett");
	ab_add("Licence", "PolyForm Noncommercial 1.0.0: phosphor/LICENSE.md");
	ab_add("Open-source software", "LVGL, SQLite, musl and more: phosphor/THIRD_PARTY_NOTICES.md");
	ab_add("Main and menu core", "GPL-3.0; their source: phosphor/SOURCE.md");

	vlist_create(&ab_list, body, S(56), S(176), SW(900), S(460));
	ab_list.fill = ab_fill;
	ab_list.sel = 0;
	ab_list.top = 0;
	ab_list.count = ab_n;
	vlist_refresh(&ab_list);
	static const char *const hints[] = { "B", "Back", NULL };
	footer_set(hints);
}

static void about_key(pkey_t k)
{
	if (vlist_key(&ab_list, k)) return;
	if (k == K_B || k == K_MENU) back_to_settings(SET_ABOUT);
}

/* ---------- Welcome (first start) ----------
 * Shown once on a new install (no frontend.ini yet): network, cores, accent colour, the phone
 * app and cloud saves, each optional; "Start" opens the library. Settings has all of these later too. */

enum { WL_NET, WL_CORES, WL_ACCENT, WL_PHONE, WL_CLOUD, WL_START, WL_COUNT };
static vlist_t wl_list;
static char wl_net[120];

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
	cores_refresh();
}

static void wl_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	switch (i) {
	case WL_NET: snprintf(t, n, "Network"); snprintf(val, vn, "%s  >", wl_net); *dim = 0; break;
	case WL_CORES: snprintf(t, n, "Get cores and filters (MiSTer's downloader)"); cores_value(val, vn); break;
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
	vlist_create(&wl_list, body, S(56), S(224), SW(900), S(380));
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
	case WL_CORES: if (k == K_A) { cores_start(strcmp(wl_net, "Not connected") != 0); vlist_refresh(&wl_list); } break;
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

/* ---------- navigation ---------- */

/* ---------- Looking for games ----------
 * "Look for games" takes over the screen until it's done (nothing else runs meanwhile): the step,
 * the system and file it's on, a bar, and the systems list. B twice stops it (what's identified
 * stays; the next run carries on). When it's done: "Found N games", then the library. */
static struct {
	lv_obj_t *step, *sys, *file, *bar, *count, *eta, *list;
	int started, seen_busy, quiet, gen0, b_asked_ms, done_at, stop_sent;
	uint32_t t0, b_asked;
	int last_done;
} sc;
static const char *const SCAN_HINTS[] = { "B", "twice: Stop", NULL };
static const char *const SCAN_DONE_HINTS[] = { NULL };

static void scan_row(lv_obj_t *list, int y, const char *mark, const char *name, const char *val, int state)
{
	/* state: 2 done, 1 current, 0 waiting */
	if (state == 1) {
		lv_obj_t *hl = mk_box(list, S(384), S(34), PAL.panel_on, S(6));
		lv_obj_set_pos(hl, 0, y - S(5));
		lv_obj_t *edge = mk_box(hl, S(3), S(26), PAL.acc, 0);
		lv_obj_set_pos(edge, 0, S(4));
	}
	lv_color_t c = state ? PAL.text : PAL.faint;
	lv_obj_t *m = mk_label(list, FONT_TITLE, state ? PAL.acc : PAL.faint, mark);
	lv_obj_set_pos(m, S(12), y - S(2));
	lv_obj_t *n = mk_label(list, FONT_TITLE, c, name);
	lv_obj_set_pos(n, S(42), y - S(2));
	lv_obj_t *v = mk_label(list, FONT_BODY, state == 1 ? PAL.acc : state ? PAL.dim : PAL.faint, val);
	lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -S(14), y);
}

static void scan_update(void)
{
	cJSON *r = api_call("{\"cmd\":\"scan_progress\"}");
	if (!r) return;
	const char *stage = api_str(r, "stage");
	int listing = stage && !strcmp(stage, "listing"), ident = stage && !strcmp(stage, "identifying");
	int done = (int)api_num(r, "done", 0), total = (int)api_num(r, "total", 0), listed = (int)api_num(r, "listed", 0);
	const char *cur_sys = api_str(r, "system"), *file = api_str(r, "file");
	char t[200];
	if (listing || ident) sc.seen_busy = 1;
	if (listing) {
		lv_label_set_text(sc.step, "STEP 1 OF 2  \xC2\xB7  LISTING THE GAME FILES");
		lv_label_set_text(sc.sys, cur_sys && *cur_sys ? cur_sys : "Looking...");
		snprintf(t, sizeof(t), "%d files so far", listed);
		lv_label_set_text(sc.file, t);
		lv_bar_set_value(sc.bar, 0, LV_ANIM_OFF);
		lv_label_set_text(sc.count, "");
		lv_label_set_text(sc.eta, "");
	} else if (ident) {
		lv_label_set_text(sc.step, "STEP 2 OF 2  \xC2\xB7  WORKING OUT WHAT EACH GAME IS");
		lv_label_set_text(sc.sys, cur_sys && *cur_sys ? cur_sys : "");
		lv_label_set_text(sc.file, file ? file : "");
		int pct = total ? done * 100 / total : 0;
		lv_bar_set_value(sc.bar, pct, LV_ANIM_OFF);
		snprintf(t, sizeof(t), "%d of %d games", done, total);
		lv_label_set_text(sc.count, t);
		uint32_t el = (lv_tick_get() - sc.t0) / 1000;
		if (done >= 5 && done < total) {
			uint32_t left = (uint32_t)((uint64_t)el * (uint64_t)(total - done) / (uint64_t)done);
			if (left < 60) snprintf(t, sizeof(t), "%d%%  \xC2\xB7  less than a minute left", pct);
			else snprintf(t, sizeof(t), "%d%%  \xC2\xB7  about %u minute%s left", pct, (left + 30) / 60, (left + 30) / 60 == 1 ? "" : "s");
		} else snprintf(t, sizeof(t), "%d%%", pct);
		lv_label_set_text(sc.eta, t);
	}
	/* the systems: done, the one it's on, waiting (a window of 9 around the current one) */
	lv_obj_clean(sc.list);
	cJSON *arr = cJSON_GetObjectItem(r, "systems"), *e;
	int n = cJSON_GetArraySize(arr), curi = -1, i = 0;
	cJSON_ArrayForEach(e, arr) { const char *nm = api_str(e, "name"); if (cur_sys && nm && !strcmp(nm, cur_sys)) curi = i; i++; }
	int from = curi > 4 ? curi - 4 : 0;
	if (from + 9 > n) from = n > 9 ? n - 9 : 0;
	i = 0;
	int y = 0;
	cJSON_ArrayForEach(e, arr) {
		if (i >= from && i < from + 9) {
			const char *nm = api_str(e, "name");
			int f = (int)api_num(e, "files", 0), id = (int)api_num(e, "identified", 0);
			char v[48];
			int st;
			if (i == curi) { st = 1; if (ident) snprintf(v, sizeof(v), "%d of %d", id, f); else snprintf(v, sizeof(v), "%d", f); }
			else if (listing || id >= f) { st = 2; snprintf(v, sizeof(v), "%d", f); }
			else { st = 0; snprintf(v, sizeof(v), "waiting"); }
			scan_row(sc.list, y, st == 2 ? "\xE2\x9C\x93" : st == 1 ? "\xE2\x80\xBA" : "\xC2\xB7", nm ? nm : "", v, st);
			y += S(40);
		}
		i++;
	}
	/* finished: quiet twice in a row after it was seen working (or never got going in 20 s) */
	if (!listing && !ident && !(stage && !strcmp(stage, "waiting"))) {
		sc.quiet++;
		if ((sc.seen_busy && sc.quiet >= 2) || sc.quiet >= 20) {
			int games = (int)api_num(r, "games", 0);
			snprintf(t, sizeof(t), sc.stop_sent ? "Stopped: %d games so far" : "Found %d games", games);
			lv_label_set_text(sc.step, "DONE");
			lv_label_set_text(sc.sys, t);
			lv_label_set_text(sc.file, "");
			lv_bar_set_value(sc.bar, 100, LV_ANIM_OFF);
			lv_label_set_text(sc.count, "");
			lv_label_set_text(sc.eta, "");
			footer_set(SCAN_DONE_HINTS);
			sc.done_at = (int)lv_tick_get() | 1;
		}
	} else sc.quiet = 0;
	cJSON_Delete(r);
}

static void show_home(void);
static void show_scan(void)
{
	cur = SCR_SCAN;
	memset(&sc, 0, sizeof(sc));
	lv_obj_clean(body);
	screen_title("LIBRARY", "Looking for games");
	sc.step = mk_label(body, FONT_LABEL, PAL.acc, "STARTING");
	lv_obj_set_style_text_letter_space(sc.step, S(1), 0);
	lv_obj_set_pos(sc.step, S(56), S(200));
	sc.sys = mk_label(body, FONT_HEADING, PAL.text, "");
	lv_obj_set_pos(sc.sys, S(56), S(224));
	sc.file = mk_label(body, FONT_BODY, PAL.dim, "");
	lv_obj_set_width(sc.file, SW(706));
	lv_label_set_long_mode(sc.file, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_pos(sc.file, S(56), S(284));
	sc.bar = lv_bar_create(body);
	lv_obj_set_size(sc.bar, SW(706), S(16));
	lv_obj_set_pos(sc.bar, S(56), S(346));
	lv_obj_set_style_bg_color(sc.bar, PAL.line, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(sc.bar, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(sc.bar, PAL.acc, LV_PART_INDICATOR);
	lv_obj_set_style_radius(sc.bar, S(8), LV_PART_MAIN);
	lv_obj_set_style_radius(sc.bar, S(8), LV_PART_INDICATOR);
	lv_bar_set_range(sc.bar, 0, 100);
	sc.count = mk_label(body, FONT_TITLE, PAL.text, "");
	lv_obj_set_pos(sc.count, S(56), S(376));
	sc.eta = mk_label(body, FONT_BODY, PAL.dim, "");
	lv_obj_set_width(sc.eta, SW(500));
	lv_obj_set_style_text_align(sc.eta, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_pos(sc.eta, SX(56 + 706) - SW(500), S(380));
	lv_obj_t *note = mk_box(body, SW(706), NARROW ? LV_SIZE_CONTENT : S(100), PAL.bg, S(10));   /* (4:3: as tall as its text) */
	lv_obj_set_style_border_color(note, PAL.line, 0);
	lv_obj_set_style_border_width(note, S(1) > 0 ? S(1) : 1, 0);
	lv_obj_set_pos(note, S(56), S(466));
	lv_obj_t *nt = mk_label(note, FONT_NAME, PAL.text, "Everything else waits until this is done.");
	lv_obj_set_pos(nt, S(20), S(18));
	lv_obj_set_style_pad_bottom(note, S(18), 0);
	lv_obj_t *nb = mk_label(note, FONT_SMALL, PAL.dim, NARROW ? "Games found so far are kept. If it is stopped or the SS1 is turned off, Look for games carries on from where it left off next time."
	                                                 : "Games found so far are kept. If it is stopped or the SS1 is turned off,\nLook for games carries on from where it left off next time.");
	lv_label_set_long_mode(nb, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(nb, SW(706) - S(40));
	lv_obj_set_pos(nb, S(20), S(50));
	lv_obj_t *sl = mk_label(body, FONT_LABEL, PAL.dim, "SYSTEMS");
	lv_obj_set_style_text_letter_space(sl, S(2), 0);
	lv_obj_set_pos(sl, SX(840), S(166));
	sc.list = mk_box(body, SW(384), S(380), PAL.bg, 0);
	lv_obj_set_pos(sc.list, SX(830), S(200));
	if (theme_lr) {
		int w = LRW - 32;
		lr_at(sc.step, 16, 58, w); lr_at(sc.sys, 16, 68, w); lr_at(sc.file, 16, 90, w); lr_at(sc.count, 16, 114, 0);
		lr_progress(sc.bar, sc.eta, note, nt, nb, sl, sc.list, 106, 140);
	}
	footer_set(SCAN_HINTS);
	sc.t0 = lv_tick_get();
	cJSON_Delete(api_call("{\"cmd\":\"rescan\"}"));
	scan_update();
}

static void scan_key(pkey_t k)
{
	if (sc.done_at) return;                      /* "Found N games" is on: it goes to the library itself */
	if (k != K_B) return;                        /* nothing else until it's done */
	uint32_t now = lv_tick_get();
	if (!sc.b_asked || now - sc.b_asked > 4000) { sc.b_asked = now ? now : 1; toast_show("Press B again to stop"); return; }
	sc.b_asked = 0;
	sc.stop_sent = 1;
	cJSON_Delete(api_call("{\"cmd\":\"rescan\",\"stop\":true}"));
	lv_label_set_text(sc.step, "STOPPING");
}

static void scan_tick(uint32_t now)
{
	static uint32_t last;
	if (sc.done_at) { if (now - (uint32_t)sc.done_at > 2500) { sc.done_at = 0; show_home(); } return; }
	if (now - last < 500) return;
	last = now;
	scan_update();
}


/* ---------- Getting box art and details ----------
 * Like Looking for games: "Get box art and details" takes over the screen until it's done. The
 * source it's asking, the system and game it's on, a bar, this run's counts, and the systems list
 * (art found of games). B twice stops it (what's found stays; the next run carries on). */
static struct {
	lv_obj_t *step, *sys, *game, *src, *bar, *count, *eta, *tally, *list;
	int seen_running, quiet, done_at, stop_sent;
	uint32_t t0, b_asked;
} ar;
static const char *const ART_HINTS[] = { "B", "twice: Stop", NULL };

static void upper(char *s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32; }

static void art_update(void)
{
	cJSON *r = api_call("{\"cmd\":\"scrape_progress\"}");
	if (!api_ok(r)) { cJSON_Delete(r); return; }
	int running = cJSON_IsTrue(cJSON_GetObjectItem(r, "running"));
	int total = (int)api_num(r, "total", 0), done = (int)api_num(r, "done", 0);
	int found = (int)api_num(r, "found", 0), nf = (int)api_num(r, "not_found", 0), later = (int)api_num(r, "later", 0);
	const char *cur_sys = api_str(r, "system"), *game = api_str(r, "game"), *src = api_str(r, "source"), *status = api_str(r, "status");
	char t[240];
	if (running) {
		ar.seen_running = 1;
		ar.quiet = 0;
		if (src && *src) snprintf(t, sizeof(t), "ASKING %s", src);
		else snprintf(t, sizeof(t), "%s", status ? status : "STARTING");
		upper(t);
		lv_label_set_text(ar.step, t);
		lv_label_set_text(ar.sys, cur_sys && *cur_sys ? cur_sys : "");
		lv_label_set_text(ar.game, game ? game : "");
		lv_label_set_text(ar.src, src && *src ? "Looking it up..." : (status ? status : ""));
		int pct = total ? done * 100 / total : 0;
		if (pct > 100) pct = 100;
		lv_bar_set_value(ar.bar, pct, LV_ANIM_OFF);
		snprintf(t, sizeof(t), "%d of %d games", done, total);
		lv_label_set_text(ar.count, t);
		uint32_t el = (lv_tick_get() - ar.t0) / 1000;
		if (done >= 5 && done < total) {
			uint32_t left = (uint32_t)((uint64_t)el * (uint64_t)(total - done) / (uint64_t)done);
			if (left < 60) snprintf(t, sizeof(t), "%d%%  \xC2\xB7  less than a minute left", pct);
			else if (left < 5400) snprintf(t, sizeof(t), "%d%%  \xC2\xB7  about %u minute%s left", pct, (left + 30) / 60, (left + 30) / 60 == 1 ? "" : "s");
			else snprintf(t, sizeof(t), "%d%%  \xC2\xB7  about %u hours left", pct, (left + 1800) / 3600);
		} else snprintf(t, sizeof(t), "%d%%", pct);
		lv_label_set_text(ar.eta, t);
	}
	if (later) snprintf(t, sizeof(t), "Found  %d        Not found  %d        No connection  %d (tried again next time)", found, nf, later);
	else snprintf(t, sizeof(t), "Found  %d        Not found  %d", found, nf);
	lv_label_set_text(ar.tally, t);
	/* the systems: art found of games; the one it's on; waiting (a window of 9 around the current one) */
	lv_obj_clean(ar.list);
	cJSON *arr = cJSON_GetObjectItem(r, "systems"), *e;
	int n = cJSON_GetArraySize(arr), curi = -1, i = 0;
	cJSON_ArrayForEach(e, arr) { const char *nm = api_str(e, "name"); if (running && cur_sys && nm && !strcmp(nm, cur_sys)) curi = i; i++; }
	int from = curi > 4 ? curi - 4 : 0;
	if (from + 9 > n) from = n > 9 ? n - 9 : 0;
	i = 0;
	int y = 0;
	cJSON_ArrayForEach(e, arr) {
		if (i >= from && i < from + 9) {
			const char *nm = api_str(e, "name");
			int g = (int)api_num(e, "games", 0), a = (int)api_num(e, "art", 0), p = (int)api_num(e, "pending", 0);
			char v[48];
			int st = i == curi ? 1 : (!p || !running) ? 2 : 0;
			if (st) snprintf(v, sizeof(v), "%d of %d", a, g);
			else snprintf(v, sizeof(v), "waiting");
			scan_row(ar.list, y, st == 2 ? "\xE2\x9C\x93" : st == 1 ? "\xE2\x80\xBA" : "\xC2\xB7", nm ? nm : "", v, st);
			y += S(40);
		}
		i++;
	}
	/* finished: not running twice in a row after it was seen working (or never got going in 10 s) */
	if (!running) {
		ar.quiet++;
		/* (the worker starts at once when asked, so 3 quiet looks means it's finished, or had nothing to do) */
		if ((ar.seen_running && ar.quiet >= 2) || ar.quiet >= 3) {
			lv_label_set_text(ar.step, "DONE");
			if (!done && !ar.stop_sent) {
				lv_label_set_text(ar.sys, "Nothing new to look up");
				lv_label_set_text(ar.game, "Every game has been looked up. Added games? Look for games first.");
			} else {
				snprintf(t, sizeof(t), ar.stop_sent ? "Stopped: box art for %d more games" : "Found box art and details for %d games", found);
				lv_label_set_text(ar.sys, t);
				lv_label_set_text(ar.game, status ? status : "");
			}
			lv_label_set_text(ar.src, "");
			lv_bar_set_value(ar.bar, 100, LV_ANIM_OFF);
			lv_label_set_text(ar.count, "");
			lv_label_set_text(ar.eta, "");
			footer_set(SCAN_DONE_HINTS);
			ar.done_at = (int)lv_tick_get() | 1;
		}
	}
	cJSON_Delete(r);
}

static void show_art(void)
{
	cur = SCR_ART;
	memset(&ar, 0, sizeof(ar));
	lv_obj_clean(body);
	screen_title("LIBRARY", "Getting box art and details");
	ar.step = mk_label(body, FONT_LABEL, PAL.acc, "STARTING");
	lv_obj_set_style_text_letter_space(ar.step, S(1), 0);
	lv_obj_set_width(ar.step, SW(706));
	lv_label_set_long_mode(ar.step, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_pos(ar.step, S(56), S(200));
	ar.sys = mk_label(body, FONT_HEADING, PAL.text, "");
	lv_obj_set_width(ar.sys, SW(706));
	lv_label_set_long_mode(ar.sys, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_pos(ar.sys, S(56), S(224));
	ar.game = mk_label(body, FONT_TITLE, PAL.text, "");
	lv_obj_set_width(ar.game, SW(706));
	lv_label_set_long_mode(ar.game, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_pos(ar.game, S(56), S(276));
	ar.src = mk_label(body, FONT_BODY, PAL.dim, "");
	lv_obj_set_width(ar.src, SW(706));
	lv_label_set_long_mode(ar.src, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_pos(ar.src, S(56), S(306));
	ar.bar = lv_bar_create(body);
	lv_obj_set_size(ar.bar, SW(706), S(16));
	lv_obj_set_pos(ar.bar, S(56), S(346));
	lv_obj_set_style_bg_color(ar.bar, PAL.line, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(ar.bar, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_bg_color(ar.bar, PAL.acc, LV_PART_INDICATOR);
	lv_obj_set_style_radius(ar.bar, S(8), LV_PART_MAIN);
	lv_obj_set_style_radius(ar.bar, S(8), LV_PART_INDICATOR);
	lv_bar_set_range(ar.bar, 0, 100);
	ar.count = mk_label(body, FONT_TITLE, PAL.text, "");
	lv_obj_set_pos(ar.count, S(56), S(376));
	ar.eta = mk_label(body, FONT_BODY, PAL.dim, "");
	lv_obj_set_width(ar.eta, SW(500));
	lv_obj_set_style_text_align(ar.eta, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_pos(ar.eta, SX(56 + 706) - SW(500), S(380));
	ar.tally = mk_label(body, FONT_BODY, PAL.dim, "");
	lv_obj_set_pos(ar.tally, S(56), S(412));
	lv_obj_t *note = mk_box(body, SW(706), NARROW ? LV_SIZE_CONTENT : S(100), PAL.bg, S(10));   /* (4:3: as tall as its text) */
	lv_obj_set_style_border_color(note, PAL.line, 0);
	lv_obj_set_style_border_width(note, S(1) > 0 ? S(1) : 1, 0);
	lv_obj_set_pos(note, S(56), S(466));
	lv_obj_t *nt = mk_label(note, FONT_NAME, PAL.text, "Everything else waits until this is done.");
	lv_obj_set_pos(nt, S(20), S(18));
	lv_obj_set_style_pad_bottom(note, S(18), 0);
	lv_obj_t *nb = mk_label(note, FONT_SMALL, PAL.dim, NARROW ? "Box art and details found so far are kept. If it is stopped or the SS1 is turned off, it carries on from where it left off next time."
	                                                 : "Box art and details found so far are kept. If it is stopped or the SS1 is\nturned off, it carries on from where it left off next time.");
	lv_label_set_long_mode(nb, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(nb, SW(706) - S(40));
	lv_obj_set_pos(nb, S(20), S(50));
	lv_obj_t *sl = mk_label(body, FONT_LABEL, PAL.dim, "SYSTEMS  \xC2\xB7  BOX ART FOUND");
	lv_obj_set_style_text_letter_space(sl, S(2), 0);
	lv_obj_set_pos(sl, SX(840), S(166));
	ar.list = mk_box(body, SW(384), S(380), PAL.bg, 0);
	lv_obj_set_pos(ar.list, SX(830), S(200));
	if (theme_lr) {
		int w = LRW - 32;
		lr_at(ar.step, 16, 58, w); lr_at(ar.sys, 16, 68, w); lr_at(ar.game, 16, 88, w); lr_at(ar.src, 16, 103, w);
		lr_at(ar.count, 16, 126, 0); lr_at(ar.tally, 16, 142, w);
		lr_progress(ar.bar, ar.eta, note, nt, nb, sl, ar.list, 118, 160);
	}
	footer_set(ART_HINTS);
	ar.t0 = lv_tick_get();
	/* start it, unless it's already working (then this screen just follows it) */
	cJSON *r = api_call("{\"cmd\":\"scrape\"}");
	int on = r && cJSON_IsTrue(cJSON_GetObjectItem(r, "on"));
	cJSON_Delete(r);
	if (!on) cJSON_Delete(api_call("{\"cmd\":\"scrape\",\"on\":true}"));
	art_update();
}

static void art_key(pkey_t k)
{
	if (ar.done_at) return;
	if (k != K_B) return;
	uint32_t now = lv_tick_get();
	if (!ar.b_asked || now - ar.b_asked > 4000) { ar.b_asked = now ? now : 1; toast_show("Press B again to stop"); return; }
	ar.b_asked = 0;
	ar.stop_sent = 1;
	cJSON_Delete(api_call("{\"cmd\":\"scrape\",\"on\":false}"));
	lv_label_set_text(ar.step, "STOPPING");
}

static void art_tick(uint32_t now)
{
	static uint32_t last;
	if (ar.done_at) { if (now - (uint32_t)ar.done_at > 3500) { ar.done_at = 0; back_to_settings(SET_SCRAPE); } return; }
	if (now - last < 500) return;
	last = now;
	art_update();
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

/* ---------- Power: the window (Select + Start) and the safe shutdown / restart ----------
 * Select + Start together anywhere in Phosphor: Shut down safely, Restart, Reload the screen, MiSTer menu (OSD). Shut down and Restart
 * first make every drive read-only through phosphord (phosphor.sh safe-stop), which is when exFAT
 * clears its dirty flag, so Windows doesn't ask to scan the card afterwards; the screen then says how
 * each drive came out. Phosphor for Windows' Shut down / Restart land here too (phosphord leaves a
 * request in /tmp/phosphor/power-request). */

#define PM_N 4
static struct { lv_obj_t *dim, *box, *row[PM_N], *ic[PM_N], *nm[PM_N]; int sel, open; } pm;
static lv_obj_t *splash;
static int powering;                        /* a safe stop is under way: nothing else happens */

static void pm_draw(void)
{
	for (int i = 0; i < PM_N; i++) {
		int on = i == pm.sel;
		lv_obj_set_style_bg_opa(pm.row[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
		lv_obj_set_style_bg_color(pm.row[i], PAL.panel_on, 0);
		lv_obj_set_style_border_width(pm.row[i], on ? S(3) : 0, 0);
		lv_obj_set_style_text_color(pm.ic[i], on ? PAL.acc : PAL.dim, 0);
		lv_obj_set_style_text_color(pm.nm[i], on ? PAL.text : PAL.dim, 0);
	}
}

static void pm_close(void)
{
	if (!pm.open) return;
	lv_obj_delete(pm.dim);
	memset(&pm, 0, sizeof(pm));
}

static void pm_open(void)
{
	if (pm.open || powering) return;
	pm.open = 1;
	pm.sel = 0;
	pm.dim = lv_obj_create(lv_layer_top());
	lv_obj_remove_style_all(pm.dim);
	lv_obj_set_size(pm.dim, W, H);
	lv_obj_set_style_bg_color(pm.dim, lv_color_hex(0x08080a), 0);
	lv_obj_set_style_bg_opa(pm.dim, LV_OPA_80, 0);
	pm.box = mk_box(pm.dim, S(480), LV_SIZE_CONTENT, PAL.panel, S(16));
	lv_obj_set_style_border_width(pm.box, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(pm.box, PAL.line, 0);
	lv_obj_set_style_pad_all(pm.box, S(24), 0);
	lv_obj_set_flex_flow(pm.box, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(pm.box, S(4), 0);
	lv_obj_center(pm.box);
	mk_label(pm.box, FONT_TITLE, PAL.text, "Power");
	lv_obj_t *nt = mk_label(pm.box, FONT_SMALL, PAL.dim, "Saves are written as they happen, so nothing is lost.");
	lv_obj_set_style_pad_bottom(nt, S(12), 0);
	static const icon_t IC[PM_N] = { ICON_POWER, ICON_RESTART, ICON_SCREEN, ICON_GEAR };
	static const char *NM[PM_N] = { "Shut down safely", "Restart", "Reload the screen", "MiSTer menu (OSD)" };
	static const char *SUB[PM_N] = { "Switch off when the screen says so", "The SS1 starts again", "Only Phosphor's screen restarts",
	                                 "MiSTer's own menu and settings" };
	for (int i = 0; i < PM_N; i++) {
		lv_obj_t *r = mk_box(pm.box, LV_PCT(100), S(60), PAL.panel_on, S(10));
		lv_obj_set_style_border_side(r, LV_BORDER_SIDE_LEFT, 0);
		lv_obj_set_style_border_color(r, PAL.acc, 0);
		lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
		lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_style_pad_left(r, S(18), 0);
		lv_obj_set_style_pad_column(r, S(16), 0);
		pm.row[i] = r;
		pm.ic[i] = icon_create(r, IC[i], S(28));
		lv_obj_t *col = mk_box(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT, PAL.panel, 0);
		lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
		lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
		pm.nm[i] = mk_label(col, FONT_NAME, PAL.dim, NM[i]);
		mk_label(col, FONT_SMALL, PAL.faint, SUB[i]);
	}
	lv_obj_t *hn = mk_label(pm.box, FONT_SMALL, PAL.dim, "A  Choose        B  Cancel");
	lv_obj_set_style_pad_top(hn, S(14), 0);
	pm_draw();
	plog("power window");
}

/* the full-screen splash while stopping, and the answer after */
static lv_obj_t *sp_title, *sp_sub, *sp_ic, *sp_list;
static void splash_show(int restart)
{
	if (splash) lv_obj_delete(splash);
	splash = lv_obj_create(lv_layer_top());
	lv_obj_remove_style_all(splash);
	lv_obj_set_size(splash, W, H);
	lv_obj_set_style_bg_color(splash, PAL.bg, 0);
	lv_obj_set_style_bg_opa(splash, LV_OPA_COVER, 0);
	texture_apply(splash, (texture_t)SETTINGS.texture);
	lv_obj_t *word = mk_label(splash, FONT_WORD, PAL.acc, "PHOSPHOR");
	lv_obj_set_style_text_letter_space(word, S(3), 0);
	lv_obj_set_pos(word, S(56), S(38));
	lv_obj_t *card = mk_box(splash, S(560), LV_SIZE_CONTENT, PAL.panel, S(18));
	lv_obj_set_style_border_width(card, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(card, PAL.line, 0);
	lv_obj_set_style_pad_all(card, S(32), 0);
	lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
	lv_obj_set_style_pad_row(card, S(10), 0);
	lv_obj_center(card);
	sp_ic = icon_create(card, restart ? ICON_RESTART : ICON_POWER, S(56));
	lv_obj_set_style_text_color(sp_ic, PAL.acc, 0);
	sp_title = mk_label(card, FONT_HEADING, PAL.text, restart ? "Restarting" : "Shutting down");
	sp_sub = mk_label(card, FONT_BODY, PAL.dim, "Writing everything to the drives and marking them clean. Don't switch off yet.");
	lv_obj_set_width(sp_sub, S(496));
	lv_label_set_long_mode(sp_sub, LV_LABEL_LONG_WRAP);
	sp_list = mk_box(card, S(496), LV_SIZE_CONTENT, PAL.panel, 0);
	lv_obj_set_style_bg_opa(sp_list, LV_OPA_TRANSP, 0);
	lv_obj_set_flex_flow(sp_list, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_style_pad_row(sp_list, S(6), 0);
	lv_obj_set_style_pad_top(sp_list, S(8), 0);
}

static void splash_drives(cJSON *r)
{
	int bad = 0, any = 0;
	cJSON *d;
	cJSON_ArrayForEach(d, cJSON_GetObjectItem(r, "drives")) {
		const char *p = api_str(d, "path"), *st = api_str(d, "state");
		if (!p || !st) continue;
		any = 1;
		char name[64];
		if (!strcmp(p, "/media/fat")) snprintf(name, sizeof(name), "SD card");
		else snprintf(name, sizeof(name), "Drive %s", p + (strncmp(p, "/media/", 7) ? 0 : 7));
		int clean = !strcmp(st, "clean"), na = !strcmp(st, "n/a");
		if (!clean && !na) bad = 1;
		lv_obj_t *row = mk_box(sp_list, LV_PCT(100), S(30), PAL.panel, 0);
		lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
		lv_obj_t *nm = mk_label(row, FONT_NAME, PAL.text, name);
		lv_obj_align(nm, LV_ALIGN_LEFT_MID, 0, 0);
		lv_color_t c = clean ? lv_color_hex(0x5cf08a) : na ? PAL.dim : lv_color_hex(0xff6b5e);
		lv_obj_t *v = mk_label(row, FONT_NAME, c, clean ? "Clean" : na ? "Done" : !strcmp(st, "DIRTY") ? "Still marked dirty" : st);
		lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
	}
	if (!any) lv_label_set_text(sp_sub, "Everything is written to the card.");
	else if (!bad) lv_label_set_text(sp_sub, "Everything is written and the drives are marked clean: Windows won't ask to scan the card.");
	else if (bad) lv_label_set_text(sp_sub, "Everything is written, but a drive is still marked dirty (nothing is lost). Phosphor checks an NVMe or USB drive when it starts; for the SD card, repair it on a PC with Phosphor for Windows (SD card flash & health).");
}

static void power_run(int restart)
{
	if (powering) return;
	powering = 1;
	pm_close();
	plog("power: %s", restart ? "restart" : "shut down");
	splash_show(restart);
	lv_refr_now(NULL);
	cJSON *r = api_call_slow(90000, "{\"cmd\":\"system\",\"do\":\"safe_stop\"}");
	if (r && api_ok(r)) splash_drives(r);
	else lv_label_set_text(sp_sub, "Couldn't check the drives; everything was flushed.");
	cJSON_Delete(r);
	if (restart) {
		lv_label_set_text(sp_title, "Restarting");
		lv_refr_now(NULL);
		usleep(1200000);
		cJSON *rr = api_call("{\"cmd\":\"system\",\"do\":\"reboot_now\"}");
		if (!rr && !getenv("PHOS_NO_REBOOT")) {          /* phosphord not answering: restart from here */
			plog("power: phosphord didn't answer, restarting from the shell");
			sync();
			if (system("reboot") != 0 && system("reboot -f") != 0) plog("power: reboot didn't run");
		}
		cJSON_Delete(rr);
	} else {
		lv_label_set_text(sp_title, "Safe to switch off");
		lv_obj_set_style_text_color(sp_title, PAL.acc, 0);
		icon_set(sp_ic, ICON_CHECK);
		lv_refr_now(NULL);
		cJSON_Delete(api_call("{\"cmd\":\"system\",\"do\":\"halt_now\"}"));
	}
	lv_refr_now(NULL);
}

static void pm_choose(void)
{
	int s = pm.sel;
	if (s == 3) {                                    /* MiSTer's own menu, as Settings > MiSTer menu (OSD) */
		pm_close();
		if (mainlink_give()) toast_show("Back to Phosphor: hold the menu button, or System > Launch Phosphor");
		else toast_show("Needs Phosphor's Main: press F9 on a keyboard to switch");
		return;
	}
	if (s == 2) {
		pm_close();
		cJSON *r = api_call("{\"cmd\":\"system\",\"do\":\"reload_ui\"}");
		toast_show(api_ok(r) ? "Reloading the screen..." : "Couldn't reload");
		cJSON_Delete(r);
		return;
	}
	power_run(s == 1);
}

/* the power window's part of a key press: 1 when it ate it */
static int pm_key(pkey_t k)
{
	if (powering) return 1;
	if (k == K_POWER) { if (pm.open) pm_close(); else pm_open(); return 1; }
	if (!pm.open) return 0;
	if (k == K_UP) pm.sel = (pm.sel + PM_N - 1) % PM_N;
	else if (k == K_DOWN) pm.sel = (pm.sel + 1) % PM_N;
	else if (k == K_A) { pm_choose(); return 1; }
	else if (k == K_B || k == K_MENU) { pm_close(); return 1; }
	pm_draw();
	return 1;
}

/* Phosphor for Windows (or the phone) asked: phosphord left a request */
static void power_requests(void)
{
	if (powering) return;
	char p[300], what[32] = "";
	const char *t = getenv("PHOS_TMP");
	snprintf(p, sizeof(p), "%s/power-request", t && *t ? t : "/tmp/phosphor");
	FILE *f = fopen(p, "r");
	if (!f) return;
	if (!fgets(what, sizeof(what), f)) what[0] = 0;
	fclose(f);
	plog("power: asked from another device (%s)", what);
	power_run(!strncmp(what, "reboot", 6));
}

/* ---------- Screensaver ----------
 * After the idle time set in Settings the screen goes dark: only the wordmark, dim, moving to a new
 * place every 20 s so nothing stays lit in one spot. Any button wakes it (that press does nothing else). */

static lv_obj_t *saver, *saver_word;
static uint32_t last_input, saver_moved;

static void saver_move(void)
{
	lv_obj_update_layout(saver_word);
	int ww = lv_obj_get_width(saver_word), wh = lv_obj_get_height(saver_word);
	int x = (int)(lv_rand(S(40), (uint32_t)LV_MAX(S(41), W - ww - S(40))));
	int y = (int)(lv_rand(S(40), (uint32_t)LV_MAX(S(41), H - wh - S(40))));
	lv_obj_set_pos(saver_word, x, y);
}

static void saver_show(void)
{
	if (saver) return;
	saver = lv_obj_create(lv_layer_top());
	lv_obj_remove_style_all(saver);
	lv_obj_set_size(saver, W, H);
	lv_obj_set_style_bg_color(saver, lv_color_black(), 0);
	lv_obj_set_style_bg_opa(saver, LV_OPA_COVER, 0);
	saver_word = mk_label(saver, FONT_WORD, PAL.acc, "PHOSPHOR");
	lv_obj_set_style_text_letter_space(saver_word, S(3), 0);
	lv_obj_set_style_text_opa(saver_word, LV_OPA_40, 0);
	saver_move();
	lv_obj_fade_in(saver, 800, 0);
	saver_moved = lv_tick_get();
	plog("screensaver on");
}

static void saver_hide(void)
{
	if (!saver) return;
	lv_obj_delete(saver);
	saver = NULL;
	saver_word = NULL;
}

/* the screensaver's part of a key press: 1 when it ate it (woke up) */
static int saver_key(void)
{
	last_input = lv_tick_get();
	if (!saver) return 0;
	saver_hide();
	return 1;
}

static void saver_tick(uint32_t now)
{
	if (!last_input) last_input = now ? now : 1;
	/* mapping, or watching a controller: its buttons go to Main, not here, so they don't count as input */
	if (cur == SCR_MAP || cur == SCR_CTRL || cur == SCR_CTEST) { last_input = now ? now : 1; if (saver) saver_hide(); return; }
	if (saver) {
		if (now - saver_moved >= 20000) { saver_moved = now; saver_move(); }
		return;
	}
	if (SETTINGS.saver_min && now - last_input >= (uint32_t)SETTINGS.saver_min * 60000u) saver_show();
}

void ui_screensaver_now(void) { saver_show(); }   /* tests and snapshots */
void ui_activity(void) { last_input = lv_tick_get(); saver_hide(); }

/* Remote control from the Windows app: never into network, pairing or remote access settings */
static int app_key_blocked(pkey_t k)
{
	if (!input_from_app()) return 0;
	if (cur == SCR_WIFI || cur == SCR_BT || cur == SCR_REMOTE || cur == SCR_WELCOME) return k != K_B && k != K_MENU;
	if (cur == SCR_SETTINGS && (k == K_A || k == K_LEFT || k == K_RIGHT))
		return set_id(set_list.sel) == SET_WIFI || set_id(set_list.sel) == SET_BT || set_id(set_list.sel) == SET_REMOTE || set_id(set_list.sel) == SET_SAMBA;
	return 0;
}

void ui_key(pkey_t k)
{
	if (powering) return;
	if (upd_key(k)) return;
	if (app_key_blocked(k)) { toast_show("Network, Bluetooth and remote access are changed on the SuperStation itself"); return; }
	if (k == K_POWER) saver_hide();
	else if (k == K_SHOT) { if (!SETTINGS.shot_off) take_shot(); return; }    /* L3 + R3 (Main tells us) */
	else if (saver_key()) return;
	if (cur == SCR_MAP) { map_key(k); return; }       /* locked: only Select + Start (cancel) */
	if (cur == SCR_CTEST) { tw_key(k); return; }    /* locked: Select + Start closes */
	if (vk_key(k)) return;                           /* a video try waits for Keep */
	if (pm_key(k)) return;
	if (cc_key(k)) return;
	if (osk_key(k)) return;
	switch (cur) {
	case SCR_HOME:
		if (k == K_LEFT) shelf_go(home.sel - 1);
		else if (k == K_RIGHT) shelf_go(home.sel + 1);
		else if (k == K_A && home.sel == SH_MUSIC) show_music();
		else if (k == K_A && (home.sel == SH_OTHER || home.sel == SH_UTIL)) show_folder(home.sel == SH_UTIL);
		else if (k == K_A && home.sel == SH_FAV) show_list(GL_FAV);
		else if (k == K_A && home.sel == SH_RECENT) show_list(GL_RECENT);
		else if (k == K_A) show_systems((category_t)home.sel);
		else if (k == K_Y) search_open();
		else if (k == K_MENU) show_settings();
		else if (k == K_X && last_game && cJSON_IsTrue(cJSON_GetObjectItem(last_game, "disc"))) disc_play_now();
		else if (k == K_X && last_game) play_id((long long)api_num(last_game, "id", -1));
		break;
	case SCR_DISCS:
		discs_key(k);
		break;
	case SCR_RA:
		ra_key(k);
		break;
	case SCR_MUSIC:
		music_key(k);
		break;
	case SCR_SHOTS:
		shots_key(k);
		break;
	case SCR_SHOTGRID:
		shotgrid_key(k);
		break;
	case SCR_SHOTVIEW:
		shotview_key(k);
		break;
	case SCR_NOWPLAY:
		nowplay_key(k);
		break;
	case SCR_RAVIEW:
		raview_key(k);
		break;
	case SCR_INGAME:
		ingame_key(k);
		break;
	case SCR_SYSTEMS:
		if (vlist_key(&sys_list, k)) sys_detail();
		else if (nsys && sys_items[sys_list.sel].drive) {
			if (k == K_A && dk.playable) disc_play_now();
			else if (k == K_A && dk.reason[0]) toast_show(dk.reason);
			else if (k == K_X && dk.copyable) show_dcopy(1);
			else if (k == K_Y) disc_eject_now();
			else if (k == K_B) show_home();
		}
		else if (k == K_A && nsys) show_games(sys_items[sys_list.sel].id, sys_items[sys_list.sel].name, sys_cat);
		else if (k == K_B) show_home();
		break;
	case SCR_GAMES:
		if (picker_open()) { picker_key(k); break; }
		if (vlist_key(&game_list, k)) game_detail();
		else if (k == K_Y && ngames) picker_show();
		else if (k == K_X && ngames) games_toggle_fav();
		else if (k == K_A && ngames) play_id(games[game_list.sel].id);
		else if (k == K_SELECT && ngames) play_choose(games[game_list.sel].id);   /* Start with...: always ask */
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
	case SCR_MCARDS:
		mcards_key(k);
		break;
	case SCR_PADTEST:
		padtest_key(k);
		break;
	case SCR_STORAGE:
		storage_key(k);
		break;
	case SCR_PORTS:
		ports_key(k);
		break;
	case SCR_BT:
		bt_key(k);
		break;
	case SCR_ABOUT:
		about_key(k);
		break;
	case SCR_LOG:
		log_key(k);
		break;
	case SCR_SCAN:
		scan_key(k);
		break;
	case SCR_GUARD:
		guard_key(k);
		break;
	case SCR_SCRIPTS:
		scripts_key(k);
		break;
	case SCR_FOLDER:
		folder_key(k);
		break;
	case SCR_WALL:
		wall_key(k);
		break;
	case SCR_DCOPY:
		dcopy_key(k);
		break;
	case SCR_VIDEO:
		video_key(k);
		break;
	case SCR_ART:
		art_key(k);
		break;
	case SCR_CTRL:
		ct_key(k);
		break;
	case SCR_PUPD:
		up_key(k);
		break;
	case SCR_MAP:
	case SCR_CTEST:
		break;
	case SCR_SETTINGS:
		if (vlist_key(&set_list, k)) { si_update(); break; }
		if (k == K_LEFT) settings_change(-1);
		else if (k == K_RIGHT || k == K_A) {
			if (set_id(set_list.sel) == SET_RESCAN) { if (k == K_A) show_scan(); }
			else if (set_id(set_list.sel) == SET_SCRAPE) { if (k == K_A) show_art(); }
			else if (set_id(set_list.sel) == SET_SOURCES) show_sources();
			else if (set_id(set_list.sel) == SET_DIAG) show_diag();
			else if (set_id(set_list.sel) == SET_WIFI) show_wifi();
			else if (set_id(set_list.sel) == SET_BT) show_bt();
			else if (set_id(set_list.sel) == SET_CTRL) show_ctrl();
			else if (set_id(set_list.sel) == SET_PHOSUPD) show_update();
			else if (set_id(set_list.sel) == SET_REMOTE) show_remote();
			else if (set_id(set_list.sel) == SET_CLOUD) show_cloud();
			else if (set_id(set_list.sel) == SET_BACKUP) show_backup();
			else if (set_id(set_list.sel) == SET_PORTS) show_ports();
			else if (set_id(set_list.sel) == SET_STORAGE) show_storage();
			else if (set_id(set_list.sel) == SET_DISCS) show_discs();
			else if (set_id(set_list.sel) == SET_RA) show_ra();
			else if (set_id(set_list.sel) == SET_SHOTS) show_shots();
			else if (set_id(set_list.sel) == SET_VIDEO) show_video();
			else if (set_id(set_list.sel) == SET_ABOUT) show_about();
			else if ((set_id(set_list.sel) == SET_CORES || set_id(set_list.sel) == SET_UPDATE_ALL) && k == K_A) {
				if (set_id(set_list.sel) == SET_UPDATE_ALL && !cr.all_available) { toast_show("Update All isn't on the SD card (Scripts/update_all.sh)"); break; }
				sysinfo_t si;
				sysinfo_read(&si);
				cJSON *w = api_call("{\"cmd\":\"wifi\"}");
				cJSON *ws = w ? cJSON_GetObjectItem(w, "status") : NULL;
				int net = si.ethernet || cJSON_IsTrue(cJSON_GetObjectItem(ws, "connected"));
				cJSON_Delete(w);
				if (!cores_start_job(net, set_id(set_list.sel) == SET_UPDATE_ALL)) vlist_refresh(&set_list);
			}
			else if (set_id(set_list.sel) == SET_SCRIPTS) show_scripts();
			else if (set_id(set_list.sel) == SET_WALL_PICK) show_wallpapers();
			else if (set_id(set_list.sel) == SET_GUARD) show_guard();
			else if (set_id(set_list.sel) == SET_UPDLOG) show_log();
			else if (set_id(set_list.sel) == SET_MISTER && k == K_A) {
				if (mainlink_give()) toast_show("Back to Phosphor: hold the menu button, or System > Launch Phosphor");
				else toast_show("Needs Phosphor's Main: press F9 on a keyboard to switch");
			}
			else if (set_id(set_list.sel) == SET_RESTART && k == K_A) restart_ask();
			else if (set_id(set_list.sel) == SET_SHUTDOWN && k == K_A) shutdown_ask();
			else settings_change(1);
		}
		else if (k == K_B || k == K_MENU) show_home();
		break;
	}
}

/* ---------- Settings › Discs ---------- */
static vlist_t disc_list;
enum { DS_ON, DS_DRIVE, DS_EJECT, DS_COUNT };
static lv_obj_t *ds_note;
static void ds_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	*dim = 0;
	if (i == DS_ON) { snprintf(t, n, "Discs"); snprintf(val, vn, "<  %s  >", dk.enabled ? "On" : "Off"); }
	else if (i == DS_DRIVE) {
		snprintf(t, n, "Drive");
		if (!dk.enabled) snprintf(val, vn, "not used");
		else if (!strcmp(dk.state, "none") || !dk.state[0]) snprintf(val, vn, "no drive found");
		else if (!strcmp(dk.state, "empty")) snprintf(val, vn, "empty");
		else snprintf(val, vn, "%s", disc_line());
		*dim = 1;
	} else { snprintf(t, n, "Eject the disc"); snprintf(val, vn, ">"); *dim = !dk.enabled || !strcmp(dk.state, "none"); }
}
static void discs_screen_update(void)
{
	if (cur != SCR_DISCS) return;
	vlist_refresh(&disc_list);
}
static void show_discs(void)
{
	cur = SCR_DISCS;
	disc_refresh();
	lv_obj_clean(body);
	screen_title("SETTINGS", "Discs");
	vlist_create(&disc_list, body, S(56), S(176), SW(700), S(440));
	disc_list.count = DS_COUNT;
	disc_list.fill = ds_fill;
	disc_list.sel = 0;
	disc_list.top = 0;
	vlist_refresh(&disc_list);
	/* 16:9: as designed; narrower (4:3): stacked and as tall as its text, which wraps onto more lines */
	lv_obj_t *panel = mk_box(body, SW(444), NARROW ? LV_SIZE_CONTENT : S(300), PAL.panel, S(16));
	LR_HIDE(panel);
	lv_obj_set_style_border_width(panel, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(panel, PAL.line, 0);
	lv_obj_set_pos(panel, SX(780), S(176));
	if (NARROW) {
		lv_obj_set_style_pad_hor(panel, S(24), 0);
		lv_obj_set_style_pad_ver(panel, S(22), 0);
		lv_obj_set_style_pad_row(panel, S(14), 0);
		lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
	}
	lv_obj_t *h = mk_label(panel, FONT_TITLE, PAL.text, "Discs");
	if (!NARROW) lv_obj_set_pos(h, S(24), S(22));
	ds_note = mk_label(panel, FONT_BODY, PAL.dim, "Play games from the dock's CD/DVD drive: PlayStation, Saturn, Mega-CD, PC Engine CD, "
	                   "Neo Geo CD, CD-i and 3DO, and MSU-1 / MD+ discs. Off: Phosphor doesn't use the drive.");
	lv_label_set_long_mode(ds_note, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(ds_note, NARROW ? SW(444) - 2 * S(24) : S(396));
	if (!NARROW) lv_obj_set_pos(ds_note, S(24), S(64));
	lv_obj_t *cr = mk_label(panel, FONT_SMALL, PAL.faint, "Disc support by Anime0t4ku and bonna79.");
	if (NARROW) {
		lv_label_set_long_mode(cr, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_set_width(cr, SW(444) - 2 * S(24));
		lv_obj_set_style_margin_top(cr, S(16), 0);
	} else lv_obj_set_pos(cr, S(24), S(250));
	static const char *const hints[] = { "A", "Select", "\xE2\x86\x94", "Change", "B", "Back", NULL };
	footer_set(hints);
}
static void discs_key(int k)
{
	if (vlist_key(&disc_list, k)) return;
	if (disc_list.sel == DS_ON && (k == K_LEFT || k == K_RIGHT || k == K_A)) {
		int on = !dk.enabled;
		cJSON_Delete(api_call("{\"cmd\":\"disc_enable\",\"on\":%s}", on ? "true" : "false"));
		mainlink_disc_enable(on);
		dk.enabled = on;
		disc_refresh();
		toast_show(on ? "Discs on" : "Discs off: the drive isn't used");
	} else if (disc_list.sel == DS_EJECT && k == K_A && dk.enabled) disc_eject_now();
	else if (k == K_B || k == K_MENU) back_to_settings(SET_DISCS);
}



/* ---------- Settings › Screenshots (phosphord shot.c) ----------
 * Both sticks in (L3 + R3): in a game Phosphor's Main takes MiSTer's own screenshot (screenshots/<core>/);
 * on Phosphor's screens phosphord saves the framebuffer (screenshots/Phosphor/). Here: the combo on or
 * off, and the screenshots (a grid of small copies; A one full screen, Left/Right through them, Y deletes). */
static vlist_t shot_list;
enum { SH_COMBO, SH_MINE, SH_WHERE, SHOT_ROWS };
static cJSON *shots;               /* phosphord's "shots" */
static int shot_sel, shot_top, shot_del_armed;
static uint32_t shot_del_at;
static char shot_src[16][840];     /* the grid's pictures ("A:<path>") */
static char shotv_src[840];
#define SHOT_COLS 5
#define SHOT_ROWS_VIS 2
static void take_shot(void)
{
	cJSON *r = api_call_slow(8000, "{\"cmd\":\"shot\"}");
	input_flush();          /* presses made while the picture was saved would land all at once */
	if (api_ok(r)) toast_show(!strcmp(api_str(r, "where") ? api_str(r, "where") : "", "game") ? "Screenshot saved" : "Screenshot saved: Settings › Screenshots");
	else toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't take the screenshot");
	cJSON_Delete(r);
}
static void shots_load(void)
{
	cJSON_Delete(shots);
	cJSON *r = api_call("{\"cmd\":\"shots\"}");
	shots = api_ok(r) ? r : (cJSON_Delete(r), NULL);
}
static int shots_n(void) { return cJSON_GetArraySize(cJSON_GetObjectItem(shots, "shots")); }
static void shots_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	*dim = 0;
	if (i == SH_COMBO) { snprintf(t, n, "Button combo"); snprintf(val, vn, "<  %s  >", SETTINGS.shot_off ? "Off" : "Both sticks in (L3 + R3)"); }
	else if (i == SH_MINE) {
		snprintf(t, n, "Your screenshots");
		double mb = api_num(shots, "bytes", 0) / 1048576.0;
		if (shots_n()) snprintf(val, vn, "%d  \xC2\xB7  %.0f MB  >", shots_n(), mb < 1 && mb > 0 ? 1.0 : mb);
		else { snprintf(val, vn, "none yet"); *dim = 1; }
	} else { snprintf(t, n, "Where they're saved"); snprintf(val, vn, "SD card \xC2\xB7 screenshots"); *dim = 1; }
}
static void show_shots(void)
{
	cur = SCR_SHOTS;
	shots_load();
	lv_obj_clean(body);
	screen_title("SETTINGS", "Screenshots");
	vlist_create(&shot_list, body, S(56), S(176), SW(700), S(440));
	shot_list.count = SHOT_ROWS;
	shot_list.fill = shots_fill;
	vlist_refresh(&shot_list);
	lv_obj_t *panel = mk_box(body, SW(444), S(300), PAL.panel, S(16));
	LR_HIDE(panel);
	lv_obj_set_style_border_width(panel, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(panel, PAL.line, 0);
	lv_obj_set_pos(panel, SX(780), S(176));
	lv_obj_t *h = mk_label(panel, FONT_TITLE, PAL.text, "Screenshots");
	lv_obj_set_pos(h, S(24), S(22));
	lv_obj_t *nt = mk_label(panel, FONT_BODY, PAL.dim, "Press both sticks in (L3 + R3) to save a picture of the screen: the game, or Phosphor's own screens. "
	                        "Off: the combo does nothing.");
	lv_label_set_long_mode(nt, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(nt, SW(396));
	lv_obj_set_pos(nt, S(24), S(64));
	static const char *const hints[] = { "A", "Select", "\xE2\x86\x94", "Change", "B", "Back", NULL };
	footer_set(hints);
}
static void show_shotgrid(void);
static void shots_key(int k)
{
	if (vlist_key(&shot_list, k)) return;
	if (shot_list.sel == SH_COMBO && (k == K_LEFT || k == K_RIGHT || k == K_A)) {
		SETTINGS.shot_off = !SETTINGS.shot_off;
		settings_save();
		mainlink_shot_combo(!SETTINGS.shot_off);
		vlist_refresh(&shot_list);
		toast_show(SETTINGS.shot_off ? "Screenshot combo off" : "Screenshots: press both sticks in (L3 + R3)");
	} else if (shot_list.sel == SH_MINE && k == K_A && shots_n()) { shot_sel = 0; shot_top = 0; show_shotgrid(); }
	else if (k == K_B || k == K_MENU) back_to_settings(SET_SHOTS);
}
static const char *shot_id(int i) { return api_str(cJSON_GetArrayItem(cJSON_GetObjectItem(shots, "shots"), i), "id"); }
static void shot_caption(int i, char *out, size_t n)
{
	cJSON *x = cJSON_GetArrayItem(cJSON_GetObjectItem(shots, "shots"), i);
	time_t at = (time_t)api_num(x, "at", 0);
	struct tm tm;
	localtime_r(&at, &tm);
	char when[48];
	strftime(when, sizeof(when), SETTINGS.clock24 ? "%b %d, %H:%M" : "%b %d, %I:%M %p", &tm);
	snprintf(out, n, "%s  \xC2\xB7  %s  \xC2\xB7  %d of %d", api_str(x, "core") ? api_str(x, "core") : "", when, i + 1, shots_n());
}
static lv_obj_t *shot_cap;
static void show_shotgrid(void)
{
	cur = SCR_SHOTGRID;
	shot_del_armed = 0;
	int n = shots_n();
	if (!n) { show_shots(); return; }
	if (shot_sel >= n) shot_sel = n - 1;
	if (shot_sel < shot_top * SHOT_COLS) shot_top = shot_sel / SHOT_COLS;
	if (shot_sel >= (shot_top + SHOT_ROWS_VIS) * SHOT_COLS) shot_top = shot_sel / SHOT_COLS - SHOT_ROWS_VIS + 1;
	lv_obj_clean(body);
	char title[64];
	snprintf(title, sizeof(title), "%d screenshot%s", n, n == 1 ? "" : "s");
	screen_title("SCREENSHOTS", title);
	int gap = S(16), cw = (W - S(112) - gap * (SHOT_COLS - 1)) / SHOT_COLS, ch = cw * 9 / 16;
	for (int k = 0; k < SHOT_COLS * SHOT_ROWS_VIS; k++) {
		int i = shot_top * SHOT_COLS + k;
		if (i >= n) break;
		lv_obj_t *c = mk_box(body, cw, ch, PAL.panel, S(8));
		lv_obj_set_pos(c, S(56) + (k % SHOT_COLS) * (cw + gap), S(180) + (k / SHOT_COLS) * (ch + gap));
		lv_obj_set_style_clip_corner(c, true, 0);
		if (i == shot_sel) { lv_obj_set_style_outline_width(c, S(3), 0); lv_obj_set_style_outline_color(c, PAL.acc, 0); lv_obj_set_style_outline_pad(c, S(3), 0); }
		cJSON *r = api_call("{\"cmd\":\"shot_img\",\"id\":\"%s\"}", shot_id(i) ? shot_id(i) : "");
		if (api_ok(r) && api_str(r, "path")) {
			snprintf(shot_src[k], sizeof(shot_src[k]), "A:%s", api_str(r, "path"));
			lv_obj_t *im = lv_image_create(c);
			lv_obj_set_size(im, cw, ch);
			lv_image_set_inner_align(im, LV_IMAGE_ALIGN_COVER);
			lv_image_set_src(im, shot_src[k]);
			lv_obj_center(im);
		}
		cJSON_Delete(r);
	}
	char cap[160];
	shot_caption(shot_sel, cap, sizeof(cap));
	shot_cap = mk_label(body, FONT_BODY, PAL.dim, cap);
	lv_obj_set_pos(shot_cap, S(56), S(180) + SHOT_ROWS_VIS * (ch + gap) + S(8));
	static const char *const hints[] = { "A", "View", "Y", "Delete", "B", "Back", NULL };
	footer_set(hints);
}
static int shot_delete_sel(void)
{
	uint32_t now = lv_tick_get();
	if (!shot_del_armed || now - shot_del_at > 3000) {
		shot_del_armed = 1;
		shot_del_at = now;
		toast_show("Press Y again to delete this screenshot");
		return 0;
	}
	shot_del_armed = 0;
	char id[320];
	snprintf(id, sizeof(id), "%s", shot_id(shot_sel) ? shot_id(shot_sel) : "");
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "shot_delete");
	cJSON_AddStringToObject(q, "id", id);
	char *b = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = b ? api_call("%s", b) : NULL;
	free(b);
	int ok = api_ok(r);
	toast_show(ok ? "Deleted" : (r && api_str(r, "error") ? api_str(r, "error") : "Couldn't delete it"));
	cJSON_Delete(r);
	shots_load();
	return ok;
}
static void show_shotview(void);
static void shotgrid_key(int k)
{
	int n = shots_n(), s = shot_sel;
	if (k == K_LEFT && s > 0) s--;
	else if (k == K_RIGHT && s + 1 < n) s++;
	else if (k == K_UP && s >= SHOT_COLS) s -= SHOT_COLS;
	else if (k == K_DOWN && s + SHOT_COLS < n) s += SHOT_COLS;
	else if (k == K_DOWN && s / SHOT_COLS < (n - 1) / SHOT_COLS) s = n - 1;
	else if (k == K_A) { show_shotview(); return; }
	else if (k == K_Y) { if (shot_delete_sel()) show_shotgrid(); return; }
	else if (k == K_B || k == K_MENU) { show_shots(); shot_list.sel = SH_MINE; vlist_refresh(&shot_list); return; }
	if (s != shot_sel) { shot_sel = s; show_shotgrid(); }
}
/* one full screen (a TV-sized copy), Left / Right through them */
static void show_shotview(void)
{
	cur = SCR_SHOTVIEW;
	shot_del_armed = 0;
	if (!shots_n()) { show_shots(); return; }
	lv_obj_clean(body);
	lv_obj_t *bg = mk_box(body, W, H, lv_color_hex(0x000000), 0);
	lv_obj_set_pos(bg, 0, 0);
	cJSON *r = api_call_slow(8000, "{\"cmd\":\"shot_img\",\"id\":\"%s\",\"view\":true}", shot_id(shot_sel) ? shot_id(shot_sel) : "");
	if (api_ok(r) && api_str(r, "path")) {
		snprintf(shotv_src, sizeof(shotv_src), "A:%s", api_str(r, "path"));
		lv_obj_t *im = lv_image_create(bg);
		lv_obj_set_size(im, W - S(112), H - S(84) - S(76));       /* between the status bar and the hints */
		lv_image_set_inner_align(im, LV_IMAGE_ALIGN_CONTAIN);
		lv_image_set_src(im, shotv_src);
		lv_obj_set_pos(im, S(56), S(84));
	} else {
		lv_obj_t *e = mk_label(bg, FONT_BODY, PAL.dim, "This screenshot can't be shown.");
		lv_obj_center(e);
	}
	cJSON_Delete(r);
	char cap[160];
	shot_caption(shot_sel, cap, sizeof(cap));
	lv_obj_t *c = mk_label(bg, FONT_SMALL, PAL.dim, cap);
	lv_obj_align(c, LV_ALIGN_BOTTOM_RIGHT, -S(56), -S(30));
	static const char *const hints[] = { "\xE2\x86\x94", "Next", "Y", "Delete", "B", "Back", NULL };
	footer_set(hints);
}
static void shotview_key(int k)
{
	int n = shots_n();
	if (k == K_LEFT && shot_sel > 0) { shot_sel--; show_shotview(); }
	else if (k == K_RIGHT && shot_sel + 1 < n) { shot_sel++; show_shotview(); }
	else if (k == K_Y) { if (shot_delete_sel()) { if (!shots_n()) show_shots(); else { if (shot_sel >= shots_n()) shot_sel = shots_n() - 1; show_shotview(); } } }
	else if (k == K_B || k == K_MENU || k == K_A) show_shotgrid();
}


/* ---------- Music (phosphord music.c): albums, a music CD in the drive, Now playing ----------
 * Plays only while no game runs. L / R pick what's shown: All, Steam, Ripped CDs, In the drive. */
static vlist_t mu_list, mu_tracks;
static cJSON *mu_albums, *mu_cd, *mu_status, *mu_album;   /* lists; the playing state; the album in the panel */
static int mu_show, mu_rows[2100], mu_n, mu_tracks_view;
static lv_obj_t *mu_cover, *mu_title, *mu_info, *mu_tl;
static char mu_cover_src[1200];
static const char *const MU_SHOW[] = { "All", "Steam", "Ripped CDs", "In the drive" };
enum { MR_NOW = -2, MR_CD = -1 };       /* the rows before the albums */
static int mu_playing(void)
{
	cJSON *st = mu_status ? cJSON_GetObjectItem(mu_status, "track") : NULL;
	return st != NULL;
}
static void mu_load(void)
{
	cJSON *r = api_call("{\"cmd\":\"music_albums\"}");
	cJSON_Delete(mu_albums); mu_albums = NULL;
	cJSON_Delete(mu_cd); mu_cd = NULL;
	if (api_ok(r)) { mu_albums = cJSON_DetachItemFromObject(r, "albums"); mu_cd = cJSON_DetachItemFromObject(r, "cd"); }
	cJSON_Delete(r);
	r = api_call("{\"cmd\":\"music\"}");
	cJSON_Delete(mu_status);
	mu_status = api_ok(r) ? cJSON_DetachItemFromObject(r, "music") : NULL;
	cJSON_Delete(r);
	/* the rows: Now playing, the CD, then the albums this view shows */
	mu_n = 0;
	if (mu_playing() && mu_show == 0) mu_rows[mu_n++] = MR_NOW;
	if (mu_cd && (mu_show == 0 || mu_show == 3)) mu_rows[mu_n++] = MR_CD;
	cJSON *a;
	int i = 0;
	cJSON_ArrayForEach(a, mu_albums) {
		const char *src = api_str(a, "source") ? api_str(a, "source") : "";
		int keep = mu_show == 0 || (mu_show == 1 && !strcmp(src, "steam")) || (mu_show == 2 && !strcmp(src, "cd"));
		if (keep && mu_n < 2100) mu_rows[mu_n++] = i;
		i++;
	}
}
static cJSON *mu_row_album(int row)
{
	if (row < 0 || row >= mu_n) return NULL;
	return mu_rows[row] == MR_CD ? mu_cd : mu_rows[row] >= 0 ? cJSON_GetArrayItem(mu_albums, mu_rows[row]) : NULL;
}
static void mu_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	*dim = 0;
	if (i < 0 || i >= mu_n) { t[0] = 0; val[0] = 0; return; }
	if (mu_rows[i] == MR_NOW) {
		cJSON *tr = cJSON_GetObjectItem(mu_status, "track");
		snprintf(t, n, "\xE2\x99\xAA  Now playing \xC2\xB7 %s", api_str(tr, "title") ? api_str(tr, "title") : "");
		snprintf(val, vn, "%s", !strcmp(api_str(mu_status, "state") ? api_str(mu_status, "state") : "", "paused") ? "paused" : ">");
		return;
	}
	cJSON *a = mu_row_album(i);
	if (mu_rows[i] == MR_CD) {
		snprintf(t, n, "In the drive \xC2\xB7 %s", a && api_str(a, "title") ? api_str(a, "title") : "Music CD");
		snprintf(val, vn, "music CD \xC2\xB7 %d tracks", a ? (int)api_num(a, "tracks", 0) : 0);
		return;
	}
	snprintf(t, n, "%s", a && api_str(a, "title") ? api_str(a, "title") : "");
	const char *src = a ? api_str(a, "source") : "";
	snprintf(val, vn, "%s \xC2\xB7 %d", src && !strcmp(src, "steam") ? "Steam" : src && !strcmp(src, "cd") ? "ripped CD" : "album", a ? (int)api_num(a, "tracks", 0) : 0);
}
/* the panel: the album's cover, name, and its first tracks */
static void mu_panel(void)
{
	if (!mu_title) return;
	int row = mu_list.sel;
	if (row < mu_n && mu_rows[row] == MR_NOW) {
		cJSON *tr = cJSON_GetObjectItem(mu_status, "track");
		lv_label_set_text(mu_title, api_str(tr, "album") ? api_str(tr, "album") : "");
		char t[300];
		snprintf(t, sizeof(t), "%s\nTrack %d of %d", api_str(tr, "artist") ? api_str(tr, "artist") : "", (int)api_num(tr, "index", 0) + 1, (int)api_num(tr, "count", 0));
		lv_label_set_text(mu_info, t);
		lv_label_set_text(mu_tl, "A opens Now playing");
		const char *cv = api_str(tr, "cover");
		lv_obj_set_hidden(mu_cover, !cv);
		if (cv) { snprintf(mu_cover_src, sizeof(mu_cover_src), "A:%s", cv); lv_image_set_src(mu_cover, mu_cover_src); }
		return;
	}
	cJSON *a = mu_row_album(row);
	if (!a) { lv_label_set_text(mu_title, mu_n ? "" : "No music yet"); lv_label_set_text(mu_info, mu_n ? "" : "Put albums in a Music folder on the SD card or a drive, rip a CD, or send soundtracks from Phosphor for Windows."); lv_label_set_text(mu_tl, ""); lv_obj_set_hidden(mu_cover, true); return; }
	cJSON_Delete(mu_album);
	mu_album = NULL;
	if (mu_rows[row] == MR_CD) mu_album = cJSON_Duplicate(a, 1);
	else {
		cJSON *r = api_call("{\"cmd\":\"music_album\",\"id\":\"%s\"}", api_str(a, "id") ? api_str(a, "id") : "");
		mu_album = api_ok(r) ? cJSON_DetachItemFromObject(r, "album") : NULL;
		cJSON_Delete(r);
	}
	lv_label_set_text(mu_title, api_str(a, "title") ? api_str(a, "title") : "");
	char t[300];
	const char *ar = api_str(a, "artist"), *yr = api_str(a, "year");
	snprintf(t, sizeof(t), "%s%s%s%s%d tracks%s", ar && *ar ? ar : "", ar && *ar ? "  \xC2\xB7  " : "", yr && *yr ? yr : "", yr && *yr ? "  \xC2\xB7  " : "",
	         (int)api_num(a, "tracks", 0), mu_rows[row] == MR_CD && cJSON_IsTrue(cJSON_GetObjectItem(a, "looking")) ? "\nLooking it up on MusicBrainz..." :
	         mu_rows[row] == MR_CD && !cJSON_IsTrue(cJSON_GetObjectItem(a, "named")) ? "\nNot in MusicBrainz" : "");
	cJSON *rp = mu_status ? cJSON_GetObjectItem(mu_status, "rip") : NULL;
	if (mu_rows[row] == MR_CD && rp && (cJSON_IsTrue(cJSON_GetObjectItem(rp, "busy")) || (api_str(rp, "message") && *api_str(rp, "message")))) {
		size_t l = strlen(t);
		if (cJSON_IsTrue(cJSON_GetObjectItem(rp, "busy")))
			snprintf(t + l, sizeof(t) - l, "\n\n%s\n%d%% done (B stops)", api_str(rp, "message") ? api_str(rp, "message") : "", (int)api_num(rp, "percent", 0));
		else snprintf(t + l, sizeof(t) - l, "\n\n%s", api_str(rp, "message"));
	}
	lv_label_set_text(mu_info, t);
	char tl[700] = "";
	cJSON *x;
	int k = 0;
	cJSON_ArrayForEach(x, cJSON_GetObjectItem(mu_album, "track_list")) {
		if (k == 6) { strncat(tl, "...", sizeof(tl) - strlen(tl) - 1); break; }
		char line[200];
		snprintf(line, sizeof(line), "%d  %s\n", (int)api_num(x, "no", k + 1), api_str(x, "title") ? api_str(x, "title") : "");
		strncat(tl, line, sizeof(tl) - strlen(tl) - 1);
		k++;
	}
	lv_label_set_text(mu_tl, tl);
	const char *cv = api_str(a, "cover");
	lv_obj_set_hidden(mu_cover, !cv);
	if (cv) { snprintf(mu_cover_src, sizeof(mu_cover_src), "A:%s", cv); lv_image_set_src(mu_cover, mu_cover_src); }
}
static void mu_tracks_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	cJSON *x = cJSON_GetArrayItem(cJSON_GetObjectItem(mu_album, "track_list"), i);
	*dim = 0;
	snprintf(t, n, "%d  %s", x ? (int)api_num(x, "no", i + 1) : i + 1, x && api_str(x, "title") ? api_str(x, "title") : "");
	long ms = x ? (long)api_num(x, "ms", 0) : 0;
	if (ms > 0) snprintf(val, vn, "%ld:%02ld", ms / 60000, ms / 1000 % 60); else val[0] = 0;
}
static const char *const MU_HINTS[] = { "A", "Play", "X", "Shuffle", "Y", "Tracks", "L R", "Show", "B", "Back", NULL };
static const char *const MU_CD_HINTS[] = { "A", "Play", "X", "Rip to the NVMe", "Y", "Tracks", "\xE2\x89\xA1", "Eject", "B", "Back", NULL };
static void mu_footer(void)
{
	static const char *const tr_hints[] = { "A", "Play from here", "B", "Back", NULL };
	if (mu_tracks_view) footer_set(tr_hints);
	else footer_set(mu_list.sel < mu_n && mu_rows[mu_list.sel] == MR_CD ? MU_CD_HINTS : MU_HINTS);
}
static void mu_mk_chips(void)
{
	char b[160] = "";
	for (int i = 0; i < 4; i++) {
		size_t l = strlen(b);
		snprintf(b + l, sizeof(b) - l, i == mu_show ? "%s[ %s ]" : "%s%s", i ? "     " : "", MU_SHOW[i]);
	}
	lv_obj_t *c = mk_label(body, FONT_SMALL, PAL.acc, b);
	lv_obj_set_pos(c, S(56), S(170));
}
static void mu_draw(void)
{
	lv_obj_clean(body);
	screen_title("LIBRARY", "Music");
	mu_mk_chips();
	lv_obj_t *panel = mk_box(body, SW(480), S(420), PAL.panel, S(16));
	LR_HIDE(panel);
	lv_obj_set_style_border_width(panel, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(panel, PAL.line, 0);
	lv_obj_set_pos(panel, W - S(56) - SW(480), S(204));
	lv_obj_t *ph = mk_box(panel, S(180), S(180), lv_color_hex(0x2a2119), S(10));   /* no cover: a note */
	lv_obj_set_pos(ph, S(24), S(24));
	lv_obj_t *phi = icon_create(ph, ICON_MUSIC, S(64));
	lv_obj_set_style_text_color(phi, PAL.acc, 0);
	lv_obj_center(phi);
	mu_cover = lv_image_create(panel);
	lv_obj_set_size(mu_cover, S(180), S(180));
	lv_image_set_inner_align(mu_cover, LV_IMAGE_ALIGN_CONTAIN);
	lv_obj_set_pos(mu_cover, S(24), S(24));
	lv_obj_set_hidden(mu_cover, true);
	mu_title = mk_label(panel, FONT_TITLE, PAL.text, "");
	lv_label_set_long_mode(mu_title, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(mu_title, S(220));
	lv_obj_set_pos(mu_title, S(228), S(24));
	mu_info = mk_label(panel, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(mu_info, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(mu_info, S(220));
	lv_obj_set_pos(mu_info, S(228), S(110));
	mu_tl = mk_label(panel, FONT_SMALL, PAL.dim, "");
	lv_label_set_long_mode(mu_tl, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(mu_tl, S(432));
	lv_obj_set_pos(mu_tl, S(24), S(224));
	if (mu_tracks_view) {
		vlist_create(&mu_tracks, body, S(56), S(204), SW(620), S(420));
		mu_tracks.fill = mu_tracks_fill;
		mu_tracks.count = cJSON_GetArraySize(cJSON_GetObjectItem(mu_album, "track_list"));
		vlist_refresh(&mu_tracks);
		lv_label_set_text(mu_title, api_str(mu_album, "title") ? api_str(mu_album, "title") : "");
		const char *cv = api_str(mu_album, "cover");
		lv_obj_set_hidden(mu_cover, !cv);
		if (cv) { snprintf(mu_cover_src, sizeof(mu_cover_src), "A:%s", cv); lv_image_set_src(mu_cover, mu_cover_src); }
	} else {
		vlist_create(&mu_list, body, S(56), S(204), SW(620), S(420));
		mu_list.fill = mu_fill;
		mu_list.count = mu_n;
		if (mu_list.sel >= mu_n) mu_list.sel = mu_n ? mu_n - 1 : 0;
		vlist_refresh(&mu_list);
		mu_panel();
	}
	mu_footer();
}
static void show_music(void)
{
	cur = SCR_MUSIC;
	mu_tracks_view = 0;
	mu_load();
	mu_list.sel = 0;
	mu_list.top = 0;
	mu_draw();
}
static void mu_play(const char *album, int track, int shuffle)
{
	if (shuffle) cJSON_Delete(api_call("{\"cmd\":\"music_ctl\",\"do\":\"shuffle\",\"on\":true}"));
	cJSON *r = api_call("{\"cmd\":\"music_play\",\"album\":\"%s\",\"track\":%d}", album, track);
	if (!api_ok(r)) toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't play it");
	cJSON_Delete(r);
	show_nowplay();
}
static void music_key(int k)
{
	if (mu_tracks_view) {
		if (vlist_key(&mu_tracks, k)) return;
		if (k == K_A) {
			cJSON *x = cJSON_GetArrayItem(cJSON_GetObjectItem(mu_album, "track_list"), mu_tracks.sel);
			cJSON_Delete(api_call("{\"cmd\":\"music_ctl\",\"do\":\"shuffle\",\"on\":false}"));
			mu_play(api_str(mu_album, "id") ? api_str(mu_album, "id") : "cd", x ? (int)api_num(x, "no", mu_tracks.sel + 1) : 1, 0);
		} else if (k == K_B || k == K_MENU) { mu_tracks_view = 0; mu_draw(); }
		return;
	}
	if (vlist_key(&mu_list, k)) { mu_panel(); mu_footer(); return; }
	if (k == K_L || k == K_R) {             /* (vlist takes L/R for paging only when it has more than a page) */
		mu_show = (mu_show + (k == K_L ? 3 : 1)) % 4;
		mu_load(); mu_list.sel = 0; mu_list.top = 0; mu_draw();
		return;
	}
	int row = mu_list.sel;
	if (row < mu_n && mu_rows[row] == MR_NOW) { if (k == K_A) show_nowplay(); else if (k == K_B || k == K_MENU) show_home(); return; }
	cJSON *a = mu_row_album(row);
	const char *id = a ? (mu_rows[row] == MR_CD ? "cd" : api_str(a, "id")) : NULL;
	if (k == K_A && id) { cJSON_Delete(api_call("{\"cmd\":\"music_ctl\",\"do\":\"shuffle\",\"on\":false}")); mu_play(id, 0, 0); }
	else if (k == K_X && row < mu_n && mu_rows[row] == MR_CD) {
		cJSON *r = api_call("{\"cmd\":\"music_rip\"}");
		toast_show(api_ok(r) ? "Copying the CD to Music (FLAC, an exact copy)" : (r && api_str(r, "error") ? api_str(r, "error") : "Couldn't start"));
		cJSON_Delete(r);
		mu_load(); mu_panel();
	}
	else if (k == K_X && id) mu_play(id, 0, 1);
	else if (k == K_Y && mu_album) { mu_tracks_view = 1; mu_tracks.sel = 0; mu_tracks.top = 0; mu_draw(); }
	else if (k == K_MENU && row < mu_n && mu_rows[row] == MR_CD) { disc_eject_now(); }
	else if (k == K_B && row < mu_n && mu_rows[row] == MR_CD && mu_status && cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(mu_status, "rip"), "busy"))) {
		cJSON_Delete(api_call("{\"cmd\":\"music_rip\",\"stop\":true}"));
		toast_show("Stopping: the tracks done are kept");
	}
	else if (k == K_B || k == K_MENU) show_home();
}

/* Now playing: the cover, the track, where it's at; follows the player once a second */
static lv_obj_t *np_cover, *np_eyebrow, *np_title, *np_sub, *np_bar, *np_fill, *np_pos, *np_len, *np_idx, *np_next;
static char np_src[1200];
static void np_update(void)
{
	cJSON *r = api_call("{\"cmd\":\"music\"}");
	cJSON *m = api_ok(r) ? cJSON_GetObjectItem(r, "music") : NULL;
	cJSON *t = m ? cJSON_GetObjectItem(m, "track") : NULL;
	if (!t) {
		lv_label_set_text(np_eyebrow, "MUSIC");
		lv_label_set_text(np_title, "Nothing playing");
		lv_label_set_text(np_sub, "Pick an album in Music");
		lv_obj_set_hidden(np_cover, true);
		lv_obj_set_width(np_fill, 0);
		cJSON_Delete(r);
		return;
	}
	char e[120];
	snprintf(e, sizeof(e), "%s%s%s", !strcmp(api_str(m, "state"), "paused") ? "PAUSED" : "NOW PLAYING",
	         cJSON_IsTrue(cJSON_GetObjectItem(t, "cd")) ? "  \xC2\xB7  MUSIC CD" : "",
	         cJSON_IsTrue(cJSON_GetObjectItem(m, "shuffle")) ? "  \xC2\xB7  SHUFFLE" : "");
	lv_label_set_text(np_eyebrow, e);
	lv_label_set_text(np_title, api_str(t, "title") ? api_str(t, "title") : "");
	char sub[400];
	const char *ar = api_str(t, "artist");
	snprintf(sub, sizeof(sub), "%s%s%s", ar && *ar ? ar : "", ar && *ar ? "  \xC2\xB7  " : "", api_str(t, "album") ? api_str(t, "album") : "");
	lv_label_set_text(np_sub, sub);
	long pos = (long)api_num(t, "pos_ms", 0), len = (long)api_num(t, "len_ms", 0);
	char b[32];
	snprintf(b, sizeof(b), "%ld:%02ld", pos / 60000, pos / 1000 % 60); lv_label_set_text(np_pos, b);
	snprintf(b, sizeof(b), "%ld:%02ld", len / 60000, len / 1000 % 60); lv_label_set_text(np_len, b);
	snprintf(b, sizeof(b), "track %d of %d", (int)api_num(t, "index", 0) + 1, (int)api_num(t, "count", 0)); lv_label_set_text(np_idx, b);
	int bw = lv_obj_get_width(np_bar);
	lv_obj_set_width(np_fill, len > 0 ? (int)((long long)bw * pos / len) : 0);
	char nx[300];
	snprintf(nx, sizeof(nx), "%s%s", api_str(t, "next") ? "Next: " : "", api_str(t, "next") ? api_str(t, "next") : "");
	lv_label_set_text(np_next, nx);
	const char *cv = api_str(t, "cover");
	lv_obj_set_hidden(np_cover, !cv);
	if (cv && strcmp(np_src + 2, cv)) { snprintf(np_src, sizeof(np_src), "A:%s", cv); lv_image_set_src(np_cover, np_src); }
	cJSON_Delete(r);
}
static void show_nowplay(void)
{
	cur = SCR_NOWPLAY;
	lv_obj_clean(body);
	np_src[0] = 0;
	lv_obj_t *cb = mk_box(body, S(380), S(380), lv_color_hex(0x2a2119), S(12));
	lv_obj_set_style_clip_corner(cb, true, 0);
	lv_obj_set_pos(cb, S(56), S(140));
	lv_obj_t *ic = icon_create(cb, ICON_MUSIC, S(96));
	lv_obj_set_style_text_color(ic, PAL.acc, 0);
	lv_obj_center(ic);
	np_cover = lv_image_create(cb);
	lv_obj_set_size(np_cover, S(380), S(380));
	lv_image_set_inner_align(np_cover, LV_IMAGE_ALIGN_COVER);
	lv_obj_center(np_cover);
	int x = S(480), w = W - x - S(56);
	np_eyebrow = mk_label(body, FONT_LABEL, PAL.dim, "");
	lv_obj_set_style_text_letter_space(np_eyebrow, S(3), 0);
	lv_obj_set_pos(np_eyebrow, x, S(176));
	np_title = mk_label(body, FONT_HEADING, PAL.text, "");
	lv_label_set_long_mode(np_title, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_width(np_title, w);
	lv_obj_set_pos(np_title, x, S(204));
	np_sub = mk_label(body, FONT_BODY, PAL.dim, "");
	lv_label_set_long_mode(np_sub, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_width(np_sub, w);
	lv_obj_set_pos(np_sub, x, S(260));
	np_bar = mk_box(body, w, S(6), PAL.line, S(3));
	lv_obj_set_pos(np_bar, x, S(330));
	np_fill = mk_box(np_bar, 0, S(6), PAL.acc, S(3));
	lv_obj_set_pos(np_fill, 0, 0);
	np_pos = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_obj_set_pos(np_pos, x, S(346));
	np_idx = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_obj_set_pos(np_idx, x + w / 2 - S(50), S(346));
	np_len = mk_label(body, FONT_SMALL, PAL.dim, "");
	lv_obj_align_to(np_len, np_bar, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, S(10));
	np_next = mk_label(body, FONT_BODY, PAL.dim, "");
	lv_obj_set_pos(np_next, x, S(420));
	static const char *const hints[] = { "A", "Pause", "\xE2\x86\x94", "Track", "L R", "Seek", "X", "Shuffle", "Y", "Repeat", "B", "Back (keeps playing)", NULL };
	footer_set(hints);
	np_update();
}
static void nowplay_key(int k)
{
	const char *d = NULL;
	char extra[80] = "";
	if (k == K_A) d = "toggle";
	else if (k == K_LEFT) d = "previous";
	else if (k == K_RIGHT) d = "next";
	else if (k == K_L || k == K_R) {
		cJSON *r = api_call("{\"cmd\":\"music\"}");
		cJSON *t = api_ok(r) ? cJSON_GetObjectItem(cJSON_GetObjectItem(r, "music"), "track") : NULL;
		long pos = t ? (long)api_num(t, "pos_ms", 0) : 0;
		cJSON_Delete(r);
		pos += k == K_R ? 10000 : -10000;
		if (pos < 0) pos = 0;
		d = "seek";
		snprintf(extra, sizeof(extra), ",\"ms\":%ld", pos);
	} else if (k == K_X) {
		cJSON *r = api_call("{\"cmd\":\"music\"}");
		int on = !(api_ok(r) && cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(r, "music"), "shuffle")));
		cJSON_Delete(r);
		d = "shuffle";
		snprintf(extra, sizeof(extra), ",\"on\":%s", on ? "true" : "false");
		toast_show(on ? "Shuffle on" : "Shuffle off");
	} else if (k == K_Y) {
		cJSON *r = api_call("{\"cmd\":\"music\"}");
		const char *rp = api_ok(r) ? api_str(cJSON_GetObjectItem(r, "music"), "repeat") : NULL;
		const char *nx = rp && !strcmp(rp, "off") ? "all" : rp && !strcmp(rp, "all") ? "one" : "off";
		d = "repeat";
		snprintf(extra, sizeof(extra), ",\"mode\":\"%s\"", nx);
		toast_show(!strcmp(nx, "all") ? "Repeat the album" : !strcmp(nx, "one") ? "Repeat this track" : "Repeat off");
		cJSON_Delete(r);
	} else if (k == K_B || k == K_MENU) { show_music(); return; }
	if (d) { cJSON_Delete(api_call("{\"cmd\":\"music_ctl\",\"do\":\"%s\"%s}", d, extra)); np_update(); }
}
static void music_tick(void)
{
	if (cur == SCR_NOWPLAY) np_update();
	else if (cur == SCR_MUSIC && !mu_tracks_view) {
		/* a CD named after the lookup, or put in / taken out */
		static int tick;
		if (++tick % 3) return;
		int had_cd = mu_cd != NULL, was_named = mu_cd && cJSON_IsTrue(cJSON_GetObjectItem(mu_cd, "named"));
		cJSON *r = api_call("{\"cmd\":\"music_albums\"}");
		cJSON *c = api_ok(r) ? cJSON_GetObjectItem(r, "cd") : NULL;
		int has = c != NULL, named = c && cJSON_IsTrue(cJSON_GetObjectItem(c, "named"));
		cJSON_Delete(r);
		cJSON *rp = mu_status ? cJSON_GetObjectItem(mu_status, "rip") : NULL;
		int ripping = rp && cJSON_IsTrue(cJSON_GetObjectItem(rp, "busy"));
		static int was_ripping;
		if (has != had_cd || named != was_named) { mu_load(); mu_draw(); }
		else if (ripping || was_ripping) { int sel = mu_list.sel; mu_load(); mu_list.sel = sel; mu_panel(); }
		was_ripping = ripping;
	}
}

/* ---------- Settings › RetroAchievements (phosphord ra.c; achievements run in Phosphor's Main) ----------
 * Log in once (only the token RetroAchievements gives back is kept), Hardcore, the pop-up corner and the
 * unlock sound (read by Main), odelot's cores, the Web API key for the viewer, matching the library. */
static vlist_t ra_list;
enum { RA_ACCOUNT, RA_HC, RA_POP, RA_SND, RA_CORES, RA_KEY, RA_MATCH, RA_VIEW, RA_COUNT };
static cJSON *ras;                 /* phosphord's "ra" */
static char ra_user_typed[80];
static lv_obj_t *ra_note;
static void ra_load(void)
{
	cJSON *r = api_call("{\"cmd\":\"ra\"}");
	cJSON *x = api_ok(r) ? cJSON_DetachItemFromObject(r, "ra") : NULL;
	cJSON_Delete(r);
	if (x) { cJSON_Delete(ras); ras = x; }
}
static int ra_b(const char *k) { return ras && cJSON_IsTrue(cJSON_GetObjectItem(ras, k)); }
static void ra_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	*dim = 0;
	int in = ra_b("logged_in");
	cJSON *job = ras ? cJSON_GetObjectItem(ras, "job") : NULL, *mt = ras ? cJSON_GetObjectItem(ras, "match") : NULL;
	switch (i) {
	case RA_ACCOUNT:
		snprintf(t, n, "Account");
		if (in) snprintf(val, vn, "%s  \xC2\xB7  logged in", api_str(ras, "user") ? api_str(ras, "user") : "");
		else snprintf(val, vn, "Log in  >");
		break;
	case RA_HC: snprintf(t, n, "Hardcore"); snprintf(val, vn, "<  %s  >", ra_b("hardcore") ? "On" : "Off"); *dim = !in; break;
	case RA_POP: {
		const char *p = ras && api_str(ras, "popups") ? api_str(ras, "popups") : "left";
		snprintf(t, n, "Pop-ups"); snprintf(val, vn, "<  Top %s  >", p); *dim = !in; break;
	}
	case RA_SND: snprintf(t, n, "Unlock sound"); snprintf(val, vn, "<  %s  >", ra_b("sound") ? "On" : "Off"); *dim = !in; break;
	case RA_CORES:
		snprintf(t, n, "RetroAchievements cores");
		if (job && cJSON_IsTrue(cJSON_GetObjectItem(job, "busy"))) snprintf(val, vn, "%s", api_str(job, "message") ? api_str(job, "message") : "Getting them...");
		else snprintf(val, vn, "%d of %d  \xC2\xB7  get updates  >", ras ? (int)api_num(ras, "cores_installed", 0) : 0,
		              ras ? cJSON_GetArraySize(cJSON_GetObjectItem(ras, "cores")) : 0);
		break;
	case RA_KEY: snprintf(t, n, "Web API key (for Your progress)"); snprintf(val, vn, ra_b("web_key") ? "set  >" : "add  >"); break;
	case RA_MATCH:
		snprintf(t, n, "Match my library");
		if (mt && api_str(mt, "message") && *api_str(mt, "message")) snprintf(val, vn, "%s", api_str(mt, "message"));
		else snprintf(val, vn, ">");
		*dim = !ra_b("web_key");
		break;
	case RA_VIEW: snprintf(t, n, "Your progress"); snprintf(val, vn, ">"); *dim = !in || !ra_b("web_key"); break;
	}
}
static const char *const RA_HINTS[] = { "A", "Select", "\xE2\x86\x94", "Change", "B", "Back", NULL };
static const char *const RA_HINTS_OUT[] = { "A", "Select", "X", "Log out", "B", "Back", NULL };
static void ra_footer(void) { footer_set(ra_list.sel == RA_ACCOUNT && ra_b("logged_in") ? RA_HINTS_OUT : RA_HINTS); }
static void ra_refresh(void) { if (cur != SCR_RA) return; ra_load(); vlist_refresh(&ra_list); }
static void show_ra(void)
{
	cur = SCR_RA;
	ra_load();
	lv_obj_clean(body);
	screen_title("SETTINGS", "RetroAchievements");
	vlist_create(&ra_list, body, S(56), S(176), SW(700), S(440));
	ra_list.count = RA_COUNT;
	ra_list.fill = ra_fill;
	ra_list.sel = 0;
	ra_list.top = 0;
	vlist_refresh(&ra_list);
	/* 16:9: as designed; narrower (4:3): stacked and as tall as its text, which wraps onto more lines */
	lv_obj_t *panel = mk_box(body, SW(444), NARROW ? LV_SIZE_CONTENT : S(400), PAL.panel, S(16));
	LR_HIDE(panel);
	lv_obj_set_style_border_width(panel, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(panel, PAL.line, 0);
	lv_obj_set_pos(panel, SX(780), S(176));
	if (NARROW) {
		lv_obj_set_style_pad_hor(panel, S(24), 0);
		lv_obj_set_style_pad_ver(panel, S(22), 0);
		lv_obj_set_style_pad_row(panel, S(8), 0);
		lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_max_height(panel, S(494), 0);   /* (to just above the hints) */
	}
	lv_obj_t *h = mk_label(panel, FONT_TITLE, PAL.text, "RetroAchievements");
	if (!NARROW) lv_obj_set_pos(h, S(24), S(22));
	ra_note = mk_label(panel, FONT_BODY, PAL.dim,
		"Log in once: your password goes to RetroAchievements and only the login it gives back is kept. Games on the "
		"RetroAchievements cores get achievements; unlocks pop up in the game.\n\nHardcore: no save states, cheats or slow motion. "
		"Your progress needs your Web API key, from your settings on retroachievements.org (or paste it from Phosphor for Windows).");
	lv_label_set_long_mode(ra_note, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(ra_note, NARROW ? SW(444) - 2 * S(24) : S(396));
	if (!NARROW) lv_obj_set_pos(ra_note, S(24), S(64));
	lv_obj_t *cr = mk_label(panel, FONT_SMALL, PAL.faint, "RetroAchievements support by odelot.\nrcheevos by the RetroAchievements team.");
	if (NARROW) {
		lv_label_set_long_mode(cr, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_set_width(cr, SW(444) - 2 * S(24));
		lv_obj_set_style_margin_top(cr, S(8), 0);
	} else lv_obj_set_pos(cr, S(24), S(340));
	ra_footer();
}
static void ra_pw_done(const char *pw)
{
	if (!pw) { ra_footer(); return; }
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "ra_login");
	cJSON_AddStringToObject(q, "user", ra_user_typed);
	cJSON_AddStringToObject(q, "password", pw);
	char *js = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	toast_show("Logging in...");
	lv_refr_now(NULL);
	cJSON *r = js ? api_call_slow(20000, "%s", js) : NULL;
	if (js) { memset(js, 0, strlen(js)); free(js); }
	if (api_ok(r)) toast_show("Logged in. Get the RetroAchievements cores next, if you haven't.");
	else toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't log in");
	cJSON_Delete(r);
	ra_refresh();
	ra_footer();
}
static void ra_user_done(const char *u)
{
	if (!u || !*u) { ra_footer(); return; }
	snprintf(ra_user_typed, sizeof(ra_user_typed), "%s", u);
	osk_open("RetroAchievements password", "Used once to log in, then forgotten: only the login it gives back is kept.", 1, 64, ra_pw_done, RA_HINTS);
}
static void ra_key_done(const char *k)
{
	if (!k) { ra_footer(); return; }
	cJSON *q = cJSON_CreateObject();
	cJSON_AddStringToObject(q, "cmd", "ra_key");
	cJSON_AddStringToObject(q, "key", k);
	char *js = cJSON_PrintUnformatted(q);
	cJSON_Delete(q);
	cJSON *r = js ? api_call("%s", js) : NULL;
	free(js);
	toast_show(api_ok(r) ? (*k ? "Web API key saved: Match my library next" : "Web API key removed") : (r && api_str(r, "error") ? api_str(r, "error") : "Couldn't save it"));
	cJSON_Delete(r);
	ra_refresh();
	ra_footer();
}
static void ra_set_one(const char *json)
{
	cJSON *r = api_call("{\"cmd\":\"ra_set\",%s}", json);
	if (!api_ok(r)) toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't change it");
	cJSON_Delete(r);
	ra_refresh();
}
static void show_raview(void);
static void ra_key(int k)
{
	if (vlist_key(&ra_list, k)) { ra_footer(); return; }
	int in = ra_b("logged_in");
	int lr = k == K_LEFT || k == K_RIGHT || k == K_A;
	switch (ra_list.sel) {
	case RA_ACCOUNT:
		if (k == K_A && !in) osk_open("RetroAchievements username", "Your username on retroachievements.org", 0, 64, ra_user_done, RA_HINTS);
		else if (k == K_X && in) { cJSON_Delete(api_call("{\"cmd\":\"ra_logout\"}")); toast_show("Logged out: games start on the usual cores again"); ra_refresh(); ra_footer(); }
		break;
	case RA_HC:
		if (lr && in) { int on = !ra_b("hardcore"); ra_set_one(on ? "\"hardcore\":true" : "\"hardcore\":false"); toast_show(on ? "Hardcore on: from the next game you start" : "Hardcore off"); }
		break;
	case RA_POP:
		if (lr && in) {
			static const char *const P[] = { "left", "center", "right" };
			const char *cur_p = api_str(ras, "popups") ? api_str(ras, "popups") : "left";
			int c = !strcmp(cur_p, "center") ? 1 : !strcmp(cur_p, "right") ? 2 : 0;
			c = (c + (k == K_LEFT ? 2 : 1)) % 3;
			char j[64];
			snprintf(j, sizeof(j), "\"popups\":\"%s\"", P[c]);
			ra_set_one(j);
		}
		break;
	case RA_SND:
		if (lr && in) ra_set_one(ra_b("sound") ? "\"sound\":false" : "\"sound\":true");
		break;
	case RA_CORES:
		if (k == K_A) { cJSON *r = api_call("{\"cmd\":\"ra_cores\"}"); toast_show(api_ok(r) ? "Getting odelot's RetroAchievements cores from GitHub" : "Couldn't start"); cJSON_Delete(r); ra_refresh(); }
		break;
	case RA_KEY:
		if (k == K_A) osk_open("Web API key", "From retroachievements.org: Settings, Authentication, Web API key (letters and numbers). Empty removes it.", 0, 64, ra_key_done, RA_HINTS);
		break;
	case RA_MATCH:
		if (k == K_A) {
			if (!ra_b("web_key")) { toast_show("Add your Web API key first"); break; }
			cJSON *r = api_call("{\"cmd\":\"ra_match\"}");
			toast_show(api_ok(r) ? "Matching your library to RetroAchievements..." : "Couldn't start");
			cJSON_Delete(r);
			ra_refresh();
		}
		break;
	case RA_VIEW:
		if (k == K_A) {
			if (!in) toast_show("Log in first");
			else if (!ra_b("web_key")) toast_show("Add your Web API key first");
			else show_raview();
		}
		break;
	}
	if (k == K_B || k == K_MENU) back_to_settings(SET_RA);
}
/* while a download or a match runs: its progress */
static void ra_tick(void)
{
	if (cur != SCR_RA || !ras) return;
	cJSON *job = cJSON_GetObjectItem(ras, "job"), *mt = cJSON_GetObjectItem(ras, "match");
	static int was;
	int busy = cJSON_IsTrue(cJSON_GetObjectItem(job, "busy")) || cJSON_IsTrue(cJSON_GetObjectItem(mt, "busy"));
	if (busy || was) ra_refresh();
	was = busy;
}

/* ---------- Settings › RetroAchievements › Your progress (the viewer) ---------- */
static vlist_t rv_list, rv_ach;
static cJSON *rv, *rv_game;        /* the profile; the achievements of one game (A) */
static lv_obj_t *rv_head, *rv_sub, *rv_title, *rv_info, *rv_desc;
static int rv_mode;                /* 0: games, 1: one game's achievements */
static cJSON *rv_games(void) { return rv ? cJSON_GetObjectItem(rv, "games") : NULL; }
static void rv_panel(void)
{
	cJSON *g = cJSON_GetArrayItem(rv_games(), rv_list.sel);
	if (!g) { lv_label_set_text(rv_title, ""); lv_label_set_text(rv_info, ""); return; }
	lv_label_set_text(rv_title, api_str(g, "title") ? api_str(g, "title") : "");
	char t[300];
	int tot = (int)api_num(g, "total", 0), un = (int)api_num(g, "unlocked", 0);
	snprintf(t, sizeof(t), "%s\n%d of %d achievements (%d%%)%s%s\n%s", api_str(g, "console") ? api_str(g, "console") : "",
	         un, tot, tot ? un * 100 / tot : 0, api_num(g, "hardcore", 0) > 0 ? ", hardcore " : "", "",
	         api_num(g, "game_id", 0) > 0 ? "In your library: X plays it" : "Not matched to your library");
	lv_label_set_text(rv_info, t);
}
static void rv_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	cJSON *g = cJSON_GetArrayItem(rv_games(), i);
	*dim = 0;
	snprintf(t, n, "%s", g && api_str(g, "title") ? api_str(g, "title") : "");
	int tot = g ? (int)api_num(g, "total", 0) : 0, un = g ? (int)api_num(g, "unlocked", 0) : 0;
	const char *aw = g ? api_str(g, "award") : NULL;
	if (aw && (!strcmp(aw, "mastered") || !strcmp(aw, "completed"))) snprintf(val, vn, "%d / %d  \xC2\xB7  %s", un, tot, aw);
	else snprintf(val, vn, "%d / %d  \xC2\xB7  %d%%", un, tot, tot ? un * 100 / tot : 0);
	if (i == rv_list.sel) rv_panel();
}
static void rv_ach_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	cJSON *a = cJSON_GetArrayItem(cJSON_GetObjectItem(rv_game, "achievements"), i);
	int got = a && cJSON_IsTrue(cJSON_GetObjectItem(a, "unlocked"));
	snprintf(t, n, "%s%s", got ? "\xE2\x9C\x93  " : "", a && api_str(a, "title") ? api_str(a, "title") : "");
	snprintf(val, vn, "%d pts%s", a ? (int)api_num(a, "points", 0) : 0, a && cJSON_IsTrue(cJSON_GetObjectItem(a, "hardcore")) ? "  \xC2\xB7  hardcore" : "");
	*dim = !got;
	if (i == rv_ach.sel && rv_desc) lv_label_set_text(rv_desc, a && api_str(a, "description") ? api_str(a, "description") : "");
}
static const char *const RV_HINTS[] = { "A", "Achievements", "X", "Play", "B", "Back", NULL };
static const char *const RV_ACH_HINTS[] = { "\xE2\x86\x95", "Look", "B", "Back", NULL };
static void rv_draw(void)
{
	lv_obj_clean(body);
	rv_desc = NULL;
	char sub[200] = "";
	if (rv) snprintf(sub, sizeof(sub), "%d points  \xC2\xB7  %d achievements  \xC2\xB7  %d games mastered", (int)api_num(rv, "points", 0),
	                 (int)api_num(rv, "achievements", 0), (int)api_num(rv, "mastered", 0));
	screen_title("SETTINGS  \xC2\xB7  RETROACHIEVEMENTS", rv && api_str(rv, "user") ? api_str(rv, "user") : "Your progress");
	rv_sub = mk_label(body, FONT_SMALL, PAL.dim, sub);
	lv_obj_set_pos(rv_sub, S(56), S(168));
	if (rv_mode == 0) {
		vlist_create(&rv_list, body, S(56), S(200), SW(700), S(420));
		lv_obj_t *panel = mk_box(body, SW(444), S(300), PAL.panel, S(16));
		lv_obj_set_style_border_width(panel, LV_MAX(1, S(1)), 0);
		lv_obj_set_style_border_color(panel, PAL.line, 0);
		lv_obj_set_pos(panel, SX(780), S(200));
		LR_HIDE(panel);
		rv_title = mk_label(panel, FONT_TITLE, PAL.text, "");
		lv_label_set_long_mode(rv_title, LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_width(rv_title, SW(396));
		lv_obj_set_pos(rv_title, S(24), S(22));
		rv_info = mk_label(panel, FONT_BODY, PAL.dim, "");
		lv_label_set_long_mode(rv_info, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_set_width(rv_info, SW(396));
		lv_obj_set_pos(rv_info, S(24), S(64));
		rv_list.fill = rv_fill;
		rv_list.count = cJSON_GetArraySize(rv_games());
		vlist_refresh(&rv_list);
		if (!rv_list.count) { lv_label_set_text(rv_title, "No games yet"); lv_label_set_text(rv_info, "Play a game on a RetroAchievements core and it shows here."); }
		footer_set(RV_HINTS);
	} else {
		cJSON *g = cJSON_GetArrayItem(rv_games(), rv_list.sel);
		rv_head = mk_label(body, FONT_TITLE, PAL.text, g && api_str(g, "title") ? api_str(g, "title") : "");
		lv_obj_set_pos(rv_head, S(56), S(196));
		vlist_create(&rv_ach, body, S(56), S(240), W - S(112), S(340));
		rv_ach.fill = rv_ach_fill;
		rv_ach.count = cJSON_GetArraySize(cJSON_GetObjectItem(rv_game, "achievements"));
		rv_desc = mk_label(body, FONT_BODY, PAL.dim, "");
		lv_label_set_long_mode(rv_desc, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_set_width(rv_desc, W - S(112));
		lv_obj_set_pos(rv_desc, S(56), S(600));
		vlist_refresh(&rv_ach);
		footer_set(RV_ACH_HINTS);
	}
}
/* phosphord answers at once with what it last got from RetroAchievements and asks again in the
   background ("updating"): asked again every 1.5 s while it does, for up to 2 minutes, and redrawn */
static int rv_want_profile;
static long long rv_want_game;
static uint32_t rv_poll_since;
static lv_timer_t *rv_timer;
static int rv_fetch_profile(void)       /* 1: still updating */
{
	cJSON *r = api_call("{\"cmd\":\"ra_profile\"}");
	int upd = r && cJSON_IsTrue(cJSON_GetObjectItem(r, "updating"));
	if (api_ok(r)) {
		cJSON_Delete(rv);
		rv = cJSON_DetachItemFromObject(r, "profile");
		int sel = rv_list.sel, top = rv_list.top;
		if (rv_mode == 0) { rv_draw(); rv_list.sel = sel; rv_list.top = top; vlist_refresh(&rv_list); }
	} else if (!upd) toast_show(r && api_str(r, "error") ? api_str(r, "error") : "RetroAchievements didn't answer");
	cJSON_Delete(r);
	return upd;
}
static int rv_fetch_game(long long gid)
{
	cJSON *r = api_call("{\"cmd\":\"ra_game\",\"id\":%lld}", gid);
	int upd = r && cJSON_IsTrue(cJSON_GetObjectItem(r, "updating"));
	if (api_ok(r)) {
		int first = rv_mode != 1;
		int sel = rv_ach.sel, top = rv_ach.top;
		cJSON_Delete(rv_game);
		rv_game = cJSON_DetachItemFromObject(r, "ra");
		rv_mode = 1;
		rv_draw();
		if (!first) { rv_ach.sel = sel; rv_ach.top = top; vlist_refresh(&rv_ach); }
	} else if (!upd) toast_show(r && api_str(r, "error") ? api_str(r, "error") : "RetroAchievements didn't answer");
	cJSON_Delete(r);
	return upd;
}
static void rv_tick(lv_timer_t *t)
{
	(void)t;
	if (cur != SCR_RAVIEW || lv_tick_get() - rv_poll_since > 120000) rv_want_profile = 0, rv_want_game = 0;
	if (rv_want_profile) rv_want_profile = rv_fetch_profile();
	if (rv_want_game) { if (!rv_fetch_game(rv_want_game)) rv_want_game = 0; }
	if (!rv_want_profile && !rv_want_game) { lv_timer_delete(rv_timer); rv_timer = NULL; }
}
static void rv_poll(void)
{
	rv_poll_since = lv_tick_get();
	if (!rv_timer && (rv_want_profile || rv_want_game)) rv_timer = lv_timer_create(rv_tick, 1500, NULL);
}
static void show_raview(void)
{
	cur = SCR_RAVIEW;
	rv_mode = 0;
	rv_want_game = 0;
	cJSON_Delete(rv);
	rv = NULL;
	rv_list.sel = 0;
	rv_list.top = 0;
	rv_want_profile = rv_fetch_profile();
	if (!rv) { toast_show("Asking RetroAchievements..."); rv_draw(); }
	rv_poll();
}
static void raview_key(int k)
{
	if (rv_mode == 1) {
		if (vlist_key(&rv_ach, k)) return;
		if (k == K_B || k == K_MENU) { rv_mode = 0; rv_want_game = 0; rv_draw(); }
		return;
	}
	if (vlist_key(&rv_list, k)) return;
	cJSON *g = cJSON_GetArrayItem(rv_games(), rv_list.sel);
	long long gid = g ? (long long)api_num(g, "game_id", 0) : 0;
	if (k == K_A && g) {
		if (gid <= 0) { toast_show("Not in your library (or not matched yet), so its list isn't here"); return; }
		rv_ach.sel = 0; rv_ach.top = 0;
		rv_want_game = rv_fetch_game(gid) ? gid : 0;
		if (rv_mode != 1) toast_show("Asking RetroAchievements...");
		rv_poll();
	} else if (k == K_X && gid > 0) {
		cJSON *r = api_call("{\"cmd\":\"launch\",\"id\":%lld}", gid);
		toast_show(api_ok(r) ? "Starting..." : (r && api_str(r, "error") ? api_str(r, "error") : "Couldn't start it"));
		cJSON_Delete(r);
	} else if (k == K_B || k == K_MENU) show_ra();
}

/* ---------- the in-game menu ----------
 * A short press of the menu button in a game (a long one opens MiSTer's menu, as before): Phosphor's
 * screen over the running game (Main's "ingame"; the game keeps running behind it) and the controller
 * here, until Back. Needs Phosphor's Main with API 5. */
static vlist_t ig_list;
static int ig_text;                    /* CRT mode: the in-game menu is OSD text (below) */
enum { IG_BACK, IG_ACH, IG_DISC, IG_OSD, IG_QUIT, IG_LOAD };
static int ig_rows[8], ig_n;
/* RetroAchievements in the game being played (Main's "ra"): the Achievements row and its list */
static struct { int on, total, unlocked, points, points_unlocked, hardcore, view; cJSON *list; } igra;
static lv_obj_t *ig_left, *ig_ach_desc;
static vlist_t ig_ach_list;
static void ig_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	static const char *const NAME[] = { "Back to the game", "Achievements", "Change disc", "MiSTer menu", "Quit to Phosphor",
	                                    "Load game" };
	*dim = 0;
	snprintf(t, n, "%s", NAME[ig_rows[i]]);
	if (ig_rows[i] == IG_ACH) snprintf(val, vn, "%d / %d", igra.unlocked, igra.total);
	else snprintf(val, vn, "%s", ig_rows[i] == IG_BACK ? "" : ">");
}
static void ig_ach_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	cJSON *a = cJSON_GetArrayItem(cJSON_GetObjectItem(igra.list, "achievements"), i);
	int got = a && cJSON_IsTrue(cJSON_GetObjectItem(a, "unlocked"));
	snprintf(t, n, "%s%s", got ? "\xE2\x9C\x93  " : "", a && api_str(a, "title") ? api_str(a, "title") : "");
	const char *pr = a ? api_str(a, "progress") : NULL;
	snprintf(val, vn, "%s%s%d pts", pr && *pr && !got ? pr : "", pr && *pr && !got ? "  \xC2\xB7  " : "", a ? (int)api_num(a, "points", 0) : 0);
	*dim = !got;
	if (ig_ach_desc && i == ig_ach_list.sel)
		lv_label_set_text(ig_ach_desc, a && api_str(a, "description") ? api_str(a, "description") : "");
}
/* the left side: the game (box art, what it's about) */
static void ig_left_game(void)
{
	lv_obj_clean(ig_left);
	ig_ach_desc = NULL;
	if (ig_game) {
		int lx = S(56), ly = S(110), aw = S(240), ah = S(300);
		lv_obj_t *art = mk_box(ig_left, aw, ah, lv_color_hex(0x2a2119), S(10));
		lv_obj_set_style_clip_corner(art, true, 0);
		lv_obj_set_pos(art, lx, ly);
		const char *a = api_str(ig_game, "art");
		if (a) {
			static char src[1100];
			snprintf(src, sizeof(src), "A:%s", a);
			lv_obj_t *img = lv_image_create(art);
			lv_obj_set_size(img, aw, ah);
			lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CONTAIN);
			lv_image_set_src(img, src);
			lv_obj_center(img);
		} else {
			lv_obj_t *aic = icon_create(art, ig_from_disc ? ICON_DISC : ICON_CARTRIDGE, S(64));
			lv_obj_set_style_text_color(aic, PAL.acc, 0);
			lv_obj_center(aic);
		}
		/* the text in a column, each part below the last: a title on two lines moves the rest down */
		int tx = lx + aw + S(32), tw = W - S(460) - S(40) - tx;
		lv_obj_t *col = mk_box(ig_left, tw, H - ly - S(110), PAL.bg, 0);
		lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
		lv_obj_set_pos(col, tx, ly);
		lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_row(col, S(12), 0);
		lv_obj_t *t = mk_label(col, FONT_HEADING, PAL.text, api_str(ig_game, "title") ? api_str(ig_game, "title") : "");
		lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_set_width(t, tw);
		lv_obj_update_layout(t);
		int lh = lv_font_get_line_height(FONT_HEADING);
		if (lv_obj_get_height(t) > 2 * lh + S(4)) {      /* at most two lines */
			lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_DOTS);
			lv_obj_set_height(t, 2 * lh);
		}
		char meta[200] = "";
		const char *keys[] = { "year", "developer", "players", "genre" };
		for (int i = 0; i < 4; i++) {
			const char *v = api_str(ig_game, keys[i]);
			if (!v || !*v) continue;
			size_t l = strlen(meta);
			snprintf(meta + l, sizeof(meta) - l, "%s%s%s", l ? "  \xC2\xB7  " : "", v, !strcmp(keys[i], "players") ? (strchr(v, 'p') ? "" : " players") : "");
		}
		if (meta[0]) {
			lv_obj_t *m = mk_label(col, FONT_SMALL, PAL.acc, meta);
			lv_label_set_long_mode(m, LV_LABEL_LONG_MODE_WRAP);
			lv_obj_set_width(m, tw);
		}
		const char *desc = api_str(ig_game, "description");
		if (desc && *desc) {
			lv_obj_t *d = mk_label(col, FONT_BODY, PAL.dim, desc);
			lv_obj_set_width(d, tw);
			lv_obj_set_flex_grow(d, 1);                   /* the rest of the column; "..." where it ends */
			lv_label_set_long_mode(d, LV_LABEL_LONG_MODE_DOTS);
			lv_obj_set_style_margin_top(d, S(8), 0);
		}
	}
}
/* the left side: this game's achievements, unlocked first (Main's "ra list") */
static void ig_left_ach(void)
{
	lv_obj_clean(ig_left);
	cJSON_Delete(igra.list);
	char *l = mainlink_query("ra list");
	igra.list = l ? cJSON_Parse(l) : NULL;
	free(l);
	int n = cJSON_GetArraySize(cJSON_GetObjectItem(igra.list, "achievements"));
	lv_obj_t *e = mk_label(ig_left, FONT_LABEL, PAL.dim, "ACHIEVEMENTS");
	lv_obj_set_style_text_letter_space(e, S(3), 0);
	lv_obj_set_pos(e, S(56), S(96));
	char h[160];
	snprintf(h, sizeof(h), "%d of %d  \xC2\xB7  %d of %d points%s", igra.unlocked, igra.total, igra.points_unlocked, igra.points,
	         igra.hardcore ? "  \xC2\xB7  Hardcore" : "");
	lv_obj_t *hl = mk_label(ig_left, FONT_TITLE, PAL.text, h);
	lv_obj_set_pos(hl, S(56), S(120));
	vlist_create(&ig_ach_list, ig_left, S(48), S(170), W - S(460) - S(96), S(400));
	ig_ach_list.fill = ig_ach_fill;
	ig_ach_list.count = n;
	ig_ach_list.sel = 0;
	ig_ach_list.top = 0;
	ig_ach_desc = mk_label(ig_left, FONT_BODY, PAL.dim, "");
	lv_label_set_long_mode(ig_ach_desc, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_width(ig_ach_desc, W - S(460) - S(112));
	lv_obj_set_pos(ig_ach_desc, S(56), S(590));
	if (!n) lv_label_set_text(ig_ach_desc, igra.list ? "No achievements for this game." : "RetroAchievements didn't answer.");
	vlist_refresh(&ig_ach_list);
	static const char *const hints[] = { "\xE2\x86\x95", "Look", "B", "Back", NULL };
	footer_set(hints);
}
/* ---------- the in-game menu: Load game ----------
 * Load game: first every installed core (phosphord's "system_cores", as in MiSTer's core list;
 * the one running marked), then that core's games (search with Y). B goes back a step each time:
 * games -> cores -> the menu. A on a game asks first ("Press A again"), since the game running stops;
 * then it starts straight from here, no stop at Home. */
enum { IGP_NONE, IGP_GAMES, IGP_CORES };
static struct {
	int view;
	char sys[24], q[64];
	cJSON *items;                     /* games, systems or cores */
	vlist_t list;
	int asked_row;                    /* the row A was pressed on once (-1: none) */
	uint32_t asked_at;
} igp = { .asked_row = -1 };
static void ig_left_game(void);
static void ig_left_pick(void);

static int igp_extra(void) { return 0; }
static cJSON *igp_item(int i) { return cJSON_GetArrayItem(igp.items, i - igp_extra()); }

static void igp_fill(int i, char *t, size_t n, char *val, size_t vn, int *dim)
{
	*dim = 0;
	if (i < igp_extra()) { snprintf(t, n, "Another system"); snprintf(val, vn, ">"); return; }
	cJSON *x = igp_item(i);
	if (igp.view == IGP_GAMES) {
		snprintf(t, n, "%s", x && api_str(x, "title") ? api_str(x, "title") : "");

	} else {
		const char *core = x ? api_str(x, "core") : NULL;
		int on = core && ig_core[0] && !strcasecmp(core, ig_core);
		snprintf(t, n, "%s", x && api_str(x, "name") ? api_str(x, "name") : "");
		snprintf(val, vn, "%s", on ? "Running now" : "");
		*dim = on;
	}
}

static void igp_load(void)
{
	cJSON_Delete(igp.items);
	igp.items = NULL;
	cJSON *r = NULL;
	if (igp.view == IGP_GAMES && igp.q[0]) {
		cJSON *q = cJSON_CreateObject();
		cJSON_AddStringToObject(q, "cmd", "games");
		cJSON_AddStringToObject(q, "system", igp.sys);     /* (that core's games only) */
		cJSON_AddStringToObject(q, "q", igp.q);
		cJSON_AddNumberToObject(q, "limit", 500);
		char *b = cJSON_PrintUnformatted(q);
		cJSON_Delete(q);
		r = api_call("%s", b);
		free(b);
	} else if (igp.view == IGP_GAMES) r = api_call("{\"cmd\":\"games\",\"system\":\"%s\"}", igp.sys);
	else r = api_call("{\"cmd\":\"system_cores\"}");
	const char *key = igp.view == IGP_GAMES ? "games" : "cores";
	if (api_ok(r)) igp.items = cJSON_DetachItemFromObject(r, key);
	else if (r && api_str(r, "error")) toast_show(api_str(r, "error"));
	cJSON_Delete(r);
}

static void igp_open(int view)
{
	igp.view = view;
	igp.asked_row = -1;
	igp.q[0] = igp.sys[0] = 0;
	igp_load();
	if (view == IGP_CORES && !cJSON_GetArraySize(igp.items)) {
		toast_show("No cores are installed");
		igp.view = IGP_NONE;
		return;
	}
	ig_left_pick();
}

static void igp_close(void)
{
	igp.view = IGP_NONE;
	igp.q[0] = 0;
	igp.asked_row = -1;
	cJSON_Delete(igp.items);
	igp.items = NULL;
	ig_left_game();
	static const char *const hints[] = { "A", "Select", "B", "Back to the game", NULL };
	footer_set(hints);
}

static void ig_left_pick(void)
{
	lv_obj_clean(ig_left);
	ig_ach_desc = NULL;
	char head[120];
	if (igp.view == IGP_GAMES && igp.q[0]) snprintf(head, sizeof(head), "%s: \"%s\"", system_name(igp.sys), igp.q);
	else if (igp.view == IGP_GAMES) snprintf(head, sizeof(head), "%s", system_name(igp.sys));
	else snprintf(head, sizeof(head), "Pick a core");
	lv_obj_t *e = mk_label(ig_left, FONT_LABEL, PAL.dim, "LOAD GAME");
	lv_obj_set_style_text_letter_space(e, S(3), 0);
	lv_obj_set_pos(e, S(56), S(96));
	lv_obj_t *hl = mk_label(ig_left, FONT_TITLE, PAL.text, head);
	lv_label_set_long_mode(hl, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_width(hl, W - S(460) - S(112));
	lv_obj_set_pos(hl, S(56), S(120));
	vlist_create(&igp.list, ig_left, S(48), S(170), W - S(460) - S(96), S(460));
	igp.list.fill = igp_fill;
	igp.list.count = cJSON_GetArraySize(igp.items) + igp_extra();
	igp.list.sel = igp.list.top = 0;
	vlist_refresh(&igp.list);
	if (!cJSON_GetArraySize(igp.items)) {
		lv_obj_t *m = mk_label(ig_left, FONT_BODY, PAL.dim, igp.view == IGP_GAMES ? (igp.q[0] ? "Nothing found." : "No games here.") : "Nothing here.");
		lv_obj_set_pos(m, S(56), S(240));
	}
	static const char *const gh[] = { "A", "Load", "Y", "Search", "B", "Back to the cores", NULL };
	static const char *const ch[] = { "A", "Its games", "B", "Back to the menu", NULL };
	footer_set(igp.view == IGP_GAMES ? gh : ch);
}

static void igp_search_done(const char *text)
{
	if (cur != SCR_INGAME || igp.view != IGP_GAMES) return;
	if (!text) { ig_left_pick(); return; }          /* (left the keyboard) */
	snprintf(igp.q, sizeof(igp.q), "%s", text);
	igp_load();
	ig_left_pick();
}

/* A twice on the same row within 4 s: the game running stops, this one starts */
static int igp_confirmed(const char *what)
{
	uint32_t now = lv_tick_get();
	if (igp.asked_row == igp.list.sel && now - igp.asked_at <= 4000) { igp.asked_row = -1; return 1; }
	igp.asked_row = igp.list.sel;
	igp.asked_at = now;
	char m[200];
	snprintf(m, sizeof(m), "Press A again to start %.120s: unsaved progress is lost", what);
	toast_show(m);
	return 0;
}

static void igp_start(long long id, int core)
{
	mainlink_ingame(0);                               /* the game's own picture back, then the new one */
	cJSON *r = core >= 0 ? api_call("{\"cmd\":\"launch\",\"id\":%lld,\"core\":%d}", id, core)
	                     : api_call("{\"cmd\":\"launch\",\"id\":%lld}", id);
	int ok_ = api_ok(r);
	if (!ok_) toast_show(r && api_str(r, "error") ? api_str(r, "error") : "Couldn't start it");
	cJSON_Delete(r);
	if (!ok_) return;
	igp.view = IGP_NONE;
	cJSON_Delete(igp.items);
	igp.items = NULL;
	igp.q[0] = 0;
	igp.sys[0] = 0;
	play_id_quiet(id);
	show_home();
}

static void igp_key(int k)
{
	if (vlist_key(&igp.list, k)) { igp.asked_row = -1; return; }
	if (k == K_B || k == K_MENU) {
		if (igp.view == IGP_GAMES && igp.q[0]) { igp.q[0] = 0; igp_load(); ig_left_pick(); return; }   /* the search off */
		if (igp.view == IGP_GAMES) {                     /* back to the cores, on the one it came from */
			char was[24];
			snprintf(was, sizeof(was), "%s", igp.sys);
			igp.view = IGP_CORES;
			igp.sys[0] = 0;
			igp.asked_row = -1;
			igp_load();
			ig_left_pick();
			for (int i = 0; i < cJSON_GetArraySize(igp.items); i++)
				if (!strcmp(api_str(cJSON_GetArrayItem(igp.items, i), "id") ? api_str(cJSON_GetArrayItem(igp.items, i), "id") : "", was)) { igp.list.sel = i; vlist_refresh(&igp.list); break; }
			return;
		}
		igp_close();                                    /* back to the menu */
		return;
	}
	if (k == K_Y && igp.view == IGP_GAMES && !ig_text) {
		static const char *const back[] = { "A", "Load", "Y", "Search", "B", "Back", NULL };
		osk_open("Search", "Part of a game's name, then Done.", 0, 60, igp_search_done, back);
		return;
	}
	if (k != K_A) return;
	cJSON *x = igp_item(igp.list.sel);
	if (!x) return;
	if (igp.view == IGP_CORES) {                        /* a core: its games */
		snprintf(igp.sys, sizeof(igp.sys), "%s", api_str(x, "id") ? api_str(x, "id") : "");
		igp.view = IGP_GAMES;
		igp.asked_row = -1;
		igp_load();
		ig_left_pick();
	} else if (igp_confirmed(api_str(x, "title") ? api_str(x, "title") : "it"))
		igp_start((long long)api_num(x, "id", -1), -1);
}

/* ---------- CRT mode: the in-game menu as text in MiSTer's OSD ----------
 * With MiSTer's analog port at the TV's own timing (vga_scaler=0) a running core's picture goes out with
 * MiSTer's OSD on top, and never Linux's framebuffer, so Phosphor's in-game menu can't be drawn there.
 * Then the same menu (the same rows, keys and lists) is sent to Phosphor's Main as text, which shows it
 * in the OSD over the game: 16 lines of 30 characters, the highlighted row inverted. */
static void osd_ascii(char *d, size_t n, const char *s)
{
	size_t o = 0;
	while (*s && o + 1 < n) {
		unsigned char c = (unsigned char)*s;
		if (c < 0x80) { d[o++] = (char)(c >= 0x20 ? c : ' '); s++; continue; }
		const char *r = "";
		if (!strncmp(s, "\xC2\xB7", 2)) r = "-";
		else if (!strncmp(s, "\xE2\x9C\x93", 3)) r = "*";
		else if (!strncmp(s, "\xE2\x80\xA6", 3)) r = "...";
		else if (!strncmp(s, "\xE2\x80\x94", 3) || !strncmp(s, "\xE2\x80\x93", 3)) r = "-";
		else if (!strncmp(s, "\xE2\x80\x98", 3) || !strncmp(s, "\xE2\x80\x99", 3)) r = "'";
		else if (!strncmp(s, "\xC3", 1) && s[1]) {               /* Latin-1 letters: the plain letter */
			static const char L[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
			unsigned char k = (unsigned char)s[1];
			if (k >= 0x80 && k <= 0xBF) { char one[2] = { L[k - 0x80], 0 }; snprintf(d + o, n - o, "%s", one); o += strlen(d + o); s += 2; continue; }
		}
		for (; *r && o + 1 < n; r++) d[o++] = *r;
		s++;
		while ((*s & 0xC0) == 0x80) s++;
	}
	d[o] = 0;
}
/* one row: the text, and its value at the right edge (30 characters) */
static void osd_row(char *out, size_t n, const char *t, const char *v)
{
	char a[160], b[64];
	osd_ascii(a, sizeof(a), t);
	osd_ascii(b, sizeof(b), v);
	int bl = (int)strlen(b), room = 29 - (bl ? bl + 1 : 0);
	if (room < 8) { room = 29; bl = 0; b[0] = 0; }
	if ((int)strlen(a) > room) { a[room - 2] = '.'; a[room - 1] = '.'; a[room] = 0; }
	snprintf(out, n, " %-*s%s%s", room, a, bl ? " " : "", b);
}
static void ig_text_draw(void)
{
	if (!ig_text || cur != SCR_INGAME) return;
	char text[1100] = "", line[80], head[160];
	int sel = -1, nl = 0;
#define ADD(str) do { size_t l_ = strlen(text); snprintf(text + l_, sizeof(text) - l_, "%s%s", nl ? "\n" : "", str); nl++; } while (0)
	vlist_t *v = &ig_list;
	if (igp.view) {
		v = &igp.list;
		if (igp.view == IGP_GAMES) snprintf(head, sizeof(head), "Load game: %s", system_name(igp.sys));
		else snprintf(head, sizeof(head), "Load game: pick a core");
	} else if (igra.view) {
		v = &ig_ach_list;
		snprintf(head, sizeof(head), "Achievements %d/%d", igra.unlocked, igra.total);
	} else snprintf(head, sizeof(head), "%s", ig_title[0] ? ig_title : "In game");
	osd_ascii(line, sizeof(line), head);
	char hl[40];
	snprintf(hl, sizeof(hl), " %.29s", line);
	ADD(hl);
	int small = v == &ig_list;                       /* the menu: MiSTer's small OSD (8 lines); a list: 16 */
	if (!small) ADD("");
	int rows = small ? 6 : 12, top = v->sel - rows / 2;
	if (top > v->count - rows) top = v->count - rows;
	if (top < 0) top = 0;
	for (int i = top; i < v->count && i < top + rows; i++) {
		char t[256], val[64] = "";
		int dim = 0;
		v->fill(i, t, sizeof(t), val, sizeof(val), &dim);
		if (v == &ig_list && !strcmp(val, ">")) val[0] = 0;
		osd_row(line, sizeof(line), t, val);
		if (i == v->sel) sel = nl;
		ADD(line);
	}
	if (!v->count) ADD(igp.view ? " Nothing here." : " No achievements here.");
	while (nl < (small ? 7 : 15)) ADD("");
	if (ig_note[0] && lv_tick_get() - ig_note_at < 4000) { osd_ascii(head, sizeof(head), ig_note); snprintf(line, sizeof(line), " %.29s", head); }
	else snprintf(line, sizeof(line), " %s", igp.view || igra.view ? "A select   B back" : "A select   B back to game");
	ADD(line);
#undef ADD
	mainlink_osdmenu(sel, "Phosphor", text);
}

static void ingame_open(void)
{
	if (cur == SCR_INGAME || !mainlink_has_ingame()) return;
	/* anything open over the screen it leaves goes (the menu takes every key) */
	if (osk.open) osk_close(NULL);
	if (pick_panel) picker_close();
	if (ms.menu_box) ms_menu_close();
	if (sg.menu_box) sg_menu_close();
	cur = SCR_INGAME;
	/* RetroAchievements: the game being played has a set (a RetroAchievements core, logged in) */
	memset(&igra, 0, sizeof(igra));
	if (mainlink_has_ra()) {
		char *q = mainlink_query("ra");
		cJSON *r = q ? cJSON_Parse(q) : NULL;
		free(q);
		if (r && api_num(r, "game", 0) > 0 && api_num(r, "total", 0) > 0) {
			igra.on = 1;
			igra.total = (int)api_num(r, "total", 0); igra.unlocked = (int)api_num(r, "unlocked", 0);
			igra.points = (int)api_num(r, "points", 0); igra.points_unlocked = (int)api_num(r, "points_unlocked", 0);
			igra.hardcore = cJSON_IsTrue(cJSON_GetObjectItem(r, "hardcore"));
		}
		cJSON_Delete(r);
	}
	ig_n = 0;
	ig_rows[ig_n++] = IG_BACK;
	if (igra.on) ig_rows[ig_n++] = IG_ACH;
	if (ig_from_disc && dk.enabled) ig_rows[ig_n++] = IG_DISC;
	ig_rows[ig_n++] = IG_LOAD;
	ig_rows[ig_n++] = IG_OSD;
	ig_rows[ig_n++] = IG_QUIT;
	lv_obj_clean(body);
	lv_obj_t *dimbg = mk_box(body, W, H, lv_color_hex(0x000000), 0);
	lv_obj_set_pos(dimbg, 0, 0);
	ig_left = mk_box(body, W - S(460), H, lv_color_hex(0x000000), 0);
	lv_obj_set_style_bg_opa(ig_left, LV_OPA_TRANSP, 0);
	lv_obj_set_pos(ig_left, 0, 0);
	lv_obj_t *sheet = mk_box(body, S(460), H, PAL.panel, 0);
	lv_obj_set_pos(sheet, W - S(460), 0);
	lv_obj_set_style_border_side(sheet, LV_BORDER_SIDE_LEFT, 0);
	lv_obj_set_style_border_width(sheet, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_border_color(sheet, PAL.line, 0);
	/* what's running: Phosphor's Main says which file was loaded into this core (also one chosen in
	   MiSTer's own menu, or another core loaded there), so the menu never shows the game from before */
	{
		char *q = mainlink_query("status");
		cJSON *st = q ? cJSON_Parse(q) : NULL;
		free(q);
		const char *file = api_ok(st) ? api_str(st, "file") : NULL, *core = api_ok(st) ? api_str(st, "core") : NULL;
		if (file && *file) {
			if (strcmp(file, ig_file) || !ig_game) {  /* another file than the one shown (or not found yet): look it up */
				cJSON *q2 = cJSON_CreateObject();
				cJSON_AddStringToObject(q2, "cmd", "game_at");
				cJSON_AddStringToObject(q2, "path", file);
				char *b2 = cJSON_PrintUnformatted(q2);
				cJSON_Delete(q2);
				cJSON *r = b2 ? api_call("%s", b2) : NULL;
				free(b2);
				cJSON *gg = api_ok(r) ? cJSON_DetachItemFromObject(r, "game") : NULL;
				cJSON_Delete(r);
				cJSON_Delete(ig_game);
				ig_game = cJSON_IsObject(gg) ? gg : (cJSON_Delete(gg), NULL);
				ig_from_disc = 0;
				if (ig_game && api_str(ig_game, "title")) snprintf(ig_title, sizeof(ig_title), "%s", api_str(ig_game, "title"));
				else {                                   /* not in the library: its file name */
					const char *b = strrchr(file, '/');
					snprintf(ig_title, sizeof(ig_title), "%.159s", b ? b + 1 : file);
					char *dot = strrchr(ig_title, '.');
					if (dot && dot != ig_title) *dot = 0;
				}
				snprintf(ig_file, sizeof(ig_file), "%s", file);
			}
		} else if (core && *core && ig_core[0] && strcmp(core, ig_core) && !ig_from_disc) {
			cJSON_Delete(ig_game);                       /* another core, no file: just its name */
			ig_game = NULL;
			snprintf(ig_title, sizeof(ig_title), "%s", core);
			ig_file[0] = 0;
		}
		int have_core = core && *core;              /* (core is st's: read before st goes) */
		if (have_core) snprintf(ig_core, sizeof(ig_core), "%s", core);
		if (st) cJSON_Delete(st);
		if (have_core && !ig_game && ig_title[0]) goto ig_known;   /* known to be no library game */
	}
	/* the game on the left: its box art and what it's about (as on its page in the library). Started
	   some other way (the phone, Phosphor for Windows) or the shell restarted: the last game launched */
	if (!ig_game) {
		cJSON *r = api_call("{\"cmd\":\"last\"}");
		cJSON *gg = api_ok(r) ? cJSON_DetachItemFromObject(r, "game") : NULL;
		if (cJSON_IsObject(gg)) { ig_game = gg; if (!ig_title[0] && api_str(gg, "title")) snprintf(ig_title, sizeof(ig_title), "%s", api_str(gg, "title")); }
		else cJSON_Delete(gg);
		cJSON_Delete(r);
	}
ig_known:
	ig_left_game();
	char eb[200];
	snprintf(eb, sizeof(eb), "%s%s%s", ig_title[0] ? ig_title : "In game", ig_from_disc ? "  \xC2\xB7  from the disc" : "",
	         igra.hardcore ? "  \xC2\xB7  Hardcore" : "");   /* RetroAchievements asks for Hardcore to show while playing */
	lv_obj_t *e = mk_label(sheet, FONT_LABEL, PAL.dim, eb);
	lv_obj_set_style_text_letter_space(e, S(2), 0);
	lv_label_set_long_mode(e, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_width(e, S(400));
	lv_obj_set_pos(e, S(30), S(96));
	vlist_create(&ig_list, sheet, S(20), S(140), SW(420), S(300));
	ig_list.count = ig_n;
	ig_list.fill = ig_fill;
	ig_list.sel = 0;
	ig_list.top = 0;
	vlist_refresh(&ig_list);
	static const char *const hints[] = { "A", "Select", "B", "Back to the game", NULL };
	footer_set(hints);
	ui_activity();
	/* Main shows Linux's screen over the game first (it switches consoles, which can wipe what's
	   there), then the menu is drawn once, in full: drawn before, it's lost; drawn twice, it jitters */
	ig_text = crt_active() && mainlink_has_crt();
	if (ig_text) {                                    /* a CRT: the menu in MiSTer's OSD, over the game */
		if (mainlink_ingame_text()) { show_home(); return; }
		ig_note[0] = 0;
		ig_text_draw();
		return;
	}
	if (mainlink_ingame(1)) { show_home(); return; }
	usleep(150000);
	console_quiet();
	lv_obj_invalidate(lv_screen_active());
	lv_refr_now(NULL);
}
static void ingame_close(int to_game)
{
	if (cur != SCR_INGAME) return;
	if (to_game) mainlink_ingame(0);
	show_home();                        /* behind the game, ready for when it ends */
}
static void ingame_key_(int k);
static void ingame_key(int k)
{
	ingame_key_(k);
	ig_text_draw();                                   /* (CRT mode: the OSD text again) */
}
static void ingame_key_(int k)
{
	if (igp.view) { igp_key(k); return; }
	if (igra.view) {                         /* the achievements on the left */
		if (vlist_key(&ig_ach_list, k)) return;     /* (filling the rows sets the highlighted one's description) */
		if (k == K_B || k == K_MENU) {
			igra.view = 0;
			ig_left_game();
			static const char *const hints[] = { "A", "Select", "B", "Back to the game", NULL };
			footer_set(hints);
		}
		return;
	}
	if (vlist_key(&ig_list, k)) return;
	if (k == K_B || k == K_MENU) { ingame_close(1); return; }
	if (k != K_A) return;
	switch (ig_rows[ig_list.sel]) {
	case IG_BACK: ingame_close(1); break;
	case IG_ACH: igra.view = 1; ig_left_ach(); break;
	case IG_DISC:
		disc_eject_now();
		ingame_close(1);
		toast_show("Put the next disc in and close the tray");
		break;
	case IG_OSD: mainlink_ingame_osd(); show_home(); break;
	case IG_LOAD: igp_open(IGP_CORES); break;
	case IG_QUIT: mainlink_quit_game(); show_home(); break;
	}
}

/* a game is running and the shell sleeps (main.c): the menu button (the in-game menu), and Main
   taking the in-game screen back. Also from ui_tick while the menu is open. */
void ui_game_events(void)
{
	if (mainlink_menu_button()) { if (cur == SCR_INGAME) ingame_close(1); else ingame_open(); }
	if (mainlink_ingame_ended() && cur == SCR_INGAME) ingame_close(0);
}
int ui_ingame_open(void) { return cur == SCR_INGAME; }

static uint32_t last_status, last_poll;

int ui_max_sleep(void)
{
	if (cur == SCR_PORTS || cur == SCR_PADTEST || (cur == SCR_MCARDS && ms.pending)) return 50;   /* the card map, the controller test */
	if (cur == SCR_CTRL || cur == SCR_MAP || cur == SCR_CTEST) return 50;       /* the controller, live */
	if (si_pending()) return 100;                             /* a Settings row's status, once it rests */
	return 500;                                               /* status bar, clock, spinners: twice a second */
}

static uint32_t mono_ms_ui(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u); }
/* ---------- the column cards ----------
 * Every screen (but Home, whose tiles are cards themselves) sits on one big card with everything on it. Both start and end at the same height (under the title card,
 * above the button hints, further only if something reaches past), with the same 14 (design) padding, the
 * same rounding and the screen's margin on the outer side; each is as wide as what's on it. A screen's own
 * panels become part of their column (their own fill and border off), so there's never a card on a card.
 * A few lines on one side next to a big column (a label by a row of tabs) go on the big one instead. Kept up
 * to date as things show, hide or change size. */
#define BC_FLAG LV_OBJ_FLAG_USER_2
#define BC_ITEMS 64
typedef struct { lv_area_t a; int side; } bc_item_t;
static int bc_is_card(lv_obj_t *o)
{
	if (lv_obj_get_style_bg_opa(o, 0) <= LV_OPA_50) return 0;
	lv_color_t c = lv_obj_get_style_bg_color(o, 0);
	return lv_color_eq(c, PAL.panel) || lv_color_eq(c, PAL.panel_on);
}
static int bc_collect(lv_obj_t *o, bc_item_t *it, int n, int depth)
{
	if (n >= BC_ITEMS || lv_obj_is_hidden(o) || lv_obj_has_flag(o, LV_OBJ_FLAG_USER_1) || lv_obj_has_flag(o, BC_FLAG)) return n;
	lv_area_t a;
	lv_obj_get_coords(o, &a);
	if (lv_area_get_width(&a) <= 1 || lv_area_get_height(&a) <= 1) return n;
	if (lv_obj_get_style_bg_opa(o, 0) > LV_OPA_50 && lv_color_eq(lv_obj_get_style_bg_color(o, 0), PAL.bg) &&
	    !lv_obj_check_type(o, &lv_label_class) && !lv_obj_check_type(o, &lv_image_class))
		lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);    /* (a black box on the card: the card shows through) */
	if (depth == 0 && bc_is_card(o) && lv_color_eq(lv_obj_get_style_bg_color(o, 0), PAL.panel)) {
		/* a screen's own panel: part of its column from now on */
		lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
		lv_obj_set_style_border_width(o, 0, 0);
		lv_obj_set_style_shadow_width(o, 0, 0);
	} else if (!bc_is_card(o)) {
		uint32_t nc = lv_obj_get_child_count(o);
		if (nc && depth < 2 && !lv_obj_check_type(o, &lv_label_class) && lv_obj_get_style_bg_opa(o, 0) == LV_OPA_TRANSP &&
		    lv_obj_get_style_border_width(o, 0) == 0) {
			for (uint32_t i = 0; i < nc; i++) n = bc_collect(lv_obj_get_child(o, (int32_t)i), it, n, depth + 1);
			return n;                                      /* (a holder: what's in it) */
		}
		if (lv_obj_check_type(o, &lv_label_class)) {    /* a label: just its text (its box is often bigger) */
			const char *t = lv_label_get_text(o);
			if (!t || !t[0]) return n;
			lv_point_t sz;
			int ow = lv_area_get_width(&a);
			lv_text_get_size(&sz, t, lv_obj_get_style_text_font(o, 0), lv_obj_get_style_text_letter_space(o, 0),
			                 lv_obj_get_style_text_line_space(o, 0), lv_label_get_long_mode(o) == LV_LABEL_LONG_MODE_WRAP ? ow : LV_COORD_MAX, LV_TEXT_FLAG_NONE);
			int tw = LV_MIN(sz.x, ow), th = LV_MIN(sz.y, lv_area_get_height(&a));
			lv_text_align_t al = lv_obj_get_style_text_align(o, 0);
			if (al == LV_TEXT_ALIGN_RIGHT) a.x1 = a.x2 - tw + 1;
			else if (al == LV_TEXT_ALIGN_CENTER) { int m = (a.x1 + a.x2) / 2; a.x1 = m - tw / 2; a.x2 = a.x1 + tw - 1; }
			else a.x2 = a.x1 + tw - 1;
			a.y2 = a.y1 + th - 1;
		}
	}
	it[n].a = a;
	it[n].side = (a.x1 + a.x2) / 2 >= W / 2;
	return n + 1;
}
static void body_cards(void)
{
	if (!body) return;
	lv_obj_t *card[2] = { NULL, NULL };
	uint32_t nc = lv_obj_get_child_count(body);
	for (uint32_t i = 0; i < nc; i++) {
		lv_obj_t *o = lv_obj_get_child(body, (int32_t)i);
		if (lv_obj_has_flag(o, BC_FLAG)) { int sd = (int)(intptr_t)lv_obj_get_user_data(o); if (sd == 0 || sd == 1) card[sd] = o; }
	}
	if (theme_lr || cur == SCR_HOME) { for (int k = 0; k < 2; k++) if (card[k]) lv_obj_set_hidden(card[k], true); return; }
	bc_item_t it[BC_ITEMS];
	int n = 0;
	for (uint32_t i = 0; i < nc; i++) n = bc_collect(lv_obj_get_child(body, (int32_t)i), it, n, 0);
	lv_area_t u[2];
	int have[2] = { 0, 0 };
	for (int i = 0; i < n; i++) {
		int sd = it[i].side;
		lv_area_t *b = &it[i].a;
		if (!have[sd]) { u[sd] = *b; have[sd] = 1; continue; }
		u[sd].x1 = LV_MIN(u[sd].x1, b->x1); u[sd].y1 = LV_MIN(u[sd].y1, b->y1);
		u[sd].x2 = LV_MAX(u[sd].x2, b->x2); u[sd].y2 = LV_MAX(u[sd].y2, b->y2);
	}
	lv_area_t bc;
	lv_obj_get_coords(body, &bc);
	int pad = S(14), top = bc.y1 + S(162), bottom = bc.y1 + H - S(68);
	/* the two halves touch or cross (a wide list), or one is just a few lines: one card */
	if (have[0] && have[1]) {
		int small1 = lv_area_get_height(&u[1]) < (bottom - top) / 4, small0 = lv_area_get_height(&u[0]) < (bottom - top) / 4;
		/* (the controller test: its info and its drawing on one big card) */
		/* always one card: deciding by what's showing flipped between one and two as text came and went */
		(void)small0; (void)small1;
		{
			u[0].x1 = LV_MIN(u[0].x1, u[1].x1); u[0].y1 = LV_MIN(u[0].y1, u[1].y1);
			u[0].x2 = LV_MAX(u[0].x2, u[1].x2); u[0].y2 = LV_MAX(u[0].y2, u[1].y2);
			have[1] = 0;
		}
	} else if (have[1] && !have[0]) { u[0] = u[1]; have[0] = 1; have[1] = 0; }
	for (int k = 0; k < 2; k++) if (have[k]) { top = LV_MIN(top, u[k].y1 - pad); bottom = LV_MAX(bottom, u[k].y2 + pad); }
	for (uint32_t i = 0; i < nc; i++) {                  /* (never up into the title's card) */
		lv_obj_t *o = lv_obj_get_child(body, (int32_t)i);
		if (!lv_obj_has_flag(o, LV_OBJ_FLAG_USER_1) || lv_obj_is_hidden(o)) continue;
		lv_area_t ta;
		lv_obj_get_coords(o, &ta);
		top = LV_MAX(top, ta.y2 + S(10));
	}
	for (int k = 0; k < 2; k++) {
		if (!have[k]) { if (card[k]) lv_obj_set_hidden(card[k], true); continue; }
		if (!card[k]) {
			card[k] = mk_box(body, 10, 10, PAL.panel, S(16));
			lv_obj_add_flag(card[k], BC_FLAG);
			lv_obj_set_user_data(card[k], (void *)(intptr_t)k);
			lv_obj_move_background(card[k]);
		}
		int x1 = k == 0 ? LV_MIN(bc.x1 + S(42), u[k].x1 - pad) : u[k].x1 - pad;
		int x2 = k == 1 || u[k].x2 + pad > bc.x1 + W - S(42) - S(60) ? bc.x1 + W - S(42) - 1 : u[k].x2 + pad;
		if (k == 0 && !have[1]) x2 = bc.x1 + W - S(42) - 1;   /* (one card: the screen's width, as on every screen) */
		int x = x1 - bc.x1, y = top - bc.y1, w = x2 - x1 + 1, h = bottom - top + 1;
		if (lv_obj_get_x(card[k]) != x || lv_obj_get_y(card[k]) != y) lv_obj_set_pos(card[k], x, y);
		if (lv_obj_get_width(card[k]) != w || lv_obj_get_height(card[k]) != h) lv_obj_set_size(card[k], w, h);
		lv_obj_set_hidden(card[k], false);
	}
}

void ui_tick(void)
{
	uint32_t now = lv_tick_get();
	/* where a slow pass went (for the log: the menu not answering the controller) */
	static const char *tn[16]; static uint32_t tt[16]; int ti = 0; uint32_t tlast = mono_ms_ui(), tstart = tlast;
#define TT(name) do { uint32_t t_ = mono_ms_ui(); if (ti < 16) { tn[ti] = (name); tt[ti++] = t_ - tlast; } tlast = t_; } while (0)
	{ static uint32_t lu; if (now - lu >= 500) { lu = now; upd_poll(); } }
	{ static uint32_t lb; if (now - lb >= 200) { lb = now; body_cards(); } }
	TT("upd");
	if (us.shown) { ui_activity(); return; }        /* an update has the screen: nothing else asks phosphord */
	if (cur == SCR_SCAN) { ui_activity(); scan_tick(now); }   /* (no screensaver over it) */
	if (cur == SCR_ART) { ui_activity(); art_tick(now); }
	if (cur == SCR_DCOPY) { ui_activity(); static uint32_t ld; if (now - ld >= 500) { ld = now; dcopy_update(); } }
	if (cur == SCR_MAP) { ui_activity(); static uint32_t lm; if (now - lm >= 100) { lm = now; map_tick(); } }
	if (cur == SCR_CTEST) { ui_activity(); static uint32_t lt; if (now - lt >= 60) { lt = now; tw_poll(); } }
	if (cur == SCR_CTRL) {
		static uint32_t lc, lr;
		if (now - lc >= 100 && ct.mode == 0 && ct_list.sel < ct.n) { lc = now; ct_live(ct.pv, ct.ev[ct_list.sel], ct_list.sel, NULL, NULL, 0); }
		if (now - lr >= 2000) { lr = now; api_bg_ask("ctrls", "{\"cmd\":\"controllers\"}"); }   /* plugged in, switched on */
		{ cJSON *cr_ = api_bg_take("ctrls"); if (cr_) ct_refresh(cr_); }
	}
	{ static uint32_t lq; if (now - lq >= 250) { lq = now; map_requests(); } }
	TT("screen");
	/* the dock's drive changed; the menu button in a game; Main took the in-game screen back */
	if (mainlink_disc_changed()) disc_refresh();
	if (mainlink_bt_pair() && cur != SCR_MAP) {          /* the SS1's Bluetooth button: Phosphor's own pairing (not over a
	                                                        mapping in progress: MiSTer's Main is in the middle of it) */
		saver_hide();
		if (pm.open) pm_close();
		show_bt();
		if (bt.present && !bt.busy) bt_do("pair", NULL);
		else if (!bt.present) toast_show("No Bluetooth adapter found");
	}
	{ static uint32_t la; if (dk.in && !dk.art[0] && !strcmp(dk.state, "ready") && dk.tries < 6 && now - la >= 3000 && (cur == SCR_SYSTEMS || cur == SCR_HOME)) { la = now; dk.tries++; disc_refresh(); } }
	ui_game_events();
	TT("maps");
	vk_check(0);
	TT("events");
	vk_tick();
	if (si_pending()) si_update();
	TT("vk");
	/* the alerts, once after start-up: one at a time, 1.5 s apart (each asks phosphord, and the menu waits
	   for its answer: all four at once froze it for seconds at a busy start-up) */
	{ static int step; static uint32_t next = 5000;
	  if (step < 4 && now > next) { switch (step++) { case 0: guard_check(); break; case 1: power_check(); break; case 2: update_check(); break; default: sdrescue_check(); } next = now + 1500; } }
	{ static int wall_done; if (!wall_done && now > 3000) { wall_done = 1; if (SETTINGS.wallpaper_on) bg_apply(root); } }   /* (phosphord up by now) */
	wall_tick();
	TT("info+alerts+wall");
	/* Phosphor's releases on GitHub: a minute after start-up, then every 30 min, never while a game runs
	   (phosphord asks in the background; its answer is read 20 s later) */
	{
		static uint32_t next = 60000, follow;
		if (now >= next && !mainlink_in_game()) {
			next = now + 30u * 60u * 1000u;
			cJSON_Delete(api_call("{\"cmd\":\"update\",\"do\":\"check\"}"));
			follow = now + 20000;
		}
		if (follow && now >= follow) { follow = 0; cJSON *r = api_call("{\"cmd\":\"update\"}"); upo_apply(r); cJSON_Delete(r); }
	}
	saver_tick(now);
	TT("updcheck");
	const char *note = input_notice();
	if (note) toast_show(note);
	if (now - last_status > 2000 || last_status == 0) { status_update(); last_status = now ? now : 1; }
	TT("saver");
	/* the controllers on the front ports, live */
	TT("status");
	static uint32_t last_pads;
	if (cur == SCR_PORTS && pt.available && now - last_pads >= 100) { last_pads = now; pt_pads(); }
	/* a card job: the map fills in as it goes */
	static uint32_t last_mc;
	if (cur == SCR_PORTS && pt.busy && now - last_mc >= 250) { last_mc = now; pt_apply(api_call("{\"cmd\":\"memcard\"}")); }
	if (cur == SCR_MCARDS && ms.pending && now - last_mc >= 250) { last_mc = now; ms_poll(); }
	if (cur == SCR_PADTEST && now - last_pads >= 50) { last_pads = now; padtest_poll(); }
	/* the library changed (a scan finished): refresh what's on screen */
	TT("ports");
	if (now - last_poll >= 1000 || last_poll == 0) {
		last_poll = now ? now : 1;
		power_requests();
		if (cur == SCR_SETTINGS) { scrape_refresh(); cores_refresh(); vlist_refresh(&set_list); si_update(); }
		gd_tick();                                    /* (only while one of its jobs runs) */
		/* the Welcome screen's Network line follows WiFi coming up by itself */
		{ static int wl_tick; if (cur == SCR_WELCOME && !osk.open && ++wl_tick % 3 == 0) { wl_refresh_net(); vlist_refresh(&wl_list); } }
		if (cur == SCR_BACKUP && bk.busy) bk_apply(api_call("{\"cmd\":\"backup\"}"));
		if (cur == SCR_STORAGE && (sg.busy || sg.bk_busy || sg.nqueue)) sg_refresh();
		if (cur == SCR_PUPD) { static int ut; if (up.busy || ++ut % 3 == 0) up_apply(api_call("{\"cmd\":\"update\"}")); }
		if (cur == SCR_BT) { static int bt_tick; if (bt.busy || ++bt_tick % 3 == 0) bt_apply(api_call("{\"cmd\":\"bluetooth\"}")); }
		/* (a card job is followed 4 times a second, below); otherwise the ports' state every 2 s, so the
		   screen is right again by itself after Main restarts */
		{ static int pt_tick; if (cur == SCR_PORTS && !pt.busy && ++pt_tick % 2 == 0) pt_apply(api_call("{\"cmd\":\"memcard\"}")); }
		if (cur == SCR_CLOUD && cl.busy && !osk.open) cl_apply(api_call("{\"cmd\":\"cloud\"}"));
		if (cur == SCR_RA && !osk.open) ra_tick();
		if (cur == SCR_NOWPLAY || cur == SCR_MUSIC) music_tick();
		if (cur == SCR_REMOTE && pair_box) remote_apply(api_call("{\"cmd\":\"remote\"}"));
		if (cur == SCR_DIAG && diag_busy[0]) diag_apply(api_call("{\"cmd\":\"diag\"}"));
		if (cur == SCR_WIFI && !osk.open) { static int tick; if (wscanning || ++tick % 5 == 0) wifi_apply(api_call("{\"cmd\":\"wifi\"}")); }
	}
	TT("polls");
	if (tlast - tstart >= 100) {
		char b[300]; size_t o = 0;
		for (int i = 0; i < ti && o < sizeof(b) - 40; i++) if (tt[i] >= 20) o += (size_t)snprintf(b + o, sizeof(b) - o, " %s %u", tn[i], tt[i]);
		plog("slow: timers %u ms:%s (screen %d)", tlast - tstart, b, (int)cur);
	}
#undef TT
}

void ui_init(int w, int h)
{
	/* CRT mode: overscan keeps everything inside the part of the picture the TV shows (the edges black);
	   W and H are then that part (a rebuild passes them back: the screen's own size is kept here) */
	static int scr_w, scr_h;
	if (!scr_w) { scr_w = w; scr_h = h; }
	w = scr_w;
	h = scr_h;
	int ox = 0, oy = 0;
	if (crt_active() && SETTINGS.crt_overscan > 0) {
		ox = w * SETTINGS.crt_overscan / 100;
		oy = h * SETTINGS.crt_overscan / 100;
		w -= 2 * ox;
		h -= 2 * oy;
	}
	theme_crt_font = SETTINGS.crt_font;
	W = w;
	H = h;
	theme_set_scale(w, h);
	lv_obj_t *scr = lv_screen_active();
	lv_obj_set_style_bg_color(scr, ox || oy ? lv_color_hex(0) : PAL.bg, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	if (!root) {
		root = mk_box(scr, w, h, PAL.bg, 0);
		lv_obj_set_pos(root, ox, oy);
	}
	lv_obj_set_style_bg_color(root, PAL.bg, 0);
	bg_apply(root);                                      /* redrawn in the accent colour (a texture) */
	mainlink_shot_combo(!SETTINGS.shot_off);
	upd_note();
	status_bar();
	body = mk_box(root, w, h, PAL.bg, 0);
	lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
	lv_obj_move_background(body);
	footer = mk_box(root, LV_SIZE_CONTENT, theme_lr ? L(22) : LV_SIZE_CONTENT, PAL.panel, theme_lr ? L(4) : S(14));
	if (!theme_lr) lv_obj_set_style_pad_ver(footer, S(10), 0);
	lv_obj_set_style_max_width(footer, theme_lr ? w - L(32) : w - S(112), 0);   /* (a card: as wide as its hints) */
	lv_obj_set_style_pad_hor(footer, theme_lr ? L(6) : S(16), 0);
	lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_column(footer, theme_lr ? L(10) : S(28), 0);
	lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, theme_lr ? L(12) : S(42), theme_lr ? 0 : -S(8));

	if (toast_timer) { lv_timer_delete(toast_timer); toast_timer = NULL; }   /* (a rebuild: the old toast's) */
	toast = mk_box(root, LV_SIZE_CONTENT, theme_lr ? L(20) : S(44), lv_color_hex(0x1f1f25), theme_lr ? L(4) : S(12));
	lv_obj_set_style_border_width(toast, 1, 0);
	lv_obj_set_style_border_color(toast, PAL.acc, 0);
	lv_obj_set_style_pad_hor(toast, theme_lr ? L(8) : S(20), 0);
	lv_obj_t *tl = mk_label(toast, FONT_SMALL, PAL.text, "");
	lv_obj_center(tl);
	lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, theme_lr ? -L(26) : -S(80));
	lv_obj_set_hidden(toast, true);

	status_update();
	alert_refresh();                          /* a rebuild (new accent): the pill and the disc as they were */
	if (sb_disc) lv_obj_set_hidden(sb_disc, !dk.in);
	if (SETTINGS.setup_done) show_home();
	else show_welcome();
	const char *reopen = getenv("PHOS_REOPEN");          /* started again from Settings > Video: back there */
	if (reopen && !strcmp(reopen, "video") && SETTINGS.setup_done) show_video();
	if (reopen && !strcmp(reopen, "update") && SETTINGS.setup_done) show_update();   /* (tests: --snapshot) */
	unsetenv("PHOS_REOPEN");
	static int crt_checked;
	crt_cfg_t prev, now;
	if (!crt_checked && crt_pending(&prev)) {        /* started again for a CRT mode try: Keep, or it goes back */
		char what[80];
		crt_load(&now);
		snprintf(what, sizeof(what), "CRT mode: %s", crt_mode_name(now.on ? now.mode : CRT_OFF));
		vk_box(what, CRT_KEEP_SECS, CRT_KEEP_SECS);
		vk.crt = 1;
	}
	crt_checked = 1;
}
