/* phosphord: Phosphor's system service.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 *   phosphord [--log-serial [DEV]]
 *
 * Serves the API on a local socket (<tmp>/phosphord.sock): one JSON request per line, one JSON
 * reply per line. Indexes the library in the background, again whenever the mount table changes,
 * and keeps play time. Sleeps in poll() between events. */
#include "busy.h"
#include "api.h"
#include "remote.h"
#include "sshacc.h"
#include "phantom.h"
#include "memcard.h"
#include "wifi.h"
#include "heartbeat.h"
#include "db.h"
#include "indexer.h"
#include "log.h"
#include "paths.h"
#include "play.h"
#include "scraper.h"
#include "systems.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS 16
#define LINE_MAX_ 65536

typedef struct {
	int fd;
	char *buf;
	size_t len;
} client_t;

static client_t cl[MAX_CLIENTS];

static int listen_socket(void)
{
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) return -1;
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	snprintf(a.sun_path, sizeof(a.sun_path), "%s", p_socket());
	unlink(a.sun_path);
	if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(fd, 8) < 0) {
		plog("can't listen on %s: %s", a.sun_path, strerror(errno));
		close(fd);
		return -1;
	}
	chmod(a.sun_path, 0666);
	return fd;
}

static void write_all(int fd, const char *s, size_t n)
{
	while (n) {
		ssize_t w = write(fd, s, n);
		if (w < 0) {
			if (errno == EINTR) continue;
			if (errno == EAGAIN) { struct pollfd p = { fd, POLLOUT, 0 }; poll(&p, 1, 1000); continue; }
			return;
		}
		s += w;
		n -= (size_t)w;
	}
}

static void drop(client_t *c)
{
	close(c->fd);
	free(c->buf);
	memset(c, 0, sizeof(*c));
	c->fd = -1;
}

static void serve(sqlite3 *db, client_t *c)
{
	char tmp[4096];
	ssize_t r = read(c->fd, tmp, sizeof(tmp));
	if (r <= 0) { drop(c); return; }
	c->buf = realloc(c->buf, c->len + (size_t)r + 1);
	memcpy(c->buf + c->len, tmp, (size_t)r);
	c->len += (size_t)r;
	c->buf[c->len] = 0;
	char *nl;
	while ((nl = memchr(c->buf, '\n', c->len))) {
		*nl = 0;
		char *out = api_handle(db, c->buf);
		if (out) {
			write_all(c->fd, out, strlen(out));
			write_all(c->fd, "\n", 1);
			free(out);
		}
		size_t used = (size_t)(nl - c->buf) + 1;
		memmove(c->buf, nl + 1, c->len - used);
		c->len -= used;
	}
	if (c->len > LINE_MAX_) drop(c);
}

int main(int argc, char **argv)
{
	const char *serial = NULL;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--log-serial")) serial = (i + 1 < argc && argv[i + 1][0] == '/') ? argv[++i] : "/dev/ttyS0";
		else { fprintf(stderr, "usage: phosphord [--log-serial [DEV]]\n"); return 2; }
	}
	signal(SIGPIPE, SIG_IGN);
	mkdirs(p_tmp());
	log_open(serial);
	plog("starting");
	systems_load();
	sqlite3 *db = db_open();
	if (!db) return 1;
	int lfd = listen_socket();
	if (lfd < 0) return 1;
	for (int i = 0; i < MAX_CLIENTS; i++) cl[i].fd = -1;
	busy_init();
	indexer_start();
	scraper_start();
	sshacc_boot();
	remote_start();
	memcard_start();
	wifi_start();
	heartbeat_start();

	plog("ready on %s", p_socket());
	for (;;) {
		struct pollfd p[MAX_CLIENTS + 1];
		int n = 0, idx[MAX_CLIENTS];
		p[n++] = (struct pollfd){ lfd, POLLIN, 0 };
		for (int i = 0; i < MAX_CLIENTS; i++)
			if (cl[i].fd >= 0) { idx[n - 1] = i; p[n++] = (struct pollfd){ cl[i].fd, POLLIN, 0 }; }
		int r = poll(p, (nfds_t)n, 2000);   /* wakes every 2 s only to keep play time */
		play_tick(db);
		sshacc_tick();
		phantom_tick();
		if (r <= 0) continue;
		if (p[0].revents & POLLIN) {
			int fd = accept4(lfd, NULL, NULL, SOCK_CLOEXEC);
			if (fd >= 0) {
				int slot = -1;
				for (int i = 0; i < MAX_CLIENTS; i++) if (cl[i].fd < 0) { slot = i; break; }
				if (slot < 0) close(fd);
				else { cl[slot].fd = fd; cl[slot].len = 0; cl[slot].buf = NULL; }
			}
		}
		for (int k = 1; k < n; k++)
			if (p[k].revents & (POLLIN | POLLHUP | POLLERR)) serve(db, &cl[idx[k - 1]]);
	}
}
