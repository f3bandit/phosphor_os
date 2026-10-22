/* phosphord: where things live.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "paths.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

const char *p_data(void)
{
	static char b[512];
	snprintf(b, sizeof(b), "%s/phosphor/data", p_fat());
	return b;
}

const char *p_config(void)
{
	static char b[512];
	snprintf(b, sizeof(b), "%s/phosphor/config", p_fat());
	return b;
}

const char *p_socket(void)
{
	static char b[512];
	snprintf(b, sizeof(b), "%s/phosphord.sock", p_tmp());
	return b;
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
	for (int i = 0; i < 8 && n < max; i++) snprintf(out[n++], 256, "/media/usb%d/games", i);
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
