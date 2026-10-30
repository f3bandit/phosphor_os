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
#include "vtry.h"
#include "remote.h"
#include "ra.h"
#include "music.h"
#include "disc.h"
#include "phantom.h"
#include "storage.h"
#include "memcard.h"
#include "wifi.h"
#include "db.h"
#include "dump.h"
#include "indexer.h"
#include "log.h"
#include "paths.h"
#include "play.h"
#include "scraper.h"
#include "systems.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS 16
#define LINE_MAX_ (1024 * 1024)        /* a whole memory card (.mcd) imported is ~175 KB of JSON */

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

static pthread_mutex_t dw_lk = PTHREAD_MUTEX_INITIALIZER;
static char dw_key[96];
static int dw_audio;
static void *disc_watch(void *u)
{
	(void)u;
	for (;;) {
		char key[96];
		int audio;
		disc_brief(key, sizeof(key), &audio);
		pthread_mutex_lock(&dw_lk);
		snprintf(dw_key, sizeof(dw_key), "%s", key);
		dw_audio = audio;
		pthread_mutex_unlock(&dw_lk);
		sleep(2);
	}
	return NULL;
}

int main(int argc, char **argv)
{
	const char *serial = NULL;
	/* the video timer switches the profile back with this (vtry.c) */
	if (argc == 3 && !strcmp(argv[1], "--altcfg")) return vt_altcfg(atoi(argv[2])) < 0;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--log-serial")) serial = (i + 1 < argc && argv[i + 1][0] == '/') ? argv[++i] : "/dev/ttyS0";
		else { fprintf(stderr, "usage: phosphord [--log-serial [DEV]]\n"); return 2; }
	}
	signal(SIGPIPE, SIG_IGN);
	mkdirs(p_tmp());
	log_open(serial);
	plog("starting");
	systems_load();
	vt_boot();                                /* a video try that was never kept goes back */
	sqlite3 *db = db_open();
	if (!db) return 1;
	int lfd = listen_socket();
	if (lfd < 0) return 1;
	for (int i = 0; i < MAX_CLIENTS; i++) cl[i].fd = -1;
	busy_init();
	dump_cleanup();                 /* (before the library looks: half-copied discs never get listed) */
	indexer_relocate(db);                    /* (a drive that came up under another number since last time) */
	indexer_start();
	scraper_start();
	ra_init();
	music_init();
	remote_start();
	storage_prime();                          /* the drive list, in the background */
	api_prime_updates();                      /* the update packages too */
	memcard_start();
	wifi_start();
	{ /* the heartbeat log (a testing aid until 0.27.30) is gone: its files go too */
		char hb[512];
		snprintf(hb, sizeof(hb), "%s/phosphor/logs/heartbeat.log", p_fat()); unlink(hb);
		snprintf(hb, sizeof(hb), "%s/phosphor/logs/heartbeat.log.1", p_fat()); unlink(hb);
	}

	plog("ready on %s", p_socket());
	{ pthread_t t; if (pthread_create(&t, NULL, disc_watch, NULL) == 0) pthread_detach(t); }
	/* a drive mounted or unmounted: the kernel says so on /proc/self/mounts (no polling). A drive that came
	   up under another /media/usb<n> (a stick plugged in first) has its games moved there in the library */
	int mfd = open("/proc/self/mounts", O_RDONLY | O_CLOEXEC);
	for (;;) {
		struct pollfd p[MAX_CLIENTS + 2];
		int n = 0, idx[MAX_CLIENTS];
		p[n++] = (struct pollfd){ lfd, POLLIN, 0 };
		for (int i = 0; i < MAX_CLIENTS; i++)
			if (cl[i].fd >= 0) { idx[n - 1] = i; p[n++] = (struct pollfd){ cl[i].fd, POLLIN, 0 }; }
		int mi = -1;
		if (mfd >= 0) { mi = n; p[n++] = (struct pollfd){ mfd, POLLPRI, 0 }; }
		int r = poll(p, (nfds_t)n, 2000);   /* wakes every 2 s only to keep play time */
		if (mi >= 0 && (p[mi].revents & (POLLPRI | POLLERR))) {
			char b[4096];
			lseek(mfd, 0, SEEK_SET);
			while (read(mfd, b, sizeof(b)) > 0) {}  /* (read through: the next change is signalled again) */
			indexer_relocate(db);
		}
		/* (the ticks run between requests: one that takes long holds every request up; logged) */
		struct timespec tk0, tk1, tk2, tk3;
		clock_gettime(CLOCK_MONOTONIC, &tk0);
		play_tick(db);
		clock_gettime(CLOCK_MONOTONIC, &tk1);
		{   /* the music player: a game started (it stops), a music CD in or out */
			/* the dock's drive, from Phosphor's Main, in its own thread (disc_watch): asking here took up to
			   0.5 s while Main was busy (the controller test), and phosphord answers one request at a time,
			   so every request waited behind it */
			static time_t mt;
			if (time(NULL) - mt >= 2) {
				mt = time(NULL);
				char key[96];
				int audio;
				pthread_mutex_lock(&dw_lk);
				snprintf(key, sizeof(key), "%s", dw_key);
				audio = dw_audio;
				pthread_mutex_unlock(&dw_lk);
				music_tick(key, audio);
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &tk2);
		phantom_tick();
		clock_gettime(CLOCK_MONOTONIC, &tk3);
		{
#define MSD(a, b) ((long)(((b).tv_sec - (a).tv_sec) * 1000 + ((b).tv_nsec - (a).tv_nsec) / 1000000))
			long all = MSD(tk0, tk3);
			if (all >= 150) plog("slow tick: %ld ms (play %ld, music %ld, phantom %ld)", all, MSD(tk0, tk1), MSD(tk1, tk2), MSD(tk2, tk3));
#undef MSD
		}
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
			if (k != mi && (p[k].revents & (POLLIN | POLLHUP | POLLERR))) serve(db, &cl[idx[k - 1]]);
	}
}
