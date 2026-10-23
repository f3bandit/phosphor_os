/* phosphord scraper source: libretro (no account).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Each system's official game list (No-Intro for cartridges, Redump for discs) and libretro's
 * details lists (developer, publisher, genre, year, players; PlayStation also has descriptions) are
 * fetched once and kept in the library database (table ref). A game is matched by its CRC32
 * (cartridges) or product code + size (discs). This is also where a game gets its official name,
 * which the other sources use to look it up. Box art: libretro's thumbnails, by official name. */
#include "scrape_src.h"
#include "db.h"
#include "log.h"
#include "net.h"
#include "paths.h"
#include "ident.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

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
void serial_norm(const char *in, char *out, size_t n)
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
	if (++l->n % 2000 == 0) { db_exec(l->db, "COMMIT"); db_exec(l->db, "BEGIN"); }
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
	if (++l->n % 2000 == 0) { db_exec(l->db, "COMMIT"); db_exec(l->db, "BEGIN"); }
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
	net_escape(lr, esc, sizeof(esc));
	scrape_status("Getting the %s game list", lr);
	snprintf(dest, sizeof(dest), "%s/main.dat", dir);
	snprintf(url, sizeof(url), "%s/%s/%s.dat", LR_DB, disc ? "redump" : "no-intro", esc);
	int rc = net_get_file(url, dest);
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
		int r = net_get_file(url, dest);
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

/* "(Disc 2)" -> 2; 0 when not a numbered disc */
static int disc_no(const char *name)
{
	const char *d = strcasestr(name, "(Disc ");
	return d && isdigit((unsigned char)d[6]) ? atoi(d + 6) : 0;
}

/* the words in a name's first brackets, lower case: "(USA, Canada)" -> "usa, canada" */
static void regions(const char *name, char *out, size_t n)
{
	region_of(name, out, n);
	for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
}

/* the title words alone, the same way on both sides: "Legend of Zelda, The - A Link..." and
   "The Legend of Zelda - A Link..." both become "legend of zelda a link..." */
static void name_key(const char *name, char *out, int n)
{
	char base[520];
	name_base(name, base, sizeof(base));
	title_norm(!strncasecmp(base, "The ", 4) ? base + 4 : base, out, n);
}

/* No product code or CRC in the lists (a re-dump, a different revision, a file the code couldn't be
 * read from): the file name, when it is the list's name, or the same title words with the same disc
 * number, the file's region first. Only an exact title counts, never a guess. */
static long long by_name(sqlite3 *db, const char *lrs, const sgame_t *g)
{
	sqlite3_stmt *st;
	long long rid = -1;
	char sql[700];
	snprintf(sql, sizeof(sql), "SELECT id FROM ref WHERE lrsys IN (%s) AND name=? COLLATE NOCASE LIMIT 1", lrs);
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_text(st, 1, g->title, -1, SQLITE_STATIC);
		if (sqlite3_step(st) == SQLITE_ROW) rid = sqlite3_column_int64(st, 0);
		sqlite3_finalize(st);
	}
	if (rid >= 0) return rid;
	char want[512], wreg[120];
	name_key(g->title, want, sizeof(want));
	if (strlen(want) < 2) return -1;
	regions(g->title, wreg, sizeof(wreg));
	int wdisc = disc_no(g->title), best = -1;
	/* the lists' names start with the same letter (or "The ...") */
	char l1[8], l2[8];
	snprintf(l1, sizeof(l1), "%c%%", want[0]);
	snprintf(l2, sizeof(l2), "the %c%%", want[0]);
	snprintf(sql, sizeof(sql), "SELECT id, name FROM ref WHERE lrsys IN (%s) AND (name LIKE ?1 OR name LIKE ?2)", lrs);
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return -1;
	sqlite3_bind_text(st, 1, l1, -1, SQLITE_STATIC);
	sqlite3_bind_text(st, 2, l2, -1, SQLITE_STATIC);
	while (sqlite3_step(st) == SQLITE_ROW) {
		const char *nm = (const char *)sqlite3_column_text(st, 1);
		char k[512], reg[120];
		name_key(nm, k, sizeof(k));
		if (strcmp(k, want)) continue;
		int d = disc_no(nm);
		if (d != wdisc && !(wdisc == 0 && d == 1)) continue;
		regions(nm, reg, sizeof(reg));
		int score = 1;
		if (wreg[0] && !strcmp(reg, wreg)) score = 4;                    /* same region tag */
		else if (wreg[0] && strstr(reg, wreg)) score = 3;                /* "usa" in "usa, canada" */
		else if (!wreg[0] && strstr(reg, "usa")) score = 2;              /* untagged file: USA first */
		if (strstr(nm, "(Beta") || strstr(nm, "(Proto") || strstr(nm, "(Demo")) score -= 1;
		if (score > best) { best = score; rid = sqlite3_column_int64(st, 0); }
	}
	sqlite3_finalize(st);
	return rid;
}

/* libretro names its pictures after the list's names, but not always the same list version: also
   the name with only its first region, and without the disc number */
static void add_thumbs(sres_t *r, const char *lrsys, const char *official)
{
	char e1[400], tn[400], e2[700], url[1300], v[400];
	net_escape(lrsys, e1, sizeof(e1));
	for (int k = 0; k < 3; k++) {
		snprintf(v, sizeof(v), "%s", official);
		if (k == 1) {                                     /* "(USA, Canada)" -> "(USA)" */
			char *p = strchr(v, '('), *c = p ? strchr(p, ',') : NULL, *e = p ? strchr(p, ')') : NULL;
			if (!c || !e || c > e) continue;
			memmove(c, e, strlen(e) + 1);
		} else if (k == 2) {                              /* without " (Disc 1)" */
			char *d = strstr(v, " (Disc ");
			if (!d) continue;
			char *e = strchr(d + 1, ')');
			if (!e) continue;
			memmove(d, e + 1, strlen(e + 1) + 1);
		}
		thumb_name(v, tn, sizeof(tn));
		net_escape(tn, e2, sizeof(e2));
		snprintf(url, sizeof(url), "%s/%s/Named_Boxarts/%s.png", LR_THUMBS, e1, e2);
		sres_art(r, "libretro", url);
	}
}

/* returns 1 found, 0 not in the lists, -1 network trouble (try again later) */
static int lr_lookup(sqlite3 *db, sgame_t *g, sres_t *r)
{
	const lrmap_t *m = map_for(g->sys);
	if (!m) return 0;
	for (int i = 0; i < 3 && m->lr[i]; i++)
		if (ref_load(db, m->lr[i], m->disc) < 0) return -1;

	sqlite3_stmt *st;
	long long rid = -1;
	char sql[700], lrs[400] = "";
	for (int i = 0; i < 3 && m->lr[i]; i++) {
		char one[140];
		snprintf(one, sizeof(one), "%s'%s'", i ? "," : "", m->lr[i]);
		strncat(lrs, one, sizeof(lrs) - strlen(lrs) - 1);
	}
	char ser[64] = "";
	serial_norm(g->serial, ser, sizeof(ser));
	if (m->disc && ser[0]) {
		/* same product code, the one with this size first */
		snprintf(sql, sizeof(sql), "SELECT id FROM ref WHERE lrsys IN (%s) AND serial=? ORDER BY (size=?) DESC, id LIMIT 1", lrs);
		if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, ser, -1, SQLITE_STATIC);
			sqlite3_bind_int64(st, 2, g->size);
			if (sqlite3_step(st) == SQLITE_ROW) rid = sqlite3_column_int64(st, 0);
			sqlite3_finalize(st);
		}
		/* Redump writes some codes with a letter (collections: SLUS-01224B) or several codes per
		 * disc: then the code must appear in it and the size must match */
		snprintf(sql, sizeof(sql), "SELECT id FROM ref WHERE lrsys IN (%s) AND instr(serial, ?) > 0 AND size=? ORDER BY id LIMIT 1", lrs);
		if (rid < 0 && g->size > 0 && sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, ser, -1, SQLITE_STATIC);
			sqlite3_bind_int64(st, 2, g->size);
			if (sqlite3_step(st) == SQLITE_ROW) rid = sqlite3_column_int64(st, 0);
			sqlite3_finalize(st);
		}
	} else if (!m->disc && g->crc[0]) {
		snprintf(sql, sizeof(sql), "SELECT id FROM ref WHERE lrsys IN (%s) AND crc=? LIMIT 1", lrs);
		if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, g->crc, -1, SQLITE_STATIC);
			if (sqlite3_step(st) == SQLITE_ROW) rid = sqlite3_column_int64(st, 0);
			sqlite3_finalize(st);
		}
	}
	if (rid < 0) rid = by_name(db, lrs, g);
	if (rid < 0) return 0;
	if (sqlite3_prepare_v2(db, "SELECT name, lrsys, description, developer, publisher, genre, year, players FROM ref WHERE id=?", -1, &st, NULL) != SQLITE_OK)
		return 0;
	sqlite3_bind_int64(st, 1, rid);
	if (sqlite3_step(st) == SQLITE_ROW) {
		snprintf(g->official, sizeof(g->official), "%s", sqlite3_column_text(st, 0));
		snprintf(g->lrsys, sizeof(g->lrsys), "%s", sqlite3_column_text(st, 1));
		static const int map[] = { F_DESC, F_DEV, F_PUB, F_GENRE, F_YEAR, F_PLAYERS };
		for (int i = 0; i < 6; i++) sres_set(r, map[i], (const char *)sqlite3_column_text(st, 2 + i));
	}
	sqlite3_finalize(st);
	char region[80];
	region_of(g->official, region, sizeof(region));
	sres_set(r, F_REGION, region);
	add_thumbs(r, g->lrsys, g->official);
	return 1;
}

static int lr_ready(void) { return 1; }

const source_t SRC_LIBRETRO = { "libretro", "libretro", NULL, lr_ready, lr_lookup };
