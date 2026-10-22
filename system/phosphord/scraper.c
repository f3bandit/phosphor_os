/* phosphord: box art and details for identified games.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Source: libretro (no account). For each system with games, its official game list is fetched
 * once (No-Intro for cartridges, Redump for discs) with the details lists (developer, publisher,
 * genre, year, players; PlayStation also has descriptions) and kept in the library database.
 * A game is matched by content only: the CRC32 for cartridges, the product code (and size) for
 * discs. Box art comes from libretro's thumbnails by the matched official name.
 *
 * Skipped: games based on another (they use its art), unidentified games, games already looked up.
 * A game that isn't in the lists is marked "not found" and left alone: no guessing by name.
 * Runs only while the menu is up, one game at a time; downloads use curl (on MiSTer's Linux) with
 * the certificate bundle there. Nothing is downloaded twice; it all works offline afterwards. */
#include "scraper.h"
#include "art.h"
#include "db.h"
#include "log.h"
#include "paths.h"
#include "systems.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <strings.h>
#include <pthread.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define LR_DB "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat"
#define LR_THUMBS "https://thumbnails.libretro.com"

/* Phosphor system -> libretro list(s); disc systems use Redump, the rest No-Intro */
typedef struct { const char *id; const char *lr[3]; int disc; } lrmap_t;
static const lrmap_t MAP[] = {
	{ "PSX",        { "Sony - PlayStation" }, 1 },
	{ "Saturn",     { "Sega - Saturn" }, 1 },
	{ "MegaCD",     { "Sega - Mega-CD - Sega CD" }, 1 },
	{ "TGFX16CD",   { "NEC - PC Engine CD - TurboGrafx-CD" }, 1 },
	{ "NES",        { "Nintendo - Nintendo Entertainment System", "Nintendo - Family Computer Disk System" }, 0 },
	{ "SNES",       { "Nintendo - Super Nintendo Entertainment System" }, 0 },
	{ "Genesis",    { "Sega - Mega Drive - Genesis" }, 0 },
	{ "N64",        { "Nintendo - Nintendo 64" }, 0 },
	{ "SMS",        { "Sega - Master System - Mark III", "Sega - SG-1000" }, 0 },
	{ "TGFX16",     { "NEC - PC Engine - TurboGrafx 16", "NEC - PC Engine SuperGrafx" }, 0 },
	{ "ATARI7800",  { "Atari - 7800", "Atari - 2600" }, 0 },
	{ "GAMEBOY",    { "Nintendo - Game Boy", "Nintendo - Game Boy Color" }, 0 },
	{ "GBA",        { "Nintendo - Game Boy Advance" }, 0 },
	{ "GameGear",   { "Sega - Game Gear" }, 0 },
	{ "WonderSwan", { "Bandai - WonderSwan", "Bandai - WonderSwan Color" }, 0 },
	{ "AtariLynx",  { "Atari - Lynx" }, 0 },
};
#define NMAP (int)(sizeof(MAP) / sizeof(MAP[0]))
static const char *DETAIL_LISTS[] = { "developer", "publisher", "genre", "releaseyear", "maxusers", NULL };

static const lrmap_t *map_for(const char *sys)
{
	for (int i = 0; i < NMAP; i++) if (!strcmp(MAP[i].id, sys)) return &MAP[i];
	return NULL;
}

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int kicked = 1, on_cached = -1, running;   /* kicked: look at the queue right after starting */
static char status_text[160] = "Starting";

static void set_status(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void set_status(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	pthread_mutex_lock(&lk);
	vsnprintf(status_text, sizeof(status_text), fmt, ap);
	pthread_mutex_unlock(&lk);
	va_end(ap);
}

/* ---------- downloading ---------- */

static void url_escape(const char *in, char *out, size_t n)
{
	static const char *hex = "0123456789ABCDEF";
	size_t j = 0;
	for (const unsigned char *p = (const unsigned char *)in; *p && j + 4 < n; p++) {
		if (isalnum(*p) || strchr("-._~", *p)) out[j++] = (char)*p;
		else { out[j++] = '%'; out[j++] = hex[*p >> 4]; out[j++] = hex[*p & 15]; }
	}
	out[j] = 0;
}

/* 0 = saved, 1 = not there (404), -1 = no network / other failure */
static int fetch(const char *url, const char *dest)
{
	const char *curl = getenv("PHOS_CURL") ? getenv("PHOS_CURL") : "curl";
	const char *ca = getenv("PHOS_CACERT");
	if (!ca && file_exists("/etc/ssl/certs/cacert.pem")) ca = "/etc/ssl/certs/cacert.pem";   /* MiSTer's Linux */
	char tmp[700], code[16] = "";
	snprintf(tmp, sizeof(tmp), "%s.part", dest);
	const char *argv[16];
	int a = 0;
	argv[a++] = curl;
	argv[a++] = "-sSL";
	argv[a++] = "--max-time"; argv[a++] = "180";
	argv[a++] = "--connect-timeout"; argv[a++] = "15";
	if (ca) { argv[a++] = "--cacert"; argv[a++] = ca; }
	argv[a++] = "-w"; argv[a++] = "%{http_code}";
	argv[a++] = "-o"; argv[a++] = tmp;
	argv[a++] = url;
	argv[a] = NULL;
	int pfd[2];
	if (pipe(pfd)) return -1;
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, pfd[1], 1);
	posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
	posix_spawn_file_actions_addclose(&fa, pfd[0]);
	pid_t pid;
	int rc = posix_spawnp(&pid, curl, &fa, NULL, (char *const *)argv, environ);
	posix_spawn_file_actions_destroy(&fa);
	close(pfd[1]);
	if (rc) { close(pfd[0]); plog("can't run %s: %s", curl, strerror(rc)); return -1; }
	ssize_t r = read(pfd[0], code, sizeof(code) - 1);
	code[r > 0 ? r : 0] = 0;
	close(pfd[0]);
	int st;
	waitpid(pid, &st, 0);
	int http = atoi(code);
	if (WIFEXITED(st) && WEXITSTATUS(st) == 0 && http == 200) {
		if (rename(tmp, dest) == 0) return 0;
	}
	unlink(tmp);
	if (http == 404) return 1;
	return -1;
}

/* ---------- libretro's lists (clrmamepro .dat) ---------- */

typedef struct {
	char name[400], comment[400], serial[64], crc[12], description[4000];
	char developer[160], publisher[160], genre[100], year[12], players[12];
	long size;
	int have_rom;
} entry_t;

/* tokens: ( ) word "quoted string" */
static const char *token(const char *p, char *out, size_t n, int *quoted)
{
	while (*p && isspace((unsigned char)*p)) p++;
	*quoted = 0;
	if (!*p) return NULL;
	size_t j = 0;
	if (*p == '(' || *p == ')') { out[0] = *p; out[1] = 0; return p + 1; }
	if (*p == '"') {
		*quoted = 1;
		p++;
		while (*p && *p != '"') {
			if (*p == '\\' && p[1]) p++;
			if (j + 1 < n) out[j++] = *p;
			p++;
		}
		if (*p) p++;
	} else {
		while (*p && !isspace((unsigned char)*p) && *p != '(' && *p != ')') { if (j + 1 < n) out[j++] = *p; p++; }
	}
	out[j] = 0;
	return p;
}

/* calls cb for every game ( ... ) block */
static int parse_dat(const char *path, void (*cb)(void *, entry_t *), void *u)
{
	FILE *f = fopen(path, "rb");
	if (!f) return -1;
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = malloc((size_t)len + 1);
	if (!buf) { fclose(f); return -1; }
	len = (long)fread(buf, 1, (size_t)len, f);
	buf[len] = 0;
	fclose(f);
	static char tok[4096], key[64];
	const char *p = buf;
	int q, games = 0;
	while ((p = token(p, tok, sizeof(tok), &q))) {
		if (q || strcmp(tok, "game")) continue;
		p = token(p, tok, sizeof(tok), &q);
		if (!p || strcmp(tok, "(")) continue;
		entry_t *e = calloc(1, sizeof(*e));
		int depth = 1, in_rom = 0;
		key[0] = 0;
		while (depth > 0 && (p = token(p, tok, sizeof(tok), &q))) {
			if (!q && !strcmp(tok, "(")) { depth++; if (!strcmp(key, "rom")) in_rom = !e->have_rom; key[0] = 0; continue; }
			if (!q && !strcmp(tok, ")")) { depth--; if (in_rom && depth == 1) { in_rom = 0; e->have_rom = 1; } continue; }
			if (!key[0]) { snprintf(key, sizeof(key), "%s", tok); continue; }
			/* key value */
			if (depth == 1) {
				if (!strcmp(key, "name")) snprintf(e->name, sizeof(e->name), "%s", tok);
				else if (!strcmp(key, "comment")) snprintf(e->comment, sizeof(e->comment), "%s", tok);
				else if (!strcmp(key, "serial")) snprintf(e->serial, sizeof(e->serial), "%s", tok);
				else if (!strcmp(key, "description")) snprintf(e->description, sizeof(e->description), "%s", tok);
				else if (!strcmp(key, "developer")) snprintf(e->developer, sizeof(e->developer), "%s", tok);
				else if (!strcmp(key, "publisher")) snprintf(e->publisher, sizeof(e->publisher), "%s", tok);
				else if (!strcmp(key, "genre")) snprintf(e->genre, sizeof(e->genre), "%s", tok);
				else if (!strcmp(key, "releaseyear")) snprintf(e->year, sizeof(e->year), "%s", tok);
				else if (!strcmp(key, "users") || !strcmp(key, "maxusers")) snprintf(e->players, sizeof(e->players), "%s", tok);
			} else if (in_rom) {
				if (!strcmp(key, "crc")) { for (int i = 0; tok[i] && i < 8; i++) e->crc[i] = (char)tolower((unsigned char)tok[i]); }
				else if (!strcmp(key, "size")) e->size = atol(tok);
				else if (!strcmp(key, "serial") && !e->serial[0]) snprintf(e->serial, sizeof(e->serial), "%s", tok);
			}
			key[0] = 0;
		}
		cb(u, e);
		free(e);
		games++;
		if (!p) break;
	}
	free(buf);
	return games;
}

/* product codes written the same way on both sides: "GM T-81036 -00" -> "T-81036" */
static void serial_norm(const char *in, char *out, size_t n)
{
	const char *p = in;
	while (*p == ' ') p++;
	if (!strncasecmp(p, "GM ", 3)) p += 3;
	size_t j = 0;
	for (; *p && j + 1 < n; p++) if (!isspace((unsigned char)*p)) out[j++] = (char)toupper((unsigned char)*p);
	out[j] = 0;
	size_t l = strlen(out);
	if (l > 4 && out[l - 3] == '-' && isdigit((unsigned char)out[l - 2]) && isdigit((unsigned char)out[l - 1])) out[l - 3] = 0;
}

typedef struct { sqlite3 *db; sqlite3_stmt *ins, *upd_crc, *upd_serial, *upd_name; const char *lr; int n; } load_t;

static void bind_opt(sqlite3_stmt *st, int i, const char *v)
{
	if (v && *v) sqlite3_bind_text(st, i, v, -1, SQLITE_TRANSIENT);
	else sqlite3_bind_null(st, i);
}

static void on_main(void *u, entry_t *e)
{
	load_t *l = u;
	if (!e->name[0]) return;
	char ser[64];
	serial_norm(e->serial, ser, sizeof(ser));
	sqlite3_reset(l->ins);
	sqlite3_bind_text(l->ins, 1, l->lr, -1, SQLITE_STATIC);
	sqlite3_bind_text(l->ins, 2, e->name, -1, SQLITE_TRANSIENT);
	bind_opt(l->ins, 3, e->crc);
	bind_opt(l->ins, 4, ser);
	sqlite3_bind_int64(l->ins, 5, e->size);
	sqlite3_step(l->ins);
	l->n++;
}

/* libretro's lists have "???" where a name was lost (e.g. "Nintendo ???"): drop those words */
static void tidy(char *s)
{
	char out[4000];
	size_t j = 0;
	for (char *p = s; *p;) {
		char *w = p;
		while (*p && *p != ' ') p++;
		size_t len = (size_t)(p - w);
		int junk = len > 0;
		for (size_t i = 0; i < len; i++) if (w[i] != '?') junk = 0;
		if (!junk && len) {
			if (j && j + 1 < sizeof(out)) out[j++] = ' ';
			for (size_t i = 0; i < len && j + 1 < sizeof(out); i++) out[j++] = w[i];
		}
		while (*p == ' ') p++;
	}
	out[j] = 0;
	memcpy(s, out, j + 1);
}

/* details lists: keyed by the ROM's CRC (cartridges), the product code, or the name */
static void on_detail(void *u, entry_t *e)
{
	load_t *l = u;
	tidy(e->developer); tidy(e->publisher); tidy(e->genre);
	sqlite3_stmt *st;
	char ser[64];
	serial_norm(e->serial, ser, sizeof(ser));
	const char *nm = e->name[0] ? e->name : e->comment;
	if (e->crc[0]) { st = l->upd_crc; sqlite3_reset(st); sqlite3_bind_text(st, 8, e->crc, -1, SQLITE_TRANSIENT); }
	else if (ser[0]) { st = l->upd_serial; sqlite3_reset(st); sqlite3_bind_text(st, 8, ser, -1, SQLITE_TRANSIENT); }
	else if (nm[0]) { st = l->upd_name; sqlite3_reset(st); sqlite3_bind_text(st, 8, nm, -1, SQLITE_TRANSIENT); }
	else return;
	bind_opt(st, 1, e->description);
	bind_opt(st, 2, e->developer);
	bind_opt(st, 3, e->publisher);
	bind_opt(st, 4, e->genre);
	bind_opt(st, 5, e->year);
	bind_opt(st, 6, e->players);
	sqlite3_bind_text(st, 7, l->lr, -1, SQLITE_STATIC);
	sqlite3_step(st);
	l->n++;
}

/* fetch and load one libretro list set, once. 0 ok, -1 network */
static int ref_load(sqlite3 *db, const char *lr, int disc)
{
	char mk[200], v[32];
	snprintf(mk, sizeof(mk), "ref:%s", lr);
	if (db_meta_get(db, mk, v, sizeof(v))[0]) return 0;
	char dir[600], dest[800], url[900], esc[400];
	snprintf(dir, sizeof(dir), "%s/ref", p_data());
	mkdirs(dir);
	url_escape(lr, esc, sizeof(esc));
	set_status("Getting the %s game list", lr);
	snprintf(dest, sizeof(dest), "%s/main.dat", dir);
	snprintf(url, sizeof(url), "%s/%s/%s.dat", LR_DB, disc ? "redump" : "no-intro", esc);
	int rc = fetch(url, dest);
	if (rc < 0) return -1;
	load_t l = { .db = db, .lr = lr };
	db_exec(db, "BEGIN");
	sqlite3_stmt *del;
	if (sqlite3_prepare_v2(db, "DELETE FROM ref WHERE lrsys=?", -1, &del, NULL) == SQLITE_OK) {
		sqlite3_bind_text(del, 1, lr, -1, SQLITE_STATIC);
		sqlite3_step(del);
		sqlite3_finalize(del);
	}
	if (rc == 0) {
		sqlite3_prepare_v2(db, "INSERT INTO ref(lrsys, name, crc, serial, size) VALUES(?,?,?,?,?)", -1, &l.ins, NULL);
		parse_dat(dest, on_main, &l);
		sqlite3_finalize(l.ins);
		unlink(dest);
	}
	plog("%s: %d games in the list", lr, l.n);
	const char *SET = "UPDATE ref SET description=IFNULL(?1,description), developer=IFNULL(?2,developer), "
	                  "publisher=IFNULL(?3,publisher), genre=IFNULL(?4,genre), year=IFNULL(?5,year), players=IFNULL(?6,players) "
	                  "WHERE lrsys=?7 AND ";
	char q1[512], q2[512], q3[512];
	snprintf(q1, sizeof(q1), "%scrc=?8", SET);
	snprintf(q2, sizeof(q2), "%sserial=?8", SET);
	snprintf(q3, sizeof(q3), "%sname=?8", SET);
	sqlite3_prepare_v2(db, q1, -1, &l.upd_crc, NULL);
	sqlite3_prepare_v2(db, q2, -1, &l.upd_serial, NULL);
	sqlite3_prepare_v2(db, q3, -1, &l.upd_name, NULL);
	for (int i = 0; DETAIL_LISTS[i]; i++) {
		snprintf(url, sizeof(url), "%s/%s/%s.dat", LR_DB, DETAIL_LISTS[i], esc);
		snprintf(dest, sizeof(dest), "%s/%s.dat", dir, DETAIL_LISTS[i]);
		int r = fetch(url, dest);
		if (r < 0) { db_exec(db, "ROLLBACK"); sqlite3_finalize(l.upd_crc); sqlite3_finalize(l.upd_serial); sqlite3_finalize(l.upd_name); return -1; }
		if (r == 0) { parse_dat(dest, on_detail, &l); unlink(dest); }
	}
	sqlite3_finalize(l.upd_crc);
	sqlite3_finalize(l.upd_serial);
	sqlite3_finalize(l.upd_name);
	snprintf(v, sizeof(v), "%ld", (long)time(NULL));
	db_meta_set(db, mk, v);
	db_exec(db, "COMMIT");
	return 0;
}

/* ---------- matching and filling in ---------- */

static void region_of(const char *name, char *out, size_t n)
{
	out[0] = 0;
	const char *p = strchr(name, '(');
	if (!p) return;
	const char *e = strchr(p, ')');
	if (!e) return;
	snprintf(out, n, "%.*s", (int)(e - p - 1), p + 1);
}

/* libretro thumbnail file names: these characters become _ */
static void thumb_name(const char *name, char *out, size_t n)
{
	size_t j = 0;
	for (const char *p = name; *p && j + 1 < n; p++) out[j++] = strchr("&*/:`<>?\\|", *p) ? '_' : *p;
	out[j] = 0;
}

/* returns 1 found, 0 not in the lists, -1 network trouble (try again later) */
static int scrape_one(sqlite3 *db, long long gid, const char *sys, const char *key, const char *crc, const char *serial)
{
	const lrmap_t *m = map_for(sys);
	if (!m) return 0;
	for (int i = 0; i < 3 && m->lr[i]; i++)
		if (ref_load(db, m->lr[i], m->disc) < 0) return -1;

	sqlite3_stmt *st;
	long long rid = -1;
	char sql[600], lrs[400] = "";
	for (int i = 0; i < 3 && m->lr[i]; i++) {
		char one[140];
		snprintf(one, sizeof(one), "%s'%s'", i ? "," : "", m->lr[i]);
		strncat(lrs, one, sizeof(lrs) - strlen(lrs) - 1);
	}
	if (m->disc) {
		/* key: disc:<code>:<size>:<sample>; same product code, closest size first */
		char ser[64] = "";
		long size = 0;
		if (serial && *serial) serial_norm(serial, ser, sizeof(ser));
		const char *p = strchr(key, ':'), *q = p ? strchr(p + 1, ':') : NULL;
		if (q) size = atol(q + 1);
		if (!ser[0]) return 0;
		snprintf(sql, sizeof(sql), "SELECT id FROM ref WHERE lrsys IN (%s) AND serial=? ORDER BY (size=?) DESC, id LIMIT 1", lrs);
		if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
		sqlite3_bind_text(st, 1, ser, -1, SQLITE_STATIC);
		sqlite3_bind_int64(st, 2, size);
	} else {
		if (!crc || !*crc) return 0;
		snprintf(sql, sizeof(sql), "SELECT id FROM ref WHERE lrsys IN (%s) AND crc=? LIMIT 1", lrs);
		if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
		sqlite3_bind_text(st, 1, crc, -1, SQLITE_STATIC);
	}
	if (sqlite3_step(st) == SQLITE_ROW) rid = sqlite3_column_int64(st, 0);
	sqlite3_finalize(st);
	if (rid < 0 && m->disc) {
		/* Redump writes some codes with a letter (collections: SLUS-01224B) or several codes for one
		 * disc ("SLUS-00001, SLUS-00002"): then the code must appear in it and the size must match */
		char ser[64] = "";
		long size = 0;
		serial_norm(serial ? serial : "", ser, sizeof(ser));
		const char *p = strchr(key, ':'), *q = p ? strchr(p + 1, ':') : NULL;
		if (q) size = atol(q + 1);
		snprintf(sql, sizeof(sql), "SELECT id FROM ref WHERE lrsys IN (%s) AND instr(serial, ?) > 0 AND size=? ORDER BY id LIMIT 1", lrs);
		if (ser[0] && size > 0 && sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, ser, -1, SQLITE_STATIC);
			sqlite3_bind_int64(st, 2, size);
			if (sqlite3_step(st) == SQLITE_ROW) rid = sqlite3_column_int64(st, 0);
			sqlite3_finalize(st);
		}
	}
	if (rid < 0) return 0;

	char name[400] = "", lr[100] = "", region[80];
	if (sqlite3_prepare_v2(db, "SELECT name, lrsys FROM ref WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, rid);
		if (sqlite3_step(st) == SQLITE_ROW) {
			snprintf(name, sizeof(name), "%s", sqlite3_column_text(st, 0));
			snprintf(lr, sizeof(lr), "%s", sqlite3_column_text(st, 1));
		}
		sqlite3_finalize(st);
	}
	region_of(name, region, sizeof(region));
	/* only empty fields are filled: anything imported or set by hand wins */
	if (sqlite3_prepare_v2(db,
	        "UPDATE games SET ref_id=?1, ref_name=r.name, description=IFNULL(games.description, r.description),"
	        " developer=IFNULL(games.developer, r.developer), publisher=IFNULL(games.publisher, r.publisher),"
	        " genre=IFNULL(games.genre, r.genre), year=IFNULL(games.year, r.year), players=IFNULL(games.players, r.players),"
	        " region=IFNULL(games.region, NULLIF(?3,''))"
	        " FROM (SELECT * FROM ref WHERE id=?1) AS r WHERE games.id=?2", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, rid);
		sqlite3_bind_int64(st, 2, gid);
		sqlite3_bind_text(st, 3, region, -1, SQLITE_STATIC);
		sqlite3_step(st);
		sqlite3_finalize(st);
	} else {
		plog("scrape update: %s", sqlite3_errmsg(db));
	}

	/* box art, unless the game already has some */
	int has_art = 0;
	if (sqlite3_prepare_v2(db, "SELECT art_box IS NOT NULL FROM games WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, gid);
		if (sqlite3_step(st) == SQLITE_ROW) has_art = sqlite3_column_int(st, 0);
		sqlite3_finalize(st);
	}
	if (!has_art) {
		char tn[400], e1[400], e2[700], url[1300], tmp[600], rel[600];
		thumb_name(name, tn, sizeof(tn));
		url_escape(lr, e1, sizeof(e1));
		url_escape(tn, e2, sizeof(e2));
		snprintf(url, sizeof(url), "%s/%s/Named_Boxarts/%s.png", LR_THUMBS, e1, e2);
		snprintf(tmp, sizeof(tmp), "%s/scrape-art.png", p_tmp());
		int r = fetch(url, tmp);
		if (r < 0) return -1;
		if (r == 0) {
			if (art_store_box(sys, key, tmp, rel, sizeof(rel)) == 0 &&
			    sqlite3_prepare_v2(db, "UPDATE games SET art_box=? WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
				sqlite3_bind_text(st, 1, rel, -1, SQLITE_STATIC);
				sqlite3_bind_int64(st, 2, gid);
				sqlite3_step(st);
				sqlite3_finalize(st);
			}
			unlink(tmp);
		}
	}
	return 1;
}

/* ---------- the queue ---------- */

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

static const char *PENDING =
	"FROM games g WHERE g.scrape IS NULL AND g.based_on IS NULL AND g.key NOT LIKE 'n:%' AND g.system_id IN "
	"('PSX','Saturn','MegaCD','TGFX16CD','NES','SNES','Genesis','N64','SMS','TGFX16','ATARI7800','GAMEBOY','GBA','GameGear','WonderSwan','AtariLynx') "
	"AND EXISTS (SELECT 1 FROM files f WHERE f.game_id=g.id AND f.online=1)";

static int is_on(sqlite3 *db)
{
	if (on_cached < 0) {
		char v[8];
		db_meta_get(db, "scrape_on", v, sizeof(v));
		on_cached = v[0] == '1';
	}
	return on_cached;
}

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
	int backoff = 0;
	char sql[1200];
	snprintf(sql, sizeof(sql),
	         "SELECT g.id, g.system_id, g.key,"
	         " (SELECT crc32 FROM files WHERE game_id=g.id AND crc32 IS NOT NULL LIMIT 1),"
	         " (SELECT serial FROM files WHERE game_id=g.id AND serial IS NOT NULL LIMIT 1) %s ORDER BY g.system_id, g.id LIMIT 1", PENDING);
	for (;;) {
		pthread_mutex_lock(&lk);
		if (!kicked) {
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += backoff ? backoff : 20;
			pthread_cond_timedwait(&cv, &lk, &ts);
		}
		kicked = 0;
		pthread_mutex_unlock(&lk);
		if (!is_on(db)) { set_status("Off"); continue; }
		if (!menu_up()) { set_status("Waiting: a game is running"); continue; }
		backoff = 0;
		int worked = 0;
		while (is_on(db) && menu_up()) {
			sqlite3_stmt *st;
			if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) { plog("scrape: %s", sqlite3_errmsg(db)); break; }
			if (sqlite3_step(st) != SQLITE_ROW) { sqlite3_finalize(st); break; }
			long long gid = sqlite3_column_int64(st, 0);
			char sys[32], key[200], crc[16] = "", ser[64] = "";
			snprintf(sys, sizeof(sys), "%s", sqlite3_column_text(st, 1));
			snprintf(key, sizeof(key), "%s", sqlite3_column_text(st, 2));
			if (sqlite3_column_text(st, 3)) snprintf(crc, sizeof(crc), "%s", sqlite3_column_text(st, 3));
			if (sqlite3_column_text(st, 4)) snprintf(ser, sizeof(ser), "%s", sqlite3_column_text(st, 4));
			sqlite3_finalize(st);
			running = 1;
			const gsys_t *gs = system_by_id(sys);
			set_status("Looking up %s games", gs ? gs->name : sys);
			int r = scrape_one(db, gid, sys, key, crc, ser);
			running = 0;
			if (r < 0) {
				set_status("No connection: trying again in a few minutes");
				backoff = 300;
				break;
			}
			mark(db, gid, r ? "done" : "miss");
			worked++;
		}
		if (worked) plog("looked up %d game(s)", worked);
		if (!backoff && is_on(db) && menu_up()) set_status("Up to date");
	}
	return NULL;
}

void scraper_start(void)
{
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
	db_meta_set(db, "scrape_on", on ? "1" : "0");
	on_cached = on;
	scraper_kick();
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
	o->on = is_on(db);
	o->running = running;
	snprintf(sql, sizeof(sql), "SELECT COUNT(*) %s", PENDING);
	o->pending = count(db, sql);
	o->done = count(db, "SELECT COUNT(*) FROM games WHERE scrape='done'");
	o->missing = count(db, "SELECT COUNT(*) FROM games WHERE scrape='miss' AND based_on IS NULL");
	pthread_mutex_lock(&lk);
	snprintf(o->status, sizeof(o->status), "%s", o->on ? status_text : "Off");
	pthread_mutex_unlock(&lk);
}

int scraper_retry(sqlite3 *db, long long gid)
{
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "UPDATE games SET scrape=NULL WHERE id=?", -1, &st, NULL) != SQLITE_OK) return -1;
	sqlite3_bind_int64(st, 1, gid);
	sqlite3_step(st);
	sqlite3_finalize(st);
	scraper_kick();
	return 0;
}
