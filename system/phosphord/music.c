/* phosphord: the music player (Phosphor's Music tile; the phone and Windows app as remotes).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Plays only while no game runs (a game starting stops it). Albums are folders of FLAC, MP3, Ogg or WAV
 * files in a "Music" folder at the top of the SD card or a drive (NVMe, USB): ripped CDs, Steam soundtracks
 * sent from Phosphor for Windows, anything copied there. A music CD in the dock's drive plays from the disc.
 *
 * Sound: decoded to 16-bit stereo and written to aplay, i.e. MiSTer's ALSA default device: a rate
 * converter to 48 kHz in front of /dev/MrAudio, whose ring the FPGA's sys/alsa.sv plays through the menu
 * core. aplay keeps the pace.
 * Decoders: dr_flac, dr_mp3, dr_wav (David Reid, public domain / MIT-0), stb_vorbis (Sean Barrett, public
 * domain / MIT), in third_party/audio.
 * Music CDs: the table of contents gives the MusicBrainz disc ID; names come from MusicBrainz and the cover
 * from the Cover Art Archive, kept in data/music_cd so the same CD is named at once next time. */
#include "music.h"
#include "log.h"
#include "net.h"
#include "paths.h"
#include "version.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DRFLAC_API static           /* its own copy, kept private: libchdr has another dr_flac */
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_OGG
#include "dr_flac.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define STB_VORBIS_NO_PUSHDATA_API
#include "stb_vorbis.c"

extern char **environ;

/* ---------------------------------------------------------------- the library */

#define MAX_ALBUMS 2000
#define MAX_TRACKS 200

typedef struct { char path[1024], title[200]; int no; } track_t;
typedef struct {
	char id[17], dir[1024], title[200], artist[200], year[16], source[16], cover[1100];
	int ntracks;
} album_t;

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static album_t *albums;
static int nalbums;
static time_t scanned_at;

static int audio_ext(const char *n)
{
	const char *d = strrchr(n, '.');
	if (!d) return 0;
	return !strcasecmp(d, ".flac") || !strcasecmp(d, ".mp3") || !strcasecmp(d, ".ogg") || !strcasecmp(d, ".wav");
}
static int image_ext(const char *n)
{
	const char *d = strrchr(n, '.');
	return d && (!strcasecmp(d, ".jpg") || !strcasecmp(d, ".jpeg") || !strcasecmp(d, ".png"));
}

static void fnv_id(const char *s, char out[17])
{
	uint64_t h = 1469598103934665603ULL;
	for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
	snprintf(out, 17, "%016llx", (unsigned long long)h);
}

/* "03 - Harbor Lights.flac" -> 3, "Harbor Lights" */
static int name_track(const char *file, char *title, size_t n)
{
	char b[300];
	snprintf(b, sizeof(b), "%s", file);
	char *dot = strrchr(b, '.');
	if (dot) *dot = 0;
	char *p = b;
	int no = 0;
	while (isdigit((unsigned char)*p)) { no = no * 10 + (*p - '0'); p++; }
	if (p != b) while (*p == ' ' || *p == '-' || *p == '.' || *p == '_') p++;
	snprintf(title, n, "%s", *p ? p : b);
	return no;
}

static int track_cmp(const void *a, const void *b)
{
	const track_t *x = a, *y = b;
	if (x->no != y->no) return x->no - y->no;
	return strcasecmp(x->path, y->path);
}

/* a folder's tracks, in order */
static int album_tracks(const char *dir, track_t *t, int max)
{
	DIR *d = opendir(dir);
	if (!d) return 0;
	struct dirent *e;
	int n = 0;
	while ((e = readdir(d)) && n < max) {
		if (e->d_name[0] == '.' || !audio_ext(e->d_name)) continue;
		snprintf(t[n].path, sizeof(t[n].path), "%s/%s", dir, e->d_name);
		t[n].no = name_track(e->d_name, t[n].title, sizeof(t[n].title));
		n++;
	}
	closedir(d);
	qsort(t, (size_t)n, sizeof(*t), track_cmp);
	return n;
}

/* album.json (written when a CD is ripped or a soundtrack sent): {"title","artist","year","source"} */
static void album_meta(album_t *a)
{
	char p[1100];
	snprintf(p, sizeof(p), "%s/album.json", a->dir);
	FILE *f = fopen(p, "r");
	if (!f) return;
	char buf[8192];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = 0;
	cJSON *j = cJSON_Parse(buf);
	const char *v;
#define M(k, f) if ((v = cJSON_GetStringValue(cJSON_GetObjectItem(j, k))) && *v) snprintf(a->f, sizeof(a->f), "%s", v)
	M("title", title); M("artist", artist); M("year", year); M("source", source);
#undef M
	cJSON_Delete(j);
}

static void album_cover(album_t *a)
{
	static const char *const N[] = { "cover.jpg", "cover.png", "folder.jpg", "front.jpg", "Cover.jpg", "Folder.jpg", NULL };
	for (int i = 0; N[i]; i++) {
		snprintf(a->cover, sizeof(a->cover), "%s/%s", a->dir, N[i]);
		if (file_exists(a->cover)) return;
	}
	a->cover[0] = 0;
	DIR *d = opendir(a->dir);
	struct dirent *e;
	while (d && (e = readdir(d))) if (e->d_name[0] != '.' && image_ext(e->d_name)) { snprintf(a->cover, sizeof(a->cover), "%s/%s", a->dir, e->d_name); break; }
	if (d) closedir(d);
}

static void scan_dir(const char *dir, const char *parent_name, int depth, album_t *out, int *n)
{
	if (depth > 3 || *n >= MAX_ALBUMS) return;
	DIR *d = opendir(dir);
	if (!d) return;
	struct dirent *e;
	int tracks = 0;
	char sub[64][256];
	int nsub = 0;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		if (audio_ext(e->d_name)) { tracks++; continue; }
		if (e->d_type == DT_DIR || e->d_type == DT_UNKNOWN) {
			if (nsub < 64) snprintf(sub[nsub++], sizeof(sub[0]), "%s", e->d_name);
		}
	}
	closedir(d);
	if (tracks) {
		album_t *a = &out[(*n)++];
		memset(a, 0, sizeof(*a));
		snprintf(a->dir, sizeof(a->dir), "%s", dir);
		fnv_id(dir, a->id);
		const char *base = strrchr(dir, '/');
		snprintf(a->title, sizeof(a->title), "%s", base ? base + 1 : dir);
		if (parent_name && depth >= 2) snprintf(a->artist, sizeof(a->artist), "%s", parent_name);
		snprintf(a->source, sizeof(a->source), strstr(dir, "/Steam/") ? "steam" : "files");
		a->ntracks = tracks;
		album_meta(a);
		album_cover(a);
	}
	const char *me = strrchr(dir, '/');
	for (int i = 0; i < nsub; i++) {
		char p[1100];
		snprintf(p, sizeof(p), "%s/%s", dir, sub[i]);
		struct stat sb;
		if (stat(p, &sb) || !S_ISDIR(sb.st_mode)) continue;
		scan_dir(p, me ? me + 1 : dir, depth + 1, out, n);
	}
}

static int album_cmp(const void *a, const void *b)
{
	const album_t *x = a, *y = b;
	int c = strcasecmp(x->artist, y->artist);
	return c ? c : strcasecmp(x->title, y->title);
}

/* "Music" (or "music") at the top of the SD card and each drive */
static void music_roots(char roots[][600], int *n)
{
	*n = 0;
	const char *bases[12];
	int nb = 0;
	bases[nb++] = p_fat();
	static char usb[8][32];
	for (int i = 0; i < 8; i++) { snprintf(usb[i], sizeof(usb[i]), "/media/usb%d", i); bases[nb++] = usb[i]; }
	for (int i = 0; i < nb; i++)
		for (int c = 0; c < 2; c++) {
			snprintf(roots[*n], 600, "%s/%s", bases[i], c ? "music" : "Music");
			struct stat sb;
			if (!stat(roots[*n], &sb) && S_ISDIR(sb.st_mode)) { (*n)++; break; }
		}
}

void music_scan(void)
{
	album_t *a = calloc(MAX_ALBUMS, sizeof(*a));
	if (!a) return;
	int n = 0;
	char roots[12][600];
	int nr;
	music_roots(roots, &nr);
	for (int i = 0; i < nr; i++) scan_dir(roots[i], NULL, 0, a, &n);
	qsort(a, (size_t)n, sizeof(*a), album_cmp);
	pthread_mutex_lock(&lk);
	free(albums);
	albums = a;
	nalbums = n;
	scanned_at = time(NULL);
	pthread_mutex_unlock(&lk);
}

/* where new albums go (rips, Steam): the first drive's Music folder, else the SD card's */
int music_home(char *out, size_t n)
{
	for (int i = 0; i < 8; i++) {
		char d[32];
		snprintf(d, sizeof(d), "/media/usb%d", i);
		struct stat sb;
		if (!stat(d, &sb) && S_ISDIR(sb.st_mode) && access(d, W_OK) == 0) {
			FILE *f = fopen("/proc/mounts", "r");
			int mounted = 0;
			char line[512];
			while (f && fgets(line, sizeof(line), f)) if (strstr(line, d) && strstr(line, " rw")) mounted = 1;
			if (f) fclose(f);
			if (mounted) { snprintf(out, n, "%s/Music", d); mkdir(out, 0755); return 0; }
		}
	}
	snprintf(out, n, "%s/Music", p_fat());
	mkdir(out, 0755);
	return 0;
}

/* ---------------------------------------------------------------- the drive (music CDs) */

#define CD_DEV "/dev/sr0"
typedef struct { int first, last, lba[100], leadout, audio[100]; } toc_t;
static struct {
	int present;               /* an audio CD's TOC was read */
	toc_t toc;
	char discid[40], title[200], artist[200], year[16], cover[700], tracks[100][160];
	int named, looking;
} cd;

static int read_toc(toc_t *t)
{
	int fd = open(CD_DEV, O_RDONLY | O_NONBLOCK);
	if (fd < 0) return -1;
	int ok = -1;
	if (ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_DISC_OK) {
		struct cdrom_tochdr h;
		if (!ioctl(fd, CDROMREADTOCHDR, &h) && h.cdth_trk1 >= h.cdth_trk0 && h.cdth_trk1 < 100) {
			memset(t, 0, sizeof(*t));
			t->first = h.cdth_trk0;
			t->last = h.cdth_trk1;
			int audio = 0;
			for (int i = t->first; i <= t->last + 1; i++) {
				struct cdrom_tocentry e = { 0 };
				e.cdte_track = i > t->last ? CDROM_LEADOUT : i;
				e.cdte_format = CDROM_LBA;
				if (ioctl(fd, CDROMREADTOCENTRY, &e)) break;
				if (i > t->last) t->leadout = e.cdte_addr.lba;
				else { t->lba[i] = e.cdte_addr.lba; t->audio[i] = !(e.cdte_ctrl & CDROM_DATA_TRACK); audio += t->audio[i]; }
			}
			ok = audio ? 0 : -1;
		}
	}
	close(fd);
	return ok;
}

/* ---- MusicBrainz disc ID: SHA-1 over the TOC, base64 with ._- (musicbrainz.org/doc/Disc_ID_Calculation) */
typedef struct { uint32_t h[5]; uint64_t len; uint8_t buf[64]; size_t n; } sha1_t;
#define ROL(x, c) (((x) << (c)) | ((x) >> (32 - (c))))
static void sha1_block(sha1_t *s, const uint8_t *p)
{
	uint32_t w[80];
	for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
	for (int i = 16; i < 80; i++) w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4];
	for (int i = 0; i < 80; i++) {
		uint32_t f, k;
		if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
		else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
		else { f = b ^ c ^ d; k = 0xCA62C1D6; }
		uint32_t t = ROL(a, 5) + f + e + k + w[i];
		e = d; d = c; c = ROL(b, 30); b = a; a = t;
	}
	s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e;
}
static void sha1_init(sha1_t *s) { static const uint32_t I[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 }; memcpy(s->h, I, sizeof(I)); s->len = 0; s->n = 0; }
static void sha1_add(sha1_t *s, const void *d, size_t n)
{
	const uint8_t *p = d;
	s->len += n;
	while (n--) { s->buf[s->n++] = *p++; if (s->n == 64) { sha1_block(s, s->buf); s->n = 0; } }
}
static void sha1_end(sha1_t *s, uint8_t out[20])
{
	uint64_t bits = s->len * 8;
	uint8_t one = 0x80, zero = 0;
	sha1_add(s, &one, 1);
	while (s->n != 56) sha1_add(s, &zero, 1);
	for (int i = 7; i >= 0; i--) { uint8_t b = (uint8_t)(bits >> (i * 8)); sha1_add(s, &b, 1); }
	for (int i = 0; i < 5; i++) { out[i * 4] = (uint8_t)(s->h[i] >> 24); out[i * 4 + 1] = (uint8_t)(s->h[i] >> 16); out[i * 4 + 2] = (uint8_t)(s->h[i] >> 8); out[i * 4 + 3] = (uint8_t)s->h[i]; }
}

static void disc_id(const toc_t *t, char out[40])
{
	sha1_t s;
	sha1_init(&s);
	char b[16];
	snprintf(b, sizeof(b), "%02X", t->first); sha1_add(&s, b, 2);
	snprintf(b, sizeof(b), "%02X", t->last); sha1_add(&s, b, 2);
	for (int i = 0; i < 100; i++) {
		int off = i == 0 ? t->leadout + 150 : (i >= t->first && i <= t->last ? t->lba[i] + 150 : 0);
		snprintf(b, sizeof(b), "%08X", off);
		sha1_add(&s, b, 8);
	}
	uint8_t h[20];
	sha1_end(&s, h);
	static const char *B = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";
	int o = 0;
	for (int i = 0; i < 20; i += 3) {
		uint32_t v = (uint32_t)h[i] << 16 | (i + 1 < 20 ? (uint32_t)h[i + 1] << 8 : 0) | (i + 2 < 20 ? h[i + 2] : 0);
		out[o++] = B[(v >> 18) & 63];
		out[o++] = B[(v >> 12) & 63];
		out[o++] = i + 1 < 20 ? B[(v >> 6) & 63] : '-';
		out[o++] = i + 2 < 20 ? B[v & 63] : '-';
	}
	out[o] = 0;
}

static void cd_cache_path(char *out, size_t n, const char *ext)
{
	snprintf(out, n, "%s/music_cd", p_data());
	mkdir(out, 0755);
	size_t l = strlen(out);
	snprintf(out + l, n - l, "/%s.%s", cd.discid, ext);
}

static void cd_apply(cJSON *j)
{
	const char *v;
	if ((v = cJSON_GetStringValue(cJSON_GetObjectItem(j, "title")))) snprintf(cd.title, sizeof(cd.title), "%s", v);
	if ((v = cJSON_GetStringValue(cJSON_GetObjectItem(j, "artist")))) snprintf(cd.artist, sizeof(cd.artist), "%s", v);
	if ((v = cJSON_GetStringValue(cJSON_GetObjectItem(j, "year")))) snprintf(cd.year, sizeof(cd.year), "%s", v);
	cJSON *t;
	int i = 0;
	cJSON_ArrayForEach(t, cJSON_GetObjectItem(j, "tracks")) {
		if (i >= 99) break;
		if (cJSON_IsString(t)) snprintf(cd.tracks[i + 1], sizeof(cd.tracks[0]), "%s", t->valuestring);
		i++;
	}
	cd.named = 1;
}

/* MusicBrainz: the release with this disc ID (or, failing that, a close track layout) */
static void *cd_lookup(void *arg)
{
	(void)arg;
	char id[40];
	toc_t t;
	pthread_mutex_lock(&lk);
	snprintf(id, sizeof(id), "%s", cd.discid);
	t = cd.toc;
	pthread_mutex_unlock(&lk);
	char toc[1200];
	int o = snprintf(toc, sizeof(toc), "%d+%d+%d", t.first, t.last, t.leadout + 150);
	for (int i = t.first; i <= t.last && o < (int)sizeof(toc) - 16; i++) o += snprintf(toc + o, sizeof(toc) - (size_t)o, "+%d", t.lba[i] + 150);
	char url[1600];
	snprintf(url, sizeof(url), "https://musicbrainz.org/ws/2/discid/%s?toc=%s&inc=recordings+artist-credits&fmt=json", id, toc);
	char *out = NULL;
	int http = 0;
	net_request("GET", url, NULL, NULL, &out, NULL, &http);
	cJSON *j = out ? cJSON_Parse(out) : NULL;
	free(out);
	cJSON *rel = cJSON_GetArrayItem(cJSON_GetObjectItem(j, "releases"), 0);
	cJSON *res = NULL;
	char mbid[64] = "";
	if (rel) {
		res = cJSON_CreateObject();
		const char *v;
		if ((v = cJSON_GetStringValue(cJSON_GetObjectItem(rel, "title")))) cJSON_AddStringToObject(res, "title", v);
		if ((v = cJSON_GetStringValue(cJSON_GetObjectItem(rel, "date")))) { char y[8]; snprintf(y, sizeof(y), "%.4s", v); cJSON_AddStringToObject(res, "year", y); }
		if ((v = cJSON_GetStringValue(cJSON_GetObjectItem(rel, "id")))) snprintf(mbid, sizeof(mbid), "%s", v);
		char artist[200] = "";
		cJSON *ac;
		cJSON_ArrayForEach(ac, cJSON_GetObjectItem(rel, "artist-credit")) {
			const char *nm = cJSON_GetStringValue(cJSON_GetObjectItem(ac, "name")), *jp = cJSON_GetStringValue(cJSON_GetObjectItem(ac, "joinphrase"));
			size_t l = strlen(artist);
			snprintf(artist + l, sizeof(artist) - l, "%s%s", nm ? nm : "", jp ? jp : "");
		}
		cJSON_AddStringToObject(res, "artist", artist);
		/* the medium (disc) with this ID */
		cJSON *media = cJSON_GetObjectItem(rel, "media"), *m, *pick = cJSON_GetArrayItem(media, 0);
		cJSON_ArrayForEach(m, media) {
			cJSON *d;
			cJSON_ArrayForEach(d, cJSON_GetObjectItem(m, "discs")) {
				const char *did = cJSON_GetStringValue(cJSON_GetObjectItem(d, "id"));
				if (did && !strcmp(did, id)) pick = m;
			}
		}
		cJSON *tr = cJSON_AddArrayToObject(res, "tracks"), *x;
		cJSON_ArrayForEach(x, cJSON_GetObjectItem(pick, "tracks")) {
			const char *tt = cJSON_GetStringValue(cJSON_GetObjectItem(x, "title"));
			cJSON_AddItemToArray(tr, cJSON_CreateString(tt ? tt : ""));
		}
		if (mbid[0]) cJSON_AddStringToObject(res, "mbid", mbid);
	}
	cJSON_Delete(j);
	char jp[700], cp[700];
	pthread_mutex_lock(&lk);
	int same = !strcmp(id, cd.discid);
	if (same) { cd_cache_path(jp, sizeof(jp), "json"); cd_cache_path(cp, sizeof(cp), "jpg"); }
	pthread_mutex_unlock(&lk);
	if (res && same) {
		char *s = cJSON_Print(res);
		FILE *f = s ? fopen(jp, "w") : NULL;
		if (f) { fputs(s, f); fclose(f); }
		free(s);
		/* the cover (Cover Art Archive, linked to MusicBrainz) */
		if (mbid[0]) {
			snprintf(url, sizeof(url), "https://coverartarchive.org/release/%s/front-500", mbid);
			if (net_get_file(url, cp) != 0) cp[0] = 0;
		} else cp[0] = 0;
		pthread_mutex_lock(&lk);
		if (!strcmp(id, cd.discid)) { cd_apply(res); if (cp[0]) snprintf(cd.cover, sizeof(cd.cover), "%s", cp); }
		pthread_mutex_unlock(&lk);
		plog("music: CD %s named from MusicBrainz", id);
	} else plog("music: CD %s isn't in MusicBrainz (HTTP %d)", id, http);
	cJSON_Delete(res);
	pthread_mutex_lock(&lk);
	cd.looking = 0;
	pthread_mutex_unlock(&lk);
	return NULL;
}

/* called when phosphord sees the drive change: a music CD -> its TOC, then its names */
void music_cd_check(void)
{
	toc_t t;
	int ok = read_toc(&t) == 0;
	pthread_mutex_lock(&lk);
	if (!ok) { memset(&cd, 0, sizeof(cd)); pthread_mutex_unlock(&lk); return; }
	char id[40];
	disc_id(&t, id);
	if (cd.present && !strcmp(id, cd.discid)) { pthread_mutex_unlock(&lk); return; }
	memset(&cd, 0, sizeof(cd));
	cd.present = 1;
	cd.toc = t;
	snprintf(cd.discid, sizeof(cd.discid), "%s", id);
	char jp[700], cp[700];
	cd_cache_path(jp, sizeof(jp), "json");
	cd_cache_path(cp, sizeof(cp), "jpg");
	FILE *f = fopen(jp, "r");
	if (f) {
		char buf[16384];
		size_t n = fread(buf, 1, sizeof(buf) - 1, f);
		fclose(f);
		buf[n] = 0;
		cJSON *j = cJSON_Parse(buf);
		if (j) cd_apply(j);
		cJSON_Delete(j);
		if (file_exists(cp)) snprintf(cd.cover, sizeof(cd.cover), "%s", cp);
	}
	int look = !cd.named;
	if (look) cd.looking = 1;
	pthread_mutex_unlock(&lk);
	if (look) {
		pthread_t th;
		if (pthread_create(&th, NULL, cd_lookup, NULL) == 0) pthread_detach(th);
		else { pthread_mutex_lock(&lk); cd.looking = 0; pthread_mutex_unlock(&lk); }
	}
}

/* ---------------------------------------------------------------- the player */

typedef struct { int cd_track; char path[1024], title[200], album[200], artist[200], cover[1100]; } qitem_t;
static struct {
	qitem_t *q;
	int n, cur;               /* the queue and the track playing */
	int state;                /* 0 stopped, 1 playing, 2 paused */
	int shuffle, repeat;      /* repeat: 0 off, 1 the queue, 2 the track */
	int volume;               /* 0..100 */
	long pos_ms, len_ms;
	long seek_ms;             /* -1: none asked */
	int skip;                 /* +1 next, -1 previous: asked of the player thread */
	int gen;                  /* bumped when the queue changes: the thread starts the new track */
	char error[200];
} pl = { .volume = 100, .seek_ms = -1 };
static pthread_cond_t wake = PTHREAD_COND_INITIALIZER;
static int thread_up;

/* aplay: ALSA's default device (a rate converter to 48 kHz in front of /dev/MrAudio) */
static pid_t out_pid = -1;
static int out_fd = -1, out_rate, out_ch;
static void out_close(void)
{
	if (out_fd >= 0) close(out_fd);
	out_fd = -1;
	if (out_pid > 0) { kill(out_pid, SIGTERM); waitpid(out_pid, NULL, 0); }
	out_pid = -1;
}
static int out_open(int rate)
{
	if (out_fd >= 0 && out_rate == rate) return 0;
	out_close();
	int p[2];
	if (pipe(p)) return -1;
	char r[16];
	snprintf(r, sizeof(r), "%d", rate);
	const char *argv[] = { "aplay", "-q", "-t", "raw", "-f", "S16_LE", "-c", "2", "-r", r, "-B", "200000", NULL };
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, p[0], 0);
	posix_spawn_file_actions_addclose(&fa, p[1]);
	int devnull = open("/dev/null", O_WRONLY);
	if (devnull >= 0) posix_spawn_file_actions_adddup2(&fa, devnull, 2);
	int rc = posix_spawnp(&out_pid, "aplay", &fa, NULL, (char *const *)argv, environ);
	posix_spawn_file_actions_destroy(&fa);
	if (devnull >= 0) close(devnull);
	close(p[0]);
	if (rc) { close(p[1]); out_pid = -1; return -1; }
	out_fd = p[1];
	fcntl(out_fd, F_SETFD, FD_CLOEXEC);
	out_rate = rate;
	out_ch = 2;
	return 0;
}
static int out_write(int16_t *s, size_t frames)
{
	int v = pl.volume;
	if (v < 100) for (size_t i = 0; i < frames * 2; i++) s[i] = (int16_t)((int)s[i] * v * v / 10000);
	const char *p = (const char *)s;
	size_t left = frames * 4;
	while (left) {
		ssize_t w = write(out_fd, p, left);
		if (w < 0 && errno == EINTR) continue;
		if (w <= 0) return -1;
		p += w;
		left -= (size_t)w;
	}
	return 0;
}

/* one decoder for every kind of file: frames of interleaved 16-bit stereo */
typedef struct {
	int kind;                 /* 1 flac 2 mp3 3 ogg 4 wav 5 cd */
	drflac *flac; drmp3 mp3; drwav wav; stb_vorbis *ogg;
	int rate, ch;
	uint64_t frames, at;
	int cdfd, cd_lba0, cd_lba1;   /* CD: the track's sectors */
} dec_t;

static int dec_open(dec_t *d, const qitem_t *it)
{
	memset(d, 0, sizeof(*d));
	if (it->cd_track) {
		pthread_mutex_lock(&lk);
		int ok = cd.present && it->cd_track >= cd.toc.first && it->cd_track <= cd.toc.last;
		if (ok) { d->cd_lba0 = cd.toc.lba[it->cd_track]; d->cd_lba1 = it->cd_track < cd.toc.last ? cd.toc.lba[it->cd_track + 1] : cd.toc.leadout; }
		pthread_mutex_unlock(&lk);
		if (!ok) return -1;
		if (it->cd_track < cd.toc.last && !cd.toc.audio[it->cd_track + 1]) d->cd_lba1 -= 11400;   /* before a data track (CD-Extra) */
		d->cdfd = open(CD_DEV, O_RDONLY | O_NONBLOCK);
		if (d->cdfd < 0) return -1;
		ioctl(d->cdfd, CDROM_SELECT_SPEED, 4);       /* quiet */
		d->kind = 5; d->rate = 44100; d->ch = 2;
		d->frames = (uint64_t)(d->cd_lba1 - d->cd_lba0) * 588;
		return 0;
	}
	const char *e = strrchr(it->path, '.');
	if (!e) return -1;
	if (!strcasecmp(e, ".flac")) {
		if (!(d->flac = drflac_open_file(it->path, NULL))) return -1;
		d->kind = 1; d->rate = (int)d->flac->sampleRate; d->ch = d->flac->channels; d->frames = d->flac->totalPCMFrameCount;
	} else if (!strcasecmp(e, ".mp3")) {
		if (!drmp3_init_file(&d->mp3, it->path, NULL)) return -1;
		d->kind = 2; d->rate = (int)d->mp3.sampleRate; d->ch = (int)d->mp3.channels; d->frames = drmp3_get_pcm_frame_count(&d->mp3);
	} else if (!strcasecmp(e, ".ogg")) {
		int err;
		if (!(d->ogg = stb_vorbis_open_filename(it->path, &err, NULL))) return -1;
		stb_vorbis_info i = stb_vorbis_get_info(d->ogg);
		d->kind = 3; d->rate = (int)i.sample_rate; d->ch = i.channels; d->frames = stb_vorbis_stream_length_in_samples(d->ogg);
	} else if (!strcasecmp(e, ".wav")) {
		if (!drwav_init_file(&d->wav, it->path, NULL)) return -1;
		d->kind = 4; d->rate = (int)d->wav.sampleRate; d->ch = d->wav.channels; d->frames = d->wav.totalPCMFrameCount;
	} else return -1;
	return d->rate > 0 && d->ch > 0 ? 0 : -1;
}

static void dec_close(dec_t *d)
{
	if (d->kind == 1) drflac_close(d->flac);
	else if (d->kind == 2) drmp3_uninit(&d->mp3);
	else if (d->kind == 3) stb_vorbis_close(d->ogg);
	else if (d->kind == 4) drwav_uninit(&d->wav);
	else if (d->kind == 5 && d->cdfd >= 0) close(d->cdfd);
	d->kind = 0;
}

static void dec_seek(dec_t *d, uint64_t f)
{
	if (f > d->frames) f = d->frames;
	if (d->kind == 1) drflac_seek_to_pcm_frame(d->flac, f);
	else if (d->kind == 2) drmp3_seek_to_pcm_frame(&d->mp3, f);
	else if (d->kind == 3) stb_vorbis_seek(d->ogg, (unsigned)f);
	else if (d->kind == 4) drwav_seek_to_pcm_frame(&d->wav, f);
	else if (d->kind == 5) f -= f % 588;
	d->at = f;
}

/* up to n frames into s (stereo); 0 at the end */
static size_t dec_read(dec_t *d, int16_t *s, size_t n)
{
	static int16_t tmp[4096 * 8];
	size_t got = 0;
	int ch = d->ch;
	if (n > 4096) n = 4096;
	if (d->kind == 5) {
		/* CD audio: whole sectors (588 frames each) read with CDROMREADAUDIO */
		int lba = d->cd_lba0 + (int)(d->at / 588);
		int sectors = (int)(n / 588);
		if (sectors < 1) sectors = 1;
		if (lba + sectors > d->cd_lba1) sectors = d->cd_lba1 - lba;
		if (sectors <= 0) return 0;
		struct cdrom_read_audio ra = { 0 };
		ra.addr.lba = lba;
		ra.addr_format = CDROM_LBA;
		ra.nframes = sectors;
		ra.buf = (unsigned char *)s;
		int tries = 0;
		while (ioctl(d->cdfd, CDROMREADAUDIO, &ra) && ++tries < 3) usleep(20000);
		if (tries >= 3) return 0;
		d->at += (uint64_t)sectors * 588;
		return (size_t)sectors * 588;
	}
	if (ch > 8) return 0;
	if (d->kind == 1) got = (size_t)drflac_read_pcm_frames_s16(d->flac, n, tmp);
	else if (d->kind == 2) got = (size_t)drmp3_read_pcm_frames_s16(&d->mp3, n, tmp);
	else if (d->kind == 3) got = (size_t)stb_vorbis_get_samples_short_interleaved(d->ogg, ch, tmp, (int)(n * (size_t)ch));
	else if (d->kind == 4) got = (size_t)drwav_read_pcm_frames_s16(&d->wav, n, tmp);
	for (size_t i = 0; i < got; i++) {
		s[i * 2] = tmp[i * (size_t)ch];
		s[i * 2 + 1] = tmp[i * (size_t)ch + (ch > 1 ? 1 : 0)];
	}
	d->at += got;
	return got;
}

static void *player(void *arg)
{
	(void)arg;
	static int16_t buf[4096 * 2];
	for (;;) {
		pthread_mutex_lock(&lk);
		while (pl.state != 1) { out_close(); pthread_cond_wait(&wake, &lk); }
		if (pl.cur < 0 || pl.cur >= pl.n) { pl.state = 0; pthread_mutex_unlock(&lk); continue; }
		qitem_t it = pl.q[pl.cur];
		int gen = pl.gen;
		pthread_mutex_unlock(&lk);

		dec_t d;
		if (dec_open(&d, &it) || out_open(d.rate)) {
			pthread_mutex_lock(&lk);
			snprintf(pl.error, sizeof(pl.error), "Couldn't play %s", it.title);
			if (gen == pl.gen) { if (pl.cur + 1 < pl.n) pl.cur++; else pl.state = 0; }
			pthread_mutex_unlock(&lk);
			dec_close(&d);
			sleep(1);
			continue;
		}
		pthread_mutex_lock(&lk);
		pl.len_ms = (long)(d.frames * 1000 / (uint64_t)d.rate);
		pl.pos_ms = 0;
		pl.error[0] = 0;
		pthread_mutex_unlock(&lk);
		int ended = 0, left_track = 0;
		for (;;) {
			pthread_mutex_lock(&lk);
			while (pl.state == 2 && gen == pl.gen && !pl.skip && pl.seek_ms < 0) pthread_cond_wait(&wake, &lk);
			if (gen != pl.gen || pl.state != 1 && pl.state != 2) { left_track = 1; pthread_mutex_unlock(&lk); break; }
			if (pl.skip) { left_track = 1; pthread_mutex_unlock(&lk); break; }
			if (pl.seek_ms >= 0) { dec_seek(&d, (uint64_t)pl.seek_ms * (uint64_t)d.rate / 1000); pl.seek_ms = -1; }
			if (pl.state == 2) { pthread_mutex_unlock(&lk); continue; }
			pthread_mutex_unlock(&lk);
			size_t got = dec_read(&d, buf, d.kind == 5 ? 588 * 4 : 2048);
			if (!got) { ended = 1; break; }
			if (out_write(buf, got)) { out_close(); if (out_open(d.rate)) { ended = 1; break; } }
			pthread_mutex_lock(&lk);
			pl.pos_ms = (long)(d.at * 1000 / (uint64_t)d.rate);
			pthread_mutex_unlock(&lk);
		}
		dec_close(&d);
		pthread_mutex_lock(&lk);
		if (gen == pl.gen) {
			int step = pl.skip ? pl.skip : 1;
			pl.skip = 0;
			if (ended && pl.repeat == 2) step = 0;
			if (ended || left_track) {
				int nx = pl.cur + step;
				if (pl.shuffle && step && pl.n > 1) { nx = rand() % pl.n; if (nx == pl.cur) nx = (nx + 1) % pl.n; }
				if (nx < 0) nx = 0;
				if (nx >= pl.n) { if (pl.repeat == 1) nx = 0; else { pl.state = 0; nx = pl.cur; } }
				pl.cur = nx;
				pl.pos_ms = 0;
			}
		}
		pthread_mutex_unlock(&lk);
	}
	return NULL;
}

static void start_thread(void)
{
	if (thread_up) return;
	pthread_t t;
	if (pthread_create(&t, NULL, player, NULL) == 0) { pthread_detach(t); thread_up = 1; }
}

/* ---------------------------------------------------------------- what the API calls */

static const album_t *album_by_id(const char *id)
{
	for (int i = 0; i < nalbums; i++) if (!strcmp(albums[i].id, id)) return &albums[i];
	return NULL;
}

cJSON *music_albums(void)
{
	if (!scanned_at || time(NULL) - scanned_at > 300) music_scan();
	cJSON *arr = cJSON_CreateArray();
	pthread_mutex_lock(&lk);
	for (int i = 0; i < nalbums; i++) {
		cJSON *a = cJSON_CreateObject();
		cJSON_AddStringToObject(a, "id", albums[i].id);
		cJSON_AddStringToObject(a, "title", albums[i].title);
		cJSON_AddStringToObject(a, "artist", albums[i].artist);
		cJSON_AddStringToObject(a, "year", albums[i].year);
		cJSON_AddStringToObject(a, "source", albums[i].source);
		cJSON_AddNumberToObject(a, "tracks", albums[i].ntracks);
		if (albums[i].cover[0]) cJSON_AddStringToObject(a, "cover", albums[i].cover);
		cJSON_AddItemToArray(arr, a);
	}
	pthread_mutex_unlock(&lk);
	return arr;
}

static cJSON *cd_json_locked(void)
{
	if (!cd.present) return NULL;
	cJSON *c = cJSON_CreateObject();
	cJSON_AddStringToObject(c, "id", "cd");
	cJSON_AddStringToObject(c, "title", cd.named && cd.title[0] ? cd.title : "Music CD");
	cJSON_AddStringToObject(c, "artist", cd.artist);
	cJSON_AddStringToObject(c, "year", cd.year);
	cJSON_AddStringToObject(c, "source", "cd");
	cJSON_AddStringToObject(c, "discid", cd.discid);
	cJSON_AddBoolToObject(c, "named", cd.named);
	cJSON_AddBoolToObject(c, "looking", cd.looking);
	if (cd.cover[0]) cJSON_AddStringToObject(c, "cover", cd.cover);
	cJSON *t = cJSON_AddArrayToObject(c, "track_list");
	int n = 0;
	for (int i = cd.toc.first; i <= cd.toc.last; i++) {
		if (!cd.toc.audio[i]) continue;
		n++;
		int end = i < cd.toc.last ? cd.toc.lba[i + 1] : cd.toc.leadout;
		cJSON *x = cJSON_CreateObject();
		char nm[180];
		snprintf(nm, sizeof(nm), "%s", cd.tracks[i][0] ? cd.tracks[i] : "");
		if (!nm[0]) snprintf(nm, sizeof(nm), "Track %d", i);
		cJSON_AddNumberToObject(x, "no", i);
		cJSON_AddStringToObject(x, "title", nm);
		cJSON_AddNumberToObject(x, "ms", (double)(end - cd.toc.lba[i]) * 1000 / 75);
		cJSON_AddItemToArray(t, x);
	}
	cJSON_AddNumberToObject(c, "tracks", n);
	return c;
}

cJSON *music_cd(void)
{
	pthread_mutex_lock(&lk);
	cJSON *c = cd_json_locked();
	pthread_mutex_unlock(&lk);
	return c;
}

cJSON *music_album(const char *id)
{
	if (!strcmp(id, "cd")) return music_cd();
	pthread_mutex_lock(&lk);
	const album_t *a = album_by_id(id);
	album_t copy;
	if (a) copy = *a;
	pthread_mutex_unlock(&lk);
	if (!a) return NULL;
	cJSON *o = cJSON_CreateObject();
	cJSON_AddStringToObject(o, "id", copy.id);
	cJSON_AddStringToObject(o, "title", copy.title);
	cJSON_AddStringToObject(o, "artist", copy.artist);
	cJSON_AddStringToObject(o, "year", copy.year);
	cJSON_AddStringToObject(o, "source", copy.source);
	if (copy.cover[0]) cJSON_AddStringToObject(o, "cover", copy.cover);
	track_t *t = calloc(MAX_TRACKS, sizeof(*t));
	int n = t ? album_tracks(copy.dir, t, MAX_TRACKS) : 0;
	cJSON *arr = cJSON_AddArrayToObject(o, "track_list");
	for (int i = 0; i < n; i++) {
		cJSON *x = cJSON_CreateObject();
		cJSON_AddNumberToObject(x, "no", t[i].no ? t[i].no : i + 1);
		cJSON_AddStringToObject(x, "title", t[i].title);
		cJSON_AddItemToArray(arr, x);
	}
	cJSON_AddNumberToObject(o, "tracks", n);
	free(t);
	return o;
}

int music_play(const char *album, int track, char *err, size_t errn)
{
	qitem_t *q = NULL;
	int n = 0, start = 0;
	if (!strcmp(album, "cd")) {
		pthread_mutex_lock(&lk);
		if (!cd.present) { pthread_mutex_unlock(&lk); snprintf(err, errn, "No music CD in the drive"); return -1; }
		q = calloc(100, sizeof(*q));
		for (int i = cd.toc.first; q && i <= cd.toc.last; i++) {
			if (!cd.toc.audio[i]) continue;
			if (i == track) start = n;
			q[n].cd_track = i;
			snprintf(q[n].title, sizeof(q[n].title), "%s", cd.tracks[i][0] ? cd.tracks[i] : "");
			if (!q[n].title[0]) snprintf(q[n].title, sizeof(q[n].title), "Track %d", i);
			snprintf(q[n].album, sizeof(q[n].album), "%s", cd.named && cd.title[0] ? cd.title : "Music CD");
			snprintf(q[n].artist, sizeof(q[n].artist), "%s", cd.artist);
			snprintf(q[n].cover, sizeof(q[n].cover), "%s", cd.cover);
			n++;
		}
		pthread_mutex_unlock(&lk);
	} else {
		pthread_mutex_lock(&lk);
		const album_t *a = album_by_id(album);
		album_t copy;
		if (a) copy = *a;
		pthread_mutex_unlock(&lk);
		if (!a) { snprintf(err, errn, "No such album"); return -1; }
		track_t *t = calloc(MAX_TRACKS, sizeof(*t));
		int nt = t ? album_tracks(copy.dir, t, MAX_TRACKS) : 0;
		q = calloc((size_t)(nt ? nt : 1), sizeof(*q));
		for (int i = 0; q && i < nt; i++) {
			snprintf(q[i].path, sizeof(q[i].path), "%s", t[i].path);
			snprintf(q[i].title, sizeof(q[i].title), "%s", t[i].title);
			snprintf(q[i].album, sizeof(q[i].album), "%s", copy.title);
			snprintf(q[i].artist, sizeof(q[i].artist), "%s", copy.artist);
			snprintf(q[i].cover, sizeof(q[i].cover), "%s", copy.cover);
			if (track > 0 && (t[i].no == track || (!t[i].no && i + 1 == track))) start = i;
		}
		n = nt;
		free(t);
	}
	if (!q || !n) { free(q); snprintf(err, errn, "Nothing to play"); return -1; }
	pthread_mutex_lock(&lk);
	free(pl.q);
	pl.q = q;
	pl.n = n;
	pl.cur = pl.shuffle && track <= 0 ? rand() % n : start;
	pl.state = 1;
	pl.skip = 0;
	pl.seek_ms = -1;
	pl.gen++;
	pthread_cond_broadcast(&wake);
	pthread_mutex_unlock(&lk);
	start_thread();
	return 0;
}

int music_control(const char *what, cJSON *req, char *err, size_t errn)
{
	pthread_mutex_lock(&lk);
	int rc = 0;
	if (!strcmp(what, "pause")) { if (pl.state == 1) pl.state = 2; }
	else if (!strcmp(what, "resume")) { if (pl.state == 2) pl.state = 1; }
	else if (!strcmp(what, "toggle")) { if (pl.state == 1) pl.state = 2; else if (pl.state == 2) pl.state = 1; }
	else if (!strcmp(what, "stop")) { pl.state = 0; pl.gen++; }
	else if (!strcmp(what, "next")) { if (pl.state) { pl.skip = 1; if (pl.state == 2) pl.state = 1; } }
	else if (!strcmp(what, "previous")) {
		if (pl.state) { if (pl.pos_ms > 3000) pl.seek_ms = 0; else pl.skip = -1; if (pl.state == 2) pl.state = 1; }
	}
	else if (!strcmp(what, "seek")) { double ms = cJSON_GetNumberValue(cJSON_GetObjectItem(req, "ms")); if (pl.state && ms >= 0) pl.seek_ms = (long)ms; }
	else if (!strcmp(what, "volume")) { double v = cJSON_GetNumberValue(cJSON_GetObjectItem(req, "value")); if (v >= 0 && v <= 100) pl.volume = (int)v; }
	else if (!strcmp(what, "shuffle")) pl.shuffle = cJSON_IsTrue(cJSON_GetObjectItem(req, "on"));
	else if (!strcmp(what, "repeat")) { const char *m = cJSON_GetStringValue(cJSON_GetObjectItem(req, "mode")); pl.repeat = m && !strcmp(m, "all") ? 1 : m && !strcmp(m, "one") ? 2 : 0; }
	else if (!strcmp(what, "jump")) { int i = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(req, "index")); if (i >= 0 && i < pl.n) { pl.cur = i; pl.state = 1; pl.gen++; } }
	else { snprintf(err, errn, "do: pause resume toggle stop next previous seek volume shuffle repeat jump"); rc = -1; }
	pthread_cond_broadcast(&wake);
	pthread_mutex_unlock(&lk);
	return rc;
}

void music_stop(void)
{
	pthread_mutex_lock(&lk);
	if (pl.state) { pl.state = 0; pl.gen++; pthread_cond_broadcast(&wake); }
	pthread_mutex_unlock(&lk);
}

cJSON *music_status(void)
{
	cJSON *o = cJSON_CreateObject();
	pthread_mutex_lock(&lk);
	static const char *const ST[] = { "stopped", "playing", "paused" };
	cJSON_AddStringToObject(o, "state", ST[pl.state]);
	cJSON_AddBoolToObject(o, "shuffle", pl.shuffle);
	cJSON_AddStringToObject(o, "repeat", pl.repeat == 1 ? "all" : pl.repeat == 2 ? "one" : "off");
	cJSON_AddNumberToObject(o, "volume", pl.volume);
	if (pl.error[0]) cJSON_AddStringToObject(o, "error", pl.error);
	if (pl.state && pl.cur >= 0 && pl.cur < pl.n) {
		qitem_t *it = &pl.q[pl.cur];
		cJSON *t = cJSON_AddObjectToObject(o, "track");
		cJSON_AddStringToObject(t, "title", it->title);
		cJSON_AddStringToObject(t, "album", it->album);
		cJSON_AddStringToObject(t, "artist", it->artist);
		if (it->cover[0]) cJSON_AddStringToObject(t, "cover", it->cover);
		cJSON_AddBoolToObject(t, "cd", it->cd_track > 0);
		cJSON_AddNumberToObject(t, "index", pl.cur);
		cJSON_AddNumberToObject(t, "count", pl.n);
		cJSON_AddNumberToObject(t, "pos_ms", (double)pl.pos_ms);
		cJSON_AddNumberToObject(t, "len_ms", (double)pl.len_ms);
		if (pl.cur + 1 < pl.n) cJSON_AddStringToObject(t, "next", pl.q[pl.cur + 1].title);
	}
	cJSON *c = cd_json_locked();
	if (c) cJSON_AddItemToObject(o, "cd", c);
	pthread_mutex_unlock(&lk);
	return o;
}

int music_playing(void) { return pl.state != 0; }

/* the queue (the phone's "up next") */
cJSON *music_queue(void)
{
	cJSON *a = cJSON_CreateArray();
	pthread_mutex_lock(&lk);
	for (int i = 0; i < pl.n; i++) {
		cJSON *x = cJSON_CreateObject();
		cJSON_AddStringToObject(x, "title", pl.q[i].title);
		cJSON_AddBoolToObject(x, "current", i == pl.cur);
		cJSON_AddItemToArray(a, x);
	}
	pthread_mutex_unlock(&lk);
	return a;
}

void music_init(void) { srand((unsigned)time(NULL)); }

/* every couple of seconds from phosphord's loop: a game started (stop), and a music CD put in or taken out
   (Main's "disc" says what's in the drive; the TOC is read only when that changes) */
void music_tick(const char *drive_key, int audio_cd)
{
	char core[64] = "";
	FILE *f = fopen(p_corename(), "r");
	if (f) { if (!fgets(core, sizeof(core), f)) core[0] = 0; fclose(f); }
	core[strcspn(core, "\r\n")] = 0;
	if (core[0] && strcmp(core, "MENU") && pl.state) music_stop();
	static char last[128];
	if (!drive_key || !strcmp(drive_key, last)) return;
	snprintf(last, sizeof(last), "%s", drive_key);
	if (audio_cd) music_cd_check();
	else { pthread_mutex_lock(&lk); if (cd.present) memset(&cd, 0, sizeof(cd)); pthread_mutex_unlock(&lk); }
}
