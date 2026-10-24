/* phosphord scraper source: ArcadeDB (adb.arcadeitalia.net), no account.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Arcade games, by the MAME set name in the .mra (e.g. sf2). Gives the full title, maker, year,
 * genre, players, the game's history text (from arcade-history.com) and the flyer as box art. */
#include "scrape_src.h"
#include "cJSON.h"
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* "Arcade Video game published 35 years ago:\r\n\r\nStreet Fighter II ... - TECHNICAL - ..." -> the story part */
static char *history_text(const char *h)
{
	if (!h || !*h) return NULL;
	const char *s = h;
	if (!strncmp(s, "Arcade Video game published", 27)) { const char *nl = strstr(s, "\n"); if (nl) s = nl + 1; }
	char *out = strdup(s), *w = out;
	for (char *p = out; *p; p++) if (*p != '\r') *w++ = *p;
	*w = 0;
	static const char *cut[] = { "- TECHNICAL -", "- TRIVIA -", "- CONTRIBUTE -", "- UPDATES -", "- SERIES -", "- STAFF -", NULL };
	for (int i = 0; cut[i]; i++) { char *c = strstr(out, cut[i]); if (c) *c = 0; }
	size_t n = strlen(out);
	while (n && (out[n - 1] == '\n' || out[n - 1] == ' ')) out[--n] = 0;
	char *b = out;
	while (*b == '\n' || *b == ' ') b++;
	if (!*b) { free(out); return NULL; }
	memmove(out, b, strlen(b) + 1);
	return out;
}

/* one set: its JSON entry (caller frees the root in *root), NULL not known; *rc -1 network trouble */
static cJSON *adb_query(const char *set, cJSON **root, int *rc)
{
	char esc[200], url[400];
	net_escape(set, esc, sizeof(esc));
	snprintf(url, sizeof(url), "https://adb.arcadeitalia.net/service_scraper.php?ajax=query_mame&lang=en&game_name=%s", esc);
	char *body = NULL;
	*root = NULL;
	*rc = net_request("GET", url, NULL, NULL, &body, NULL, NULL);
	usleep(250000);                      /* be gentle: one arcade game at a time, ~4 a second at most */
	if (*rc) { free(body); return NULL; }
	*root = cJSON_Parse(body);
	free(body);
	cJSON *x = cJSON_GetArrayItem(*root ? cJSON_GetObjectItem(*root, "result") : NULL, 0);
	return cJSON_IsObject(x) ? x : NULL;
}

static void adb_fill(cJSON *x, sres_t *r)
{
	const char *maker = cJSON_GetStringValue(cJSON_GetObjectItem(x, "manufacturer"));
	sres_set(r, F_DEV, maker);
	sres_set(r, F_PUB, maker);
	sres_set(r, F_YEAR, cJSON_GetStringValue(cJSON_GetObjectItem(x, "year")));
	sres_set(r, F_GENRE, cJSON_GetStringValue(cJSON_GetObjectItem(x, "genre")));
	cJSON *pl = cJSON_GetObjectItem(x, "players");
	if (cJSON_IsNumber(pl) && pl->valueint > 0) { char b[8]; snprintf(b, sizeof(b), "%d", pl->valueint); sres_set(r, F_PLAYERS, b); }
	char *h = history_text(cJSON_GetStringValue(cJSON_GetObjectItem(x, "history")));
	if (h) { sres_set(r, F_DESC, h); free(h); }
}

/* a hack or homebrew name: never looked up by its title */
static int hack_name(const char *t)
{
	return strcasestr(t, "hbmame") || strcasestr(t, "hack") || strcasestr(t, "[h]") || strcasestr(t, "trainer");
}

static int adb_lookup(sqlite3 *db, sgame_t *g, sres_t *r)
{
	(void)db;
	if (!g->arcade || !g->serial[0]) return 0;
	cJSON *j;
	int rc;
	cJSON *x = adb_query(g->serial, &j, &rc);
	if (rc < 0) { cJSON_Delete(j); return -1; }
	if (!x) {
		/* not a MAME set (MiSTer-only, homebrew): the .mra name is the game's own name, for the sources
		   that look up by name */
		if (!g->official[0] && !hack_name(g->title)) snprintf(g->official, sizeof(g->official), "%s", g->title);
		cJSON_Delete(j);
		return 0;
	}
	const char *t = cJSON_GetStringValue(cJSON_GetObjectItem(x, "title"));
	if (t && !g->official[0]) snprintf(g->official, sizeof(g->official), "%s", t);
	adb_fill(x, r);
	const char *flyer = cJSON_GetStringValue(cJSON_GetObjectItem(x, "url_image_flyer"));
	const char *parent = cJSON_GetStringValue(cJSON_GetObjectItem(x, "cloneof"));
	if ((!flyer || !*flyer) && parent && *parent) {
		/* a clone (another region or revision) mostly has no flyer of its own: the parent's is the game's */
		cJSON *pj;
		int prc;
		cJSON *p = adb_query(parent, &pj, &prc);
		if (prc < 0) { cJSON_Delete(pj); cJSON_Delete(j); return -1; }
		if (p) {
			adb_fill(p, r);
			sres_art(r, "arcadedb", cJSON_GetStringValue(cJSON_GetObjectItem(p, "url_image_flyer")));
			sres_art(r, "arcadedb", cJSON_GetStringValue(cJSON_GetObjectItem(x, "url_image_title")));
			sres_art(r, "arcadedb", cJSON_GetStringValue(cJSON_GetObjectItem(p, "url_image_title")));
			cJSON_Delete(pj);
			cJSON_Delete(j);
			return 1;
		}
		cJSON_Delete(pj);
	}
	sres_art(r, "arcadedb", flyer);
	sres_art(r, "arcadedb", cJSON_GetStringValue(cJSON_GetObjectItem(x, "url_image_title")));
	cJSON_Delete(j);
	return 1;
}

static int adb_ready(void) { return 1; }

const source_t SRC_ARCADEDB = { "arcadedb", "ArcadeDB", NULL, adb_ready, adb_lookup };
