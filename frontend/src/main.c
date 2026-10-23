/* Phosphor frontend.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 *   phosphor                         run on the MiSTer's Linux framebuffer (/dev/fb0)
 *   phosphor --fb /dev/fbN           another framebuffer
 *   phosphor --fb-mode WxH           ask MiSTer Main for this framebuffer size (fb_cmd1). Default "full":
 *                                    the size of the picture MiSTer sends to the TV, so Phosphor fills the
 *                                    screen (Main only scales the framebuffer by whole numbers, so 1280x720
 *                                    on a 1080p TV would sit in a box in the middle). "none": leave it.
 *   phosphor --fb-rb 0|1             colour order for fb_cmd1 (default 1; try 0 if red and blue are swapped)
 *   phosphor --log-serial [dev]      also log to the serial console (default /dev/ttyS0)
 *   phosphor --snapshot out.ppm [--keys right,a,wait,...] [--size 1280x720]   (wait: 1 s for background work)
 *                                    render without a screen, press the keys, save a picture
 *
 * While a game runs (MiSTer's /tmp/CORENAME is not MENU) the frontend sleeps and ignores input. */
#include "input.h"
#include "api.h"
#include "mainlink.h"
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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>


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
			if (!strcmp(k, "saver")) { ui_screensaver_now(); advance(1000); ui_tick(); continue; }
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

/* The Linux console (the login prompt on tty1, kernel messages) draws on the same framebuffer:
 * switch the shown console to graphics mode so only the shell draws there. Again after Main gives
 * the screen back to us, since it switches consoles itself. */
void console_quiet(void)
{
	const char *ttys[] = { "/dev/tty0", "/dev/tty1", NULL };
	for (int i = 0; ttys[i]; i++) {
		int fd = open(ttys[i], O_RDWR | O_CLOEXEC);
		if (fd < 0) continue;
		if (ioctl(fd, KDSETMODE, KD_GRAPHICS) < 0) plog("console: %s stays in text mode", ttys[i]);
		close(fd);
	}
	int f = open("/sys/class/graphics/fbcon/cursor_blink", O_WRONLY | O_CLOEXEC);
	if (f >= 0) { if (write(f, "0", 1) < 0) {} close(f); }
}

/* Before a script runs in MiSTer's terminal: the console back to text mode, or the kernel ignores
 * Main's switch to tty2 (no switches away from a KD_GRAPHICS console) and Main waits for it forever.
 * console_quiet() puts it back when the shell has the screen again. */
void console_text(void)
{
	const char *ttys[] = { "/dev/tty0", "/dev/tty1", NULL };
	for (int i = 0; ttys[i]; i++) {
		int fd = open(ttys[i], O_RDWR | O_CLOEXEC);
		if (fd < 0) continue;
		if (ioctl(fd, KDSETMODE, KD_TEXT) < 0) plog("console: %s stays in graphics mode", ttys[i]);
		close(fd);
	}
}

/* the framebuffer to black, so nothing that was on it (the Linux console's text) shows before the first
   picture */
static void fb_black(const char *fbdev)
{
	int fd = open(fbdev, O_RDWR | O_CLOEXEC);
	if (fd < 0) return;
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	if (ioctl(fd, FBIOGET_VSCREENINFO, &v) == 0 && ioctl(fd, FBIOGET_FSCREENINFO, &f) == 0) {
		size_t len = (size_t)f.line_length * v.yres_virtual;
		if (!len || len > f.smem_len) len = f.smem_len;
		void *m = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (m != MAP_FAILED) { memset(m, 0, len); munmap(m, len); }
	}
	close(fd);
}

static int run_fbdev(const char *fbdev, const char *fb_mode, int fb_rb)
{
	lv_tick_set_cb(mono_ms);
	int mw = 0, mh = 0;
	/* "full": 0x0, which Main turns into the size it sends to the TV */
	int have_mode = fb_mode && (!strcmp(fb_mode, "full") || sscanf(fb_mode, "%dx%d", &mw, &mh) == 2);
	/* no console text on the screen: the console stops drawing and what it drew is cleared before Main
	   shows the framebuffer (and again after the mode change) */
	console_quiet();
	fb_black(fbdev);
	/* with phosphor-main the shell takes the screen itself; with stock Main it's F9 */
	mainlink_start();
	if (have_mode) {
		/* Main only accepts this while the Linux framebuffer is showing */
		char cmd[64], err[256];
		snprintf(cmd, sizeof(cmd), "fb_cmd1 8888 %d %d %d\n", fb_rb, mw, mh);
		if (mister_cmd(cmd, err, sizeof(err)) != 0) plog("%s", err);
		usleep(300000);
	}
	console_quiet();
	fb_black(fbdev);
	lv_display_t *d = lv_linux_fbdev_create();
	if (!d) { plog("can't create the framebuffer display"); return 1; }
	lv_linux_fbdev_set_file(d, fbdev);
	int w = lv_display_get_horizontal_resolution(d), h = lv_display_get_vertical_resolution(d);
	plog("screen %dx%d on %s", w, h, fbdev);
	input_init();
	ui_init(w, h);

	int was_active = 1;
	uint32_t redraw_at = 0;
	for (;;) {
		uint32_t now = mono_ms();
		if (mainlink_poll() && have_mode) {     /* Main restarted and gave the screen back to us */
			char cmd[64], err[256];
			snprintf(cmd, sizeof(cmd), "fb_cmd1 8888 %d %d %d\n", fb_rb, mw, mh);
			if (mister_cmd(cmd, err, sizeof(err)) != 0) plog("%s", err);
			console_quiet();
			lv_obj_invalidate(lv_screen_active());
			redraw_at = now + 700;      /* Main may still be setting the video up: draw it all again */
			int st = mainlink_script_result();
			if (st >= 0) ui_script_done(st);
		}
		if (redraw_at && (int)(now - redraw_at) >= 0) {
			redraw_at = 0;
			console_quiet();
			lv_obj_invalidate(lv_screen_active());
		}
		int active = menu_active() && !mainlink_hidden() && !mainlink_script_running();
		if (!active) {
			/* a game is running: drain input so nothing happens behind it, and sleep */
			while (input_poll(now) != KEY_NONE_) {}
			if (was_active) plog(mainlink_script_running() ? "a script has the screen: idle" : mainlink_hidden() ? "MiSTer's menu has the screen: idle" : "game running: idle");
			was_active = 0;
			usleep(200000);
			continue;
		}
		if (!was_active) {
			plog("back in the menu");
			was_active = 1;
			console_quiet();
			ui_activity();
			lv_obj_invalidate(lv_screen_active());
			redraw_at = now + 700;
		}
		pkey_t k;
		while ((k = input_poll(now)) != KEY_NONE_) ui_key(k);
		ui_tick();
		/* sleep until input (controllers through Main, keyboards, remote keys) or the next screen timer:
		   at idle that's up to half a second, not 60 wake-ups a second */
		uint32_t wait = lv_timer_handler();
		uint32_t cap = (uint32_t)ui_max_sleep();
		if (input_held() || redraw_at) cap = 16;
		if (wait > cap) wait = cap;
		struct pollfd p[24];
		int n = input_pollfds(p, 23);
		if (mainlink_fd() >= 0) p[n++] = (struct pollfd){ mainlink_fd(), POLLIN, 0 };
		poll(p, (nfds_t)n, (int)wait);
	}
	return 0;
}

/* the controllers on the front ports (through phosphord and phosphor-main), 60 times a second while
 * the setting is on; when the ports aren't there, look again every 5 s, and once ask phosphord to
 * load the Phosphor menu core (stock menu core at boot) */
int main(int argc, char **argv)
{
	const char *snap = NULL, *keys = NULL, *fb = "/dev/fb0", *serial = NULL, *fb_mode = "full";
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
