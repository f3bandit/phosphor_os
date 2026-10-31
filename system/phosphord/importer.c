/* phosphord: one-time import of artwork and details other tools left on the card.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Only fills what's empty, never overwrites what Phosphor already has, and never changes the
 * source files. Art is copied into Phosphor's store (scaled down), so the originals can go. */
#include "importer.h"
#include "art.h"
#include "db.h"
#include "log.h"
#include "paths.h"
#include "systems.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- a small reader for gamelist.xml ---------- */

static void unescape(char *s)
{
	char *o = s;
	while (*s) {
		if (*s == '&') {
			if (!strncmp(s, "&amp;", 5)) { *o++ = '&'; s += 5; continue; }
			if (!strncmp(s, "&lt;", 4)) { *o++ = '<'; s += 4; continue; }
			if (!strncmp(s, "&gt;", 4)) { *o++ = '>'; s += 4; continue; }
			if (!strncmp(s, "&quot;", 6)) { *o++ = '"'; s += 6; continue; }
			if (!strncmp(s, "&apos;", 6)) { *o++ = '\''; s += 6; continue; }
			if (s[1] == '#') {
				int hex = s[2] == 'x' || s[2] == 'X';
				char *end;
				long c = strtol(s + (hex ? 3 : 2), &end, hex ? 16 : 10);
				if (*end == ';' && c > 0) {
					if (c < 0x80) *o++ = (char)c;
					else if (c < 0x800) { *o++ = (char)(0xC0 | (c >> 6)); *o++ = (char)(0x80 | (c & 0x3F)); }
					else { *o++ = (char)(0xE0 | (c >> 12)); *o++ = (char)(0x80 | ((c >> 6) & 0x3F)); *o++ = (char)(0x80 | (c & 0x3F)); }
					s = end + 1;
					continue;
				}
			}
		}
		*o++ = *s++;
	}
	*o = 0;
}

char *gl_tag_text(const char *from, const char *to, const char *tag)
{
	char open[32], close[32];
	snprintf(open, sizeof(open), "<%s>", tag);
	snprintf(close, sizeof(close), "</%s>", tag);
	const char *a = strstr(from, open);
	if (!a || a >= to) return NULL;
	a += strlen(open);
	const char *b = strstr(a, close);
	if (!b || b > to) return NULL;
	char *t = strndup(a, (size_t)(b - a));
	unescape(t);
	char *s = t;
	while (isspace((unsigned char)*s)) s++;
	char *e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
	if (s != t) memmove(t, s, strlen(s) + 1);
	if (!*t) { free(t); return NULL; }
	return t;
}

void gl_resolve(const char *dir, const char *rel, char *out, size_t n)
{
	if (rel[0] == '/') snprintf(out, n, "%s", rel);
	else snprintf(out, n, "%s/%s", dir, rel[0] == '.' && rel[1] == '/' ? rel + 2 : rel);
}

/* ---------- applying what was found ---------- */

typedef struct {
	sqlite3 *db;
	sqlite3_stmt *by_path, *fill, *set_art;
	import_result_t r;
} imp_t;

/* the game a file belongs to: id, key, whether it already has art / a description */
static int game_of(imp_t *m, const char *path, long long *id, char *key, size_t kn, int *has_art, int *has_desc)
{
	sqlite3_reset(m->by_path);
	sqlite3_bind_text(m->by_path, 1, path, -1, SQLITE_STATIC);
	if (sqlite3_step(m->by_path) != SQLITE_ROW) return 0;
	*id = sqlite3_column_int64(m->by_path, 0);
	snprintf(key, kn, "%s", sqlite3_column_text(m->by_path, 1));
	*has_art = sqlite3_column_type(m->by_path, 2) != SQLITE_NULL;
	*has_desc = sqlite3_column_type(m->by_path, 3) != SQLITE_NULL;
	return 1;
}

static void bind_or_null(sqlite3_stmt *st, int i, const char *v)
{
	if (v) sqlite3_bind_text(st, i, v, -1, SQLITE_TRANSIENT);
	else sqlite3_bind_null(st, i);
}

static void apply(imp_t *m, const char *sys, const char *game_path, const char *title, const char *desc,
                  const char *year, const char *dev, const char *pub, const char *genre, const char *players,
                  const char *image)
{
	long long id;
	char key[600];
	int has_art, has_desc;
	if (!game_of(m, game_path, &id, key, sizeof(key), &has_art, &has_desc)) return;
	m->r.games++;
	if (desc || year || dev || pub || genre || players || title) {
		sqlite3_reset(m->fill);
		bind_or_null(m->fill, 1, desc);
		bind_or_null(m->fill, 2, year);
		bind_or_null(m->fill, 3, dev);
		bind_or_null(m->fill, 4, pub);
		bind_or_null(m->fill, 5, genre);
		bind_or_null(m->fill, 6, players);
		sqlite3_bind_int64(m->fill, 7, id);
		bind_or_null(m->fill, 8, title);   /* the scraped name is nicer than the file name */
		sqlite3_step(m->fill);
		if (sqlite3_changes(m->db) > 0 && desc && !has_desc) m->r.details++;
	}
	if (image && !has_art && file_exists(image)) {
		char rel[600];
		if (art_store_box(sys, key, image, rel, sizeof(rel)) == 0) {
			sqlite3_reset(m->set_art);
			sqlite3_bind_text(m->set_art, 1, rel, -1, SQLITE_STATIC);
			sqlite3_bind_int64(m->set_art, 2, id);
			sqlite3_step(m->set_art);
			m->r.art++;
		}
	}
}

static void import_gamelist(imp_t *m, const gsys_t *s, const char *dir)
{
	char path[700];
	snprintf(path, sizeof(path), "%s/gamelist.xml", dir);
	FILE *f = fopen(path, "rb");
	if (!f) return;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n <= 0 || n > 64L * 1024 * 1024) { fclose(f); return; }
	char *buf = malloc((size_t)n + 1);
	size_t got = fread(buf, 1, (size_t)n, f);
	fclose(f);
	buf[got] = 0;
	int count = 0;
	for (char *p = buf; (p = strstr(p, "<game")) != NULL;) {
		if (p[5] != '>' && p[5] != ' ') { p += 5; continue; }
		char *end = strstr(p, "</game>");
		if (!end) break;
		char *rel = gl_tag_text(p, end, "path");
		if (rel) {
			char gp[1024], ip[1024];
			gl_resolve(dir, rel, gp, sizeof(gp));
			char *name = gl_tag_text(p, end, "name"), *desc = gl_tag_text(p, end, "desc");
			char *img = gl_tag_text(p, end, "image");
			if (!img) img = gl_tag_text(p, end, "thumbnail");
			if (img) gl_resolve(dir, img, ip, sizeof(ip));
			char *date = gl_tag_text(p, end, "releasedate");
			if (date && strlen(date) >= 4) date[4] = 0;   /* 19970131T000000 -> 1997 */
			char *dev = gl_tag_text(p, end, "developer"), *pub = gl_tag_text(p, end, "publisher");
			char *genre = gl_tag_text(p, end, "genre"), *players = gl_tag_text(p, end, "players");
			apply(m, s->id, gp, name, desc, date, dev, pub, genre, players, img ? ip : NULL);
			free(rel); free(name); free(desc); free(img); free(date); free(dev); free(pub); free(genre); free(players);
			count++;
		}
		p = end + 7;
	}
	free(buf);
	plog("import: %s (%d entries)", path, count);
}

/* media/<title>.png|jpg and .txt next to the games, or in a parent folder up to the system folder */
static void import_media(imp_t *m, const gsys_t *s)
{
	sqlite3_stmt *st;
	if (sqlite3_prepare_v2(m->db, "SELECT f.path FROM files f JOIN games g ON g.id=f.game_id WHERE g.system_id=? AND f.online=1",
	                       -1, &st, NULL) != SQLITE_OK) return;
	sqlite3_bind_text(st, 1, s->id, -1, SQLITE_STATIC);
	while (sqlite3_step(st) == SQLITE_ROW) {
		const char *path = (const char *)sqlite3_column_text(st, 0);
		char dir[1024], title[512], img[1600] = "", txt[1600] = "";
		snprintf(dir, sizeof(dir), "%s", path);
		char *slash = strrchr(dir, '/');
		if (!slash) continue;
		snprintf(title, sizeof(title), "%s", slash + 1);
		char *dot = strrchr(title, '.');
		if (dot) *dot = 0;
		*slash = 0;
		for (int up = 0; up < 3 && (!img[0] || !txt[0]); up++) {
			static const char *const ex[] = { "png", "jpg", "jpeg", "PNG", "JPG" };
			for (size_t i = 0; i < 5 && !img[0]; i++) {
				char p[1600];
				snprintf(p, sizeof(p), "%s/media/%s.%s", dir, title, ex[i]);
				if (file_exists(p)) snprintf(img, sizeof(img), "%s", p);
			}
			if (!txt[0]) {
				char p[1600];
				snprintf(p, sizeof(p), "%s/media/%s.txt", dir, title);
				if (file_exists(p)) snprintf(txt, sizeof(txt), "%s", p);
			}
			char *s2 = strrchr(dir, '/');
			if (!s2 || !strcmp(s2, "/games")) break;
			*s2 = 0;
		}
		char *desc = NULL;
		if (txt[0]) {
			FILE *f = fopen(txt, "rb");
			if (f) {
				desc = calloc(1, 8192);
				size_t r = fread(desc, 1, 8191, f);
				desc[r] = 0;
				fclose(f);
			}
		}
		if (img[0] || desc) apply(m, s->id, path, NULL, desc, NULL, NULL, NULL, NULL, NULL, img[0] ? img : NULL);
		free(desc);
	}
	sqlite3_finalize(st);
}

import_result_t import_existing(sqlite3 *db)
{
	imp_t m = { .db = db };
	sqlite3_prepare_v2(db, "SELECT g.id, g.key, g.art_box, g.description FROM files f JOIN games g ON g.id=f.game_id WHERE f.path=?",
	                   -1, &m.by_path, NULL);
	sqlite3_prepare_v2(db, "UPDATE games SET title=COALESCE(?8,title), description=COALESCE(description,?1), year=COALESCE(year,?2), "
	                       "developer=COALESCE(developer,?3), publisher=COALESCE(publisher,?4), genre=COALESCE(genre,?5), "
	                       "players=COALESCE(players,?6) WHERE id=?7", -1, &m.fill, NULL);
	sqlite3_prepare_v2(db, "UPDATE games SET art_box=? WHERE id=?", -1, &m.set_art, NULL);
	for (int i = 0; i < NSYS; i++) {
		char dirs[16][512];
		int nd = system_dirs(&SYS[i], dirs, 16);
		db_exec(db, "BEGIN");
		for (int d = 0; d < nd; d++) import_gamelist(&m, &SYS[i], dirs[d]);
		import_media(&m, &SYS[i]);
		db_exec(db, "COMMIT");
	}
	sqlite3_finalize(m.by_path);
	sqlite3_finalize(m.fill);
	sqlite3_finalize(m.set_art);
	plog("import done: %d games matched, %d descriptions, %d box art", m.r.games, m.r.details, m.r.art);
	return m.r;
}
