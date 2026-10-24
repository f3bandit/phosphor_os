/* phosphord: box art and details for identified games, from the sources the user picked.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Sources are asked in the user's order (config/scraper.ini, Settings > Box art sources). For each
 * field (description, developer, publisher, genre, year, players, region, box art) the first answer
 * wins; fields a game already has (imported, set by hand) are never replaced. The title stays the
 * file name. libretro also gives a game its official name, which the name-based sources look up.
 * libretro matches by content first (CRC, product code); when that fails, by the file name if it is
 * an exact list name or has exactly the same title words (same disc, the file's region first). A
 * game nobody recognises is left for "Match to base games". Box art falls through: a picture that
 * isn't there (404) makes the next source's art be tried; Disc 2+ borrows Disc 1's art.
 *
 * Skipped: games based on another (they use its art), unidentified games, games already looked
 * up. Runs only while the menu is up, one game at a time. Network trouble pauses it for a few
 * minutes without marking anything. */
#include "busy.h"
#include "cJSON.h"
#include "scraper.h"
#include "art.h"
#include "db.h"
#include "log.h"
#include "net.h"
#include "paths.h"
#include "scrape_src.h"
#include "systems.h"
#include <ctype.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

static const source_t *const ALL[] = { &SRC_LIBRETRO, &SRC_ARCADEDB, &SRC_LAUNCHBOX, &SRC_TGDB, &SRC_SCREENSCRAPER, &SRC_IGDB };
#define NSRC (int)(sizeof(ALL) / sizeof(ALL[0]))
static const char *DEFAULT_ORDER = "libretro, arcadedb, launchbox, thegamesdb, screenscraper, igdb";

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int kicked, stop_now, running;   /* runs only when asked (Get box art), never by itself */
static char status_text[160] = "Not run yet";
static scrape_progress_t pg;            /* this run (under lk) */
/* the Phosphor app looking things up on the PC (much faster): the SS1's own run waits meanwhile. A
   lease, renewed by each request from the app; gone a minute after the app stops asking */
static time_t pc_until;
static char pc_by[64];
static int pc_active(void) { return time(NULL) < pc_until; }

void scraper_progress(scrape_progress_t *o)
{
	pthread_mutex_lock(&lk);
	*o = pg;
	o->running = running || pc_active();
	snprintf(o->status, sizeof(o->status), "%s", status_text);
	pthread_mutex_unlock(&lk);
}

void scrape_status(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	pthread_mutex_lock(&lk);
	vsnprintf(status_text, sizeof(status_text), fmt, ap);
	pthread_mutex_unlock(&lk);
	va_end(ap);
}

/* ---------- results ---------- */

void sres_set(sres_t *r, int f, const char *v)
{
	if (!v || r->f[f]) return;
	while (isspace((unsigned char)*v)) v++;
	size_t n = strlen(v);
	while (n && isspace((unsigned char)v[n - 1])) n--;
	if (!n) return;
	char *t = strndup(v, n), *w = t;
	for (char *p = t; *p; p++) if (*p != '\r') *w++ = *p;   /* Windows line ends */
	*w = 0;
	r->f[f] = t;
}

void sres_art(sres_t *r, const char *src, const char *url)
{
	if (!url || !*url || r->nart >= MAX_ART) return;
	r->art[r->nart] = strdup(url);
	snprintf(r->art_src[r->nart], sizeof(r->art_src[0]), "%s", src);
	r->nart++;
}

void sres_free(sres_t *r)
{
	for (int i = 0; i < F_COUNT; i++) free(r->f[i]);
	for (int i = 0; i < r->nart; i++) free(r->art[i]);
	memset(r, 0, sizeof(*r));
}

int sres_complete(const sres_t *r, int has_art)
{
	for (int i = 0; i < F_COUNT; i++) if (!r->f[i] && i != F_REGION) return 0;
	return has_art || r->nart > 0;
}

void name_base(const char *official, char *out, size_t n)
{
	snprintf(out, n, "%s", official);
	char *p = strstr(out, " (");
	if (p) *p = 0;
	p = strstr(out, " [");
	if (p) *p = 0;
	/* "Legend of Zelda, The - A Link to the Past" -> "The Legend of Zelda - A Link to the Past" */
	char *the = strstr(out, ", The");
	if (the && (the[5] == 0 || the[5] == ' ')) {
		char tmp[512];
		snprintf(tmp, sizeof(tmp), "The %.*s%s", (int)(the - out), out, the + 5);
		snprintf(out, n, "%s", tmp);
	}
}

void xml_unescape(char *s)
{
	char *w = s;
	for (char *p = s; *p;) {
		if (*p == '&') {
			if (!strncmp(p, "&amp;", 5)) { *w++ = '&'; p += 5; continue; }
			if (!strncmp(p, "&lt;", 4)) { *w++ = '<'; p += 4; continue; }
			if (!strncmp(p, "&gt;", 4)) { *w++ = '>'; p += 4; continue; }
			if (!strncmp(p, "&quot;", 6)) { *w++ = '"'; p += 6; continue; }
			if (!strncmp(p, "&apos;", 6)) { *w++ = '\''; p += 6; continue; }
			if (p[1] == '#') {
				unsigned long c = p[2] == 'x' ? strtoul(p + 3, NULL, 16) : strtoul(p + 2, NULL, 10);
				char *semi = strchr(p, ';');
				if (semi && c) {
					if (c < 0x80) *w++ = (char)c;
					else if (c < 0x800) { *w++ = (char)(0xC0 | c >> 6); *w++ = (char)(0x80 | (c & 63)); }
					else { *w++ = (char)(0xE0 | c >> 12); *w++ = (char)(0x80 | (c >> 6 & 63)); *w++ = (char)(0x80 | (c & 63)); }
					p = semi + 1;
					continue;
				}
			}
		}
		*w++ = *p++;
	}
	*w = 0;
}

/* ---------- settings: config/scraper.ini ----------
 *   [sources]  order = libretro, arcadedb, ...   off = igdb, ...
 *   [screenscraper] user, password, devid, devpassword   [thegamesdb] apikey   [igdb] client_id, client_secret */

#define CFG_MAX 64
static struct { char sec[32], key[48], val[256]; } cfg[CFG_MAX];
static int ncfg;
static pthread_mutex_t cfg_lk = PTHREAD_MUTEX_INITIALIZER;

static void cfg_path(char *out, size_t n) { snprintf(out, n, "%s/scraper.ini", p_config()); }

static int cfg_has(const char *sec, const char *key)
{
	for (int i = 0; i < ncfg; i++) if (!strcasecmp(cfg[i].sec, sec) && !strcasecmp(cfg[i].key, key) && cfg[i].val[0]) return 1;
	return 0;
}

static void cfg_set(const char *sec, const char *key, const char *val)
{
	if (ncfg >= CFG_MAX || !*val || cfg_has(sec, key)) return;
	snprintf(cfg[ncfg].sec, sizeof(cfg[0].sec), "%s", sec);
	snprintf(cfg[ncfg].key, sizeof(cfg[0].key), "%s", key);
	snprintf(cfg[ncfg].val, sizeof(cfg[0].val), "%s", val);
	ncfg++;
}

/* Console Mode keeps the user's own TheGamesDB key and ScreenScraper login in its folder: used when
 * scraper.ini doesn't have them (read where they are; never copied, never sent anywhere but to those
 * services). Its developer login for ScreenScraper is Console Mode's own and isn't used. */
static void cfg_from_console_mode(void)
{
	char path[600], line[300];
	snprintf(path, sizeof(path), "%s/ConsoleMode/tgdb_apikey.txt", p_fat());
	FILE *f = fopen(path, "r");
	if (f) {
		if (fgets(line, sizeof(line), f)) {
			line[strcspn(line, "\r\n \t")] = 0;
			if (strlen(line) >= 16) cfg_set("thegamesdb", "apikey", line);
		}
		fclose(f);
	}
	snprintf(path, sizeof(path), "%s/ConsoleMode/screenscraper.txt", p_fat());
	f = fopen(path, "r");
	if (f) {
		while (fgets(line, sizeof(line), f)) {
			line[strcspn(line, "\r\n")] = 0;
			char *eq = strchr(line, '=');
			if (!eq) continue;
			*eq = 0;
			const char *k = line, *v = eq + 1;
			while (*v == ' ') v++;
			if (!strcasecmp(k, "ssid")) cfg_set("screenscraper", "user", v);
			else if (!strcasecmp(k, "sspassword")) cfg_set("screenscraper", "password", v);
		}
		fclose(f);
	}
}

static void cfg_load(void)
{
	pthread_mutex_lock(&cfg_lk);
	ncfg = 0;
	char path[600], line[512], sec[32] = "";
	cfg_path(path, sizeof(path));
	FILE *f = fopen(path, "r");
	if (f) {
		while (fgets(line, sizeof(line), f) && ncfg < CFG_MAX) {
			char *s = line;
			while (isspace((unsigned char)*s)) s++;
			s[strcspn(s, "\r\n")] = 0;
			if (!*s || *s == ';' || *s == '#') continue;
			if (*s == '[') { char *e = strchr(s, ']'); if (e) { *e = 0; snprintf(sec, sizeof(sec), "%s", s + 1); } continue; }
			char *eq = strchr(s, '=');
			if (!eq) continue;
			*eq = 0;
			char *k = s, *v = eq + 1;
			while (*k && isspace((unsigned char)k[strlen(k) - 1])) k[strlen(k) - 1] = 0;
			while (isspace((unsigned char)*v)) v++;
			while (*v && isspace((unsigned char)v[strlen(v) - 1])) v[strlen(v) - 1] = 0;
			snprintf(cfg[ncfg].sec, sizeof(cfg[0].sec), "%s", sec);
			snprintf(cfg[ncfg].key, sizeof(cfg[0].key), "%.47s", k);
			snprintf(cfg[ncfg].val, sizeof(cfg[0].val), "%s", v);
			ncfg++;
		}
		fclose(f);
	}
	cfg_from_console_mode();
	pthread_mutex_unlock(&cfg_lk);
}

const char *scfg(const char *section, const char *key)
{
	for (int i = 0; i < ncfg; i++)
		if (!strcasecmp(cfg[i].sec, section) && !strcasecmp(cfg[i].key, key)) return cfg[i].val;
	return "";
}

static int in_list(const char *list, const char *id)
{
	char buf[300], *save = NULL;
	snprintf(buf, sizeof(buf), "%s", list);
	for (char *t = strtok_r(buf, ", ", &save); t; t = strtok_r(NULL, ", ", &save))
		if (!strcasecmp(t, id)) return 1;
	return 0;
}

/* the sources in the user's order; ones missing from the order go last */
static int sources_in_order(const source_t **out)
{
	const char *order = scfg("sources", "order");
	if (!*order) order = DEFAULT_ORDER;
	int n = 0;
	char buf[300], *save = NULL;
	snprintf(buf, sizeof(buf), "%s", order);
	for (char *t = strtok_r(buf, ", ", &save); t; t = strtok_r(NULL, ", ", &save))
		for (int i = 0; i < NSRC; i++) {
			int dup = 0;
			for (int k = 0; k < n; k++) if (out[k] == ALL[i]) dup = 1;
			if (!dup && !strcasecmp(t, ALL[i]->id)) out[n++] = ALL[i];
		}
	for (int i = 0; i < NSRC; i++) {
		int have = 0;
		for (int k = 0; k < n; k++) if (out[k] == ALL[i]) have = 1;
		if (!have) out[n++] = ALL[i];
	}
	return n;
}

static int source_on(const source_t *s) { return !in_list(scfg("sources", "off"), s->id); }

/* rewrite [sources] in scraper.ini, keeping everything else (logins and keys) as it is */
static int cfg_save_sources(const char *order, const char *off)
{
	char path[600], tmp[620], line[512];
	cfg_path(path, sizeof(path));
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	mkdirs(p_config());
	FILE *in = fopen(path, "r"), *out = fopen(tmp, "w");
	if (!out) { if (in) fclose(in); return -1; }
	fprintf(out, "[sources]\n; asked in this order; for each field the first answer wins\norder = %s\noff = %s\n", order, off);
	int skip = 0, wrote_any = 0;
	if (in) {
		while (fgets(line, sizeof(line), in)) {
			const char *s = line;
			while (isspace((unsigned char)*s)) s++;
			if (*s == '[') skip = !strncasecmp(s, "[sources]", 9);
			if (!skip) { fputs(line, out); wrote_any = 1; }
		}
		fclose(in);
	}
	if (!wrote_any)
		fputs("\n; Sources that need an account or key (Phosphor works without them):\n"
		      "[screenscraper]\n; your screenscraper.fr login (enough on its own), and a developer login if you have one\nuser =\npassword =\ndevid =\ndevpassword =\n\n"
		      "[thegamesdb]\n; API key from thegamesdb.net (free)\napikey =\n\n"
		      "[igdb]\n; Twitch developer app (dev.twitch.tv): Client ID and Client Secret\nclient_id =\nclient_secret =\n", out);
	fclose(out);
	if (rename(tmp, path)) return -1;
	cfg_load();
	return 0;
}

/* the account keys Settings (and the PC app) may set: nothing else in scraper.ini */
static const char *const ACCT[][2] = {
	{ "screenscraper", "user" }, { "screenscraper", "password" }, { "screenscraper", "devid" }, { "screenscraper", "devpassword" },
	{ "thegamesdb", "apikey" }, { "igdb", "client_id" }, { "igdb", "client_secret" },
};
#define NACCT (int)(sizeof(ACCT) / sizeof(ACCT[0]))

int scraper_account_ok(const char *sec, const char *key)
{
	for (int i = 0; i < NACCT; i++) if (!strcmp(ACCT[i][0], sec) && !strcmp(ACCT[i][1], key)) return 1;
	return 0;
}

/* one key in its [section] of scraper.ini, written in place (comments and the rest kept); "" clears it */
int scraper_set_account(const char *sec, const char *key, const char *val)
{
	if (!scraper_account_ok(sec, key)) return -1;
	char v[256];
	snprintf(v, sizeof(v), "%s", val ? val : "");
	v[strcspn(v, "\r\n")] = 0;
	pthread_mutex_lock(&cfg_lk);
	char path[600], tmp[620], line[512];
	cfg_path(path, sizeof(path));
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	mkdirs(p_config());
	FILE *in = fopen(path, "r"), *out = fopen(tmp, "w");
	if (!out) { if (in) fclose(in); pthread_mutex_unlock(&cfg_lk); return -1; }
	int insec = 0, done = 0, seen_sec = 0;
	if (in) {
		while (fgets(line, sizeof(line), in)) {
			const char *t = line;
			while (isspace((unsigned char)*t)) t++;
			if (*t == '[') {
				if (insec && !done) { fprintf(out, "%s = %s\n", key, v); done = 1; }
				char nm[40];
				insec = sscanf(t, "[%39[^]]", nm) == 1 && !strcasecmp(nm, sec);
				if (insec) seen_sec = 1;
			} else if (insec && !done) {
				size_t kl = strlen(key);
				if (!strncasecmp(t, key, kl) && (isspace((unsigned char)t[kl]) || t[kl] == '=')) {
					fprintf(out, "%s = %s\n", key, v);
					done = 1;
					continue;
				}
			}
			fputs(line, out);
		}
		fclose(in);
	}
	if (!done) {
		if (!seen_sec) fprintf(out, "\n[%s]\n", sec);
		fprintf(out, "%s = %s\n", key, v);
	}
	fclose(out);
	int rc = rename(tmp, path) ? -1 : 0;
	pthread_mutex_unlock(&cfg_lk);
	cfg_load();
	return rc;
}

/* what's set: user names as they are, secrets only as set / not set */
void scraper_accounts(scrape_account_t *out, int *n)
{
	cfg_load();
	*n = 0;
	for (int i = 0; i < NACCT; i++) {
		scrape_account_t *a = &out[(*n)++];
		snprintf(a->sec, sizeof(a->sec), "%s", ACCT[i][0]);
		snprintf(a->key, sizeof(a->key), "%s", ACCT[i][1]);
		const char *v = scfg(ACCT[i][0], ACCT[i][1]);
		a->set = *v != 0;
		int secret = strcmp(ACCT[i][1], "user") && strcmp(ACCT[i][1], "devid") && strcmp(ACCT[i][1], "client_id");
		snprintf(a->value, sizeof(a->value), "%s", secret ? "" : v);
	}
}

/* ---------- one game ---------- */

static int menu_up(void)
{
	char core[64] = "";
	FILE *f = fopen(p_corename(), "r");
	if (!f) return 1;
	if (!fgets(core, sizeof(core), f)) core[0] = 0;
	fclose(f);
	core[strcspn(core, "\r\n")] = 0;
	return !core[0] || !strcmp(core, "MENU");
}

static void put_text(sqlite3 *db, long long gid, const char *col, const char *v)
{
	if (!v) return;
	char sql[160];
	snprintf(sql, sizeof(sql), "UPDATE games SET %s=? WHERE id=? AND %s IS NULL", col, col);
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return;
	sqlite3_bind_text(st, 1, v, -1, SQLITE_STATIC);
	sqlite3_bind_int64(st, 2, gid);
	sqlite3_step(st);
	sqlite3_finalize(st);
}

static int has_col(sqlite3 *db, long long gid, const char *col)
{
	char sql[120];
	snprintf(sql, sizeof(sql), "SELECT %s IS NOT NULL FROM games WHERE id=?", col);
	sqlite3_stmt *st;
	int v = 0;
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
	sqlite3_bind_int64(st, 1, gid);
	if (sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int(st, 0);
	sqlite3_finalize(st);
	return v;
}

/* Disc 2, 3...: the art of the same game's Disc 1 when it has some ("Final Fantasy VII (USA) (Disc 2)") */
static int disc1_art(sqlite3 *db, const sgame_t *g)
{
	const char *d = strstr(g->title, "(Disc ");
	if (!d || d[6] == '1' || !isdigit((unsigned char)d[6])) return 0;
	char want[520];
	snprintf(want, sizeof(want), "%.*s(Disc 1)%s", (int)(d - g->title), g->title, strchr(d, ')') ? strchr(d, ')') + 1 : "");
	sqlite3_stmt *st;
	int ok = 0;
	if (sqlite3_prepare_v2(db, "UPDATE games SET art_box=(SELECT o.art_box FROM games o WHERE o.system_id=?1 AND o.title=?2 "
	                           "AND o.art_box IS NOT NULL LIMIT 1) WHERE id=?3 AND EXISTS (SELECT 1 FROM games o WHERE "
	                           "o.system_id=?1 AND o.title=?2 AND o.art_box IS NOT NULL)", -1, &st, NULL) != SQLITE_OK)
		return 0;
	sqlite3_bind_text(st, 1, g->sys, -1, SQLITE_STATIC);
	sqlite3_bind_text(st, 2, want, -1, SQLITE_STATIC);
	sqlite3_bind_int64(st, 3, g->id);
	ok = sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(db) > 0;
	sqlite3_finalize(st);
	return ok;
}

/* the box art: the first URL that downloads and decodes. 1 stored, 0 none, -1 network trouble */
static int fetch_art(sqlite3 *db, sgame_t *g, sres_t *r, int from)
{
	char tmp[600], rel[600];
	int trouble = 0;
	snprintf(tmp, sizeof(tmp), "%s/scrape-art.img", p_tmp());
	for (int i = from; i < r->nart; i++) {
		int rc = net_get_file(r->art[i], tmp);
		if (rc < 0) { trouble = 1; continue; }
		if (rc) continue;
		int ok = art_store_box(g->sys, g->key, tmp, rel, sizeof(rel)) == 0;
		unlink(tmp);
		if (ok) { put_text(db, g->id, "art_box", rel); return 1; }
	}
	return trouble ? -1 : 0;
}

/* returns 1 found (with box art, or it already had some), 0 nobody has it (or no box art anywhere:
 * then it belongs in "Match to base games"), -1 network trouble: try again later */
static int scrape_one(sqlite3 *db, sgame_t *g)
{
	const source_t *src[NSRC];
	int n = sources_in_order(src), found = 0, trouble = 0;
	int had_art = has_col(db, g->id, "art_box"), art = had_art, tried = 0;
	sres_t r;
	memset(&r, 0, sizeof(r));
	/* already filled fields count as answered */
	static const char *COL[F_COUNT] = { "description", "developer", "publisher", "genre", "year", "players", "region" };
	for (int i = 0; i < F_COUNT; i++) if (has_col(db, g->id, COL[i])) r.f[i] = strdup("");
	for (int i = 0; i < n; i++) {
		if (!source_on(src[i]) || !src[i]->ready()) continue;
		if (sres_complete(&r, art)) {
			if (art) break;
			/* every field answered and an address for the art: try it now; if it doesn't download
			   (libretro has no picture under that name), the remaining sources are asked for art */
			int a = fetch_art(db, g, &r, tried);
			tried = r.nart;
			if (a > 0) { art = 1; break; }
			if (a < 0) trouble = 1;
		}
		pthread_mutex_lock(&lk);
		snprintf(pg.source, sizeof(pg.source), "%s", src[i]->name);
		pthread_mutex_unlock(&lk);
		int rc = src[i]->lookup(db, g, &r);
		if (rc == 1) {
			found = 1;
			if (r.found_by[0]) strncat(r.found_by, ", ", sizeof(r.found_by) - strlen(r.found_by) - 1);
			strncat(r.found_by, src[i]->name, sizeof(r.found_by) - strlen(r.found_by) - 1);
		} else if (rc == -1) {
			trouble = 1;
		}
	}
	if (found) {
		for (int i = 0; i < F_COUNT; i++) if (r.f[i] && *r.f[i]) put_text(db, g->id, COL[i], r.f[i]);
		if (g->official[0]) {
			sqlite3_stmt *st;
			if (sqlite3_prepare_v2(db, "UPDATE games SET ref_name=? WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
				sqlite3_bind_text(st, 1, g->official, -1, SQLITE_STATIC);
				sqlite3_bind_int64(st, 2, g->id);
				sqlite3_step(st);
				sqlite3_finalize(st);
			}
		}
		if (!art) {
			int a = fetch_art(db, g, &r, tried);
			if (a > 0) art = 1;
			else if (a < 0) trouble = 1;
		}
	}
	sres_free(&r);
	if (!art && disc1_art(db, g)) art = 1;
	if (art) return 1;
	return trouble ? -1 : 0;
}

/* ---------- the queue ---------- */

static const char *PENDING =
	"FROM games g WHERE g.scrape IS NULL AND g.based_on IS NULL AND g.key NOT LIKE 'n:%' "
	"AND EXISTS (SELECT 1 FROM files f WHERE f.game_id=g.id AND f.online=1)";

static void mark(sqlite3 *db, long long gid, const char *state)
{
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "UPDATE games SET scrape=? WHERE id=?", -1, &st, NULL) != SQLITE_OK) return;
	sqlite3_bind_text(st, 1, state, -1, SQLITE_STATIC);
	sqlite3_bind_int64(st, 2, gid);
	sqlite3_step(st);
	sqlite3_finalize(st);
}

static void *thread(void *arg)
{
	(void)arg;
	sqlite3 *db = db_open();
	if (!db) return NULL;
	char sql[1200];
	snprintf(sql, sizeof(sql),
	         "SELECT g.id, g.system_id, g.key, g.title,"
	         " (SELECT crc32 FROM files WHERE game_id=g.id AND crc32 IS NOT NULL LIMIT 1),"
	         " (SELECT serial FROM files WHERE game_id=g.id AND serial IS NOT NULL LIMIT 1) %s ORDER BY g.system_id, g.id LIMIT 1", PENDING);
	for (;;) {
		pthread_mutex_lock(&lk);
		while (!kicked) pthread_cond_wait(&cv, &lk);
		kicked = 0;
		stop_now = 0;
		pthread_mutex_unlock(&lk);
		if (!menu_up()) { scrape_status("Not while a game is running"); continue; }
		pthread_mutex_lock(&lk);
		int pc = pc_active();
		pthread_mutex_unlock(&lk);
		if (pc) continue;                   /* (the status already says who's doing it) */
		cfg_load();
		db_exec(db, "UPDATE games SET scrape=NULL WHERE scrape='later'");   /* network trouble last time: try again */
		int worked = 0, fails = 0, cut = 0;
		{
			char csql[1300];
			snprintf(csql, sizeof(csql), "SELECT COUNT(*) %s", PENDING);
			sqlite3_stmt *cs;
			int total = 0;
			if (sqlite3_prepare_v2(db, csql, -1, &cs, NULL) == SQLITE_OK) {
				if (sqlite3_step(cs) == SQLITE_ROW) total = sqlite3_column_int(cs, 0);
				sqlite3_finalize(cs);
			}
			pthread_mutex_lock(&lk);
			memset(&pg, 0, sizeof(pg));
			pg.total = total;
			pthread_mutex_unlock(&lk);
		}
		running = 1;
		while (!stop_now && menu_up() && !pc_active()) {
			sqlite3_stmt *st;
			if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) { plog("scrape: %s", sqlite3_errmsg(db)); break; }
			if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); break; }
			sgame_t g;
			memset(&g, 0, sizeof(g));
			g.id = sqlite3_column_int64(st, 0);
			snprintf(g.sys, sizeof(g.sys), "%s", sqlite3_column_text(st, 1));
			snprintf(g.key, sizeof(g.key), "%s", sqlite3_column_text(st, 2));
			snprintf(g.title, sizeof(g.title), "%s", sqlite3_column_text(st, 3));
			if (sqlite3_column_text(st, 4)) snprintf(g.crc, sizeof(g.crc), "%s", sqlite3_column_text(st, 4));
			if (sqlite3_column_text(st, 5)) snprintf(g.serial, sizeof(g.serial), "%s", sqlite3_column_text(st, 5));
			sqlite3_finalize(st);
			const gsys_t *gs = system_by_id(g.sys);
			g.disc = gs && gs->type == 's';
			g.arcade = gs && gs->cat == CAT_ARCADE;
			if (!strncmp(g.key, "disc:", 5)) {            /* disc:<code>:<size>:<sample> */
				const char *p = strchr(g.key + 5, ':');
				if (p) g.size = atol(p + 1);
			}
			scrape_status("Looking up %s games", gs ? gs->name : g.sys);
			pthread_mutex_lock(&lk);
			snprintf(pg.system, sizeof(pg.system), "%s", gs ? gs->name : g.sys);
			snprintf(pg.game, sizeof(pg.game), "%.119s", g.title);
			pg.source[0] = 0;
			pthread_mutex_unlock(&lk);
			int r = scrape_one(db, &g);
			pthread_mutex_lock(&lk);
			pg.done++;
			if (r < 0) pg.later++;
			else if (r) pg.found++;
			else pg.not_found++;
			pthread_mutex_unlock(&lk);
			if (r < 0) {
				/* one source unreachable: this game later; several in a row: stop, the user can ask again */
				mark(db, g.id, "later");
				if (++fails >= 3) { cut = 1; break; }
				continue;
			}
			fails = 0;
			mark(db, g.id, r ? "done" : "miss");
			worked++;
		}
		running = 0;
		if (worked) plog("looked up %d game(s)", worked);
		if (pc_active()) { /* the app took over: its status stands */ }
		else if (cut) scrape_status("No connection: stopped (choose Get box art to try again)");
		else if (stop_now) scrape_status("Stopped");
		else if (!menu_up()) scrape_status("Stopped: a game was started");
		else scrape_status("Done");
	}
	return NULL;
}

void scraper_start(void)
{
	cfg_load();
	pthread_t t;
	if (pthread_create(&t, NULL, thread, NULL) == 0) pthread_detach(t);
}

void scraper_kick(void)
{
	pthread_mutex_lock(&lk);
	kicked = 1;
	pthread_cond_signal(&cv);
	pthread_mutex_unlock(&lk);
}

void scraper_set(sqlite3 *db, int on)
{
	(void)db;
	if (on) scraper_kick();             /* Get box art: go through the games once */
	else stop_now = 1;                  /* stop */
}

static int count(sqlite3 *db, const char *sql)
{
	sqlite3_stmt *st;
	int n = 0;
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
		if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
		sqlite3_finalize(st);
	}
	return n;
}

void scraper_info(sqlite3 *db, scrape_info_t *o)
{
	char sql[1200];
	memset(o, 0, sizeof(*o));
	o->on = running || kicked || pc_active();
	o->running = running || pc_active();
	snprintf(sql, sizeof(sql), "SELECT COUNT(*) %s", PENDING);
	o->pending = count(db, sql);
	o->done = count(db, "SELECT COUNT(*) FROM games WHERE scrape='done'");
	o->missing = count(db, "SELECT COUNT(*) FROM games WHERE scrape='miss' AND based_on IS NULL");
	pthread_mutex_lock(&lk);
	snprintf(o->status, sizeof(o->status), "%s", status_text);
	pthread_mutex_unlock(&lk);
}

int scraper_retry(sqlite3 *db, long long gid)
{
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "UPDATE games SET scrape=NULL WHERE id=?", -1, &st, NULL) != SQLITE_OK) return -1;
	sqlite3_bind_int64(st, 1, gid);
	sqlite3_step(st);
	sqlite3_finalize(st);
	/* looked up the next time the user chooses Get box art */
	return 0;
}

int scraper_sources(scrape_source_t *out, int max)
{
	cfg_load();
	const source_t *src[NSRC];
	int n = sources_in_order(src);
	for (int i = 0; i < n && i < max; i++) {
		out[i].id = src[i]->id;
		out[i].name = src[i]->name;
		out[i].needs = src[i]->needs;
		out[i].on = source_on(src[i]);
		out[i].ready = src[i]->ready();
	}
	return n < max ? n : max;
}

int scraper_set_sources(sqlite3 *db, const char *order, const char *off)
{
	if (cfg_save_sources(order, off)) return -1;
	/* look again at games that are missing something: a newly turned-on source may have it */
	db_exec(db, "UPDATE games SET scrape=NULL WHERE scrape='miss' OR (scrape='done' AND (description IS NULL OR art_box IS NULL))");
	return 0;
}

/* ---------- the Phosphor app looking things up on the PC ----------
 * The app asks for the games that still need looking up (with what it needs to match them: CRC,
 * product code, file name, which fields are already filled), looks them up with the same sources,
 * order and accounts, makes the box art, and sends it back (PUT /art) with the details
 * (scrape_result). Fields a game already has are never replaced; the game is marked like the SS1's
 * own run would. */

static const char *const COLS[F_COUNT] = { "description", "developer", "publisher", "genre", "year", "players", "region" };

cJSON *scraper_todo(sqlite3 *db, cJSON *req)
{
	const char *by = cJSON_GetStringValue(cJSON_GetObjectItem(req, "from"));
	cJSON *lim = cJSON_GetObjectItem(req, "limit");
	int limit = cJSON_IsNumber(lim) && lim->valueint > 0 && lim->valueint <= 5000 ? lim->valueint : 500;
	int start = cJSON_IsTrue(cJSON_GetObjectItem(req, "start"));
	cfg_load();
	if (start) db_exec(db, "UPDATE games SET scrape=NULL WHERE scrape='later'");
	char sql[1600];
	snprintf(sql, sizeof(sql), "SELECT COUNT(*) %s", PENDING);
	int total = count(db, sql);
	pthread_mutex_lock(&lk);
	pc_until = time(NULL) + 60;
	snprintf(pc_by, sizeof(pc_by), "%s", by && *by ? by : "the Phosphor app");
	if (start) { memset(&pg, 0, sizeof(pg)); pg.total = total; }
	snprintf(status_text, sizeof(status_text), "Being looked up by %s", pc_by);
	stop_now = 1;                          /* the SS1's own run steps aside */
	pthread_mutex_unlock(&lk);
	cJSON *o = cJSON_CreateObject();
	cJSON_AddNumberToObject(o, "total", total);
	/* the same settings the SS1 would use (sent over the home network only) */
	cJSON *cf = cJSON_AddObjectToObject(o, "config");
	const source_t *src[NSRC];
	int n = sources_in_order(src);
	cJSON *ord = cJSON_AddArrayToObject(cf, "order");
	for (int i = 0; i < n; i++) if (source_on(src[i])) cJSON_AddItemToArray(ord, cJSON_CreateString(src[i]->id));
	cJSON *ac = cJSON_AddObjectToObject(cf, "accounts");
	for (int i = 0; i < NACCT; i++) {
		const char *v = scfg(ACCT[i][0], ACCT[i][1]);
		if (!*v) continue;
		cJSON *sec = cJSON_GetObjectItem(ac, ACCT[i][0]);
		if (!sec) sec = cJSON_AddObjectToObject(ac, ACCT[i][0]);
		cJSON_AddStringToObject(sec, ACCT[i][1], v);
	}
	cJSON *arr = cJSON_AddArrayToObject(o, "games");
	snprintf(sql, sizeof(sql),
	         "SELECT g.id, g.system_id, g.key, g.title,"
	         " (SELECT crc32 FROM files WHERE game_id=g.id AND crc32 IS NOT NULL LIMIT 1),"
	         " (SELECT serial FROM files WHERE game_id=g.id AND serial IS NOT NULL LIMIT 1),"
	         " g.art_box IS NOT NULL, g.description IS NOT NULL, g.developer IS NOT NULL, g.publisher IS NOT NULL,"
	         " g.genre IS NOT NULL, g.year IS NOT NULL, g.players IS NOT NULL, g.region IS NOT NULL"
	         " %s ORDER BY g.system_id, g.id LIMIT %d", PENDING, limit);
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
		while (sqlite3_step(st) == SQLITE_ROW) {
			cJSON *g = cJSON_CreateObject();
			const char *sys = (const char *)sqlite3_column_text(st, 1), *key = (const char *)sqlite3_column_text(st, 2);
			cJSON_AddNumberToObject(g, "id", (double)sqlite3_column_int64(st, 0));
			cJSON_AddStringToObject(g, "sys", sys ? sys : "");
			cJSON_AddStringToObject(g, "key", key ? key : "");
			cJSON_AddStringToObject(g, "title", sqlite3_column_text(st, 3) ? (const char *)sqlite3_column_text(st, 3) : "");
			if (sqlite3_column_text(st, 4)) cJSON_AddStringToObject(g, "crc", (const char *)sqlite3_column_text(st, 4));
			if (sqlite3_column_text(st, 5)) cJSON_AddStringToObject(g, "serial", (const char *)sqlite3_column_text(st, 5));
			const gsys_t *gs = system_by_id(sys ? sys : "");
			cJSON_AddBoolToObject(g, "disc", gs && gs->type == 's');
			cJSON_AddBoolToObject(g, "arcade", gs && gs->cat == CAT_ARCADE);
			if (gs) cJSON_AddStringToObject(g, "system", gs->name);
			if (key && !strncmp(key, "disc:", 5)) {
				const char *p = strchr(key + 5, ':');
				if (p) cJSON_AddNumberToObject(g, "size", (double)atol(p + 1));
			}
			cJSON_AddBoolToObject(g, "has_art", sqlite3_column_int(st, 6));
			cJSON *has = cJSON_AddArrayToObject(g, "has");
			for (int i = 0; i < F_COUNT; i++) if (sqlite3_column_int(st, 7 + i)) cJSON_AddItemToArray(has, cJSON_CreateString(COLS[i]));
			cJSON_AddItemToArray(arr, g);
		}
		sqlite3_finalize(st);
	}
	return o;
}

/* {"id", "found": bool, "later": bool (network trouble), "official", "fields": {description, ...},
 *  "game": what the app is on (for the TV), "source"} after any PUT /art for it */
const char *scraper_result(sqlite3 *db, cJSON *req)
{
	cJSON *idj = cJSON_GetObjectItem(req, "id");
	if (!cJSON_IsNumber(idj)) return "id missing";
	long long id = (long long)idj->valuedouble;
	sqlite3_stmt *st;
	sgame_t g;
	memset(&g, 0, sizeof(g));
	if (sqlite3_prepare_v2(db, "SELECT system_id, title FROM games WHERE id=?", -1, &st, NULL) != SQLITE_OK) return "database";
	sqlite3_bind_int64(st, 1, id);
	if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); return "no such game"; }
	snprintf(g.sys, sizeof(g.sys), "%s", sqlite3_column_text(st, 0));
	snprintf(g.title, sizeof(g.title), "%s", sqlite3_column_text(st, 1));
	sqlite3_finalize(st);
	g.id = id;
	int found = cJSON_IsTrue(cJSON_GetObjectItem(req, "found")), later = cJSON_IsTrue(cJSON_GetObjectItem(req, "later"));
	cJSON *f = cJSON_GetObjectItem(req, "fields");
	for (int i = 0; found && i < F_COUNT; i++) {
		const char *v = cJSON_GetStringValue(cJSON_GetObjectItem(f, COLS[i]));
		if (!v || !*v) continue;
		sres_t tmp;
		memset(&tmp, 0, sizeof(tmp));
		sres_set(&tmp, 0, v);                       /* trimmed, Windows line ends out */
		if (tmp.f[0] && strlen(tmp.f[0]) < 20000) put_text(db, id, COLS[i], tmp.f[0]);
		sres_free(&tmp);
	}
	const char *off = cJSON_GetStringValue(cJSON_GetObjectItem(req, "official"));
	if (found && off && *off && strlen(off) < 400 && sqlite3_prepare_v2(db, "UPDATE games SET ref_name=? WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_text(st, 1, off, -1, SQLITE_STATIC);
		sqlite3_bind_int64(st, 2, id);
		sqlite3_step(st);
		sqlite3_finalize(st);
	}
	int art = has_col(db, id, "art_box");
	if (!art && disc1_art(db, &g)) art = 1;
	const char *state = later && !art ? "later" : art ? "done" : "miss";
	mark(db, id, state);
	const char *by = cJSON_GetStringValue(cJSON_GetObjectItem(req, "from"));
	const char *sysname = cJSON_GetStringValue(cJSON_GetObjectItem(req, "system"));
	pthread_mutex_lock(&lk);
	pc_until = time(NULL) + 60;
	if (by && *by) snprintf(pc_by, sizeof(pc_by), "%s", by);
	pg.done++;
	if (!strcmp(state, "later")) pg.later++;
	else if (!strcmp(state, "done")) pg.found++;
	else pg.not_found++;
	snprintf(pg.game, sizeof(pg.game), "%.119s", g.title);
	snprintf(pg.system, sizeof(pg.system), "%s", sysname && *sysname ? sysname : g.sys);
	snprintf(pg.source, sizeof(pg.source), "%.39s", pc_by);
	snprintf(status_text, sizeof(status_text), "Being looked up by %s", pc_by);
	pthread_mutex_unlock(&lk);
	return NULL;
}

/* the app is done (or stopped): the SS1 may run its own again */
void scraper_pc_done(const char *status)
{
	pthread_mutex_lock(&lk);
	pc_until = 0;
	snprintf(status_text, sizeof(status_text), "%s", status && *status ? status : "Done");
	pthread_mutex_unlock(&lk);
}
