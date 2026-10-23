/* phosphord: box art and details for identified games, from the sources the user picked.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Sources are asked in the user's order (config/scraper.ini, Settings > Box art sources). For each
 * field (description, developer, publisher, genre, year, players, region, box art) the first answer
 * wins; fields a game already has (imported, set by hand) are never replaced. The title stays the
 * file name. libretro also gives a game its official name, which the name-based sources look up;
 * nothing is ever looked up by file name, so a game nobody recognises by content is left alone
 * ("not found": link it with "Based on").
 *
 * Skipped: games based on another (they use its art), unidentified games, games already looked
 * up. Runs only while the menu is up, one game at a time. Network trouble pauses it for a few
 * minutes without marking anything. */
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
		      "[screenscraper]\n; your screenscraper.fr login (optional, faster), and Phosphor's developer id\nuser =\npassword =\ndevid =\ndevpassword =\n\n"
		      "[thegamesdb]\n; API key from thegamesdb.net (free)\napikey =\n\n"
		      "[igdb]\n; Twitch developer app (dev.twitch.tv): Client ID and Client Secret\nclient_id =\nclient_secret =\n", out);
	fclose(out);
	if (rename(tmp, path)) return -1;
	cfg_load();
	return 0;
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

/* returns 1 found, 0 nobody knows it, -1 network trouble before anything was found */
static int scrape_one(sqlite3 *db, sgame_t *g)
{
	const source_t *src[NSRC];
	int n = sources_in_order(src), found = 0, trouble = 0;
	int had_art = has_col(db, g->id, "art_box");
	sres_t r;
	memset(&r, 0, sizeof(r));
	/* already filled fields count as answered */
	static const char *COL[F_COUNT] = { "description", "developer", "publisher", "genre", "year", "players", "region" };
	for (int i = 0; i < F_COUNT; i++) if (has_col(db, g->id, COL[i])) r.f[i] = strdup("");
	for (int i = 0; i < n; i++) {
		if (!source_on(src[i]) || !src[i]->ready()) continue;
		if (sres_complete(&r, had_art)) break;
		int rc = src[i]->lookup(db, g, &r);
		if (rc == 1) {
			found = 1;
			if (r.found_by[0]) strncat(r.found_by, ", ", sizeof(r.found_by) - strlen(r.found_by) - 1);
			strncat(r.found_by, src[i]->name, sizeof(r.found_by) - strlen(r.found_by) - 1);
		} else if (rc == -1) {
			trouble = 1;
		}
	}
	if (!found) { sres_free(&r); return trouble ? -1 : 0; }

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
	/* box art: the first that downloads and decodes */
	if (!had_art) {
		char tmp[600], rel[600];
		snprintf(tmp, sizeof(tmp), "%s/scrape-art.img", p_tmp());
		for (int i = 0; i < r.nart; i++) {
			int rc = net_get_file(r.art[i], tmp);
			if (rc < 0) { trouble = 1; continue; }
			if (rc) continue;
			int ok = art_store_box(g->sys, g->key, tmp, rel, sizeof(rel)) == 0;
			unlink(tmp);
			if (ok) { put_text(db, g->id, "art_box", rel); break; }
		}
	}
	sres_free(&r);
	return 1;
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
		cfg_load();
		db_exec(db, "UPDATE games SET scrape=NULL WHERE scrape='later'");   /* network trouble last time: try again */
		int worked = 0, fails = 0, cut = 0;
		running = 1;
		while (!stop_now && menu_up()) {
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
			int r = scrape_one(db, &g);
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
		if (cut) scrape_status("No connection: stopped (choose Get box art to try again)");
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
	o->on = running || kicked;
	o->running = running;
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
