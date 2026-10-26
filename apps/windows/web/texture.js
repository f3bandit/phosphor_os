/* The console's background textures (frontend/src/texture.c), drawn the same way: one tile, lines
   from distance fields, in the accent colour at the same faint strength, repeated across the window.
   The numbers are the console's, so a texture picked here is the one picked on the TV. */
"use strict";
/* sprite textures (26 on, made with tools/texture-maker): SpriteTex.alpha is the same drawing as the
   console's sprite_alpha (frontend/src/texsprite.c) and the Texture Maker's (kept identical) */
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
const Texture = (() => {
	const SH = { ZIGZAG: 0, X: 1, SQUARE: 2, TRIANGLE: 3, HEX: 4, TRIGRID: 5, DIAMOND: 6, DIAGRID: 7, GRID: 8, BOARD: 9, SPRITES: 10 };
	const LINE = 0, FILL = 1, INVERT = 2, STRENGTH = 0.13;
	const T = [
		["None"],
		["Zigzag", SH.ZIGZAG, 16, 16, 1, LINE],
		["X's", SH.X, 40, 40, 1, LINE],
		["Squares", SH.SQUARE, 40, 40, 1, LINE],
		["Triangles", SH.TRIANGLE, 40, 36, 1, LINE],
		null,                                                   /* 5: the old circles */
		["Hexagons", SH.HEX, 25.980762, 45, 1, LINE],
		null,                                                   /* 7: the old 8-bit tiles */
		["Zigzag, tight", SH.ZIGZAG, 16, 8, 1, LINE],
		["Zigzag, bold", SH.ZIGZAG, 16, 16, 2.4, LINE],
		["X's, packed", SH.X, 14, 14, 1, LINE],
		["Squares, packed", SH.SQUARE, 17, 17, 1, LINE],
		["Squares, filled", SH.SQUARE, 26, 26, 1, FILL],
		["Triangles, packed", SH.TRIGRID, 20, 34.641016, 1, LINE],
		["Triangles, bold", SH.TRIANGLE, 40, 36, 2.4, LINE],
		["Hexagons, bold", SH.HEX, 25.980762, 45, 2.4, LINE],
		["Honeycomb, inverted", SH.HEX, 25.980762, 45, 2.0, INVERT],
		["Squares, inverted", SH.SQUARE, 17, 17, 1.6, INVERT],
		["Diamonds", SH.DIAMOND, 40, 40, 1, LINE],
		["Diamonds, packed", SH.DIAMOND, 20, 20, 1, LINE],
		["Diamonds, filled", SH.DIAMOND, 24, 24, 1, FILL],
		["Diamond lattice", SH.DIAGRID, 20, 20, 1, LINE],
		["Square grid", SH.GRID, 20, 20, 1, LINE],
		["Square grid, bold", SH.GRID, 20, 20, 2.4, LINE],
		/* one whole screen, not a repeating tile (circuit.js) */
		["Circuit board", SH.BOARD, 1280, 720, 1, LINE],
		["Circuit board, bold", SH.BOARD, 1280, 720, 2, LINE],
		/* sprite textures (26 on): the 7th item is the texture's data (tex_<id>.js) */
		null,   /* 26 was a sprite texture (removed): none */
		null,   /* 27 likewise */
		/* sprite textures: entries (tools/texture-maker) */
	];
	const ORDER = [0, 1, 8, 9, 2, 10, 3, 11, 12, 17, 22, 23, 18, 19, 20, 21, 4, 13, 14, 6, 15, 16, 24, 25];

	function seg(px, py, ax, ay, bx, by) {
		const vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay, l2 = vx * vx + vy * vy;
		let t = l2 > 0 ? (wx * vx + wy * vy) / l2 : 0;
		t = Math.max(0, Math.min(1, t));
		const dx = wx - t * vx, dy = wy - t * vy;
		return Math.sqrt(dx * dx + dy * dy);
	}
	function shape(s, x, y, w, h, o) {
		const cx = w / 2, cy = h / 2;
		o.inside = false;
		switch (s) {
		case SH.ZIGZAG: { const m = h / 2, a = Math.min(3, h / 3);
			return Math.min(seg(x, y, 0, m + a, w / 2, m - a), seg(x, y, w / 2, m - a, w, m + a)); }
		case SH.X: return Math.min(seg(x, y, cx - 5, cy - 5, cx + 5, cy + 5), seg(x, y, cx + 5, cy - 5, cx - 5, cy + 5));
		case SH.SQUARE: { const m = Math.max(Math.abs(x - cx), Math.abs(y - cy)), r = 7; o.inside = m < r; return Math.abs(m - r); }
		case SH.TRIANGLE: { const r = 9, ax = cx, ay = cy - r, bx = cx - r * 0.866, by = cy + r * 0.5, qx = cx + r * 0.866, qy = by;
			return Math.min(seg(x, y, ax, ay, bx, by), seg(x, y, bx, by, qx, qy), seg(x, y, qx, qy, ax, ay)); }
		case SH.HEX: { const R = 15, W = 1.7320508 * R, H = 3 * R; let best = 1e9;
			for (let j = -1; j <= 2; j++) for (let i = -1; i <= 2; i++) {
				const hx = i * W + ((j & 1) ? W / 2 : 0), hy = j * H / 2, vx = [], vy = [];
				for (let k = 0; k < 6; k++) { const a = Math.PI / 180 * (60 * k - 30); vx.push(hx + R * Math.cos(a)); vy.push(hy + R * Math.sin(a)); }
				for (let k = 0; k < 6; k++) best = Math.min(best, seg(x, y, vx[k], vy[k], vx[(k + 1) % 6], vy[(k + 1) % 6]));
			}
			return best; }
		case SH.TRIGRID: { let d = Math.min(Math.abs(y), Math.abs(y - h), Math.abs(y - h / 2)); const s3 = 0.8660254;
			for (let k = -2; k <= 2; k++) { const ox = k * w; d = Math.min(d, Math.abs(s3 * (x - ox) + 0.5 * (y - h)), Math.abs(s3 * (x - ox) - 0.5 * (y - h))); }
			return d; }
		case SH.DIAMOND: { const m = Math.abs(x - cx) + Math.abs(y - cy), r = Math.min(9, Math.min(w, h) / 2 - 1.5); o.inside = m < r; return Math.abs(m - r) * 0.70710678; }
		case SH.DIAGRID: return 0.70710678 * Math.min(Math.abs((x + y + 4 * w) % w - w / 2), Math.abs((x - y + 4 * w) % w - w / 2));
		case SH.GRID: { x = (x + 4 * w) % w; y = (y + 4 * h) % h; return Math.min(x, w - x, y, h - y); }
		}
		return 1e9;
	}
	/* the circuit board: the console's traces and vias (circuit.js), the same sizes */
	function board(rgb, scale, lw) {
		const pw = Math.round(1280 * scale), ph = Math.round(720 * scale), C = CIRCUIT.cell, H = C / 2;
		const c = document.createElement("canvas");
		c.width = pw; c.height = ph;
		const g = c.getContext("2d"), P = v => (v * C + H) * scale;
		g.strokeStyle = g.fillStyle = "#fff";
		g.lineWidth = 1.4 * lw * scale; g.lineJoin = "round"; g.lineCap = "round";
		g.beginPath();
		for (let t = 0; t < CIRCUIT.traces.length; t += 2) {
			const a = CIRCUIT.traces[t], n = CIRCUIT.traces[t + 1];
			for (let i = a; i < a + n; i++) {
				const x = P(CIRCUIT.pts[i * 2]), y = P(CIRCUIT.pts[i * 2 + 1]);
				if (i === a) g.moveTo(x, y); else g.lineTo(x, y);
			}
		}
		g.stroke();
		const ro = (2.8 + (lw - 1) * 0.6) * scale, rh = 1.5 * scale;
		for (let i = 0; i < CIRCUIT.vias.length; i += 2) {
			const x = P(CIRCUIT.vias[i]), y = P(CIRCUIT.vias[i + 1]);
			g.beginPath(); g.arc(x, y, ro, 0, 2 * Math.PI); g.arc(x, y, rh, 0, 2 * Math.PI, true); g.fill("evenodd");
			g.globalCompositeOperation = "destination-out";
			g.beginPath(); g.arc(x, y, rh, 0, 2 * Math.PI); g.fill();
			g.globalCompositeOperation = "source-over";
		}
		/* laid on the window's background colour, not left see-through: a picture this faint loses its
		   colour in a see-through PNG (the vias came out green) */
		const bg = (getComputedStyle(document.body).backgroundColor.match(/\d+/g) || [14, 14, 16]).map(Number);
		const img = g.getImageData(0, 0, pw, ph), d = img.data;
		for (let i = 0; i < d.length; i += 4) {
			const a = d[i + 3] / 255 * STRENGTH;
			for (let k = 0; k < 3; k++) d[i + k] = Math.round(bg[k] * (1 - a) + rgb[k] * a);
			d[i + 3] = 255;
		}
		g.putImageData(img, 0, 0);
		return c.toDataURL("image/png");
	}
	/* a sprite texture: a whole screen is laid on the window's background colour like the board, a
	   tile is left see-through like the other tiles */
	function sprites(data, rgb, scale) {
		const a = SpriteTex.alpha(data, scale), c = document.createElement("canvas");
		c.width = a.pw; c.height = a.ph;
		const g = c.getContext("2d"), img = g.createImageData(a.pw, a.ph), d = img.data;
		const full = data.w >= 1280 && data.h >= 720;
		const bg = (getComputedStyle(document.body).backgroundColor.match(/\d+/g) || [14, 14, 16]).map(Number);
		for (let i = 0; i < a.a.length; i++) {
			const k = a.a[i] / 255, j = i * 4;
			if (full) { for (let q = 0; q < 3; q++) d[j + q] = Math.round(bg[q] * (1 - k) + rgb[q] * k); d[j + 3] = 255; }
			else { d[j] = rgb[0]; d[j + 1] = rgb[1]; d[j + 2] = rgb[2]; d[j + 3] = a.a[i]; }
		}
		g.putImageData(img, 0, 0);
		return c.toDataURL("image/png");
	}
	/* the tile as a data: URL, in colour rgb ([r,g,b]) at scale (screen px per console px) */
	function tile(n, rgb, scale) {
		const t = T[n];
		if (!t || n === 0) return null;
		const [, sh, bw, bh, lw, mode] = t;
		if (sh === SH.BOARD) return board(rgb, scale, lw);
		if (sh === SH.SPRITES) return t[6] ? sprites(t[6], rgb, scale) : null;
		const pw = Math.round(bw * scale), ph = Math.round(bh * scale);
		if (pw < 4 || ph < 4) return null;
		const c = document.createElement("canvas");
		c.width = pw; c.height = ph;
		const g = c.getContext("2d"), img = g.createImageData(pw, ph), o = {};
		const half = 0.55 * scale * lw, strength = mode === LINE ? STRENGTH : STRENGTH * 0.75;
		for (let y = 0; y < ph; y++) for (let x = 0; x < pw; x++) {
			const fx = (x + 0.5) * bw / pw, fy = (y + 0.5) * bh / ph;
			let d = 1e9, anyIn = false;
			for (let oy = -1; oy <= 1; oy++) for (let ox = -1; ox <= 1; ox++) {
				const dd = shape(sh, fx + ox * bw, fy + oy * bh, bw, bh, o);
				if (dd < d) d = dd;
				if (o.inside && ox === 0 && oy === 0) anyIn = true;
			}
			d *= pw / bw;
			const line = Math.max(0, Math.min(1, half + 0.5 - d));
			const a = mode === FILL ? (anyIn ? 1 : line) : mode === INVERT ? 1 - line : line;
			const i = (y * pw + x) * 4;
			img.data[i] = rgb[0]; img.data[i + 1] = rgb[1]; img.data[i + 2] = rgb[2];
			img.data[i + 3] = Math.round(a * strength * 255);
		}
		g.putImageData(img, 0, 0);
		return c.toDataURL("image/png");
	}
	return {
		name: n => (T[n] ? T[n][0] : "None"),
		valid: n => n === 0 || !!T[n],
		order: ORDER,
		full: n => !!T[n] && (T[n][1] === SH.BOARD || (T[n][1] === SH.SPRITES && T[n][2] >= 1280 && T[n][3] >= 720)),   /* one picture for the whole window, not a tile */
		tile,
	};
})();
