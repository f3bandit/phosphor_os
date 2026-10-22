/* Phosphor frontend: controller and keyboard input (Linux evdev), with hot-plug and key repeat.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Reads every /dev/input/event* device that has keys or buttons. New devices are picked up when
 * /dev/input changes (inotify), so a controller plugged in later just works. Devices aren't
 * grabbed: MiSTer Main keeps seeing them too.
 *
 * While Phosphor has the screen, MiSTer Main turns every controller into keys on its "MiSTer virtual
 * input" device, using the button mapping set up in MiSTer (D-pad = arrows, A = Enter, B = Esc,
 * X = Space, Y = Tab, L/R = PgUp/PgDn). That's what controllers are read from when it's there, so a
 * controller behaves the way it's mapped in MiSTer (an 8BitDo's A is A, wherever it sits), and the
 * controller's own D-pad and face buttons are left alone so one press doesn't arrive twice or three
 * times. Only what Main doesn't pass on is read from the controller itself: Start, Select and the
 * left stick. A key held from several places still counts as one press, and a second press of the
 * same key within a few milliseconds of letting go (some pads bounce) is dropped. */
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
#define BOUNCE_MS 35   /* a press this soon after letting go of the same key is a bounce */
#define SRC_PAD0 16    /* sources: devices 0..15 (their slot), front ports 16 and 17 */

typedef struct {
	int fd;
	char name[24];
	char label[64];      /* what the device calls itself */
	int ax_min[64], ax_max[64];
	int ax_dir[64];      /* -1/0/1 last reported direction per axis */
	int slot;            /* source bit, stable while the device is there */
	int virt;            /* MiSTer Main's virtual input (controllers as keys) */
	int pad;             /* a controller (has gamepad or joystick buttons) */
} dev_t_;

static dev_t_ devs[MAX_DEV];
static int ndev, ino_fd = -1, started;
static char notice[96];           /* "Connected: ..." for the screen, once */

/* MiSTer Main's own virtual devices come and go with every core: not news */
static int newsworthy(const char *label) { return !strstr(label, "MiSTer") && !strstr(label, "uinput") && !strstr(label, "virtual"); }

const char *input_notice(void)
{
	static char out[96];
	if (!notice[0]) return NULL;
	snprintf(out, sizeof(out), "%s", notice);
	notice[0] = 0;
	return out;
}

static pkey_t queue[32];
static int qh, qt;
static pkey_t held = KEY_NONE_;
static unsigned held_since, last_repeat;
static unsigned holders[K_R + 1];    /* which sources hold each key down */
static unsigned released_at[K_R + 1];
static unsigned slots_used;

static int have_virt(void)
{
	for (int i = 0; i < ndev; i++) if (devs[i].virt) return 1;
	return 0;
}

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
	int slot = 0;
	while (slot < MAX_DEV && (slots_used & (1u << slot))) slot++;
	if (slot >= MAX_DEV) { close(fd); return; }
	slots_used |= 1u << slot;
	dev_t_ *d = &devs[ndev++];
	memset(d, 0, sizeof(*d));
	d->fd = fd;
	d->slot = slot;
	d->pad = has_bit(keys, BTN_SOUTH) || has_bit(keys, BTN_TRIGGER) || has_bit(keys, BTN_DPAD_UP) ||
	         has_bit(abs_, ABS_HAT0X);
	snprintf(d->name, sizeof(d->name), "%s", name);
	for (int a = 0; a < 64; a++) {
		if (!has_bit(abs_, a)) continue;
		struct input_absinfo ai;
		if (ioctl(fd, EVIOCGABS(a), &ai) == 0) { d->ax_min[a] = ai.minimum; d->ax_max[a] = ai.maximum; }
	}
	char dname[128] = "?";
	ioctl(fd, EVIOCGNAME(sizeof(dname)), dname);
	snprintf(d->label, sizeof(d->label), "%.63s", dname);
	d->virt = strstr(dname, "MiSTer virtual input") != NULL;
	plog("input: %s (%s)", name, dname);
	if (started && newsworthy(d->label)) snprintf(notice, sizeof(notice), "Connected: %.80s", d->label);
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
	started = 1;
}

/* MiSTer Main's virtual input: controllers as keys, mapped the way MiSTer has them */
static pkey_t map_virt(int code)
{
	switch (code) {
	case KEY_UP: return K_UP;
	case KEY_DOWN: return K_DOWN;
	case KEY_LEFT: return K_LEFT;
	case KEY_RIGHT: return K_RIGHT;
	case KEY_ENTER: return K_A;
	case KEY_ESC: return K_B;
	case KEY_SPACE: return K_X;
	case KEY_TAB: return K_Y;
	case KEY_PAGEUP: return K_L;
	case KEY_PAGEDOWN: return K_R;
	default: return KEY_NONE_;
	}
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

static void press(pkey_t k, int src, unsigned now)
{
	if (k <= KEY_NONE_ || k > K_R) return;
	unsigned bit = 1u << src;
	if (holders[k]) { holders[k] |= bit; return; }      /* already down from somewhere else */
	holders[k] = bit;
	if (released_at[k] && now - released_at[k] < BOUNCE_MS) return;
	input_push(k);
	if (k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT) { held = k; held_since = now; last_repeat = now; }
}

static void release(pkey_t k, int src, unsigned now)
{
	if (k <= KEY_NONE_ || k > K_R || !(holders[k] & (1u << src))) return;
	holders[k] &= ~(1u << src);
	if (holders[k]) return;
	released_at[k] = now ? now : 1;
	if (held == k) held = KEY_NONE_;
}

static void release_all(int src, unsigned now)
{
	for (int k = K_UP; k <= K_R; k++) release((pkey_t)k, src, now);
}

/* sticks and hats become directions */
static void axis(dev_t_ *d, int code, int value, unsigned now)
{
	if (code >= 64) return;
	int dir = 0;
	if (code == ABS_HAT0X || code == ABS_HAT0Y) {
		if (have_virt()) return;              /* the D-pad comes from Main's virtual input */
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
	if (d->ax_dir[code] < 0) release(neg, d->slot, now);
	if (d->ax_dir[code] > 0) release(pos, d->slot, now);
	d->ax_dir[code] = dir;
	if (dir < 0) press(neg, d->slot, now);
	if (dir > 0) press(pos, d->slot, now);
}

/* front-port controllers: Cross confirms and Circle goes back (the PlayStation way), Square X,
 * Triangle Y, Start the menu, the left stick moves like the D-pad */
void input_pad(int port, unsigned b, int lx, int ly, int analog, unsigned now)
{
	static unsigned prev[2];
	static const struct { int bit; pkey_t k; } M[] = {
		{ 4, K_UP }, { 5, K_RIGHT }, { 6, K_DOWN }, { 7, K_LEFT }, { 14, K_A }, { 13, K_B }, { 15, K_X },
		{ 12, K_Y }, { 3, K_MENU }, { 0, K_SELECT }, { 10, K_L }, { 11, K_R },
	};
	if (port < 0 || port > 1) return;
	if (analog) {
		if (lx < 48) b |= 1u << 7;
		if (lx > 208) b |= 1u << 5;
		if (ly < 48) b |= 1u << 4;
		if (ly > 208) b |= 1u << 6;
	}
	for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++) {
		unsigned bit = 1u << M[i].bit;
		if ((b & bit) && !(prev[port] & bit)) press(M[i].k, SRC_PAD0 + port, now);
		else if (!(b & bit) && (prev[port] & bit)) release(M[i].k, SRC_PAD0 + port, now);
	}
	prev[port] = b;
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
				dev_t_ *d = &devs[i];
				pkey_t k;
				if (d->virt) k = map_virt(ev.code);
				else if (d->pad && have_virt() && ev.code >= BTN_MISC && ev.code != BTN_START &&
				         ev.code != BTN_MODE && ev.code != BTN_SELECT) k = KEY_NONE_;  /* Main passes these on */
				else k = map_key(ev.code);
				if (k == KEY_NONE_) continue;
				if (ev.value == 1) press(k, d->slot, now);
				else if (ev.value == 0) release(k, d->slot, now);
			} else if (ev.type == EV_ABS) {
				axis(&devs[i], ev.code, ev.value, now);
			}
		}
		if (r < 0 && errno == ENODEV) {
			plog("input: %s gone", devs[i].name);
			if (newsworthy(devs[i].label)) snprintf(notice, sizeof(notice), "Disconnected: %.80s", devs[i].label);
			close(devs[i].fd);
			release_all(devs[i].slot, now);
			slots_used &= ~(1u << devs[i].slot);
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
