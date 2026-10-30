/* phosphord: HTTP through curl (on MiSTer's Linux), with its certificate bundle.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * curl does TLS, redirects and timeouts, so phosphord carries no network code of its own.
 *   PHOS_CURL    curl to run (default: curl on the PATH)
 *   PHOS_CACERT  certificate bundle (default: /etc/ssl/certs/cacert.pem when it exists) */
#include "net.h"
#include "log.h"
#include "paths.h"
#include "version.h"
#include <ctype.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

void net_escape(const char *in, char *out, size_t n)
{
	static const char *hex = "0123456789ABCDEF";
	size_t j = 0;
	for (const unsigned char *p = (const unsigned char *)in; *p && j + 4 < n; p++) {
		if (isalnum(*p) || strchr("-._~", *p)) out[j++] = (char)*p;
		else { out[j++] = '%'; out[j++] = hex[*p >> 4]; out[j++] = hex[*p & 15]; }
	}
	out[j] = 0;
}

static int result_of(int ok, int http)
{
	if (ok && http >= 200 && http < 300) return 0;
	if (http == 404) return 1;
	if (http == 401 || http == 403) return 2;
	return -1;
}

static __thread volatile int *cancel_flag;
void net_set_cancel(volatile int *flag) { cancel_flag = flag; }

/* runs curl; its stdout goes to *out (when out) — with -o, stdout carries only the status code */
static int run_curl(const char *const *args, const char *body, char **out, size_t *outlen)
{
	const char *curl = getenv("PHOS_CURL") ? getenv("PHOS_CURL") : "curl";
	const char *ca = getenv("PHOS_CACERT");
	if (!ca && file_exists("/etc/ssl/certs/cacert.pem")) ca = "/etc/ssl/certs/cacert.pem";
	const char *argv[64];
	int a = 0;
	argv[a++] = curl;
	argv[a++] = "-sSL";
	argv[a++] = "--max-time"; argv[a++] = "300";
	argv[a++] = "--connect-timeout"; argv[a++] = "15";
	argv[a++] = "-A"; argv[a++] = "Phosphor/" PHOSD_VERSION " (MiSTer; https://phosphor-os.github.io)";
	if (ca) { argv[a++] = "--cacert"; argv[a++] = ca; }
	for (int i = 0; args[i] && a < 62; i++) argv[a++] = args[i];
	argv[a] = NULL;

	/* close-on-exec: other threads spawn too, and a stray copy of the write end means no EOF here
	   (dup2 onto 0-2 in the child clears it for curl's own copy) */
	int po[2], pi[2] = { -1, -1 };
	if (pipe2(po, O_CLOEXEC)) return -1;
	if (body && pipe2(pi, O_CLOEXEC)) { close(po[0]); close(po[1]); return -1; }
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, po[1], 1);
	posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
	posix_spawn_file_actions_addclose(&fa, po[0]);
	if (body) { posix_spawn_file_actions_adddup2(&fa, pi[0], 0); posix_spawn_file_actions_addclose(&fa, pi[1]); }
	else posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
	pid_t pid;
	int rc = posix_spawnp(&pid, curl, &fa, NULL, (char *const *)argv, environ);
	posix_spawn_file_actions_destroy(&fa);
	close(po[1]);
	if (body) close(pi[0]);
	if (rc) {
		close(po[0]);
		if (body) close(pi[1]);
		plog("can't run %s", curl);
		return -1;
	}
	if (body) {
		size_t left = strlen(body);
		const char *p = body;
		while (left) { ssize_t w = write(pi[1], p, left); if (w <= 0) break; p += w; left -= (size_t)w; }
		close(pi[1]);
	}
	size_t cap = 4096, len = 0;
	char *buf = malloc(cap);
	int cancelled = 0;
	for (;;) {
		if (cancel_flag && *cancel_flag) { kill(pid, SIGTERM); cancelled = 1; break; }
		struct pollfd pf = { po[0], POLLIN, 0 };
		if (poll(&pf, 1, 200) == 0) continue;           /* (a look at the cancel flag 5 times a second) */
		if (len + 4096 > cap) { cap *= 2; buf = realloc(buf, cap); }
		ssize_t r = read(po[0], buf + len, cap - len - 1);
		if (r <= 0) break;
		len += (size_t)r;
	}
	buf[len] = 0;
	close(po[0]);
	int st;
	waitpid(pid, &st, 0);
	if (cancelled) { *out = buf; if (outlen) *outlen = len; return -1; }
	*out = buf;
	if (outlen) *outlen = len;
	return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

int net_get_file(const char *url, const char *dest)
{
	char tmp[1100];
	snprintf(tmp, sizeof(tmp), "%s.part", dest);
	const char *args[] = { "-w", "%{http_code}", "-o", tmp, url, NULL };
	char *out = NULL;
	int ex = run_curl(args, NULL, &out, NULL);
	int http = out ? atoi(out) : 0;
	free(out);
	int r = result_of(ex == 0, http);
	if (r == 0 && rename(tmp, dest) != 0) r = -1;
	if (r != 0) unlink(tmp);
	return r;
}

int net_request(const char *method, const char *url, const char *const *headers, const char *body,
                char **out, size_t *outlen, int *http)
{
	const char *args[48];
	int a = 0;
	args[a++] = "-X"; args[a++] = method;
	for (int i = 0; headers && headers[i] && a < 40; i++) { args[a++] = "-H"; args[a++] = headers[i]; }
	if (body) { args[a++] = "--data-binary"; args[a++] = "@-"; }
	args[a++] = "-w"; args[a++] = "\n%{http_code}";
	args[a++] = url;
	args[a] = NULL;
	char *buf = NULL;
	size_t len = 0;
	int ex = run_curl(args, body, &buf, &len);
	/* the last line is the status code */
	int code = 0;
	char *nl = buf ? strrchr(buf, '\n') : NULL;
	if (nl) { code = atoi(nl + 1); *nl = 0; len = (size_t)(nl - buf); }
	if (http) *http = code;
	int r = result_of(ex == 0, code);
	if (out) { *out = buf; if (outlen) *outlen = len; }
	else free(buf);
	return r;
}
