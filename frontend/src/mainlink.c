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

static int fd = -1, ever = 0, hidden = 0, pads = 0, scripts = 0;
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

/* one command, one reply line (1 s at most) */
static int ask(const char *cmd, char *reply, size_t n)
{
	char line[128];
	snprintf(line, sizeof(line), "%s\n", cmd);
	if (send(fd, line, strlen(line), MSG_NOSIGNAL) < 0) return -1;
	size_t len = 0;
	long long end = now_ms() + 1000;
	while (len + 1 < n) {
		struct pollfd p = { fd, POLLIN, 0 };
		int left = (int)(end - now_ms());
		if (left <= 0 || poll(&p, 1, left) <= 0) return -1;
		ssize_t r = recv(fd, reply + len, 1, 0);
		if (r <= 0) return -1;
		if (reply[len] == '\n') { reply[len] = 0; return 0; }
		len++;
	}
	return -1;
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
	    || ask("screen phosphor", r, sizeof(r)) || !strstr(r, "\"ok\":true") || ask("subscribe", r, sizeof(r))) {
		close(fd);
		fd = -1;
		return 0;
	}
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

int mainlink_give(void)
{
	if (fd < 0) return 0;
	if (send(fd, "screen main\n", 12, MSG_NOSIGNAL) != 12) return 0;
	hidden = 1;
	plog("phosphor-main: screen given to MiSTer's menu");
	return 1;
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
					/* a controller button, read by Main with MiSTer's mapping (and repeated by it) */
					if (!hidden && strstr(lbuf, "\"event\":\"pad\"")) {
						char b[16] = "";
						int p = 0;
						if (sscanf(lbuf, "{\"event\":\"pad\",\"b\":\"%15[a-z]\",\"p\":%d", b, &p) == 2) input_main_pad(b, p);
						else input_watch("main", lbuf);                /* not understood: show it in the watch log */
						continue;
					}
					/* a script ran in MiSTer's terminal; it's over (or Main refused it): the screen again */
					if (script_on && (strstr(lbuf, "\"event\":\"script_done\"") || (!strncmp(lbuf, "{\"ok\":false", 12)))) {
						int st = 0;
						const char *q = strstr(lbuf, "\"status\":");
						if (q) st = atoi(q + 9);
						else { st = 1; plog("phosphor-main: script refused: %s", lbuf); }
						script_on = 0;
						script_done_status = st;
						retake = 1;
						continue;
					}
					if (hidden && strstr(lbuf, "\"event\":\"show\"")) {
						if (send(fd, "screen phosphor\n", 16, MSG_NOSIGNAL) == 16) {
							hidden = 0;
							retake = 1;
							plog("phosphor-main: the shell has the screen again");
						}
					}
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
			next_try = now_ms() + 2000;
			return 0;
		}
	}
	if (!ever || now_ms() < next_try) return 0;   /* stock Main: don't keep knocking */
	next_try = now_ms() + 3000;
	return take();
}
