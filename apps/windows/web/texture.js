/* The console's background textures (frontend/src/texture.c), drawn the same way: one tile, lines
   from distance fields, in the accent colour at the same faint strength, repeated across the window.
   The numbers are the console's, so a texture picked here is the one picked on the TV. */
"use strict";
const Texture = (() => {
	const SH = { ZIGZAG: 0, X: 1, SQUARE: 2, TRIANGLE: 3, HEX: 4, TRIGRID: 5, DIAMOND: 6, DIAGRID: 7, GRID: 8 };
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
	];
	const ORDER = [0, 1, 8, 9, 2, 10, 3, 11, 12, 17, 22, 23, 18, 19, 20, 21, 4, 13, 14, 6, 15, 16];

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
	/* the tile as a data: URL, in colour rgb ([r,g,b]) at scale (screen px per console px) */
	function tile(n, rgb, scale) {
		const t = T[n];
		if (!t || n === 0) return null;
		const [, sh, bw, bh, lw, mode] = t;
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
		tile,
	};
})();
