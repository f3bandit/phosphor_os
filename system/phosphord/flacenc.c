/* phosphord: a small FLAC encoder for ripping music CDs (16-bit stereo, 44.1 kHz).
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 *
 * Lossless and standard (any FLAC player reads it), just simpler than libFLAC: blocks of 4096 samples,
 * each channel coded with the best of FLAC's fixed predictors (order 0 to 4), mid/side or left/right,
 * whichever is smaller, and Rice-coded residuals with the best partition order and parameters.
 * Format: https://xiph.org/flac/format.html */
#include "flacenc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BS 4096

struct flacenc {
	FILE *f;
	int16_t buf[BS * 2];
	int n;                     /* frames buffered */
	uint32_t frame_no;
	uint64_t total;
	uint8_t *out;              /* one frame's bytes */
	size_t cap;
};

/* ---- bits */
typedef struct { uint8_t *p; size_t pos; uint32_t acc; int nacc; } bw_t;
static void bw_put(bw_t *b, uint32_t v, int bits)
{
	while (bits > 0) {
		int take = bits > 24 ? 24 : bits;
		uint32_t part = (v >> (bits - take)) & ((1u << take) - 1);
		b->acc = (b->acc << take) | part;
		b->nacc += take;
		bits -= take;
		while (b->nacc >= 8) { b->p[b->pos++] = (uint8_t)(b->acc >> (b->nacc - 8)); b->nacc -= 8; }
		b->acc &= (1u << b->nacc) - 1;
	}
}
static void bw_align(bw_t *b) { if (b->nacc) bw_put(b, 0, 8 - b->nacc); }
static void bw_unary(bw_t *b, uint32_t q) { while (q >= 24) { bw_put(b, 0, 24); q -= 24; } bw_put(b, 1, (int)q + 1); }

static uint8_t crc8(const uint8_t *d, size_t n)
{
	uint8_t c = 0;
	while (n--) { c ^= *d++; for (int i = 0; i < 8; i++) c = (uint8_t)(c & 0x80 ? (c << 1) ^ 0x07 : c << 1); }
	return c;
}
static uint16_t crc16(const uint8_t *d, size_t n)
{
	uint16_t c = 0;
	while (n--) { c ^= (uint16_t)(*d++ << 8); for (int i = 0; i < 8; i++) c = (uint16_t)(c & 0x8000 ? (c << 1) ^ 0x8005 : c << 1); }
	return c;
}

/* ---- residuals: fixed predictor of order o */
static void residual(const int32_t *x, int n, int o, int32_t *r)
{
	for (int i = o; i < n; i++) {
		switch (o) {
		case 0: r[i] = x[i]; break;
		case 1: r[i] = x[i] - x[i - 1]; break;
		case 2: r[i] = x[i] - 2 * x[i - 1] + x[i - 2]; break;
		case 3: r[i] = x[i] - 3 * x[i - 1] + 3 * x[i - 2] - x[i - 3]; break;
		default: r[i] = x[i] - 4 * x[i - 1] + 6 * x[i - 2] - 4 * x[i - 3] + x[i - 4]; break;
		}
	}
}
static uint32_t zz(int32_t v) { return (uint32_t)((v << 1) ^ (v >> 31)); }

/* bits for a partition with Rice parameter k */
static uint64_t rice_bits(const int32_t *r, int a, int b, int k)
{
	uint64_t bits = 0;
	for (int i = a; i < b; i++) bits += (zz(r[i]) >> k) + 1 + (uint64_t)k;
	return bits;
}
static int best_k(const int32_t *r, int a, int b, uint64_t *bits)
{
	uint64_t sum = 0;
	for (int i = a; i < b; i++) sum += zz(r[i]);
	int n = b - a, k0 = 0;
	if (n > 0) { uint64_t mean = sum / (uint64_t)n; while (k0 < 14 && (1ULL << (k0 + 1)) <= mean) k0++; }
	int best = k0;
	uint64_t bb = rice_bits(r, a, b, k0);
	for (int k = k0 > 0 ? k0 - 1 : 0; k <= k0 + 1 && k <= 14; k++) {
		uint64_t t = rice_bits(r, a, b, k);
		if (t < bb) { bb = t; best = k; }
	}
	*bits = bb;
	return best;
}

/* the cheapest fixed subframe for one channel: its order, partition order and size in bits */
typedef struct { int order, porder; uint64_t bits; int k[16]; } plan_t;
static plan_t plan(const int32_t *x, int n, int bps, int32_t *r)
{
	plan_t best = { 0, 0, ~0ULL, { 0 } };
	for (int o = 0; o <= 4 && o < n; o++) {
		residual(x, n, o, r);
		for (int po = 0; po <= 4; po++) {
			int parts = 1 << po;
			if (n % parts || n / parts <= o) break;
			uint64_t bits = 8 + (uint64_t)o * (uint64_t)bps + 6;
			int k[16];
			for (int p = 0; p < parts; p++) {
				int a = p == 0 ? o : p * (n / parts), b = (p + 1) * (n / parts);
				uint64_t pb;
				k[p] = best_k(r, a, b, &pb);
				bits += 4 + pb;
			}
			if (bits < best.bits) { best.order = o; best.porder = po; best.bits = bits; memcpy(best.k, k, sizeof(k)); }
		}
	}
	return best;
}

static void subframe(bw_t *b, const int32_t *x, int n, int bps, int32_t *r)
{
	plan_t p = plan(x, n, bps, r);
	/* verbatim when prediction doesn't help */
	if (p.bits >= (uint64_t)n * (uint64_t)bps + 8) {
		bw_put(b, 0x02, 8);                           /* 0 000001 0: verbatim */
		for (int i = 0; i < n; i++) bw_put(b, (uint32_t)x[i] & ((1u << bps) - 1), bps);
		return;
	}
	residual(x, n, p.order, r);
	bw_put(b, (uint32_t)(0x08 | p.order) << 1, 8);    /* 0 001ooo 0: fixed */
	for (int i = 0; i < p.order; i++) bw_put(b, (uint32_t)x[i] & ((1u << bps) - 1), bps);
	bw_put(b, 0, 2);                                  /* Rice, 4-bit parameters */
	bw_put(b, (uint32_t)p.porder, 4);
	int parts = 1 << p.porder;
	for (int q = 0; q < parts; q++) {
		int a = q == 0 ? p.order : q * (n / parts), e = (q + 1) * (n / parts), k = p.k[q];
		bw_put(b, (uint32_t)k, 4);
		for (int i = a; i < e; i++) {
			uint32_t u = zz(r[i]);
			bw_unary(b, u >> k);
			if (k) bw_put(b, u & ((1u << k) - 1), k);
		}
	}
}

static int write_frame(flacenc_t *e)
{
	int n = e->n;
	if (!n) return 0;
	static int32_t L[BS], R[BS], M[BS], S[BS], tmp[BS];
	for (int i = 0; i < n; i++) {
		L[i] = e->buf[i * 2]; R[i] = e->buf[i * 2 + 1];
		M[i] = (L[i] + R[i]) >> 1; S[i] = L[i] - R[i];
	}
	/* left/right or mid/side, whichever is smaller */
	uint64_t lr = plan(L, n, 16, tmp).bits + plan(R, n, 16, tmp).bits;
	uint64_t ms = plan(M, n, 16, tmp).bits + plan(S, n, 17, tmp).bits;
	int midside = ms < lr;
	size_t need = (size_t)n * 2 * 5 + 64;
	if (e->cap < need) { uint8_t *o = realloc(e->out, need); if (!o) return -1; e->out = o; e->cap = need; }
	bw_t b = { e->out, 0, 0, 0 };
	bw_put(&b, 0xFFF8, 16);                          /* sync, fixed block size */
	int bscode = n == BS ? 12 : 7;                   /* 12: 4096; 7: 16-bit (n-1) after the header */
	bw_put(&b, (uint32_t)bscode, 4);
	bw_put(&b, 9, 4);                                /* 44.1 kHz */
	bw_put(&b, midside ? 10 : 1, 4);                 /* mid/side, or two independent channels */
	bw_put(&b, 4, 3);                                /* 16 bits */
	bw_put(&b, 0, 1);
	/* frame number, UTF-8 style */
	uint32_t fn = e->frame_no;
	if (fn < 0x80) bw_put(&b, fn, 8);
	else if (fn < 0x800) { bw_put(&b, 0xC0 | (fn >> 6), 8); bw_put(&b, 0x80 | (fn & 0x3F), 8); }
	else if (fn < 0x10000) { bw_put(&b, 0xE0 | (fn >> 12), 8); bw_put(&b, 0x80 | ((fn >> 6) & 0x3F), 8); bw_put(&b, 0x80 | (fn & 0x3F), 8); }
	else { bw_put(&b, 0xF0 | (fn >> 18), 8); bw_put(&b, 0x80 | ((fn >> 12) & 0x3F), 8); bw_put(&b, 0x80 | ((fn >> 6) & 0x3F), 8); bw_put(&b, 0x80 | (fn & 0x3F), 8); }
	if (bscode == 7) bw_put(&b, (uint32_t)(n - 1), 16);
	uint8_t c8 = crc8(e->out, b.pos);
	bw_put(&b, c8, 8);
	if (midside) { subframe(&b, M, n, 16, tmp); subframe(&b, S, n, 17, tmp); }
	else { subframe(&b, L, n, 16, tmp); subframe(&b, R, n, 16, tmp); }
	bw_align(&b);
	uint16_t c16 = crc16(e->out, b.pos);
	bw_put(&b, c16, 16);
	if (fwrite(e->out, 1, b.pos, e->f) != b.pos) return -1;
	e->frame_no++;
	e->total += (uint64_t)n;
	e->n = 0;
	return 0;
}

static void le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

flacenc_t *flacenc_open(const char *path, const char *const *tags)
{
	flacenc_t *e = calloc(1, sizeof(*e));
	if (!e) return NULL;
	e->f = fopen(path, "w+b");               /* (read back at the end: the sample count) */
	if (!e->f) { free(e); return NULL; }
	fwrite("fLaC", 1, 4, e->f);
	/* STREAMINFO; the sample count goes in when it's known (flacenc_close) */
	uint8_t si[4 + 34] = { 0x00, 0, 0, 34 };
	uint8_t *s = si + 4;
	s[0] = BS >> 8; s[1] = BS & 0xFF; s[2] = BS >> 8; s[3] = BS & 0xFF;     /* min/max block size (the last may be shorter) */
	/* min/max frame size 0 (unknown); 44100 Hz, 2 channels, 16 bits, samples, MD5 0 (unknown) */
	uint32_t sr = 44100;
	s[10] = (uint8_t)(sr >> 12); s[11] = (uint8_t)(sr >> 4);
	s[12] = (uint8_t)(((sr & 0xF) << 4) | ((2 - 1) << 1) | ((16 - 1) >> 4));
	s[13] = (uint8_t)(((16 - 1) & 0xF) << 4);
	fwrite(si, 1, sizeof(si), e->f);
	/* VORBIS_COMMENT (last block): the tags */
	const char *vendor = "Phosphor";
	size_t len = 4 + strlen(vendor) + 4;
	int nt = 0;
	for (; tags && tags[nt]; nt++) len += 4 + strlen(tags[nt]);
	uint8_t *vc = malloc(4 + len);
	if (!vc) { fclose(e->f); free(e); return NULL; }
	vc[0] = 0x80 | 4; vc[1] = (uint8_t)(len >> 16); vc[2] = (uint8_t)(len >> 8); vc[3] = (uint8_t)len;
	size_t o = 4;
	le32(vc + o, (uint32_t)strlen(vendor)); o += 4;
	memcpy(vc + o, vendor, strlen(vendor)); o += strlen(vendor);
	le32(vc + o, (uint32_t)nt); o += 4;
	for (int i = 0; i < nt; i++) { size_t l = strlen(tags[i]); le32(vc + o, (uint32_t)l); o += 4; memcpy(vc + o, tags[i], l); o += l; }
	fwrite(vc, 1, o, e->f);
	free(vc);
	return e;
}

int flacenc_write(flacenc_t *e, const int16_t *stereo, int frames)
{
	while (frames > 0) {
		int take = BS - e->n;
		if (take > frames) take = frames;
		memcpy(e->buf + e->n * 2, stereo, (size_t)take * 4);
		e->n += take;
		stereo += take * 2;
		frames -= take;
		if (e->n == BS && write_frame(e)) return -1;
	}
	return 0;
}

int flacenc_close(flacenc_t *e)
{
	if (!e) return -1;
	int rc = write_frame(e);
	/* the sample count in STREAMINFO (36 bits from byte 13, low nibble) */
	uint8_t b[5];
	if (!rc && !fseek(e->f, 4 + 4 + 13, SEEK_SET) && fread(b, 1, 1, e->f) == 1) {
		uint64_t t = e->total;
		b[0] = (uint8_t)((b[0] & 0xF0) | ((t >> 32) & 0x0F));
		b[1] = (uint8_t)(t >> 24); b[2] = (uint8_t)(t >> 16); b[3] = (uint8_t)(t >> 8); b[4] = (uint8_t)t;
		fseek(e->f, 4 + 4 + 13, SEEK_SET);
		if (fwrite(b, 1, 5, e->f) != 5) rc = -1;
	}
	if (fclose(e->f)) rc = -1;
	free(e->out);
	free(e);
	return rc;
}
