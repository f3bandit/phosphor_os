/* Phosphor frontend: the offline drive repair's screen (the SD card, then the other drives).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Only in the repair copy of MiSTer's Linux on the repair drive (the storage module's sdrescue.sh puts this
 * program, Phosphor's Main and the repair step in it): phosphor --sdrepair STATE. No phosphord, no library:
 * one screen, drawn from the repair step's state file (key=value lines, read twice a second):
 *   phase=repairing|done   drive=NVMe drive|USB drive
 *   result=clean|repaired|errors_left|failed   reason=<why it didn't run>
 *   dirs=N files=N secs=N fixed=<what the checker fixed>   restart=<seconds until the restart>
 *   step=<the drive being repaired> stepn=N steps=N   (repairing: "NVMe drive · 2 of 3")
 *   d1..d3=<drive>|clean|repaired|errors_left|failed|skipped   others_bad=1   (done: the other drives)
 * The repair step restarts the SuperStation itself; this only shows what's happening. */
#include "sdrepair.h"
#include "icons.h"
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define KVN 4
static const char *state_path;
static struct {
	char phase[16], drive[40], result[16], reason[120], fixed[120], dirs[16], files[16], secs[16], step[48];
	char d[3][72];
	int restart, stepn, steps, others_bad;
} st;
static char last_raw[1024];
static lv_obj_t *ic, *lab, *title, *body, *bar, *bar_fill, *sub, *note, *note_txt, *note_ic, *kv, *kv_k[KVN], *kv_v[KVN], *foot;
static int bar_x, bar_w, fill_w;
static time_t done_at;

static lv_color_t C(uint32_t hex) { return lv_color_hex(hex); }
#define GREEN 0x5cf08a
#define RED   0xff6b5e
#define AMBER 0xffb347

static void read_state(void)
{
	FILE *f = fopen(state_path, "r");
	char raw[1024] = "";
	if (f) { size_t n = fread(raw, 1, sizeof(raw) - 1, f); raw[n] = 0; fclose(f); }
	if (!strcmp(raw, last_raw)) return;
	snprintf(last_raw, sizeof(last_raw), "%s", raw);
	memset(&st, 0, sizeof(st));
	snprintf(st.phase, sizeof(st.phase), "repairing");
	char *save = NULL;
	for (char *l = strtok_r(raw, "\n", &save); l; l = strtok_r(NULL, "\n", &save)) {
		char *eq = strchr(l, '=');
		if (!eq) continue;
		*eq = 0;
		const char *k = l, *v = eq + 1;
#define SET(name) if (!strcmp(k, #name)) snprintf(st.name, sizeof(st.name), "%s", v)
		SET(phase); SET(drive); SET(result); SET(reason); SET(fixed); SET(dirs); SET(files); SET(secs); SET(step);
#undef SET
		if (!strcmp(k, "restart")) st.restart = atoi(v);
		if (!strcmp(k, "stepn")) st.stepn = atoi(v);
		if (!strcmp(k, "steps")) st.steps = atoi(v);
		if (!strcmp(k, "others_bad")) st.others_bad = atoi(v);
		if (k[0] == 'd' && k[1] >= '1' && k[1] <= '3' && !k[2]) snprintf(st.d[k[1] - '1'], sizeof(st.d[0]), "%s", v);
	}
	if (!strcmp(st.phase, "done") && !done_at) done_at = time(NULL);
}

/* 25716 -> "25,716" */
static void commas(const char *n, char *out, size_t on)
{
	size_t l = strlen(n), j = 0;
	for (size_t i = 0; i < l && j + 2 < on; i++) {
		out[j++] = n[i];
		if ((l - i - 1) % 3 == 0 && i + 1 < l) out[j++] = ',';
	}
	out[j] = 0;
}

static void kv_row(int i, const char *k, const char *v)
{
	lv_label_set_text(kv_k[i], k);
	lv_label_set_text(kv_v[i], v);
	lv_obj_set_hidden(kv_k[i], !*k);
	lv_obj_set_hidden(kv_v[i], !*k);
}

static void draw(void)
{
	char b[200], c1[24], c2[24];
	int done = !strcmp(st.phase, "done");
	const char *r = st.result;
	lv_obj_set_hidden(bar, done);
	lv_obj_set_hidden(sub, done);
	lv_obj_set_hidden(note, done && strcmp(r, "errors_left"));
	lv_obj_align(note, LV_ALIGN_BOTTOM_MID, 0, -S(done ? 140 : 56));   /* (above the countdown when done) */
	lv_obj_set_hidden(kv, !done || (strcmp(r, "clean") && strcmp(r, "repaired")));
	lv_obj_set_hidden(foot, !done);
	for (int i = 0; i < KVN; i++) kv_row(i, "", "");
	int multi = st.steps > 1 || st.d[0][0];
	if (!done) {
		icon_set(ic, ICON_SDCARD);
		lv_obj_set_style_text_color(ic, PAL.acc, 0);
		lv_label_set_text(lab, "O F F L I N E   R E P A I R");
		lv_label_set_text(title, multi ? "Repairing the drives" : "Repairing the SD card");
		snprintf(b, sizeof(b), multi ? "Started from the %s so the SD card and the other drives can be checked while nothing is using them. Phosphor starts again by itself when it's done."
		                             : "Started from the %s so the SD card can be checked while nothing is using it. Phosphor starts again by itself when it's done.",
			st.drive[0] ? st.drive : "repair drive");
		lv_label_set_text(body, b);
		if (multi && st.step[0]) { snprintf(b, sizeof(b), "%s \xC2\xB7 %d of %d", st.step, st.stepn, st.steps); lv_label_set_text(sub, b); }
		else lv_label_set_text(sub, "Checking files and folders \xC2\xB7 usually under a minute");
		lv_label_set_text(note_txt, "Don't switch off or unplug the SuperStation");
		icon_set(note_ic, ICON_POWER);
		lv_obj_set_style_text_color(note_ic, PAL.acc, 0);
		lv_obj_set_style_bg_color(note, C(0x2a1f12), 0);
		lv_obj_set_style_border_color(note, C(0x6b4a1f), 0);
		return;
	}
	lv_label_set_text(lab, "O F F L I N E   R E P A I R   \xC2\xB7   D O N E");
	if ((!strcmp(r, "clean") || !strcmp(r, "repaired")) && multi) {
		/* the SD card and the other drives, one row each */
		int fixed = !strcmp(r, "repaired");
		for (int i = 0; i < 3 && st.d[i][0]; i++) if (strstr(st.d[i], "|repaired")) fixed = 1;
		int bad = st.others_bad;
		icon_set(ic, bad ? ICON_WARN : fixed ? ICON_UTILITY : ICON_DONE);
		lv_obj_set_style_text_color(ic, C(bad ? AMBER : GREEN), 0);
		lv_label_set_text(title, bad ? "Some errors couldn't be fixed here" : fixed ? "The drives are repaired" : "All drives are fine");
		lv_label_set_text(body, bad ? "Phosphor still starts, and what it fixed stays fixed. For a drive with errors left, back up what's on it and repair it on a PC."
		                            : fixed ? "The errors found are fixed. Your games, saves and settings are where they were, and the drives are marked clean again."
		                                    : "No errors found. They're marked clean again, so Windows won't ask to scan them.");
		lv_obj_set_hidden(note, true);
		kv_row(0, "SD card", fixed && !strcmp(r, "repaired") ? "Repaired" : "No errors");
		for (int i = 0; i < 3 && st.d[i][0]; i++) {
			char name[48];
			const char *bar_ = strrchr(st.d[i], '|'), *res = bar_ ? bar_ + 1 : "";
			snprintf(name, sizeof(name), "%.*s", bar_ ? (int)(bar_ - st.d[i]) : 40, st.d[i]);
			kv_row(i + 1, name, !strcmp(res, "clean") ? "No errors" : !strcmp(res, "repaired") ? "Repaired"
			                  : !strcmp(res, "errors_left") ? "Errors left" : !strcmp(res, "skipped") ? "Not checked here" : "Not repaired");
		}
	} else if (!strcmp(r, "clean") || !strcmp(r, "repaired")) {
		int fixed = !strcmp(r, "repaired");
		icon_set(ic, fixed ? ICON_UTILITY : ICON_DONE);
		lv_obj_set_style_text_color(ic, C(GREEN), 0);
		lv_label_set_text(title, fixed ? "The SD card is repaired" : "The SD card is fine");
		lv_label_set_text(body, fixed ? "The file system had errors and they're fixed. Your games, saves and settings are where they were. It's marked clean again."
		                              : "No errors found. It's marked clean again, so Windows won't ask to scan it.");
		int n = 0;
		if (fixed && st.fixed[0]) kv_row(n++, "Fixed", st.fixed);
		if (st.dirs[0] && st.files[0]) {
			commas(st.dirs, c1, sizeof(c1)); commas(st.files, c2, sizeof(c2));
			snprintf(b, sizeof(b), "%s folders, %s files", c1, c2);
			kv_row(n++, "Checked", b);
		}
		if (!fixed && st.secs[0]) { snprintf(b, sizeof(b), "%s second%s", st.secs, strcmp(st.secs, "1") ? "s" : ""); kv_row(n++, "Took", b); }
		if (fixed) kv_row(n++, "Full report", "Settings \xE2\x80\xBA Storage \xE2\x80\xBA Repair all drives");
		if (!n) lv_obj_set_hidden(kv, true);
	} else if (!strcmp(r, "errors_left")) {
		icon_set(ic, ICON_WARN);
		lv_obj_set_style_text_color(ic, C(AMBER), 0);
		lv_label_set_text(title, "Some errors couldn't be fixed here");
		lv_label_set_text(body, "Phosphor still starts, and what it fixed stays fixed. For the rest, put the card in a PC and use Phosphor for Windows: SD card flash & health \xE2\x80\xBA Card health \xE2\x80\xBA Deep repair.");
		lv_label_set_text(note_txt, "Back up your saves first: Settings \xE2\x80\xBA Storage \xE2\x80\xBA Back up saves & settings");
		icon_set(note_ic, ICON_UPDATE);
		lv_obj_set_style_text_color(note_ic, C(0x6cb6ff), 0);
		lv_obj_set_style_bg_color(note, PAL.panel, 0);
		lv_obj_set_style_border_color(note, PAL.line, 0);
	} else {
		icon_set(ic, ICON_FAILED);
		lv_obj_set_style_text_color(ic, C(RED), 0);
		lv_label_set_text(lab, "O F F L I N E   R E P A I R");
		lv_label_set_text(title, "The repair didn't run");
		snprintf(b, sizeof(b), "%s%s Nothing on it was changed. Try again from Settings \xE2\x80\xBA Storage, or repair it on a PC with Phosphor for Windows.",
			st.reason[0] ? st.reason : "The SD card couldn't be checked", st.reason[0] && st.reason[strlen(st.reason) - 1] == '.' ? "" : ".");
		lv_label_set_text(body, b);
	}
}

static void tick_cb(lv_timer_t *t)
{
	(void)t;
	char before[1024];
	snprintf(before, sizeof(before), "%s", last_raw);
	read_state();
	if (strcmp(before, last_raw)) draw();
	/* the bar: there's no progress to show (the checker doesn't tell), so a part of it sweeps across */
	if (!lv_obj_is_hidden(bar)) {
		bar_x += LV_MAX(1, bar_w / 60);
		if (bar_x > bar_w) bar_x = -fill_w;
		lv_obj_set_x(bar_fill, bar_x);
	}
	if (done_at) {
		int left = st.restart - (int)(time(NULL) - done_at);
		char b[64];
		if (left > 0) snprintf(b, sizeof(b), "Starting Phosphor again in %d s", left);
		else snprintf(b, sizeof(b), "Starting Phosphor again...");
		lv_label_set_text(foot, b);
	}
}

static lv_obj_t *centred_label(lv_obj_t *p, const lv_font_t *f, lv_color_t c, int w)
{
	lv_obj_t *l = mk_label(p, f, c, "");
	lv_obj_set_width(l, w);
	lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
	lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
	return l;
}

void sdrepair_build(int w, int h, const char *state)
{
	state_path = state;
	theme_set_scale(w, h);
	lv_obj_t *scr = lv_screen_active();
	lv_obj_clean(scr);
	lv_obj_set_style_bg_color(scr, PAL.bg, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

	lv_obj_t *word = mk_label(scr, FONT_WORD, PAL.acc, "PHOSPHOR");
	lv_obj_set_style_text_letter_space(word, S(5), 0);
	lv_obj_set_pos(word, SX(56), S(30));

	lv_obj_t *col = lv_obj_create(scr);
	lv_obj_remove_style_all(col);
	lv_obj_set_size(col, LV_PCT(100), LV_SIZE_CONTENT);
	lv_obj_set_pos(col, 0, S(140));
	lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_row(col, S(20), 0);

	ic = icon_create(col, ICON_SDCARD, S(72));
	lab = mk_label(col, FONT_LABEL, PAL.dim, "");
	title = centred_label(col, FONT_HEADING, PAL.text, SW(1100));
	body = centred_label(col, FONT_NAME, C(0xc9c5be), SW(680));
	lv_obj_set_style_text_line_space(body, S(6), 0);

	bar_w = SW(560);
	fill_w = bar_w / 4;
	bar = mk_box(col, bar_w, S(8), PAL.line, S(4));
	lv_obj_set_style_clip_corner(bar, true, 0);
	lv_obj_set_style_margin_top(bar, S(8), 0);
	bar_fill = mk_box(bar, fill_w, S(8), PAL.acc, S(4));
	bar_x = -fill_w;
	sub = mk_label(col, FONT_BODY, PAL.dim, "Checking files and folders \xC2\xB7 usually under a minute");

	kv = lv_obj_create(col);
	lv_obj_remove_style_all(kv);
	lv_obj_set_size(kv, SW(600), LV_SIZE_CONTENT);
	lv_obj_set_style_bg_color(kv, PAL.panel, 0);
	lv_obj_set_style_bg_opa(kv, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(kv, PAL.line, 0);
	lv_obj_set_style_border_width(kv, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_radius(kv, S(12), 0);
	lv_obj_set_style_pad_hor(kv, S(30), 0);
	lv_obj_set_style_pad_ver(kv, S(20), 0);
	lv_obj_set_style_margin_top(kv, S(6), 0);
	static int32_t cols[] = { 0, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST }, rows[] = { LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST };
	cols[0] = SW(170);
	lv_obj_set_grid_dsc_array(kv, cols, rows);
	lv_obj_set_style_pad_row(kv, S(10), 0);
	for (int i = 0; i < KVN; i++) {
		kv_k[i] = mk_label(kv, FONT_BODY, PAL.dim, "");
		kv_v[i] = mk_label(kv, FONT_BODY, PAL.text, "");
		lv_obj_set_grid_cell(kv_k[i], LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, i, 1);
		lv_obj_set_grid_cell(kv_v[i], LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_START, i, 1);
		lv_label_set_long_mode(kv_v[i], LV_LABEL_LONG_MODE_WRAP);
		lv_obj_set_width(kv_v[i], LV_PCT(100));
	}

	/* the warning (repairing) or the back-up hint (errors left), at the bottom */
	note = lv_obj_create(scr);
	lv_obj_remove_style_all(note);
	lv_obj_set_size(note, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_set_style_bg_opa(note, LV_OPA_COVER, 0);
	lv_obj_set_style_border_width(note, LV_MAX(1, S(1)), 0);
	lv_obj_set_style_radius(note, S(12), 0);
	lv_obj_set_style_pad_hor(note, S(26), 0);
	lv_obj_set_style_pad_ver(note, S(14), 0);
	lv_obj_set_flex_flow(note, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(note, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_column(note, S(14), 0);
	lv_obj_align(note, LV_ALIGN_BOTTOM_MID, 0, -S(56));
	note_ic = icon_create(note, ICON_POWER, S(24));
	note_txt = mk_label(note, FONT_NAME, PAL.text, "");

	foot = mk_label(scr, FONT_NAME, PAL.dim, "");
	lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, -S(64));

	read_state();
	draw();
	lv_timer_create(tick_cb, 33, NULL);
}
