/* phosphord: the systems Phosphor knows, and how each one is started.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * The MGL slot numbers (type/index/delay) follow each core's file menu. They still need checking
 * on hardware, so they can be changed in config/systems.ini without rebuilding. */
#include "systems.h"
#include "log.h"
#include "paths.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

gsys_t *SYS;
int NSYS;

static const char *CAT_NAMES[CAT_COUNT] = { "Disc", "Cartridge", "Handheld", "Arcade", "Computer" };
const char *category_name(category_t c) { return c < CAT_COUNT ? CAT_NAMES[c] : "?"; }

static const gsys_t BUILTIN[] = {
	/* id          name                    category       folders                rbf                                   exts               t  i  d */
	{ "PSX",       "PlayStation",          CAT_DISC,      "PSX",                 "_Console/PSX",                       "cue|chd|exe",     's', 1, 1, "" },
	{ "Saturn",    "Saturn",               CAT_DISC,      "Saturn",              "_Console/Saturn",                    "cue|chd",         's', 0, 1, "" },
	{ "MegaCD",    "Mega-CD / Sega CD",    CAT_DISC,      "MegaCD",              "_Console/MegaCD",                    "cue|chd",         's', 0, 1, "" },
	{ "TGFX16CD",  "PC Engine CD",         CAT_DISC,      "TGFX16-CD",           "_Console/TurboGrafx16",              "cue|chd",         's', 0, 1, "" },
	{ "NES",       "NES / Famicom",        CAT_CARTRIDGE, "NES",                 "_Console/NES",                       "nes|fds|nsf",     'f', 1, 1, "" },
	{ "SNES",      "Super NES",            CAT_CARTRIDGE, "SNES",                "_Console/SNES",                      "sfc|smc",         'f', 0, 1, "" },
	{ "Genesis",   "Mega Drive / Genesis", CAT_CARTRIDGE, "MegaDrive|Genesis",   "_Console/MegaDrive|_Console/Genesis","md|gen|bin|smd",  'f', 1, 1, "" },
	{ "N64",       "Nintendo 64",          CAT_CARTRIDGE, "N64",                 "_Console/N64",                       "n64|z64|v64",     'f', 1, 1, "" },
	{ "SMS",       "Master System",        CAT_CARTRIDGE, "SMS",                 "_Console/SMS",                       "sms|sg",          'f', 1, 1, "" },
	{ "TGFX16",    "PC Engine",            CAT_CARTRIDGE, "TGFX16",              "_Console/TurboGrafx16",              "pce|sgx",         'f', 1, 1, "" },
	{ "ATARI7800", "Atari 7800 / 2600",    CAT_CARTRIDGE, "ATARI7800|ATARI2600", "_Console/Atari7800",                 "a78|a26|bin",     'f', 1, 1, "" },
	{ "GAMEBOY",   "Game Boy / Color",     CAT_HANDHELD,  "GAMEBOY|GBC",         "_Console/Gameboy",                   "gb|gbc",          'f', 1, 1, "" },
	{ "GBA",       "Game Boy Advance",     CAT_HANDHELD,  "GBA",                 "_Console/GBA",                       "gba",             'f', 1, 1, "" },
	{ "GameGear",  "Game Gear",            CAT_HANDHELD,  "GameGear",            "_Console/SMS",                       "gg",              'f', 2, 1, "" },
	{ "WonderSwan","WonderSwan",           CAT_HANDHELD,  "WonderSwan",          "_Console/WonderSwan",                "ws|wsc",          'f', 1, 1, "" },
	{ "AtariLynx", "Atari Lynx",           CAT_HANDHELD,  "AtariLynx",           "_Console/AtariLynx",                 "lnx",             'f', 1, 1, "" },
	{ "Arcade",    "Arcade",               CAT_ARCADE,    "",                    "",                                   "mra",             'f', 0, 0, "" },
	{ "C64",       "Commodore 64",         CAT_COMPUTER,  "C64",                 "_Computer/C64",                      "prg|crt|d64|t64", 'f', 1, 1, "" },
	{ "MSX",       "MSX",                  CAT_COMPUTER,  "MSX",                 "_Computer/MSX",                      "rom|dsk",         'f', 1, 1, "" },
	{ "Atari800",  "Atari 800",            CAT_COMPUTER,  "ATARI800",            "_Computer/Atari800",                 "atr|xex|car|rom", 'f', 1, 1, "" },
};

const gsys_t *system_by_id(const char *id)
{
	for (int i = 0; i < NSYS; i++)
		if (!strcasecmp(SYS[i].id, id)) return &SYS[i];
	return NULL;
}

static char *trim(char *s)
{
	while (isspace((unsigned char)*s)) s++;
	char *e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
	if (*s == '"' && e > s + 1 && e[-1] == '"') { e[-1] = 0; s++; }
	return s;
}

static void load_ini(void)
{
	char path[600], line[512];
	snprintf(path, sizeof(path), "%s/systems.ini", p_config());
	FILE *f = fopen(path, "r");
	if (!f) return;
	gsys_t *cur = NULL;
	while (fgets(line, sizeof(line), f)) {
		char *s = trim(line);
		if (!*s || *s == ';' || *s == '#') continue;
		if (*s == '[') {
			char *e = strchr(s, ']');
			if (!e) continue;
			*e = 0;
			cur = (gsys_t *)system_by_id(s + 1);
			if (!cur) {
				SYS = realloc(SYS, sizeof(*SYS) * (NSYS + 1));
				cur = &SYS[NSYS++];
				memset(cur, 0, sizeof(*cur));
				snprintf(cur->id, sizeof(cur->id), "%s", s + 1);
				snprintf(cur->name, sizeof(cur->name), "%s", s + 1);
				cur->cat = CAT_COMPUTER; cur->type = 'f'; cur->index = 1; cur->delay = 1;
			}
			continue;
		}
		char *eq = strchr(s, '=');
		if (!cur || !eq) continue;
		*eq = 0;
		char *k = trim(s), *v = trim(eq + 1);
		if (!strcasecmp(k, "name")) snprintf(cur->name, sizeof(cur->name), "%s", v);
		else if (!strcasecmp(k, "folders")) snprintf(cur->folders, sizeof(cur->folders), "%s", v);
		else if (!strcasecmp(k, "rbf")) snprintf(cur->rbf, sizeof(cur->rbf), "%s", v);
		else if (!strcasecmp(k, "extensions")) snprintf(cur->exts, sizeof(cur->exts), "%s", v);
		else if (!strcasecmp(k, "type")) cur->type = (char)tolower((unsigned char)v[0]);
		else if (!strcasecmp(k, "index")) cur->index = atoi(v);
		else if (!strcasecmp(k, "delay")) cur->delay = atoi(v);
		else if (!strcasecmp(k, "cfgname")) snprintf(cur->cfgname, sizeof(cur->cfgname), "%s", v);
		else if (!strcasecmp(k, "category")) {
			for (int c = 0; c < CAT_COUNT; c++)
				if (!strcasecmp(v, CAT_NAMES[c])) cur->cat = (category_t)c;
		}
	}
	fclose(f);
	plog("loaded %s", path);
}

void systems_load(void)
{
	NSYS = (int)(sizeof(BUILTIN) / sizeof(BUILTIN[0]));
	SYS = malloc(sizeof(BUILTIN));
	memcpy(SYS, BUILTIN, sizeof(BUILTIN));
	load_ini();
}

int has_ext(const char *name, const char *exts)
{
	const char *dot = strrchr(name, '.');
	if (!dot || !dot[1]) return 0;
	size_t el = strlen(dot + 1);
	for (const char *p = exts; *p;) {
		const char *bar = strchr(p, '|');
		size_t l = bar ? (size_t)(bar - p) : strlen(p);
		if (l == el && !strncasecmp(p, dot + 1, l)) return 1;
		if (!bar) break;
		p = bar + 1;
	}
	return 0;
}

int system_dirs(const gsys_t *s, char out[][512], int max)
{
	int n = 0;
	if (!strcmp(s->id, "Arcade")) {
		snprintf(out[n], 512, "%s/_Arcade", p_fat());
		if (dir_exists(out[n])) n++;
		return n;
	}
	char roots[12][256];
	int nr = p_roots(roots, 12);
	for (int i = 0; i < nr && n < max; i++) {
		char folders[96], *save = NULL;
		snprintf(folders, sizeof(folders), "%s", s->folders);
		for (char *f = strtok_r(folders, "|", &save); f && n < max; f = strtok_r(NULL, "|", &save)) {
			snprintf(out[n], 512, "%s/%s", roots[i], f);
			if (dir_exists(out[n])) n++;
		}
	}
	return n;
}

int system_core(const gsys_t *s, char *rbf, size_t rn)
{
	if (!strcmp(s->id, "Arcade")) { if (rn) rbf[0] = 0; return 1; }
	char rbfs[96], *save = NULL;
	snprintf(rbfs, sizeof(rbfs), "%s", s->rbf);
	for (char *r = strtok_r(rbfs, "|", &save); r; r = strtok_r(NULL, "|", &save)) {
		char dir[600], base[128];
		const char *slash = strrchr(r, '/');
		snprintf(dir, sizeof(dir), "%s/%.*s", p_fat(), slash ? (int)(slash - r) : 0, r);
		snprintf(base, sizeof(base), "%s", slash ? slash + 1 : r);
		DIR *d = opendir(dir);
		if (!d) continue;
		struct dirent *e;
		size_t bl = strlen(base);
		int found = 0;
		while ((e = readdir(d)) && !found)
			if (!strncasecmp(e->d_name, base, bl) && (e->d_name[bl] == '_' || e->d_name[bl] == '.') && has_ext(e->d_name, "rbf"))
				found = 1;
		closedir(d);
		if (found) { snprintf(rbf, rn, "%s", r); return 1; }
	}
	return 0;
}
