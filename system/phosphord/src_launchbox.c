/* phosphord scraper source: the LaunchBox Games Database, no account.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * LaunchBox publishes its whole database as one download (Metadata.zip, ~110 MB; the XML inside is
 * ~515 MB). It's fetched once, read as a stream (never held in memory), and only the systems that
 * have games are kept, in the library database: names, alternate names, descriptions, developer,
 * publisher, genre, year, players, and the front box art file names. The zip is deleted after.
 *
 * Lookups are by the game's official name (from libretro or ArcadeDB, i.e. matched by content
 * first), never by file name. Box art: images.launchbox-app.com, the game's region first. */
#include "scrape_src.h"
#include "db.h"
#include "ident.h"
#include "log.h"
#include "net.h"
#include "paths.h"
#include "miniz.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#define LB_ZIP_URL "https://gamesdb.launchbox-app.com/Metadata.zip"
#define LB_IMAGES "https://images.launchbox-app.com/"

typedef struct { const char *sys; const char *plat[3]; } lbmap_t;
static const lbmap_t MAP[] = {
	{ "PSX", { "Sony Playstation" } },
	{ "Saturn", { "Sega Saturn" } },
	{ "MegaCD", { "Sega CD" } },
	{ "TGFX16CD", { "NEC TurboGrafx-CD" } },
	{ "NES", { "Nintendo Entertainment System", "Nintendo Famicom Disk System" } },
	{ "SNES", { "Super Nintendo Entertainment System" } },
	{ "Genesis", { "Sega Genesis" } },
	{ "N64", { "Nintendo 64" } },
	{ "SMS", { "Sega Master System", "Sega SG-1000" } },
	{ "TGFX16", { "NEC TurboGrafx-16" } },
	{ "ATARI7800", { "Atari 7800", "Atari 2600" } },
	{ "GAMEBOY", { "Nintendo Game Boy", "Nintendo Game Boy Color" } },
	{ "GBA", { "Nintendo Game Boy Advance" } },
	{ "GameGear", { "Sega Game Gear" } },
	{ "WonderSwan", { "WonderSwan", "WonderSwan Color" } },
	{ "AtariLynx", { "Atari Lynx" } },
	{ "Arcade", { "Arcade" } },
	{ "C64", { "Commodore 64" } },
	{ "MSX", { "Microsoft MSX", "Microsoft MSX2" } },
	{ "Atari800", { "Atari 800" } },
};
#define NMAP (int)(sizeof(MAP) / sizeof(MAP[0]))

static const lbmap_t *map_for(const char *sys)
{
	for (int i = 0; i < NMAP; i++) if (!strcmp(MAP[i].sys, sys)) return &MAP[i];
	return NULL;
}

static const char *SCHEMA =
	"CREATE TABLE IF NOT EXISTS lb_game (dbid INTEGER PRIMARY KEY, platform TEXT, norm TEXT, name TEXT,"
	" overview TEXT, developer TEXT, publisher TEXT, genre TEXT, year TEXT, players TEXT);"
	"CREATE INDEX IF NOT EXISTS lb_game_norm ON lb_game(platform, norm);"
	"CREATE TABLE IF NOT EXISTS lb_alt (dbid INTEGER, norm TEXT);"
	"CREATE INDEX IF NOT EXISTS lb_alt_norm ON lb_alt(norm);"
	"CREATE TABLE IF NOT EXISTS lb_image (dbid INTEGER, region TEXT, file TEXT);"
	"CREATE INDEX IF NOT EXISTS lb_image_dbid ON lb_image(dbid);";

/* ---------- reading Metadata.xml out of the zip, as a stream ---------- */

typedef struct {
	sqlite3 *db;
	sqlite3_stmt *game, *alt, *img, *known;
	char plats[1024];           /* |platform|platform| we keep */
	int games, imgs, pending;
} lbload_t;

/* short write transactions, so identifying and the API (favourites, links) aren't kept waiting */
static void batch(lbload_t *l)
{
	if (++l->pending < 2000) return;
	l->pending = 0;
	db_exec(l->db, "COMMIT");
	db_exec(l->db, "BEGIN");
}

/* text of <tag>...</tag> inside a block; "" if missing */
static void field(const char *blk, const char *tag, char *out, size_t n)
{
	char open[48], close[48];
	snprintf(open, sizeof(open), "<%s>", tag);
	snprintf(close, sizeof(close), "</%s>", tag);
	out[0] = 0;
	const char *a = strstr(blk, open);
	if (!a) return;
	a += strlen(open);
	const char *b = strstr(a, close);
	if (!b) return;
	size_t len = (size_t)(b - a);
	if (len >= n) len = n - 1;
	memcpy(out, a, len);
	out[len] = 0;
	xml_unescape(out);
}

static void bind_or_null(sqlite3_stmt *st, int i, const char *v)
{
	if (v && *v) sqlite3_bind_text(st, i, v, -1, SQLITE_TRANSIENT);
	else sqlite3_bind_null(st, i);
}

static int kept_platform(lbload_t *l, const char *p)
{
	char k[200];
	snprintf(k, sizeof(k), "|%s|", p);
	return strstr(l->plats, k) != NULL;
}

/* games come first in the file, then alternate names, then images: keep only those of kept games */
static int known(lbload_t *l, long long dbid)
{
	sqlite3_reset(l->known);
	sqlite3_bind_int64(l->known, 1, dbid);
	return sqlite3_step(l->known) == SQLITE_ROW;
}

static void on_block(lbload_t *l, const char *kind, char *blk)
{
	static char name[600], plat[200], ov[16384], dev[300], pub[300], gen[300], year[32], pl[16], id[32], norm[600], file[300], type[100], reg[100];
	if (!strcmp(kind, "Game")) {
		field(blk, "Platform", plat, sizeof(plat));
		if (!kept_platform(l, plat)) return;
		field(blk, "Name", name, sizeof(name));
		field(blk, "DatabaseID", id, sizeof(id));
		field(blk, "Overview", ov, sizeof(ov));
		field(blk, "Developer", dev, sizeof(dev));
		field(blk, "Publisher", pub, sizeof(pub));
		field(blk, "Genres", gen, sizeof(gen));
		field(blk, "MaxPlayers", pl, sizeof(pl));
		field(blk, "ReleaseYear", year, sizeof(year));
		if (!year[0]) { field(blk, "ReleaseDate", year, sizeof(year)); year[4 < sizeof(year) ? 4 : 0] = 0; }
		for (char *p = gen; *p; p++) if (*p == ';') *p = ',';
		title_norm(name, norm, sizeof(norm));
		sqlite3_reset(l->game);
		sqlite3_bind_int64(l->game, 1, atoll(id));
		sqlite3_bind_text(l->game, 2, plat, -1, SQLITE_STATIC);
		sqlite3_bind_text(l->game, 3, norm, -1, SQLITE_STATIC);
		sqlite3_bind_text(l->game, 4, name, -1, SQLITE_STATIC);
		bind_or_null(l->game, 5, ov);
		bind_or_null(l->game, 6, dev);
		bind_or_null(l->game, 7, pub);
		bind_or_null(l->game, 8, gen);
		bind_or_null(l->game, 9, year);
		bind_or_null(l->game, 10, strcmp(pl, "0") ? pl : "");
		sqlite3_step(l->game);
		l->games++;
		batch(l);
	} else if (!strcmp(kind, "GameAlternateName")) {
		field(blk, "DatabaseID", id, sizeof(id));
		if (!known(l, atoll(id))) return;          /* a game of a system that isn't kept */
		field(blk, "AlternateName", name, sizeof(name));
		title_norm(name, norm, sizeof(norm));
		if (!norm[0]) return;
		sqlite3_reset(l->alt);
		sqlite3_bind_int64(l->alt, 1, atoll(id));
		sqlite3_bind_text(l->alt, 2, norm, -1, SQLITE_STATIC);
		sqlite3_step(l->alt);
		batch(l);
	} else if (!strcmp(kind, "GameImage")) {
		field(blk, "Type", type, sizeof(type));
		if (strcmp(type, "Box - Front")) return;
		field(blk, "DatabaseID", id, sizeof(id));
		if (!known(l, atoll(id))) return;
		field(blk, "FileName", file, sizeof(file));
		field(blk, "Region", reg, sizeof(reg));
		sqlite3_reset(l->img);
		sqlite3_bind_int64(l->img, 1, atoll(id));
		bind_or_null(l->img, 2, reg);
		sqlite3_bind_text(l->img, 3, file, -1, SQLITE_STATIC);
		sqlite3_step(l->img);
		l->imgs++;
		batch(l);
	}
}

/* pulls complete <Game>, <GameAlternateName>, <GameImage> blocks out of buf; returns bytes used */
static size_t scan(lbload_t *l, char *buf, size_t len)
{
	static const char *KINDS[] = { "Game", "GameAlternateName", "GameImage" };
	size_t pos = 0;
	for (;;) {
		char *lt = memchr(buf + pos, '<', len - pos);
		if (!lt) return len;
		size_t at = (size_t)(lt - buf);
		int k = -1;
		for (int i = 0; i < 3; i++) {
			size_t tl = strlen(KINDS[i]);
			if (at + tl + 2 <= len && !strncmp(lt + 1, KINDS[i], tl) && lt[1 + tl] == '>') k = i;
		}
		if (k < 0) {
			if (len - at < 32) return at;          /* a tag may be cut off: keep it for the next chunk */
			pos = at + 1;
			continue;
		}
		char close[32];
		snprintf(close, sizeof(close), "</%s>", KINDS[k]);
		buf[len] = 0;
		char *end = strstr(lt, close);
		if (!end) return at;                       /* block continues in the next chunk */
		*end = 0;
		on_block(l, KINDS[k], lt);
		pos = (size_t)(end - buf) + strlen(close);
	}
}

static int lb_import(sqlite3 *db, const char *zip)
{
	FILE *f = fopen(zip, "rb");
	if (!f) return -1;
	/* end of central directory -> the Metadata.xml entry */
	unsigned char tail[66000];
	fseek(f, 0, SEEK_END);
	long fsz = ftell(f);
	long tl = fsz < (long)sizeof(tail) ? fsz : (long)sizeof(tail);
	fseek(f, fsz - tl, SEEK_SET);
	if (fread(tail, 1, (size_t)tl, f) != (size_t)tl) { fclose(f); return -1; }
	long eocd = -1;
	for (long i = tl - 22; i >= 0; i--) if (tail[i] == 0x50 && tail[i + 1] == 0x4b && tail[i + 2] == 5 && tail[i + 3] == 6) { eocd = i; break; }
	if (eocd < 0) { fclose(f); plog("launchbox: not a zip"); return -1; }
	uint32_t cd_size = tail[eocd + 12] | tail[eocd + 13] << 8 | tail[eocd + 14] << 16 | (uint32_t)tail[eocd + 15] << 24;
	uint32_t cd_off = tail[eocd + 16] | tail[eocd + 17] << 8 | tail[eocd + 18] << 16 | (uint32_t)tail[eocd + 19] << 24;
	unsigned char *cd = malloc(cd_size);
	fseek(f, (long)cd_off, SEEK_SET);
	if (!cd || fread(cd, 1, cd_size, f) != cd_size) { free(cd); fclose(f); return -1; }
	long local = -1;
	uint32_t csize = 0;
	for (uint32_t p = 0; p + 46 <= cd_size;) {
		uint16_t nl = cd[p + 28] | cd[p + 29] << 8, el = cd[p + 30] | cd[p + 31] << 8, cl = cd[p + 32] | cd[p + 33] << 8;
		if (nl == 12 && !memcmp(cd + p + 46, "Metadata.xml", 12)) {
			csize = cd[p + 20] | cd[p + 21] << 8 | cd[p + 22] << 16 | (uint32_t)cd[p + 23] << 24;
			local = cd[p + 42] | cd[p + 43] << 8 | cd[p + 44] << 16 | (long)cd[p + 45] << 24;
			break;
		}
		p += 46 + nl + el + cl;
	}
	free(cd);
	if (local < 0) { fclose(f); plog("launchbox: no Metadata.xml in the zip"); return -1; }
	unsigned char lh[30];
	fseek(f, local, SEEK_SET);
	if (fread(lh, 1, 30, f) != 30) { fclose(f); return -1; }
	fseek(f, local + 30 + (lh[26] | lh[27] << 8) + (lh[28] | lh[29] << 8), SEEK_SET);

	lbload_t l = { .db = db };
	/* keep the platforms of systems that have games */
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, "SELECT DISTINCT system_id FROM games", -1, &st, NULL) == SQLITE_OK) {
		snprintf(l.plats, sizeof(l.plats), "|");
		while (sqlite3_step(st) == SQLITE_ROW) {
			const lbmap_t *m = map_for((const char *)sqlite3_column_text(st, 0));
			for (int i = 0; m && i < 3 && m->plat[i]; i++) {
				strncat(l.plats, m->plat[i], sizeof(l.plats) - strlen(l.plats) - 2);
				strncat(l.plats, "|", sizeof(l.plats) - strlen(l.plats) - 1);
			}
		}
		sqlite3_finalize(st);
	}
	db_exec(db, SCHEMA);
	db_exec(db, "BEGIN");
	db_exec(db, "DELETE FROM lb_game; DELETE FROM lb_alt; DELETE FROM lb_image;");
	sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO lb_game VALUES(?,?,?,?,?,?,?,?,?,?)", -1, &l.game, NULL);
	sqlite3_prepare_v2(db, "INSERT INTO lb_alt VALUES(?,?)", -1, &l.alt, NULL);
	sqlite3_prepare_v2(db, "INSERT INTO lb_image VALUES(?,?,?)", -1, &l.img, NULL);
	sqlite3_prepare_v2(db, "SELECT 1 FROM lb_game WHERE dbid=?", -1, &l.known, NULL);

	mz_stream z;
	memset(&z, 0, sizeof(z));
	mz_inflateInit2(&z, -MZ_DEFAULT_WINDOW_BITS);
	size_t inbuf_sz = 256 * 1024, cap = 8 * 1024 * 1024, have = 0;
	unsigned char *in = malloc(inbuf_sz);
	char *text = malloc(cap + 1);
	uint32_t left = csize;
	int status = MZ_OK;
	while (status != MZ_STREAM_END && in && text) {
		if (z.avail_in == 0 && left) {
			size_t want = left < inbuf_sz ? left : inbuf_sz;
			size_t got = fread(in, 1, want, f);
			if (!got) break;
			left -= (uint32_t)got;
			z.next_in = in;
			z.avail_in = (unsigned)got;
		}
		z.next_out = (unsigned char *)text + have;
		z.avail_out = (unsigned)(cap - have);
		status = mz_inflate(&z, MZ_NO_FLUSH);
		if (status != MZ_OK && status != MZ_STREAM_END) { plog("launchbox: inflate error %d", status); break; }
		have = cap - z.avail_out;
		size_t used = scan(&l, text, have);
		memmove(text, text + used, have - used);
		have -= used;
		if (have == cap) { plog("launchbox: an entry is too big, stopping"); break; }
	}
	mz_inflateEnd(&z);
	free(in);
	free(text);
	fclose(f);
	sqlite3_finalize(l.game);
	sqlite3_finalize(l.alt);
	sqlite3_finalize(l.img);
	sqlite3_finalize(l.known);
	if (status != MZ_STREAM_END) { db_exec(db, "ROLLBACK"); return -1; }
	char v[1100];
	snprintf(v, sizeof(v), "%ld%s", (long)time(NULL), l.plats);
	db_meta_set(db, "launchbox", v);
	db_exec(db, "COMMIT");
	plog("launchbox: %d games, %d box fronts for %s", l.games, l.imgs, l.plats);
	return 0;
}

/* fetched once; again when a system with games isn't in it yet */
static int lb_ensure(sqlite3 *db, const lbmap_t *m)
{
	char v[1100];
	db_meta_get(db, "launchbox", v, sizeof(v));
	int have = v[0] != 0;
	for (int i = 0; have && i < 3 && m->plat[i]; i++) {
		char k[200];
		snprintf(k, sizeof(k), "|%s|", m->plat[i]);
		if (!strstr(v, k)) have = 0;
	}
	if (have) return 0;
	char dir[600], zip[700];
	snprintf(dir, sizeof(dir), "%s/ref", p_data());
	mkdirs(dir);
	snprintf(zip, sizeof(zip), "%s/launchbox-metadata.zip", dir);
	scrape_status("Getting the LaunchBox games database (one time, ~110 MB)");
	int rc = net_get_file(LB_ZIP_URL, zip);
	if (rc < 0) return -1;
	if (rc) return 1;
	scrape_status("Reading the LaunchBox games database (one time)");
	rc = lb_import(db, zip);
	unlink(zip);
	return rc ? 1 : 0;
}

static int lb_lookup(sqlite3 *db, sgame_t *g, sres_t *r)
{
	const lbmap_t *m = map_for(g->sys);
	if (!m || !g->official[0]) return 0;           /* only once matched by content */
	int e = lb_ensure(db, m);
	if (e < 0) return -1;
	if (e) return 0;
	char base[512], norm[600], ps[400] = "";
	name_base(g->official, base, sizeof(base));
	title_norm(base, norm, sizeof(norm));
	if (!norm[0]) return 0;
	for (int i = 0; i < 3 && m->plat[i]; i++) {
		char one[120];
		snprintf(one, sizeof(one), "%s'%s'", i ? "," : "", m->plat[i]);
		strncat(ps, one, sizeof(ps) - strlen(ps) - 1);
	}
	char sql[1200];
	snprintf(sql, sizeof(sql),
	         "SELECT dbid, overview, developer, publisher, genre, year, players FROM lb_game WHERE platform IN (%s) AND "
	         "(norm=?1 OR dbid IN (SELECT dbid FROM lb_alt WHERE norm=?1)) ORDER BY (norm=?1) DESC, (platform=%s'%s') DESC LIMIT 1",
	         ps, "", m->plat[0]);
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) { plog("launchbox: %s", sqlite3_errmsg(db)); return 0; }
	sqlite3_bind_text(st, 1, norm, -1, SQLITE_STATIC);
	long long dbid = -1;
	if (sqlite3_step(st) == SQLITE_ROW) {
		dbid = sqlite3_column_int64(st, 0);
		static const int map[] = { F_DESC, F_DEV, F_PUB, F_GENRE, F_YEAR, F_PLAYERS };
		for (int i = 0; i < 6; i++) sres_set(r, map[i], (const char *)sqlite3_column_text(st, 1 + i));
	}
	sqlite3_finalize(st);
	if (dbid < 0) return 0;
	/* box art: the game's region first */
	const char *want = strstr(g->official, "(USA") || strstr(g->official, "USA)") ? "North America" :
	                   strstr(g->official, "Europe") ? "Europe" : strstr(g->official, "Japan") ? "Japan" : "North America";
	if (sqlite3_prepare_v2(db, "SELECT file FROM lb_image WHERE dbid=? ORDER BY (region=?) DESC, (region='World') DESC, (region IS NULL) DESC LIMIT 2", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, dbid);
		sqlite3_bind_text(st, 2, want, -1, SQLITE_STATIC);
		while (sqlite3_step(st) == SQLITE_ROW) {
			char url[600], esc[400];
			net_escape((const char *)sqlite3_column_text(st, 0), esc, sizeof(esc));
			snprintf(url, sizeof(url), "%s%s", LB_IMAGES, esc);
			sres_art(r, "launchbox", url);
		}
		sqlite3_finalize(st);
	}
	return 1;
}

static int lb_ready(void) { return 1; }

const source_t SRC_LAUNCHBOX = { "launchbox", "LaunchBox", NULL, lb_ready, lb_lookup };
