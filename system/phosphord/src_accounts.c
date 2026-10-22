/* phosphord scraper sources that need an account or key: TheGamesDB, ScreenScraper, IGDB.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Keys go in config/scraper.ini; a source without its key is skipped. ScreenScraper is asked by
 * content (CRC32 / product code); TheGamesDB and IGDB by the official name a content match gave. */
#include "scrape_src.h"
#include "cJSON.h"
#include "ident.h"
#include "log.h"
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

typedef struct { const char *sys; int tgdb, ss, igdb[2]; } plat_t;
/* platform numbers in each service */
static const plat_t P[] = {
	{ "PSX", 10, 57, { 7 } },
	{ "Saturn", 17, 22, { 32 } },
	{ "MegaCD", 21, 20, { 78 } },
	{ "TGFX16CD", 4955, 114, { 150 } },
	{ "NES", 7, 3, { 18, 51 } },
	{ "SNES", 6, 4, { 19 } },
	{ "Genesis", 18, 1, { 29 } },
	{ "N64", 3, 14, { 4 } },
	{ "SMS", 35, 2, { 64, 84 } },
	{ "TGFX16", 34, 31, { 86 } },
	{ "ATARI7800", 27, 41, { 60, 59 } },
	{ "GAMEBOY", 4, 9, { 33, 22 } },
	{ "GBA", 5, 12, { 24 } },
	{ "GameGear", 20, 21, { 35 } },
	{ "WonderSwan", 4925, 45, { 57, 123 } },
	{ "AtariLynx", 4924, 28, { 61 } },
	{ "Arcade", 23, 75, { 52 } },
	{ "C64", 40, 66, { 15 } },
	{ "MSX", 4929, 113, { 27 } },
	{ "Atari800", 4943, 43, { 65 } },
};

static const plat_t *plat_of(const char *sys)
{
	for (size_t i = 0; i < sizeof(P) / sizeof(P[0]); i++) if (!strcmp(P[i].sys, sys)) return &P[i];
	return NULL;
}

static const char *str(cJSON *o, const char *k) { return cJSON_GetStringValue(cJSON_GetObjectItem(o, k)); }

/* ---------- TheGamesDB (api.thegamesdb.net, free API key) ---------- */

static int tgdb_ready(void) { return *scfg("thegamesdb", "apikey") != 0; }

static int tgdb_lookup(sqlite3 *db, sgame_t *g, sres_t *r)
{
	(void)db;
	const plat_t *p = plat_of(g->sys);
	if (!p || !g->official[0]) return 0;
	char base[512], name[700], key[200], url[1600];
	name_base(g->official, base, sizeof(base));
	net_escape(base, name, sizeof(name));
	net_escape(scfg("thegamesdb", "apikey"), key, sizeof(key));
	snprintf(url, sizeof(url), "https://api.thegamesdb.net/v1.1/Games/ByGameName?apikey=%s&name=%s"
	         "&fields=players,overview&filter%%5Bplatform%%5D=%d&include=boxart", key, name, p->tgdb);
	char *body = NULL;
	int rc = net_request("GET", url, NULL, NULL, &body, NULL, NULL);
	if (rc == 2) { plog("TheGamesDB refused the API key"); free(body); return 0; }
	if (rc) { free(body); return rc < 0 ? -1 : 0; }
	cJSON *j = cJSON_Parse(body);
	free(body);
	cJSON *games = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "data"), "games");
	cJSON *best = NULL, *x;
	char want[600], have[600];
	title_norm(base, want, sizeof(want));
	cJSON_ArrayForEach(x, games) {                 /* the exact title, not just the first hit */
		title_norm(str(x, "game_title") ? str(x, "game_title") : "", have, sizeof(have));
		if (!strcmp(want, have)) { best = x; break; }
	}
	if (!best) { cJSON_Delete(j); return 0; }
	sres_set(r, F_DESC, str(best, "overview"));
	const char *rd = str(best, "release_date");
	if (rd && strlen(rd) >= 4) { char y[5]; memcpy(y, rd, 4); y[4] = 0; sres_set(r, F_YEAR, y); }
	cJSON *pl = cJSON_GetObjectItem(best, "players");
	if (cJSON_IsNumber(pl) && pl->valueint > 0) { char b[8]; snprintf(b, sizeof(b), "%d", pl->valueint); sres_set(r, F_PLAYERS, b); }
	/* include.boxart: base_url.original + data[<id>][] {type, side, filename} */
	cJSON *bx = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "include"), "boxart");
	const char *baseurl = str(cJSON_GetObjectItem(bx, "base_url"), "original");
	char idk[32];
	cJSON *idj = cJSON_GetObjectItem(best, "id");
	snprintf(idk, sizeof(idk), "%d", cJSON_IsNumber(idj) ? idj->valueint : -1);
	cJSON *imgs = cJSON_GetObjectItem(cJSON_GetObjectItem(bx, "data"), idk);
	cJSON_ArrayForEach(x, imgs) {
		const char *side = str(x, "side"), *fn = str(x, "filename");
		if (baseurl && fn && side && !strcmp(side, "front")) {
			char u[800];
			snprintf(u, sizeof(u), "%s%s", baseurl, fn);
			sres_art(r, "thegamesdb", u);
			break;
		}
	}
	cJSON_Delete(j);
	return 1;
}

const source_t SRC_TGDB = { "thegamesdb", "TheGamesDB", "[thegamesdb] apikey", tgdb_ready, tgdb_lookup };

/* ---------- ScreenScraper (screenscraper.fr: Phosphor's developer id + optional user login) ---------- */

static int ss_ready(void) { return *scfg("screenscraper", "devid") && *scfg("screenscraper", "devpassword"); }

/* the English text from a [{langue, text}] list, else the first */
static const char *lang_text(cJSON *arr, const char *field)
{
	cJSON *x, *first = NULL;
	cJSON_ArrayForEach(x, arr) {
		if (!first) first = x;
		const char *l = str(x, "langue");
		if (l && !strcmp(l, "en")) return str(x, field);
	}
	return first ? str(first, field) : NULL;
}

static int ss_lookup(sqlite3 *db, sgame_t *g, sres_t *r)
{
	(void)db;
	const plat_t *p = plat_of(g->sys);
	if (!p) return 0;
	if (!g->crc[0] && !g->serial[0]) return 0;
	char a[200], b[200], c[200], d[200], nm[700], ser[120], url[2000];
	net_escape(scfg("screenscraper", "devid"), a, sizeof(a));
	net_escape(scfg("screenscraper", "devpassword"), b, sizeof(b));
	net_escape(scfg("screenscraper", "user"), c, sizeof(c));
	net_escape(scfg("screenscraper", "password"), d, sizeof(d));
	net_escape(g->arcade ? g->serial : g->title, nm, sizeof(nm));
	net_escape(g->serial, ser, sizeof(ser));
	int n = snprintf(url, sizeof(url), "https://api.screenscraper.fr/api2/jeuInfos.php?devid=%s&devpassword=%s&softname=Phosphor"
	                 "&output=json&ssid=%s&sspassword=%s&systemeid=%d&romtype=rom&romnom=%s", a, b, c, d, p->ss, nm);
	if (g->crc[0]) n += snprintf(url + n, sizeof(url) - (size_t)n, "&crc=%s", g->crc);
	if (g->disc && g->serial[0]) n += snprintf(url + n, sizeof(url) - (size_t)n, "&serialnum=%s", ser);
	if (g->size > 0) snprintf(url + n, sizeof(url) - (size_t)n, "&romtaille=%ld", g->size);
	char *body = NULL;
	int http = 0;
	int rc = net_request("GET", url, NULL, NULL, &body, NULL, &http);
	if (rc == 2) { plog("ScreenScraper refused the login (HTTP %d)", http); free(body); return 0; }
	if (http == 430 || http == 429) { free(body); return -1; }      /* over the daily/thread limit: later */
	if (rc) { free(body); return rc < 0 ? -1 : 0; }
	cJSON *j = cJSON_Parse(body);
	free(body);
	cJSON *jeu = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "response"), "jeu");
	if (!cJSON_IsObject(jeu)) { cJSON_Delete(j); return 0; }
	if (!g->official[0]) {
		const char *t = lang_text(cJSON_GetObjectItem(jeu, "noms"), "text");
		if (t) snprintf(g->official, sizeof(g->official), "%s", t);
	}
	sres_set(r, F_DESC, lang_text(cJSON_GetObjectItem(jeu, "synopsis"), "text"));
	sres_set(r, F_DEV, str(cJSON_GetObjectItem(jeu, "developpeur"), "text"));
	sres_set(r, F_PUB, str(cJSON_GetObjectItem(jeu, "editeur"), "text"));
	sres_set(r, F_PLAYERS, str(cJSON_GetObjectItem(jeu, "joueurs"), "text"));
	cJSON *g0 = cJSON_GetArrayItem(cJSON_GetObjectItem(jeu, "genres"), 0);
	sres_set(r, F_GENRE, lang_text(cJSON_GetObjectItem(g0, "noms"), "text"));
	const char *date = str(cJSON_GetArrayItem(cJSON_GetObjectItem(jeu, "dates"), 0), "text");
	if (date && strlen(date) >= 4) { char y[5]; memcpy(y, date, 4); y[4] = 0; sres_set(r, F_YEAR, y); }
	/* front box: the game's region first */
	static const char *REG[] = { "us", "wor", "eu", "ss", "jp", NULL };
	cJSON *media = cJSON_GetObjectItem(jeu, "medias"), *x;
	for (int ri = 0; REG[ri]; ri++)
		cJSON_ArrayForEach(x, media) {
			const char *t = str(x, "type"), *rg = str(x, "region"), *u = str(x, "url");
			if (t && u && !strcmp(t, "box-2D") && rg && !strcmp(rg, REG[ri])) { sres_art(r, "screenscraper", u); goto art_done; }
		}
art_done:
	cJSON_Delete(j);
	return 1;
}

const source_t SRC_SCREENSCRAPER = { "screenscraper", "ScreenScraper", "[screenscraper] devid, devpassword (+ your user, password)", ss_ready, ss_lookup };

/* ---------- IGDB (api.igdb.com via a Twitch developer app) ---------- */

static char igdb_token[200];
static time_t igdb_expires;

static int igdb_ready(void) { return *scfg("igdb", "client_id") && *scfg("igdb", "client_secret"); }

static int igdb_auth(void)
{
	if (igdb_token[0] && time(NULL) < igdb_expires - 60) return 0;
	char a[200], b[200], url[600];
	net_escape(scfg("igdb", "client_id"), a, sizeof(a));
	net_escape(scfg("igdb", "client_secret"), b, sizeof(b));
	snprintf(url, sizeof(url), "https://id.twitch.tv/oauth2/token?client_id=%s&client_secret=%s&grant_type=client_credentials", a, b);
	char *body = NULL;
	int rc = net_request("POST", url, NULL, "", &body, NULL, NULL);
	if (rc) { free(body); if (rc == 2 || rc == 1) plog("IGDB: Twitch refused the client id/secret"); return rc < 0 ? -1 : 1; }
	cJSON *j = cJSON_Parse(body);
	free(body);
	const char *t = str(j, "access_token");
	cJSON *ex = cJSON_GetObjectItem(j, "expires_in");
	if (t) {
		snprintf(igdb_token, sizeof(igdb_token), "%s", t);
		igdb_expires = time(NULL) + (cJSON_IsNumber(ex) ? (time_t)ex->valuedouble : 3600);
	}
	cJSON_Delete(j);
	return t ? 0 : 1;
}

static int igdb_lookup(sqlite3 *db, sgame_t *g, sres_t *r)
{
	(void)db;
	const plat_t *p = plat_of(g->sys);
	if (!p || !g->official[0]) return 0;
	int a = igdb_auth();
	if (a) return a < 0 ? -1 : 0;
	char base[512], q[1500], h1[300], h2[300];
	name_base(g->official, base, sizeof(base));
	for (char *c = base; *c; c++) if (*c == '"') *c = '\'';
	char plats[32];
	if (p->igdb[1]) snprintf(plats, sizeof(plats), "%d,%d", p->igdb[0], p->igdb[1]);
	else snprintf(plats, sizeof(plats), "%d", p->igdb[0]);
	snprintf(q, sizeof(q), "search \"%s\"; fields name,summary,first_release_date,genres.name,"
	         "involved_companies.company.name,involved_companies.developer,involved_companies.publisher,cover.image_id;"
	         " where platforms = (%s); limit 10;", base, plats);
	snprintf(h1, sizeof(h1), "Client-ID: %s", scfg("igdb", "client_id"));
	snprintf(h2, sizeof(h2), "Authorization: Bearer %s", igdb_token);
	const char *hdr[] = { h1, h2, "Accept: application/json", NULL };
	char *body = NULL;
	int rc = net_request("POST", "https://api.igdb.com/v4/games", hdr, q, &body, NULL, NULL);
	usleep(260000);                                   /* IGDB allows 4 requests a second */
	if (rc == 2) { igdb_token[0] = 0; free(body); return 0; }
	if (rc) { free(body); return rc < 0 ? -1 : 0; }
	cJSON *j = cJSON_Parse(body);
	free(body);
	cJSON *best = NULL, *x;
	char want[600], have[600];
	title_norm(base, want, sizeof(want));
	cJSON_ArrayForEach(x, j) {
		title_norm(str(x, "name") ? str(x, "name") : "", have, sizeof(have));
		if (!strcmp(want, have)) { best = x; break; }
	}
	if (!best) { cJSON_Delete(j); return 0; }
	sres_set(r, F_DESC, str(best, "summary"));
	cJSON *t = cJSON_GetObjectItem(best, "first_release_date");
	if (cJSON_IsNumber(t)) {
		time_t ts = (time_t)t->valuedouble;
		struct tm tm;
		gmtime_r(&ts, &tm);
		char y[8];
		snprintf(y, sizeof(y), "%d", tm.tm_year + 1900);
		sres_set(r, F_YEAR, y);
	}
	sres_set(r, F_GENRE, str(cJSON_GetArrayItem(cJSON_GetObjectItem(best, "genres"), 0), "name"));
	cJSON_ArrayForEach(x, cJSON_GetObjectItem(best, "involved_companies")) {
		const char *co = str(cJSON_GetObjectItem(x, "company"), "name");
		if (cJSON_IsTrue(cJSON_GetObjectItem(x, "developer"))) sres_set(r, F_DEV, co);
		if (cJSON_IsTrue(cJSON_GetObjectItem(x, "publisher"))) sres_set(r, F_PUB, co);
	}
	const char *img = str(cJSON_GetObjectItem(best, "cover"), "image_id");
	if (img) {
		char u[300];
		snprintf(u, sizeof(u), "https://images.igdb.com/igdb/image/upload/t_cover_big/%s.jpg", img);
		sres_art(r, "igdb", u);
	}
	cJSON_Delete(j);
	return 1;
}

const source_t SRC_IGDB = { "igdb", "IGDB", "[igdb] client_id, client_secret", igdb_ready, igdb_lookup };
