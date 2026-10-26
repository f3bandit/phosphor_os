/* phosphord: what a game file is.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Cartridges and computer files: CRC32 of the ROM data, the way the No-Intro sets count it
 * (copier headers left out: iNES/FDS 16 bytes, SNES 512, Atari 7800 128, Lynx 64; N64 read in
 * big-endian .z64 order whatever the file's byte order).
 * Discs (.cue/.bin, .iso): the product code (PlayStation SYSTEM.CNF, Saturn and Mega-CD headers)
 * for looking the game up; identity is product code + data track size + CRC of its first megabyte.
 * That reads well under a second per disc; hashing whole discs took ~40 s each over USB. A .chd is compressed: its identity is
 * the SHA-1 of the original data that the .chd header carries, and its product code is read from
 * the disc inside (libchdr) for looking it up.
 * Arcade: the MAME set name in the .mra.
 *
 * Identity decides which entries are the same game: two copies on the SD card and a USB drive
 * become one game with two files, and a file renamed later keeps its history. Hashing reads whole
 * files, so it only runs while the menu is up, never while a core is running from the same drive. */
#include "ident.h"
#include "db.h"
#include "log.h"
#include <libchdr/chd.h>
#include <libchdr/cdrom.h>
#include <ctype.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define MAX_HASH_SIZE (1024L * 1024 * 1024)

static int (*stop_cb)(void);
/* progress for the scan screen (read from the API thread while identifying runs) */
static pthread_mutex_t prog_lk = PTHREAD_MUTEX_INITIALIZER;
static int prog_done, prog_total;
static char prog_sys[64], prog_file[160];
void ident_progress(int *done, int *total, char *sys, size_t sn, char *file, size_t fn)
{
	pthread_mutex_lock(&prog_lk);
	*done = prog_done;
	*total = prog_total;
	snprintf(sys, sn, "%s", prog_sys);
	snprintf(file, fn, "%s", prog_file);
	pthread_mutex_unlock(&prog_lk);
}   /* set by ident_pending: a game was started, or a rescan asked for */

/* ---------- CRC32 ---------- */

static uint32_t crc_table[256];

static void crc_init(void)
{
	if (crc_table[1]) return;
	for (uint32_t i = 0; i < 256; i++) {
		uint32_t c = i;
		for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
		crc_table[i] = c;
	}
}

static uint32_t crc_update(uint32_t crc, const unsigned char *p, size_t n)
{
	crc = ~crc;
	while (n--) crc = crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
	return ~crc;
}

typedef enum { N64_NONE, N64_Z64, N64_V64, N64_N64 } n64_order_t;

static int crc_file(const gsys_t *sys, const char *path, char out[9])
{
	out[0] = 0;
	crc_init();
	FILE *f = fopen(path, "rb");
	if (!f) return -1;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size <= 0 || size > MAX_HASH_SIZE) { fclose(f); out[0] = 0; return size <= 0 ? -1 : 0; }

	unsigned char head[128];
	size_t hn = fread(head, 1, sizeof(head), f);
	long skip = 0;
	n64_order_t order = N64_NONE;
	if (hn >= 16 && (!memcmp(head, "NES\x1a", 4) || !memcmp(head, "FDS\x1a", 4))) skip = 16;
	else if (!strcmp(sys->id, "SNES") && size % 1024 == 512) skip = 512;
	else if (hn >= 10 && !memcmp(head + 1, "ATARI7800", 9)) skip = 128;
	else if (hn >= 4 && !memcmp(head, "LYNX", 4)) skip = 64;
	else if (!strcmp(sys->id, "N64") && hn >= 4) {
		uint32_t m = (uint32_t)head[0] << 24 | head[1] << 16 | head[2] << 8 | head[3];
		order = m == 0x80371240u ? N64_Z64 : m == 0x37804012u ? N64_V64 : m == 0x40123780u ? N64_N64 : N64_NONE;
	}
	fseek(f, skip, SEEK_SET);

	static unsigned char buf[256 * 1024];
	uint32_t crc = 0;
	size_t n;
	int chunks = 0;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		if (stop_cb && ++chunks % 16 == 0 && stop_cb()) { fclose(f); return -1; }   /* finish later */
		if (order == N64_V64) {
			for (size_t i = 0; i + 1 < n; i += 2) { unsigned char t = buf[i]; buf[i] = buf[i + 1]; buf[i + 1] = t; }
		} else if (order == N64_N64) {
			for (size_t i = 0; i + 3 < n; i += 4) {
				unsigned char a = buf[i], b = buf[i + 1];
				buf[i] = buf[i + 3]; buf[i + 1] = buf[i + 2]; buf[i + 2] = b; buf[i + 3] = a;
			}
		}
		crc = crc_update(crc, buf, n);
	}
	int err = ferror(f);
	fclose(f);
	if (err) return -1;
	snprintf(out, 9, "%08x", crc);
	return 0;
}

/* CRC32 of the first `max` bytes of a file, and its size */
static int crc_head(const char *path, long max, char out[9], long *size)
{
	crc_init();
	FILE *f = fopen(path, "rb");
	if (!f) return -1;
	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	fseek(f, 0, SEEK_SET);
	static unsigned char buf[64 * 1024];
	uint32_t crc = 0;
	long left = max;
	size_t n;
	while (left > 0 && (n = fread(buf, 1, (size_t)(left < (long)sizeof(buf) ? left : (long)sizeof(buf)), f)) > 0) {
		crc = crc_update(crc, buf, n);
		left -= (long)n;
	}
	int err = ferror(f);
	fclose(f);
	if (err) return -1;
	snprintf(out, 9, "%08x", crc);
	return 0;
}

/* ---------- discs ---------- */

typedef struct {
	FILE *f;
	int sector;   /* bytes per sector in the file: 2352 (raw) or 2048 */
	int offset;   /* where the 2048 data bytes start in a sector */
	/* a .chd instead of a file: frames of 2448 bytes (2352 + subcode) in compressed hunks */
	chd_file *chd;
	uint32_t hunkbytes, frames_per_hunk, pregap;
	long cached_hunk;
	unsigned char *hunk;
} disc_t;

static const unsigned char SYNC[12] = { 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0 };

static int disc_read(disc_t *d, long lba, unsigned char out[2048])
{
	if (lba < 0 || lba > 0x7fffffffL / 2448) return -1;   /* a damaged image's sector number */
	if (d->chd) {
		long frame = lba + (long)d->pregap;
		long hunk = frame / (long)d->frames_per_hunk;
		if (hunk != d->cached_hunk) {
			if (chd_read(d->chd, (uint32_t)hunk, d->hunk) != CHDERR_NONE) return -1;
			d->cached_hunk = hunk;
		}
		const unsigned char *f = d->hunk + (frame % (long)d->frames_per_hunk) * CD_FRAME_SIZE;
		int off = !memcmp(f, SYNC, 12) ? (f[15] == 2 ? 24 : 16) : 0;   /* raw mode 1 / mode 2, or cooked */
		memcpy(out, f + off, 2048);
		return 0;
	}
	if (fseek(d->f, lba * d->sector + d->offset, SEEK_SET)) return -1;
	return fread(out, 1, 2048, d->f) == 2048 ? 0 : -1;
}

static int chd_disc_open(const char *path, disc_t *d)
{
	memset(d, 0, sizeof(*d));
	if (chd_open(path, CHD_OPEN_READ, NULL, &d->chd) != CHDERR_NONE) { d->chd = NULL; return -1; }
	const chd_header *h = chd_get_header(d->chd);
	d->hunkbytes = h->hunkbytes;
	d->frames_per_hunk = h->hunkbytes / CD_FRAME_SIZE;
	if (!d->frames_per_hunk) { chd_close(d->chd); d->chd = NULL; return -1; }
	/* track 1's pregap is stored in the image when its type starts with V */
	char meta[256];
	uint32_t len = 0;
	if (chd_get_metadata(d->chd, CDROM_TRACK_METADATA2_TAG, 0, meta, sizeof(meta) - 1, &len, NULL, NULL) == CHDERR_NONE) {
		meta[len < sizeof(meta) ? len : sizeof(meta) - 1] = 0;
		int pregap = 0;
		char pgtype[32] = "";
		const char *p = strstr(meta, "PREGAP:");
		if (p) pregap = atoi(p + 7);
		p = strstr(meta, "PGTYPE:");
		if (p) sscanf(p + 7, "%31s", pgtype);
		if (pgtype[0] == 'V') d->pregap = (uint32_t)pregap;
	}
	d->hunk = malloc(d->hunkbytes);
	d->cached_hunk = -1;
	if (!d->hunk) { chd_close(d->chd); d->chd = NULL; return -1; }
	return 0;
}

static void disc_close(disc_t *d)
{
	if (d->chd) { chd_close(d->chd); free(d->hunk); d->chd = NULL; }
	if (d->f) { fclose(d->f); d->f = NULL; }
}

/* the first data track of a .cue (or a .bin/.iso itself) */
static int disc_open(const char *path, disc_t *d, char *track, size_t tn)
{
	snprintf(track, tn, "%s", path);
	const char *dot = strrchr(path, '.');
	if (dot && !strcasecmp(dot, ".cue")) {
		FILE *c = fopen(path, "r");
		if (!c) return -1;
		char line[1200], name[1000] = "";
		while (fgets(line, sizeof(line), c)) {
			char *p = line;
			while (isspace((unsigned char)*p)) p++;
			if (!strncasecmp(p, "FILE", 4) && !name[0]) {
				char *q = strchr(p, '"'), *e = q ? strchr(q + 1, '"') : NULL;
				if (q && e) { *e = 0; snprintf(name, sizeof(name), "%s", q + 1); }
				else if (sscanf(p + 4, " %999s", name) != 1) name[0] = 0;
			}
		}
		fclose(c);
		if (!name[0]) return -1;
		const char *slash = strrchr(path, '/');
		if (name[0] == '/' || !slash) snprintf(track, tn, "%s", name);
		else snprintf(track, tn, "%.*s/%s", (int)(slash - path), path, name);
	} else if (dot && !strcasecmp(dot, ".chd")) {
		return chd_disc_open(path, d);
	}
	memset(d, 0, sizeof(*d));
	d->f = fopen(track, "rb");
	if (!d->f) return -1;
	unsigned char h[16];
	if (fread(h, 1, 16, d->f) != 16) { fclose(d->f); return -1; }
	if (!memcmp(h, SYNC, 12)) {
		d->sector = 2352;
		d->offset = h[15] == 2 ? 24 : 16;   /* mode 2 form 1 has an 8-byte subheader */
	} else {
		d->sector = 2048;
		d->offset = 0;
	}
	return 0;
}

static void trim_copy(char *dst, int n, const unsigned char *src, int len)
{
	while (len > 0 && (src[len - 1] == ' ' || src[len - 1] == 0)) len--;
	int s = 0;
	while (s < len && src[s] == ' ') s++;
	int j = 0;
	for (int i = s; i < len && j + 1 < n; i++) dst[j++] = isprint(src[i]) ? (char)src[i] : '?';
	dst[j] = 0;
}

/* SLUS_005.94 -> SLUS-00594 */
static void psx_serial(const char *raw, char *out, int n)
{
	char t[32];
	int j = 0;
	for (const char *p = raw; *p && j < 31; p++)
		if (isalnum((unsigned char)*p)) t[j++] = (char)toupper((unsigned char)*p);
	t[j] = 0;
	int a = 0;
	while (isalpha((unsigned char)t[a])) a++;
	/* some discs boot a file with an extra letter (Dragon Warrior VII: SLUSP012.06): the code is
	   the first four letters and the digits */
	if (a == 5 && isdigit((unsigned char)t[a])) { memmove(t + 4, t + 5, strlen(t + 5) + 1); a = 4; }
	if (a >= 3 && a <= 4 && t[a]) snprintf(out, (size_t)n, "%.*s-%s", a, t, t + a);
	else snprintf(out, (size_t)n, "%s", t);
}

static int psx_boot(disc_t *d, char *serial, int n)
{
	unsigned char s[2048];
	if (disc_read(d, 16, s) || s[0] != 1 || memcmp(s + 1, "CD001", 5)) return -1;
	const unsigned char *root = s + 156;
	long lba = root[2] | root[3] << 8 | root[4] << 16 | (long)root[5] << 24;
	long len = root[10] | root[11] << 8 | root[12] << 16 | (long)root[13] << 24;
	for (long sec = 0; sec * 2048 < len && sec < 16; sec++) {
		unsigned char dir[2048];
		if (disc_read(d, lba + sec, dir)) return -1;
		for (int i = 0; i < 2048;) {
			int rl = dir[i];
			if (!rl || i + rl > 2048) break;
			if (rl < 34) break;                      /* too short to be a directory record */
			int nl = dir[i + 32];
			if (i + 33 + nl > 2048) break;
			if (nl >= 10 && !strncasecmp((const char *)dir + i + 33, "SYSTEM.CNF", 10)) {
				long flba = dir[i + 2] | dir[i + 3] << 8 | dir[i + 4] << 16 | (long)dir[i + 5] << 24;
				unsigned char cnf[2049];
				if (disc_read(d, flba, cnf)) return -1;
				cnf[2048] = 0;
				char *b = strstr((char *)cnf, "BOOT");
				if (!b) return -1;
				char *st = strchr(b, '=');
				if (!st) return -1;
				char *name = st + 1, *e;
				for (char *p = name; *p && *p != '\n' && *p != '\r' && *p != ';'; p++)
					if (*p == '\\' || *p == ':' || *p == '/') name = p + 1;
				for (e = name; *e && *e != ';' && *e != '\r' && *e != '\n' && *e != ' '; e++) ;
				*e = 0;
				if (!*name) return -1;
				psx_serial(name, serial, n);
				return 0;
			}
			i += rl;
		}
	}
	return -1;
}

static int disc_serial(const gsys_t *sys, const char *path, char *serial, int n, char *track, size_t tn)
{
	disc_t d;
	int rc = disc_open(path, &d, track, tn);
	if (rc) return -3;
	unsigned char s[2048];
	rc = -1;
	if (!strcmp(sys->id, "PSX")) {
		rc = psx_boot(&d, serial, n);
	} else if (!disc_read(&d, 0, s)) {
		if (!memcmp(s, "SEGA SEGASATURN", 15)) { trim_copy(serial, n, s + 0x20, 10); rc = serial[0] ? 0 : -1; }
		else if (!memcmp(s, "SEGADISCSYSTEM", 14)) { trim_copy(serial, n, s + 0x180, 14); rc = serial[0] ? 0 : -1; }
	}
	disc_close(&d);
	return rc;
}

/* a .chd keeps the SHA-1 of the original data in its header: identity without decompressing */
static int chd_sha1(const char *path, char *out, int n)
{
	FILE *f = fopen(path, "rb");
	if (!f) return -1;
	unsigned char h[124];
	size_t got = fread(h, 1, sizeof(h), f);
	fclose(f);
	out[0] = 0;
	if (got < 108 || memcmp(h, "MComprHD", 8)) return 0;
	uint32_t ver = (uint32_t)h[12] << 24 | h[13] << 16 | h[14] << 8 | h[15];
	int off = ver == 5 ? 64 : ver == 4 ? 88 : -1;    /* raw data SHA-1 (same whatever the compression) */
	if (off < 0 || got < (size_t)off + 20) return 0;
	int zero = 1;
	for (int i = 0; i < 20; i++) if (h[off + i]) zero = 0;
	if (zero) return 0;
	for (int i = 0; i < 20 && 2 * i + 2 < n; i++) snprintf(out + 2 * i, 3, "%02x", h[off + i]);
	return 0;
}

/* ---------- arcade ---------- */

static int mra_setname(const char *path, char *out, int n)
{
	FILE *f = fopen(path, "r");
	if (!f) return -1;
	static char buf[65536];
	size_t len = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[len] = 0;
	out[0] = 0;
	char *p = strstr(buf, "<setname>");
	if (p) {
		p += 9;
		char *e = strstr(p, "</setname>");
		if (e) {
			while (p < e && isspace((unsigned char)*p)) p++;
			while (e > p && isspace((unsigned char)e[-1])) e--;
			int j = 0;
			for (; p < e && j + 1 < n; p++) out[j++] = (char)tolower((unsigned char)*p);
			out[j] = 0;
		}
	}
	return 0;
}

int ident_file(const gsys_t *sys, const char *path, ident_t *out)
{
	memset(out, 0, sizeof(*out));
	if (sys->cat == CAT_ARCADE) {
		if (mra_setname(path, out->serial, sizeof(out->serial))) return -1;
		if (out->serial[0]) snprintf(out->key, sizeof(out->key), "mra:%s", out->serial);
		return 0;
	}
	const char *dot = strrchr(path, '.');
	if (sys->type == 's' && dot && !strcasecmp(dot, ".chd")) {
		/* identity: the header's SHA-1; product code: read from the disc inside (for lookups) */
		char sha[48], track[8];
		if (chd_sha1(path, sha, sizeof(sha))) return -1;
		if (sha[0]) snprintf(out->key, sizeof(out->key), "chd:%s", sha);
		if (disc_serial(sys, path, out->serial, sizeof(out->serial), track, sizeof(track))) out->serial[0] = 0;
		return 0;
	}
	if (sys->type == 's' && !(dot && !strcasecmp(dot, ".exe"))) {
		/* Fast: product code + data track size + a CRC of its first megabyte, all read in well under
		 * a second (a whole-disc CRC takes ~40 s per disc over USB). Size and the sample keep a mod or
		 * patched disc, which usually keeps the original's product code, apart from the original. */
		char track[1024];
		int rc = disc_serial(sys, path, out->serial, sizeof(out->serial), track, sizeof(track));
		if (rc == -3) return -1;             /* can't open the disc image right now */
		if (rc) out->serial[0] = 0;          /* no product code on the disc (e.g. PC Engine CD) */
		long size;
		char sample[9];
		if (crc_head(track, 1024L * 1024, sample, &size)) return -1;
		snprintf(out->key, sizeof(out->key), "disc:%.20s:%ld:%s", out->serial[0] ? out->serial : "-", size, sample);
		return 0;
	}
	if (crc_file(sys, path, out->crc32)) return -1;
	if (out->crc32[0]) snprintf(out->key, sizeof(out->key), "crc:%s", out->crc32);
	return 0;
}

/* ---------- giving games an identity ---------- */

static long long one_ll(sqlite3 *db, const char *sql, long long a, const char *s1, const char *s2)
{
	sqlite3_stmt *st;
	long long v = -1;
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) { plog("sql: %s", sqlite3_errmsg(db)); return -1; }
	int i = 1;
	if (a >= 0) sqlite3_bind_int64(st, i++, a);
	if (s1) sqlite3_bind_text(st, i++, s1, -1, SQLITE_TRANSIENT);
	if (s2) sqlite3_bind_text(st, i++, s2, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int64(st, 0);
	sqlite3_finalize(st);
	return v;
}

static void exec2(sqlite3 *db, const char *sql, long long a, long long b)
{
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) { plog("sql: %s", sqlite3_errmsg(db)); return; }
	sqlite3_bind_int64(st, 1, a);
	if (b >= 0) sqlite3_bind_int64(st, 2, b);
	sqlite3_step(st);
	sqlite3_finalize(st);
}

/* which of two file names makes the better title: a proper name with a region tag beats "smb" */
static int title_score(const char *t)
{
	int sc = (int)strlen(t);
	if (strchr(t, '(')) sc += 100;                    /* (USA), (Europe), (Disc 1) */
	if (strchr(t, '[')) sc -= 60;                     /* [h], [b], [!]: dump tags */
	if (strchr(t, ' ')) sc += 20;
	return sc;
}

static void better_title(sqlite3 *db, long long from, long long to)
{
	sqlite3_stmt *st;
	char a[512] = "", b[512] = "";
	if (sqlite3_prepare_v2(db, "SELECT (SELECT title FROM games WHERE id=?1), (SELECT title FROM games WHERE id=?2)", -1, &st, NULL) != SQLITE_OK) return;
	sqlite3_bind_int64(st, 1, from);
	sqlite3_bind_int64(st, 2, to);
	if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_text(st, 0) && sqlite3_column_text(st, 1)) {
		snprintf(a, sizeof(a), "%s", sqlite3_column_text(st, 0));
		snprintf(b, sizeof(b), "%s", sqlite3_column_text(st, 1));
	}
	sqlite3_finalize(st);
	/* `to` with no files of its own: the file was renamed or moved, so the new name is the one to show */
	long long left = one_ll(db, "SELECT COUNT(*) FROM files WHERE game_id=?", to, NULL, NULL);
	if (a[0] && (left == 0 || title_score(a) > title_score(b))) exec2(db, "UPDATE games SET title=(SELECT title FROM games WHERE id=?1) WHERE id=?2", from, to);
}

/* two entries turned out to be the same game: keep `to`, add `from`'s history and details */
static void merge_games(sqlite3 *db, long long from, long long to)
{
	better_title(db, from, to);
	exec2(db,
	      "UPDATE games SET "
	      " favorite = MAX(favorite, (SELECT favorite FROM games WHERE id=?1)),"
	      " play_count = play_count + (SELECT play_count FROM games WHERE id=?1),"
	      " play_seconds = play_seconds + (SELECT play_seconds FROM games WHERE id=?1),"
	      " last_played = CASE WHEN (SELECT last_played FROM games WHERE id=?1) > IFNULL(last_played, 0)"
	      "   THEN (SELECT last_played FROM games WHERE id=?1) ELSE last_played END,"
	      " art_box = IFNULL(art_box, (SELECT art_box FROM games WHERE id=?1)),"
	      " description = IFNULL(description, (SELECT description FROM games WHERE id=?1)),"
	      " year = IFNULL(year, (SELECT year FROM games WHERE id=?1)),"
	      " developer = IFNULL(developer, (SELECT developer FROM games WHERE id=?1)),"
	      " publisher = IFNULL(publisher, (SELECT publisher FROM games WHERE id=?1)),"
	      " genre = IFNULL(genre, (SELECT genre FROM games WHERE id=?1)),"
	      " players = IFNULL(players, (SELECT players FROM games WHERE id=?1)),"
	      " region = IFNULL(region, (SELECT region FROM games WHERE id=?1)),"
	      " based_on = IFNULL(based_on, (SELECT based_on FROM games WHERE id=?1)),"
	      " added = MIN(IFNULL(added, 9e18), IFNULL((SELECT added FROM games WHERE id=?1), 9e18))"
	      " WHERE id=?2", from, to);
	exec2(db, "UPDATE games SET based_on=NULL WHERE id=?1 AND based_on=?1", to, -1);
	exec2(db, "UPDATE files SET game_id=?2 WHERE game_id=?1", from, to);
	exec2(db, "UPDATE games SET based_on=?2 WHERE based_on=?1", from, to);
	exec2(db, "DELETE FROM games WHERE id=?1", from, -1);
	char buf[32], v[32];
	db_meta_get(db, "last_game", buf, sizeof(buf));
	if (buf[0] && atoll(buf) == from) { snprintf(v, sizeof(v), "%lld", to); db_meta_set(db, "last_game", v); }
}

/* returns 1 if the library changed */
static int assign(sqlite3 *db, long long fid, long long gid, const char *sys, const char *key, const char *path)
{
	if (!key[0]) return 0;
	sqlite3_stmt *st;
	char gkey[600] = "";
	if (sqlite3_prepare_v2(db, "SELECT key FROM games WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(st, 1, gid);
		if (sqlite3_step(st) == SQLITE_ROW) snprintf(gkey, sizeof(gkey), "%s", sqlite3_column_text(st, 0));
		sqlite3_finalize(st);
	}
	if (!strcmp(gkey, key)) return 0;
	long long other = one_ll(db, "SELECT id FROM games WHERE system_id=? AND key=?", -1, sys, key);
	long long nfiles = one_ll(db, "SELECT COUNT(*) FROM files WHERE game_id=?", gid, NULL, NULL);
	int by_name = !strncmp(gkey, "n:", 2);

	if (by_name && nfiles <= 1) {
		if (other >= 0) merge_games(db, gid, other);
		else if (sqlite3_prepare_v2(db, "UPDATE games SET key=? WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
			sqlite3_bind_int64(st, 2, gid);
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
		return 1;
	}
	if (by_name && other < 0) {
		/* several files with this name: the first one identified names the game */
		if (sqlite3_prepare_v2(db, "UPDATE games SET key=? WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
			sqlite3_bind_int64(st, 2, gid);
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
		return 1;
	}
	if (other < 0) {
		/* this file is a different game from the others with its name (another version, a hack) */
		char title[512];
		const char *b = strrchr(path, '/');
		snprintf(title, sizeof(title), "%s", b ? b + 1 : path);
		char *dot = strrchr(title, '.');
		if (dot) *dot = 0;
		if (sqlite3_prepare_v2(db, "INSERT INTO games(system_id, key, title, added) VALUES(?,?,?,?)", -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_text(st, 1, sys, -1, SQLITE_STATIC);
			sqlite3_bind_text(st, 2, key, -1, SQLITE_STATIC);
			sqlite3_bind_text(st, 3, title, -1, SQLITE_STATIC);
			sqlite3_bind_int64(st, 4, (sqlite3_int64)time(NULL));
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
		other = sqlite3_last_insert_rowid(db);
	}
	exec2(db, "UPDATE files SET game_id=?2 WHERE id=?1", fid, other);
	return 1;
}

int ident_pending(sqlite3 *db, int (*should_stop)(void))
{
	long long last = 0;
	int changed = 0, done = 0;
	stop_cb = should_stop;
	time_t t0 = time(NULL);
	sqlite3_stmt *next, *save;
	if (sqlite3_prepare_v2(db,
	        "SELECT f.id, f.path, f.game_id, g.system_id FROM files f JOIN games g ON g.id=f.game_id "
	        "WHERE f.ident=0 AND f.online=1 AND f.id>? ORDER BY f.id LIMIT 1", -1, &next, NULL) != SQLITE_OK) {
		plog("ident: %s", sqlite3_errmsg(db));
		return 0;
	}
	sqlite3_prepare_v2(db, "UPDATE files SET crc32=NULLIF(?,''), serial=NULLIF(?,''), ident=1 WHERE id=?", -1, &save, NULL);
	{
		sqlite3_stmt *c;
		int tot = 0;
		if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM files WHERE ident=0 AND online=1", -1, &c, NULL) == SQLITE_OK) {
			if (sqlite3_step(c) == SQLITE_ROW) tot = sqlite3_column_int(c, 0);
			sqlite3_finalize(c);
		}
		pthread_mutex_lock(&prog_lk);
		prog_done = 0;
		prog_total = tot;
		prog_sys[0] = prog_file[0] = 0;
		pthread_mutex_unlock(&prog_lk);
	}
	for (;;) {
		if (should_stop && should_stop()) break;
		sqlite3_reset(next);
		sqlite3_bind_int64(next, 1, last);
		if (sqlite3_step(next) != SQLITE_ROW) break;
		long long fid = sqlite3_column_int64(next, 0), gid = sqlite3_column_int64(next, 2);
		char path[1024], sysid[32];
		snprintf(path, sizeof(path), "%s", sqlite3_column_text(next, 1));
		snprintf(sysid, sizeof(sysid), "%s", sqlite3_column_text(next, 3));
		sqlite3_reset(next);
		last = fid;
		const gsys_t *sys = system_by_id(sysid);
		if (!sys) continue;
		{
			const char *b = strrchr(path, '/');
			pthread_mutex_lock(&prog_lk);
			snprintf(prog_sys, sizeof(prog_sys), "%s", sys->name);
			snprintf(prog_file, sizeof(prog_file), "%s", b ? b + 1 : path);
			pthread_mutex_unlock(&prog_lk);
		}
		ident_t id;
		if (ident_file(sys, path, &id)) continue;   /* unreadable now: tried again next time */
		db_exec(db, "BEGIN");
		sqlite3_reset(save);
		sqlite3_bind_text(save, 1, id.crc32, -1, SQLITE_STATIC);
		sqlite3_bind_text(save, 2, id.serial, -1, SQLITE_STATIC);
		sqlite3_bind_int64(save, 3, fid);
		int src = sqlite3_step(save);
		sqlite3_reset(save);
		if (src != SQLITE_DONE) { db_exec(db, "ROLLBACK"); last = fid - 1; break; }   /* busy: next round */
		changed += assign(db, fid, gid, sysid, id.key, path);
		if (db_exec(db, "COMMIT") != SQLITE_OK) { db_exec(db, "ROLLBACK"); last = fid - 1; break; }
		done++;
		pthread_mutex_lock(&prog_lk);
		prog_done = done;
		pthread_mutex_unlock(&prog_lk);
	}
	sqlite3_finalize(next);
	sqlite3_finalize(save);
	stop_cb = NULL;
	if (done) plog("identified %d file(s), %d game(s) changed, in %lds", done, changed, (long)(time(NULL) - t0));
	return changed;
}

/* ---------- titles ---------- */

static const char *NOISE[] = {
	"hack", "hacks", "hacked", "patch", "patched", "mod", "modded", "translation", "translated", "trans",
	"english", "eng", "en", "fix", "fixed", "fixes", "edition", "improvement", "improved", "restoration",
	"redux", "beta", "proto", "prototype", "demo", "by", "unl", "hb", "rev", "version", "romhack", "kaizo", NULL
};

void title_norm(const char *title, char *out, int n)
{
	char tmp[512];
	int j = 0, depth = 0;
	for (const char *p = title; *p && j + 1 < (int)sizeof(tmp); p++) {
		if (*p == '(' || *p == '[' || *p == '{') { depth++; continue; }
		if (*p == ')' || *p == ']' || *p == '}') { if (depth) depth--; continue; }
		if (depth) continue;
		tmp[j++] = isalnum((unsigned char)*p) ? (char)tolower((unsigned char)*p) : ' ';
	}
	tmp[j] = 0;
	out[0] = 0;
	int o = 0;
	char *save = NULL;
	for (char *w = strtok_r(tmp, " ", &save); w; w = strtok_r(NULL, " ", &save)) {
		int skip = 0;
		for (int i = 0; NOISE[i]; i++) if (!strcmp(w, NOISE[i])) { skip = 1; break; }
		if (w[0] == 'v' && isdigit((unsigned char)w[1])) skip = 1;           /* v1, v12 */
		if (skip) continue;
		o += snprintf(out + o, (size_t)(n - o > 0 ? n - o : 0), "%s%s", o ? " " : "", w);
		if (o >= n - 1) break;
	}
}

static int weak_word(const char *w)
{
	static const char *W[] = { "the", "of", "a", "an", "and", "to", "in", "no", "de", "la", "le", "vs", NULL };
	if (isdigit((unsigned char)w[0])) return 1;
	if (strspn(w, "ivxl") == strlen(w)) return 1;            /* roman numerals */
	for (int i = 0; W[i]; i++) if (!strcmp(w, W[i])) return 1;
	return 0;
}

/* two words of 6 letters or more with one letter changed, added or left out ("nankoku" / "nangoku"):
 * a translated title spelt the other way (#142) */
static int one_letter(const char *a, const char *b)
{
	size_t la = strlen(a), lb = strlen(b);
	if (la < 6 || lb < 6 || !strcmp(a, b)) return 0;
	if (la > lb) { const char *t = a; a = b; b = t; size_t u = la; la = lb; lb = u; }
	if (lb - la > 1) return 0;
	size_t i = 0;
	while (i < la && a[i] == b[i]) i++;
	return la == lb ? !strcmp(a + i + 1, b + i + 1) : !strcmp(a + i, b + i + 1);
}

/* the subtitle after the first " - ", as title_norm words ("" when there is none, or it's shorter than
 * two words and 10 letters: too common to tell games apart) */
void title_subtitle(const char *title, char *out, int n)
{
	out[0] = 0;
	char t[512];
	snprintf(t, sizeof(t), "%s", title);
	char *p = strchr(t, '(');                 /* tags aside */
	char *q = strchr(t, '[');
	if (q && (!p || q < p)) p = q;
	if (p) *p = 0;
	const char *d = strstr(t, " - ");
	if (!d) return;
	title_norm(d + 3, out, n);
	int words = 1, letters = 0;
	for (const char *c = out; *c; c++) { if (*c == ' ') words++; else letters++; }
	if (words < 2 || letters < 10) out[0] = 0;
}

int title_similarity(const char *a, const char *b)
{
	if (!*a || !*b) return 0;
	char x[512], y[512];
	snprintf(x, sizeof(x), "%s", a);
	snprintf(y, sizeof(y), "%s", b);
	char *ta[64], *tb[64], *s1 = NULL, *s2 = NULL;
	int na = 0, nb = 0;
	for (char *w = strtok_r(x, " ", &s1); w && na < 64; w = strtok_r(NULL, " ", &s1)) ta[na++] = w;
	for (char *w = strtok_r(y, " ", &s2); w && nb < 64; w = strtok_r(NULL, " ", &s2)) tb[nb++] = w;
	if (!na || !nb) return 0;
	/* numbers and little words alone don't make two games alike: "Dragon Warrior VII" and "Final
	   Fantasy VII" share only "vii" */
	int used[64] = { 0 }, common = 0, strong = 0;
	for (int i = 0; i < na; i++)
		for (int k = 0; k < nb; k++)
			if (!used[k] && (!strcmp(ta[i], tb[k]) || one_letter(ta[i], tb[k]))) { used[k] = 1; common++; if (!weak_word(ta[i])) strong++; break; }
	if (!strong) return 0;
	int score = 2000 * common / (na + nb);
	/* "super mario world" vs "super mario world return to dinosaur land": a hack named after its base */
	size_t la = strlen(a), lb = strlen(b);
	const char *s = la <= lb ? a : b, *l = la <= lb ? b : a;
	size_t ls = la <= lb ? la : lb, ll = la <= lb ? lb : la;
	if (!strncmp(s, l, ls) && (l[ls] == ' ' || l[ls] == 0)) {
		int p = 850 + (int)(150 * ls / ll);
		if (p > score) score = p;
	}
	return score > 1000 ? 1000 : score;
}
