/* phosphord: get or update cores with MiSTer's own downloader (Scripts/update.sh), on request.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Runs only when asked (the Welcome screen's "Get cores", Settings): MiSTer's update.sh fetches
 * MiSTer-devel's downloader and installs or updates cores, filters and the rest the same way it does on
 * any MiSTer. Its output goes to <fat>/phosphor/logs/downloader.log; the latest line is shown.
 *   PHOS_UPDATE_SH: the updater, for tests */
#include "cores.h"
#include "log.h"
#include "paths.h"
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static cores_state_t st;

static void updater(char *out, size_t n)
{
	const char *e = getenv("PHOS_UPDATE_SH");
	if (e && *e) snprintf(out, n, "%s", e);
	else snprintf(out, n, "%s/Scripts/update.sh", p_fat());
}

static void *run(void *arg)
{
	(void)arg;
	char sh[600], logp[600];
	updater(sh, sizeof(sh));
	snprintf(logp, sizeof(logp), "%s/phosphor/logs", p_fat());
	mkdirs(logp);
	snprintf(logp, sizeof(logp), "%s/phosphor/logs/downloader.log", p_fat());
	FILE *lf = fopen(logp, "w");
	int po[2];
	int ok = 0;
	if (pipe2(po, O_CLOEXEC) == 0) {
		posix_spawn_file_actions_t fa;
		posix_spawn_file_actions_init(&fa);
		posix_spawn_file_actions_adddup2(&fa, po[1], 1);
		posix_spawn_file_actions_adddup2(&fa, po[1], 2);
		posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
		char dir[600];
		snprintf(dir, sizeof(dir), "%s", sh);
		char *slash = strrchr(dir, '/');
		if (slash) { *slash = 0; posix_spawn_file_actions_addchdir_np(&fa, dir); }
		posix_spawnattr_t at;
		posix_spawnattr_init(&at);
		sigset_t all, none;
		sigfillset(&all);
		sigemptyset(&none);
		posix_spawnattr_setsigdefault(&at, &all);
		posix_spawnattr_setsigmask(&at, &none);
		posix_spawnattr_setflags(&at, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETPGROUP);
		const char *argv[] = { "/bin/bash", sh, NULL };
		pid_t pid;
		int rc = posix_spawn(&pid, argv[0], &fa, &at, (char *const *)argv, environ);
		posix_spawn_file_actions_destroy(&fa);
		posix_spawnattr_destroy(&at);
		close(po[1]);
		if (rc == 0) {
			FILE *in = fdopen(po[0], "r");
			char line[400];
			while (in && fgets(line, sizeof(line), in)) {
				if (lf) fputs(line, lf);
				line[strcspn(line, "\r\n")] = 0;
				/* strip colour codes; keep the last line that says something */
				char clean[200];
				size_t k = 0;
				for (const char *p = line; *p && k + 1 < sizeof(clean); p++) {
					if (*p == 0x1b) { while (*p && *p != 'm') p++; if (!*p) break; continue; }
					if ((unsigned char)*p >= 0x20) clean[k++] = *p;
				}
				clean[k] = 0;
				if (k > 2) {
					pthread_mutex_lock(&lk);
					snprintf(st.line, sizeof(st.line), "%s", clean);
					pthread_mutex_unlock(&lk);
				}
			}
			if (in) fclose(in); else close(po[0]);
			int ws = 0;
			waitpid(pid, &ws, 0);
			ok = WIFEXITED(ws) && WEXITSTATUS(ws) == 0;
		} else close(po[0]);
	}
	if (lf) fclose(lf);
	pthread_mutex_lock(&lk);
	st.busy = 0;
	st.ok = ok;
	st.at = time(NULL);
	if (!ok && !st.line[0]) snprintf(st.line, sizeof(st.line), "The downloader didn't run (is the network up?)");
	char msg[200];
	snprintf(msg, sizeof(msg), "%s", st.line);
	pthread_mutex_unlock(&lk);
	plog("cores: downloader %s: %s", ok ? "done" : "failed", msg);
	return NULL;
}

int cores_update(void)
{
	char sh[600];
	updater(sh, sizeof(sh));
	if (access(sh, R_OK)) return -1;
	pthread_mutex_lock(&lk);
	if (st.busy) { pthread_mutex_unlock(&lk); return 1; }
	st.busy = 1;
	st.ok = 0;
	snprintf(st.line, sizeof(st.line), "Starting MiSTer's downloader...");
	pthread_mutex_unlock(&lk);
	pthread_t t;
	if (pthread_create(&t, NULL, run, NULL)) { pthread_mutex_lock(&lk); st.busy = 0; pthread_mutex_unlock(&lk); return -1; }
	pthread_detach(t);
	return 0;
}

void cores_state(cores_state_t *s)
{
	char sh[600];
	updater(sh, sizeof(sh));
	pthread_mutex_lock(&lk);
	*s = st;
	pthread_mutex_unlock(&lk);
	s->available = access(sh, R_OK) == 0;
}
