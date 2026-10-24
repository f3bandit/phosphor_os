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
		if (send_all(req, (size_t)n) != 0) { hang_up(); continue; }
		char *line = read_line(5000);
		if (!line) { hang_up(); continue; }
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
