/* Phosphor frontend: settings and the last game played (frontend.ini on the SD card).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
#include "settings.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

settings_t SETTINGS;

static void path(char *out, size_t n)
{
	const char *f = getenv("PHOS_FAT");
	snprintf(out, n, "%s/phosphor/config/frontend.ini", f && *f ? f : "/media/fat");
}

void settings_load(void)
{
	char p[512], line[1400];
	path(p, sizeof(p));
	FILE *f = fopen(p, "r");
	if (!f) return;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = 0;
		const char *k = line, *v = eq + 1;
		if (!strcmp(k, "accent")) SETTINGS.accent = atoi(v);
		else if (!strcmp(k, "clock24")) SETTINGS.clock24 = atoi(v);
		else if (!strcmp(k, "last_system")) snprintf(SETTINGS.last_system, sizeof(SETTINGS.last_system), "%s", v);
		else if (!strcmp(k, "last_system_name")) snprintf(SETTINGS.last_system_name, sizeof(SETTINGS.last_system_name), "%s", v);
		else if (!strcmp(k, "last_path")) snprintf(SETTINGS.last_path, sizeof(SETTINGS.last_path), "%s", v);
		else if (!strcmp(k, "last_title")) snprintf(SETTINGS.last_title, sizeof(SETTINGS.last_title), "%s", v);
	}
	fclose(f);
}

static void mkdirs(const char *file)
{
	char d[512];
	snprintf(d, sizeof(d), "%s", file);
	for (char *p = d + 1; *p; p++) {
		if (*p == '/') { *p = 0; mkdir(d, 0755); *p = '/'; }
	}
}

void settings_save(void)
{
	char p[512], tmp[520];
	path(p, sizeof(p));
	mkdirs(p);
	snprintf(tmp, sizeof(tmp), "%s.tmp", p);
	FILE *f = fopen(tmp, "w");
	if (!f) { plog("can't write %s", p); return; }
	fprintf(f, "accent=%d\nclock24=%d\nlast_system=%s\nlast_system_name=%s\nlast_path=%s\nlast_title=%s\n",
	        SETTINGS.accent, SETTINGS.clock24, SETTINGS.last_system, SETTINGS.last_system_name,
	        SETTINGS.last_path, SETTINGS.last_title);
	fclose(f);
	rename(tmp, p);
}

void settings_set_last(const game_system_t *s, const game_t *g)
{
	snprintf(SETTINGS.last_system, sizeof(SETTINGS.last_system), "%s", s->id);
	snprintf(SETTINGS.last_system_name, sizeof(SETTINGS.last_system_name), "%s", s->name);
	snprintf(SETTINGS.last_path, sizeof(SETTINGS.last_path), "%s", g->path);
	snprintf(SETTINGS.last_title, sizeof(SETTINGS.last_title), "%s", g->title);
	settings_save();
}
