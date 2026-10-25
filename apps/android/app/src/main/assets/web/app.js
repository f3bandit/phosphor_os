/* Phosphor for Android: the screens. A web page in the app's WebView (SS1 Tool for Android's layout,
   Phosphor's look). Everything the MiSTer does goes through PhosNative.call (the app sends it to
   phosphord's remote access with this phone's key); what only the phone does (pairing, box art
   lookups on the phone, notifications, its storage) through PhosNative.app.
   Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
"use strict";

/* ---------------------------------------------------------------- the app underneath */
const N = window.PhosNative || null;
let seq = 0;
const waiting = {};
window.__phos = (id, ok, data) => {
	const w = waiting[id];
	if (!w) return;
	delete waiting[id];
	let v = data;
	try { v = JSON.parse(data); } catch (e) { /* text */ }
	ok ? w.res(v) : w.rej(new Error(typeof v === "string" ? v : (v && v.error) || "Something went wrong"));
};
function bridge(kind, a, b) {
	return new Promise((res, rej) => {
		if (!N) return rej(new Error("Not running in the app"));
		const id = String(++seq);
		waiting[id] = { res, rej };
		kind === "call" ? N.call(id, JSON.stringify(a)) : N.app(id, a, JSON.stringify(b || {}));
	});
}
/** a phosphord command (docs/remote-api.md) */
const call = (cmd, extra) => bridge("call", Object.assign({ cmd }, extra || {}));
/** something only the phone does */
const app = (action, args) => bridge("app", action, args);

const ev = {};
window.__phosEvent = (name, data) => {
	let v = data;
	try { v = JSON.parse(data); } catch (e) { /* text */ }
	(ev[name] || []).forEach(f => f(v));
};
function on(name, f) { (ev[name] = ev[name] || []).push(f); }

/* ---------------------------------------------------------------- small helpers */
const $ = s => document.querySelector(s);
const esc = s => String(s == null ? "" : s).replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const attr = esc;
const sleep = ms => new Promise(r => setTimeout(r, ms));
function gb(kb) {
	kb = Number(kb) || 0;
	if (kb >= 1048576 * 1024) return (kb / (1048576 * 1024)).toFixed(1) + " TB";
	if (kb >= 1048576) return (kb / 1048576).toFixed(1) + " GB";
	if (kb >= 1024) return Math.round(kb / 1024) + " MB";
	return kb + " KB";
}
const sizeText = bytes => gb(Math.floor((Number(bytes) || 0) / 1024));
function whenText(ms) {
	if (!ms || ms <= 0) return "";
	const d = new Date(ms), p = n => String(n).padStart(2, "0");
	return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}`;
}
let toastT;
function toast(text, bad) {
	const t = $("#toast");
	t.textContent = text;
	t.className = bad ? "err" : "";
	t.hidden = false;
	clearTimeout(toastT);
	toastT = setTimeout(() => { t.hidden = true; }, 3200);
}
/* pieces of the page */
const row = (t, o = {}) => {
	const tag = o.go ? "button" : "div";
	const v = o.value != null ? `<span class="v${o.accent ? " acc" : ""}">${esc(o.value)}</span>` : "";
	const go = o.go && o.value == null ? `<svg class="go" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><path d="M9 6l6 6-6 6"/></svg>` : "";
	return `<${tag} class="row${o.dim ? " dim" : ""}"${o.go ? ` type="button" data-go="${attr(o.go)}"` : ""}>${o.pre || ""}<span class="t"><b>${o.html ? t : esc(t)}</b>${o.sub ? `<span>${esc(o.sub)}</span>` : ""}${o.tags || ""}</span>${v}${go}</${tag}>`;
};
const card = (inner, cls = "") => `<section class="card ${cls}">${inner}</section>`;
const flat = inner => `<section class="card flat">${inner}</section>`;
const lbl = t => `<div class="lbl">${esc(t)}</div>`;
const note = t => `<p class="note">${esc(t)}</p>`;
const btn = (t, act, o = {}) => `<button type="button" class="b${o.p ? " p" : ""}${o.d ? " d" : ""}${o.wide ? " wide" : ""}" data-act="${attr(act)}"${o.arg != null ? ` data-arg="${attr(o.arg)}"` : ""}${o.off ? " disabled" : ""}${o.sure ? ` data-sure="${attr(o.sure)}"` : ""}>${esc(t)}</button>`;
const spin = () => `<div class="spin" role="progressbar" aria-label="Loading"></div>`;
const errp = e => e ? `<p class="err">${esc(e)}</p>` : "";
const tog = (on, act) => `<button type="button" class="tog${on ? " on" : ""}" role="switch" aria-checked="${on}" data-act="${attr(act)}"></button>`;

/* ---------------------------------------------------------------- the look: the TV's accent and texture */
const ACCENTS = { amber: "#ffb347", red: "#ff6b5e", green: "#5cf08a", blue: "#6cb6ff", purple: "#c792ff" };
const LOOK = { accent: "amber", texture: 0 };
const rgb = hex => { const v = parseInt(hex.slice(1), 16); return [v >> 16, (v >> 8) & 255, v & 255]; };
function applyLook() {
	const acc = ACCENTS[LOOK.accent] || ACCENTS.amber, [r, g, b] = rgb(acc);
	const mix = (c, d, k) => Math.round(c * k + d * (1 - k));
	const s = document.documentElement.style;
	s.setProperty("--acc", acc);
	s.setProperty("--on", `rgb(${mix(r, 0x14, .12)},${mix(g, 0x14, .12)},${mix(b, 0x16, .12)})`);
	s.setProperty("--navon", `rgb(${mix(r, 0x14, .22)},${mix(g, 0x14, .22)},${mix(b, 0x17, .22)})`);
	try {
		const full = Texture.full(LOOK.texture);
		const scale = full ? Math.max(innerWidth / 1280, innerHeight / 720) * (devicePixelRatio || 1) : 1;
		const url = Texture.tile(LOOK.texture, [r, g, b], scale);
		document.body.style.backgroundImage = url ? `url(${url})` : "none";
		document.body.style.backgroundSize = full ? "cover" : "";
		document.body.style.backgroundPosition = full ? "center" : "";
	} catch (e) { document.body.style.backgroundImage = "none"; }
}
function setLook(l) {
	if (!l) return;
	const a = ACCENTS[l.accent] ? l.accent : LOOK.accent, t = Number.isInteger(l.texture) ? l.texture : LOOK.texture;
	if (a === LOOK.accent && t === LOOK.texture) return;
	LOOK.accent = a; LOOK.texture = t;
	applyLook();
	app("look", LOOK).catch(() => {});                 /* the app keeps it for the next start and its own screens */
}
let lookT;
addEventListener("resize", () => { clearTimeout(lookT); lookT = setTimeout(applyLook, 200); });

/* ---------------------------------------------------------------- state, the bar and the bottom bar */
const ST = { paired: false, address: "", reachable: null, version: "", appVersion: "", notesUnseen: 0 };
function showConn() {
	const c = $("#conn");
	c.hidden = !ST.paired;
	c.className = ST.reachable === true ? "ok" : ST.reachable === false ? "bad" : "";
	c.querySelector("span").textContent = ST.reachable === true ? "SS1 · connected" : ST.reachable === false ? "Not answering" : "Connecting";
}
on("state", s => {
	const was = ST.paired;
	Object.assign(ST, s);
	if (s.look) setLook(s.look);
	showConn();
	drawNav();
	if (was && !ST.paired) go("connect", true);
});

const NAV = [
	["home", "Home", '<path d="M3 11l9-7 9 7v9a1 1 0 0 1-1 1h-5v-6h-6v6H4a1 1 0 0 1-1-1z"/>'],
	["library", "Library", '<rect x="3" y="4" width="7" height="16" rx="1"/><rect x="14" y="4" width="7" height="16" rx="1"/>'],
	["saves", "Saves", '<rect x="4" y="3" width="16" height="18" rx="2"/><path d="M8 3v6h8V3"/>'],
	["video", "Video", '<rect x="3" y="5" width="18" height="12" rx="2"/><path d="M8 21h8M12 17v4"/>'],
	["more", "More", '<circle cx="5" cy="12" r="1.5"/><circle cx="12" cy="12" r="1.5"/><circle cx="19" cy="12" r="1.5"/>'],
];
const MORE = [
	["boxart", "Box art", "Look up box art and details, sources and logins"],
	["look", "Look", "The TV's accent colour and background texture"],
	["power", "Power & updates", "Power check, restart, Phosphor updates, restore points"],
	["storage", "Storage", "SD card, NVMe and USB drives, and this phone"],
	["bluetooth", "Bluetooth", "Controllers paired with the MiSTer"],
	["diag", "Diagnostics", "The MiSTer's whole-system checks"],
	["notes", "Notifications", "Warnings and news from the MiSTer"],
	["connection", "Connection", "The paired MiSTer, find it again, forget it"],
	["about", "About", "Version, privacy and licences"],
];
const SECTION_OF = { home: "home", library: "library", games: "library", game: "library", edit: "library", based: "library", saves: "saves", video: "video" };
function drawNav() {
	const n = $("#nav");
	n.hidden = !ST.paired || R.name === "connect";
	const sec = SECTION_OF[R.name] || (MORE.some(m => m[0] === R.name) ? "more" : "");
	n.innerHTML = NAV.map(([id, name, ic]) =>
		`<button type="button" data-nav="${id}" class="${sec === id ? "on" : ""}${id === "more" && ST.notesUnseen ? " badge" : ""}" aria-label="${name}"><i><svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true">${ic}</svg></i><span>${name}</span></button>`).join("");
}
$("#nav").addEventListener("click", e => {
	const b = e.target.closest("[data-nav]");
	if (!b) return;
	if (b.dataset.nav === "more") return openMore();
	go(b.dataset.nav, true);
});
function openMore() {
	$("#sheetlist").innerHTML = MORE.map(([id, name, d]) => row(name, { sub: d, go: id, value: id === "notes" && ST.notesUnseen ? String(ST.notesUnseen) : null, accent: true })).join("");
	$("#sheet").hidden = $("#sheetbg").hidden = false;
}
function closeMore() { $("#sheet").hidden = $("#sheetbg").hidden = true; }
$("#sheetbg").onclick = closeMore;
$("#sheet").addEventListener("click", e => { const b = e.target.closest("[data-go]"); if (b) { closeMore(); go(b.dataset.go, true); } });
$("#conn").onclick = () => go("connection", true);
$("#back").onclick = () => back();

/* ---------------------------------------------------------------- routes: #name/arg?k=v */
const R = { name: "", arg: "", q: {}, gen: 0 };
function parse() {
	const h = location.hash.replace(/^#/, "") || (ST.paired ? "home" : "connect");
	const [path, qs] = h.split("?");
	const [name, ...rest] = path.split("/");
	const q = {};
	(qs || "").split("&").filter(Boolean).forEach(p => { const [k, v] = p.split("="); q[decodeURIComponent(k)] = decodeURIComponent(v || ""); });
	return { name, arg: rest.join("/"), q };
}
/* the screens opened from a section, so Back goes to the one before; a section from the bottom bar starts again */
let stack = [];
/** go to a screen; top: a section (no back) */
function go(to, top) {
	if (top) { stack = [to]; history.replaceState(null, "", "#" + to); route(); return; }
	stack.push(to);
	location.hash = to;
}
function back() {
	if (!$("#sheet").hidden) { closeMore(); return true; }
	if (stack.length > 1) { history.back(); return true; }
	if (R.name !== "home" && ST.paired && R.name !== "connect") { go("home", true); return true; }
	return false;
}
window.__phosBack = () => back();          /* the phone's back button: false lets the app close */
addEventListener("hashchange", () => {
	const h = location.hash.replace(/^#/, "");
	if (stack.length > 1 && stack[stack.length - 2] === h) stack.pop();        /* Back */
	else if (stack[stack.length - 1] !== h) stack.push(h);
	route();
});

const PAGES = {};
function setBar(title, word) {
	const t = $("#title");
	t.textContent = title;
	t.className = word ? "word" : "";
	$("#back").hidden = !(R.depth > 0);
}
function setChips(list, cur, onPick) {
	const c = $("#chips");
	c.innerHTML = (list || []).map(([id, name]) => `<button type="button" class="chip${id === cur ? " on" : ""}" data-chip="${attr(id)}">${esc(name)}</button>`).join("");
	c.onclick = e => { const b = e.target.closest("[data-chip]"); if (b) onPick(b.dataset.chip); };
	const on = c.querySelector(".on");
	if (on) on.scrollIntoView({ inline: "nearest", block: "nearest" });
}
async function route() {
	const p = parse();
	if (!ST.paired && p.name !== "connect") { history.replaceState(null, "", "#connect"); return route(); }
	const gen = ++R.gen;
	Object.assign(R, p);
	if (!stack.length) stack = [location.hash.replace(/^#/, "") || p.name];
	R.depth = PAGES[p.name] && PAGES[p.name].top ? 0 : stack.length - 1;
	closeMore();
	drawNav();
	setChips([]);
	const page = $("#page");
	page.innerHTML = spin();
	scrollTo(0, 0);
	const P = PAGES[p.name] || PAGES.home;
	const ctx = {
		alive: () => gen === R.gen,
		draw: html => { if (gen === R.gen) { page.innerHTML = html; layoutSoon(); } },
		/* runs f now and again after the delay it returns (ms), while this screen is open */
		every: async f => { while (gen === R.gen) { let ms = 5000; try { ms = await f(); } catch (e) { ms = 5000; } if (gen !== R.gen) break; await sleep(ms || 5000); } },
	};
	try { await P.show(ctx, p); } catch (e) { if (ctx.alive()) ctx.draw(errp(e.message)); }
}

/* every click on the page: data-go (a screen), data-act (an action of the screen on show) */
let acts = {};
$("#page").addEventListener("click", async e => {
	const g = e.target.closest("[data-go]");
	if (g) { go(g.dataset.go); return; }
	const a = e.target.closest("[data-act]");
	if (!a || a.disabled) return;
	if (a.dataset.sure && !a.classList.contains("armed")) {           /* press twice for anything that can't be undone */
		const was = a.textContent, cls = a.className;
		a.classList.add("armed", "p");
		a.textContent = a.dataset.sure;
		setTimeout(() => { if (a.isConnected) { a.className = cls; a.textContent = was; } }, 4000);
		return;
	}
	const f = acts[a.dataset.act];
	if (f) {
		try { await f(a.dataset.arg, a); } catch (err) { toast(err.message, true); }
	}
});
$("#page").addEventListener("input", e => { const f = acts["input:" + (e.target.dataset.in || "")]; if (f) f(e.target.value, e.target); });

/* ---------------------------------------------------------------- the safety net: nothing past the edge */
let layQ = 0;
function layoutSoon() { if (!layQ) layQ = requestAnimationFrame(layout); }
function layout() {
	layQ = 0;
	const vw = document.documentElement.clientWidth;
	const page = $("#page");
	page.querySelectorAll("div[data-act]").forEach(x => { x.setAttribute("role", "button"); x.tabIndex = 0; });
	page.querySelectorAll(".a1col").forEach(x => x.classList.remove("a1col"));
	page.querySelectorAll(".afit").forEach(x => x.classList.remove("afit"));
	for (let pass = 0; pass < 3; pass++) {
		let bad = 0;
		for (const el of page.querySelectorAll("*")) {
			if (!el.offsetParent || el.closest(".out,textarea,svg,canvas")) continue;
			const r = el.getBoundingClientRect();
			if (!r.width) continue;
			const over = r.right > vw + 1 && el.parentElement.getBoundingClientRect().right <= vw + 1;
			const clip = el.matches(".b,.chip,.tag") && el.scrollWidth > el.clientWidth + 2;
			if (!over && !clip) continue;
			let box = el.parentElement;
			while (box && box !== page) { const d = getComputedStyle(box).display; if ((d.includes("flex") || d.includes("grid")) && !box.classList.contains("a1col")) break; box = box.parentElement; }
			if (box && box !== page && over) box.classList.add("a1col"); else el.classList.add("afit");
			bad++;
		}
		if (!bad) break;
	}
}
addEventListener("resize", layoutSoon);

/* ---------------------------------------------------------------- Connect (pairing) */
PAGES.connect = { top: true, async show(c) {
	setBar("Pair with your MiSTer");
	$("#nav").hidden = true;
	const s = { found: [], searching: true, address: "", code: "", busy: false, error: "" };
	const draw = () => c.draw(`
		${note("On the MiSTer: Settings › Remote access. Turn it on, then choose Pair a device. The TV shows the address and a 6-digit code.")}
		${lbl("On this WiFi")}
		${flat(s.found.length ? s.found.map(f => row("Phosphor " + f.version, { value: f.address, accent: f.address === s.address }).replace('<div class="row', `<div data-act="pick" data-arg="${attr(f.address)}" class="row`)).join("")
			: row(s.searching ? "Looking for MiSTers…" : "None found", { dim: true, value: s.searching ? null : "Search again" }).replace('<div class="row', s.searching ? '<div class="row' : '<div data-act="search" class="row'))}
		${lbl("Address")}
		<input class="f" data-in="address" inputmode="url" autocomplete="off" placeholder="192.168.1.23:8767" value="${attr(s.address)}">
		${lbl("Code from the TV")}
		<input class="f code" data-in="code" inputmode="numeric" maxlength="6" placeholder="000000" value="${attr(s.code)}">
		${errp(s.error)}
		${btn(s.busy ? "Pairing…" : "Pair", "pair", { p: true, off: s.busy })}`);
	acts = {
		pick: a => { s.address = a; draw(); },
		search: () => find(),
		"input:address": v => { s.address = v; s.error = ""; },
		"input:code": (v, el) => { s.code = v.replace(/\D/g, "").slice(0, 6); if (el.value !== s.code) el.value = s.code; },
		pair: async () => {
			if (!s.address.trim() || s.code.length !== 6) { s.error = "Type the address and the 6-digit code from the TV"; draw(); return; }
			s.busy = true; s.error = ""; draw();
			try { await app("pair", { address: s.address, code: s.code }); go("home", true); }
			catch (e) { s.error = e.message; s.busy = false; draw(); }
		},
	};
	async function find() {
		s.searching = true; draw();
		try { const r = await app("discover"); s.found = r.found || []; if (r.error) s.error = r.error; }
		catch (e) { s.error = e.message; }
		s.searching = false;
		if (!s.address && s.found.length === 1) s.address = s.found[0].address;
		if (c.alive()) draw();
	}
	draw();
	find();
} };

/* ---------------------------------------------------------------- Home */
async function status() {
	const s = await call("status");
	const sc = await call("scrape").catch(() => null);
	const count = f => call("games", { filter: f, limit: 0 }).then(r => r.total || 0).catch(() => 0);
	const [missingArt, missingDetails, notFound] = await Promise.all([count("missing_art"), count("missing_details"), count("not_found")]);
	call("look").then(setLook).catch(() => {});
	return Object.assign(s, { scraperOn: sc && sc.on, scraperRunning: sc && sc.running, scraperStatus: sc && sc.status || "", missingArt, missingDetails, notFound });
}
const busyText = s => s.indexing ? "Looking for games…" : s.identifying ? "Identifying games…" : s.scraperRunning ? (s.scraperStatus || "Getting box art…") : null;
const artUrl = (id, v) => `art/${id}?v=${v || 0}`;
let artVer = 0;            /* bumps when box art changes, so pictures load again */

PAGES.home = { top: true, async show(c) {
	setBar("PHOSPHOR", true);
	let st = null, last = null, err = "";
	acts = {
		play: async () => { await call("launch", { id: last.id }); toast("Starting on the TV"); },
	};
	await c.every(async () => {
		try { st = await status(); last = (await call("last").catch(() => ({}))).game || null; err = ""; } catch (e) { err = e.message; }
		const busy = st && busyText(st);
		c.draw(`${errp(err)}
			${st ? card(`${lbl("Status")}<div class="tiles">
				<div class="tile"><small>Phosphor</small><b>${esc(st.version)}</b></div>
				<div class="tile"><small>Library</small><b>${(st.games || 0).toLocaleString()} games</b></div>
				<div class="tile${st.missingArt ? " on" : ""}"><small>Box art</small><b>${st.missingArt ? st.missingArt + " missing" : "All found"}</b></div>
				<div class="tile"><small>Details</small><b>${st.missingDetails ? st.missingDetails + " missing" : "All there"}</b></div>
				${busy ? `<div class="tile wide on"><small>Working</small><b>${esc(busy)}</b></div>` : ""}
			</div>`) : err ? "" : spin()}
			${last ? card(`${lbl("Continue")}
				<div style="display:flex;gap:12px;align-items:center">
					${last.art ? `<img class="cover" style="width:56px;height:74px;border-radius:8px;object-fit:cover" src="${artUrl(last.id, artVer)}" alt="">` : ""}
					<div style="min-width:0;flex:1"><b style="display:block;font-weight:500">${esc(last.title)}</b><span class="small">${esc(last.system_name || "")}</span></div>
				</div>
				<div class="btns">${btn("Play on TV", "play", { p: true })}<button type="button" class="b" data-go="game/${last.id}">Details</button></div>`) : ""}
			${st && (st.missingArt + st.missingDetails + st.notFound) > 0 ? lbl("Needs attention") + flat(
				(st.missingArt ? row("No box art", { value: String(st.missingArt), accent: true, go: "games?filter=missing_art&title=No%20box%20art" }) : "") +
				(st.missingDetails ? row("No details", { value: String(st.missingDetails), accent: true, go: "games?filter=missing_details&title=No%20details" }) : "") +
				(st.notFound ? row("Not found by the lookup", { value: String(st.notFound), go: "games?filter=not_found&title=Not%20found" }) : "")) : ""}
			${st ? lbl("Your games") + flat(
				row("Favorites", { go: "games?filter=favorites&title=Favorites" }) +
				row("Recently played", { go: "games?filter=recent&title=Recently%20played" }) +
				row(`${(st.games || 0).toLocaleString()} games in the library`, { sub: busy || "Phosphor " + st.version, go: "library" })) : ""}`);
		return st && (st.indexing || st.identifying || st.scraperRunning) ? 3000 : 10000;
	});
} };

/* ---------------------------------------------------------------- Library, games, a game */
const gameTags = g => `<span class="tags">${g.favorite ? '<span class="tag acc">favorite</span>' : ""}${g.linked ? '<span class="tag">based on</span>' : ""}${!g.art ? '<span class="tag acc">no art</span>' : ""}${!g.details ? '<span class="tag">no details</span>' : ""}</span>`;
const gameRow = (g, sys) => row(g.title, {
	sub: sys ? g.system : "", tags: gameTags(g), go: "game/" + g.id,
	pre: g.art ? `<img class="cover" loading="lazy" src="${artUrl(g.id, artVer)}" alt="">` : `<span class="cover" aria-hidden="true"></span>`,
});

PAGES.library = { top: true, async show(c) {
	setBar("Library");
	let cats = [], cat = Number(sessionStorage.getItem("cat") || 0), systems = null, q = "", results = null, total = 0, st = null, err = "", gen = -1, qT;
	const draw = () => {
		setChips([["0", "All"]].concat(cats.map((x, i) => [String(i + 1), x.name])), String(cat), v => { cat = Number(v); sessionStorage.setItem("cat", v); systems = null; draw(); loadSystems(); });
		const busy = st && (st.indexing ? "Looking for games…" : st.identifying ? "Identifying games…" : null);
		let body;
		if (q.trim()) body = results == null ? spin() : !results.length ? note("Nothing matches.") : note(total > results.length ? `${results.length} of ${total}` : `${total} games`) + flat(results.map(g => gameRow(g, true)).join(""));
		else body = (systems == null ? spin() : !systems.length ? flat(row("No games here yet", { sub: "Rescan looks for them", dim: true }))
			: flat(systems.map(s => row(s.name, { value: String(s.games), go: `games?system=${encodeURIComponent(s.id)}&title=${encodeURIComponent(s.name)}` })).join("")))
			+ card(`<h3>Rescan the library</h3><p class="small">${esc(busy || "Looks for new games and works out what they are. Runs only when you ask.")}</p>${btn("Rescan", "rescan", { off: !!busy })}`);
		const keep = document.activeElement && document.activeElement.dataset.in === "q";
		c.draw(`<input class="f" data-in="q" type="search" placeholder="Search every system" value="${attr(q)}">${errp(err)}${body}`);
		if (keep) { const i = $('[data-in="q"]'); i.focus(); i.setSelectionRange(i.value.length, i.value.length); }
	};
	const loadSystems = async () => {
		try {
			const id = cats[cat - 1] ? cats[cat - 1].id : undefined;
			systems = ((await call("systems", id != null ? { category: id } : {})).systems || []).filter(s => s.games > 0);
			err = "";
		} catch (e) { err = e.message; }
		if (c.alive()) draw();
	};
	acts = {
		rescan: async () => { await call("rescan"); toast("Looking for games"); st = await call("status"); draw(); },
		"input:q": v => {
			q = v; clearTimeout(qT);
			if (!v.trim()) { results = null; draw(); return; }
			qT = setTimeout(async () => {
				try { const r = await call("games", { q: v, limit: 500 }); if (q === v) { total = r.total; results = r.games || []; draw(); } } catch (e) { err = e.message; draw(); }
			}, 300);
		},
	};
	draw();
	await c.every(async () => {
		try {
			st = await call("status");
			if (st.generation !== gen) {
				gen = st.generation;
				cats = ((await call("categories")).categories || []).filter(x => x.games > 0);
				if (cat > cats.length) cat = 0;
				await loadSystems();
			}
			err = "";
		} catch (e) { err = e.message; draw(); }
		return st && (st.indexing || st.identifying) ? 3000 : 15000;
	});
} };

const FILTERS = [["", "All"], ["missing_art", "No box art"], ["missing_details", "No details"], ["not_found", "Not found"], ["linked", "Based on"]];
PAGES.games = { async show(c, p) {
	const system = p.q.system || null;
	setBar(p.q.title || "Games");
	let filter = p.q.filter || "", q = "", games = null, total = 0, err = "", qT;
	const draw = () => {
		const keep = document.activeElement && document.activeElement.dataset.in === "gq";
		c.draw(`<input class="f" data-in="gq" type="search" placeholder="Search titles" value="${attr(q)}">
			${errp(err)}
			${games == null ? spin() : !games.length ? note(!filter && !q ? "No games here." : "Nothing matches.")
				: note(total > games.length ? `${games.length} of ${total}` : `${total} games`) + flat(games.map(g => gameRow(g, !system)).join(""))}`);
		if (keep) { const i = $('[data-in="gq"]'); i.focus(); i.setSelectionRange(i.value.length, i.value.length); }
	};
	const load = async () => {
		if (!system && !filter && !q.trim()) { games = []; total = 0; draw(); return; }
		try {
			const r = await call("games", { system: system || undefined, filter: filter || undefined, q: q.trim() || undefined, limit: 500 });
			total = r.total; games = r.games || []; err = "";
		} catch (e) { err = e.message; }
		if (c.alive()) draw();
	};
	const chips = () => setChips(FILTERS, filter, v => { filter = v; games = null; chips(); draw(); load(); });
	chips();
	acts = { "input:gq": v => { q = v; clearTimeout(qT); qT = setTimeout(load, 300); } };
	draw();
	load();
} };

const SCRAPE_TEXT = { found: "Found", not_found: "Not found by the sources", linked: "Uses the base game's", ready: "Waiting for the lookup", unidentified: "Not identified yet" };
function fsize(n) { return n >= 1 << 30 ? (n / 1073741824).toFixed(1) + " GB" : n >= 1 << 20 ? (n / 1048576).toFixed(1) + " MB" : Math.floor(n / 1024) + " KB"; }
PAGES.game = { async show(c, p) {
	const id = Number(p.arg);
	let g = null, err = "", busy = "";
	const draw = () => {
		if (!g) { c.draw(err ? errp(err) : spin()); return; }
		setBar(g.title);
		const d = (k, name) => row(name, { value: g[k] || "—", dim: !g[k] });
		const b = g.based_on;
		c.draw(`<div class="gtop">
				${g.art ? `<img src="${artUrl(id, artVer)}" alt="${attr(g.title)}">` : `<div class="noart" aria-hidden="true">${esc((g.title || "?").slice(0, 1).toUpperCase())}</div>`}
				<div class="btns-col">${btn("Play on TV", "launch", { p: true })}${btn(g.favorite ? "★ Favorite" : "☆ Favorite", "fav")}${btn("Change box art", "art")}</div>
			</div>
			${busy ? `<p class="busy">${esc(busy)}</p>` : ""}${errp(err)}
			${lbl("Details")}
			${flat(d("year", "Year") + d("developer", "Developer") + d("publisher", "Publisher") + d("genre", "Genre") + d("players", "Players") + (g.region ? d("region", "Region") : "") + row("Edit details", { go: "edit/" + id, accent: true }))}
			<p class="desc${g.description ? "" : " small"}">${esc(g.description || "No description yet.")}</p>
			${lbl("Based on")}
			${flat(row(b && b.title ? b.title : "Not based on another game", { sub: b && b.title ? "Uses its box art and details unless this game has its own" : "For hacks, mods and translations", dim: !(b && b.title), go: "based/" + id }))}
			${lbl("Box art and details lookup")}
			${card(`<p class="small" style="color:var(--text)">${esc(SCRAPE_TEXT[g.scrape] || String(g.scrape || "").replace(/_/g, " "))}</p>${btn("Look up again", "rescrape")}`)}
			${lbl("Files")}
			${flat((g.files || []).map(f => row(String(f.path || "").split("/").pop(), { sub: [f.where, fsize(f.size || 0), f.serial, f.crc32 && "CRC " + f.crc32, f.online === false && "offline"].filter(Boolean).join("  ·  "), dim: f.online === false })).join(""))}
			${g.play_count > 0 ? note(`Played ${g.play_count} times, ${Math.floor(g.play_seconds / 3600)} h ${Math.floor(g.play_seconds / 60) % 60} min`) : ""}`);
	};
	const act = async (what, f) => { busy = what; err = ""; draw(); try { await f(); } catch (e) { err = e.message; } busy = ""; if (c.alive()) draw(); };
	acts = {
		launch: () => act("Starting it on the TV…", async () => { await call("launch", { id }); toast("Starting on the TV"); }),
		fav: () => act("", async () => { await call("favorite", { id, on: !g.favorite }); g = (await call("game", { id })).game; }),
		rescrape: () => act("Asking the MiSTer to look it up again…", async () => { await call("rescrape", { id }); toast("Queued: it fills only empty fields"); }),
		art: () => act("Sending the picture…", async () => {
			const r = await app("pickArt", { id });
			if (r && r.game) { g = r.game; artVer++; toast("Box art changed"); }
		}),
	};
	try { g = (await call("game", { id })).game; } catch (e) { err = e.message; }
	draw();
} };

const EDIT_FIELDS = [["title", "Title"], ["year", "Year"], ["developer", "Developer"], ["publisher", "Publisher"], ["genre", "Genre"], ["players", "Players"], ["region", "Region"], ["description", "Description"]];
PAGES.edit = { async show(c, p) {
	const id = Number(p.arg);
	setBar("Edit details");
	let g, err = "", busy = false;
	try { g = (await call("game", { id })).game; } catch (e) { c.draw(errp(e.message)); return; }
	const orig = {}, vals = {};
	EDIT_FIELDS.forEach(([k]) => { orig[k] = vals[k] = g[k] || ""; });
	const draw = () => c.draw(EDIT_FIELDS.map(([k, name]) => `<div class="flbl">${esc(name)}</div>${k === "description"
		? `<textarea class="f" data-in="e:${k}" placeholder="A few sentences about the game">${esc(vals[k])}</textarea>`
		: `<input class="f" data-in="e:${k}" value="${attr(vals[k])}">`}`).join("")
		+ errp(err) + btn(busy ? "Saving…" : "Save", "save", { p: true, off: busy })
		+ note("Empty fields are cleared. The MiSTer's box art lookup never overwrites what you write here."));
	acts = { save: async () => {
		const set = {};
		EDIT_FIELDS.forEach(([k]) => { if (vals[k] !== orig[k]) set[k] = vals[k].trim(); });
		if (!Object.keys(set).length) { back(); return; }
		busy = true; draw();
		try { await call("edit", { id, set }); back(); } catch (e) { err = e.message; busy = false; draw(); }
	} };
	EDIT_FIELDS.forEach(([k]) => { acts["input:e:" + k] = v => { vals[k] = v; }; });
	draw();
} };

PAGES.based = { async show(c, p) {
	const id = Number(p.arg);
	setBar("Based on");
	let g = null, q = "", results = null, err = "", qT;
	const draw = () => {
		const keep = document.activeElement && document.activeElement.dataset.in === "bq";
		const cur = g && g.based_on && g.based_on.title;
		c.draw(`${note("For a hack, mod, translation or other version: pick the original, and this game uses its box art and details (unless it has its own).")}
			${cur ? flat(row("Now: " + cur, { value: "Remove", accent: true }).replace('<div class="row', '<div data-act="unlink" class="row')) : ""}
			<input class="f" data-in="bq" type="search" placeholder="Search for the original" value="${attr(q)}">
			${errp(err)}
			${results == null ? spin() : !results.length ? note("No matches. Try a shorter name.") : flat(results.map(x => row(x.title, { value: x.score >= 400 ? "close match" : null }).replace('<div class="row', `<div data-act="link" data-arg="${x.id}" class="row`)).join(""))}`);
		if (keep) { const i = $('[data-in="bq"]'); i.focus(); i.setSelectionRange(i.value.length, i.value.length); }
	};
	const load = async () => { try { results = (await call("candidates", { id, q: q.trim() || undefined, limit: 30 })).games || []; err = ""; } catch (e) { err = e.message; } if (c.alive()) draw(); };
	acts = {
		"input:bq": v => { q = v; clearTimeout(qT); qT = setTimeout(load, 300); },
		link: async base => { await call("link", { id, base: Number(base) }); back(); },
		unlink: async () => { await call("unlink", { id }); back(); },
	};
	try { g = (await call("game", { id })).game; } catch (e) { err = e.message; }
	draw();
	load();
} };

/* ---------------------------------------------------------------- Saves: memory cards, backups, cloud */
const SAVES_TABS = [["cards", "Memory cards"], ["backups", "Backups"], ["cloud", "Cloud"]];
const PAD_BUTTONS = ["Select", "L3", "R3", "Start", "Up", "Right", "Down", "Left", "L2", "R2", "L1", "R1", "Triangle", "Circle", "Cross", "Square"];
function cardLabel(b) {
	const m = /port(\d)-(\d{4})(\d{2})(\d{2})-(\d{2})(\d{2})/.exec(b.name);
	return m ? `Port ${m[1]} card  ·  ${m[2]}-${m[3]}-${m[4]} ${m[5]}:${m[6]}` : b.name;
}
const reasonText = r => r === "manual" ? "Backed up by hand" : r === "daily" ? "Daily" : r === "pre-restore" ? "Before a restore" : /^exit-/.test(r) ? "After " + r.slice(5) : r;
PAGES.saves = { top: true, async show(c, p) {
	setBar("Saves");
	const tab = SAVES_TABS.some(t => t[0] === p.arg) ? p.arg : "cards";
	setChips(SAVES_TABS, tab, v => go("saves/" + v, true));
	if (tab === "cards") return savesCards(c);
	if (tab === "backups") return savesBackups(c);
	return savesCloud(c);
} };

async function savesCards(c) {
	let st = null, pads = [], err = "", pick = null, n = 0, wasBusy = false;
	const draw = () => {
		if (!st) { c.draw(err ? errp(err) : spin()); return; }
		const pc = st.job && st.job.total ? Math.floor(st.job.done * 100 / st.job.total) : 0;
		const j = st.job || {}, P = st.ports || {};
		let h = errp(err);
		if (j.busy) h += `<p class="busy">${esc(j.name === "prepare" ? "Loading the Phosphor menu core…" : j.name === "backup" ? `Backing up the card in port ${j.port}… ${pc}%` : j.name === "restore" ? (j.done < j.total / 2 ? "Backing up the card first… " : "Writing… ") + pc + "%" : "Working…")}</p>`;
		if (!P.main_api) return c.draw(h + flat(row("Needs Phosphor's Main", { sub: "phosphor-main isn't running on the MiSTer", dim: true })));
		if (P.in_game) return c.draw(h + flat(row("In a game", { sub: "The front ports are read only from the menu", dim: true })));
		if (!P.available && !j.busy) return c.draw(h + flat(row("Front ports off", { sub: P.can_load ? "Loads the Phosphor menu core (a few seconds)" : "The Phosphor menu core isn't installed", dim: true })) + (P.can_load ? btn("Turn on the front ports", "prepare", { p: true }) : ""));
		if (pick) return c.draw(h + note(`${cardLabel(pick)}: put it on the card in which port? The card is backed up first, so this can be undone from the list.`) + `<div class="btns">${btn("Port 1", "restore", { p: true, arg: 1, off: j.busy })}${btn("Port 2", "restore", { arg: 2, off: j.busy })}</div>${btn("Cancel", "cancel")}`);
		const digital = pads.filter(x => x.connected && x.id === 0x41);
		c.draw(h + (digital.length ? card(`<h3>DualShock in digital mode</h3><p class="small">Port ${digital.map(x => x.port).join(" and ")}: some games need analog</p>${btn("Switch DualShocks to analog", "analog")}`, "on") : "")
			+ lbl("Memory cards") + `<div class="btns">${btn("Back up port 1", "backup", { p: true, arg: 1, off: j.busy })}${btn("Back up port 2", "backup", { arg: 2, off: j.busy })}</div>`
			+ ((st.backups || []).length ? flat(st.backups.map((b, i) => row(cardLabel(b), { sub: /^port/.test(b.name) ? "" : whenText(b.at * 1000), value: "Put back ›" }).replace('<div class="row', `<div data-act="pick" data-arg="${i}" class="row`)).join("")) : "")
			+ note("Backups are whole cards (.mcd, like MiSTer's PSX core) kept in phosphor/memcards on the MiSTer. MT32-pi and MIDI are off while the front ports are in use."));
	};
	const start = async (what, port, name) => { st = await call("memcard", { do: what, port, name }); draw(); };
	acts = {
		prepare: () => start("prepare"), backup: a => start("backup", Number(a)),
		pick: i => { pick = st.backups[Number(i)]; draw(); }, cancel: () => { pick = null; draw(); },
		restore: a => { const b = pick; pick = null; return start("restore", Number(a), b.name); },
		analog: async () => { for (const x of pads.filter(x => x.connected && x.id === 0x41)) await call("pad_analog", { port: x.port }).catch(() => {}); pads = (await call("pads")).pads || []; draw(); },
	};
	await c.every(async () => {
		if (n % 8 === 0 || (st && st.job && st.job.busy && n % 3 === 0)) {
			try {
				st = await call("memcard"); err = "";
				const j = st.job || {};
				if (wasBusy && !j.busy && (j.name !== "prepare" || !j.ok) && j.message) toast(j.message, !j.ok);
				wasBusy = !!j.busy;
			} catch (e) { err = e.message; }
			draw();
		}
		if (st && st.ports && st.ports.available && n % 16 === 0) { pads = (await call("pads").catch(() => ({ pads: [] }))).pads || []; draw(); }
		n++;
		return 250;
	});
}

async function savesBackups(c) {
	let st = null, err = "", preview = null, files = [], seen = -1, wasBusy = false;
	const draw = () => {
		if (!st) { c.draw(err ? errp(err) : spin()); return; }
		const S = st.status || {}, j = st.job || {}, snaps = st.snapshots || [];
		let h = errp(err) + (j.busy ? `<p class="busy">${j.name === "now" ? "Backing up…" : j.name === "restore" ? "Restoring…" : "Looking inside…"}</p>` : "");
		if (!st.installed) return c.draw(h + flat(row("Not installed", { sub: "Install the backup-agent module on the MiSTer", dim: true })));
		if (preview) return c.draw(h + note(`${preview.when} (${reasonText(preview.reason)}). Restore saves puts back only saves and save states; Everything also puts back the settings in it. A backup is taken first, so either can be undone.`)
			+ flat(files.length ? files.map(f => row(f)).join("") : row("No saves in this backup", { dim: true }))
			+ `<div class="btns">${btn("Restore saves", "rsaves", { p: true, off: j.busy || !files.length })}${btn("Everything", "rall", { off: j.busy })}</div>${btn("Back", "unpick")}`);
		c.draw(h + flat(row(`${snaps.length} backups`, { sub: S.last_error || S.dest || "" })) + btn("Back up now", "now", { p: true, off: j.busy })
			+ lbl("Backups") + flat(snaps.map((s, i) => row(s.when, { value: `${reasonText(s.reason)}  ·  ${s.kb} KB`, dim: s.reason === "pre-restore" }).replace('<div class="row', `<div data-act="pick" data-arg="${i}" class="row`)).join("") || row("None yet", { dim: true }))
			+ note("Taken on the MiSTer when you leave a game and once a day. They stay on its drives, never in the cloud."));
	};
	const start = async (what, name, only) => { st = await call("backup", { do: what, name, only }); draw(); };
	acts = {
		now: () => start("now"),
		pick: i => { preview = st.snapshots[Number(i)]; files = []; draw(); return start("preview", preview.name, "saves|savestates"); },
		unpick: () => { preview = null; draw(); },
		rsaves: () => start("restore", preview.name, "saves|savestates"),
		rall: () => start("restore", preview.name),
	};
	await c.every(async () => {
		try {
			st = await call("backup"); err = "";
			const j = st.job || {};
			if (wasBusy && !j.busy && j.at !== seen) {
				seen = j.at;
				if (j.name === "preview") files = j.ok ? String(j.output || "").split("\n").filter(l => l.trim() && !l.endsWith("/") && !l.startsWith("Would restore")) : [];
				if (j.name === "now") toast(j.ok ? "Backed up" : "Backup failed", !j.ok);
				if (j.name === "restore") { toast(j.ok ? "Restored" : "Restore failed: nothing changed", !j.ok); preview = null; }
			}
			wasBusy = !!j.busy;
		} catch (e) { err = e.message; }
		draw();
		return st && st.job && st.job.busy ? 800 : 5000;
	});
}

const CLOUD_KINDS = [["webdav", "WebDAV (NAS, Nextcloud)"], ["sftp", "SFTP"], ["smb", "Windows share (SMB)"], ["drive", "Google Drive"], ["dropbox", "Dropbox"], ["onedrive", "OneDrive"]];
const CLOUD_BUSY = { setup: "Downloading rclone on the MiSTer…", push: "Sending…", pull: "Getting saves back…", preview: "Checking what would come back…", test: "Checking the connection…", devices: "Looking in the cloud…", add: "Adding…", use: "Choosing…" };
async function savesCloud(c) {
	let st = null, err = "", mode = "main", lines = [], from = "", seen = -1, wasBusy = false;
	const form = { kind: "webdav", address: "", user: "", password: "", share: "", token: "" };
	const draw = () => {
		if (!st) { c.draw(err ? errp(err) : spin()); return; }
		const S = st.status || {}, j = st.job || {}, ready = !!S.rclone, places = st.remotes || [];
		let h = errp(err) + (j.busy ? `<p class="busy">${esc(CLOUD_BUSY[j.name] || "Working…")}</p>` : "");
		if (mode === "devices") return c.draw(h + note("Each MiSTer has its own copy in the cloud.") + flat(lines.map(l => { const me = l.endsWith("(this MiSTer)"), name = l.replace(/\s+\(this MiSTer\)$/, ""); return row(name, { value: me ? "this MiSTer" : "another MiSTer", accent: me }).replace('<div class="row', `<div data-act="device" data-arg="${attr(name)}" class="row`); }).join("")) + btn("Back", "main"));
		if (mode === "preview") {
			const nothing = !lines.length || lines[0].startsWith("Nothing");
			return c.draw(h + note(`From ${from}. Newer files win; anything replaced on the MiSTer is kept in phosphor/backup/cloud-sync first.`) + flat(lines.map(l => row(l.trim(), { dim: l.startsWith(" ") })).join("")) + (nothing ? "" : btn("Get them back", "pull", { p: true, off: j.busy })) + btn("Back", "main"));
		}
		if (mode === "add") {
			const oauth = ["drive", "dropbox", "onedrive"].includes(form.kind);
			const ok = oauth ? form.token.trim().startsWith("{") : form.address.trim() && (form.kind !== "smb" || form.share.trim());
			return c.draw(h + `<div style="display:flex;flex-wrap:wrap;gap:8px">${CLOUD_KINDS.map(([k, n]) => `<button type="button" class="chip${form.kind === k ? " on" : ""}" data-act="kind" data-arg="${k}">${esc(n)}</button>`).join("")}</div>`
				+ (oauth ? note(`Sign in once on a PC: install rclone (rclone.org/downloads), run  rclone authorize "${form.kind}" , sign in in the browser, then copy the whole {"access_token":...} line it prints and paste it here. It's stored only on your MiSTer.`)
					+ `<div class="flbl">Sign-in token</div><textarea class="f" data-in="c:token" placeholder='{"access_token":...}'>${esc(form.token)}</textarea>`
					: `<div class="flbl">${form.kind === "webdav" ? "WebDAV address" : "Computer or NAS"}</div><input class="f" data-in="c:address" value="${attr(form.address)}" placeholder="${form.kind === "webdav" ? "https://nas.local/remote.php/dav/files/me" : "192.168.1.5"}">`
					+ (form.kind === "smb" ? `<div class="flbl">Share name</div><input class="f" data-in="c:share" value="${attr(form.share)}" placeholder="Backups">` : "")
					+ `<div class="flbl">User name</div><input class="f" data-in="c:user" value="${attr(form.user)}" autocomplete="off"><div class="flbl">Password</div><input class="f" type="password" data-in="c:password" value="${attr(form.password)}" autocomplete="off">`)
				+ btn("Add and use it", "submit", { p: true, off: !ok || j.busy }) + btn("Back", "main") + note("Passwords are stored on the MiSTer only (obscured by rclone), never in the cloud."));
		}
		let top;
		if (!st.installed) top = row("Not installed", { sub: "Install the cloud-sync module on the MiSTer (modules/cloud-sync/install.sh)", dim: true });
		else if (!ready) top = row("Download rclone", { sub: "Once, about 25 MB, checked against rclone's checksums", accent: true, value: "›" }).replace('<div class="row', '<div data-act="setup" class="row');
		else if (!S.remote) top = row("No storage place yet", { sub: "Add one below", dim: true });
		else top = row("Sending to " + S.remote, { sub: S.last_error ? S.last_error : S.last_push ? `Last sent ${S.last_push}  ·  ${S.device}` : `Not sent yet  ·  ${S.device}` });
		c.draw(h + flat(top) + (ready && S.remote ? `<div class="btns">${btn("Send now", "push", { p: true, off: j.busy })}${btn("Get saves back", "devices", { off: j.busy })}</div>` : "")
			+ (ready ? lbl("Storage places") + flat(places.map(x => row(x.name, { value: x.in_use ? "in use" : "Use", sub: x.type, accent: x.in_use }).replace('<div class="row', x.in_use ? '<div class="row' : `<div data-act="use" data-arg="${attr(x.name)}" class="row`)).join("")
				+ row("Add a storage place", { value: "›" }).replace('<div class="row', '<div data-act="add" class="row') + (S.remote ? row("Check the connection", { value: "›" }).replace('<div class="row', '<div data-act="test" class="row') : "")) : "")
			+ note("Sent after each game: saves, save states and core settings, with older versions kept in the cloud. WiFi networks, Bluetooth pairings and passwords are never sent."));
	};
	const start = async (what, extra) => { st = await call("cloud", Object.assign({ do: what }, extra || {})); draw(); };
	acts = {
		setup: () => start("setup"), push: () => start("push"), devices: () => start("devices"), test: () => start("test"),
		device: name => { from = name; return start("preview", { from: name }); },
		pull: () => start("pull", { from }),
		use: name => start("use", { name }),
		add: () => { mode = "add"; draw(); }, main: () => { mode = "main"; draw(); },
		kind: k => { form.kind = k; draw(); },
		submit: () => {
			const settings = ["drive", "dropbox", "onedrive"].includes(form.kind) ? { token: form.token.trim() }
				: form.kind === "webdav" ? { url: form.address.trim(), vendor: "other", user: form.user, pass: form.password } : { host: form.address.trim(), user: form.user, pass: form.password };
			Object.keys(settings).forEach(k => { if (!settings[k]) delete settings[k]; });
			return start("add", { name: form.kind, type: form.kind, settings });
		},
	};
	["token", "address", "share", "user", "password"].forEach(k => { acts["input:c:" + k] = v => { form[k] = v; }; });
	await c.every(async () => {
		try {
			st = await call("cloud"); err = "";
			const j = st.job || {};
			if (wasBusy && !j.busy && j.at !== seen) {
				seen = j.at;
				const out = String(j.output || "").split("\n").filter(l => l.trim());
				if (j.name === "devices" && j.ok) { lines = out; mode = "devices"; }
				else if (j.name === "preview") { lines = out; mode = "preview"; }
				else if (j.name === "add") {
					if (j.ok) {
						const extra = form.kind === "smb" ? { name: form.kind, folder: form.share + "/Phosphor" } : { name: form.kind };
						try { st = await call("cloud", Object.assign({ do: "use" }, extra)); mode = "main"; Object.assign(form, { address: "", user: "", password: "", share: "", token: "" }); } catch (e) { err = e.message; }
					} else toast(out[0] || "Couldn't add it", true);
				}
				else if (j.name === "push") toast(j.ok ? "Saves sent" : out[out.length - 1] || "Sending failed", !j.ok);
				else if (j.name === "pull") { toast(j.ok ? "Saves are back" : "Couldn't get them back", !j.ok); mode = "main"; }
				else if (j.name === "test") toast(j.ok ? "Connection OK" : out[out.length - 1] || "Couldn't reach it", !j.ok);
				else if (j.name === "setup") toast(j.ok ? "rclone is ready" : out[out.length - 1] || "Download failed", !j.ok);
				else if (j.name === "use" && !j.ok) toast(out[out.length - 1] || "Couldn't choose it", true);
			}
			wasBusy = !!j.busy;
		} catch (e) { err = e.message; }
		const typing = document.activeElement && /^(INPUT|TEXTAREA)$/.test(document.activeElement.tagName);
		if (!typing) draw();
		return st && st.job && st.job.busy ? 1000 : 5000;
	});
}

/* ---------------------------------------------------------------- Video: MiSTer's video settings, tried with Keep */
const DIP_NOTE_UNITS = "SuperStation One units made after December 2025.";
/** which DIP switches on the side of the SS1 a set of analog settings needs (SS1 Tool's setup wizard) */
function dipFor(v, profileLabel) {
	const mode = v.vga_mode || "rgb", csync = v.composite_sync !== "0", pal = v.menu_pal === "1";
	const reg = pal ? "Up (PAL)" : "Down (NTSC)";
	const lab = (profileLabel || "").toLowerCase();
	if (mode === "subcarrier") return [["3", "Down (Sony encoder)"], ["1", reg], ["2", reg], ["4", "Up (off)"]];
	if (mode === "svideo" || mode === "cvbs") return [["3", "Up (FPGA encoder)"], ["1", reg], ["2", reg], ["4", "Up (off)"]];
	if (mode === "ypbpr") return [["4", "Down (sync on Y)"]];
	if (mode === "rgb" && !csync) return [["4", "Up (off)"]];
	if (lab === "rgsb") return [["4", "Down (sync on green)"]];
	return [["4", "Up for SCART and RGB with separate sync; Down for sync on green"]];
}
const dipBox = rows => `<div class="dip"><b>Set on the side of the SS1 first</b><div class="g">${rows.map(([n, s]) => `<span>DIP ${n}</span><span>${esc(s)}</span>`).join("")}</div><span class="small">${DIP_NOTE_UNITS} The ini and the switches have to agree: otherwise the picture is black and white, or missing.</span></div>`;
const ANALOG_KEYS = new Set(["vga_mode", "composite_sync", "vga_sog", "menu_pal"]);
PAGES.video = { top: true, async show(c) {
	setBar("Video");
	let v = null, err = "", file = null, edits = {}, tick = null;
	const valueName = (k, val) => { const o = VOPT[k]; const x = o && o.v.find(e => e[0] === val); return x ? x[1] : (val === "" ? "Not set (" + (o ? o.d : "default") + ")" : val); };
	const cur = k => { const s = v.settings[k]; return s && s.set ? s.value : ""; };
	const draw = () => {
		if (!v) { c.draw(err ? errp(err) : spin()); return; }
		const pend = v.pending, files = (v.files || []).filter(f => f.profile >= 0);
		const shown = files.find(f => f.name === v.file) || files[0];
		const using = shown && shown.active;
		setChips(files.map(f => [f.name, f.label + (f.active ? " ✓" : "")]), v.file, name => { file = name; edits = {}; load(); });
		let h = errp(err);
		if (pend) {
			const what = pend.kind === "profile" ? "profile " + ((files.find(f => f.profile === pend.changes.profile) || {}).label || pend.changes.profile)
				: pend.changes.undo ? "the settings from before" : Object.keys(pend.changes).map(k => (VOPT[k] ? VOPT[k].n : k) + " " + valueName(k, pend.changes[k])).join(", ");
			h += `<section id="keep" class="card on" aria-live="polite"><div style="display:flex;justify-content:space-between;gap:8px;align-items:baseline"><h3>Trying ${esc(what)}</h3><span class="cd" id="cd">${pend.secs_left} s</span></div>
				<p class="small">Picture OK? Keep it. Otherwise the SS1 goes back by itself, even with no picture or with this phone gone.</p>
				<div class="btns">${btn("Keep", "keep", { p: true })}${btn("Put back now", "revert")}</div></section>`;
		}
		if (v.game_running) h += card(`<p class="small">A game is running: close it first (trying restarts the menu).</p>`, "warn");
		if (shown && !using) {
			h += card(`<h3>${esc(shown.label)} isn't in use</h3><p class="small">${esc(shown.name)}. Main uses ${esc((files.find(f => f.active) || {}).label || "Main")} now. Try switching to it: it goes back by itself unless you keep it.</p>
				${dipBox(dipFor(Object.fromEntries(Object.entries(v.settings).map(([k, s]) => [k, s.set ? s.value : ""])), shown.label))}
				${btn("Try " + shown.label, "profile", { p: true, arg: shown.profile, off: !!pend || v.game_running })}`);
		}
		const grp = {};
		Object.keys(VOPT).forEach(k => { (grp[VOPT[k].g] = grp[VOPT[k].g] || []).push(k); });
		VGROUPS.forEach(([g, name]) => {
			const keys = grp[g] || [];
			if (!keys.length) return;
			h += lbl(name) + card(keys.map(k => {
				const o = VOPT[k], now = cur(k), val = k in edits ? edits[k] : now, changed = k in edits && edits[k] !== now;
				const rk = VRISK[o.r] || ["muted", ""];
				const ov = (v.settings[k] && v.settings[k].overrides || []);
				const analog = ANALOG_KEYS.has(k) && changed;
				return `<div class="vrow"><div class="vh"><b>${esc(o.n)}</b>${rk[1] ? `<span class="risk ${rk[0]}">${esc(rk[1])}</span>` : ""}</div>
					<p class="small">${esc(o.t || "")}</p>${o.w ? `<p class="vw">${esc(o.w)}</p>` : ""}
					${ov.length ? `<p class="vw">Some cores set their own: ${esc(ov.join(", "))}</p>` : ""}
					<div class="vpick">${btn("◀", "prev", { arg: k, off: !using || !!pend })}<div class="vv${changed ? " changed" : ""}">${esc(valueName(k, val))}</div>${btn("▶", "next", { arg: k, off: !using || !!pend })}</div>
					${analog ? dipBox(dipFor(Object.assign(Object.fromEntries(Object.keys(VOPT).map(x => [x, cur(x)])), edits), shown && shown.label)) : ""}
					${changed ? `<div class="btns">${btn("Try", "try", { p: true, arg: k, off: !!pend || v.game_running })}${btn("Cancel", "cancel", { arg: k })}</div>` : ""}</div>`;
			}).join(""), "flat");
		});
		h += card(`<h3>Go back to before</h3><p class="small">Tries the copy of ${esc(v.file)} from before the last change (kept in phosphor/backups/ini on the SD card).</p>${btn("Try the earlier settings", "undo", { off: !using || !!pend || v.game_running })}`);
		h += note("Each try keeps the old file first, restarts the menu with the change, and goes back by itself unless you press Keep within " + (v.secs || 35) + " seconds. Based on the SuperStation wiki and documentation and MiSTer's own documentation.");
		c.draw(h);
	};
	const load = async () => {
		try { v = await call("vset", file ? { file } : {}); if (!file) file = v.file; err = ""; } catch (e) { err = e.message; }
		if (c.alive()) draw();
		countdown();
	};
	function countdown() {
		clearInterval(tick);
		if (!v || !v.pending) return;
		let left = v.pending.secs_left;
		tick = setInterval(() => {
			if (!c.alive()) { clearInterval(tick); return; }
			left = Math.max(0, left - 1);
			const el = $("#cd");
			if (el) el.textContent = left + " s";
			if (left <= 0) { clearInterval(tick); setTimeout(load, 2500); }
		}, 1000);
	}
	const step = (k, d) => {
		const o = VOPT[k], list = o.v.map(e => e[0]), now = k in edits ? edits[k] : cur(k);
		let i = list.indexOf(now);
		i = i < 0 ? 0 : (i + d + list.length) % list.length;
		edits[k] = list[i];
		if (edits[k] === cur(k)) delete edits[k];
		draw();
	};
	acts = {
		prev: k => step(k, -1), next: k => step(k, 1),
		cancel: k => { delete edits[k]; draw(); },
		try: async k => {
			const t = {};
			t[k] = edits[k];
			/* analog changes go together (output, sync, sync on green, PAL menu) */
			if (ANALOG_KEYS.has(k)) Object.keys(edits).filter(x => ANALOG_KEYS.has(x)).forEach(x => { t[x] = edits[x]; });
			v = await call("vset", { file, try: t });
			Object.keys(t).forEach(x => delete edits[x]);
			draw(); countdown();
		},
		profile: async n => { v = await call("vset", { profile: Number(n) }); draw(); countdown(); },
		undo: async () => { v = await call("vset", { file, undo: true }); draw(); countdown(); },
		keep: async () => { v = await call("vset", { file, keep: true }); toast("Kept"); file = null; edits = {}; await load(); },
		revert: async () => { v = await call("vset", { file, revert: true }); toast("Put back"); draw(); countdown(); },
	};
	await load();
	/* the try's state can change on the TV too (Keep there, or the timer): look again now and then */
	await c.every(async () => { if (v && v.pending) { await load(); return 3000; } return 15000; });
	clearInterval(tick);
} };

/* ---------------------------------------------------------------- Box art */
const SOURCE_ABOUT = {
	libretro: "No account. Box art from libretro's thumbnails, matched by each file's content (CRC or product code), then by its exact name.",
	arcadedb: "No account. Arcade games by their MAME set name.",
	launchbox: "No account. LaunchBox's games database: on the phone it's downloaded once (about 110 MB) and kept in the app's cache.",
	thegamesdb: "Needs a free API key from thegamesdb.net.",
	screenscraper: "Your screenscraper.fr login is enough (a free account). A developer login works too, if you have one.",
	igdb: "Needs a free Twitch developer app (dev.twitch.tv): its Client ID and secret.",
};
const SOURCE_FIELDS = {
	screenscraper: [["user", "User name", false], ["password", "Password", true], ["devid", "Developer id (only if you have one)", false], ["devpassword", "Developer password", true]],
	thegamesdb: [["apikey", "API key (free, from thegamesdb.net)", true]],
	igdb: [["client_id", "Client ID (Twitch developer app)", false], ["client_secret", "Client secret", true]],
};
PAGES.boxart = { async show(c, p) {
	setBar("Box art");
	const tab = p.arg === "sources" ? "sources" : "lookup";
	setChips([["lookup", "Look up"], ["sources", "Sources"]], tab, x => go("boxart/" + x, true));
	if (tab === "lookup") {
		let phone = {}, ss1 = null, err = "";
		const draw = () => {
			const pct = phone.total ? Math.floor(phone.done * 100 / phone.total) : 0;
			const counts = x => `${x.found} found, ${x.notFound != null ? x.notFound : x.not_found} not found` + (x.later ? `, ${x.later} for later` : "");
			c.draw(errp(err) + lbl("On this phone") + flat(phone.running ? row(phone.status || "Looking up box art", { value: phone.total ? pct + "%" : null, sub: `${phone.done} of ${phone.total}  ·  ` + counts(phone) }) + (phone.game ? row(phone.game, { sub: phone.system, dim: true }) : "")
				: phone.started > 0 ? row(phone.status || "Done", { sub: counts(phone) })
				: row("Much faster than the MiSTer", { sub: "The phone fetches the lists and pictures, makes the box art and sends it to the MiSTer. It keeps going with the app in the background.", dim: true }))
				+ (phone.notes || []).map(n => `<p class="vw">${esc(n)}</p>`).join("")
				+ btn(phone.running ? "Stop" : "Look up on this phone", "phone", { p: !phone.running })
				+ note("Each picture is kept on the phone until the MiSTer has it, then deleted (Storage > This phone). The first run downloads the game lists (libretro, and LaunchBox's 110 MB once): use WiFi.")
				+ lbl("On the MiSTer") + flat(!ss1 ? row("Asking the MiSTer…", { dim: true }) : ss1.running ? row(ss1.status || "Getting box art", { value: ss1.total ? Math.floor(ss1.done * 100 / ss1.total) + "%" : null, sub: `${ss1.done} of ${ss1.total}  ·  ` + counts(ss1) + (ss1.game ? `\n${ss1.system} › ${ss1.game}` : "") })
				: row("Not running", { sub: ss1.status || "Slower, but the phone can be put away", dim: true }))
				+ (ss1 && !ss1.running ? btn("Look up on the MiSTer", "ss1", { off: phone.running }) : ""));
		};
		on("phone", s => { phone = s; if (c.alive() && R.name === "boxart") draw(); });
		acts = {
			phone: async () => { phone = await app("phoneScrape", { start: !phone.running }); draw(); },
			ss1: async () => { await call("scrape", { on: true }); toast("Looking up box art on the MiSTer (slower)"); ss1 = await call("scrape_progress"); draw(); },
		};
		phone = await app("phoneScrape", {}).catch(() => ({}));
		await c.every(async () => { try { ss1 = await call("scrape_progress"); err = ""; } catch (e) { err = e.message; } draw(); return ss1 && ss1.running ? 2000 : 6000; });
		return;
	}
	let sources = null, accounts = [], sel = 0, vals = {}, err = "", busy = false;
	const fill = () => {
		vals = {};
		const x = sources && sources[sel];
		(x && SOURCE_FIELDS[x.id] || []).forEach(([k, , secret]) => { const a = accounts.find(a => a.sec === x.id && a.key === k); vals[k] = secret ? "" : (a && a.value) || ""; });
	};
	const draw = () => {
		if (!sources) { c.draw(err ? errp(err) : spin()); return; }
		const isSet = (x, k) => (accounts.find(a => a.sec === x.id && a.key === k) || {}).set;
		const x = sources[sel];
		c.draw(errp(err) + note("Asked in this order, by the MiSTer and by the phone. Tap one for what it needs; the switch turns it on or off.")
			+ flat(sources.map((s, i) => {
				const st = !s.on ? "Off" : s.ready ? "On" : s.id === "screenscraper" && isSet(s, "user") ? "Needs the password" : "Needs a login";
				return `<div class="srow${i === sel ? " on" : ""}"><button type="button" class="t" data-act="sel" data-arg="${i}"><b>${i + 1}. ${esc(s.name)}</b><span class="${s.on && s.ready ? "good" : s.on ? "warnc" : ""}">${esc(st)}</span></button>
					<span class="ctl">${tog(s.on, "toggle").replace("data-act", `data-arg="${i}" data-act`)}<button type="button" class="b" aria-label="Move up" data-act="up" data-arg="${i}"${i ? "" : " disabled"}>↑</button><button type="button" class="b" aria-label="Move down" data-act="down" data-arg="${i}"${i < sources.length - 1 ? "" : " disabled"}>↓</button></span></div>`;
			}).join(""))
			+ (x ? lbl(x.name) + note(SOURCE_ABOUT[x.id] || "") + (SOURCE_FIELDS[x.id] ? SOURCE_FIELDS[x.id].map(([k, name, secret]) =>
				`<div class="flbl">${esc(name)}</div><input class="f" data-in="s:${k}"${secret ? ' type="password"' : ""} autocomplete="off" value="${attr(vals[k] || "")}" placeholder="${secret && isSet(x, k) ? "Saved (type to change)" : "Not set"}">`).join("")
				+ btn(busy ? "Saving…" : "Save on the MiSTer", "save", { p: true, off: busy }) + note("Saved on the MiSTer (phosphor/config/scraper.ini), sent over your home network only.") : "") : ""));
		(SOURCE_FIELDS[x && x.id] || []).forEach(([k]) => { acts["input:s:" + k] = val => { vals[k] = val; }; });
	};
	const save = async (list, s) => {
		sources = (await call("scrape_sources", { order: list.map(x => x.id), off: list.filter(x => !x.on).map(x => x.id) })).sources;
		sel = s; fill(); draw();
	};
	acts = {
		sel: i => { sel = Number(i); fill(); draw(); },
		toggle: i => save(sources.map((x, j) => j === Number(i) ? Object.assign({}, x, { on: !x.on }) : x), Number(i)),
		up: i => { i = Number(i); const l = sources.slice(); [l[i - 1], l[i]] = [l[i], l[i - 1]]; return save(l, i - 1); },
		down: i => { i = Number(i); const l = sources.slice(); [l[i + 1], l[i]] = [l[i], l[i + 1]]; return save(l, i + 1); },
		save: async () => {
			const x = sources[sel];
			busy = true; draw();
			try {
				for (const [k, , secret] of SOURCE_FIELDS[x.id] || []) {
					const val = vals[k] || "";
					if (secret && !val) continue;                     /* empty: keep what's saved */
					accounts = (await call("scrape_accounts", { set: { sec: x.id, key: k, value: val } })).accounts || accounts;
				}
				sources = (await call("scrape_sources")).sources; fill(); err = "";
				toast("Saved on the MiSTer");
			} catch (e) { err = e.message; }
			busy = false; draw();
		},
	};
	try { sources = (await call("scrape_sources")).sources; accounts = (await call("scrape_accounts")).accounts || []; fill(); } catch (e) { err = e.message; }
	draw();
} };

/* ---------------------------------------------------------------- Look (the TV's) */
PAGES.look = { async show(c) {
	setBar("Look");
	try { setLook(await call("look")); } catch (e) { /* keep the last one */ }
	const acc = ACCENTS[LOOK.accent] || ACCENTS.amber;
	let tile = "";
	try { tile = Texture.tile(LOOK.texture, rgb(acc), 1) || ""; } catch (e) { /* none */ }
	c.draw(card(`${lbl("Accent colour")}<div style="display:flex;align-items:center;gap:12px"><span class="dot on" style="background:${acc};display:inline-block"></span><b>${esc(LOOK.accent[0].toUpperCase() + LOOK.accent.slice(1))}</b></div>`)
		+ card(`${lbl("Background texture")}<div class="tex on" style="max-width:160px"><i style="${tile ? `background-image:url(${tile})${Texture.full(LOOK.texture) ? ";background-size:cover" : ""}` : ""}"></i><span>${esc(Texture.name(LOOK.texture))}</span></div>`)
		+ note("The app uses the TV's own look. Change it on the MiSTer (Settings > Look) and the app follows."));
} };

/* ---------------------------------------------------------------- Power & updates */
PAGES.power = { async show(c, p) {
	setBar("Power & updates");
	const tab = ["power", "update", "guard"].includes(p.arg) ? p.arg : "power";
	setChips([["power", "Power"], ["update", "Update"], ["guard", "Restore points"]], tab, x => go("power/" + x, true));
	if (tab === "power") {
		let pw = null, err = "";
		const draw = () => {
			let top;
			if (!pw) top = err ? "" : spin();
			else if (!pw.installed) top = flat(row("Not installed", { sub: "The health-watch module isn't on the MiSTer", dim: true }));
			else top = flat((pw.warn ? row("Power supply problem?", { sub: `Lost power ${pw.count} times in the middle of something. A weak or shared power adapter is the usual cause.`, value: "!", accent: true })
				: row(pw.count === 0 ? "No power cuts" : `Lost power ${pw.count} time${pw.count === 1 ? "" : "s"}`, { sub: `Tells you after ${pw.warn_at || 3}: heavy work cut short with no trace`, value: "OK" }))
				+ (pw.events || []).slice(0, 5).map(e => row(e.what, { sub: whenText(e.at * 1000), dim: true })).join(""));
			c.draw(errp(err) + lbl("Power check") + top + (pw && pw.warn ? btn("I've dealt with it", "ack") : "")
				+ lbl("The MiSTer")
				+ card(`<h3>Restart</h3><p class="small">The drives are marked clean first, then it starts again. A game that's running is closed.</p>${btn("Restart", "sys", { arg: "reboot", sure: "Press again to restart" })}`)
				+ card(`<h3>Shut down safely</h3><p class="small">Writes everything and marks the drives clean. The TV says when it's safe to switch off.</p>${btn("Shut down", "sys", { arg: "shutdown", sure: "Press again to shut down" })}`)
				+ card(`<h3>Reload the TV screen</h3><p class="small">Restarts Phosphor's screen on the TV, not the MiSTer: for when it looks stuck. A running game keeps running.</p>${btn("Reload", "sys", { arg: "reload_ui", sure: "Press again to reload" })}`));
		};
		acts = {
			ack: async () => { pw = await call("power", { do: "ack" }); draw(); },
			sys: async what => { await call("system", { do: what }); toast(what === "reboot" ? "Restarting the MiSTer…" : what === "shutdown" ? "Shutting down: the TV says when it's safe to switch off" : "Reloading the TV screen…"); },
		};
		try { pw = await call("power"); } catch (e) { err = e.message; }
		draw();
		return;
	}
	if (tab === "update") {
		let u = null, err = "";
		const newer = (a, b) => { const x = a.split(/[.-]/).map(Number), y = b.split(/[.-]/).map(Number); for (let i = 0; i < Math.max(x.length, y.length); i++) { const p = x[i] || 0, q = y[i] || 0; if (p !== q) return p > q; } return false; };
		const draw = () => {
			if (!u) { c.draw(err ? errp(err) : spin()); return; }
			if (!u.available) { c.draw(errp(err) + flat(row("The updater isn't on the MiSTer", { sub: "Install Phosphor 0.27.6 once with the update script, then updates work from here.", dim: true }))); return; }
			const S = u.state || {}, busy = S.state === "working", pk = u.packages || [], rb = (u.rollbacks || []).filter(r => r.version !== u.version);
			const nw = pk.filter(x => newer(x.version, u.version)), other = pk.filter(x => !nw.includes(x));
			let h = errp(err) + flat(row("Phosphor " + (u.version || "?"), { value: "installed" }));
			if (S.msg && (busy || Date.now() / 1000 - S.at < 600)) h += `<p class="${S.state === "failed" ? "err" : busy ? "busy" : "small good"}">${esc(S.msg)}</p>`;
			if (!pk.length && !rb.length) h += note("No packages on the MiSTer yet, and no earlier version kept. Send one from Phosphor for Windows, or copy phosphor-<version>-armhf.tar.gz to the SD card.");
			nw.concat(other).forEach(x => { h += card(`<h3>Install Phosphor ${esc(x.version)}</h3><p class="small">${/\/media\/usb/.test(x.file) ? "On a USB drive" : "On the MiSTer"}</p>${btn("Install " + x.version, "install", { arg: x.file, p: nw.includes(x), off: busy, sure: "Press again: the TV closes Phosphor and opens it again" })}`, nw.includes(x) ? "on" : ""); });
			rb.forEach(r => { h += card(`<h3>Roll back to Phosphor ${esc(r.version)}</h3><p class="small">${esc(r.date)}</p>${btn("Roll back", "rollback", { arg: r.version, off: busy, sure: "Press again to roll back" })}`); });
			c.draw(h + note("A package is checked, the installed version is backed up, and the MiSTer restarts by itself to finish, with your library, box art and settings as they were."));
		};
		acts = {
			install: async f => { u = await call("update", { do: "install", file: f }); toast("Updating: the TV closes Phosphor and opens it again"); draw(); },
			rollback: async ver => { u = await call("update", { do: "rollback", version: ver }); toast("Rolling back to Phosphor " + ver); draw(); },
		};
		await c.every(async () => { try { u = await call("update"); err = ""; } catch (e) { err = e.message; } draw(); return u && u.state && u.state.state === "working" ? 2000 : 8000; });
		return;
	}
	let g = null, err = "";
	const draw = () => {
		if (!g) { c.draw(err ? errp(err) : spin()); return; }
		if (!g.installed) { c.draw(flat(row("Not installed", { sub: "The update-guard module isn't on the MiSTer", dim: true }))); return; }
		const j = g.job && g.job.name ? g.job : null, busy = j && j.busy;
		const reset = (g.files || []).filter(f => f.reset);
		let h = errp(err) + flat(row("MiSTer Linux", { value: g.linux || "–" }) + (g.updated_at ? row("Last update", { value: g.updated_at, sub: g.updated_from ? "from " + g.updated_from : "" }) : ""));
		if (g.awaiting && reset.length) h += lbl("Reset by the last update") + flat(reset.map(f => row(f.path.split("/").pop(), { sub: f.path })).join(""))
			+ `<div class="btns">${btn("Put my settings back", "grestore", { p: true, off: busy })}${btn("Keep the new ones", "gkeep", { off: busy, sure: "Press again" })}</div>` + note("Network settings are put back on the MiSTer itself (Settings > Updates).");
		h += lbl("Restore points") + ((g.points || []).length ? "" : note("None yet. One is saved before every update."));
		(g.points || []).forEach(x => { h += card(`<h3>${esc(x.label || x.name)}</h3><p class="small">${esc(x.created)}${x.complete === false ? " · partial" : ""} · MiSTer Linux ${esc(x.linux || "–")}</p>${btn("Roll back to this", "groll", { arg: x.name, off: busy, sure: "Press again to roll back" })}`); });
		if (j) h += lbl(busy ? "Working…" : "Last job: " + j.name) + (j.output ? `<pre class="out">${esc(String(j.output).trim().split("\n").slice(-14).join("\n"))}</pre>` : "");
		c.draw(h + note("Rolling back puts MiSTer's system files as they were then (it may download that Linux release). Your games, saves and Phosphor settings aren't touched."));
	};
	acts = {
		grestore: async () => {
			const items = (g.files || []).filter(f => f.reset).map(f => f.path).filter(x => !/dhcpcd|hostname|\/etc\/hosts|network|wpa_/.test(x));
			if (!items.length) { toast("Only network settings were reset: put them back on the MiSTer (Settings > Updates)"); return; }
			g = await call("guard", { do: "restore", items }); toast("Putting your settings back"); draw();
		},
		gkeep: async () => { g = await call("guard", { do: "keep" }); draw(); },
		groll: async name => { g = await call("guard", { do: "rollback", items: [name] }); toast("Rolling back…"); draw(); },
	};
	await c.every(async () => { try { g = await call("guard"); err = ""; } catch (e) { err = e.message; } draw(); return g && g.job && g.job.busy ? 2000 : 8000; });
} };

/* ---------------------------------------------------------------- Storage */
const driveName = d => d.kind === "sd" ? "SD card" : (d.kind === "nvme" ? "NVMe" : "USB") + (d.model ? " · " + d.model : "");
function checksFor(d) {
	const c = [["quick", "Quick check", "Partitions, file system and the error log"], ["windows", "Will Windows complain?", "Whether the drive is marked dirty"],
		["speed", "Speed test", "Reads and writes one temporary file"], ["fscheck", "Check the file system", "Unmounts it and checks it, read only. Games on it stop while it runs."]];
	if (d.kind === "nvme") c.splice(2, 0, ["health", "Drive health", "The NVMe drive's own health report"]);
	return c;
}
PAGES.storage = { async show(c, p) {
	setBar("Storage");
	const tab = ["sd", "drives", "phone"].includes(p.arg) ? p.arg : "sd";
	setChips([["sd", "SD card"], ["drives", "NVMe / USB"], ["phone", "This phone"]], tab, x => go("storage/" + x, true));
	if (tab === "phone") {
		let s = null, busy = false;
		const draw = () => {
			if (!s) { c.draw(spin()); return; }
			const art = s.art || [], waiting = art.filter(a => !a.sent).length;
			c.draw(lbl("Downloaded box art") + flat(row(`${art.length} picture${art.length === 1 ? "" : "s"}`, { value: sizeText(s.artBytes), sub: waiting ? `${waiting} the MiSTer hasn't got yet` : art.length ? "All on the MiSTer" : "Nothing kept on the phone" })
				+ `<div class="sw"><div class="t"><b>Auto delete</b><span>Delete each picture once the MiSTer has it</span></div>${tog(s.autoDelete, "auto")}</div>`)
				+ (waiting ? btn(busy ? "Sending…" : `Send again (${waiting})`, "send", { p: true, off: busy }) : "")
				+ (art.length ? btn("Delete all", "delart", { off: busy, sure: `Press again to delete ${art.length}` }) : "")
				+ (art.length ? flat(art.slice(0, 30).map(a => row(a.title, { sub: a.system, value: a.sent ? "on the MiSTer" : "waiting", accent: !a.sent })).join("")) : "")
				+ note("Only pictures this app downloaded are here, in its own folder. Photos you pick from your gallery are never touched.")
				+ lbl("Game lists") + flat(row("libretro and LaunchBox lists", { value: sizeText(s.listsBytes), sub: "Kept so the next lookup is quick; fetched again after 30 days" }))
				+ (s.listsBytes > 0 ? btn("Delete the lists", "dellists", { off: busy, sure: "Press again (LaunchBox is 110 MB to get again)" }) : ""));
		};
		const load = async () => { s = await app("phoneStorage"); if (c.alive()) draw(); };
		acts = {
			auto: async () => { await app("setAutoDelete", { on: !s.autoDelete }); await load(); },
			send: async () => { busy = true; draw(); const r = await app("sendWaiting"); toast(r.sent + " sent"); busy = false; await load(); },
			delart: async () => { await app("deleteArt"); await load(); },
			dellists: async () => { busy = true; draw(); await app("deleteLists"); busy = false; await load(); },
		};
		await load();
		return;
	}
	let s = null, err = "";
	const draw = () => {
		if (!s) { c.draw(err ? errp(err) : spin()); return; }
		if (!s.installed) { c.draw(flat(row("The storage module isn't on the MiSTer yet", { sub: "It comes with Phosphor 0.27", dim: true }))); return; }
		const sd = tab === "sd", ds = (s.drives || []).filter(d => (d.kind === "sd") === sd), l = s.last || {};
		let h = errp(err);
		if (!ds.length) h += flat(row(sd ? "No SD card found" : "No NVMe or USB drive is plugged in", { dim: true }));
		ds.forEach(d => {
			const mounted = d.mounted !== false, used = d.size_kb ? 1 - d.free_kb / d.size_kb : 0;
			h += card(`${row(driveName(d), { value: mounted ? gb(d.free_kb) + " free" : "not mounted", sub: `${gb(d.size_kb)}  ·  ${d.fs}` + (d.kind === "nvme" ? "  ·  PCIe " + (d.gen === "gen4" ? "Gen 4" : d.gen === "gen3" ? "Gen 3" : "not known") : ""), accent: !mounted || d.gen === "gen4" })}
				${d.size_kb && mounted ? `<div style="height:6px;border-radius:3px;background:var(--panel2);overflow:hidden"><div style="height:6px;width:${Math.round(Math.min(1, Math.max(0, used)) * 100)}%;background:${used > .95 ? "var(--bad)" : "var(--acc)"}"></div></div>` : ""}
				${d.gen === "gen4" ? `<p class="err">Gen 4 NVMe drive: these cause boot and file system trouble in the SS1 dock. A Gen 3 drive is the safe choice.</p>` : ""}
				${parseFloat(d.watts) > 7 ? `<p class="vw">Rated at ${esc(d.watts)} W: more than the dock likes to give. If the MiSTer restarts under load, this drive is a likely cause.</p>` : ""}
				${!mounted ? `<p class="vw">Not mounted: MiSTer can't see the games on it right now.</p>${btn("Mount it again", "mount", { p: true, arg: d.path, off: l.busy })}`
					: lbl("Checks") + `<div>${checksFor(d).map(([t, name, what]) => row(name, { sub: what, value: l.busy ? null : "Run", accent: true }).replace('<div class="row', l.busy ? '<div class="row' : `<div data-act="check" data-arg="${attr(t + "|" + d.path)}" class="row`)).join("")}</div>`
					+ btn("Repair the file system", "check", { arg: "fsrepair|" + d.path, off: l.busy, sure: "Press again: it unmounts and repairs" })}
				${l.target === d.path && l.job ? lbl(l.busy ? "Checking: " + l.job : `Last: ${l.job} · ${whenText(l.at * 1000)}`) + (l.output ? `<pre class="out">${esc(String(l.output).trim().split("\n").slice(-14).join("\n"))}</pre>` : "") : ""}`);
		});
		if (sd && (s.overlap_tool || []).length) h += `<p class="err">Faulty overlap "fix" tool found (${esc(s.overlap_tool.map(x => x.split("/").pop()).join(", "))}): remove it from Phosphor for Windows or on the MiSTer.</p>`;
		c.draw(h + note("The checks only read, except the speed test (one temporary file) and Repair, which asks first."));
	};
	acts = {
		check: async a => { const [test, target] = a.split("|"); s = await call("storage", { do: "check", test, target }); toast("Checking…"); draw(); },
		mount: async target => { s = await call("storage", { do: "mount", target }); toast("Mounting…"); draw(); },
	};
	await c.every(async () => { try { s = await call("storage"); err = ""; } catch (e) { err = e.message; } draw(); return s && s.last && s.last.busy ? 2000 : 10000; });
} };

/* ---------------------------------------------------------------- Bluetooth, Diagnostics */
PAGES.bluetooth = { async show(c) {
	setBar("Bluetooth");
	let st = null, err = "";
	await c.every(async () => {
		try { st = await call("bluetooth"); err = ""; } catch (e) { err = e.message; }
		if (!st) { c.draw(errp(err)); return 3000; }
		if (!st.present) { c.draw(flat(row("No Bluetooth adapter", { sub: "Plug one into the MiSTer", dim: true }))); return 5000; }
		const p = st.pairing || {};
		c.draw(errp(err) + (p.busy ? `<p class="busy">${esc(p.found && /\.\.\.$/.test(p.message) ? p.message.replace(/\.\.\.$/, "") + ": " + p.found + "…" : p.message)}</p>` : "")
			+ lbl("Paired with the MiSTer") + flat((st.devices || []).length ? st.devices.map(d => row(d.name, { value: d.connected ? "Connected" : "Not connected", dim: !d.connected })).join("") : row("Nothing paired yet", { dim: true }))
			+ note("Pairing, disconnecting and forgetting are done on the TV (Settings › Bluetooth), so nothing pairs with the MiSTer from somewhere else. Pairings stay on the MiSTer's SD card."));
		return p.busy ? 800 : 3000;
	});
} };

const DIAG_ORDER = ["fail", "warn", "info", "ok"];
PAGES.diag = { async show(c) {
	setBar("Diagnostics");
	let st = null, err = "";
	const draw = () => {
		if (!st) { c.draw(err ? errp(err) : spin()); return; }
		if (!st.installed) { c.draw(flat(row("Not installed", { sub: "The diag module comes with Phosphor", dim: true }))); return; }
		const busy = !!st.busy, sm = st.summary || {};
		const items = (st.results || []).slice().sort((a, b) => (DIAG_ORDER.indexOf(a.level) + 10) % 14 - (DIAG_ORDER.indexOf(b.level) + 10) % 14);
		const areas = {};
		items.forEach(d => { (areas[d.area || "System"] = areas[d.area || "System"] || []).push(d); });
		c.draw(errp(err) + flat(row(sm.text || (st.at ? "Checked" : "Not checked yet"), { sub: busy ? "Checking…" : "Last checked " + (st.at ? whenText(st.at * 1000) : "never"), accent: sm.level === "fail" || sm.level === "warn", value: sm.level || null }))
			+ btn(busy ? "Checking…" : "Check now", "run", { p: true, off: busy }) + (st.error ? errp(st.error) : "")
			+ Object.keys(areas).map(a => lbl(a) + flat(areas[a].map(d => row(d.message, { value: d.level, accent: d.level === "fail" || d.level === "warn", dim: d.level === "ok" })).join(""))).join("")
			+ note("Support bundles are made on the MiSTer itself (Settings › Diagnostics), not from the phone."));
	};
	acts = { run: async () => { st = await call("diag", { run: true }); draw(); } };
	await c.every(async () => { try { st = await call("diag"); err = ""; } catch (e) { err = e.message; } draw(); return st && st.busy ? 1500 : 10000; });
} };

/* ---------------------------------------------------------------- Notifications, Connection, About */
PAGES.notes = { async show(c) {
	setBar("Notifications");
	let n = null;
	const draw = () => {
		if (!n) { c.draw(spin()); return; }
		c.draw(card(`<div class="sw"><div class="t"><b>Tell me on the phone</b><span>Checked every minute with the app open, about every 15 minutes in the background (on WiFi)</span></div>${tog(n.notify, "notify")}</div>`)
			+ (n.notify && !n.allowed ? `<p class="vw">Android's notifications are off for Phosphor.</p>${btn("Allow notifications", "allow", { p: true })}` : "")
			+ (!(n.list || []).length ? flat(row("Nothing to tell", { sub: "Power, storage and update warnings, and box art finishing, show here", dim: true }))
				: flat(n.list.map(x => row(x.title, { sub: x.body + "\n" + whenText(x.at), accent: x.level === "warn" && !x.seen, go: x.go ? goRoute(x.go) : null })).join("")) + btn("Clear", "clear")));
	};
	const load = async () => { n = await app("notes"); if (c.alive()) draw(); };
	acts = {
		notify: async () => { await app("setNotify", { on: !n.notify }); await load(); },
		allow: async () => { await app("allowNotify"); await load(); },
		clear: async () => { await app("clearNotes"); await load(); },
	};
	await load();
	setTimeout(() => { if (c.alive()) app("markSeen").catch(() => {}); }, 1500);
} };
/* "power/2" (the Compose app's tab numbers, from notifications) -> "power/guard" */
function goRoute(g) {
	const [s, t] = String(g).split("/");
	const tabs = { saves: ["cards", "backups", "cloud"], boxart: ["lookup", "sources"], power: ["power", "update", "guard"], storage: ["sd", "drives", "phone"] };
	return t != null && tabs[s] && /^\d+$/.test(t) ? s + "/" + tabs[s][Number(t)] : g;
}

PAGES.connection = { async show(c) {
	setBar("Connection");
	let searching = false, msg = "";
	const draw = () => c.draw(flat(row("Paired with", { value: ST.address }) + row(ST.reachable === true ? "Connected" : ST.reachable === false ? "Not answering" : "Checking…", { value: ST.version ? "Phosphor " + ST.version : null, accent: ST.reachable === false }))
		+ (ST.reachable === false ? note("Is it on, on this WiFi, with Remote access turned on? If the router gave it a new address, look for it:") : "")
		+ btn(searching ? "Looking on this WiFi…" : "Find it on this WiFi", "find", { off: searching }) + (msg ? `<p class="busy">${esc(msg)}</p>` : "")
		+ lbl("Forget") + btn("Forget this MiSTer", "forget", { d: true, sure: "Press again: you'll pair again" })
		+ note("To remove this phone on the MiSTer too: Settings › Remote access, select the phone, press X."));
	acts = {
		find: async () => { searching = true; msg = ""; draw(); try { msg = (await app("find")).message; } catch (e) { msg = e.message; } searching = false; draw(); },
		forget: async () => { await app("forget"); },
	};
	on("state", () => { if (c.alive() && R.name === "connection") draw(); });
	draw();
} };

PAGES.about = { async show(c) {
	setBar("About");
	c.draw(flat(row("Phosphor for Android", { value: ST.appVersion }) + row("Works with", { value: "Phosphor 0.27.25 or later" }))
		+ note("The app only talks to your MiSTer, on your home network. Box art lookups on the phone go to the services you turned on (libretro, ArcadeDB, LaunchBox, and with your logins TheGamesDB, ScreenScraper, IGDB). It collects nothing and sends nothing anywhere else. Passwords, WiFi and Bluetooth pairings stay on the MiSTer.")
		+ note("PolyForm Noncommercial 1.0.0, like the rest of Phosphor. Fonts: Inter (SIL Open Font License 1.1), DejaVu Sans Mono (Bitstream Vera licence)."));
} };

/* ---------------------------------------------------------------- start */
(async () => {
	try {
		const s = await app("state");
		Object.assign(ST, s);
		if (s.look) { LOOK.accent = ACCENTS[s.look.accent] ? s.look.accent : "amber"; LOOK.texture = s.look.texture | 0; }
	} catch (e) { /* not in the app: shows Connect */ }
	applyLook();
	showConn();
	if (ST.go) location.hash = goRoute(ST.go);
	route();
})();
on("go", g => go(goRoute(g), true));
