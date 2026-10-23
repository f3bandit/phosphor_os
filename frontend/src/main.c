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
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>

static void front_pads(uint32_t now);

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
static void console_quiet(void)
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

static int run_fbdev(const char *fbdev, const char *fb_mode, int fb_rb)
{
	lv_tick_set_cb(mono_ms);
	int mw = 0, mh = 0;
	/* "full": 0x0, which Main turns into the size it sends to the TV */
	int have_mode = fb_mode && (!strcmp(fb_mode, "full") || sscanf(fb_mode, "%dx%d", &mw, &mh) == 2);
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
		}
		if (redraw_at && (int)(now - redraw_at) >= 0) {
			redraw_at = 0;
			console_quiet();
			lv_obj_invalidate(lv_screen_active());
		}
		int active = menu_active() && !mainlink_hidden();
		if (!active) {
			/* a game is running: drain input so nothing happens behind it, and sleep */
			while (input_poll(now) != KEY_NONE_) {}
			if (was_active) plog(mainlink_hidden() ? "MiSTer's menu has the screen: idle" : "game running: idle");
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
		front_pads(now);
		pkey_t k;
		while ((k = input_poll(now)) != KEY_NONE_) ui_key(k);
		ui_tick();
		uint32_t wait = lv_timer_handler();
		if (wait > 16) wait = 16;   /* stay responsive to input */
		usleep(wait * 1000);
	}
	return 0;
}

/* the controllers on the front ports (through phosphord and phosphor-main), 60 times a second while
 * the setting is on; when the ports aren't there, look again every 5 s, and once ask phosphord to
 * load the Phosphor menu core (stock menu core at boot) */
/* The front-port controllers are asked for 60 times a second on a thread of their own (its own
 * connection to phosphord), so the screen never waits on Main: the main loop only picks up the latest
 * answer. */
static struct {
	pthread_mutex_t mu;
	int run, seq, failed;
	unsigned btn[2];
	int conn[2], id[2], lx[2], ly[2];
} fp = { .mu = PTHREAD_MUTEX_INITIALIZER };

static int fp_connect(void)
{
	const char *t = getenv("PHOS_TMP");
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	snprintf(a.sun_path, sizeof(a.sun_path), "%s/phosphord.sock", t && *t ? t : "/tmp/phosphor");
	int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (s < 0) return -1;
	if (connect(s, (struct sockaddr *)&a, sizeof(a)) < 0) { close(s); return -1; }
	return s;
}

/* one request, one line back (up to 500 ms); NULL on any trouble */
static cJSON *fp_ask(int *s)
{
	static const char req[] = "{\"cmd\":\"pads\"}\n";
	if (*s < 0 && (*s = fp_connect()) < 0) return NULL;
	if (write(*s, req, sizeof(req) - 1) != (ssize_t)(sizeof(req) - 1)) { close(*s); *s = -1; return NULL; }
	char buf[2048];
	size_t n = 0;
	for (;;) {
		struct pollfd p = { *s, POLLIN, 0 };
		if (poll(&p, 1, 500) <= 0) break;
		ssize_t r = read(*s, buf + n, sizeof(buf) - 1 - n);
		if (r <= 0) break;
		n += (size_t)r;
		char *nl = memchr(buf, '\n', n);
		if (nl) { *nl = 0; return cJSON_Parse(buf); }
		if (n >= sizeof(buf) - 1) break;
	}
	close(*s);
	*s = -1;
	return NULL;
}

static void *fp_thread(void *arg)
{
	(void)arg;
	int s = -1;
	for (;;) {
		pthread_mutex_lock(&fp.mu);
		int run = fp.run;
		pthread_mutex_unlock(&fp.mu);
		if (!run) { usleep(100000); continue; }
		cJSON *r = fp_ask(&s);
		int ok = r && cJSON_IsTrue(cJSON_GetObjectItem(r, "ok"));
		pthread_mutex_lock(&fp.mu);
		fp.failed = !ok;
		for (int i = 0; i < 2; i++) fp.conn[i] = 0;
		if (ok) {
			cJSON *it;
			cJSON_ArrayForEach(it, cJSON_GetObjectItem(r, "pads")) {
				int port = (int)api_num(it, "port", 0) - 1;
				if (port < 0 || port > 1) continue;
				fp.conn[port] = cJSON_IsTrue(cJSON_GetObjectItem(it, "connected"));
				fp.id[port] = (int)api_num(it, "id", 0);
				fp.btn[port] = fp.conn[port] ? (unsigned)api_num(it, "buttons", 0) : 0;
				fp.lx[port] = (int)api_num(it, "lx", 128);
				fp.ly[port] = (int)api_num(it, "ly", 128);
			}
		}
		fp.seq++;
		pthread_mutex_unlock(&fp.mu);
		cJSON_Delete(r);
		usleep(ok ? 16000 : 2000000);         /* nothing there: look again every 2 s */
	}
	return NULL;
}

static void front_pads(uint32_t now)
{
	static int started, seen, asked_load;
	static uint32_t next_look;
	if (!started) {
		pthread_t t;
		if (pthread_create(&t, NULL, fp_thread, NULL) == 0) pthread_detach(t);
		started = 1;
	}
	pthread_mutex_lock(&fp.mu);
	fp.run = SETTINGS.front_pads;
	int seq = fp.seq, failed = fp.failed;
	unsigned btn[2] = { fp.btn[0], fp.btn[1] };
	int conn[2] = { fp.conn[0], fp.conn[1] }, id[2] = { fp.id[0], fp.id[1] };
	int lx[2] = { fp.lx[0], fp.lx[1] }, ly[2] = { fp.ly[0], fp.ly[1] };
	pthread_mutex_unlock(&fp.mu);
	if (!SETTINGS.front_pads || seq == seen) return;
	seen = seq;
	if (failed) {
		for (int i = 0; i < 2; i++) input_pad(i, 0, 128, 128, 0, now);
		/* the front ports aren't on yet: once, after Main has had time to look, ask phosphord to set them up */
		if (!asked_load && mainlink_connected() && (int32_t)(now - next_look) >= 0) {
			next_look = now + 5000;
			cJSON *p = api_call("{\"cmd\":\"ports\"}");
			cJSON *pp = cJSON_GetObjectItem(p, "ports");
			if (cJSON_IsTrue(cJSON_GetObjectItem(pp, "can_load")) || cJSON_IsTrue(cJSON_GetObjectItem(pp, "available"))) {
				asked_load = 1;
				plog("front ports: turning them on");
				cJSON_Delete(api_call("{\"cmd\":\"memcard\",\"do\":\"prepare\"}"));
			}
			cJSON_Delete(p);
		}
		return;
	}
	for (int i = 0; i < 2; i++)
		input_pad(i, conn[i] ? btn[i] : 0, lx[i], ly[i], conn[i] && (id[i] == 0x73 || id[i] == 0x53), now);
}

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
