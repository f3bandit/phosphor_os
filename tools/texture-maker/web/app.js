/* Phosphor Texture Maker: the page. Sprites are read here (each colour turned into a shade), laid
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
   out, drawn with SpriteTex (the same drawing Phosphor does) and exported. */
"use strict";
const TOKEN = new URLSearchParams(location.search).get("k") || "";
const ACCENTS = [["Amber", "#ffb347"], ["Red", "#ff6b5e"], ["Green", "#5cf08a"], ["Blue", "#6cb6ff"], ["Purple", "#c792ff"]];
const BG = [14, 14, 16];
const MAX_SPRITE = 128;
const $ = id => document.getElementById(id);

const S = {
	sprites: [],            /* { name, src (data: URL), on, img } */
	bg: "auto", shadeBy: "spread", levels: 4, floor: 35, invShade: 0,
	canvas: "screen", tileAuto: 1, tileW: 640, tileH: 360, px: 3, density: 40, gap: 24, angleMode: "random", angleDeg: 0, mirror: 1, colGap: 16, rowGap: 16, gridMirror: 1, snap: 1, seed: 1847,
	inv: 0, str: 13, name: "My texture",
	view: "real", accent: 0,
};
const KEYS = ["bg", "shadeBy", "levels", "floor", "invShade", "canvas", "tileAuto", "tileW", "tileH", "px", "density", "gap", "angleMode", "angleDeg", "mirror", "snap", "seed", "inv", "str", "name", "colGap", "rowGap", "gridMirror"];

/* ---------------------------------------------------------------- server */
async function api(url, body) {
	const r = await fetch(url, { method: body === undefined ? "GET" : "POST", headers: { "X-Token": TOKEN, "Content-Type": "application/json" }, body: body === undefined ? undefined : JSON.stringify(body) });
	const j = await r.json().catch(() => ({}));
	if (!r.ok) throw new Error(j.error || "HTTP " + r.status);
	return j;
}
let toastT;
function toast(msg, err) {
	const t = $("toast");
	t.textContent = msg;
	t.className = err ? "err" : "";
	t.style.display = "block";
	clearTimeout(toastT);
	toastT = setTimeout(() => t.style.display = "none", err ? 9000 : 5000);
}

/* ---------------------------------------------------------------- colours to shades */
function lin(c) { c /= 255; return c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4); }
/* how light a colour looks, 0..100 (CIE L*) */
function lightness(r, g, b) {
	const y = 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b);
	return y > 0.008856 ? 116 * Math.cbrt(y) - 16 : 903.3 * y;
}
function hueSat(r, g, b) {
	const mx = Math.max(r, g, b), mn = Math.min(r, g, b), d = mx - mn;
	let h = 0;
	if (d) h = mx === r ? ((g - b) / d) % 6 : mx === g ? (b - r) / d + 2 : (r - g) / d + 4;
	return [h, mx ? d / mx : 0];
}

/* a picture (ImageData) to a sprite: { w, h, sh: Uint8Array of shades 0..15, colours } */
function convert(img) {
	const { width: W, height: H, data: d } = img;
	const px = i => [d[i * 4], d[i * 4 + 1], d[i * 4 + 2], d[i * 4 + 3]];
	let mode = S.bg;
	let seeThrough = false;
	for (let i = 0; i < W * H; i++) if (d[i * 4 + 3] < 128) { seeThrough = true; break; }
	if (mode === "auto") mode = seeThrough ? "alpha" : "corner";
	let key = null;
	if (mode === "corner") {                          /* a picture with no see-through parts: its corner colour is the background */
		const cs = [0, W - 1, (H - 1) * W, H * W - 1].map(i => px(i).slice(0, 3).join(","));
		const count = {};
		cs.forEach(c => count[c] = (count[c] || 0) + 1);
		const best = Object.keys(count).sort((a, b) => count[b] - count[a])[0];
		if (count[best] >= 3) key = best;
	}
	const used = new Uint8Array(W * H);
	const cols = new Map();
	for (let i = 0; i < W * H; i++) {
		const [r, g, b, a] = px(i);
		if ((mode === "alpha" || mode === "corner") && a < 128) continue;
		if (key && [r, g, b].join(",") === key) continue;
		used[i] = 1;
		const k = (r << 16) | (g << 8) | b;
		if (!cols.has(k)) cols.set(k, { r, g, b, L: lightness(r, g, b), n: 0 });
		cols.get(k).n++;
	}
	/* each colour's place from 0 (darkest) to 1 (brightest) */
	const list = [...cols.values()];
	if (S.shadeBy === "spread") {
		list.sort((a, b) => a.L - b.L || hueSat(a.r, a.g, a.b)[1] - hueSat(b.r, b.g, b.b)[1] || hueSat(a.r, a.g, a.b)[0] - hueSat(b.r, b.g, b.b)[0]);
		list.forEach((c, i) => c.t = list.length > 1 ? i / (list.length - 1) : 1);
	} else {
		const lo = Math.min(...list.map(c => c.L)), hi = Math.max(...list.map(c => c.L));
		list.forEach(c => c.t = hi > lo ? (c.L - lo) / (hi - lo) : 1);
	}
	const lv = Math.max(2, S.levels), fl = S.floor / 100;
	for (const c of list) {
		let t = S.invShade ? 1 - c.t : c.t;
		const step = Math.round(t * (lv - 1)) / (lv - 1);           /* the nearest of the shades */
		c.s = Math.max(1, Math.min(15, Math.round((fl + (1 - fl) * step) * 15)));
	}
	/* trimmed to the drawn part */
	let x0 = W, y0 = H, x1 = -1, y1 = -1;
	for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) if (used[y * W + x]) {
		if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
	}
	if (x1 < 0) return { w: 0, h: 0, sh: new Uint8Array(0), colours: 0 };
	const w = x1 - x0 + 1, h = y1 - y0 + 1, sh = new Uint8Array(w * h);
	for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
		const i = (y + y0) * W + x + x0;
		if (!used[i]) continue;
		sh[y * w + x] = cols.get((d[i * 4] << 16) | (d[i * 4 + 1] << 8) | d[i * 4 + 2]).s;
	}
	return { w, h, sh, colours: list.length };
}
function pack(sp) {                                  /* two shades a byte, rows padded */
	const stride = (sp.w + 1) >> 1, out = new Uint8Array(stride * sp.h);
	for (let y = 0; y < sp.h; y++) for (let x = 0; x < sp.w; x++) {
		const s = sp.sh[y * sp.w + x];
		out[y * stride + (x >> 1)] |= (x & 1) ? s : s << 4;
	}
	return out;
}
function loadImage(src) {
	return new Promise((ok, bad) => { const i = new Image(); i.onload = () => ok(i); i.onerror = () => bad(new Error("not a picture")); i.src = src; });
}
function imageData(img) {
	const c = document.createElement("canvas");
	c.width = img.naturalWidth; c.height = img.naturalHeight;
	const g = c.getContext("2d", { willReadFrequently: true });
	g.drawImage(img, 0, 0);
	return g.getImageData(0, 0, c.width, c.height);
}
let conv = [];                                       /* the active sprites, converted: { w, h, sh, bytes, colours, idx } */
function convertAll() {
	conv = [];
	S.sprites.forEach((s, idx) => {
		if (!s.img) return;
		s.c = convert(imageData(s.img));
		if (s.on && s.c.w) conv.push({ ...s.c, bytes: pack(s.c), idx });
	});
}

/* ---------------------------------------------------------------- layout */
function rng(seed) {
	let a = seed >>> 0;
	return () => { a = (a + 0x6D2B79F5) >>> 0; let t = a; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
}
/* the free part of the angle (under 90 degrees) and the whole turns */
function angleParts() {
	if (S.angleMode !== "set") return { turns: -1, fine: 0 };
	let d = ((+S.angleDeg % 360) + 360) % 360;
	const turns = Math.floor(d / 90) & 3, fine = d - Math.floor(d / 90) * 90;
	return { turns, fine };
}
/* a sprite's room on screen: its turned box, then the free angle's bounding box */
function footprint(c, o, fine) {
	const px = S.px, bw = ((o & 1) ? c.h : c.w) * px, bh = ((o & 1) ? c.w : c.h) * px;
	const r = fine * Math.PI / 180, cs = Math.abs(Math.cos(r)), sn = Math.abs(Math.sin(r));
	return { bw, bh, rw: Math.ceil(bw * cs + bh * sn), rh: Math.ceil(bw * sn + bh * cs) };
}
/* Auto tile: big enough that every sprite fits about twice at the density picked, so the repeat
   is a scatter, not a grid of one sprite (16:9, a multiple of 8, the whole screen at most) */
function autoTile() {
	const gap = S.gap, { fine } = angleParts();
	let area = 0, big = 0;
	for (const c of conv) {
		const f = footprint(c, 0, fine), g = footprint(c, 1, fine);
		area += Math.max(f.rw, g.rw) * Math.max(f.rh, g.rh) + gap * (Math.max(f.rw, g.rw) + Math.max(f.rh, g.rh)) + gap * gap;
		big = Math.max(big, Math.max(f.rw, f.rh, g.rw, g.rh) + gap);
	}
	area *= 2 / (0.55 * Math.max(S.density, 5) / 100);
	let w = Math.ceil(Math.sqrt(area * 16 / 9) / 8) * 8, hh = Math.ceil(w * 9 / 16 / 8) * 8;
	w = Math.max(w, Math.ceil(big / 8) * 8, 64); hh = Math.max(hh, Math.ceil(big / 8) * 8, 64);
	return [Math.min(w, 1280), Math.min(hh, 720)];
}
function size() { return [1280, 720]; }   /* a grid covers the whole screen (tilted, it wouldn't repeat as a tile) */
let tex = null;                                      /* the texture as Phosphor gets it */
let missing = [];                                    /* sprites that found no room at all */
/* the grid: every cell the size of the biggest sprite plus the spacing; sprites in the order they
   were added along each row (then again from the first), every other one in a row mirrored; the
   whole grid turned by the tilt about the middle of the screen, far enough out to cover it */
function layout() {
	const W = 1280, H = 720, px = S.px, pl = [];
	missing = [];
	let cq, sq;
	if (conv.length) {
		let deg = ((+S.angleDeg % 360) + 360) % 360;
		if (!Number.isFinite(deg)) deg = 0;
		const turns = Math.floor(deg / 90) & 3, fine = deg - Math.floor(deg / 90) * 90;
		const r = deg * Math.PI / 180, cs = Math.cos(r), sn = Math.sin(r);
		const cw = Math.max(...conv.map(c => c.w)) * px + S.colGap, ch = Math.max(...conv.map(c => c.h)) * px + S.rowGap;
		const reach = Math.hypot(W, H) / 2 + Math.max(cw, ch);
		const ni = Math.ceil(reach / cw), nj = Math.ceil(reach / ch);
		for (let j = -nj; j <= nj; j++)
			for (let i = -ni; i <= ni; i++) {
				const col = i + ni;                                   /* 0 at the row's left end */
				const si = col % conv.length, c = conv[si];
				const mirrored = S.gridMirror && col % 2 === 1;
				/* the data mirrors first, then turns; turning the whole grid turns each sprite the same way */
				const o = turns | (mirrored ? 4 : 0);
				const bw = ((o & 1) ? c.h : c.w) * px, bh = ((o & 1) ? c.w : c.h) * px;
				const gx = i * cw, gy = j * ch;                       /* the cell's middle in the grid, before the tilt */
				const sx = W / 2 + gx * cs - gy * sn, sy = H / 2 + gx * sn + gy * cs;
				const half = Math.hypot(bw, bh) / 2;
				if (sx + half < 0 || sy + half < 0 || sx - half > W || sy - half > H) continue;   /* off screen */
				pl.push(Math.round(sx - bw / 2), Math.round(sy - bh / 2), si, o);
			}
		if (fine) { const f = fine * Math.PI / 180; cq = Math.round(Math.cos(f) * 65536); sq = Math.round(Math.sin(f) * 65536); }
	}
	tex = { w: W, h: H, px, inv: S.inv, str: S.str / 100, spr: conv.map(c => [c.w, c.h, c.bytes]), pl };
	if (cq !== undefined) { tex.cq = cq; tex.sq = sq; }
}

/* ---------------------------------------------------------------- drawing */
function hex2rgb(h) { return [1, 3, 5].map(i => parseInt(h.slice(i, i + 2), 16)); }
/* the texture over Phosphor's background, as the TV shows it (bright: stronger, to see the layout) */
function paint(canvas, scale, rgb, bright) {
	const a = SpriteTex.alpha(tex, scale);
	const cw = canvas.width, ch = canvas.height, g = canvas.getContext("2d"), img = g.createImageData(cw, ch), d = img.data;
	const boost = bright ? Math.min(1 / Math.max(tex.str, 0.01), 6) : 1;
	for (let y = 0; y < ch; y++) for (let x = 0; x < cw; x++) {
		const v = a.a[(y % a.ph) * a.pw + (x % a.pw)] / 255 * boost;
		const k = Math.min(1, v), i = (y * cw + x) * 4;
		for (let c = 0; c < 3; c++) d[i + c] = Math.round(BG[c] * (1 - k) + rgb[c] * k);
		d[i + 3] = 255;
	}
	g.putImageData(img, 0, 0);
}
function drawThumbs() {
	const box = $("sprites");
	box.innerHTML = "";
	const rgb = hex2rgb(ACCENTS[S.accent][1]);
	S.sprites.forEach((s, i) => {
		const el = document.createElement("div");
		el.className = "sp" + (s.on ? "" : " off");
		el.title = s.name + (s.c ? `\n${s.c.w}×${s.c.h}, ${s.c.colours} colour${s.c.colours === 1 ? "" : "s"}` : "");
		const c = document.createElement("canvas");
		if (s.c && s.c.w) {
			c.width = s.c.w; c.height = s.c.h;
			const g = c.getContext("2d"), img = g.createImageData(s.c.w, s.c.h);
			for (let k = 0; k < s.c.sh.length; k++) {      /* the sprite's shades in the accent, at full strength */
				const v = s.c.sh[k] / 15;
				for (let q = 0; q < 3; q++) img.data[k * 4 + q] = Math.round(BG[q] * (1 - v) + rgb[q] * v);
				img.data[k * 4 + 3] = 255;
			}
			g.putImageData(img, 0, 0);
			const z = Math.max(1, Math.floor(56 / Math.max(s.c.w, s.c.h)));
			c.style.width = s.c.w * z + "px"; c.style.height = s.c.h * z + "px";
		}
		el.appendChild(c);
		el.insertAdjacentHTML("beforeend", `<span class="t" title="Use / leave out">◐</span><span class="x" title="Remove">✕</span><span class="n"></span>`);
		el.querySelector(".n").textContent = s.name;
		el.querySelector(".t").onclick = () => { s.on = !s.on; refresh(true); };
		el.querySelector(".x").onclick = () => { S.sprites.splice(i, 1); refresh(true); };
		box.appendChild(el);
	});
	const add = document.createElement("div");
	add.className = "sp add"; add.textContent = "+"; add.title = "Add sprites";
	add.onclick = () => $("file").click();
	box.appendChild(add);
}
function drawPreview() {
	const big = $("big"), rgb = hex2rgb(ACCENTS[S.accent][1]);
	paint(big, 1, rgb, S.view === "bright");
	const n = tex.pl.length / 4, [W, H] = size();
	const used = new Set();
	for (let i = 2; i < tex.pl.length; i += 4) used.add(tex.pl[i]);
	const miss = "";
	$("lbl").textContent = conv.length
		? `1280 × 720 grid${+S.angleDeg ? `, tilted ${+S.angleDeg}°` : ""} · ${n} placed, ${used.size} of ${conv.length} sprite${conv.length === 1 ? "" : "s"} used · ${ACCENTS[S.accent][0]}${S.view === "bright" ? " · bright view" : ""}${miss}`
		: "Add sprites to start";
	const sws = $("sws");
	if (!sws.children.length) ACCENTS.forEach(([nm, col], i) => {
		const d = document.createElement("div");
		d.className = "sw";
		d.innerHTML = `<canvas width="400" height="225"></canvas><span style="color:${col}">${nm}</span>`;
		d.onclick = () => { S.accent = i; refresh(); };
		sws.appendChild(d);
	});
	[...sws.children].forEach((d, i) => paint(d.firstChild, 400 / 1280, hex2rgb(ACCENTS[i][1]), false));
}
function syncControls() {
	document.querySelectorAll(".seg[data-k]").forEach(seg => {
		const k = seg.dataset.k;
		seg.querySelectorAll("b").forEach(b => b.classList.toggle("on", String(S[k]) === b.dataset.v));
	});
	document.querySelectorAll("input[data-k]").forEach(inp => { if (document.activeElement !== inp) inp.value = S[inp.dataset.k]; });
	$("v_levels").textContent = S.levels;
	$("v_floor").textContent = S.floor + "%";
	$("v_colGap").textContent = S.colGap + " px";
	$("v_rowGap").textContent = S.rowGap + " px";
	$("v_str").textContent = (S.str / 100).toFixed(2);
	$("idv").textContent = texId();
	$("dots").innerHTML = ACCENTS.map(([n, c], i) => `<span class="dot${i === S.accent ? " on" : ""}" title="${n}" data-i="${i}" style="background:${c}"></span>`).join(" ");
	$("dots").querySelectorAll(".dot").forEach(d => d.onclick = () => { S.accent = +d.dataset.i; refresh(); });
	document.documentElement.style.setProperty("--acc", ACCENTS[S.accent][1]);
}
let busy = 0;
function refresh(reconvert) {
	if (reconvert) convertAll();
	layout();
	syncControls();
	drawThumbs();
	clearTimeout(busy);
	busy = setTimeout(drawPreview, 10);
	clearTimeout(lastT);
	lastT = setTimeout(() => api("/api/last", project()).catch(() => {}), 800);   /* the next start opens where you left off */
}
let lastT = 0;

/* ---------------------------------------------------------------- controls */
const NUM = new Set(["colGap", "rowGap", "gridMirror", "levels", "floor", "invShade", "tileAuto", "angleDeg", "tileW", "tileH", "px", "density", "gap", "mirror", "snap", "inv", "str", "seed"]);
const CONVERT = new Set(["bg", "shadeBy", "levels", "floor", "invShade"]);
document.querySelectorAll(".seg[data-k]").forEach(seg => seg.addEventListener("click", e => {
	const b = e.target.closest("b");
	if (!b) return;
	const k = seg.dataset.k;
	S[k] = NUM.has(k) ? +b.dataset.v : b.dataset.v;
	refresh(CONVERT.has(k));
}));
document.querySelectorAll("input[data-k]").forEach(inp => inp.addEventListener("input", () => {
	const k = inp.dataset.k;
	if (NUM.has(k)) {
		let v = +inp.value;
		if (!Number.isFinite(v)) return;
		if (k === "tileW") v = Math.max(32, Math.min(1280, v));
		if (k === "tileH") v = Math.max(32, Math.min(720, v));
		if (k === "angleDeg") v = Math.max(0, Math.min(359.5, v));
		S[k] = v;
	} else S[k] = inp.value;
	if (k === "name") { syncControls(); return; }
	refresh(CONVERT.has(k));
}));
async function addFiles(files) {
	let bad = [];
	for (const f of files) {
		try {
			const src = await new Promise((ok, no) => { const r = new FileReader(); r.onload = () => ok(r.result); r.onerror = no; r.readAsDataURL(f); });
			const img = await loadImage(src);
			if (img.naturalWidth > MAX_SPRITE || img.naturalHeight > MAX_SPRITE) { bad.push(`${f.name}: ${img.naturalWidth}×${img.naturalHeight} (128×128 at most)`); continue; }
			S.sprites.push({ name: f.name.replace(/\.[^.]+$/, ""), src, on: true, img });
		} catch (e) { bad.push(f.name + ": " + e.message); }
	}
	refresh(true);
	if (bad.length) toast("Not added:\n" + bad.join("\n"), true);
}
$("file").onchange = e => { addFiles([...e.target.files]); e.target.value = ""; };
const app = $("app");
app.addEventListener("dragover", e => { e.preventDefault(); app.classList.add("drop"); });
app.addEventListener("dragleave", e => { if (e.target === app || !app.contains(e.relatedTarget)) app.classList.remove("drop"); });
app.addEventListener("drop", e => {
	e.preventDefault(); app.classList.remove("drop");
	const fs = [...e.dataTransfer.files];
	const p = fs.find(f => /\.ptm$/i.test(f.name));
	if (p) openProject(p); else addFiles(fs.filter(f => f.type.startsWith("image/")));
});

/* ---------------------------------------------------------------- projects */
function project() {
	const p = { kind: "phosphor-texture-maker", version: 1, sprites: S.sprites.map(s => ({ name: s.name, src: s.src, on: s.on })) };
	KEYS.forEach(k => p[k] = S[k]);
	return p;
}
async function loadProject(p) {
	if (!p || p.kind !== "phosphor-texture-maker") throw new Error("not a Texture Maker project");
	KEYS.forEach(k => { if (p[k] !== undefined) S[k] = p[k]; });
	/* older projects: rot (1.0.0) or angle (1.0.2) */
	if (p.angleMode === undefined) {
		const a = p.angle !== undefined ? String(p.angle) : p.rot === 0 ? "0" : "random";
		S.angleMode = a === "random" || a === "null" ? "random" : "set";
		S.angleDeg = S.angleMode === "set" ? +a || 0 : 0;
	}
	if (S.angleMode !== "set") S.angleMode = "random";
	if (!Number.isFinite(+S.angleDeg)) S.angleDeg = 0;
	if (p.tileAuto === undefined) S.tileAuto = 1;
	if (![0, 1, 3].includes(+S.mirror)) S.mirror = 1;
	S.mirror = +S.mirror;
	S.sprites = [];
	for (const s of p.sprites || []) {
		try { S.sprites.push({ name: s.name, src: s.src, on: s.on !== false, img: await loadImage(s.src) }); } catch (e) { /* a broken picture is skipped */ }
	}
	refresh(true);
}
function download(name, blob) {
	const a = document.createElement("a");
	a.href = URL.createObjectURL(blob);
	a.download = name;
	a.click();
	setTimeout(() => URL.revokeObjectURL(a.href), 5000);
}
$("save").onclick = () => download(texId() + ".ptm", new Blob([JSON.stringify(project())], { type: "application/json" }));
$("open").onclick = () => $("pfile").click();
async function openProject(f) {
	try { await loadProject(JSON.parse(await f.text())); toast("Opened " + f.name); } catch (e) { toast("Couldn't open " + f.name + ": " + e.message, true); }
}
$("pfile").onchange = e => { if (e.target.files[0]) openProject(e.target.files[0]); e.target.value = ""; };

/* ---------------------------------------------------------------- export */
function texId() {
	let id = (S.name || "").toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "");
	if (!/^[a-z]/.test(id)) id = "t_" + id;
	return id.slice(0, 32).replace(/_+$/, "") || "texture";
}
function b64(u8) { let s = ""; for (let i = 0; i < u8.length; i++) s += String.fromCharCode(u8[i]); return btoa(s); }
function wrapNums(nums, per) {
	const out = [];
	for (let i = 0; i < nums.length; i += per) out.push("\t" + nums.slice(i, i + per).join(",") + ",");
	return out.join("\n");
}
function files() {
	const id = texId(), ID = id.toUpperCase(), name = S.name.trim() || "Texture";
	const n = tex.pl.length / 4, kind = "the whole screen, a grid";
	const head = `"${name}" background texture: made by Phosphor Texture Maker (tools/texture-maker), don't edit.\n * ${tex.w}x${tex.h} base pixels (${kind}), ${tex.spr.length} sprite${tex.spr.length === 1 ? "" : "s"}, ${n} placed, a sprite pixel is ${tex.px} base pixel${tex.px === 1 ? "" : "s"}.`;
	let off = 0;
	const sprC = [], allBytes = [];
	tex.spr.forEach(([w, h, b]) => { sprC.push(`{ ${w}, ${h}, ${off} }`); off += b.length; allBytes.push(...b); });
	const c = `/* ${head}\n * Shades 0..15, two pixels a byte (the left one in the high half), rows padded to a byte. */\n` +
		`static const uint8_t TX_${ID}_PIX[] = {\n${wrapNums(allBytes, 24)}\n};\n` +
		`static const tex_sprite_t TX_${ID}_SPR[] = { ${sprC.join(", ")} };\n` +
		`/* x, y, sprite, orientation (turns of 90 degrees clockwise; 4 = mirrored first) */\n` +
		`static const uint16_t TX_${ID}_PL[] = {\n${wrapNums(tex.pl.map(v => v & 0xFFFF), 24)}\n};\n` +
		`static const tex_sprites_t TX_${ID} = { ${tex.w}, ${tex.h}, ${tex.px}, ${tex.inv}, ${tex.str.toFixed(3)}, ${tex.spr.length}, TX_${ID}_SPR, TX_${ID}_PIX, ${n}, TX_${ID}_PL, ${tex.cq || 0}, ${tex.sq || 0} };\n`;
	const js = `/* ${head} */\n"use strict";\nconst TEX_${ID} = { w: ${tex.w}, h: ${tex.h}, px: ${tex.px}, inv: ${tex.inv}, str: ${tex.str.toFixed(3)},\n` +
		`\tspr: [${tex.spr.map(([w, h, b]) => `[${w}, ${h}, "${b64(b)}"]`).join(", ")}],\n\tpl: [${tex.pl.join(",")}]${tex.cq !== undefined ? `,\n\tcq: ${tex.cq}, sq: ${tex.sq}` : ""} };\n`;
	const howto =
`"${name}" (id ${id}): exported by Phosphor Texture Maker.

Add to Phosphor does all of this. By hand:

1. Copy tex_${id}.h to frontend/src/ and tex_${id}.js to apps/windows/web/.
2. frontend/src/texture.c
   - above the line ${'"/* sprite textures: includes (tools/texture-maker) */"'}:
       #include "tex_${id}.h"
   - above the line ${'"/* sprite textures: entries (tools/texture-maker) */"'} in T[] (it becomes the next number):
       { "${name}", SH_SPRITES, ${tex.w}, ${tex.h}, 1, M_LINE, &TX_${ID} },
   - add that number to the end of ORDER[].
3. apps/windows/web/texture.js
   - above "/* sprite textures: entries (tools/texture-maker) */" in T (the same number):
       ["${name}", SH.SPRITES, ${tex.w}, ${tex.h}, 1, LINE, TEX_${ID}],
   - add the number to the end of ORDER.
4. apps/windows/web/index.html: <script src="tex_${id}.js"></script> before <script src="texture.js"></script>.
`;
	return { id, name, files: {
		[`tex_${id}.h`]: c,
		[`tex_${id}.js`]: js,
		"texture.json": JSON.stringify({ name, id, w: tex.w, h: tex.h, px: tex.px, inv: tex.inv, str: tex.str, sprites: tex.spr.length, placed: n }, null, 1),
		"HOW_TO_ADD.txt": howto,
		[`${id}.ptm`]: JSON.stringify(project()),
		"preview.png": previewSheet(),
	} };
}
/* all five accents at real strength, and one bright, 640x360 each */
function previewSheet() {
	const c = document.createElement("canvas"), cw = 640, ch = 360;
	c.width = cw * 3; c.height = ch * 2 + 0;
	const g = c.getContext("2d"), tmp = document.createElement("canvas");
	tmp.width = cw; tmp.height = ch;
	ACCENTS.forEach(([nm, col], i) => {
		paint(tmp, 0.5, hex2rgb(col), false);
		g.drawImage(tmp, (i % 3) * cw, Math.floor(i / 3) * ch);
		g.fillStyle = col; g.font = "16px Segoe UI, sans-serif"; g.fillText(nm, (i % 3) * cw + 12, Math.floor(i / 3) * ch + ch - 12);
	});
	paint(tmp, 0.5, hex2rgb(ACCENTS[S.accent][1]), true);
	g.drawImage(tmp, 2 * cw, ch);
	g.fillStyle = "#ece8e1"; g.font = "16px Segoe UI, sans-serif"; g.fillText("Bright (layout)", 2 * cw + 12, 2 * ch - 12);
	return c.toDataURL("image/png");
}
function ready() {
	if (!conv.length) { toast("Add a sprite first.", true); return false; }
	if (!tex.pl.length) { toast("No sprite fits: try a smaller pixel scale, a smaller gap or a bigger tile.", true); return false; }
	if (!S.name.trim()) { toast("Give the texture a name.", true); return false; }
	return true;
}
$("export").onclick = async () => {
	if (!ready()) return;
	try {
		const f = files(), r = await api("/api/export", f);
		lastOut = r.dir;
		$("exnote").textContent = "Exported to " + r.dir;
		toast(`Exported "${f.name}" to\n${r.dir}\n\n${r.files.join(", ")}`);
	} catch (e) { toast("Export failed: " + e.message, true); }
};
$("install").onclick = async () => {
	if (!ready()) return;
	const f = files();
	if (!confirm(`Add "${f.name}" to Phosphor's source?\n\nIt writes tex_${f.id}.h and tex_${f.id}.js and adds its line to texture.c, texture.js and the Windows app's index.html. A texture already added with this id keeps its number.`)) return;
	try {
		await api("/api/export", f);
		const r = await api("/api/install", f);
		$("exnote").textContent = r.message;
		toast(r.message + "\n\n" + r.files.join("\n"));
		loadCfg();
	} catch (e) { toast("Not added: " + e.message, true); }
};

/* ---------------------------------------------------------------- folders */
let CFG = {}, lastOut = "";
async function loadCfg() {
	try {
		CFG = await api("/api/config");
		$("ver").textContent = "version " + CFG.version + (CFG.phosphor ? "" : " · Phosphor's source not set");
		$("install").disabled = !CFG.phosphor || !(CFG.info && CFG.info.ready);
		$("install").title = !CFG.phosphor ? "Set Phosphor's source in Folders…" : !(CFG.info && CFG.info.ready) ? (CFG.phosphor_error || "This Phosphor can't draw sprite textures yet") : `Next texture number: ${CFG.info.next}`;
	} catch (e) { toast(e.message, true); }
}
$("folders").onclick = () => {
	$("fOut").value = CFG.out || ""; $("fPh").value = CFG.phosphor || "";
	$("fInfo").textContent = CFG.info ? `Next texture number: ${CFG.info.next}. Sprite textures in it: ${(CFG.info.sprites || []).join(", ") || "none yet"}.${CFG.info.ready ? "" : " It can't draw sprite textures yet."}` : (CFG.phosphor_error || "");
	$("dlg").showModal();
};
$("fCancel").onclick = () => $("dlg").close();
$("fSave").onclick = async () => {
	try { await api("/api/config", { out: $("fOut").value.trim(), phosphor: $("fPh").value.trim() }); $("dlg").close(); loadCfg(); }
	catch (e) { $("fInfo").textContent = e.message; }
};
$("openOut").onclick = () => api("/api/open", { path: lastOut || CFG.out }).catch(e => toast(e.message, true));

/* ---------------------------------------------------------------- start */
setInterval(() => api("/api/ping", {}).catch(() => {}), 5000);
api("/api/ping", {}).catch(() => {});
(async () => {
	loadCfg();
	let saved = null;
	try { saved = await api("/api/last"); } catch (e) { /* none yet */ }
	if (saved && saved.kind) await loadProject(saved).catch(() => refresh(true));
	else refresh(true);
})();
