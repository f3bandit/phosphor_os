/* Phosphor shell: its own settings (frontend.ini on the SD card).
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
	SETTINGS.front_pads = 1;
	SETTINGS.saver_min = 10;
	SETTINGS.setup_done = f != NULL;     /* no settings yet: a new install, show the welcome */
	if (!f) return;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = 0;
		const char *k = line, *v = eq + 1;
		if (!strcmp(k, "accent")) SETTINGS.accent = atoi(v);
		else if (!strcmp(k, "clock24")) SETTINGS.clock24 = atoi(v);
		else if (!strcmp(k, "texture")) SETTINGS.texture = atoi(v);
		else if (!strcmp(k, "setup_done")) SETTINGS.setup_done = atoi(v);
		else if (!strcmp(k, "front_pads")) SETTINGS.front_pads = atoi(v);
		else if (!strcmp(k, "saver_min")) SETTINGS.saver_min = atoi(v);
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
	fprintf(f, "accent=%d\nclock24=%d\ntexture=%d\nsetup_done=%d\nfront_pads=%d\nsaver_min=%d\n", SETTINGS.accent, SETTINGS.clock24,
		SETTINGS.texture, SETTINGS.setup_done, SETTINGS.front_pads, SETTINGS.saver_min);
	fclose(f);
	rename(tmp, p);
}
