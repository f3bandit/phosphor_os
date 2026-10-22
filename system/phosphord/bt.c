/* phosphord: Bluetooth controllers from the menu (pair, list, disconnect, forget) with MiSTer's own tools.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Pairing runs MiSTer's own btctl ("btctl pair", what the OSD's BT pairing runs): it looks for an input
 * device in pairing mode, pairs, trusts and connects it. phosphord stops it after the first controller
 * (or 90 s, or Cancel). The list comes from bluetoothctl (paired-devices, info); forget and disconnect
 * use btctl too. Pairings stay where MiSTer keeps them (linux/bluetooth on the SD card): never sent
 * anywhere.
 *   PHOS_BTCTL, PHOS_BLUETOOTHCTL, PHOS_BT_SYS: the tools and /sys/class/bluetooth, for tests */
#include "bt.h"
#include "log.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
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

#define PAIR_LIMIT_S 90

static const char *env_or(const char *k, const char *d) { const char *v = getenv(k); return v && *v ? v : d; }
static const char *btctl(void) { return env_or("PHOS_BTCTL", "/usr/sbin/btctl"); }
static const char *bluetoothctl(void) { return env_or("PHOS_BLUETOOTHCTL", "bluetoothctl"); }
static const char *sysdir(void) { return env_or("PHOS_BT_SYS", "/sys/class/bluetooth"); }

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static bt_state_t st;
static pid_t pair_pid;
static int cancel_req;

static int adapter_present(void)
{
	DIR *d = opendir(sysdir());
	if (!d) return 0;
	struct dirent *e;
	int n = 0;
	while ((e = readdir(d))) if (!strncmp(e->d_name, "hci", 3)) n++;
	closedir(d);
	return n > 0;
}

static int valid_mac(const char *m)
{
	if (!m || strlen(m) != 17) return 0;
	for (int i = 0; i < 17; i++) {
		if (i % 3 == 2) { if (m[i] != ':') return 0; }
		else if (!isxdigit((unsigned char)m[i])) return 0;
	}
	return 1;
}

static pid_t spawn_out(const char *const *argv, int *fd)
{
	int po[2];
	if (pipe2(po, O_CLOEXEC)) return -1;
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, po[1], 1);
	posix_spawn_file_actions_adddup2(&fa, po[1], 2);
	posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
	posix_spawnattr_t at;
	posix_spawnattr_init(&at);
	/* default signal handling in the child: phosphord may have been started with SIGINT ignored
	 * (a background job), and btctl stops cleanly only on SIGINT */
	sigset_t all, none;
	sigfillset(&all);
	sigemptyset(&none);
	posix_spawnattr_setsigdefault(&at, &all);
	posix_spawnattr_setsigmask(&at, &none);
	posix_spawnattr_setflags(&at, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK);
	posix_spawnattr_setpgroup(&at, 0);
	pid_t pid;
	int rc = posix_spawnp(&pid, argv[0], &fa, &at, (char *const *)argv, environ);
	posix_spawn_file_actions_destroy(&fa);
	posix_spawnattr_destroy(&at);
	close(po[1]);
	if (rc) { close(po[0]); return -1; }
	fcntl(po[0], F_SETFL, O_NONBLOCK);
	*fd = po[0];
	return pid;
}

/* a command's output (malloc'd) or NULL; killed after limit seconds */
static char *capture(const char *const *argv, int limit)
{
	int fd;
	pid_t pid = spawn_out(argv, &fd);
	if (pid < 0) return NULL;
	size_t cap = 8192, len = 0;
	char *buf = malloc(cap);
	if (!buf) { kill(-pid, SIGKILL); waitpid(pid, NULL, 0); close(fd); return NULL; }
	time_t end = time(NULL) + limit;
	int done = 0;
	while (!done) {
		ssize_t r = read(fd, buf + len, cap - len - 1);
		if (r > 0) {
			len += (size_t)r;
			if (len + 1024 > cap) {
				if (cap >= 1 << 20) { kill(-pid, SIGKILL); break; }
				char *nb = realloc(buf, cap *= 2);
				if (!nb) { kill(-pid, SIGKILL); break; }
				buf = nb;
			}
			continue;
		}
		if (r == 0) done = 1;                              /* closed: the command ended */
		else if (time(NULL) > end) { kill(-pid, SIGKILL); done = 1; }
		else usleep(30000);
	}
	waitpid(pid, NULL, 0);
	close(fd);
	buf[len] = 0;
	return buf;
}

static void set_msg(const char *m)
{
	pthread_mutex_lock(&lk);
	snprintf(st.message, sizeof(st.message), "%s", m);
	pthread_mutex_unlock(&lk);
}

/* btctl pair prints:  Searching... / NAME: x / MAC: y / Pairing... / Trusting... / Connecting... / Done.
 * or Failed! / Pair error! / Timed out. ; it then looks for the next controller: stopped after one */
static void *pair_thread(void *arg)
{
	(void)arg;
	const char *argv[] = { btctl(), "pair", NULL };
	int fd, ok = 0, ended = 0;
	char name[64] = "", mac[18] = "", line[256], outcome[160] = "";
	size_t ll = 0;
	pid_t pid = spawn_out(argv, &fd);
	if (pid < 0) {
		pthread_mutex_lock(&lk);
		st.busy = 0; st.ok = 0; st.at = time(NULL);
		snprintf(st.message, sizeof(st.message), "MiSTer's Bluetooth tool (btctl) isn't there");
		pthread_mutex_unlock(&lk);
		return NULL;
	}
	pthread_mutex_lock(&lk);
	pair_pid = pid;
	pthread_mutex_unlock(&lk);
	set_msg("Put the controller in pairing mode");
	time_t end = time(NULL) + PAIR_LIMIT_S;
	while (!ended) {
		char c;
		ssize_t r = read(fd, &c, 1);
		if (r == 1) {
			if (c != '\n' && ll + 1 < sizeof(line)) { line[ll++] = c; continue; }
			if (c != '\n') continue;
			line[ll] = 0;
			ll = 0;
			if (!strncmp(line, "NAME: ", 6)) snprintf(name, sizeof(name), "%.63s", line + 6);
			else if (!strncmp(line, "MAC:", 4)) {
				const char *m = line + 4;
				while (*m == ' ') m++;
				snprintf(mac, sizeof(mac), "%.17s", m);
				pthread_mutex_lock(&lk);
				snprintf(st.found, sizeof(st.found), "%s", name[0] ? name : mac);
				pthread_mutex_unlock(&lk);
			}
			else if (!strcmp(line, "Pairing...")) set_msg("Pairing...");
			else if (!strcmp(line, "Connecting...")) set_msg("Connecting...");
			else if (!strncmp(line, "Skipping", 8)) set_msg("Found something that isn't a controller; still looking");
			else if (!strcmp(line, "Done.")) { ok = 1; ended = 1; }
			else if (!strcmp(line, "Failed!")) { snprintf(outcome, sizeof(outcome), "Paired, but it didn't connect: try turning it off and on"); ended = 1; }
			else if (!strcmp(line, "Pair error!") || !strcmp(line, "Timed out.")) { snprintf(outcome, sizeof(outcome), "Pairing didn't work: put it in pairing mode again and retry"); ended = 1; }
			continue;
		}
		if (r == 0) { if (!outcome[0]) snprintf(outcome, sizeof(outcome), "Pairing stopped"); break; }
		pthread_mutex_lock(&lk);
		int cancel = cancel_req;
		pthread_mutex_unlock(&lk);
		if (cancel) { snprintf(outcome, sizeof(outcome), "Cancelled"); break; }
		if (time(NULL) > end) { snprintf(outcome, sizeof(outcome), "No controller found in pairing mode"); break; }
		usleep(50000);
	}
	/* btctl stops discovery on Ctrl+C; then make sure it's gone */
	kill(-pid, SIGINT);
	for (int i = 0; i < 30 && waitpid(pid, NULL, WNOHANG) == 0; i++) usleep(100000);
	if (waitpid(pid, NULL, WNOHANG) == 0) { kill(-pid, SIGKILL); waitpid(pid, NULL, 0); }
	close(fd);
	pthread_mutex_lock(&lk);
	pair_pid = 0;
	st.busy = 0;
	st.ok = ok;
	st.at = time(NULL);
	if (ok) snprintf(st.message, sizeof(st.message), "Paired %s", name[0] ? name : mac);
	else snprintf(st.message, sizeof(st.message), "%s", outcome);
	pthread_mutex_unlock(&lk);
	plog("bluetooth: %s", st.message);
	return NULL;
}

int bt_pair_start(void)
{
	if (!adapter_present()) return -1;
	pthread_mutex_lock(&lk);
	if (st.busy) { pthread_mutex_unlock(&lk); return 1; }
	st.busy = 1;
	st.ok = 0;
	st.found[0] = 0;
	cancel_req = 0;
	snprintf(st.message, sizeof(st.message), "Looking...");
	pthread_mutex_unlock(&lk);
	pthread_t t;
	if (pthread_create(&t, NULL, pair_thread, NULL)) {
		pthread_mutex_lock(&lk);
		st.busy = 0;
		pthread_mutex_unlock(&lk);
		return -1;
	}
	pthread_detach(t);
	return 0;
}

void bt_pair_cancel(void)
{
	pthread_mutex_lock(&lk);
	if (st.busy) cancel_req = 1;
	pthread_mutex_unlock(&lk);
}

void bt_state(bt_state_t *s)
{
	pthread_mutex_lock(&lk);
	*s = st;
	pthread_mutex_unlock(&lk);
	s->present = adapter_present();
}

int bt_devices(bt_dev_t *out, int max)
{
	if (!adapter_present()) return 0;
	const char *a1[] = { bluetoothctl(), "paired-devices", NULL };
	char *list = capture(a1, 5);
	if (!list) return 0;
	int n = 0;
	char *save = NULL;
	for (char *line = strtok_r(list, "\n", &save); line && n < max; line = strtok_r(NULL, "\n", &save)) {
		/* "Device AA:BB:CC:DD:EE:FF Name with spaces" (colour codes possible in front) */
		char *d = strstr(line, "Device ");
		if (!d) continue;
		d += 7;
		char mac[18];
		snprintf(mac, sizeof(mac), "%.17s", d);
		if (!valid_mac(mac) || (d[17] && d[17] != ' ')) continue;
		bt_dev_t *x = &out[n];
		memset(x, 0, sizeof(*x));
		snprintf(x->mac, sizeof(x->mac), "%s", mac);
		snprintf(x->name, sizeof(x->name), "%s", d[17] == ' ' && d[18] ? d + 18 : mac);
		const char *a2[] = { bluetoothctl(), "info", x->mac, NULL };
		char *info = capture(a2, 5);
		if (info) {
			char *s2 = NULL;
			for (char *l = strtok_r(info, "\n", &s2); l; l = strtok_r(NULL, "\n", &s2)) {
				while (*l == ' ' || *l == '\t') l++;
				if (!strncmp(l, "Connected: ", 11)) x->connected = !strncmp(l + 11, "yes", 3);
				else if (!strncmp(l, "Icon: ", 6)) snprintf(x->icon, sizeof(x->icon), "%s", l + 6);
				else if (!strncmp(l, "Alias: ", 7) && l[7]) snprintf(x->name, sizeof(x->name), "%s", l + 7);
			}
			free(info);
		}
		x->input = !strncmp(x->icon, "input-", 6);
		n++;
	}
	free(list);
	return n;
}

static int btctl_mac(const char *cmd, const char *mac, char *err, size_t n)
{
	if (!valid_mac(mac)) { snprintf(err, n, "That isn't a Bluetooth address"); return -1; }
	if (!adapter_present()) { snprintf(err, n, "No Bluetooth adapter"); return -1; }
	pthread_mutex_lock(&lk);
	int busy = st.busy;
	pthread_mutex_unlock(&lk);
	if (busy) { snprintf(err, n, "Pairing is running; wait for it or cancel it"); return -1; }
	char up[18];
	for (int i = 0; i < 18; i++) up[i] = (char)toupper((unsigned char)mac[i]);
	const char *argv[] = { btctl(), cmd, up, NULL };
	char *out = capture(argv, 15);
	if (!out) { snprintf(err, n, "MiSTer's Bluetooth tool (btctl) isn't there"); return -1; }
	plog("bluetooth %s %s: %s", cmd, up, out);
	free(out);
	return 0;
}

int bt_forget(const char *mac, char *err, size_t n) { return btctl_mac("remove", mac, err, n); }
int bt_disconnect(const char *mac, char *err, size_t n) { return btctl_mac("disconnect", mac, err, n); }
