/* Phosphor frontend.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 *   phosphor                         run on the MiSTer's Linux framebuffer (/dev/fb0)
 *   phosphor --fb /dev/fbN           another framebuffer
 *   phosphor --fb-mode 1280x720      ask MiSTer Main for this framebuffer size (fb_cmd1; default 1280x720,
 *                                    "none" to leave it as it is)
 *   phosphor --fb-rb 0|1             colour order for fb_cmd1 (default 1; try 0 if red and blue are swapped)
 *   phosphor --log-serial [dev]      also log to the serial console (default /dev/ttyS0)
 *   phosphor --snapshot out.ppm [--keys right,a,wait,...] [--size 1280x720]   (wait: 1 s for background work)
 *                                    render without a screen, press the keys, save a picture
 *
 * While a game runs (MiSTer's /tmp/CORENAME is not MENU) the frontend sleeps and ignores input. */
#include "input.h"
#include "api.h"
#include "log.h"
#include "settings.h"
#include "theme.h"
#include "jpeg.h"
#include "ui.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint32_t mono_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* ---------- headless: render into memory, write a PPM ---------- */

static uint32_t virt_ms;
static uint32_t virt_tick(void) { return virt_ms; }
static uint8_t *shot;
static int shot_w, shot_h;

static void shot_flush(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
	int w = lv_area_get_width(a);
	for (int y = a->y1; y <= a->y2; y++)
		memcpy(shot + ((size_t)y * shot_w + a->x1) * 4, px + (size_t)(y - a->y1) * w * 4, (size_t)w * 4);
	lv_display_flush_ready(d);
}

static void advance(uint32_t ms)
{
	for (uint32_t t = 0; t < ms; t += 10) {
		virt_ms += 10;
		lv_timer_handler();
	}
}

static int run_snapshot(const char *out, const char *keys, int w, int h)
{
	lv_tick_set_cb(virt_tick);
	shot_w = w; shot_h = h;
	shot = calloc((size_t)w * h, 4);
	lv_display_t *d = lv_display_create(w, h);
	lv_display_set_color_format(d, LV_COLOR_FORMAT_ARGB8888);
	size_t bsz = (size_t)w * h * 4;
	lv_display_set_buffers(d, malloc(bsz), NULL, bsz, LV_DISPLAY_RENDER_MODE_FULL);
	lv_display_set_flush_cb(d, shot_flush);

	/* wait for phosphord to finish its first scan, so counts are real */
	for (int i = 0; i < 100; i++) {
		cJSON *r = api_call("{\"cmd\":\"status\"}");
		int busy = !r || cJSON_IsTrue(cJSON_GetObjectItem(r, "indexing")) || api_num(r, "generation", 0) < 1;
		cJSON_Delete(r);
		if (!busy) break;
		usleep(100000);
	}
	ui_init(w, h);
	advance(300);
	ui_tick();
	if (keys && *keys) {
		char buf[512];
		snprintf(buf, sizeof(buf), "%s", keys);
		char *save = NULL;
		for (char *k = strtok_r(buf, ",", &save); k; k = strtok_r(NULL, ",", &save)) {
			if (!strcmp(k, "wait")) {                 /* let background work (phosphord jobs) finish: 1 s */
				usleep(1000000);
				advance(1000);
				ui_tick();
				continue;
			}
			pkey_t pk = key_from_name(k);
			if (pk != KEY_NONE_) ui_key(pk);
			advance(250);
			ui_tick();
		}
	}
	advance(400);
	lv_refr_now(d);

	FILE *f = fopen(out, "wb");
	if (!f) { perror(out); return 1; }
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for (size_t i = 0; i < (size_t)w * h; i++) {
		uint8_t *p = shot + i * 4;      /* ARGB8888 in memory: B G R A */
		fputc(p[2], f); fputc(p[1], f); fputc(p[0], f);
	}
	fclose(f);
	plog("wrote %s", out);
	return 0;
}

/* ---------- on the MiSTer ---------- */

static int menu_active(void)
{
	char name[64] = "";
	FILE *f = fopen("/tmp/CORENAME", "r");
	if (!f) return 1;   /* no core info yet: assume the menu */
	if (!fgets(name, sizeof(name), f)) name[0] = 0;
	fclose(f);
	name[strcspn(name, "\r\n")] = 0;
	return !name[0] || !strcmp(name, "MENU");
}

static int run_fbdev(const char *fbdev, const char *fb_mode, int fb_rb)
{
	lv_tick_set_cb(mono_ms);
	int mw, mh;
	if (fb_mode && sscanf(fb_mode, "%dx%d", &mw, &mh) == 2) {
		/* Main only accepts this while the Linux framebuffer is showing (F9 in the stock menu) */
		char cmd[64], err[256];
		snprintf(cmd, sizeof(cmd), "fb_cmd1 8888 %d %d %d\n", fb_rb, mw, mh);
		if (mister_cmd(cmd, err, sizeof(err)) != 0) plog("%s", err);
		usleep(300000);
	}
	lv_display_t *d = lv_linux_fbdev_create();
	if (!d) { plog("can't create the framebuffer display"); return 1; }
	lv_linux_fbdev_set_file(d, fbdev);
	int w = lv_display_get_horizontal_resolution(d), h = lv_display_get_vertical_resolution(d);
	plog("screen %dx%d on %s", w, h, fbdev);
	input_init();
	ui_init(w, h);

	int was_active = 1;
	for (;;) {
		uint32_t now = mono_ms();
		int active = menu_active();
		if (!active) {
			/* a game is running: drain input so nothing happens behind it, and sleep */
			while (input_poll(now) != KEY_NONE_) {}
			if (was_active) plog("game running: idle");
			was_active = 0;
			usleep(200000);
			continue;
		}
		if (!was_active) {
			plog("back in the menu");
			was_active = 1;
			lv_obj_invalidate(lv_screen_active());
		}
		pkey_t k;
		while ((k = input_poll(now)) != KEY_NONE_) ui_key(k);
		ui_tick();
		uint32_t wait = lv_timer_handler();
		if (wait > 16) wait = 16;   /* stay responsive to input */
		usleep(wait * 1000);
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *snap = NULL, *keys = NULL, *fb = "/dev/fb0", *serial = NULL, *fb_mode = "1280x720";
	int w = 1280, h = 720, fb_rb = 1;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--snapshot") && i + 1 < argc) snap = argv[++i];
		else if (!strcmp(argv[i], "--keys") && i + 1 < argc) keys = argv[++i];
		else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &w, &h);
		else if (!strcmp(argv[i], "--fb") && i + 1 < argc) fb = argv[++i];
		else if (!strcmp(argv[i], "--fb-mode") && i + 1 < argc) { fb_mode = argv[++i]; if (!strcmp(fb_mode, "none")) fb_mode = NULL; }
		else if (!strcmp(argv[i], "--fb-rb") && i + 1 < argc) fb_rb = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--log-serial")) serial = (i + 1 < argc && argv[i + 1][0] == '/') ? argv[++i] : "/dev/ttyS0";
		else if (!strcmp(argv[i], "--version")) { puts(PHOS_VERSION); return 0; }
		else { fprintf(stderr, "usage: phosphor [--fb DEV] [--fb-mode WxH|none] [--fb-rb 0|1] [--log-serial [DEV]]\n"
			                "       phosphor --snapshot OUT.ppm [--keys k,k] [--size WxH]\n"); return 2; }
	}
	log_open(serial);
	plog("Phosphor %s", PHOS_VERSION);
	lv_init();
	jpeg_init();
	settings_load();
	theme_init((accent_t)SETTINGS.accent);
	return snap ? run_snapshot(snap, keys, w, h) : run_fbdev(fb, fb_mode, fb_rb);
}
