/* phosphord: screen filters per system (video filter, scanlines, shadow mask, gamma, presets).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * MiSTer Main keeps each core's choices in config/<core>_scaler.cfg, _shmask.cfg and _gamma.cfg
 * (what the OSD's video processing menu saves) and reads them when the core starts. Phosphor writes
 * the same files the same way, so a change is what picking it in the OSD would have done; it shows
 * the next time that core starts. Layouts, from MiSTer Main's video.cpp:
 *   _scaler.cfg  4 x { char mode; char file[1023]; }  horizontal, vertical, scanlines, interlaced
 *   _shmask.cfg  char mode (0 off, 1 1x, 2 2x, 3 1x rotated, 4 2x rotated); char file[1023]
 *   _gamma.cfg   char on; char file[1023]
 * File names are relative to Filters/, Shadow_Masks/ and Gamma/. Presets/<name>.ini (hfilter=, vfilter=,
 * sfilter=, ifilter=, mask=, maskmode=, gamma=) are applied the way Main applies them.
 * Before every change the three files are backed up; undo puts the last backup back. */
#include "video.h"
#include "log.h"
#include "paths.h"
#include "systems.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SLOT 1024
enum { V_HORZ, V_VERT, V_SCAN, V_ILACE };
static const char *MASK_MODES[] = { "off", "1x", "2x", "1x rotated", "2x rotated" };

typedef struct {
	unsigned char scaler[4 * SLOT], shmask[SLOT], gamma[SLOT];
	int have_scaler, have_shmask, have_gamma;
} vcfg_t;

/* the core's name as MiSTer Main uses it for config files: from the installed core (NES_20260101.rbf
 * -> NES), or the [System] cfgname= in systems.ini */
int video_core_name(const char *system_id, char *out, size_t n)
{
	const gsys_t *s = system_by_id(system_id);
	if (!s || s->cat == CAT_ARCADE) return -1;
	if (s->cfgname[0]) { snprintf(out, n, "%s", s->cfgname); return 0; }
	char rbf[300];
	if (!system_core(s, rbf, sizeof(rbf))) return -1;
	const char *b = strrchr(rbf, '/');
	b = b ? b + 1 : rbf;
	snprintf(out, n, "%s", b);
	char *u = strrchr(out, '_');                     /* the date */
	if (u && strlen(u) >= 9 && isdigit((unsigned char)u[1])) *u = 0;
	char *dot = strrchr(out, '.');
	if (dot && !strcasecmp(dot, ".rbf")) *dot = 0;
	return out[0] ? 0 : -1;
}

static void cfg_file(const char *core, const char *what, char *out, size_t n)
{
	snprintf(out, n, "%s/config/%s_%s.cfg", p_fat(), core, what);
}

static int read_into(const char *path, unsigned char *buf, size_t n)
{
	memset(buf, 0, n);
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	size_t got = fread(buf, 1, n, f);
	fclose(f);
	return got > 0;
}

static void load(const char *core, vcfg_t *v)
{
	char p[700];
	cfg_file(core, "scaler", p, sizeof(p)); v->have_scaler = read_into(p, v->scaler, sizeof(v->scaler));
	cfg_file(core, "shmask", p, sizeof(p)); v->have_shmask = read_into(p, v->shmask, sizeof(v->shmask));
	cfg_file(core, "gamma", p, sizeof(p)); v->have_gamma = read_into(p, v->gamma, sizeof(v->gamma));
	/* Main resets out-of-range values the same way */
	if (v->scaler[0] > 1) memset(v->scaler, 0, sizeof(v->scaler));
	if (v->shmask[0] > 4) v->shmask[0] = 0;
	if (v->gamma[0] > 1) memset(v->gamma, 0, sizeof(v->gamma));
}

static int write_file(const char *path, const unsigned char *buf, size_t n)
{
	char tmp[760];
	snprintf(tmp, sizeof(tmp), "%s.phosphor-new", path);
	FILE *f = fopen(tmp, "wb");
	if (!f) return -1;
	if (fwrite(buf, 1, n, f) != n || fflush(f) || fsync(fileno(f)) || fclose(f)) { unlink(tmp); return -1; }
	if (rename(tmp, path)) { unlink(tmp); return -1; }
	return 0;
}

/* backup: data/backup/video/<time>-<core>/ with whichever of the three files existed */
static int backup(const char *core)
{
	char dir[700], src[700], dst[800];
	time_t t = time(NULL);
	struct tm tm;
	localtime_r(&t, &tm);
	static int seq;
	snprintf(dir, sizeof(dir), "%s/backup/video/%04d%02d%02d-%02d%02d%02d-%02d-%s", p_data(), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
	         tm.tm_hour, tm.tm_min, tm.tm_sec, seq++ % 100, core);
	if (mkdirs(dir)) return -1;
	static const char *W[] = { "scaler", "shmask", "gamma" };
	for (int i = 0; i < 3; i++) {
		cfg_file(core, W[i], src, sizeof(src));
		unsigned char buf[4 * SLOT];
		FILE *f = fopen(src, "rb");
		if (!f) continue;                              /* didn't exist: undo removes it again */
		size_t n = fread(buf, 1, sizeof(buf), f);
		fclose(f);
		snprintf(dst, sizeof(dst), "%s/%s", dir, W[i]);
		if (write_file(dst, buf, n)) return -1;
	}
	return 0;
}

static int save(const char *core, vcfg_t *v, int scaler, int shmask, int gamma)
{
	char p[700];
	snprintf(p, sizeof(p), "%s/config", p_fat());
	mkdirs(p);
	if (backup(core)) return -1;
	if (scaler) { cfg_file(core, "scaler", p, sizeof(p)); if (write_file(p, v->scaler, sizeof(v->scaler))) return -1; }
	if (shmask) { cfg_file(core, "shmask", p, sizeof(p)); if (write_file(p, v->shmask, sizeof(v->shmask))) return -1; }
	if (gamma) { cfg_file(core, "gamma", p, sizeof(p)); if (write_file(p, v->gamma, sizeof(v->gamma))) return -1; }
	sync();
	return 0;
}

static void set_name(unsigned char *slot, const char *name)
{
	memset(slot + 1, 0, SLOT - 1);
	snprintf((char *)slot + 1, SLOT - 1, "%s", name);
}

static const char *name_of(const unsigned char *slot) { return (const char *)slot + 1; }

void video_get(const char *core, video_state_t *s)
{
	vcfg_t v;
	load(core, &v);
	memset(s, 0, sizeof(*s));
	snprintf(s->core, sizeof(s->core), "%s", core);
	static const int ORDER[4] = { V_HORZ, V_VERT, V_SCAN, V_ILACE };
	for (int i = 0; i < 4; i++) {
		const unsigned char *slot = v.scaler + ORDER[i] * SLOT;
		s->flt_on[i] = slot[0] == 1;
		snprintf(s->flt[i], sizeof(s->flt[i]), "%s", name_of(slot));
	}
	s->mask_mode = v.shmask[0];
	snprintf(s->mask, sizeof(s->mask), "%s", name_of(v.shmask));
	s->gamma_on = v.gamma[0] == 1;
	snprintf(s->gamma, sizeof(s->gamma), "%s", name_of(v.gamma));
	s->custom = v.have_scaler || v.have_shmask || v.have_gamma;
}

/* one setting: what = filter|vertical|scanlines|interlace|mask|maskmode|gamma; value = file, or off */
int video_set(const char *core, const char *what, const char *value, char *err, size_t errn)
{
	vcfg_t v;
	load(core, &v);
	int sc = 0, sm = 0, ga = 0, off = !value || !*value || !strcasecmp(value, "off") || !strcasecmp(value, "none");
	int slot = !strcmp(what, "filter") ? V_HORZ : !strcmp(what, "vertical") ? V_VERT : !strcmp(what, "scanlines") ? V_SCAN :
	           !strcmp(what, "interlace") ? V_ILACE : -1;
	if (slot >= 0) {
		unsigned char *s = v.scaler + slot * SLOT;
		if (off) s[0] = 0;
		else { set_name(s, value); s[0] = 1; }
		sc = 1;
	} else if (!strcmp(what, "mask")) {
		if (off) v.shmask[0] = 0;
		else { set_name(v.shmask, value); if (!v.shmask[0]) v.shmask[0] = 1; }
		sm = 1;
	} else if (!strcmp(what, "maskmode")) {
		int m = -1;
		for (int i = 0; i < 5; i++) if (value && !strcasecmp(value, MASK_MODES[i])) m = i;
		if (off) m = 0;
		if (m < 0) { snprintf(err, errn, "mask mode is off, 1x, 2x, 1x rotated or 2x rotated"); return -1; }
		v.shmask[0] = (unsigned char)m;
		sm = 1;
	} else if (!strcmp(what, "gamma")) {
		if (off) v.gamma[0] = 0;
		else { set_name(v.gamma, value); v.gamma[0] = 1; }
		ga = 1;
	} else {
		snprintf(err, errn, "unknown setting %s", what);
		return -1;
	}
	if (save(core, &v, sc, sm, ga)) { snprintf(err, errn, "Couldn't save the settings"); return -1; }
	plog("video: %s %s = %s", core, what, off ? "off" : value);
	return 0;
}

static char *trim(char *s)
{
	while (isspace((unsigned char)*s)) s++;
	char *e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
	return s;
}

/* a Presets/ .ini, applied as MiSTer Main applies it (only the keys it has) */
int video_preset(const char *core, const char *preset, char *err, size_t errn)
{
	char path[900];
	snprintf(path, sizeof(path), "%s/Presets/%s%s", p_fat(), preset, strcasestr(preset, ".ini") ? "" : ".ini");
	if (strstr(preset, "..")) { snprintf(err, errn, "bad preset name"); return -1; }
	FILE *f = fopen(path, "r");
	if (!f) { snprintf(err, errn, "Preset not found"); return -1; }
	vcfg_t v;
	load(core, &v);
	int sc = 0, sm = 0, ga = 0;
	char line[1200];
	static const struct { const char *k; int slot; } FK[] = { { "hfilter", V_HORZ }, { "vfilter", V_VERT }, { "sfilter", V_SCAN }, { "ifilter", V_ILACE } };
	while (fgets(line, sizeof(line), f)) {
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = 0;
		char *k = trim(line), *a = trim(eq + 1);
		if (!*a) continue;
		int done = 0;
		for (int i = 0; i < 4; i++) {
			if (strcasecmp(k, FK[i].k)) continue;
			unsigned char *s = v.scaler + FK[i].slot * SLOT;
			if (!strcasecmp(a, "same") || !strcasecmp(a, "off")) s[0] = 0;
			else { set_name(s, a); s[0] = 1; }
			sc = done = 1;
		}
		if (done) continue;
		if (!strcasecmp(k, "mask")) {
			if (!strcasecmp(a, "off") || !strcasecmp(a, "none")) v.shmask[0] = 0;
			else set_name(v.shmask, a);
			sm = 1;
		} else if (!strcasecmp(k, "maskmode")) {
			for (int i = 0; i < 5; i++) if (!strcasecmp(a, MASK_MODES[i])) v.shmask[0] = (unsigned char)i;
			if (!strcasecmp(a, "none")) v.shmask[0] = 0;
			sm = 1;
		} else if (!strcasecmp(k, "gamma")) {
			if (!strcasecmp(a, "off") || !strcasecmp(a, "none")) v.gamma[0] = 0;
			else { set_name(v.gamma, a); v.gamma[0] = 1; }
			ga = 1;
		}
	}
	fclose(f);
	if (!sc && !sm && !ga) { snprintf(err, errn, "That preset doesn't set anything"); return -1; }
	if (save(core, &v, sc, sm, ga)) { snprintf(err, errn, "Couldn't save the settings"); return -1; }
	plog("video: %s preset %s", core, preset);
	return 0;
}

/* put the last backup for this core back (files that didn't exist then are removed) */
int video_undo(const char *core, char *err, size_t errn)
{
	char dir[700], best[300] = "";
	snprintf(dir, sizeof(dir), "%s/backup/video", p_data());
	DIR *d = opendir(dir);
	if (d) {
		struct dirent *e;
		size_t cl = strlen(core);
		while ((e = readdir(d))) {
			size_t l = strlen(e->d_name);
			if (l > cl + 1 && !strcmp(e->d_name + l - cl, core) && e->d_name[l - cl - 1] == '-' && strcmp(e->d_name, best) > 0)
				snprintf(best, sizeof(best), "%s", e->d_name);
		}
		closedir(d);
	}
	if (!best[0]) { snprintf(err, errn, "Nothing to undo"); return -1; }
	static const char *W[] = { "scaler", "shmask", "gamma" };
	for (int i = 0; i < 3; i++) {
		char b[1100], c[700];
		snprintf(b, sizeof(b), "%s/%s/%s", dir, best, W[i]);
		cfg_file(core, W[i], c, sizeof(c));
		unsigned char buf[4 * SLOT];
		FILE *f = fopen(b, "rb");
		if (!f) { unlink(c); continue; }
		size_t n = fread(buf, 1, sizeof(buf), f);
		fclose(f);
		if (write_file(c, buf, n)) { snprintf(err, errn, "Couldn't restore the settings"); return -1; }
		unlink(b);
	}
	char bd[1100];
	snprintf(bd, sizeof(bd), "%s/%s", dir, best);
	rmdir(bd);
	sync();
	plog("video: %s undone", core);
	return 0;
}

/* ---------- lists of files ---------- */

static void walk(const char *base, const char *rel, const char *ext, video_list_cb cb, void *u, int depth)
{
	char dir[1200];
	snprintf(dir, sizeof(dir), "%s%s%s", base, rel[0] ? "/" : "", rel);
	DIR *d = opendir(dir);
	if (!d) return;
	struct dirent *e;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		char r[1200], full[2400];
		snprintf(r, sizeof(r), "%s%s%s", rel, rel[0] ? "/" : "", e->d_name);
		snprintf(full, sizeof(full), "%s/%s", base, r);
		struct stat st;
		if (stat(full, &st)) continue;
		if (S_ISDIR(st.st_mode)) { if (depth < 4) walk(base, r, ext, cb, u, depth + 1); }
		else {
			const char *dot = strrchr(e->d_name, '.');
			if (dot && !strcasecmp(dot, ext)) cb(u, r);
		}
	}
	closedir(d);
}

/* kind: filters, masks, gamma, presets */
int video_list(const char *kind, video_list_cb cb, void *u)
{
	const char *dir = !strcmp(kind, "filters") ? "Filters" : !strcmp(kind, "masks") ? "Shadow_Masks" :
	                  !strcmp(kind, "gamma") ? "Gamma" : !strcmp(kind, "presets") ? "Presets" : NULL;
	if (!dir) return -1;
	char base[700];
	snprintf(base, sizeof(base), "%s/%s", p_fat(), dir);
	walk(base, "", !strcmp(kind, "presets") ? ".ini" : ".txt", cb, u, 0);
	return 0;
}
