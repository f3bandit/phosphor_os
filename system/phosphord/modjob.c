/* phosphord: runs a phosphor-modules script, quickly in the foreground or one job at a time in the background.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * The script gets phosphord's own paths (PHOSPHOR_ROOT, PHOSPHOR_TMP, the CORENAME file), runs in its own
 * process group so a timeout stops all of it, and its output is kept for the screen to show. */
#include "modjob.h"
#include "log.h"
#include "paths.h"
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static void script(modjob_t *m, char *o, size_t n)
{
	const char *e = m->script_env ? getenv(m->script_env) : NULL;
	if (e && *e) snprintf(o, n, "%s", e);
	else snprintf(o, n, "%s/phosphor/modules/%s/%s.sh", p_fat(), m->module, m->module);
}

int modjob_installed(modjob_t *m)
{
	char sc[600];
	script(m, sc, sizeof(sc));
	return file_exists(sc);
}

char *modjob_run(modjob_t *m, const char *const *args, int limit, int *rc)
{
	char sc[600], env_root[660], env_tmp[640], env_core[700];
	script(m, sc, sizeof(sc));
	*rc = -1;
	if (!file_exists(sc)) {
		char msg[200];
		snprintf(msg, sizeof(msg), "The %s module isn't installed", m->module);
		return strdup(msg);
	}
	snprintf(env_root, sizeof(env_root), "PHOSPHOR_ROOT=%s/phosphor", p_fat());
	snprintf(env_tmp, sizeof(env_tmp), "PHOSPHOR_TMP=%s", p_tmp());
	snprintf(env_core, sizeof(env_core), "%s=%s", m->core_env ? m->core_env : "PHOSD_UNUSED", p_corename());
	size_t cl = strcspn(env_core, "=") + 1;
	const char *argv[40];
	int a = 0;
	argv[a++] = "bash";
	argv[a++] = sc;
	for (int i = 0; args && args[i] && a < 39; i++) argv[a++] = args[i];
	argv[a] = NULL;
	char *envp[200];
	int e = 0;
	for (char **p = environ; *p && e < 190; p++)
		if (strncmp(*p, "PHOSPHOR_ROOT=", 14) && strncmp(*p, "PHOSPHOR_TMP=", 13) && strncmp(*p, env_core, cl)) envp[e++] = *p;
	envp[e++] = env_root;
	envp[e++] = env_tmp;
	envp[e++] = env_core;
	envp[e] = NULL;

	int po[2];
	if (pipe(po)) return strdup("Couldn't start it");
	fcntl(po[0], F_SETFL, O_NONBLOCK);
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, po[1], 1);
	posix_spawn_file_actions_adddup2(&fa, po[1], 2);
	posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
	posix_spawn_file_actions_addclose(&fa, po[0]);
	posix_spawnattr_t at;
	posix_spawnattr_init(&at);
	posix_spawnattr_setflags(&at, POSIX_SPAWN_SETPGROUP);
	posix_spawnattr_setpgroup(&at, 0);
	pid_t pid;
	int r = posix_spawnp(&pid, "bash", &fa, &at, (char *const *)argv, envp);
	posix_spawn_file_actions_destroy(&fa);
	posix_spawnattr_destroy(&at);
	close(po[1]);
	if (r) { close(po[0]); return strdup("Couldn't start it"); }
	size_t cap = 8192, len = 0;
	char *buf = malloc(cap);
	time_t end = time(NULL) + limit;
	int st = 0;
	for (;;) {
		ssize_t n = read(po[0], buf + len, cap - len - 1);
		if (n > 0) {
			len += (size_t)n;
			if (len + 2048 > cap) {
				if (cap < (1 << 20)) { cap *= 2; buf = realloc(buf, cap); }
				else len = 0;          /* very long output: keep the end */
			}
			continue;
		}
		if (waitpid(pid, &st, WNOHANG) == pid) {
			while (len + 1 < cap && (n = read(po[0], buf + len, cap - len - 1)) > 0) len += (size_t)n;
			*rc = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
			break;
		}
		if (time(NULL) > end) {
			kill(-pid, SIGKILL);
			waitpid(pid, &st, 0);
			plog("%s: %s stopped after %d s", m->module, args && args[0] ? args[0] : "", limit);
			len = (size_t)snprintf(buf, cap, "It took too long and was stopped.");
			break;
		}
		usleep(100000);
	}
	close(po[0]);
	buf[len] = 0;
	return buf;
}

typedef struct { modjob_t *m; char job[24]; char *argv[24]; int limit; } work_t;

static void *worker(void *arg)
{
	work_t *w = arg;
	int rc;
	char *o = modjob_run(w->m, (const char *const *)w->argv, w->limit, &rc);
	pthread_mutex_lock(&w->m->lk);
	free(w->m->out);
	w->m->out = o;
	w->m->at = time(NULL);
	w->m->ok = rc == 0;
	w->m->busy = 0;
	pthread_mutex_unlock(&w->m->lk);
	plog("%s: %s %s", w->m->module, w->job, rc == 0 ? "done" : "failed");
	for (int i = 0; w->argv[i]; i++) free(w->argv[i]);
	free(w);
	return NULL;
}

int modjob_start(modjob_t *m, const char *job, const char *const *args, int limit)
{
	if (!modjob_installed(m)) return -1;
	pthread_mutex_lock(&m->lk);
	if (m->busy) { pthread_mutex_unlock(&m->lk); return 1; }
	m->busy = 1;
	snprintf(m->job, sizeof(m->job), "%s", job);
	pthread_mutex_unlock(&m->lk);
	work_t *w = calloc(1, sizeof(*w));
	w->m = m;
	w->limit = limit;
	snprintf(w->job, sizeof(w->job), "%s", job);
	int a = 0;
	for (int i = 0; args && args[i] && a < 23; i++) w->argv[a++] = strdup(args[i]);
	w->argv[a] = NULL;
	pthread_t t;
	if (pthread_create(&t, NULL, worker, w)) {
		pthread_mutex_lock(&m->lk);
		m->busy = 0;
		pthread_mutex_unlock(&m->lk);
		for (int i = 0; w->argv[i]; i++) free(w->argv[i]);
		free(w);
		return -1;
	}
	pthread_detach(t);
	return 0;
}

void modjob_last(modjob_t *m, char *job, size_t jn, int *busy, int *ok, char **out, time_t *at)
{
	pthread_mutex_lock(&m->lk);
	snprintf(job, jn, "%s", m->job);
	*busy = m->busy;
	*ok = m->ok;
	*out = m->out ? strdup(m->out) : NULL;
	*at = m->at;
	pthread_mutex_unlock(&m->lk);
}
