/* phosphord: where things live.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "paths.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char *env_or(const char *name, const char *def)
{
	const char *v = getenv(name);
	return v && *v ? v : def;
}

const char *p_fat(void) { return env_or("PHOS_FAT", "/media/fat"); }
const char *p_tmp(void) { return env_or("PHOS_TMP", "/tmp/phosphor"); }
const char *p_mister_cmd(void) { return env_or("PHOS_MISTER_CMD", "/dev/MiSTer_cmd"); }
const char *p_corename(void) { return env_or("PHOS_CORENAME", "/tmp/CORENAME"); }

/* built once (the env is read at the first call): many threads ask, and rewriting a shared buffer
   on every call let one of them see it half written */
static char data_b[512], config_b[512], socket_b[512];
static pthread_once_t built = PTHREAD_ONCE_INIT;

static void build(void)
{
	snprintf(data_b, sizeof(data_b), "%s/phosphor/data", p_fat());
	snprintf(config_b, sizeof(config_b), "%s/phosphor/config", p_fat());
	snprintf(socket_b, sizeof(socket_b), "%s/phosphord.sock", p_tmp());
}

const char *p_data(void) { pthread_once(&built, build); return data_b; }
const char *p_config(void) { pthread_once(&built, build); return config_b; }
const char *p_socket(void) { pthread_once(&built, build); return socket_b; }

int p_repair_drive(int usb)
{
	/* the drive Repair all drives uses now: its copy of Linux carries the drive's ID (phosphor/rescue/os.id,
	   drive=<PARTUUID>), the same one Phosphor keeps in state/sdrescue.state. A drive that was the repair
	   drive once (an old copy left on the NVMe drive) isn't */
	char p[96], want[64] = "", have[64] = "", l[128];
	FILE *f = fopen("/media/fat/phosphor/state/sdrescue.state", "r");
	if (!f) return 0;
	while (fgets(l, sizeof(l), f)) if (!strncmp(l, "partuuid=", 9)) { snprintf(want, sizeof(want), "%s", l + 9); want[strcspn(want, "\r\n")] = 0; }
	fclose(f);
	if (!want[0]) return 0;
	snprintf(p, sizeof(p), "/media/usb%d/phosphor/rescue/os.id", usb);
	if (!(f = fopen(p, "r"))) return 0;
	while (fgets(l, sizeof(l), f)) if (!strncmp(l, "drive=", 6)) { snprintf(have, sizeof(have), "%s", l + 6); have[strcspn(have, "\r\n")] = 0; }
	fclose(f);
	return !strcasecmp(want, have);
}

int p_root_is_repair(const char *root)
{
	int u;
	return sscanf(root, "/media/usb%d/", &u) == 1 && p_repair_drive(u);
}

int p_roots(char out[][256], int max)
{
	int n = 0;
	const char *env = getenv("PHOS_GAMES_ROOTS");
	if (env && *env) {
		char buf[2048], *save = NULL;
		snprintf(buf, sizeof(buf), "%s", env);
		for (char *t = strtok_r(buf, "|", &save); t && n < max; t = strtok_r(NULL, "|", &save))
			snprintf(out[n++], 256, "%s", t);
		return n;
	}
	/* a drive set up for Repair all drives is never a games drive, as in Phosphor's Main (an empty games/PSX
	   on the repair stick, usb0 when it's plugged in first, doesn't hide the NVMe drive's games) */
	for (int i = 0; i < 8 && n < max; i++)
		if (!p_repair_drive(i)) snprintf(out[n++], 256, "/media/usb%d/games", i);
	if (n < max) snprintf(out[n++], 256, "%s/cifs/games", p_fat());
	if (n < max) snprintf(out[n++], 256, "%s/games", p_fat());
	return n;
}

int mkdirs(const char *dir)
{
	char d[1024];
	snprintf(d, sizeof(d), "%s", dir);
	for (char *p = d + 1; *p; p++) {
		if (*p == '/') {
			*p = 0;
			if (mkdir(d, 0755) != 0 && errno != EEXIST) return -1;
			*p = '/';
		}
	}
	if (mkdir(d, 0755) != 0 && errno != EEXIST) return -1;
	return 0;
}

int file_exists(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

int dir_exists(const char *p)
{
	struct stat st;
	return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}
