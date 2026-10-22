/* Phosphor frontend: controller and keyboard input (Linux evdev), with hot-plug and key repeat.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Reads every /dev/input/event* device that has keys or buttons. New devices are picked up when
 * /dev/input changes (inotify), so a controller plugged in later just works. Devices aren't
 * grabbed: MiSTer Main keeps seeing them too. */
#include "input.h"
#include "log.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_DEV 16
#define REPEAT_DELAY 320
#define REPEAT_RATE 85
#define AXIS_DEAD 40   /* percent of the axis range */

typedef struct {
	int fd;
	char name[24];
	int ax_min[64], ax_max[64];
	int ax_dir[64];      /* -1/0/1 last reported direction per axis */
} dev_t_;

static dev_t_ devs[MAX_DEV];
static int ndev, ino_fd = -1;

static pkey_t queue[32];
static int qh, qt;
static pkey_t held = KEY_NONE_;
static unsigned held_since, last_repeat;

void input_push(pkey_t k)
{
	int n = (qt + 1) % 32;
	if (n != qh) { queue[qt] = k; qt = n; }
}

static int has_bit(const unsigned long *bits, int b)
{
	return (bits[b / (8 * sizeof(long))] >> (b % (8 * sizeof(long)))) & 1;
}

static void open_dev(const char *name)
{
	char path[64];
	snprintf(path, sizeof(path), "/dev/input/%s", name);
	for (int i = 0; i < ndev; i++) if (!strcmp(devs[i].name, name)) return;
	if (ndev >= MAX_DEV) return;
	int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) return;
	unsigned long keys[KEY_CNT / (8 * sizeof(long)) + 1] = { 0 };
	unsigned long abs_[ABS_CNT / (8 * sizeof(long)) + 1] = { 0 };
	ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys);
	ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_)), abs_);
	/* keyboards, gamepads and joysticks; skip mice-only and power buttons */
	int useful = has_bit(keys, KEY_ENTER) || has_bit(keys, BTN_SOUTH) || has_bit(keys, BTN_TRIGGER) ||
	             has_bit(keys, BTN_DPAD_UP);
	if (!useful) { close(fd); return; }
	dev_t_ *d = &devs[ndev++];
	memset(d, 0, sizeof(*d));
	d->fd = fd;
	snprintf(d->name, sizeof(d->name), "%s", name);
	for (int a = 0; a < 64; a++) {
		if (!has_bit(abs_, a)) continue;
		struct input_absinfo ai;
		if (ioctl(fd, EVIOCGABS(a), &ai) == 0) { d->ax_min[a] = ai.minimum; d->ax_max[a] = ai.maximum; }
	}
	char dname[128] = "?";
	ioctl(fd, EVIOCGNAME(sizeof(dname)), dname);
	plog("input: %s (%s)", name, dname);
}

static void scan_devs(void)
{
	DIR *dir = opendir("/dev/input");
	if (!dir) return;
	struct dirent *e;
	while ((e = readdir(dir)))
		if (!strncmp(e->d_name, "event", 5)) open_dev(e->d_name);
	closedir(dir);
}

void input_init(void)
{
	ino_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (ino_fd >= 0) inotify_add_watch(ino_fd, "/dev/input", IN_CREATE | IN_ATTRIB);
	scan_devs();
}

static pkey_t map_key(int code)
{
	switch (code) {
	case KEY_UP: case BTN_DPAD_UP: return K_UP;
	case KEY_DOWN: case BTN_DPAD_DOWN: return K_DOWN;
	case KEY_LEFT: case BTN_DPAD_LEFT: return K_LEFT;
	case KEY_RIGHT: case BTN_DPAD_RIGHT: return K_RIGHT;
	case KEY_ENTER: case KEY_KPENTER: case KEY_SPACE: case BTN_SOUTH: case BTN_TRIGGER: return K_A;
	case KEY_ESC: case KEY_BACKSPACE: case BTN_EAST: case BTN_THUMB: return K_B;
	case KEY_X: case BTN_WEST: return K_X;
	case KEY_Y: case BTN_NORTH: return K_Y;
	case KEY_F1: case KEY_TAB: case BTN_START: case BTN_MODE: return K_MENU;
	case BTN_SELECT: return K_SELECT;
	case KEY_PAGEUP: case BTN_TL: return K_L;
	case KEY_PAGEDOWN: case BTN_TR: return K_R;
	default: return KEY_NONE_;
	}
}

static void press(pkey_t k, unsigned now)
{
	input_push(k);
	if (k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT) { held = k; held_since = now; last_repeat = now; }
}

static void release(pkey_t k)
{
	if (held == k) held = KEY_NONE_;
}

/* sticks and hats become directions */
static void axis(dev_t_ *d, int code, int value, unsigned now)
{
	if (code >= 64) return;
	int dir = 0;
	if (code == ABS_HAT0X || code == ABS_HAT0Y) {
		dir = value < 0 ? -1 : value > 0 ? 1 : 0;
	} else if (code == ABS_X || code == ABS_Y) {
		int mn = d->ax_min[code], mx = d->ax_max[code];
		if (mx <= mn) return;
		int mid = (mn + mx) / 2, span = (mx - mn) / 2;
		int pct = (value - mid) * 100 / (span ? span : 1);
		dir = pct < -AXIS_DEAD ? -1 : pct > AXIS_DEAD ? 1 : 0;
	} else {
		return;
	}
	if (dir == d->ax_dir[code]) return;
	int horiz = code == ABS_HAT0X || code == ABS_X;
	pkey_t neg = horiz ? K_LEFT : K_UP, pos = horiz ? K_RIGHT : K_DOWN;
	if (d->ax_dir[code] < 0) release(neg);
	if (d->ax_dir[code] > 0) release(pos);
	d->ax_dir[code] = dir;
	if (dir < 0) press(neg, now);
	if (dir > 0) press(pos, now);
}

pkey_t input_poll(unsigned now)
{
	if (ino_fd >= 0) {
		char buf[1024];
		if (read(ino_fd, buf, sizeof(buf)) > 0) scan_devs();
	}
	for (int i = 0; i < ndev; i++) {
		struct input_event ev;
		ssize_t r;
		while ((r = read(devs[i].fd, &ev, sizeof(ev))) == (ssize_t)sizeof(ev)) {
			if (ev.type == EV_KEY) {
				pkey_t k = map_key(ev.code);
				if (k == KEY_NONE_) continue;
				if (ev.value == 1) press(k, now);
				else if (ev.value == 0) release(k);
			} else if (ev.type == EV_ABS) {
				axis(&devs[i], ev.code, ev.value, now);
			}
		}
		if (r < 0 && errno == ENODEV) {
			plog("input: %s gone", devs[i].name);
			close(devs[i].fd);
			devs[i] = devs[--ndev];
			i--;
		}
	}
	if (held != KEY_NONE_ && now - held_since >= REPEAT_DELAY && now - last_repeat >= REPEAT_RATE) {
		last_repeat = now;
		input_push(held);
	}
	if (qh == qt) return KEY_NONE_;
	pkey_t k = queue[qh];
	qh = (qh + 1) % 32;
	return k;
}

pkey_t key_from_name(const char *n)
{
	static const struct { const char *n; pkey_t k; } T[] = {
		{ "up", K_UP }, { "down", K_DOWN }, { "left", K_LEFT }, { "right", K_RIGHT }, { "a", K_A }, { "b", K_B },
		{ "x", K_X }, { "y", K_Y }, { "menu", K_MENU }, { "select", K_SELECT }, { "l", K_L }, { "r", K_R },
	};
	for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++) if (!strcasecmp(n, T[i].n)) return T[i].k;
	return KEY_NONE_;
}
