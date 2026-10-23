/* Phosphor for Windows: the screens. Laid out like the console: a status bar, a screen with a big
   title, lists with an info panel beside them, tabs for a page's parts, button hints at the bottom.
   Mouse, keyboard (arrows, Enter, Esc, Q/E for tabs) or a controller (A B X Y, LB/RB, Start). */
"use strict";

/* ---------- talking to the app (and through it, the SS1) ---------- */
const HASH = location.hash.slice(1);      /* the tray asked for a screen: #notifications */
const KEY = (() => {
	const k = new URLSearchParams(location.search).get("k");
	try { if (k) sessionStorage.setItem("k", k); } catch (e) {}
	let s = k;
	try { s = k || sessionStorage.getItem("k"); } catch (e) {}
	history.replaceState(null, "", "/");
	return s || "";
})();

async function api(path, body) {
	const r = await fetch(path, {
		method: body === undefined ? "GET" : "POST",
		headers: { "X-Phosphor-Key": KEY, "Content-Type": "application/json" },
		body: body === undefined ? undefined : JSON.stringify(body),
	});
	if (!r.ok) throw new Error("The app answered " + r.status);
	return r.json();
}
/* a phosphord command on the SS1 */
async function ss1(cmd) {
	const j = await api("/api/call", cmd);
	if (j.forgotten) { S.paired = false; replace(Connect); }
	if (!j.ok) throw new Error(j.error || "the SS1 didn't answer");
	return j;
}
const artURL = id => `/api/art/${id}?k=${encodeURIComponent(KEY)}`;

/* ---------- small helpers ---------- */
const $ = (sel, root = document) => root.querySelector(sel);
const esc = s => String(s ?? "").replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const n = x => Number(x || 0).toLocaleString();
function h(html) { const t = document.createElement("template"); t.innerHTML = html.trim(); return t.content; }
let toastTimer;
function toast(msg) {
	const t = $("#toast");
	t.textContent = msg; t.hidden = false;
	clearTimeout(toastTimer); toastTimer = setTimeout(() => (t.hidden = true), 3500);
}
function spaced(s) { return esc(s); }          /* the spacing is the .sub style's */

/* ---------- the look: accent and texture, like the console ---------- */
const ACCENTS = { amber: "#ffb347", red: "#ff6b5e", green: "#5cf08a", blue: "#6cb6ff", purple: "#c792ff" };
const S = { accent: "amber", texture: 0, paired: false, connected: false, name: "", address: "" };
function rgb(hex) { const v = parseInt(hex.slice(1), 16); return [v >> 16, (v >> 8) & 255, v & 255]; }
function applyTheme() {
	const acc = ACCENTS[S.accent] || ACCENTS.amber, [r, g, b] = rgb(acc);
	const mix = (c, d) => Math.round(c * 0.2 + d * 0.8);          /* the shell's panel_on: 20% accent */
	document.documentElement.style.setProperty("--acc", acc);
	document.documentElement.style.setProperty("--on", `rgb(${mix(r, 0x14)},${mix(g, 0x14)},${mix(b, 0x16)})`);
	const scale = parseFloat(getComputedStyle(document.documentElement).fontSize) / 16;
	const url = Texture.tile(S.texture, [r, g, b], scale);
	document.body.style.backgroundImage = url ? `url(${url})` : "none";
}
let themeT;
addEventListener("resize", () => { clearTimeout(themeT); themeT = setTimeout(applyTheme, 200); });

/* ---------- the status bar ---------- */
function clock() {
	$("#clock").textContent = new Date().toLocaleTimeString([], { hour: "numeric", minute: "2-digit" });
}
function showConn() {
	const p = $("#conn");
	p.className = "pill" + (S.connected ? " ok" : S.paired ? " bad" : "");
	p.querySelector("span").textContent = S.connected ? (S.name && S.name !== "Phosphor" ? S.name : "SS1") + " connected"
		: S.paired ? "Reconnecting…" : "Not connected";
}
async function refreshState() {
	try {
		const st = await api("/api/state");
		Object.assign(S, { paired: st.paired, connected: !!st.connected, name: st.name, address: st.address, accent: st.accent, texture: st.texture, appVersion: st.app_version });
		S.status = st.status || null;
		if (st.forgotten) S.paired = false;
	} catch (e) { S.connected = false; }
	showConn();
}

/* ---------- focus and navigation ---------- */
let cur = null;
function navs() { return [...document.querySelectorAll("[data-nav]")].filter(e => e.offsetParent !== null && !e.closest("[hidden]")); }
function focus(el, quiet) {
	if (!el) return;
	if (cur) cur.classList.remove("f");
	cur = el;
	el.classList.add("f");
	if (el.matches("input, textarea")) el.focus({ preventScroll: true });
	else if (document.activeElement && document.activeElement.matches("input, textarea")) document.activeElement.blur();
	el.scrollIntoView({ block: "nearest", inline: "nearest" });
	if (!quiet && el._onfocus) el._onfocus();
	const s = topScr();
	if (s && s.focused) s.focused(el);
}
function move(dx, dy) {
	const all = navs();
	if (!cur || !all.includes(cur)) { focus(all.find(e => !e.closest("#top")) || all[0]); return; }
	const a = cur.getBoundingClientRect(), ax = a.left + a.width / 2, ay = a.top + a.height / 2;
	let best = null, bestScore = Infinity;
	for (const e of all) {
		if (e === cur) continue;
		const b = e.getBoundingClientRect(), bx = b.left + b.width / 2, by = b.top + b.height / 2;
		const ex = bx - ax, ey = by - ay;
		const along = dx ? ex * dx : ey * dy, across = dx ? Math.abs(ey) : Math.abs(ex);
		if (along <= 2) continue;
		/* same row/column first; then the nearest */
		const overlap = dx ? (b.bottom > a.top + 2 && b.top < a.bottom - 2) : (b.right > a.left + 2 && b.left < a.right - 2);
		const score = along + across * (overlap ? 0.2 : 3);
		if (score < bestScore) { bestScore = score; best = e; }
	}
	if (best) focus(best);
}
function activate() { if (cur && !cur.matches("input, textarea")) cur.click(); else if (cur && cur._enter) cur._enter(); }

/* ---------- screens: a stack ---------- */
const stack = [];
let touched = false;        /* the user has moved since this screen opened: late loads leave the focus alone */
function show(scr) {
	cur = null; touched = false;
	const root = $("#screen");
	root.innerHTML = "";
	scr.render(root);
	setHints(scr.hints || []);
	if (!cur) move(0, 1);
}
function push(Scr, ...args) { stack.push(new Scr(...args)); show(stack[stack.length - 1]); }
function replace(Scr, ...args) { stack.length = 0; push(Scr, ...args); }
function back() {
	if (stack.length > 1) { stack.pop(); const s = stack[stack.length - 1]; show(s); if (s.restore) s.restore(); }
}
function topScr() { return stack[stack.length - 1]; }
function setHints(list) {
	$("#hints").innerHTML = list.map(([k, t]) => `<span class="hint"><kbd>${esc(k)}</kbd>${esc(t)}</span>`).join("");
}
/* tabs: buttons with data-tab inside .tabs; LB/RB or Q/E switch */
function tabs(root, names, cur0, onSwitch) {
	const bar = h(`<div class="tabs">${names.map((t, i) => `<button class="tab${i === cur0 ? " on" : ""}" data-nav data-tab="${i}">${esc(t)}</button>`).join("")}</div>`).firstChild;
	root.appendChild(bar);
	bar.querySelectorAll(".tab").forEach(b => b.addEventListener("click", () => onSwitch(+b.dataset.tab)));
	return bar;
}

/* ---------- input: keys, mouse, controller ---------- */
addEventListener("keydown", e => {
	touched = true;
	const typing = document.activeElement && document.activeElement.matches("input, textarea");
	const k = e.key;
	if (typing && !["ArrowUp", "ArrowDown", "Escape", "Enter", "Tab"].includes(k)) return;
	if (typing && k === "Enter" && document.activeElement.matches("textarea")) return;
	const s = topScr();
	switch (k) {
	case "ArrowLeft": move(-1, 0); break;
	case "ArrowRight": move(1, 0); break;
	case "ArrowUp": move(0, -1); break;
	case "ArrowDown": move(0, 1); break;
	case "Enter": case " ": activate(); break;
	case "Escape": case "Backspace": if (typing && k === "Backspace") return; if (!(s && s.back && s.back())) back(); break;
	case "q": case "PageUp": s && s.tab && s.tab(-1); break;
	case "e": case "PageDown": s && s.tab && s.tab(1); break;
	case "x": case "X": s && s.x && s.x(); break;
	case "y": case "Y": case "/": s && s.y ? s.y() : push(Search); break;
	case "F1": case "m": push(AppSettings); break;
	default: return;
	}
	e.preventDefault();
});
/* the mouse: clicks only, the highlight never follows the pointer. A list row with a panel beside it
   is chosen by the first click (the panel follows) and opened by a second click or a double-click;
   everything else (buttons, tiles, tabs) does its thing on the first click. */
let picked = null;
document.addEventListener("mousedown", e => {
	touched = true;
	const el = e.target.closest("[data-nav]");
	if (!el) return;
	picked = el.matches(".row") && el._onfocus && el !== cur ? el : null;   /* rows that drive a panel */
	focus(el);
});
document.addEventListener("click", e => {
	const el = e.target.closest("[data-nav]");
	if (el && el === picked) { picked = null; e.stopPropagation(); e.preventDefault(); }
}, true);
document.addEventListener("dblclick", e => {
	const el = e.target.closest(".row[data-nav]");
	if (el && el === cur) el.click();
});
$("#conn").addEventListener("click", () => (S.paired ? push(AppSettings, "connection") : push(Connect)));
$("#bell").addEventListener("click", () => push(AppSettings, "notifications"));

/* the controller (the browser's gamepad support): read only while one is plugged in */
const pad = { held: {}, next: {}, raf: 0 };
function padLoop() {
	pad.raf = 0;
	const gp = [...(navigator.getGamepads ? navigator.getGamepads() : [])].find(g => g && g.connected);
	if (!gp) return;
	if (document.hasFocus()) {
		const b = i => gp.buttons[i] && gp.buttons[i].pressed;
		const ax = gp.axes[0] || 0, ay = gp.axes[1] || 0;
		const st = {
			up: b(12) || ay < -0.6, down: b(13) || ay > 0.6, left: b(14) || ax < -0.6, right: b(15) || ax > 0.6,
			a: b(0), b: b(1), x: b(2), y: b(3), lb: b(4), rb: b(5), start: b(9),
		};
		const now = performance.now(), s = topScr();
		const act = {
			up: () => move(0, -1), down: () => move(0, 1), left: () => move(-1, 0), right: () => move(1, 0),
			a: activate, b: () => { if (!(s && s.back && s.back())) back(); }, x: () => s && s.x && s.x(), y: () => (s && s.y ? s.y() : push(Search)),
			lb: () => s && s.tab && s.tab(-1), rb: () => s && s.tab && s.tab(1), start: () => push(AppSettings),
		};
		for (const k in st) {
			if (st[k] && !pad.held[k]) { touched = true; act[k](); pad.next[k] = now + 400; }
			else if (st[k] && ["up", "down", "left", "right"].includes(k) && now >= pad.next[k]) { act[k](); pad.next[k] = now + 90; }
			pad.held[k] = st[k];
		}
	}
	pad.raf = requestAnimationFrame(padLoop);
}
addEventListener("gamepadconnected", () => { if (!pad.raf) pad.raf = requestAnimationFrame(padLoop); });

/* ---------- category icons (the console's) ---------- */
const ICON = {
	Disc: `<svg viewBox="0 0 48 48" fill="none" stroke="currentColor" stroke-width="3"><circle cx="24" cy="24" r="19"/><circle cx="24" cy="24" r="4"/></svg>`,
	Cartridge: `<svg viewBox="0 0 48 48" fill="none" stroke="currentColor" stroke-width="3"><rect x="9" y="5" width="30" height="38" rx="3"/><rect x="15" y="11" width="18" height="12"/><path d="M15 36h18"/></svg>`,
	Handheld: `<svg viewBox="0 0 48 48" fill="none" stroke="currentColor" stroke-width="3"><rect x="12" y="3" width="24" height="42" rx="4"/><rect x="17" y="8" width="14" height="14"/><circle cx="19" cy="33" r="2"/><circle cx="29" cy="31" r="2"/></svg>`,
	Arcade: `<svg viewBox="0 0 48 48" fill="none" stroke="currentColor" stroke-width="3"><path d="M14 45V12l4-8h12l4 8v33z"/><rect x="18" y="13" width="12" height="10"/><circle cx="20" cy="31" r="2"/><path d="M29 28v6"/></svg>`,
	Computer: `<svg viewBox="0 0 48 48" fill="none" stroke="currentColor" stroke-width="3"><rect x="5" y="7" width="38" height="26" rx="2"/><path d="M17 41h14M24 33v8"/></svg>`,
};

/* ---------- Connect: find the SS1, pair with the code on the TV ---------- */
class Connect {
	constructor() { this.hints = [["A", "Choose"], ["X", "Search again"], ["B", "Back"]]; this.addr = S.address || ""; this.found = []; }
	render(root) {
		root.append(h(`<div class="sub">${spaced("Connect")}</div><h1>Connect to your SS1</h1>
			<p class="lead">On the SS1, open <b>Settings &gt; Remote access</b>, turn it on and choose <b>Pair a device</b>. The TV shows a 6-digit code for 2 minutes.</p>
			<div class="split"><div class="col"><div class="sub">${spaced("Consoles on this network")}</div><div class="list" id="found"></div></div>
			<div class="panel"><h2>Pair this PC</h2>
				<div class="field"><label for="addr">The SS1's address (pick it on the left, or type it)</label><input id="addr" type="text" data-nav placeholder="192.168.1.23" value="${esc(this.addr)}"></div>
				<div class="field"><label for="code">Code on the TV</label><input id="code" class="code" type="text" inputmode="numeric" maxlength="6" data-nav placeholder="000000"></div>
				<div class="acts"><button class="btn primary" id="pair" data-nav>Pair</button></div>
				<p class="note" id="pmsg">Pairing keys are kept on this PC only, protected for your Windows account.</p></div></div>`));
		$("#pair").onclick = () => this.pair();
		$("#code")._enter = () => this.pair();
		$("#code").addEventListener("input", e => { e.target.value = e.target.value.replace(/\D/g, "").slice(0, 6); if (e.target.value.length === 6) focus($("#pair")); });
		$("#addr").addEventListener("input", () => this.mark());
		this.look();
	}
	x() { this.look(); }
	/* like a Wi-Fi list: every console that answers, pick one, then type the code */
	draw(searching) {
		const box = $("#found");
		box.innerHTML = "";
		for (const f of this.found) {
			const ip = f.address.replace(/:8767$/, "");
			const r = h(`<div class="row net" data-nav><span class="n"><b>${esc(f.name || "SuperStation One")}</b><small>${esc(ip)} · Phosphor ${esc(f.version)}</small></span><span class="v tick"></span></div>`).firstChild;
			r._ip = ip;
			r.onclick = () => { $("#addr").value = ip; this.mark(); focus($("#code")); };
			box.appendChild(r);
		}
		if (searching) box.appendChild(h(`<div class="empty"><span class="spin"></span>Searching…</div>`).firstChild);
		else {
			if (!this.found.length) box.appendChild(h(`<div class="empty">No SS1 found. Check Remote access is on, then search again, or type its address.</div>`).firstChild);
			const r = h(`<div class="row" data-nav id="again"><span class="n">Search again</span><span class="v">X</span></div>`).firstChild;
			r.onclick = () => this.look();
			box.appendChild(r);
		}
		this.mark();
	}
	mark() {
		const a = $("#addr").value.trim();
		for (const r of document.querySelectorAll("#found .net")) { const on = r._ip === a; r.classList.toggle("on", on); r.querySelector(".tick").textContent = on ? "✓ Selected" : ""; }
	}
	async look() {
		if (this.busy) return;
		this.busy = true; this.found = []; this.draw(true);
		try {
			const j = await api("/api/discover");
			this.found = j.found;
			if (this.found.length === 1 && !$("#addr").value) $("#addr").value = this.found[0].address.replace(/:8767$/, "");
		} catch (e) { toast(e.message); }
		this.busy = false;
		this.draw(false);
		const sel = document.querySelector("#found .net.on");
		focus(sel ? $("#code") : $("#found").firstElementChild, true);
	}
	async pair() {
		const addr = $("#addr").value.trim(), code = $("#code").value.trim(), msg = $("#pmsg");
		if (!addr) { msg.textContent = "Pick your SS1 on the left, or type its address."; focus($("#found").firstElementChild); return; }
		if (code.length !== 6) { msg.textContent = "Type the 6-digit code from the TV."; focus($("#code")); return; }
		msg.innerHTML = `<span class="spin"></span>Pairing…`;
		const j = await api("/api/pair", { address: addr, code });
		if (!j.ok) { msg.textContent = j.error; return; }
		toast("Paired: this PC can now manage your SS1's library");
		await refreshState();
		replace(Home);
	}
}

/* ---------- Home: the shelf, the tools, pick up where you left off ---------- */
class Home {
	constructor() { this.hints = [["A", "Open"], ["Y", "Search"], ["☰", "Settings"]]; this.sel = 0; }
	render(root) {
		root.append(h(`<div class="sub">${spaced("Library")}</div><div class="shelf" id="shelf"></div>
			<div class="sub">${spaced("Tools")}</div><div class="tools" id="tools"></div><div id="resume"></div>`));
		this.shelf(); this.tools(); this.resume();
	}
	restore() { this.tools(); }
	async shelf() {
		const box = $("#shelf");
		try {
			const j = await ss1({ cmd: "categories" });
			box.innerHTML = "";
			j.categories.forEach((c, i) => {
				const el = h(`<div class="card" data-nav>${`<span class="ic">${ICON[c.name] || ICON.Cartridge}</span>`}<b>${esc(c.name)}</b><small>${n(c.games)} game${c.games === 1 ? "" : "s"}</small></div>`).firstChild;
				el.onclick = () => { this.sel = i; push(Systems, c); };
				el._onfocus = () => (this.sel = i);
				box.appendChild(el);
			});
			if (!touched || !cur || !document.body.contains(cur)) focus(box.children[Math.min(this.sel, box.children.length - 1)], true);
		} catch (e) { box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
	async tools() {
		const box = $("#tools");
		if (!box) return;
		const t = [
			{ id: "match", b: "Match to base games", s: "…" },
			{ id: "search", b: "Search", s: "every system" },
			{ id: "art", b: "Box art", s: "sources and logins" },
			{ id: "settings", b: "Settings", s: "power, storage, look…" },
		];
		try {                                   /* warnings first, when there are new ones */
			const p = await api("/api/ping");
			if (p.unseen) t.unshift({ id: "notes", b: p.warn ? `⚠ ${p.warn} warning${p.warn === 1 ? "" : "s"}` : `${p.unseen} new`, s: "notifications", warn: p.warn > 0 });
		} catch (e) {}
		try {                                   /* the SS1's files, while file sharing is on */
			const sm = await ss1({ cmd: "samba" });
			if (sm.running) t.splice(t.length - 1, 0, { id: "files", b: "SS1 files", s: "open in File Explorer" });
		} catch (e) {}
		if (!$("#tools")) return;
		box.innerHTML = t.map(x => `<div class="tool${x.warn ? " alert" : ""}" data-nav data-id="${x.id}"><b>${esc(x.b)}</b><small id="t-${x.id}">${esc(x.s)}</small></div>`).join("");
		box.querySelectorAll(".tool").forEach(el => (el.onclick = () => {
			const id = el.dataset.id;
			if (id === "match") push(Match);
			else if (id === "search") push(Search);
			else if (id === "settings") push(AppSettings);
			else if (id === "art") push(AppSettings, "boxart");
			else if (id === "notes") push(AppSettings, "notifications");
			else if (id === "files") api("/api/explore", {}).then(j => { if (!j.ok) toast(j.error); });
		}));
		try {
			const j = await ss1({ cmd: "games", filter: "not_found", limit: 1 });
			const el = $("#t-match");
			if (el) { el.textContent = j.total ? `${n(j.total)} not found to review` : "nothing to review"; el.className = j.total ? "warn" : ""; }
		} catch (e) {}
	}
	async resume() {
		try {
			const j = await ss1({ cmd: "last" });
			const g = j.game, box = $("#resume");
			if (!g || !box) return;
			box.innerHTML = "";
			box.append(h(`<div class="resume"><div class="art" style="background-image:url('${artURL(g.id)}')"></div>
				<div class="t"><div class="cap">${spaced("Pick up where you left off")}</div><b>${esc(g.title)}</b><small>${esc(g.system_name || g.system)}${g.play_count ? ` · played ${g.play_count} time${g.play_count === 1 ? "" : "s"}` : ""}</small></div>
				<button class="btn primary" data-nav id="play">Play on TV</button></div>`));
			$("#play").onclick = () => launch(g);
		} catch (e) {}
	}
}

async function launch(g) {
	try { await ss1({ cmd: "launch", id: g.id }); toast(`Starting ${g.title} on the TV`); } catch (e) { toast(e.message); }
}

/* ---------- Systems in a category ---------- */
class Systems {
	constructor(cat) { this.cat = cat; this.hints = [["A", "Open"], ["Y", "Search"], ["B", "Back"]]; this.sel = 0; }
	render(root) {
		root.append(h(`<div class="sub">${spaced("Library")}</div><h1>${esc(this.cat.name)}</h1>
			<div class="split"><div class="list" id="sys"></div><div class="panel" id="info"></div></div>`));
		this.load();
	}
	async load() {
		const box = $("#sys");
		try {
			const j = await ss1({ cmd: "systems", category: this.cat.id });
			box.innerHTML = "";
			if (!j.systems.length) box.innerHTML = `<div class="empty">No games here yet.</div>`;
			j.systems.forEach((s, i) => {
				const el = h(`<div class="row" data-nav><span class="n">${esc(s.name)}</span><span class="v">${n(s.games)}</span></div>`).firstChild;
				el.onclick = () => { this.sel = i; push(Games, { system: s.id, title: s.name }); };
				el._onfocus = () => { this.sel = i; $("#info").innerHTML = `<h2>${esc(s.name)}</h2><dl class="kv"><dt>Games</dt><dd>${n(s.games)}</dd><dt>Core</dt><dd>${esc(s.core || "–")}</dd></dl>`; };
				box.appendChild(el);
			});
			focus(box.children[Math.min(this.sel, box.children.length - 1)]);
		} catch (e) { box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
}

/* ---------- a list of games, with the panel following the highlighted one ---------- */
const DETAIL_KEYS = [["year", "Year"], ["developer", "Developer"], ["publisher", "Publisher"], ["genre", "Genre"], ["players", "Players"], ["region", "Region"]];
const gameCache = new Map();
async function getGame(id, fresh) {
	if (!fresh && gameCache.has(id)) return gameCache.get(id);
	const j = await ss1({ cmd: "game", id });
	gameCache.set(id, j.game || j);
	return gameCache.get(id);
}
function gamePanel(g) {
	const kv = DETAIL_KEYS.filter(([k]) => g[k]).map(([k, l]) => `<dt>${l}</dt><dd>${esc(g[k])}</dd>`).join("");
	return `<div class="boxart" style="background-image:url('${artURL(g.id)}')"></div>
		<h2>${esc(g.title)}</h2>
		${kv ? `<dl class="kv">${kv}</dl>` : `<p class="note">No details yet.</p>`}
		${g.based_on ? `<p class="note">Based on <b>${esc(g.based_on.title)}</b>: uses its box art and details.</p>` : ""}
		${g.description ? `<p>${esc(g.description)}</p>` : ""}`;
}
class Games {
	/* opts: {system, title} or {filter, title, sub} or {q} */
	constructor(opts) {
		this.o = opts; this.items = []; this.total = 0; this.sel = 0;
		this.hints = [["A", "Open"], ["X", "Favorite"], ["Y", "Search"], ["B", "Back"]];
	}
	render(root) {
		root.append(h(`<div class="sub">${spaced(this.o.sub || "Library")}</div><h1>${esc(this.o.title)}</h1>
			<div class="split"><div class="list" id="games"></div><div class="panel" id="info"></div></div>`));
		this.box = $("#games");
		this.items = []; this.load();
	}
	async load() {
		const req = { cmd: "games", offset: this.items.length, limit: 200 };
		if (this.o.system) req.system = this.o.system;
		if (this.o.filter) req.filter = this.o.filter;
		if (this.o.q) req.q = this.o.q;
		try {
			const j = await ss1(req);
			this.total = j.total;
			const start = this.items.length;
			this.items.push(...j.games);
			if (start === 0) this.box.innerHTML = this.items.length ? "" : `<div class="empty">No games.</div>`;
			$(".more", this.box)?.remove();
			j.games.forEach((g, k) => this.row(g, start + k));
			if (this.items.length < this.total) {
				const m = h(`<div class="more">${n(this.total - this.items.length)} more…</div>`).firstChild;
				this.box.appendChild(m);
			}
			if (start === 0 && this.items.length) focus(this.box.children[Math.min(this.sel, this.items.length - 1)]);
		} catch (e) { this.box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
	row(g, i) {
		const sys = this.o.system ? "" : esc(g.system || "");
		const el = h(`<div class="row" data-nav><span class="n">${g.favorite ? '<span class="star">★</span> ' : ""}${esc(g.title)}</span><span class="v">${g.linked ? "based on another · " : ""}${sys}</span></div>`).firstChild;
		el.onclick = () => { this.sel = i; push(Game, g.id); };
		el._onfocus = () => {
			this.sel = i;
			if (i > this.items.length - 20 && this.items.length < this.total && !this.loading) { this.loading = true; this.load().finally(() => (this.loading = false)); }
			clearTimeout(this.t);
			this.t = setTimeout(async () => {
				try { const gg = await getGame(g.id); if (this.sel === i) $("#info").innerHTML = gamePanel(gg); } catch (e) {}
			}, 120);
		};
		this.box.appendChild(el);
	}
	async x() {
		const g = this.items[this.sel];
		if (!g) return;
		try {
			await ss1({ cmd: "favorite", id: g.id, on: !g.favorite });
			g.favorite = !g.favorite;
			cur.querySelector(".n").innerHTML = (g.favorite ? '<span class="star">★</span> ' : "") + esc(g.title);
			toast(g.favorite ? "Added to favorites" : "Removed from favorites");
		} catch (e) { toast(e.message); }
	}
	restore() { gameCache.clear(); }
}

/* ---------- a game: details, Based on, files ---------- */
class Game {
	constructor(id, tab = 0) { this.id = id; this.t = tab; this.hints = [["A", "Choose"], ["LB/RB", "Tabs"], ["B", "Back"]]; }
	render(root) {
		root.append(h(`<div class="sub">${spaced("Game")}</div><h1 id="gt">…</h1><div id="tb" style="flex:none"></div><div id="body"></div>`));
		this.load();
	}
	tab(d) { this.t = (this.t + d + 3) % 3; this.draw(); }
	async load() {
		try { this.g = await getGame(this.id, true); this.draw(); } catch (e) { $("#body").innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
	draw() {
		const g = this.g;
		$("#gt").textContent = g.title;
		$("#tb").innerHTML = "";
		tabs($("#tb"), ["Details", "Based on", "Files"], this.t, i => { this.t = i; this.draw(); });
		const body = $("#body");
		body.innerHTML = "";
		cur = null;
		if (this.t === 0) this.details(body); else if (this.t === 1) this.basedOn(body); else this.files(body);
		focus($(".tab.on"), true);
	}
	details(body) {
		const g = this.g;
		const f = (k, l) => `<div class="field"><label for="f-${k}">${l}</label><input id="f-${k}" type="text" data-nav value="${esc(g[k])}"></div>`;
		/* the fields scroll; the buttons stay in view under them */
		body.append(h(`<div class="split"><div class="col"><div class="list grow">
			${f("title", "Title")}
			<div class="field"><label for="f-description">Description</label><textarea id="f-description" data-nav>${esc(g.description)}</textarea></div>
			<div style="display:grid;grid-template-columns:1fr 1fr;gap:0 1rem">${DETAIL_KEYS.map(([k, l]) => f(k, l)).join("")}</div></div>
			<div class="acts" style="flex:none;margin-top:.8rem"><button class="btn primary" id="save" data-nav>Save</button><button class="btn" id="play" data-nav>Play on TV</button>
			<button class="btn" id="again" data-nav>Look it up again</button><button class="btn" id="setart" data-nav>Choose box art…</button></div>
			<input type="file" id="artfile" accept="image/*" hidden></div>
			<div class="panel">${gamePanel(g)}<p class="note">${esc(g.system_name || g.system)} · ${g.play_count ? `played ${g.play_count} time${g.play_count === 1 ? "" : "s"}` : "not played yet"}</p></div></div>`));
		$("#save").onclick = () => this.save();
		$("#play").onclick = () => launch(g);
		$("#again").onclick = async () => { try { await ss1({ cmd: "rescrape", id: g.id }); toast("It'll be looked up again the next time box art runs"); } catch (e) { toast(e.message); } };
		$("#setart").onclick = () => $("#artfile").click();
		$("#artfile").onchange = e => this.putArt(e.target.files[0]);
	}
	async save() {
		const set = {};
		for (const k of ["title", "description", ...DETAIL_KEYS.map(d => d[0])]) {
			const v = $("#f-" + k).value.trim();
			if (v !== (this.g[k] || "")) set[k] = v;
		}
		if (set.title === "") { toast("The title can't be empty"); return; }
		if (!Object.keys(set).length) { toast("Nothing changed"); return; }
		try { const j = await ss1({ cmd: "edit", id: this.id, set }); this.g = j.game || await getGame(this.id, true); gameCache.set(this.id, this.g); toast("Saved on the SS1"); this.draw(); }
		catch (e) { toast(e.message); }
	}
	async putArt(file) {
		if (!file) return;
		toast("Sending the box art…");
		try {
			const r = await fetch(`/api/art/${this.id}`, { method: "PUT", headers: { "X-Phosphor-Key": KEY }, body: file });
			const j = await r.json();
			if (!j.ok) throw new Error(j.error || "the SS1 didn't take it");
			this.g = await getGame(this.id, true); toast("Box art saved on the SS1"); this.draw();
		} catch (e) { toast(e.message); }
	}
	basedOn(body) {
		const g = this.g;
		body.append(h(`<div class="split"><div class="list"><div class="field"><label for="cq">Search for the base game</label><input id="cq" type="search" data-nav placeholder="Type part of the name"></div><div id="cands"></div></div>
			<div class="panel"><h2>${g.based_on ? "Based on " + esc(g.based_on.title) : "Not based on another game"}</h2>
			<p class="note">A hack, translation or patched copy can use its base game's box art and details. The SS1 shows the link too.</p>
			<div class="acts">${g.based_on ? `<button class="btn" id="unlink" data-nav>Unlink</button>` : ""}</div></div></div>`));
		if ($("#unlink")) $("#unlink").onclick = async () => { try { await ss1({ cmd: "unlink", id: g.id }); this.g = await getGame(g.id, true); toast("Unlinked"); this.draw(); } catch (e) { toast(e.message); } };
		const q = $("#cq");
		let t;
		q.addEventListener("input", () => { clearTimeout(t); t = setTimeout(() => this.cands(q.value), 250); });
		this.cands("");
	}
	async cands(q) {
		const box = $("#cands");
		if (!box) return;
		try {
			const j = await ss1({ cmd: "candidates", id: this.id, q, limit: 30 });
			box.innerHTML = j.games.length ? "" : `<div class="empty">No games on this system match.</div>`;
			for (const c of j.games) {
				const el = h(`<div class="cand" data-nav><div class="thumb" style="background-image:url('${artURL(c.id)}')"></div><div><b>${esc(c.title)}</b><small>${c.id === j.current ? "linked now" : "Link to this"}</small></div></div>`).firstChild;
				el.onclick = async () => {
					try { await ss1({ cmd: "link", id: this.id, base: c.id }); this.g = await getGame(this.id, true); toast(`Now based on ${c.title}`); this.draw(); }
					catch (e) { toast(e.message); }
				};
				box.appendChild(el);
			}
		} catch (e) { box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
	files(body) {
		const g = this.g;
		const fmt = b => b > 1 << 30 ? (b / (1 << 30)).toFixed(1) + " GB" : b > 1 << 20 ? (b / (1 << 20)).toFixed(1) + " MB" : Math.max(1, Math.round(b / 1024)) + " KB";
		body.append(h(`<div class="list" style="flex:1;min-height:0;margin-bottom:1rem">${(g.files || []).map(f => `<div class="row" data-nav><span class="n">${esc(f.path)}</span><span class="v">${esc(f.where)} · ${fmt(f.size)}${f.crc32 ? " · CRC " + esc(f.crc32) : ""}${f.serial ? " · " + esc(f.serial) : ""}${f.online ? "" : " · offline"}</span></div>`).join("") || `<div class="empty">No files.</div>`}</div>`));
	}
}

/* ---------- Match to base games: what box art couldn't find ---------- */
class Match {
	constructor() { this.items = []; this.sel = 0; this.g = null; this.hints = [["A", "Link"], ["Y", "Search"], ["X", "Look up again"], ["B", "Back"]]; }
	render(root) {
		root.append(h(`<div class="sub">${spaced("Library · Not found")}</div><h1>Match to base games</h1>
			<div class="split"><div class="list" id="nf"></div><div class="panel" id="info"><p class="note">Pick a game on the left.</p></div></div>`));
		this.load();
	}
	/* grouped by system, Arcade first, then by generation; LB/RB jump between them */
	async load() {
		const box = $("#nf");
		try {
			const [j, sy] = await Promise.all([ss1({ cmd: "games", filter: "not_found", limit: 5000 }), ss1({ cmd: "systems", all: true })]);
			const CR = { 3: 0, 1: 1, 0: 2, 2: 3, 4: 4 };   /* Arcade, cartridge consoles, disc consoles, handhelds, computers */
			const order = new Map(sy.systems.map((x, i) => [x.id, (CR[x.category] ?? 5) * 100 + i])), names = new Map(sy.systems.map(x => [x.id, x.name]));
			const coll = new Intl.Collator(undefined, { numeric: true, sensitivity: "base" });
			this.items = j.games.sort((a, b) => ((order.get(a.system) ?? 999) - (order.get(b.system) ?? 999)) || coll.compare(a.title, b.title));
			box.innerHTML = this.items.length ? "" : `<div class="empty">Nothing to match: every game was found, or is already linked.</div>`;
			this.groups = [];
			let last = null;
			for (const g of this.items) {
				if (g.system !== last) {
					last = g.system;
					const cnt = this.items.filter(x => x.system === g.system).length;
					box.appendChild(h(`<div class="grp"><span>${spaced(names.get(g.system) || g.system)}</span><span>${cnt}</span></div>`).firstChild);
					this.groups.push(null);
				}
				const el = h(`<div class="row" data-nav><span class="n">${esc(g.title)}</span></div>`).firstChild;
				el.onclick = () => this.firstCand();
				el._g = g;
				el._onfocus = () => { this.g = g; this.sel = this.items.indexOf(g); clearTimeout(this.t); this.t = setTimeout(() => this.panel(g), 150); };
				box.appendChild(el);
				if (this.groups[this.groups.length - 1] === null) this.groups[this.groups.length - 1] = el;
			}
			this.hints = [["A", "Link"], ["Y", "Search"], ["X", "Look up again"], ...(this.groups.length > 1 ? [["LB/RB", "Systems"]] : []), ["B", "Back"]];
			setHints(this.hints);
			if (this.items.length) focus(this.rows()[Math.min(this.sel, this.items.length - 1)]);
		} catch (e) { box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
	rows() { return [...$("#nf").querySelectorAll(".row")]; }
	/* the first game of the next / previous system */
	tab(d) {
		if (!this.groups || this.groups.length < 2) return;
		const r = this.rows(), i = Math.max(0, r.indexOf(cur));
		let gi = 0;
		this.groups.forEach((el, k) => { if (r.indexOf(el) <= i) gi = k; });
		const to = this.groups[(gi + d + this.groups.length) % this.groups.length];
		focus(to);
		if (to.previousElementSibling) to.previousElementSibling.scrollIntoView({ block: "start" });
	}
	async panel(g) {
		const info = $("#info");
		try {
			const [gg, c] = await Promise.all([getGame(g.id), ss1({ cmd: "candidates", id: g.id, limit: 4 })]);
			if (this.g !== g) return;
			const f = (gg.files || [])[0] || {};
			info.innerHTML = `<h2>${esc(gg.title)}</h2><p class="note">${esc(gg.system_name || gg.system)}${f.crc32 ? " · CRC " + esc(f.crc32) : ""}${f.serial ? " · " + esc(f.serial) : ""}</p>
				<p class="note" style="margin-top:.2rem">${esc((f.path || "").split(/[\\/]/).pop())}${f.where ? " · " + esc(f.where) : ""}</p>
				<div class="sub" style="margin-top:1.2rem">${spaced("Based on (suggested)")}</div><div id="cands"></div>
				<p class="note">Uses the base game's box art and details; the SS1 shows it too.</p>
				<div class="acts"><button class="btn" id="srch" data-nav>Search…</button><button class="btn" id="again" data-nav>Look it up again</button></div>`;
			const box = $("#cands");
			const likely = c.games.filter(k => k.score >= 400);   /* only really alike ones; Search finds the rest */
			if (!likely.length) box.innerHTML = `<div class="empty">No likely base game in your library. Use Search to pick one.</div>`;
			for (const k of likely) {
				const el = h(`<div class="cand" data-nav><div class="thumb" style="background-image:url('${artURL(k.id)}')"></div><div><b>${esc(k.title)}</b><small>${esc(gg.system_name || gg.system)}</small></div></div>`).firstChild;
				el.onclick = () => this.link(g, k);
				box.appendChild(el);
			}
			$("#srch").onclick = () => this.y();
			$("#again").onclick = () => this.x();
		} catch (e) { info.innerHTML = `<p class="note">${esc(e.message)}</p>`; }
	}
	firstCand() { const c = $("#cands .cand"); if (c) focus(c); else toast("No suggestion: use Search"); }
	async link(g, base) {
		try {
			await ss1({ cmd: "link", id: g.id, base: base.id });
			toast(`${g.title} now uses ${base.title}`);
			gameCache.delete(g.id);
			this.items.splice(this.items.indexOf(g), 1);
			if (!this.items.length) { $("#nf").innerHTML = `<div class="empty">All matched.</div>`; $("#info").innerHTML = ""; return; }
			const r = this.rows(), i = r.findIndex(x => x._g === g), me = r[i];
			const next = r[i + 1] || r[i - 1];
			const hd = me.previousElementSibling;
			if (hd && hd.classList.contains("grp")) {
				const after = me.nextElementSibling;
				if (!after || after.classList.contains("grp")) { this.groups = this.groups.filter(x => x !== me); hd.remove(); }   /* the system's last one */
				else this.groups = this.groups.map(x => (x === me ? after : x));
			}
			me.remove();
			for (const e of $("#nf").querySelectorAll(".grp")) {
				let c = 0;
				for (let x = e.nextElementSibling; x && !x.classList.contains("grp"); x = x.nextElementSibling) c++;
				e.lastElementChild.textContent = c;
			}
			focus(next);
		} catch (e) { toast(e.message); }
	}
	x() { const g = this.g; if (!g) return; ss1({ cmd: "rescrape", id: g.id }).then(() => toast("It'll be looked up again the next time box art runs"), e => toast(e.message)); }
	y() { const g = this.g; if (g) push(Game, g.id, 1); }
	restore() { this.load(); }
}

/* ---------- Search, every system ---------- */
class Search {
	constructor() { this.hints = [["A", "Open"], ["B", "Back"]]; this.q = ""; }
	render(root) {
		root.append(h(`<div class="sub">${spaced("Library")}</div><h1>Search</h1>
			<div class="field" style="max-width:40rem;flex:none"><input id="q" type="search" data-nav placeholder="Type part of a game's name" value="${esc(this.q)}"></div>
			<div class="split"><div class="list" id="res"></div><div class="panel" id="info"></div></div>`));
		const q = $("#q");
		let t;
		q.addEventListener("input", () => { clearTimeout(t); t = setTimeout(() => this.run(q.value), 250); });
		focus(q);
		if (this.q) this.run(this.q);
	}
	async run(q) {
		this.q = q;
		const box = $("#res");
		if (q.trim().length < 2) { box.innerHTML = ""; return; }
		try {
			const j = await ss1({ cmd: "games", q, limit: 200 });
			box.innerHTML = j.games.length ? "" : `<div class="empty">No games match.</div>`;
			j.games.forEach(g => {
				const el = h(`<div class="row" data-nav><span class="n">${esc(g.title)}</span><span class="v">${esc(g.system)}</span></div>`).firstChild;
				el.onclick = () => push(Game, g.id);
				el._onfocus = () => { clearTimeout(this.t); this.t = setTimeout(async () => { try { $("#info").innerHTML = gamePanel(await getGame(g.id)); } catch (e) {} }, 120); };
				box.appendChild(el);
			});
		} catch (e) { box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
}

/* Settings: settings.js */

/* ---------- start ---------- */
(async () => {
	clock(); setInterval(clock, 15000);
	await refreshState();
	applyTheme();
	replace(S.paired ? Home : Connect);
	if (S.paired && HASH) goTo(HASH);
	/* the connection light: every 10 s while the window is in front, never otherwise */
	setInterval(() => { if (document.visibilityState === "visible" && document.hasFocus()) refreshState(); }, 10000);
	/* the bell, and a screen the tray asked for: a quick local question every 3 s while shown */
	bell();
	setInterval(() => { if (document.visibilityState === "visible") bell(); }, 3000);
	if ([...(navigator.getGamepads ? navigator.getGamepads() : [])].some(g => g)) pad.raf = requestAnimationFrame(padLoop);
})();
