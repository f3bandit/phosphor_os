/* Sprite textures: the drawing Phosphor does (frontend/src/texture.c sprite_alpha, and the same
   function in apps/windows/web/texture.js). Kept identical in all three places, so what the Texture
   Maker shows is what the TV and the Windows app draw.

   A texture: { w, h, px, inv, str, spr: [[w, h, pixels], ...], pl: [x, y, s, o, ...], cq, sq }
     w, h   size on the 1280x720 base screen (the whole screen, or a tile that repeats)
     px     base pixels per sprite pixel
     inv    1: the accent everywhere except the sprites (each pixel's shade cut out of it)
     str    how much accent colour a full-shade pixel gets (Phosphor's lines use 0.13)
     spr    each sprite: width, height, and its pixels as shades 0..15 (0 = empty, 15 = full), two to
            a byte (the left one in the high half), each row padded to a whole byte; an array of
            bytes, or base64 in the exported JS
     pl     each sprite placed: x, y (its top left, base pixels, signed 16-bit: it may start off the
            screen), which sprite, orientation
            (bits 0-1: turns of 90 degrees clockwise, bit 2: mirrored left to right before turning,
            bit 3: the free angle below the other way round, for a sprite flipped on screen)
     cq, sq a free angle (under 90 degrees, clockwise) every sprite is turned by after that, about its
            middle: its cosine and sine times 65536, whole numbers, so the TV and the Windows app
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
            work from exactly the same values; left out (or 65536, 0) for none */
"use strict";
const SpriteTex = (() => {
	function bytes(b) {
		if (typeof b !== "string") return b;
		const s = atob(b), out = new Uint8Array(s.length);
		for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i);
		return out;
	}
	/* the shade (0..15) of the sprite's pixel at (u, v) of its turned/mirrored picture */
	function shade(sw, sh, px, o, u, v) {
		let x = u, y = v, w = (o & 1) ? sh : sw;            /* w: the width at this step of undoing turns */
		for (let r = o & 3; r > 0; r--) {                    /* undo one turn clockwise */
			const nx = y, ny = w - 1 - x;
			x = nx; y = ny;
			w = (w === sw) ? sh : sw;
		}
		if (o & 4) x = sw - 1 - x;
		const b = px[y * ((sw + 1) >> 1) + (x >> 1)];
		return (x & 1) ? (b & 15) : (b >> 4);
	}
	/* coverage 0..255 a pixel, at scale screen pixels per base pixel: { pw, ph, a } */
	function alpha(t, scale) {
		const pw = Math.round(t.w * scale), ph = Math.round(t.h * scale);
		const m = new Uint8Array(pw * ph);                   /* the strongest shade over each pixel */
		const spr = t.spr.map(s => [s[0], s[1], bytes(s[2])]);
		const tile = t.w < 1280 || t.h < 720;                /* a tile repeats: sprites over an edge come in on the other side */
		const R = v => Math.round(v * scale);
		const cq = t.cq === undefined ? 65536 : t.cq, sq = t.sq === undefined ? 0 : t.sq;
		const free = cq !== 65536 || sq !== 0, cs = cq / 65536, sn0 = sq / 65536;
		for (let i = 0; i + 3 < t.pl.length; i += 4) {
			const x0 = (t.pl[i] << 16) >> 16, y0 = (t.pl[i + 1] << 16) >> 16, o = t.pl[i + 3];   /* x, y are signed 16-bit (a grid starts off the screen) */
			const [sw, sh, px] = spr[t.pl[i + 2]];
			const ow = (o & 1) ? sh : sw, oh = (o & 1) ? sw : sh;
			for (let wy = tile ? -1 : 0; wy <= (tile ? 1 : 0); wy++)
				for (let wx = tile ? -1 : 0; wx <= (tile ? 1 : 0); wx++) {
					if (free) {                              /* turned by the free angle: each screen pixel looks up the sprite pixel under it */
						const sn = (o & 8) ? -sn0 : sn0, bw = ow * t.px, bh = oh * t.px;
						const cx = x0 + wx * t.w + bw / 2, cy = y0 + wy * t.h + bh / 2;
						const hw = (Math.abs(bw * cs) + Math.abs(bh * sn)) / 2, hh = (Math.abs(bw * sn) + Math.abs(bh * cs)) / 2;
						let xa = Math.floor((cx - hw) * scale), xb = Math.ceil((cx + hw) * scale);
						let ya = Math.floor((cy - hh) * scale), yb = Math.ceil((cy + hh) * scale);
						if (xa < 0) xa = 0;
						if (ya < 0) ya = 0;
						if (xb > pw) xb = pw;
						if (yb > ph) yb = ph;
						for (let y = ya; y < yb; y++) {
							const dy = (y + 0.5) / scale - cy;
							for (let x = xa; x < xb; x++) {
								const dx = (x + 0.5) / scale - cx;
								const u = Math.floor((dx * cs + dy * sn + bw / 2) / t.px), v = Math.floor((dy * cs - dx * sn + bh / 2) / t.px);
								if (u < 0 || v < 0 || u >= ow || v >= oh) continue;
								const s = shade(sw, sh, px, o, u, v);
								if (s && m[y * pw + x] < s) m[y * pw + x] = s;
							}
						}
						continue;
					}
					for (let v = 0; v < oh; v++)
						for (let u = 0; u < ow; u++) {
							const s = shade(sw, sh, px, o, u, v);
							if (!s) continue;
							const bx = x0 + wx * t.w + u * t.px, by = y0 + wy * t.h + v * t.px;
							let xa = R(bx), xb = R(bx + t.px), ya = R(by), yb = R(by + t.px);
							if (xb <= 0 || yb <= 0 || xa >= pw || ya >= ph) continue;
							if (xa < 0) xa = 0;
							if (ya < 0) ya = 0;
							if (xb > pw) xb = pw;
							if (yb > ph) yb = ph;
							for (let y = ya; y < yb; y++)
								for (let x = xa; x < xb; x++) if (m[y * pw + x] < s) m[y * pw + x] = s;
						}
				}
		}
		const a = new Uint8Array(pw * ph);
		for (let i = 0; i < a.length; i++) {
			const c = t.inv ? 15 - m[i] : m[i];
			a[i] = Math.round(c * t.str * 255 / 15);
		}
		return { pw, ph, a };
	}
	return { alpha, shade, bytes };
})();
