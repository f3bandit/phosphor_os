/* Phosphor shell: the link to phosphor-main (Phosphor's fork of MiSTer Main), when it's installed.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Talks to the fork's local API (/tmp/phosphor-main.sock, one text command per line) only; no code
 * from Main. With the fork, the shell holds the screen while this connection is open: Main's menu
 * ignores the controls, and if the shell stops, Main takes the screen back by itself. */
#include "mainlink.h"
#include "input.h"
#include "log.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static int fd = -1, ever = 0, hidden = 0, pads = 0, scripts = 0, mapping = 0;
/* API 5: the dock's drive (disc events) and Phosphor's in-game menu (ingame) */
static int crt_api, ra_api, disc_api, ingame_api, in_game, disc_changed, menu_btn, ingame_ended, disc_want = 1, shot_want = 1;
/* a controller mapping Main runs for the shell (its steps come as events) */
static struct { int on, pending, step, done, saved; char dev[16], err[96]; } mp;
static int script_on, script_done_status = -1;   /* a script running through Main; its result once done */
static char lbuf[2048];
static size_t llen;
static long long next_try;

static long long now_ms(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static const char *path(void)
{
	const char *p = getenv("PHOSPHOR_MAIN_SOCK");
	return p && *p ? p : "/tmp/phosphor-main.sock";
}

static int handle_line(const char *l);

/* one command, one reply line (1 s at most). Event lines can come first: they're handled as
   mainlink_poll would, and the reply is the first line that isn't one */
static int ask(const char *cmd, char *reply, size_t n)
{
	char line[128];
	snprintf(line, sizeof(line), "%s\n", cmd);
	if (send(fd, line, strlen(line), MSG_NOSIGNAL) < 0) return -1;
	size_t len = 0;
	int full = 0;                    /* a line too long for reply: the rest of it is dropped */
	long long end = now_ms() + 1000;
	for (;;) {
		struct pollfd p = { fd, POLLIN, 0 };
		int left = (int)(end - now_ms());
		if (left <= 0 || poll(&p, 1, left) <= 0) return -1;
		char c;
		ssize_t r = recv(fd, &c, 1, 0);
		if (r <= 0) return -1;
		if (c != '\n') {
			if (len + 1 < n) reply[len++] = c; else full = 1;
			continue;
		}
		reply[len] = 0;
		int event = !strncmp(reply, "{\"event\":", 9);
		if (event && !full) handle_line(reply);
		if (!event && !full) return 0;
		if (!event) return -1;          /* the reply didn't fit */
		len = 0;                         /* an event: keep reading for the reply */
		full = 0;
	}
}

static int take(void)
{
	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) return 0;
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	snprintf(a.sun_path, sizeof(a.sun_path), "%s", path());
	char r[256];
	if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0 || ask("hello", r, sizeof(r)) || !strstr(r, "\"api\":")
	    || !((pads = strstr(r, "\"pads\":true") != NULL), 1)
	    || !((scripts = strstr(r, "\"scripts\":true") != NULL), 1)
	    || !((mapping = strstr(r, "\"mapping\":true") != NULL), 1)
	    || !((disc_api = strstr(r, "\"disc\":true") != NULL), 1)
	    || !((ingame_api = strstr(r, "\"ingame\":true") != NULL), 1)
	    || !((ra_api = strstr(r, "\"ra\":true") != NULL), 1)
	    || !((crt_api = strstr(r, "\"crt\":true") != NULL), 1)
	    || ask("screen phosphor", r, sizeof(r)) || !strstr(r, "\"ok\":true") || ask("subscribe", r, sizeof(r))) {
		close(fd);
		fd = -1;
		return 0;
	}
	/* in a game (Main restarts with each core, and the shell reconnects): a short press of the menu
	   button opens Phosphor's in-game menu, a long one MiSTer's; the drive as Settings › Discs says */
	in_game = ask("status", r, sizeof(r)) == 0 && strstr(r, "\"menu\":false") != NULL;
	if (ingame_api) ask("sheet on", r, sizeof(r));
	if (disc_api) ask(disc_want ? "disc on" : "disc off", r, sizeof(r));
	if (ingame_api) ask(shot_want ? "shot on" : "shot off", r, sizeof(r));     /* the screenshot combo in games */
	disc_changed = 1;
	fcntl(fd, F_SETFL, O_NONBLOCK);
	plog("phosphor-main: the shell has the screen");
	ever = 1;
	hidden = 0;
	llen = 0;
	usleep(150000);              /* Main switches to the framebuffer in its next UI pass */
	return 1;
}

int mainlink_start(void)
{
	if (take()) return 1;
	plog("stock MiSTer Main (no phosphor-main): press F9 on a keyboard to see the shell");
	return 0;
}

int mainlink_connected(void) { return fd >= 0; }
int mainlink_hidden(void) { return fd >= 0 && hidden; }
int mainlink_pads(void) { return fd >= 0 && pads; }
int mainlink_fd(void) { return fd; }
int mainlink_script_running(void) { return script_on; }
int mainlink_has_disc(void) { return fd >= 0 && disc_api; }
int mainlink_has_ingame(void) { return fd >= 0 && ingame_api; }
int mainlink_in_game(void) { return fd >= 0 && in_game; }
int mainlink_has_ra(void) { return fd >= 0 && ra_api; }

/* one command on its own short connection (the one holding the screen carries events): the reply line,
   malloc'd (the caller frees), or NULL. For answers that can be long, like "ra list". */
char *mainlink_query(const char *cmd)
{
	int q = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (q < 0) return NULL;
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	snprintf(a.sun_path, sizeof(a.sun_path), "%s", path());
	char line[160];
	int n = snprintf(line, sizeof(line), "%s\n", cmd);
	if (connect(q, (struct sockaddr *)&a, sizeof(a)) || send(q, line, (size_t)n, MSG_NOSIGNAL) != n) { close(q); return NULL; }
	size_t cap = 4096, len = 0;
	char *out = malloc(cap);
	long long end = now_ms() + 2000;
	while (out) {
		struct pollfd p = { q, POLLIN, 0 };
		int left = (int)(end - now_ms());
		if (left <= 0 || poll(&p, 1, left) <= 0) break;
		if (len + 4096 > cap) { char *nb = realloc(out, cap * 2); if (!nb) break; out = nb; cap *= 2; }
		ssize_t r = recv(q, out + len, cap - len - 1, 0);
		if (r <= 0) break;
		len += (size_t)r;
		out[len] = 0;
		char *nl = memchr(out, '\n', len);
		if (nl) { *nl = 0; close(q); return out; }
	}
	free(out);
	close(q);
	return NULL;
}
static int take_flag(int *f) { int v = *f; *f = 0; return v; }
int mainlink_disc_changed(void) { return take_flag(&disc_changed); }
int mainlink_menu_button(void) { return take_flag(&menu_btn); }
int mainlink_ingame_ended(void) { return take_flag(&ingame_ended); }

static int say(const char *line)
{
	if (fd < 0) return -1;
	char b[80];
	int n = snprintf(b, sizeof(b), "%s\n", line);
	return send(fd, b, (size_t)n, MSG_NOSIGNAL) == n ? 0 : -1;
}
/* replies to these come back as {"ok":...} lines among the events; nothing waits for them */
int mainlink_disc_enable(int on) { disc_want = on; return disc_api ? say(on ? "disc on" : "disc off") : -1; }
int mainlink_shot_combo(int on) { shot_want = on; return ingame_api ? say(on ? "shot on" : "shot off") : -1; }
int mainlink_ingame(int on) { return ingame_api ? say(on ? "ingame on" : "ingame off") : -1; }
int mainlink_has_crt(void) { return fd >= 0 && crt_api; }
/* CRT mode: the in-game menu as text in MiSTer's OSD (the analog port never shows the framebuffer) */
int mainlink_ingame_text(void) { return crt_api ? say("ingame text on") : -1; }
int mainlink_osdmenu(int sel, const char *title, const char *text)
{
	if (fd < 0 || !crt_api) return -1;
	char b[900];
	int n = snprintf(b, sizeof(b), "osdmenu %d %.24s\x1f", sel, title);
	for (const char *c = text; *c && n < (int)sizeof(b) - 2; c++) b[n++] = *c == '\n' ? 0x1f : (*c == '\r' ? ' ' : *c);
	b[n++] = '\n';
	return send(fd, b, (size_t)n, MSG_NOSIGNAL) == n ? 0 : -1;
}
/* the menu core into a CRT mode (or back to its usual picture); Main keeps it in phosphor/config/crt */
int mainlink_crt(int on, int mode, int h, int v)
{
	if (!crt_api) return -1;
	char b[48];
	snprintf(b, sizeof(b), "crt %d %d %d %d", on ? 1 : 0, mode, h, v);
	return say(b);
}
int mainlink_ingame_osd(void) { return ingame_api ? say("ingame osd") : -1; }
/* from the in-game menu back to Phosphor: the menu core again (Phosphor's, where it's installed) */
int mainlink_quit_game(void)
{
	if (fd < 0) return -1;
	say("ingame off");
	return say(access("/media/fat/menu.rbf", F_OK) == 0 ? "launch /media/fat/menu.rbf" : "launch /media/fat/phosphor/cores/menu_phosphor.rbf");
}

int mainlink_script(const char *path)
{
	if (fd < 0 || !scripts || hidden || script_on) return -1;
	char line[600];
	int n = snprintf(line, sizeof(line), "script %s\n", path);
	if (n <= 0 || n >= (int)sizeof(line) || send(fd, line, (size_t)n, MSG_NOSIGNAL) != n) return -1;
	script_on = 1;
	plog("phosphor-main: running %s", path);
	return 0;
}

int mainlink_script_result(void)
{
	int r = script_done_status;
	script_done_status = -1;
	return r;
}

int mainlink_can_map(void) { return fd >= 0 && mapping && !hidden; }

int mainlink_map(const char *what)
{
	if (!mainlink_can_map()) return -1;
	char line[64];
	int n = snprintf(line, sizeof(line), "map_%s\n", what);
	if (n <= 0 || n >= (int)sizeof(line) || send(fd, line, (size_t)n, MSG_NOSIGNAL) != n) return -1;
	if (!strncmp(what, "start", 5)) { memset(&mp, 0, sizeof(mp)); mp.on = 1; mp.step = -100; }
	mp.pending = 1;
	return 0;
}

int mainlink_map_state(int *step, int *done, int *saved, char *dev, size_t dn, char *err, size_t en)
{
	if (step) *step = mp.step;
	if (done) *done = mp.done;
	if (saved) *saved = mp.saved;
	if (dev) snprintf(dev, dn, "%s", mp.dev);
	if (err) snprintf(err, en, "%s", mp.err);
	return mp.on;
}

int mainlink_give(void)
{
	if (fd < 0) return 0;
	if (send(fd, "screen main\n", 12, MSG_NOSIGNAL) != 12) return 0;
	hidden = 1;
	plog("phosphor-main: screen given to MiSTer's menu");
	return 1;
}

/* one line from Main: an event, or the reply to a command nothing waits for. 1: the shell has the
   screen again (the caller waits for Main to switch to the framebuffer) */
static int handle_line(const char *l)
{
	/* a controller button, read by Main with MiSTer's mapping (and repeated by it) */
	if (!hidden && strstr(l, "\"event\":\"pad\"")) {
		char b[16] = "";
		int p = 0;
		if (sscanf(l, "{\"event\":\"pad\",\"b\":\"%15[a-z]\",\"p\":%d", b, &p) == 2) input_main_pad(b, p);
		else input_watch("main", l);                /* not understood: show it in the watch log */
		return 0;
	}
	/* the drive changed (phosphord names what's in it); a game started or ended; the menu
	   button in a game (Phosphor's in-game menu); Main took the in-game screen back */
	if (strstr(l, "\"event\":\"disc\"")) { disc_changed = 1; return 0; }
	if (strstr(l, "\"event\":\"shot\"")) { input_push(K_SHOT); return 0; }      /* both sticks in, on Phosphor's screens */
	if (strstr(l, "\"event\":\"core\"")) { in_game = strstr(l, "\"menu\":false") != NULL; return 0; }
	if (strstr(l, "\"event\":\"menu_button\"")) { if (in_game) menu_btn = 1; return 0; }
	if (strstr(l, "\"event\":\"ingame\"")) { ingame_ended = 1; return 0; }
	/* the controller mapping: a step, the end, or a refused command */
	if (strstr(l, "\"event\":\"map\"")) {
		const char *q;
		if (strstr(l, "\"done\":true")) { mp.on = 0; mp.done = 1; mp.saved = strstr(l, "\"saved\":true") != NULL; }
		else if ((q = strstr(l, "\"step\":"))) mp.step = atoi(q + 7);
		if ((q = strstr(l, "\"dev\":\"")) && sscanf(q + 7, "%15[0-9a-f:]", mp.dev) != 1) mp.dev[0] = 0;
		return 0;
	}
	/* the in-game screen refused or gone (ingame on / ingame text on / osdmenu): as if Main took it back */
	if (!strncmp(l, "{\"ok\":false", 12) && (strstr(l, "no game is running") || strstr(l, "another client has the screen")
	                                       || strstr(l, "not in text mode") || strstr(l, "can't show Phosphor's screen"))) { ingame_ended = 1; return 0; }
	if (mp.pending && !strncmp(l, "{\"ok\":", 6)) {
		mp.pending = 0;
		if (!strncmp(l, "{\"ok\":false", 12)) {
			const char *q = strstr(l, "\"error\":\"");
			snprintf(mp.err, sizeof(mp.err), "%.*s", q ? (int)strcspn(q + 9, "\"") : 0, q ? q + 9 : "");
			if (mp.on && mp.step == -100) { mp.on = 0; mp.done = 1; mp.saved = 0; }
		}
		if (!script_on) return 0;
	}
	/* a script ran in MiSTer's terminal; it's over (or Main refused it): the screen again */
	if (script_on && (strstr(l, "\"event\":\"script_done\"") || (!strncmp(l, "{\"ok\":false", 12)))) {
		int st = 0;
		const char *q = strstr(l, "\"status\":");
		if (q) st = atoi(q + 9);
		else { st = 1; plog("phosphor-main: script refused: %s", l); }
		script_on = 0;
		script_done_status = st;
		return 1;
	}
	if (hidden && strstr(l, "\"event\":\"show\"")) {
		if (send(fd, "screen phosphor\n", 16, MSG_NOSIGNAL) == 16) {
			hidden = 0;
			plog("phosphor-main: the shell has the screen again");
			return 1;
		}
	}
	return 0;
}

int mainlink_poll(void)
{
	if (fd >= 0) {
		char buf[1024];
		int retake = 0;
		for (;;) {
			ssize_t r = recv(fd, buf, sizeof(buf), 0);
			if (r > 0) {
				/* events, one per line: "show" (Launch Phosphor in Main's OSD) brings the shell back */
				for (ssize_t i = 0; i < r; i++) {
					if (buf[i] != '\n') { if (llen + 1 < sizeof(lbuf)) lbuf[llen++] = buf[i]; continue; }
					lbuf[llen] = 0;
					llen = 0;
					if (handle_line(lbuf)) retake = 1;
				}
				continue;
			}
			if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
				if (retake) usleep(150000);   /* Main switches to the framebuffer in its next UI pass */
				return retake;
			}
			plog("phosphor-main went away; trying again");
			close(fd);
			fd = -1;
			if (script_on) { script_on = 0; script_done_status = 1; }
			if (mp.on) { mp.on = 0; mp.done = 1; mp.saved = 0; snprintf(mp.err, sizeof(mp.err), "MiSTer's Main restarted"); }
			next_try = now_ms() + 2000;
			return 0;
		}
	}
	if (!ever || now_ms() < next_try) return 0;   /* stock Main: don't keep knocking */
	next_try = now_ms() + 3000;
	return take();
}
