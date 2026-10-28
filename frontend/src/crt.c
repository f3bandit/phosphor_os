/* Phosphor shell: CRT mode.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * On MiSTer the analog port (VGA / SCART / component on the SS1) shows the running core's own video, not
 * Linux's framebuffer, so a CRT never sees a screen drawn by a Linux program. Phosphor's menu core has
 * the native video path from Retro-Remake's menu_ConsoleMode (rtl/native_video_*, GPL-3.0-or-later):
 * at a TV's own timing it shows frames read from a buffer in memory, in this layout:
 *   0x3A000000            the control word, (frame << 2) | buffer: a new frame number shows that buffer
 *   0x3A000100            buffer 0, then buffer 1 right after it (a whole frame each), RGBX8888 pixels
 *                         (bits 23-16 red, 15-8 green, 7-0 blue: LVGL's XRGB8888 as it is)
 * Phosphor's Main switches the menu core into the mode (crt on mode h v; phosphor/config/crt). Here every
 * frame the shell draws is kept in a copy of the whole screen, and once a frame is complete it goes into
 * the buffer not on show and the control word flips to it. The Linux framebuffer still gets every frame
 * too (at the same size), so an HDMI screen shows the same picture. */
#include "crt.h"
#include "log.h"
#include "mainlink.h"
#include "src/lvgl_private.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define NATIVE_BASE 0x3A000000UL
#define NATIVE_BUF0 0x100

static const char *cfg_path(char *buf, size_t n)
{
	const char *f = getenv("PHOS_FAT");
	snprintf(buf, n, "%s/phosphor/config/crt", f && *f ? f : "/media/fat");
	return buf;
}

int crt_load(crt_cfg_t *c)
{
	char p[300];
	memset(c, 0, sizeof(*c));
	c->mode = CRT_480I;
	FILE *f = fopen(cfg_path(p, sizeof(p)), "r");
	if (!f) return -1;
	int on = 0, mode = 0, h = 0, v = 0;
	int ok = fscanf(f, "%d %d %d %d", &on, &mode, &h, &v) == 4;
	fclose(f);
	if (!ok) return -1;
	int w, hh;
	c->on = on && crt_size(mode, &w, &hh) == 0;
	c->mode = crt_size(mode, &w, &hh) == 0 ? mode : CRT_480I;
	c->h = h < -31 ? -31 : h > 31 ? 31 : h;
	c->v = v < -31 ? -31 : v > 31 ? 31 : v;
	return 0;
}

int crt_save(const crt_cfg_t *c)
{
	char p[300], t[320];
	cfg_path(p, sizeof(p));
	snprintf(t, sizeof(t), "%s.tmp", p);
	FILE *f = fopen(t, "w");
	if (!f) return -1;
	fprintf(f, "%d %d %d %d\n", c->on ? 1 : 0, c->mode, c->h, c->v);
	if (fclose(f)) return -1;
	return rename(t, p);
}

int crt_size(int mode, int *w, int *h)
{
	switch (mode) {
	case CRT_480I: *w = 640; *h = 480; return 0;          /* 15 kHz NTSC, interlaced (the menu core filters flicker) */
	case CRT_576I: *w = 720; *h = 576; return 0;          /* 15 kHz PAL, interlaced */
	case CRT_480P: *w = 640; *h = 480; return 0;          /* 31 kHz (VGA, PVM / BVM) */
	}
	return -1;
}

const char *crt_mode_name(int mode)
{
	switch (mode) {
	case CRT_480I: return "480i (15 kHz)";
	case CRT_576I: return "576i PAL (15 kHz)";
	case CRT_480P: return "480p (31 kHz)";
	}
	return "Off";
}

static struct {
	int on, w, h, cur;
	uint32_t frame;
	volatile uint8_t *mem;                     /* the native area (control word, two buffers) */
	size_t len;
	uint32_t *shadow;                          /* the whole screen as drawn so far */
	lv_display_flush_cb_t fb_flush;            /* the framebuffer driver's own flush (HDMI) */
} cr;

int crt_active(void) { return cr.on; }
static int restart_wanted;
void crt_request_restart(void) { restart_wanted = 1; }
int crt_take_restart(void) { int r = restart_wanted; restart_wanted = 0; return r; }

static void present(void)
{
	int back = cr.cur ^ 1;
	size_t fsz = (size_t)cr.w * cr.h * 4;
	memcpy((void *)(cr.mem + NATIVE_BUF0 + (size_t)back * fsz), cr.shadow, fsz);
	__sync_synchronize();
	cr.frame++;
	*(volatile uint32_t *)cr.mem = (cr.frame << 2) | (uint32_t)back;   /* last: the frame is all there */
	__sync_synchronize();
	cr.cur = back;
}

static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
	int aw = lv_area_get_width(a), ah = lv_area_get_height(a);
	uint32_t stride = lv_draw_buf_width_to_stride((uint32_t)aw, LV_COLOR_FORMAT_XRGB8888);
	for (int y = 0; y < ah; y++) {
		int sy = a->y1 + y;
		if (sy < 0 || sy >= cr.h) continue;
		int x0 = a->x1 < 0 ? 0 : a->x1, x1 = a->x2 >= cr.w ? cr.w - 1 : a->x2;
		if (x1 < x0) continue;
		memcpy(cr.shadow + (size_t)sy * cr.w + x0, px + (size_t)y * stride + (size_t)(x0 - a->x1) * 4, (size_t)(x1 - x0 + 1) * 4);
	}
	if (lv_display_flush_is_last(d)) present();
	if (cr.fb_flush) cr.fb_flush(d, a, px);       /* (it says the flush is done) */
	else lv_display_flush_ready(d);
}

int crt_attach(lv_display_t *d, int w, int h)
{
	if (lv_display_get_color_format(d) != LV_COLOR_FORMAT_XRGB8888) { plog("crt: the display isn't 32-bit"); return -1; }
	long pg = sysconf(_SC_PAGESIZE);
	size_t len = NATIVE_BUF0 + 2 * (size_t)w * h * 4;
	len = (len + (size_t)pg - 1) / (size_t)pg * (size_t)pg;
	int fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
	if (fd < 0) { plog("crt: can't open /dev/mem"); return -1; }
	void *m = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)NATIVE_BASE);
	close(fd);
	if (m == MAP_FAILED) { plog("crt: can't map the native buffer"); return -1; }
	cr.shadow = calloc((size_t)w * h, 4);
	if (!cr.shadow) { munmap(m, len); return -1; }
	cr.mem = m;
	cr.len = len;
	cr.w = w;
	cr.h = h;
	cr.cur = 0;
	cr.frame = (*(volatile uint32_t *)cr.mem >> 2) + 1;    /* a number the menu core hasn't seen */
	cr.fb_flush = d->flush_cb;
	lv_display_set_flush_cb(d, flush);
	cr.on = 1;
	plog("crt: Phosphor's frames go to the analog port too (%dx%d)", w, h);
	return 0;
}

/* ---------- trying a mode ---------- */
static const char *prev_path(char *buf, size_t n)
{
	const char *f = getenv("PHOS_FAT");
	snprintf(buf, n, "%s/phosphor/config/crt.prev", f && *f ? f : "/media/fat");
	return buf;
}

int crt_try(const crt_cfg_t *nc)
{
	crt_cfg_t old;
	char p[300];
	crt_load(&old);                                   /* (none: off) */
	FILE *f = fopen(prev_path(p, sizeof(p)), "w");
	if (!f) return -1;
	fprintf(f, "%d %d %d %d\n", old.on ? 1 : 0, old.mode, old.h, old.v);
	fclose(f);
	if (crt_save(nc)) return -1;                       /* (Main writes the same; the shell reads it when it starts again) */
	mainlink_crt(nc->on, nc->mode, nc->h, nc->v);
	crt_request_restart();
	return 0;
}

int crt_pending(crt_cfg_t *prev)
{
	char p[300];
	FILE *f = fopen(prev_path(p, sizeof(p)), "r");
	if (!f) return 0;
	memset(prev, 0, sizeof(*prev));
	prev->mode = CRT_480I;
	int ok = fscanf(f, "%d %d %d %d", &prev->on, &prev->mode, &prev->h, &prev->v) == 4;
	fclose(f);
	if (!ok) { unlink(p); return 0; }
	return 1;
}

void crt_keep(void)
{
	char p[300];
	unlink(prev_path(p, sizeof(p)));
}

void crt_revert(void)
{
	crt_cfg_t prev;
	char p[300];
	if (!crt_pending(&prev)) return;
	crt_save(&prev);
	mainlink_crt(prev.on, prev.mode, prev.h, prev.v);
	unlink(prev_path(p, sizeof(p)));
	crt_request_restart();
}

int crt_move(int h, int v)
{
	crt_cfg_t c;
	crt_load(&c);
	c.h = h < -31 ? -31 : h > 31 ? 31 : h;
	c.v = v < -31 ? -31 : v > 31 ? 31 : v;
	crt_save(&c);
	return mainlink_crt(c.on, c.mode, c.h, c.v);
}
