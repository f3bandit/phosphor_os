/* Phosphor shell: client for phosphord, the system service.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * One connection to <tmp>/phosphord.sock, kept open. A request is one JSON line, the reply one
 * JSON line. If phosphord isn't running, the shell starts it (PHOSD_BIN, default
 * /media/fat/phosphor/bin/phosphord) and tries again. */
#include "api.h"
#include "log.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static int fd = -1;
static char *rbuf;
static size_t rlen;

static const char *CAT_NAMES[CAT_COUNT] = { "Disc", "Cartridge", "Handheld", "Arcade", "Computer" };
const char *category_name(category_t c) { return c < CAT_COUNT ? CAT_NAMES[c] : "?"; }

static const char *sock_path(void)
{
	static char p[300];
	const char *t = getenv("PHOS_TMP");
	snprintf(p, sizeof(p), "%s/phosphord.sock", t && *t ? t : "/tmp/phosphor");
	return p;
}

static int try_connect(void)
{
	int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (s < 0) return -1;
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	snprintf(a.sun_path, sizeof(a.sun_path), "%s", sock_path());
	if (connect(s, (struct sockaddr *)&a, sizeof(a)) < 0) { close(s); return -1; }
	return s;
}

static void start_service(void)
{
	const char *bin = getenv("PHOSD_BIN");
	if (!bin || !*bin) bin = "/media/fat/phosphor/bin/phosphord";
	if (access(bin, X_OK) != 0) { plog("phosphord isn't running and %s isn't there", bin); return; }
	plog("starting %s", bin);
	pid_t pid = fork();
	if (pid == 0) {
		setsid();
		int n = open("/dev/null", O_RDWR);
		dup2(n, 0); dup2(n, 1); dup2(n, 2);
		execl(bin, bin, (char *)NULL);
		_exit(127);
	}
	for (int i = 0; i < 30; i++) {   /* up to 3 s for it to come up */
		usleep(100000);
		if ((fd = try_connect()) >= 0) return;
	}
}

static int ensure(void)
{
	if (fd >= 0) return 0;
	fd = try_connect();
	if (fd < 0) start_service();
	return fd >= 0 ? 0 : -1;
}

static void hang_up(void)
{
	if (fd >= 0) close(fd);
	fd = -1;
	rlen = 0;
}

static int send_all(const char *s, size_t n)
{
	while (n) {
		ssize_t w = send(fd, s, n, MSG_NOSIGNAL);      /* phosphord restarted (an update): an error, not SIGPIPE */
		if (w < 0) { if (errno == EINTR) continue; return -1; }
		s += w;
		n -= (size_t)w;
	}
	return 0;
}

/* read until a full line, up to ms */
static char *read_line(int ms)
{
	for (;;) {
		char *nl = rbuf ? memchr(rbuf, '\n', rlen) : NULL;
		if (nl) {
			size_t n = (size_t)(nl - rbuf);
			char *line = malloc(n + 1);
			memcpy(line, rbuf, n);
			line[n] = 0;
			memmove(rbuf, nl + 1, rlen - n - 1);
			rlen -= n + 1;
			return line;
		}
		struct pollfd p = { fd, POLLIN, 0 };
		if (poll(&p, 1, ms) <= 0) return NULL;
		char tmp[16384];
		ssize_t r = read(fd, tmp, sizeof(tmp));
		if (r <= 0) return NULL;
		rbuf = realloc(rbuf, rlen + (size_t)r + 1);
		memcpy(rbuf + rlen, tmp, (size_t)r);
		rlen += (size_t)r;
	}
}

cJSON *api_call(const char *fmt, ...)
{
	char req[2048];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(req, sizeof(req) - 2, fmt, ap);
	va_end(ap);
	if (n < 0 || n >= (int)sizeof(req) - 2) return NULL;
	req[n++] = '\n';
	req[n] = 0;
	for (int attempt = 0; attempt < 2; attempt++) {
		if (ensure() != 0) return NULL;
		if (send_all(req, (size_t)n) != 0) { hang_up(); continue; }   /* not sent: try once more */
		struct timespec t0, t1;
		clock_gettime(CLOCK_MONOTONIC, &t0);
		char *line = read_line(5000);
		if (!line) {
			hang_up();
			/* closed at once: an old connection phosphord had already dropped (it restarted), so the
			   request never reached it: once more on a new one. Sent but no answer: never sent again
			   (a slow launch would run twice). */
			clock_gettime(CLOCK_MONOTONIC, &t1);
			long ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
			if (attempt == 0 && ms < 50) continue;
			plog("slow: phosphord didn't answer in %ld ms: %.80s", ms, req);
			return NULL;
		}
		clock_gettime(CLOCK_MONOTONIC, &t1);
		long ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
		if (ms >= 300) plog("slow: phosphord took %ld ms for %.80s", ms, req);   /* (the screen waits meanwhile) */
		cJSON *r = cJSON_Parse(line);
		free(line);
		return r;
	}
	return NULL;
}

int api_ok(const cJSON *r)
{
	return r && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(r, "ok"));
}

const char *api_str(const cJSON *o, const char *key)
{
	const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
	return cJSON_IsString(v) ? v->valuestring : NULL;
}

double api_num(const cJSON *o, const char *key, double def)
{
	const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
	return cJSON_IsNumber(v) ? v->valuedouble : def;
}

int mister_cmd(const char *cmd, char *err, unsigned long errn)
{
	const char *dev = getenv("PHOS_MISTER_CMD");
	if (!dev || !*dev) dev = "/dev/MiSTer_cmd";
	int f = open(dev, O_WRONLY | O_NONBLOCK | O_APPEND | (strncmp(dev, "/dev/", 5) ? O_CREAT : 0), 0644);
	if (f < 0) { snprintf(err, errn, "MiSTer isn't listening on %s (%s)", dev, strerror(errno)); return -1; }
	ssize_t w = write(f, cmd, strlen(cmd));
	close(f);
	if (w < 0) { snprintf(err, errn, "couldn't send the command to MiSTer (%s)", strerror(errno)); return -1; }
	return 0;
}

/* one request that takes a while (the safe stop before switching off): up to ms for the answer, and
   never sent twice (it isn't something to do again) */
cJSON *api_call_slow(int ms, const char *fmt, ...)
{
	char req[2048];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(req, sizeof(req) - 2, fmt, ap);
	va_end(ap);
	if (n < 0 || n >= (int)sizeof(req) - 2) return NULL;
	req[n++] = '\n';
	req[n] = 0;
	if (ensure() != 0) return NULL;
	/* not sent (phosphord restarted since the last request, an update): sent again on a new connection */
	if (send_all(req, (size_t)n) != 0) { hang_up(); if (ensure() != 0 || send_all(req, (size_t)n) != 0) { hang_up(); return NULL; } }
	char *line = read_line(ms);
	if (!line) { hang_up(); return NULL; }
	cJSON *r = cJSON_Parse(line);
	free(line);
	return r;
}

/* a fresh connection for the next request (phosphord was restarted under us, by an update) */
void api_reset(void) { hang_up(); }

/* ---------- requests in the background (api_bg_ask / api_bg_take) ----------
 * Polls the screens make again and again (a controller live, the controller list, box art and sharing
 * state) go through a thread of their own with its own connection, so the menu never waits for them:
 * phosphord answers one request at a time, and while it works one out the menu used to stand still (the
 * cursor stopped, then the presses came all at once). The newest reply per key is kept for the screen to
 * take on its next pass; a request asked again before it was sent is replaced, not queued twice. */
#include <pthread.h>
#define BG_SLOTS 16
static struct { char key[40]; char *req; cJSON *reply; int fresh; } bg[BG_SLOTS];
static pthread_mutex_t bg_lk = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t bg_cv = PTHREAD_COND_INITIALIZER;
static int bg_started;

static cJSON *bg_request(int *bfd, const char *req)
{
	for (int attempt = 0; attempt < 2; attempt++) {
		if (*bfd < 0) *bfd = try_connect();
		if (*bfd < 0) return NULL;                       /* (the menu's own requests start phosphord) */
		size_t n = strlen(req), off = 0;
		while (off < n) { ssize_t w = send(*bfd, req + off, n - off, MSG_NOSIGNAL); if (w <= 0) break; off += (size_t)w; }
		if (off < n) { close(*bfd); *bfd = -1; continue; }
		char *buf = NULL;
		size_t len = 0;
		for (;;) {
			struct pollfd p = { *bfd, POLLIN, 0 };
			if (poll(&p, 1, 8000) <= 0) break;
			char tmp[16384];
			ssize_t r = read(*bfd, tmp, sizeof(tmp));
			if (r <= 0) break;
			buf = realloc(buf, len + (size_t)r + 1);
			memcpy(buf + len, tmp, (size_t)r);
			len += (size_t)r;
			buf[len] = 0;
			if (memchr(buf, '\n', len)) break;
		}
		char *nl = buf ? memchr(buf, '\n', len) : NULL;
		if (!nl) { free(buf); close(*bfd); *bfd = -1; if (attempt == 0 && len == 0) continue; return NULL; }
		*nl = 0;
		cJSON *r = cJSON_Parse(buf);
		free(buf);
		return r;
	}
	return NULL;
}

static void *bg_thread(void *u)
{
	(void)u;
	int bfd = -1;
	for (;;) {
		pthread_mutex_lock(&bg_lk);
		int i;
		for (;;) {
			for (i = 0; i < BG_SLOTS && !(bg[i].key[0] && bg[i].req); i++) {}
			if (i < BG_SLOTS) break;
			pthread_cond_wait(&bg_cv, &bg_lk);
		}
		char *req = bg[i].req, key[40];
		bg[i].req = NULL;
		snprintf(key, sizeof(key), "%s", bg[i].key);
		pthread_mutex_unlock(&bg_lk);
		cJSON *r = bg_request(&bfd, req);
		free(req);
		pthread_mutex_lock(&bg_lk);
		for (i = 0; i < BG_SLOTS; i++) if (!strcmp(bg[i].key, key)) break;
		if (i < BG_SLOTS) { cJSON_Delete(bg[i].reply); bg[i].reply = r; bg[i].fresh = 1; } else cJSON_Delete(r);
		pthread_mutex_unlock(&bg_lk);
	}
	return NULL;
}

void api_bg_ask(const char *key, const char *fmt, ...)
{
	char req[2048];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(req, sizeof(req) - 2, fmt, ap);
	va_end(ap);
	if (n < 0 || n >= (int)sizeof(req) - 2) return;
	req[n++] = '\n';
	req[n] = 0;
	pthread_mutex_lock(&bg_lk);
	if (!bg_started) {
		pthread_t t;
		if (pthread_create(&t, NULL, bg_thread, NULL) == 0) { pthread_detach(t); bg_started = 1; }
	}
	int i, freei = -1;
	for (i = 0; i < BG_SLOTS && strcmp(bg[i].key, key); i++) if (!bg[i].key[0] && freei < 0) freei = i;
	if (i == BG_SLOTS) i = freei;
	if (i >= 0) {
		snprintf(bg[i].key, sizeof(bg[i].key), "%s", key);
		free(bg[i].req);
		bg[i].req = strdup(req);
		pthread_cond_signal(&bg_cv);
	}
	pthread_mutex_unlock(&bg_lk);
}

cJSON *api_bg_take(const char *key)
{
	cJSON *r = NULL;
	pthread_mutex_lock(&bg_lk);
	for (int i = 0; i < BG_SLOTS; i++)
		if (!strcmp(bg[i].key, key) && bg[i].fresh) { r = bg[i].reply; bg[i].reply = NULL; bg[i].fresh = 0; break; }
	pthread_mutex_unlock(&bg_lk);
	return r;
}

int api_bg_waiting(const char *key)
{
	int w = 0;
	pthread_mutex_lock(&bg_lk);
	for (int i = 0; i < BG_SLOTS; i++) if (!strcmp(bg[i].key, key)) w = bg[i].req != NULL;
	pthread_mutex_unlock(&bg_lk);
	return w;
}
