/* phosphord: drives the cloud-sync module (saves to the user's own cloud storage).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Quick things (status, storage places) are asked for on each request; anything that uses the network
 * (setup, send, get back, test, list MiSTers) runs one at a time in the background, killed after 15
 * minutes, and the screen asks again for its output. What never goes to the cloud is decided by the
 * module itself (WiFi, Bluetooth pairings, passwords, keys are on its fixed list). */
#include "cloud.h"
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
#include <unistd.h>

extern char **environ;

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static int busy, last_ok;
static char job[24];
static char *out;
static time_t out_at;

static void script(char *o, size_t n)
{
	const char *e = getenv("PHOS_CLOUD");
	if (e && *e) snprintf(o, n, "%s", e);
	else snprintf(o, n, "%s/phosphor/modules/cloud-sync/cloud-sync.sh", p_fat());
}

/* bash cloud-sync.sh argv...; output (stdout + stderr) malloc'd, exit code in *rc */
static char *run(const char *const *args, int limit, int *rc)
{
	char sc[600], env_root[660], env_tmp[640];
	script(sc, sizeof(sc));
	*rc = -1;
	if (!file_exists(sc)) return strdup("The cloud-sync module isn't installed");
	snprintf(env_root, sizeof(env_root), "PHOSPHOR_ROOT=%s/phosphor", p_fat());
	snprintf(env_tmp, sizeof(env_tmp), "PHOSPHOR_TMP=%s", p_tmp());
	char env_core[640];
	snprintf(env_core, sizeof(env_core), "CLOUD_SYNC_CORENAME=%s", p_corename());
	const char *argv[40];
	int a = 0;
	argv[a++] = "bash";
	argv[a++] = sc;
	for (int i = 0; args[i] && a < 39; i++) argv[a++] = args[i];
	argv[a] = NULL;
	char *envp[200];
	int e = 0;
	for (char **p = environ; *p && e < 190; p++)
		if (strncmp(*p, "PHOSPHOR_ROOT=", 14) && strncmp(*p, "PHOSPHOR_TMP=", 13) && strncmp(*p, "CLOUD_SYNC_CORENAME=", 20)) envp[e++] = *p;
	envp[e++] = env_root;
	envp[e++] = env_tmp;
	envp[e++] = env_core;
	envp[e] = NULL;

	int po[2];
	if (pipe(po)) return strdup("Couldn't start cloud-sync");
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
	if (r) { close(po[0]); return strdup("Couldn't start cloud-sync"); }
	size_t cap = 8192, len = 0;
	char *buf = malloc(cap);
	time_t end = time(NULL) + limit;
	int st = 0;
	for (;;) {
		ssize_t n = read(po[0], buf + len, cap - len - 1);
		if (n > 0) { len += (size_t)n; if (len + 2048 > cap && cap < (1 << 20)) { cap *= 2; buf = realloc(buf, cap); } if (len + 2048 > cap) len = 0; continue; }
		if (waitpid(pid, &st, WNOHANG) == pid) {
			while ((n = read(po[0], buf + len, cap - len - 1)) > 0) len += (size_t)n;
			*rc = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
			break;
		}
		if (time(NULL) > end) {
			kill(-pid, SIGKILL);
			waitpid(pid, &st, 0);
			plog("cloud: %s stopped after %d s", args[0], limit);
			len = (size_t)snprintf(buf, cap, "It took too long and was stopped.");
			break;
		}
		usleep(100000);
	}
	close(po[0]);
	buf[len] = 0;
	return buf;
}

typedef struct { char job[24]; char *argv[24]; } work_t;

static void *worker(void *arg)
{
	work_t *w = arg;
	int rc;
	char *o = run((const char *const *)w->argv, 900, &rc);
	pthread_mutex_lock(&lk);
	free(out);
	out = o;
	out_at = time(NULL);
	last_ok = rc == 0;
	busy = 0;
	pthread_mutex_unlock(&lk);
	plog("cloud: %s %s", w->job, rc == 0 ? "done" : "failed");
	for (int i = 0; w->argv[i]; i++) free(w->argv[i]);
	free(w);
	return NULL;
}

int cloud_start(const char *j, const char *const *args)
{
	char sc[600];
	script(sc, sizeof(sc));
	if (!file_exists(sc)) return -1;
	pthread_mutex_lock(&lk);
	if (busy) { pthread_mutex_unlock(&lk); return 1; }
	busy = 1;
	snprintf(job, sizeof(job), "%s", j);
	pthread_mutex_unlock(&lk);
	work_t *w = calloc(1, sizeof(*w));
	snprintf(w->job, sizeof(w->job), "%s", j);
	/* the script's command: "preview" is a dry-run pull */
	int a = 0;
	w->argv[a++] = strdup(!strcmp(j, "preview") ? "pull" : j);
	for (int i = 0; args && args[i] && a < 22; i++) w->argv[a++] = strdup(args[i]);
	if (!strcmp(j, "preview")) w->argv[a++] = strdup("--dry-run");
	w->argv[a] = NULL;
	pthread_t t;
	if (pthread_create(&t, NULL, worker, w)) {
		pthread_mutex_lock(&lk);
		busy = 0;
		pthread_mutex_unlock(&lk);
		for (int i = 0; w->argv[i]; i++) free(w->argv[i]);
		free(w);
		return -1;
	}
	pthread_detach(t);
	return 0;
}

void cloud_state(cloud_state_t *s)
{
	memset(s, 0, sizeof(*s));
	char sc[600];
	script(sc, sizeof(sc));
	s->installed = file_exists(sc);
	if (s->installed) {
		int rc;
		const char *st[] = { "status", NULL }, *rm[] = { "remotes", NULL };
		s->status = run(st, 20, &rc);
		s->remotes = run(rm, 20, &rc);
		if (rc != 0) { free(s->remotes); s->remotes = NULL; }     /* rclone not set up yet */
	}
	pthread_mutex_lock(&lk);
	snprintf(s->job, sizeof(s->job), "%s", job);
	s->busy = busy;
	s->ok = last_ok;
	s->output = out ? strdup(out) : NULL;
	s->at = out_at;
	pthread_mutex_unlock(&lk);
}
