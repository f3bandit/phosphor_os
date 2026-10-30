/* phosphord: saving a copy of the game disc in the dock's drive (Disc > In the drive > Save a copy).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Phosphor's own implementation, from the standards rather than anyone's dumper: the table of contents
 * (Linux's CDROMREADTOC*), raw 2352-byte sectors read with MMC's READ CD (SG_IO, opcode 0xBE), each
 * data sector checked against its own EDC (ECMA-130: CRC-32, polynomial 0x8001801B, over the sync and
 * header for Mode 1, over the subheader and data for Mode 2), audio read twice until two reads agree.
 * Where each track's pregap starts comes from the disc's subchannel Q (track and index numbers), so
 * the files are laid out the way Redump's are; the usual gaps when the drive won't give Q.
 *
 * The copy: Redump's layout, a .cue and a .bin per track ("Name (Track 1).bin"), in the system's
 * folder (games/PSX, games/Saturn, games/MegaCD, games/TGFX16-CD) on the drive that already has that
 * system's games (else the one with the most room). The data track's CRC-32 is looked up in Redump's
 * list of good dumps (libretro-database's metadat/redump, fetched once into data/redump): a match
 * means an exact copy, and the files take Redump's name. Then Look for games adds it to the library.
 * Nothing is kept from a copy that was stopped or failed. */
#include "dump.h"
#include "disc.h"
#include "indexer.h"
#include "log.h"
#include "music.h"
#include "net.h"
#include "paths.h"
#include "systems.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <pthread.h>
#include <scsi/sg.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

#define CD_DEV "/dev/sr0"
#define SECTOR 2352
#define BATCH 16                                /* sectors per read */
#define MAX_TRACKS 99

/* the disc types Phosphor's library has (disc.c lib_sys), their games folder and Redump list */
static const struct { const char *type, *sys, *folder, *dat; } KINDS[] = {
	{ "psx",    "PSX",      "PSX",       "Sony - PlayStation" },
	{ "saturn", "Saturn",   "Saturn",    "Sega - Saturn" },
	{ "megacd", "MegaCD",   "MegaCD",    "Sega - Mega-CD - Sega CD" },
	{ "pcecd",  "TGFX16CD", "TGFX16-CD", "NEC - PC Engine CD - TurboGrafx-CD" },
};
#define NKINDS ((int)(sizeof(KINDS) / sizeof(KINDS[0])))

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static struct {
	int busy;
	volatile int stop;                  /* (set by dump_stop, read by the copier without the lock) */
	char state[16];                              /* "" | working | done | failed | stopped */
	char type[16];                               /* Main's disc type: psx, saturn, megacd, pcecd */
	char msg[300], title[200], system[48], dest[700], cue[1100];
	int track, of, data;                         /* the track being read; data: it's a data track */
	long long done, total;                       /* sectors */
	int rereads, bad;                            /* sectors read again; sectors never read cleanly */
	int verified;                                /* 1 matches Redump, 0 not listed, -1 not checked */
	time_t t0, t1;
} cp;

static void setmsg(const char *st, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void setmsg(const char *st, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	pthread_mutex_lock(&lk);
	if (st) snprintf(cp.state, sizeof(cp.state), "%s", st);
	vsnprintf(cp.msg, sizeof(cp.msg), fmt, ap);
	pthread_mutex_unlock(&lk);
	va_end(ap);
}

/* ---------------------------------------------------------------- checksums */

static uint32_t crc_t[256], edc_t[256];
static void tables(void)
{
	if (crc_t[1]) return;
	for (uint32_t i = 0; i < 256; i++) {
		uint32_t c = i, e = i;
		for (int k = 0; k < 8; k++) {
			c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;      /* CRC-32 (zip, Redump's lists) */
			e = e & 1 ? 0xD8018001u ^ (e >> 1) : e >> 1;      /* ECMA-130's EDC, reflected */
		}
		crc_t[i] = c;
		edc_t[i] = e;
	}
}
static uint32_t crc32_add(uint32_t crc, const uint8_t *p, size_t n)
{
	crc = ~crc;
	while (n--) crc = crc_t[(crc ^ *p++) & 0xff] ^ (crc >> 8);
	return ~crc;
}
static uint32_t edc(const uint8_t *p, size_t n)
{
	uint32_t e = 0;
	while (n--) e = (e >> 8) ^ edc_t[(e ^ *p++) & 0xff];
	return e;
}
static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* a raw data sector's own check: 1 good, 0 bad (a wrong EDC, or no sync: a data track's sectors all have
   one), -1 can't tell (Mode 2 Form 2 without EDC, Mode 0) */
static int sector_ok(const uint8_t *s)
{
	static const uint8_t sync[12] = { 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0 };
	if (memcmp(s, sync, 12)) return 0;
	int mode = s[15];
	if (mode == 1) return edc(s, 2064) == le32(s + 2064);
	if (mode == 2) {
		if (s[18] & 0x20) {                                       /* Form 2: EDC optional */
			uint32_t want = le32(s + 2348);
			return want ? edc(s + 16, 2332) == want : -1;
		}
		return edc(s + 16, 2056) == le32(s + 2072);
	}
	return -1;                                                    /* Mode 0: zeros */
}

/* ---------------------------------------------------------------- the drive */

/* READ CD (MMC 0xBE): n raw sectors from lba (sync, headers, data, EDC/ECC: 2352 bytes each), or with
   sub 2 the formatted subchannel Q (16 bytes each) instead */
static int read_cd(int fd, int lba, int n, uint8_t *buf, int sub)
{
	uint8_t cdb[12] = { 0xBE, 0, (uint8_t)(lba >> 24), (uint8_t)(lba >> 16), (uint8_t)(lba >> 8), (uint8_t)lba,
	                    (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n, sub ? 0x00 : 0xF8, sub ? 0x02 : 0x00, 0 };
	uint8_t sense[32];
	sg_io_hdr_t io = { 0 };
	io.interface_id = 'S';
	io.dxfer_direction = SG_DXFER_FROM_DEV;
	io.cmd_len = sizeof(cdb);
	io.cmdp = cdb;
	io.dxferp = buf;
	io.dxfer_len = (unsigned)n * (sub ? 16 : SECTOR);
	io.sbp = sense;
	io.mx_sb_len = sizeof(sense);
	io.timeout = 30000;
	if (ioctl(fd, SG_IO, &io) < 0) return -1;
	if (io.host_status || (io.driver_status & ~0x08)) return -1;      /* (0x08: sense data came back) */
	if (io.status) {
		int key = -1;
		if (io.sb_len_wr >= 3 && (sense[0] & 0x7e) == 0x70) key = sense[2] & 0x0f;      /* fixed format */
		else if (io.sb_len_wr >= 2 && (sense[0] & 0x7e) == 0x72) key = sense[1] & 0x0f; /* descriptor */
		if (key == 2 || key == 6) return -2;                 /* not ready / medium changed: the disc is gone */
		if (key != 1) return -1;                             /* (1: recovered, the data is good) */
	}
	return io.resid ? -1 : 0;                                /* a short read: not all of it came */
}

typedef struct { int first, last, lba[MAX_TRACKS + 2], data[MAX_TRACKS + 2], leadout; } toc_t;

static int read_toc(int fd, toc_t *t)
{
	struct cdrom_tochdr h;
	if (ioctl(fd, CDROMREADTOCHDR, &h) || h.cdth_trk0 < 1 || h.cdth_trk1 < h.cdth_trk0 || h.cdth_trk1 > MAX_TRACKS) return -1;
	t->first = h.cdth_trk0;
	t->last = h.cdth_trk1;
	for (int i = t->first; i <= t->last + 1; i++) {
		struct cdrom_tocentry e = { 0 };
		e.cdte_track = i > t->last ? CDROM_LEADOUT : i;
		e.cdte_format = CDROM_LBA;
		if (ioctl(fd, CDROMREADTOCENTRY, &e)) return -1;
		if (i > t->last) t->leadout = e.cdte_addr.lba;
		else { t->lba[i] = e.cdte_addr.lba; t->data[i] = (e.cdte_ctrl & CDROM_DATA_TRACK) != 0; }
	}
	return 0;
}

static int bcd(uint8_t b) { return (b >> 4) * 10 + (b & 15); }

/* where track i's pregap (index 0) starts: the subchannel's track/index numbers just before its
   index 1. Without them: Redump's usual gaps (2 s data to audio, 3 s audio to data on PC Engine CD) */
static int pregap_start(int fd, const toc_t *t, int i, const char *type)
{
	int idx1 = t->lba[i], look = 300;
	if (idx1 - look < t->lba[i - 1]) look = idx1 - t->lba[i - 1];
	uint8_t *q = look > 0 ? malloc((size_t)look * 16) : NULL;
	int start = -1;
	if (q && read_cd(fd, idx1 - look, look, q, 1) == 0) {
		int seen = 0;
		start = idx1;
		for (int k = look - 1; k >= 0; k--) {                /* back from index 1, while it's track i */
			const uint8_t *s = q + (size_t)k * 16;
			if ((s[0] & 0x0f) != 1) continue;               /* not position data (ADR 1): skip */
			seen = 1;
			if (bcd(s[1]) == i && bcd(s[2]) == 0) start = idx1 - look + k;
			else break;
		}
		if (!seen) start = -1;
	}
	free(q);
	/* a few sectors off Redump's usual 2 s / 3 s gap: the subchannel is often read a little out of step
	   with the sectors (and the data track's size must be exact to match) */
	if (start >= 0) for (int g = 150; g <= 225; g += 75) if (abs((idx1 - start) - g) <= 4 && idx1 - g >= t->lba[i - 1]) start = idx1 - g;
	if (start >= 0) return start;
	if (t->data[i - 1] && !t->data[i]) return idx1 - 150;
	if (!t->data[i - 1] && t->data[i] && !strcmp(type, "pcecd")) return idx1 - 225;
	return idx1;
}

/* n sectors, read until they're right: data sectors until their EDC agrees, audio until two reads
   agree. Returns the sectors that never came out clean */
/* -1: the disc is gone (taken out, the drive not ready): the copy stops */
static int far_lba = 0;                                     /* (a sector far away: the drive's cache emptied) */
static void defeat_cache(int fd, int lba)
{
	static uint8_t junk[SECTOR];
	read_cd(fd, lba > 30000 ? lba - 30000 : lba + 30000 < far_lba ? lba + 30000 : 0, 1, junk, 0);
}
static int read_good(int fd, int lba, int n, int data, uint8_t *a, uint8_t *b)
{
	int bad = 0, r = read_cd(fd, lba, n, a, 0);
	if (r == -2) return -1;
	if (r != 0) {                                           /* the batch failed: one sector at a time */
		for (int k = 0; k < n; k++) {
			int ok = 0;
			for (int tries = 0; tries < 5 && !ok; tries++) {
				int rr = read_cd(fd, lba + k, 1, a + (size_t)k * SECTOR, 0);
				if (rr == -2) return -1;
				ok = rr == 0;
				if (!ok) { pthread_mutex_lock(&lk); cp.rereads++; pthread_mutex_unlock(&lk); }
			}
			if (!ok) { memset(a + (size_t)k * SECTOR, 0, SECTOR); bad++; }
		}
		if (!data) return bad;
	}
	if (data) {
		for (int k = 0; k < n; k++) {
			uint8_t *s = a + (size_t)k * SECTOR;
			for (int tries = 0; tries < 5 && sector_ok(s) == 0; tries++) {
				pthread_mutex_lock(&lk); cp.rereads++; pthread_mutex_unlock(&lk);
				if (read_cd(fd, lba + k, 1, s, 0) == -2) return -1;
			}
			if (sector_ok(s) == 0) bad++;
		}
		return bad;
	}
	for (int tries = 0; tries < 8; tries++) {               /* audio: two reads that agree (not the drive's cache) */
		defeat_cache(fd, lba);
		int rr = read_cd(fd, lba, n, b, 0);
		if (rr == -2) return -1;
		if (rr != 0) continue;
		if (!memcmp(a, b, (size_t)n * SECTOR)) return bad;
		pthread_mutex_lock(&lk); cp.rereads++; pthread_mutex_unlock(&lk);
		memcpy(a, b, (size_t)n * SECTOR);
	}
	return bad + n;                                          /* never agreed: kept the last read */
}

/* ---------------------------------------------------------------- where it goes */

static void clean_name(const char *in, char *out, size_t n)
{
	size_t j = 0;
	for (; *in && j + 1 < n; in++) {
		unsigned char c = (unsigned char)*in;
		out[j++] = (c < 32 || strchr("/\\*?\"<>|", c)) ? '_' : c == ':' ? '-' : (char)c;
	}
	if (*in) while (j && ((unsigned char)out[j - 1] & 0xC0) == 0x80) j--;        /* cut short: not mid-character */
	if (*in && j && ((unsigned char)out[j - 1] & 0xC0) == 0xC0) j--;
	while (j && (out[j - 1] == ' ' || out[j - 1] == '.')) j--;
	out[j] = 0;
	if (out[0] == '.') out[0] = '_';                        /* (a hidden name: the clean-up skips those) */
	if (!out[0]) snprintf(out, n, "Disc");
}

static int count_dir(const char *d)
{
	DIR *dd = opendir(d);
	int n = 0;
	struct dirent *e;
	while (dd && (e = readdir(dd))) if (e->d_name[0] != '.') n++;
	if (dd) closedir(dd);
	return n;
}

static long long free_bytes(const char *p)
{
	struct statvfs v;
	return statvfs(p, &v) ? -1 : (long long)v.f_bavail * (long long)v.f_frsize;
}

/* the games root to copy to: the one that already has the most of this system's games and room for
   it, else the one with the most room */
static int pick_root(const char *folder, long long need, char *out, size_t n)
{
	char roots[16][256];
	int nr = p_roots(roots, 14), best = -1, best_n = -1;
	long long best_free = -1;
	for (int r = 0; r < nr; r++) {
		struct stat st;
		if (stat(roots[r], &st) || !S_ISDIR(st.st_mode)) continue;
		if (p_root_is_repair(roots[r])) continue;          /* (never onto the repair stick) */
		long long fr = free_bytes(roots[r]);
		if (fr < need) continue;
		char d[600];
		snprintf(d, sizeof(d), "%.300s/%s", roots[r], folder);
		int c = count_dir(d);
		if (c > best_n || (c == best_n && fr > best_free)) { best = r; best_n = c; best_free = fr; }
	}
	if (best < 0) return -1;
	snprintf(out, n, "%s", roots[best]);
	return 0;
}

/* ---------------------------------------------------------------- Redump */

/* the Redump game whose data track has this CRC and size: its name, or "" */
static void redump_lookup(const char *dat, uint32_t crc, long long size, char *name, size_t n)
{
	name[0] = 0;
	char dir[600], path[800], url[400], q[200];
	snprintf(dir, sizeof(dir), "%s/redump", p_data());
	mkdirs(dir);
	snprintf(path, sizeof(path), "%s/%s.dat", dir, dat);
	struct stat st;
	if (stat(path, &st) || time(NULL) - st.st_mtime > 30 * 86400) {         /* fetched once a month at most */
		net_escape(dat, q, sizeof(q));
		snprintf(url, sizeof(url), "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/redump/%s.dat", q);
		if (net_get_file(url, path) != 0) plog("disc copy: couldn't get Redump's list for %s", dat);
	}
	FILE *f = fopen(path, "r");
	if (!f) return;
	char line[2048], game[400] = "", want[16];
	snprintf(want, sizeof(want), "crc %08X ", crc);
	while (fgets(line, sizeof(line), f)) {
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (!strncmp(p, "name \"", 6)) {                              /* the game's name (before its roms) */
			char *e = strchr(p + 6, '"');
			if (e) { *e = 0; snprintf(game, sizeof(game), "%s", p + 6); }
		} else if (!strncmp(p, "rom (", 5) && strstr(p, want)) {
			char *s = strstr(p, " size ");
			if (s && strtoll(s + 6, NULL, 10) == size) { snprintf(name, n, "%s", game); break; }
		}
	}
	fclose(f);
}

/* ---------------------------------------------------------------- the copy */

typedef struct { int start, end, data; char file[330]; uint32_t crc; int idx1_off; } part_t;

static void track_file(char *out, size_t n, const char *name, int i, int ntracks)
{
	if (ntracks == 1) snprintf(out, n, "%s.bin", name);
	else snprintf(out, n, ntracks >= 10 ? "%s (Track %02d).bin" : "%s (Track %d).bin", name, i);
}

static void msf(int lba, char *out, size_t n) { snprintf(out, n, "%02d:%02d:%02d", lba / 4500, lba / 75 % 60, lba % 75); }

static int write_cue(const char *path, const char *name, const part_t *pt, const toc_t *t, const int *mode)
{
	FILE *f = fopen(path, "w");
	if (!f) return -1;
	int nt = t->last - t->first + 1;
	for (int i = t->first; i <= t->last; i++) {
		char file[320], m[16];
		track_file(file, sizeof(file), name, i, nt);
		fprintf(f, "FILE \"%s\" BINARY\r\n", file);
		fprintf(f, "  TRACK %02d %s\r\n", i, pt[i].data ? (mode[i] == 1 ? "MODE1/2352" : "MODE2/2352") : "AUDIO");
		if (pt[i].idx1_off) {
			msf(pt[i].idx1_off, m, sizeof(m));
			fprintf(f, "    INDEX 00 00:00:00\r\n    INDEX 01 %s\r\n", m);
		} else fprintf(f, "    INDEX 01 00:00:00\r\n");
	}
	return fclose(f);
}

/* .phosphor-copy in a copy's folder until it's finished: a copy cut off (power, a crash) is found and
   thrown away the next time phosphord starts (dump_cleanup), never a folder Phosphor didn't make */
#define COPY_MARK ".phosphor-copy"
static void mark_copy(const char *dir, int on)
{
	char p[800];
	snprintf(p, sizeof(p), "%s/" COPY_MARK, dir);
	if (!on) { unlink(p); return; }
	FILE *f = fopen(p, "w");
	if (f) { fputs("an unfinished disc copy: removed when Phosphor starts\n", f); fclose(f); }
}

static void remove_copy(const char *dir)
{
	mark_copy(dir, 0);
	DIR *d = opendir(dir);
	struct dirent *e;
	char p[1200];
	while (d && (e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
		unlink(p);
	}
	if (d) closedir(d);
	rmdir(dir);
}

static void *copier(void *arg)
{
	(void)arg;
	tables();
	char type[16], title[200], name[200] = "", root[300] = "", dir[700] = "", err[200] = "";
	int made = 0;                                            /* the folder was made: remove it if it fails */
	pthread_mutex_lock(&lk);
	snprintf(type, sizeof(type), "%s", cp.type);
	snprintf(title, sizeof(title), "%s", cp.title);
	pthread_mutex_unlock(&lk);
	int kind = -1;
	for (int i = 0; i < NKINDS; i++) if (!strcmp(type, KINDS[i].type)) kind = i;
	if (kind < 0) kind = 0;                                  /* (dump_start only starts the four) */
	const gsys_t *g = system_by_id(KINDS[kind].sys);
	(void)g;

	int fd = open(CD_DEV, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	toc_t t = { 0 };
	part_t pt[MAX_TRACKS + 2] = { 0 };
	int mode[MAX_TRACKS + 2] = { 0 }, ok = 0;
	uint8_t *a = malloc((size_t)BATCH * SECTOR), *b = malloc((size_t)BATCH * SECTOR);
	if (fd < 0 || !a || !b) snprintf(err, sizeof(err), "The drive couldn't be opened");
	else if (read_toc(fd, &t)) snprintf(err, sizeof(err), "The disc's table of contents couldn't be read");
	if (!err[0]) {
		ioctl(fd, CDROM_SELECT_SPEED, 0);                 /* as fast as the drive goes */
		/* each track's file: from its pregap to the next one's */
		for (int i = t.first; i <= t.last; i++) {
			pt[i].data = t.data[i];
			pt[i].start = i == t.first ? t.lba[i] : pregap_start(fd, &t, i, type);
			pt[i].idx1_off = t.lba[i] - pt[i].start;
		}
		for (int i = t.first; i <= t.last; i++) pt[i].end = i < t.last ? pt[i + 1].start : t.leadout;
		far_lba = t.leadout;
		for (int i = t.first; i <= t.last; i++) {
			mode[i] = !strcmp(type, "psx") ? 2 : 1;        /* (unread: what the system's discs are) */
			for (int tries = 0; t.data[i] && tries < 3; tries++)
				if (read_cd(fd, t.lba[i], 1, a, 0) == 0 && (a[15] == 1 || a[15] == 2)) { mode[i] = a[15]; break; }
		}
		long long need = (long long)(t.leadout - pt[t.first].start) * SECTOR + (64 << 20);
		clean_name(title, name, sizeof(name));
		if (pick_root(KINDS[kind].folder, need, root, sizeof(root)))
			snprintf(err, sizeof(err), "No drive has room for it (%lld MB)", need >> 20);
	}
	if (!err[0]) {
		snprintf(dir, sizeof(dir), "%s/%s/%s", root, KINDS[kind].folder, name);
		struct stat st;
		for (int k = 2; !stat(dir, &st) && k < 100; k++)  /* a copy already there: another name */
			snprintf(dir, sizeof(dir), "%s/%s/%s (%d)", root, KINDS[kind].folder, name, k);
		char parent[700];
		snprintf(parent, sizeof(parent), "%s/%s", root, KINDS[kind].folder);
		mkdirs(parent);
		/* a folder of its own, new (never one already there: a failed copy removes what's in it) */
		if (mkdir(dir, 0755)) snprintf(err, sizeof(err), errno == EEXIST ? "Too many copies of it already" : "Couldn't make the folder on the drive");
		else { made = 1; mark_copy(dir, 1); }               /* (unfinished: cleared at the end; see dump_cleanup) */
	}
	if (!err[0]) {
		pthread_mutex_lock(&lk);
		snprintf(cp.dest, sizeof(cp.dest), "%s", dir);
		cp.total = t.leadout - pt[t.first].start;
		cp.of = t.last - t.first + 1;
		pthread_mutex_unlock(&lk);
		plog("disc copy: %s (%s), %d tracks, %d sectors, into %s", title, type, t.last - t.first + 1, t.leadout, dir);
		int nt = t.last - t.first + 1;
		for (int i = t.first; i <= t.last && !err[0] && !cp.stop; i++) {
			char file[320], path[1100], part[1110];
			track_file(file, sizeof(file), name, i, nt);
			snprintf(path, sizeof(path), "%s/%s", dir, file);
			snprintf(part, sizeof(part), "%s.part", path);
			pthread_mutex_lock(&lk);
			cp.track = i - t.first + 1;
			cp.data = pt[i].data;
			snprintf(cp.msg, sizeof(cp.msg), "Track %d of %d \xC2\xB7 %s", cp.track, cp.of, pt[i].data ? "data" : "music");
			pthread_mutex_unlock(&lk);
			FILE *f = fopen(part, "wb");
			if (!f) { snprintf(err, sizeof(err), "Couldn't write to the drive"); break; }
			uint32_t crc = 0;
			for (int lba = pt[i].start; lba < pt[i].end && !cp.stop; lba += BATCH) {
				int n = pt[i].end - lba < BATCH ? pt[i].end - lba : BATCH;
				int bad = read_good(fd, lba, n, pt[i].data, a, b);
				if (bad < 0) { snprintf(err, sizeof(err), "The disc was taken out or the drive stopped answering"); break; }
				if (cp.bad + bad > 2000) { snprintf(err, sizeof(err), "Too much of the disc can't be read: clean it and try again"); break; }
				if (fwrite(a, SECTOR, (size_t)n, f) != (size_t)n) { snprintf(err, sizeof(err), "The drive it's copying to is full or failing"); break; }
				crc = crc32_add(crc, a, (size_t)n * SECTOR);
				pthread_mutex_lock(&lk);
				cp.done += n;
				cp.bad += bad;
				pthread_mutex_unlock(&lk);
			}
			if (fclose(f) && !err[0]) snprintf(err, sizeof(err), "The drive it's copying to is full or failing");
			pt[i].crc = crc;
			snprintf(pt[i].file, sizeof(pt[i].file), "%s", file);
			if (err[0] || cp.stop) break;
			rename(part, path);
		}
		if (!err[0] && !cp.stop) {
			char cue[1100];
			snprintf(cue, sizeof(cue), "%s/%s.cue", dir, name);
			if (write_cue(cue, name, pt, &t, mode)) snprintf(err, sizeof(err), "Couldn't write the .cue");
			ok = !err[0];
		}
	}
	if (fd >= 0) { ioctl(fd, CDROM_SELECT_SPEED, 4); close(fd); }   /* quiet again */
	free(a); free(b);

	if (!ok) {
		if (made) remove_copy(dir);
		pthread_mutex_lock(&lk);
		if (cp.stop) { snprintf(cp.state, sizeof(cp.state), "stopped"); snprintf(cp.msg, sizeof(cp.msg), "Stopped: the unfinished copy was thrown away"); }
		else { snprintf(cp.state, sizeof(cp.state), "failed"); snprintf(cp.msg, sizeof(cp.msg), "%s: nothing was kept", err[0] ? err : "The copy didn't finish"); }
		cp.dest[0] = 0;
		char m2[sizeof(cp.msg)];
		snprintf(m2, sizeof(m2), "%s", cp.msg);
		cp.t1 = time(NULL);
		cp.busy = 0;
		pthread_mutex_unlock(&lk);
		plog("disc copy: %s", m2);
		return NULL;
	}

	mark_copy(dir, 0);                                       /* finished: a copy like any other now */
	/* checked against Redump: the first data track (the one Redump's lists have) */
	setmsg(NULL, "Checking the copy against Redump's list...");
	int dt = -1;
	for (int i = t.first; i <= t.last && dt < 0; i++) if (pt[i].data) dt = i;
	char rname[400] = "";
	if (dt >= 0) redump_lookup(KINDS[kind].dat, pt[dt].crc, (long long)(pt[dt].end - pt[dt].start) * SECTOR, rname, sizeof(rname));
	int verified = rname[0] ? 1 : 0;
	/* a match: Redump's name for the files and the folder (when that name is free) */
	char final_dir[700], cue_path[1100];
	snprintf(final_dir, sizeof(final_dir), "%s", dir);
	char rn[200];
	clean_name(rname, rn, sizeof(rn));
	if (verified && strcmp(rn, name)) {
		char nd[700];
		struct stat st;
		snprintf(nd, sizeof(nd), "%s/%s/%s", root, KINDS[kind].folder, rn);
		if (stat(nd, &st) != 0) {
			int nt = t.last - t.first + 1, moved = 1;
			for (int i = t.first; i <= t.last; i++) {
				char from[1100], to[1100], f2[320];
				track_file(f2, sizeof(f2), rn, i, nt);
				snprintf(from, sizeof(from), "%s/%s", dir, pt[i].file);
				snprintf(to, sizeof(to), "%s/%s", dir, f2);
				if (rename(from, to)) moved = 0;
			}
			char oldcue[1100];
			snprintf(oldcue, sizeof(oldcue), "%s/%s.cue", dir, name);
			unlink(oldcue);
			snprintf(cue_path, sizeof(cue_path), "%s/%s.cue", dir, rn);
			write_cue(cue_path, rn, pt, &t, mode);
			if (moved && rename(dir, nd) == 0) snprintf(final_dir, sizeof(final_dir), "%s", nd);
			snprintf(name, sizeof(name), "%s", rn);
		}
	}
	snprintf(cue_path, sizeof(cue_path), "%s/%s.cue", final_dir, name);
	indexer_request();                                      /* into the library (the listing is quick) */
	pthread_mutex_lock(&lk);
	cp.verified = verified;
	snprintf(cp.state, sizeof(cp.state), "done");
	snprintf(cp.dest, sizeof(cp.dest), "%s", final_dir);
	snprintf(cp.cue, sizeof(cp.cue), "%s", cue_path);
	if (verified) snprintf(cp.title, sizeof(cp.title), "%s", rname);
	cp.t1 = time(NULL);
	snprintf(cp.msg, sizeof(cp.msg), "%s%s",
	         verified ? (t.last > t.first ? "Copy saved: its data track matches Redump's" : "Copy saved: it matches Redump, an exact copy of the disc")
	                  : "Copy saved: Redump doesn't list this disc, so it can't be checked",
	         cp.bad ? " (some spots never read cleanly: clean the disc and copy it again)" : "");
	long took = (long)(cp.t1 - cp.t0);
	int rr = cp.rereads, bd = cp.bad;
	cp.busy = 0;
	pthread_mutex_unlock(&lk);
	plog("disc copy: done in %lds, %s, %d re-reads, %d bad, %s", took, verified ? "matches Redump" : "not in Redump's list", rr, bd, cue_path);
	return NULL;
}

/* ---------------------------------------------------------------- the API */

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

void dump_cleanup(void)
{
	char roots[16][256];
	int nr = p_roots(roots, 14);
	for (int r = 0; r < nr; r++)
		for (int k = 0; k < NKINDS; k++) {
			char sd[600];
			snprintf(sd, sizeof(sd), "%.300s/%s", roots[r], KINDS[k].folder);
			DIR *d = opendir(sd);
			struct dirent *e;
			while (d && (e = readdir(d))) {
				if (e->d_name[0] == '.') continue;
				char dir[900], m[1000];
				struct stat st;
				snprintf(dir, sizeof(dir), "%s/%s", sd, e->d_name);
				snprintf(m, sizeof(m), "%s/" COPY_MARK, dir);
				if (stat(m, &st) == 0) { plog("disc copy: removing an unfinished copy, %s", dir); remove_copy(dir); }
			}
			if (d) closedir(d);
		}
}

int dump_supported(const char *type)
{
	for (int i = 0; type && i < NKINDS; i++) if (!strcmp(type, KINDS[i].type)) return 1;
	return 0;
}

int dump_start(sqlite3 *db, char *err, size_t errn)
{
	cJSON *d = disc_json(db);
	const char *state = d ? cJSON_GetStringValue(cJSON_GetObjectItem(d, "state")) : NULL;
	const char *type = d ? cJSON_GetStringValue(cJSON_GetObjectItem(d, "type")) : NULL;
	const char *title = d ? cJSON_GetStringValue(cJSON_GetObjectItem(d, "title")) : NULL;
	int kind = -1;
	for (int i = 0; type && i < NKINDS; i++) if (!strcmp(type, KINDS[i].type)) kind = i;
	const char *why = !d ? "The drive isn't answering"
	                : !state || strcmp(state, "ready") ? "There's no game disc in the drive"
	                : kind < 0 ? "Copies can be saved of PlayStation, Saturn, Mega-CD and PC Engine CD discs"
	                : !menu_up() ? "Not while a game runs"
	                : music_playing() ? "Stop the music first"
	                : NULL;
	if (!why) {
		cJSON *r = music_rip_status();
		if (cJSON_IsTrue(cJSON_GetObjectItem(r, "busy"))) why = "A music CD is being copied";
		cJSON_Delete(r);
	}
	pthread_mutex_lock(&lk);
	if (!why && cp.busy) why = "A copy is already being saved";
	if (why) {
		pthread_mutex_unlock(&lk);
		snprintf(err, errn, "%s", why);
		cJSON_Delete(d);
		return -1;
	}
	memset(&cp, 0, sizeof(cp));
	cp.busy = 1;
	cp.verified = -1;
	cp.t0 = time(NULL);
	snprintf(cp.state, sizeof(cp.state), "working");
	snprintf(cp.msg, sizeof(cp.msg), "Reading the disc...");
	snprintf(cp.title, sizeof(cp.title), "%s", title && *title ? title : "Disc");
	snprintf(cp.type, sizeof(cp.type), "%s", type);
	snprintf(cp.system, sizeof(cp.system), "%s", system_by_id(KINDS[kind].sys) ? system_by_id(KINDS[kind].sys)->name : "Disc");
	pthread_mutex_unlock(&lk);
	cJSON_Delete(d);
	pthread_t th;
	if (pthread_create(&th, NULL, copier, NULL) != 0) {
		pthread_mutex_lock(&lk);
		cp.busy = 0;
		snprintf(cp.state, sizeof(cp.state), "failed");
		pthread_mutex_unlock(&lk);
		snprintf(err, errn, "Couldn't start the copy");
		return -1;
	}
	pthread_detach(th);
	return 0;
}

void dump_stop(void)
{
	pthread_mutex_lock(&lk);
	if (cp.busy) cp.stop = 1;
	pthread_mutex_unlock(&lk);
}

int dump_busy(void)
{
	pthread_mutex_lock(&lk);
	int b = cp.busy;
	pthread_mutex_unlock(&lk);
	return b;
}

cJSON *dump_status(void)
{
	cJSON *o = cJSON_CreateObject();
	pthread_mutex_lock(&lk);
	cJSON_AddBoolToObject(o, "busy", cp.busy);
	cJSON_AddStringToObject(o, "state", cp.state);
	cJSON_AddStringToObject(o, "msg", cp.msg);
	cJSON_AddStringToObject(o, "title", cp.title);
	cJSON_AddStringToObject(o, "system", cp.system);
	if (cp.dest[0]) cJSON_AddStringToObject(o, "dest", cp.dest);
	if (cp.cue[0]) cJSON_AddStringToObject(o, "cue", cp.cue);
	cJSON_AddNumberToObject(o, "track", cp.track);
	cJSON_AddNumberToObject(o, "of", cp.of);
	cJSON_AddBoolToObject(o, "data", cp.data);
	cJSON_AddNumberToObject(o, "mb_done", (double)(cp.done * SECTOR >> 20));
	cJSON_AddNumberToObject(o, "mb_total", (double)(cp.total * SECTOR >> 20));
	cJSON_AddNumberToObject(o, "percent", cp.total ? (double)cp.done * 100 / (double)cp.total : 0);
	cJSON_AddNumberToObject(o, "rereads", cp.rereads);
	cJSON_AddNumberToObject(o, "bad", cp.bad);
	if (!strcmp(cp.state, "done")) cJSON_AddBoolToObject(o, "verified", cp.verified == 1);
	long secs = (long)((cp.busy ? time(NULL) : cp.t1) - cp.t0);
	cJSON_AddNumberToObject(o, "seconds", cp.t0 ? (double)secs : 0);
	/* speed in "x" (1x = 75 sectors a second) and the time left */
	if (cp.busy && secs > 2 && cp.done > 0) {
		double rate = (double)cp.done / (double)secs;
		cJSON_AddNumberToObject(o, "speed", rate / 75.0);
		cJSON_AddNumberToObject(o, "seconds_left", (double)(cp.total - cp.done) / rate);
	}
	pthread_mutex_unlock(&lk);
	return o;
}
