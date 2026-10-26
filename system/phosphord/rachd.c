/* phosphord: .chd discs for rcheevos' hashing (matching the library to RetroAchievements).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * rcheevos' own CD reader knows .cue/.bin/.gdi/.iso but not .chd. This one opens .chd discs with libchdr
 * and hands every other file to rcheevos' reader. It answers as rcheevos' does: sectors are numbered on
 * the whole disc (sector 0 is where track 1 starts), a data sector gives its 2048 bytes and an audio one
 * all 2352. Each open track has its own chd_file, so matching in a background thread is safe. */
#include "rachd.h"
#include "rc_hash.h"
#include <libchdr/chd.h>
#include <libchdr/cdrom.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static rc_hash_cdreader_t def;   /* rcheevos' own reader, for everything that isn't a .chd */

static const uint8_t SYNC[12] = { 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0 };

typedef struct {
	void *def;               /* rcheevos' own track handle (not a .chd), or NULL */
	chd_file *chd;
	uint8_t *hunk;
	uint32_t frames_per_hunk;
	int64_t cached;          /* the hunk in hunk[], or -1 */
	uint32_t chd_frame;      /* the track's first frame in the .chd */
	uint32_t frames;         /* frames kept for the track (its pregap too, when kept) */
	uint32_t pregap;         /* pregap frames kept before the track starts */
	int64_t lba;             /* the disc sector of the first kept frame */
	int off, len, audio;     /* where a sector's data is in a frame (-1: from its header) and how much */
} trk_t;

typedef struct { int num, frames, pregap, postgap, pad, has_pad; char type[32], pgtype[32]; } meta_t;

/* track i (from 0) of the disc, from the .chd's metadata; 0 when there's no such track */
static int track_meta(chd_file *f, int i, meta_t *m)
{
	char buf[256], sub[32], pgsub[32];
	uint32_t len = 0;
	memset(m, 0, sizeof(*m));
	if (chd_get_metadata(f, CDROM_TRACK_METADATA2_TAG, (uint32_t)i, buf, sizeof(buf) - 1, &len, NULL, NULL) == CHDERR_NONE) {
		buf[len < sizeof(buf) ? len : sizeof(buf) - 1] = 0;
		return sscanf(buf, "TRACK:%d TYPE:%31s SUBTYPE:%31s FRAMES:%d PREGAP:%d PGTYPE:%31s PGSUB:%31s POSTGAP:%d",
		              &m->num, m->type, sub, &m->frames, &m->pregap, m->pgtype, pgsub, &m->postgap) >= 4;
	}
	if (chd_get_metadata(f, CDROM_TRACK_METADATA_TAG, (uint32_t)i, buf, sizeof(buf) - 1, &len, NULL, NULL) == CHDERR_NONE) {
		buf[len < sizeof(buf) ? len : sizeof(buf) - 1] = 0;
		return sscanf(buf, "TRACK:%d TYPE:%31s SUBTYPE:%31s FRAMES:%d", &m->num, m->type, sub, &m->frames) == 4;
	}
	if (chd_get_metadata(f, GDROM_TRACK_METADATA_TAG, (uint32_t)i, buf, sizeof(buf) - 1, &len, NULL, NULL) == CHDERR_NONE) {
		buf[len < sizeof(buf) ? len : sizeof(buf) - 1] = 0;
		m->has_pad = 1;
		return sscanf(buf, "TRACK:%d TYPE:%31s SUBTYPE:%31s FRAMES:%d PAD:%d PREGAP:%d PGTYPE:%31s PGSUB:%31s POSTGAP:%d",
		              &m->num, m->type, sub, &m->frames, &m->pad, &m->pregap, m->pgtype, pgsub, &m->postgap) >= 4;
	}
	return 0;
}

/* where the data is in a frame of this track type */
static void layout(const char *type, trk_t *t)
{
	t->audio = 0;
	t->len = 2048;
	t->off = -1;                                                    /* raw: from each sector's header */
	if (!strcmp(type, "AUDIO")) { t->audio = 1; t->off = 0; t->len = 2352; }
	else if (!strcmp(type, "MODE1") || !strcmp(type, "MODE1/2048") || !strcmp(type, "MODE2_FORM1")) t->off = 0;
	else if (!strcmp(type, "MODE2_FORM2")) { t->off = 0; t->len = 2324; }
	else if (!strcmp(type, "MODE2") || !strcmp(type, "MODE2/2336") || !strcmp(type, "MODE2_FORM_MIX")) t->off = 8;
}

/* frame i of the track (from its first kept frame), or NULL */
static const uint8_t *frame(trk_t *t, uint32_t i)
{
	if (i >= t->frames) return NULL;
	uint64_t f = (uint64_t)t->chd_frame + i;
	int64_t h = (int64_t)(f / t->frames_per_hunk);
	if (h != t->cached) {
		if (chd_read(t->chd, (uint32_t)h, t->hunk) != CHDERR_NONE) return NULL;
		t->cached = h;
	}
	return t->hunk + (f % t->frames_per_hunk) * CD_FRAME_SIZE;
}

static int bcd(uint8_t b) { return (b >> 4) * 10 + (b & 15); }

static void close_trk(trk_t *t)
{
	if (!t) return;
	if (t->chd) chd_close(t->chd);
	free(t->hunk);
	free(t);
}

static trk_t *open_chd(const char *path, uint32_t want)
{
	chd_file *f = NULL;
	if (chd_open(path, CHD_OPEN_READ, NULL, &f) != CHDERR_NONE) return NULL;
	const chd_header *h = chd_get_header(f);
	trk_t *t = calloc(1, sizeof(*t));
	if (!t || !h || h->hunkbytes < CD_FRAME_SIZE) { free(t); chd_close(f); return NULL; }
	t->chd = f;
	t->frames_per_hunk = h->hunkbytes / CD_FRAME_SIZE;
	t->cached = -1;
	if (want == 0) want = RC_HASH_CDTRACK_LARGEST;             /* as rcheevos' own reader */

	/* walk the tracks: where each is in the .chd and on the disc, and pick the one asked for */
	meta_t m, pick;
	memset(&pick, 0, sizeof(pick));
	uint32_t chd_frame = 0, pick_frame = 0;
	int64_t pos = 0, pick_lba = 0;                             /* pos: where the next track starts on the disc */
	int found = 0;
	for (int i = 0; i < CD_MAX_TRACKS && track_meta(f, i, &m); i++) {
		if (m.frames < 0 || m.pregap < 0 || m.postgap < 0) break;
		int kept = m.pgtype[0] == 'V' ? m.pregap : 0;             /* the pregap is in the image */
		if (kept > m.frames) kept = m.frames;
		int64_t start = i == 0 ? 0 : pos + m.pregap;               /* the disc sector of the track's start (index 1) */
		int64_t lba = start - kept;
		int data = strcmp(m.type, "AUDIO") != 0;
		int take = 0;
		if (want == RC_HASH_CDTRACK_LARGEST) take = data && (!found || m.frames > pick.frames);
		else if (want == RC_HASH_CDTRACK_FIRST_DATA) take = data && !found;
		else if (want == RC_HASH_CDTRACK_LAST) take = 1;
		else take = m.num == (int)want;
		if (take) { pick = m; pick_frame = chd_frame; pick_lba = lba; found = 1; }
		pos = start + (m.frames - kept) + m.postgap;
		chd_frame += (uint32_t)m.frames + (uint32_t)(m.has_pad ? m.pad : (CD_TRACK_PADDING - m.frames % CD_TRACK_PADDING) % CD_TRACK_PADDING);
	}
	/* (RC_HASH_CDTRACK_FIRST_OF_SECOND_SESSION: a .chd doesn't keep sessions) */
	if (!found || !pick.frames || !(t->hunk = malloc(h->hunkbytes))) { close_trk(t); return NULL; }
	t->chd_frame = pick_frame;
	t->frames = (uint32_t)pick.frames;
	t->pregap = (uint32_t)(pick.pgtype[0] == 'V' ? (pick.pregap < pick.frames ? pick.pregap : pick.frames) : 0);
	t->lba = pick_lba;
	layout(pick.type, t);
	/* a raw data track says where it is on the disc in its sectors' headers: believed over the arithmetic,
	 * as rcheevos' own reader does */
	if (!t->audio) {
		const uint8_t *fr = frame(t, t->pregap + 16);
		if (fr && !memcmp(fr, SYNC, 12))
			t->lba = ((int64_t)bcd(fr[12]) * 60 + bcd(fr[13])) * 75 + bcd(fr[14]) - 150 - (int64_t)(t->pregap + 16);
	}
	return t;
}

static void *RC_CCONV cd_open(const char *path, uint32_t track, const rc_hash_iterator_t *it)
{
	const char *dot = strrchr(path, '.');
	trk_t *t;
	if (dot && !strcasecmp(dot, ".chd")) return open_chd(path, track);
	void *d = def.open_track_iterator ? def.open_track_iterator(path, track, it) : def.open_track ? def.open_track(path, track) : NULL;
	if (!d) return NULL;
	if (!(t = calloc(1, sizeof(*t)))) { def.close_track(d); return NULL; }
	t->def = d;
	return t;
}

static size_t RC_CCONV cd_read(void *h, uint32_t sector, void *buf, size_t n)
{
	trk_t *t = h;
	if (!t) return 0;
	if (t->def) return def.read_sector(t->def, sector, buf, n);
	if ((int64_t)sector < t->lba) return 0;
	uint64_t i = (uint64_t)((int64_t)sector - t->lba);
	uint8_t *out = buf;
	size_t got = 0;
	while (got < n && i < t->frames) {
		const uint8_t *fr = frame(t, (uint32_t)i++);
		if (!fr) break;
		int off = t->off >= 0 ? t->off : !memcmp(fr, SYNC, 12) ? (fr[15] == 2 ? 24 : 16) : 0;   /* mode 2 / mode 1 / cooked */
		size_t k = n - got < (size_t)t->len ? n - got : (size_t)t->len;
		if (t->audio) for (size_t j = 0; j < k; j++) out[got + j] = fr[j ^ 1];                  /* .chd keeps audio big-endian */
		else memcpy(out + got, fr + off, k);
		got += k;
	}
	return got;
}

static void RC_CCONV cd_close(void *h)
{
	trk_t *t = h;
	if (t && t->def) { def.close_track(t->def); free(t); return; }
	close_trk(t);
}

static uint32_t RC_CCONV cd_first(void *h)
{
	trk_t *t = h;
	if (!t) return 0;
	if (t->def) return def.first_track_sector(t->def);
	int64_t s = t->lba + t->pregap;
	return s > 0 ? (uint32_t)s : 0;
}

void rachd_init(void)
{
	rc_hash_get_default_cdreader(&def);
	rc_hash_cdreader_t r;
	memset(&r, 0, sizeof(r));
	r.open_track_iterator = cd_open;
	r.read_sector = cd_read;
	r.close_track = cd_close;
	r.first_track_sector = cd_first;
	rc_hash_init_custom_cdreader(&r);
}
