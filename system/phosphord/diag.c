/* phosphord: runs the diag module (whole-system checks) in the background and keeps the results.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * diag.sh --kv prints one "area.key=level|message" line per check and a "summary=level|text" line.
 * It takes up to ~20 s (network checks), so it never runs on the API thread: a request starts it,
 * the shell asks again for the results. "bundle" makes the support .tar.gz (passwords removed). */
#include "diag.h"
#include "log.h"
#include "paths.h"
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static int busy;                 /* 0 idle, 1 checking, 2 making a bundle */
static char *last;               /* last --kv output */
static time_t last_at;
static char bundle_path[600], err_text[200];

void diag_script(char *out, size_t n)
{
	const char *e = getenv("PHOS_DIAG");
	if (e && *e) snprintf(out, n, "%s", e);
	else snprintf(out, n, "%s/phosphor/modules/diag/diag.sh", p_fat());
}

/* runs bash diag.sh args...; stdout returned (malloc'd); NULL on failure. Killed after `limit` s. */
static char *run(const char *const *args, int limit)
{
	char script[600], env_fat[640], env_root[660], env_tmp[640], env_dfat[640];
	diag_script(script, sizeof(script));
	if (!file_exists(script)) { snprintf(err_text, sizeof(err_text), "The diagnostics module isn't installed"); return NULL; }
	/* the module's own paths follow phosphord's (they differ only in tests) */
	snprintf(env_fat, sizeof(env_fat), "FAT=%s", p_fat());
	snprintf(env_dfat, sizeof(env_dfat), "DIAG_FAT=%s", p_fat());
	snprintf(env_root, sizeof(env_root), "PHOSPHOR_ROOT=%s/phosphor", p_fat());
	snprintf(env_tmp, sizeof(env_tmp), "PHOSPHOR_TMP=%s", p_tmp());
	const char *argv[16];
	int a = 0;
	argv[a++] = "bash";
	argv[a++] = script;
	for (int i = 0; args[i] && a < 15; i++) argv[a++] = args[i];
	argv[a] = NULL;
	char *envp[200];
	int e = 0;
	for (char **p = environ; *p && e < 190; p++)
		if (strncmp(*p, "DIAG_FAT=", 9) && strncmp(*p, "PHOSPHOR_ROOT=", 14) && strncmp(*p, "PHOSPHOR_TMP=", 13)) envp[e++] = *p;
	if (getenv("PHOS_FAT")) envp[e++] = env_dfat;   /* only redirect in tests; diag's defaults are the real paths */
	envp[e++] = env_root;
	envp[e++] = env_tmp;
	envp[e] = NULL;
	(void)env_fat;

	int po[2];
	if (pipe2(po, O_CLOEXEC)) return NULL;   /* no copies leak into other threads' children */
	fcntl(po[0], F_SETFL, O_NONBLOCK);
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, po[1], 1);
	posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
	posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
	posix_spawn_file_actions_addclose(&fa, po[0]);
	posix_spawnattr_t at;
	posix_spawnattr_init(&at);
	posix_spawnattr_setflags(&at, POSIX_SPAWN_SETPGROUP);   /* own group: a timeout stops all of it */
	posix_spawnattr_setpgroup(&at, 0);
	pid_t pid;
	int rc = posix_spawnp(&pid, "bash", &fa, &at, (char *const *)argv, envp);
	posix_spawn_file_actions_destroy(&fa);
	posix_spawnattr_destroy(&at);
	close(po[1]);
	if (rc) { close(po[0]); snprintf(err_text, sizeof(err_text), "Couldn't start the checks"); return NULL; }
	size_t cap = 16384, len = 0;
	char *buf = malloc(cap);
	time_t end = time(NULL) + limit;
	int st = 0, done = 0;
	while (!done) {
		ssize_t r = read(po[0], buf + len, cap - len - 1);
		if (r > 0) { len += (size_t)r; if (len + 4096 > cap) { cap *= 2; buf = realloc(buf, cap); } continue; }
		if (waitpid(pid, &st, WNOHANG) == pid) {
			/* drain what's left */
			while ((r = read(po[0], buf + len, cap - len - 1)) > 0) { len += (size_t)r; if (len + 4096 > cap) { cap *= 2; buf = realloc(buf, cap); } }
			done = 1;
			break;
		}
		if (time(NULL) > end) {
			kill(-pid, SIGKILL);
			waitpid(pid, &st, 0);
			plog("diag: stopped after %d s", limit);
			snprintf(err_text, sizeof(err_text), "The checks took too long and were stopped");
			break;
		}
		usleep(100000);
	}
	close(po[0]);
	buf[len] = 0;
	return buf;
}

static void *worker(void *arg)
{
	int what = (int)(long)arg;
	if (what == 1) {
		const char *args[] = { "--kv", NULL };
		char *out = run(args, 180);
		pthread_mutex_lock(&lk);
		if (out) { free(last); last = out; last_at = time(NULL); err_text[0] = 0; }
		busy = 0;
		pthread_mutex_unlock(&lk);
		plog("diag: checks finished");
	} else {
		const char *args[] = { "bundle", NULL };
		char *out = run(args, 240);
		pthread_mutex_lock(&lk);
		bundle_path[0] = 0;
		if (out) {
			char *nl = strrchr(out, '\n');
			while (nl && nl[1] == 0 && nl > out) { *nl = 0; nl = strrchr(out, '\n'); }
			const char *line = nl ? nl + 1 : out;           /* the path is the last line */
			if (strstr(line, ".tar.gz")) snprintf(bundle_path, sizeof(bundle_path), "%s", line);
			else snprintf(err_text, sizeof(err_text), "Couldn't make the report");
			free(out);
		}
		busy = 0;
		pthread_mutex_unlock(&lk);
		plog("diag: bundle %s", bundle_path[0] ? bundle_path : "failed");
	}
	return NULL;
}

static int start(int what)
{
	pthread_mutex_lock(&lk);
	if (busy) { pthread_mutex_unlock(&lk); return 1; }
	busy = what;
	if (what == 2) bundle_path[0] = 0;
	pthread_mutex_unlock(&lk);
	pthread_t t;
	if (pthread_create(&t, NULL, worker, (void *)(long)what)) { busy = 0; return -1; }
	pthread_detach(t);
	return 0;
}

int diag_run(void) { return start(1); }
int diag_bundle(void) { return start(2); }

void diag_state(diag_state_t *s)
{
	pthread_mutex_lock(&lk);
	s->busy = busy;
	s->output = last ? strdup(last) : NULL;
	s->at = last_at;
	snprintf(s->bundle, sizeof(s->bundle), "%s", bundle_path);
	snprintf(s->error, sizeof(s->error), "%s", err_text);
	pthread_mutex_unlock(&lk);
	char script[600];
	diag_script(script, sizeof(script));
	s->installed = file_exists(script);
}
