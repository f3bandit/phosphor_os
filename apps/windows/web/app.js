/* Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
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
/* a RetroAchievements command: phosphord answers at once with what it last got and asks again in the
   background ("updating"). show() gets each answer with data; asked again every 1.5 s while it updates
   (up to 2 minutes). Throws when there's an error and nothing to show. */
async function ss1Fresh(cmd, show, alive = () => true) {
	for (let i = 0; i < 80 && alive(); i++) {
		const j = await api("/api/call", cmd);
		if (j.forgotten) { S.paired = false; replace(Connect); return; }
		if (j.ok) show(j);
		else if (!j.updating) throw new Error(j.error || "the SS1 didn't answer");
		if (!j.updating) return;
		await new Promise(res => setTimeout(res, 1500));
	}
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
	const full = Texture.full(S.texture);
	const scale = full ? Math.max(innerWidth / 1280, innerHeight / 720) * (devicePixelRatio || 1)
		: parseFloat(getComputedStyle(document.documentElement).fontSize) / 16;
	const url = Texture.tile(S.texture, [r, g, b], scale);
	document.body.style.backgroundImage = url ? `url(${url})` : "none";
	document.body.style.backgroundSize = full ? "cover" : "";
	document.body.style.backgroundPosition = full ? "center" : "";
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
		const look = S.accent + "/" + S.texture;
		Object.assign(S, { paired: st.paired, connected: !!st.connected, name: st.name, address: st.address, accent: st.accent, texture: st.texture, appVersion: st.app_version, ss_builtin: !!st.ss_builtin });
		S.status = st.status || null;
		if (st.forgotten) S.paired = false;
		if (S.accent + "/" + S.texture !== look) applyTheme();        /* the TV's look changed */
	} catch (e) { S.connected = false; }
	showConn();
}

/* ---------- focus and navigation ---------- */
let cur = null;
/* a locked window (controller mapping): while it's open only its own controls can be reached */
let lockwin = null;
function navs() { return [...document.querySelectorAll("[data-nav]")].filter(e => e.offsetParent !== null && !e.closest("[hidden]") && (!lockwin || lockwin.contains(e))); }
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
/* The hints name the PC's keys: the controller in the SS1's ports can't drive this window. Only after a
   controller plugged into the PC is used do they name its buttons (and the keys again after a key press). */
const KEYS = { A: "Enter", B: "Esc", X: "X", Y: "Y", "LB/RB": "Q/E", Start: "M", "☰": "M" };
let hintList = [], padMode = false;
function setHints(list) {
	hintList = list;
	$("#hints").classList.toggle("pad", padMode);
	$("#hints").innerHTML = list.map(([k, t]) => `<span class="hint"><kbd>${esc(padMode ? k : KEYS[k] || k)}</kbd>${esc(t)}</span>`).join("");
}
function inputMode(pad) {
	if (pad !== padMode) { padMode = pad; setHints(hintList); }
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
	inputMode(false);
	const typing = document.activeElement && document.activeElement.matches("input, textarea");
	const k = e.key;
	if (typing && !["ArrowUp", "ArrowDown", "Escape", "Enter", "Tab"].includes(k)) return;
	if (typing && k === "Enter" && document.activeElement.matches("textarea")) return;
	if (typing && document.activeElement.hasAttribute("data-free") && k !== "Escape") return;   /* an editor: every key is its own */
	const s = topScr();
	if (lockwin && !["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown", "Enter", " ", "Escape"].includes(k)) { e.preventDefault(); return; }
	switch (k) {
	case "ArrowLeft": move(-1, 0); break;
	case "ArrowRight": move(1, 0); break;
	case "ArrowUp": move(0, -1); break;
	case "ArrowDown": move(0, 1); break;
	case "Enter": case " ": activate(); break;
	case "Escape": case "Backspace": if (typing && k === "Backspace") return; if (lockwin) { if (lockwin._esc) lockwin._esc(); break; } if (!(s && s.back && s.back())) back(); break;
	case "q": case "PageUp": s && s.tab && s.tab(-1); break;
	case "e": case "PageDown": s && s.tab && s.tab(1); break;
	case "x": case "X": s && s.x && s.x(); break;
	case "y": case "Y": case "/": s && s.y ? s.y() : push(Search); break;
	case "F1": case "m": case "M": s && s.start ? s.start() : push(AppSettings); break;   /* the controller's Start */
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
			lb: () => s && s.tab && s.tab(-1), rb: () => s && s.tab && s.tab(1), start: () => (s && s.start ? s.start() : push(AppSettings)),
		};
		for (const k in st) {
			if (st[k] && !pad.held[k]) { touched = true; inputMode(true); act[k](); pad.next[k] = now + 400; }
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
			{ id: "music", b: "Music", s: "the player, Steam soundtracks" },
			{ id: "shots", b: "Screenshots", s: "take one, see and save them" },
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
			else if (id === "music") push(Music);
			else if (id === "shots") push(Shots);
			else if (id === "settings") push(AppSettings);
			else if (id === "art") push(AppSettings, "boxart");
			else if (id === "notes") push(AppSettings, "notifications");
			else if (id === "files") api("/api/explore", {}).then(j => { if (!j.ok) toast(j.error); });
		}));
		try {
			const j = await ss1({ cmd: "games", filter: "not_found", limit: 1 });
			const el = $("#t-match");
			let am = 0;
			try { am = (await api("/api/automatch")).matches.filter(m => !m.kept).length; } catch (e) {}
			const parts = [j.total ? `${n(j.total)} not found` : "", am ? `${n(am)} matched to check` : ""].filter(Boolean);
			if (el) { el.textContent = parts.length ? parts.join(", ") : "nothing to review"; el.className = parts.length ? "warn" : ""; }
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

/* ---------- Music: the SS1's player (it plays on the TV), its albums, Steam soundtracks from this PC ---------- */
const mcoverURL = id => `/api/mcover/${encodeURIComponent(id)}?k=${encodeURIComponent(KEY)}`;
const msTime = ms => { ms = Math.max(0, ms || 0); return `${Math.floor(ms / 60000)}:${String(Math.floor(ms / 1000) % 60).padStart(2, "0")}`; };
async function mctl(d, extra) { try { await ss1(Object.assign({ cmd: "music_ctl", do: d }, extra || {})); } catch (e) { toast(e.message); } }
class Music {
	constructor() { this.t = 0; this.hints = [["A", "Choose"], ["LB/RB", "Tabs"], ["B", "Back"]]; }
	render(root) {
		root.append(h(`<div class="sub">${spaced("Tools")}</div><h1>Music</h1>`));
		tabs(root, ["Now playing", "Albums", "Steam soundtracks"], this.t, i => { this.t = i; show(this); });
		root.append(h(`<div id="mbody" style="flex:1;min-height:0;display:flex;flex-direction:column"></div>`));
		clearTimeout(this.timer);
		[this.now, this.albums, this.steam][this.t].call(this, $("#mbody"));
	}
	tab(d) { this.t = (this.t + d + 3) % 3; show(this); }
	back() { clearTimeout(this.timer); return false; }
	mine(b) { return topScr() === this && document.body.contains(b); }
	/* the player on the TV, followed once a second */
	now(b) {
		b.innerHTML = `<div class="split" style="grid-template-columns:3fr 2fr"><div class="panel" id="np" style="overflow:auto"></div><div class="list" id="nq"></div></div>`;
		let qkey = "", built = "";
		const draw = async () => {
			if (!this.mine(b)) return;
			let m;
			try { m = (await ss1({ cmd: "music" })).music; } catch (e) { $("#np").innerHTML = `<div class="empty">${esc(e.message)}</div>`; this.timer = setTimeout(draw, 3000); return; }
			if (!this.mine(b)) return;
			const t = m.track, np = $("#np");
			const key = t ? `${t.album_id}|${t.index}|${t.title}` : "none";
			if (key !== built) {                    /* a new track: build the panel; else just the moving parts */
				built = key;
				const had = cur && np.contains(cur) ? cur.id : "";
				np.innerHTML = !t ? `<div class="empty">Nothing is playing.${m.error ? ` ${esc(m.error)}` : ""} Pick an album in Albums, or put a music CD in the dock's drive.</div>`
					: `<div style="display:flex;gap:1.2rem;align-items:center">${t.cover ? `<div style="width:9rem;height:9rem;flex:none;border-radius:8px;background:center/cover url('${mcoverURL(t.album_id || "cd")}')"></div>` : ""}
					<div style="min-width:0"><div class="sub" id="mp_state"></div><h2 style="margin:.3rem 0">${esc(t.title)}</h2>
					<p class="note" style="margin:0">${esc([t.artist, t.album].filter(Boolean).join(" · "))}</p></div></div>
					<div style="height:6px;background:rgba(255,255,255,.12);border-radius:3px;margin:1rem 0 .4rem"><div id="mp_bar" style="height:100%;width:0;background:var(--acc);border-radius:3px"></div></div>
					<p class="note" id="mp_time"></p>
					<div class="acts"><button class="btn" id="mp_prev" data-nav>Previous</button><button class="btn primary" id="mp_play" data-nav>Pause</button><button class="btn" id="mp_next" data-nav>Next</button><button class="btn" id="mp_stop" data-nav>Stop</button></div>
					<div class="acts"><button class="btn" id="mp_shuf" data-nav></button><button class="btn" id="mp_rep" data-nav></button><button class="btn" id="mp_vd" data-nav>Volume −</button><button class="btn" id="mp_vu" data-nav>Volume +</button></div>`;
				if (t) {
					$("#mp_prev").onclick = () => mctl("previous").then(draw);
					$("#mp_next").onclick = () => mctl("next").then(draw);
					$("#mp_play").onclick = () => mctl("toggle").then(draw);
					$("#mp_stop").onclick = () => mctl("stop").then(draw);
					$("#mp_shuf").onclick = () => mctl("shuffle", { on: !this.m.shuffle }).then(draw);
					$("#mp_rep").onclick = () => mctl("repeat", { mode: { off: "all", all: "one", one: "off" }[this.m.repeat] || "off" }).then(draw);
					$("#mp_vd").onclick = () => mctl("volume", { value: Math.max(0, this.m.volume - 10) }).then(draw);
					$("#mp_vu").onclick = () => mctl("volume", { value: Math.min(100, this.m.volume + 10) }).then(draw);
				}
				if (had && $("#" + had)) focus($("#" + had), true);
				else if (!touched && t) focus($("#mp_play"), true);
			}
			this.m = m;
			if (t) {
				$("#mp_state").textContent = `${m.state === "paused" ? "Paused" : "Now playing"}${t.cd ? " · music CD" : ""} · on the TV`;
				$("#mp_bar").style.width = `${t.len_ms ? Math.min(100, t.pos_ms * 100 / t.len_ms) : 0}%`;
				$("#mp_time").textContent = `${msTime(t.pos_ms)} / ${msTime(t.len_ms)} · track ${t.index + 1} of ${t.count}${t.next ? ` · next: ${t.next}` : ""} · volume ${m.volume}`;
				$("#mp_play").textContent = m.state === "paused" ? "Play" : "Pause";
				$("#mp_shuf").textContent = `Shuffle: ${m.shuffle ? "on" : "off"}`;
				$("#mp_rep").textContent = `Repeat: ${m.repeat}`;
			}
			/* the queue, when it changed */
			try {
				const q = t ? (await ss1({ cmd: "music_queue" })).queue || [] : [];
				const k = q.map(x => x.title + (x.current ? "*" : "")).join("\n");
				if (k !== qkey && this.mine(b)) {
					qkey = k;
					const box = $("#nq"), had = cur && box.contains(cur) ? +cur.dataset.i : -1;
					box.innerHTML = q.length ? q.map((x, i) => `<div class="row${x.current ? " on" : ""}" data-nav data-i="${i}"><span class="n">${x.current ? "♪ " : ""}${esc(x.title)}</span><span class="v">${i + 1}</span></div>`).join("") : `<div class="empty">The queue is empty.</div>`;
					box.querySelectorAll(".row").forEach(r => (r.onclick = () => mctl("jump", { index: +r.dataset.i }).then(draw)));
					if (had >= 0 && box.children[had]) focus(box.children[had], true);
				}
			} catch (e) {}
			if (this.mine(b)) this.timer = setTimeout(draw, 1000);
		};
		draw();
	}
	/* the SS1's albums (and a music CD in the drive): play one on the TV */
	async albums(b) {
		b.innerHTML = `<div class="split"><div class="list" id="ml"><div class="empty"><span class="spin"></span>Asking the SS1…</div></div><div class="panel" id="mi"></div></div>`;
		let j;
		try { j = await ss1({ cmd: "music_albums" }); } catch (e) { $("#ml").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
		if (!this.mine(b)) return;
		const list = (j.cd ? [j.cd] : []).concat(j.albums || []);
		const box = $("#ml");
		if (!list.length) { box.innerHTML = `<div class="empty">No music on the SS1 yet. Send soundtracks from Steam soundtracks, copy a CD on the TV (Music › the CD › X), or put albums in a Music folder on its SD card or a USB drive.</div>`; return; }
		box.innerHTML = "";
		const src = a => a.id === "cd" ? "in the drive" : a.source === "steam" ? "Steam" : a.source === "cd" ? "from a CD" : "";
		list.forEach(a => {
			const el = h(`<div class="row" data-nav><span class="n">${a.id === "cd" ? "◉ " : ""}${esc(a.title)}</span><span class="v">${esc(src(a))}</span></div>`).firstChild;
			el._onfocus = async () => {
				let d = a;
				if (a.id !== "cd") try { d = (await ss1({ cmd: "music_album", id: a.id })).album; } catch (e) {}
				if (cur !== el) return;
				$("#mi").innerHTML = `<div style="display:flex;gap:1rem;align-items:center">${a.cover ? `<div style="width:7rem;height:7rem;flex:none;border-radius:6px;background:center/cover url('${mcoverURL(a.id)}')"></div>` : ""}<div style="min-width:0"><h2 style="margin:0 0 .3rem">${esc(a.title)}</h2>
					<p class="note" style="margin:0">${esc([a.artist, a.year, `${a.tracks} track${a.tracks === 1 ? "" : "s"}`].filter(Boolean).join(" · "))}${a.id === "cd" && a.looking ? " · looking up the names…" : ""}</p></div></div>
					<div class="acts"><button class="btn primary" id="ma_play" data-nav>Play on the TV</button><button class="btn" id="ma_shuf" data-nav>Shuffle</button></div>
					<ol class="note" style="margin:.8rem 0 0 1.2rem;padding:0;overflow:auto;max-height:22rem">${(d.track_list || []).map(t => `<li>${esc(t.title)}</li>`).join("")}</ol>`;
				const play = async shuffle => {
					await mctl("shuffle", { on: shuffle });
					try { await ss1({ cmd: "music_play", album: a.id }); toast(`Playing ${a.title} on the TV`); this.t = 0; show(this); } catch (e) { toast(e.message); }
				};
				$("#ma_play").onclick = () => play(false);
				$("#ma_shuf").onclick = () => play(true);
			};
			el.onclick = () => { if ($("#ma_play")) $("#ma_play").click(); };
			box.appendChild(el);
		});
		focus(box.firstElementChild);
	}
	/* Steam soundtracks installed on this PC: tick some, send them */
	steam(b) {
		b.innerHTML = `<p class="note">Soundtracks Steam has installed on this PC, read from Steam's own records. Phosphor doesn't sign in to Steam or change anything there. A soundtrack you own but haven't downloaded shows up after you install it in Steam.</p>
			<div class="list" id="sl" style="flex:1"><div class="empty"><span class="spin"></span>Looking…</div></div>
			<div class="acts"><button class="btn primary" id="st_send" data-nav>Send the ticked ones to the SS1</button><span class="note" id="st_job"></span></div>`;
		const picked = new Set();
		const draw = async first => {
			if (!this.mine(b)) return;
			let j;
			try { j = await api("/api/steam"); } catch (e) { $("#sl").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
			if (!this.mine(b)) return;
			const box = $("#sl");
			if (first) {
				if (!j.steam) box.innerHTML = `<div class="empty">Steam isn't installed on this PC.</div>`;
				else if (!j.soundtracks.length) box.innerHTML = `<div class="empty">No soundtracks are installed in Steam (looked in ${j.libraries} Steam librar${j.libraries === 1 ? "y" : "ies"}).</div>`;
				else {
					box.innerHTML = "";
					j.soundtracks.forEach(s => {
						const el = h(`<div class="row" data-nav style="gap:.8rem"><span class="ck" style="width:1rem;height:1rem;border:2px solid currentColor;border-radius:3px;flex:none;opacity:.7"></span>
							${s.cover ? `<span style="width:2.4rem;height:2.4rem;flex:none;border-radius:4px;background:center/cover url('/api/steam/cover/${encodeURIComponent(s.appid)}?k=${encodeURIComponent(KEY)}')"></span>` : ""}
							<span class="n">${esc(s.title)}<small class="note" style="display:block">${s.tracks} tracks · ${esc(s.format)} · ${Math.round(s.size / 1048576)} MB</small></span><span class="v" data-on="${esc(s.appid)}">${s.on_ss1 ? "On the SS1" : ""}</span></div>`).firstChild;
						el.onclick = () => {
							if (picked.has(s.appid)) picked.delete(s.appid); else picked.add(s.appid);
							const ck = el.querySelector(".ck"); ck.style.background = picked.has(s.appid) ? "var(--acc)" : ""; ck.style.opacity = picked.has(s.appid) ? 1 : .7;
						};
						box.appendChild(el);
					});
				}
			} else (j.soundtracks || []).forEach(s => { const v = box.querySelector(`[data-on="${CSS.escape(s.appid)}"]`); if (v) v.textContent = s.on_ss1 ? "On the SS1" : ""; });
			const jb = j.job || {};
			$("#st_job").textContent = jb.busy ? `Sending ${jb.title || "…"}${jb.total ? `: ${Math.floor(jb.done * 100 / jb.total)}%` : ""}${jb.waiting ? ` (${jb.waiting} more after it)` : ""}` : (jb.message || "");
			if (jb.busy) this.timer = setTimeout(() => draw(false), 1500);
		};
		$("#st_send").onclick = async () => {
			if (!picked.size) { toast("Tick a soundtrack first"); return; }
			try { await api("/api/steam", { send: [...picked] }); toast("Sending to the SS1's Music"); } catch (e) { toast(e.message); }
			clearTimeout(this.timer); draw(false);
		};
		draw(true);
	}
}

/* ---------- Screenshots: the SS1's (screenshots/<system>/ on its SD card); take one of the TV ---------- */
const shotURL = (kind, id, dl) => `/api/shot/${kind}/${id.split("/").map(encodeURIComponent).join("/")}?k=${encodeURIComponent(KEY)}${dl ? "&dl=1" : ""}`;
class Shots {
	constructor() { this.sel = 0; this.hints = [["A", "View"], ["X", "Take one"], ["B", "Back"]]; }
	render(root) {
		root.append(h(`<div class="sub">${spaced("Tools")}</div><h1>Screenshots</h1>
			<div class="acts" style="flex:none"><button class="btn primary" id="sh_take" data-nav>Take a screenshot of the TV</button><button class="btn" id="sh_save" data-nav>Save to this PC</button><button class="btn" id="sh_del" data-nav>Delete</button><span class="note" id="sh_info"></span></div>
			<div id="sh_grid" style="flex:1;min-height:0;overflow:auto;display:grid;grid-template-columns:repeat(4,1fr);gap:12px;align-content:start;margin-top:12px"><div class="empty"><span class="spin"></span></div></div>`));
		$("#sh_take").onclick = () => this.x();
		$("#sh_save").onclick = () => { const s = this.cur(); if (s) location.href = shotURL("file", s.id, true); };
		twice($("#sh_del"), "Press again to delete", async () => {
			const s = this.cur();
			if (!s) return;
			try { await ss1({ cmd: "shot_delete", id: s.id }); toast("Deleted"); } catch (e) { toast(e.message); }
			this.load();
		});
		this.load();
		/* new ones taken on the SS1 (L3 + R3) show up while this is open */
		clearInterval(this.poll);
		this.poll = setInterval(async () => {
			if (topScr() !== this || !$("#sh_grid")) { clearInterval(this.poll); return; }
			if (lockwin) return;
			try { const j = await ss1({ cmd: "shots" }); if (this.key(j.shots) !== this.key(this.list)) this.load(); } catch (e) {}
		}, 3000);
	}
	key(l) { return (l || []).map(s => s.id).join("|"); }
	cur() { return (this.list || [])[this.sel]; }
	x() { this.take(); }
	async take() {
		toast("Taking a screenshot of the TV…");
		try { const r = await ss1({ cmd: "shot" }); toast(r.where === "game" ? "Saved by MiSTer: it shows up here in a moment" : "Screenshot saved"); } catch (e) { toast(e.message); }
		setTimeout(() => this.load(), 1500);
	}
	async load() {
		let j;
		try { j = await ss1({ cmd: "shots" }); } catch (e) { const g = $("#sh_grid"); if (g) g.innerHTML = `<div class="empty">${esc(/unknown/i.test(e.message) ? "This SS1's Phosphor doesn't have screenshots yet: update Phosphor." : e.message)}</div>`; return; }
		const g = $("#sh_grid");
		if (!g || topScr() !== this) return;
		this.list = j.shots || [];
		if (this.sel >= this.list.length) this.sel = Math.max(0, this.list.length - 1);
		$("#sh_info").textContent = this.list.length ? `${this.list.length} on the SS1 · ${(j.bytes / 1048576).toFixed(0)} MB` : "";
		if (!this.list.length) { g.innerHTML = `<div class="empty">No screenshots yet. On the SS1: press both sticks in (L3 + R3), in a game or on Phosphor's screens. Or take one of the TV from here.</div>`; return; }
		g.innerHTML = "";
		this.list.forEach((s, i) => {
			const when = new Date(s.at * 1000).toLocaleString([], { month: "short", day: "numeric", hour: "numeric", minute: "2-digit" });
			const el = h(`<div class="row" data-nav style="display:block;padding:6px;height:auto;border-radius:10px">
				<div style="aspect-ratio:16/9;border-radius:6px;background:#000 center/cover no-repeat url('${shotURL("thumb", s.id)}')"></div>
				<small class="note" style="display:block;margin-top:4px">${esc(s.core)} · ${esc(when)}</small></div>`).firstChild;
			el._onfocus = () => { this.sel = i; };
			el.onclick = () => { this.sel = i; this.view(); };
			g.appendChild(el);
		});
		focus(g.children[this.sel], true);
	}
	/* one full size, over the page: Left / Right through them, Esc closes */
	view() {
		const s = this.cur();
		if (!s) return;
		const w = h(`<div class="lockwin" style="background:rgba(0,0,0,.92)"><div style="position:absolute;inset:24px 24px 64px;display:flex;align-items:center;justify-content:center">
			<img id="sv_img" style="max-width:100%;max-height:100%;border-radius:6px" src="${shotURL("file", s.id)}" alt=""></div>
			<div class="acts" style="position:absolute;left:24px;right:24px;bottom:12px;justify-content:space-between"><span class="note" id="sv_cap"></span>
			<span><button class="btn" id="sv_prev" data-nav>Previous</button><button class="btn" id="sv_next" data-nav>Next</button><button class="btn primary" id="sv_save" data-nav>Save to this PC</button><button class="btn" id="sv_close" data-nav>Close</button></span></div></div>`).firstChild;
		document.body.appendChild(w);
		const prev = lockwin; lockwin = w;
		const show = () => { const c = this.cur(); $("#sv_img", w).src = shotURL("file", c.id); $("#sv_cap", w).textContent = `${c.core} · ${new Date(c.at * 1000).toLocaleString()} · ${this.sel + 1} of ${this.list.length}`; };
		const close = () => { lockwin = prev; w.remove(); const g = $("#sh_grid"); if (g && g.children[this.sel]) focus(g.children[this.sel], true); };
		w._esc = close;
		$("#sv_close", w).onclick = close;
		$("#sv_prev", w).onclick = () => { if (this.sel > 0) { this.sel--; show(); } };
		$("#sv_next", w).onclick = () => { if (this.sel + 1 < this.list.length) { this.sel++; show(); } };
		$("#sv_save", w).onclick = () => { location.href = shotURL("file", this.cur().id, true); };
		show();
		focus($("#sv_next", w), true);
	}
}

/* ---------- Systems in a category ---------- */
/* Disc also has the disc in the dock's drive at the top while one is in, as on the TV:
   Play starts it on the SS1, Eject opens the tray */
function discIn(d) { return d && d.enabled && ["reading", "ready", "unreadable"].includes(d.state); }
function discLine(d) { return d.state === "reading" ? "reading the disc…" : d.state === "unreadable" ? "a disc that can't be read" : (d.title || "a disc"); }
const discArtURL = d => `/api/art/disc?k=${encodeURIComponent(KEY)}&s=${encodeURIComponent((d.serial || "") + "|" + (d.title || "") + "|" + (d.art ? 1 : 0))}`;
async function discEject() { try { await ss1({ cmd: "disc_eject" }); toast("Opening the tray"); } catch (e) { toast(e.message); } }
async function discPlay(d) { try { await ss1({ cmd: "disc_play" }); toast(`Starting ${d.title || "the disc"} on the TV`); } catch (e) { toast(e.message); } }

class Systems {
	constructor(cat) { this.cat = cat; this.hints = [["A", "Open"], ["Y", "Search"], ["B", "Back"]]; this.sel = 0; this.isDisc = cat.name === "Disc"; }
	render(root) {
		root.append(h(`<div class="sub">${spaced("Library")}</div><h1>${esc(this.cat.name)}</h1>
			<div class="split"><div class="list" id="sys"></div><div class="panel" id="info"></div></div>`));
		this.load();
	}
	async load() {
		const box = $("#sys");
		try {
			const [j, dj] = await Promise.all([ss1({ cmd: "systems", category: this.cat.id }), this.isDisc ? ss1({ cmd: "disc" }).catch(() => null) : null]);
			this.systems = j.systems;
			this.drawList(dj && dj.disc);
			if (this.isDisc && dj && dj.disc) this.watch();
		} catch (e) { box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
	drawList(d) {
		const box = $("#sys");
		if (!box) return;
		const wasDrive = cur && cur.classList.contains("drive");
		this.disc = d;
		box.innerHTML = "";
		const inDrive = discIn(d);
		if (inDrive) {
			const el = h(`<div class="row drive" data-nav><span class="n"><span class="ic">${ICON.Disc || ""}</span><span class="t">In the drive · ${esc(discLine(d))}</span></span><span class="v"></span></div>`).firstChild;
			el.onclick = () => { if (this.disc.playable) discPlay(this.disc); else toast(this.disc.reason || "This disc can't be played"); };
			el._onfocus = () => this.drivePanel();
			box.appendChild(el);
		}
		if (!this.systems.length && !inDrive) box.innerHTML = `<div class="empty">No games here yet.</div>`;
		this.systems.forEach((s, i) => {
			const el = h(`<div class="row" data-nav><span class="n">${esc(s.name)}</span><span class="v">${n(s.games)}</span></div>`).firstChild;
			el.onclick = () => { this.sel = i; push(Games, { system: s.id, title: s.name }); };
			el._onfocus = () => { this.sel = i; $("#info").innerHTML = `<h2>${esc(s.name)}</h2><dl class="kv"><dt>Games</dt><dd>${n(s.games)}</dd><dt>Core</dt><dd>${esc(s.core || "–")}</dd></dl>`; };
			box.appendChild(el);
		});
		const rows = box.querySelectorAll(".row");
		const first = !this.drawn;               /* opened: on the disc, as on the TV */
		this.drawn = true;
		focus((wasDrive || first) && inDrive ? rows[0] : rows[Math.min(this.sel + (inDrive ? 1 : 0), rows.length - 1)], touched);
	}
	drivePanel() {
		const d = this.disc, info = $("#info");
		if (!d || !info) return;
		const sub = d.system && d.serial ? `${esc(d.system)} · ${esc(d.serial)}` : esc(d.system || "In the dock's drive");
		info.innerHTML = `${d.art ? `<div class="boxart" style="background-image:url('${discArtURL(d)}')"></div>` : ""}<h2>${esc(discLine(d))}</h2>
			<p class="note">${sub}</p>${d.playable ? "" : `<p class="note warn">${esc(d.reason || "This disc can't be played")}</p>`}
			<div class="acts"><button class="btn primary" id="dk_play" data-nav${d.playable ? "" : " disabled"}>Play on TV</button><button class="btn" id="dk_ej" data-nav>Eject</button></div>`;
		$("#dk_play").onclick = () => discPlay(d);
		$("#dk_ej").onclick = discEject;
	}
	/* the drive while this screen is open: a disc in or out, its name and art when they're found */
	watch() {
		clearTimeout(this.wt);
		this.wt = setTimeout(async () => {
			if (topScr() !== this || !$("#sys")) return;
			try {
				const d = (await ss1({ cmd: "disc" })).disc;
				const key = x => x ? [discIn(x), x.state, x.serial, x.title, !!x.art, x.playable].join("|") : "";
				if (key(d) !== key(this.disc)) {
					if (cur && cur.classList.contains("drive") || discIn(d) !== discIn(this.disc)) this.drawList(d);
					else this.disc = d;
					if (cur && cur.classList.contains("drive")) this.drivePanel();
				}
			} catch (e) {}
			this.watch();
		}, 2000);
	}
	restore() { if (this.isDisc) this.watch(); }
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
	let kv = DETAIL_KEYS.filter(([k]) => g[k]).map(([k, l]) => `<dt>${l}</dt><dd>${esc(g[k])}</dd>`).join("");
	if (g.ra && g.ra.total) kv += `<dt>Achievements</dt><dd>${g.ra.unlocked} / ${g.ra.total}</dd>`;      /* RetroAchievements */
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
	tabNames() { return ["Details", "Based on", "Files"].concat(this.g && this.g.ra && this.g.ra.total ? ["Achievements"] : []); }
	tab(d) { const k = this.tabNames().length; this.t = (this.t + d + k) % k; this.draw(); }
	async load() {
		try { this.g = await getGame(this.id, true); this.draw(); } catch (e) { $("#body").innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
	draw() {
		const g = this.g;
		$("#gt").textContent = g.title;
		$("#tb").innerHTML = "";
		if (this.t >= this.tabNames().length) this.t = 0;
		tabs($("#tb"), this.tabNames(), this.t, i => { this.t = i; this.draw(); });
		const body = $("#body");
		body.innerHTML = "";
		cur = null;
		if (this.t === 0) this.details(body); else if (this.t === 1) this.basedOn(body); else if (this.t === 2) this.files(body); else this.achievements(body);
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
	/* RetroAchievements: this game's set and what's unlocked (needs the Web API key on the SS1) */
	async achievements(body) {
		body.append(h(`<div class="list grow" id="ach"><div class="empty"><span class="spin"></span>Asking RetroAchievements…</div></div>`));
		try { await ss1Fresh({ cmd: "ra_game", id: this.id }, j => this.achList(j.ra), () => !!$("#ach")); }
		catch (e) { if ($("#ach")) $("#ach").innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
	achList(r) {
		const box = $("#ach");
		if (!box || !r) return;
		if (!r.supported) { box.innerHTML = `<div class="empty">RetroAchievements has no achievements for this version of the game.</div>`; return; }
		box.innerHTML = `<p class="note">${r.unlocked} of ${r.total} unlocked${r.hardcore ? ` (${r.hardcore} in hardcore)` : ""} · ${n(r.points_unlocked)} of ${n(r.points)} points</p>`;
		(r.achievements || []).forEach(a => box.append(h(`<div class="row" data-nav style="gap:.8rem;${a.unlocked ? "" : "opacity:.6"}">${a.badge ? `<img src="${esc(a.badge)}" alt="" style="width:2.4rem;height:2.4rem;border-radius:4px;flex:none">` : ""}
			<span class="n">${esc(a.title)}<small class="note" style="display:block">${esc(a.description)}</small></span><span class="v">${a.points} pts${a.unlocked ? (a.hardcore ? " · ✓ hardcore" : " · ✓") : ""}</span></div>`)));
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
		if ($("#unlink")) $("#unlink").onclick = async () => { try { await ss1({ cmd: "unlink", id: g.id }); amForget(g.id); this.g = await getGame(g.id, true); toast("Unlinked"); this.draw(); } catch (e) { toast(e.message); } };
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
					try { await ss1({ cmd: "link", id: this.id, base: c.id }); amForget(this.id); this.g = await getGame(this.id, true); toast(`Now based on ${c.title}`); this.draw(); }
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
/* an automatic match the user replaced by hand */
const amForget = id => api("/api/automatch", { do: "forget", id }).catch(() => {});
const AM_KIND = { translation: "Translation", hack: "Hack", fan: "Fan version", spelling: "Same game", name: "Other name", related: "Built on", port: "Port", notgame: "Not a game" };

class Match {
	constructor(tab = 0) { this.t = tab; this.items = []; this.sel = 0; this.g = null; this.hints = [["A", "Link"], ["Y", "Search"], ["X", "Look up again"], ["B", "Back"]]; }
	render(root) {
		root.append(h(`<div class="sub">${spaced("Library · Match")}</div><h1>Match to base games</h1><div id="tb" style="flex:none"></div>
			<div class="split"><div class="list" id="nf"></div><div class="panel" id="info"><p class="note">Pick a game on the left.</p></div></div>`));
		this.bar();
		this.auto = this.t === 1 ? new AutoMatch(this) : null;
		this.ng = this.t === 2 ? new NotGames(this) : null;
		if (this.auto) this.auto.load(); else if (this.ng) this.ng.load(); else this.load();
	}
	/* the three tabs, with their counts */
	async bar(counts) {
		const tb = $("#tb");
		if (!tb) return;
		if (!counts) {
			counts = [null, null, null];
			try { counts[0] = (await ss1({ cmd: "games", filter: "not_found", limit: 1 })).total; } catch (e) {}
			try { counts[1] = (await api("/api/automatch")).matches.filter(m => !m.kept).length; } catch (e) {}
			try { counts[2] = (await ss1({ cmd: "games", filter: "not_game", limit: 1 })).total; } catch (e) {}   /* older phosphord: no tab count */
			if (!$("#tb")) return;
		}
		tb.innerHTML = "";
		const lab = (t, c) => c == null ? t : `${t} · ${n(c)}`;
		tabs(tb, [lab("Not found", counts[0]), lab("Matched automatically", counts[1]), lab("Not games", counts[2])], this.t, i => this.go(i));
	}
	go(i) { if (i === this.t) return; this.t = i; this.sel = 0; show(this); }
	start() { if (this.auto) this.auto.keepAll(); else push(AppSettings); }
	/* Not found: a boot file, test ROM or tool: set aside (Not games), no box art looked up */
	async notGame(g) {
		try { await ss1({ cmd: "not_game", id: g.id, on: true }); toast(`${g.title}: set aside as not a game`); gameCache.delete(g.id); this.dropRow(g); this.bar(); }
		catch (e) { toast(e.message === "unknown cmd" ? "Update Phosphor on the SS1 (0.27.33 or later) for this" : e.message); }
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
				el._onfocus = () => { this.g = g; this.sel = this.items.indexOf(g); clearTimeout(this.pt); this.pt = setTimeout(() => this.panel(g), 150); };
				box.appendChild(el);
				if (this.groups[this.groups.length - 1] === null) this.groups[this.groups.length - 1] = el;
			}
			this.hints = [["A", "Link"], ["Y", "Search"], ["X", "Look up again"], ["LB/RB", "Tabs"], ["B", "Back"]];
			setHints(this.hints);
			if (this.items.length) focus(this.rows()[Math.min(this.sel, this.items.length - 1)]);
		} catch (e) { box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
	rows() { return [...$("#nf").querySelectorAll(".row")]; }
	/* LB/RB: the next tab (Not found, Matched automatically, Not games) */
	tab(d) { this.go((this.t + d + 3) % 3); }

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
				<div class="acts"><button class="btn" id="srch" data-nav>Search…</button><button class="btn" id="again" data-nav>Look it up again</button><button class="btn" id="amall" data-nav>Match all automatically</button><button class="btn" id="ngame" data-nav>Not a game</button></div>`;
			const box = $("#cands");
			const likely = c.games.filter(k => k.score >= 400);   /* only really alike ones; Search finds the rest */
			if (!likely.length) box.innerHTML = `<div class="empty">No likely base game in your library. Use Search to pick one.</div>`;
			for (const k of likely) {
				const el = h(`<div class="cand" data-nav><div class="thumb" style="background-image:url('${artURL(k.id)}')"></div><div><b>${esc(k.title)}</b><small>${esc(gg.system_name || gg.system)}</small></div></div>`).firstChild;
				el.onclick = () => this.link(g, k);
				box.appendChild(el);
			}
			$("#srch").onclick = () => this.y();
			$("#amall").onclick = async () => {
				try { const r = await api("/api/automatch", { do: "run" }); if (!r.ok) return toast(r.error); toast("Matching to base games"); this.go(1); } catch (e) { toast(e.message); }
			};
			$("#again").onclick = () => this.x();
			$("#ngame").onclick = () => this.notGame(g);
		} catch (e) { info.innerHTML = `<p class="note">${esc(e.message)}</p>`; }
	}
	firstCand() { const c = $("#cands .cand"); if (c) focus(c); else toast("No suggestion: use Search"); }
	async link(g, base) {
		try {
			await ss1({ cmd: "link", id: g.id, base: base.id });
			toast(`${g.title} now uses ${base.title}`);
			gameCache.delete(g.id);
			this.dropRow(g);
		} catch (e) { toast(e.message); }
	}
	/* a game left Not found: its row goes, the next one is chosen */
	dropRow(g) {
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
	}
	x() { if (this.ng) return this.ng.back(); if (this.auto) return this.auto.undo(); const g = this.g; if (!g) return; ss1({ cmd: "rescrape", id: g.id }).then(() => toast("It'll be looked up again the next time box art runs"), e => toast(e.message)); }
	y() { if (this.ng) return; if (this.auto) return this.auto.pick(); const g = this.g; if (g) push(Game, g.id, 1); }
	restore() { this.bar(); if (this.auto) this.auto.load(); else if (this.ng) this.ng.load(); else this.load(); }
}

/* ---------- Match > Not games: boot files, test ROMs and tools set aside (by hand or automatically) ---------- */
class NotGames {
	constructor(page) { this.page = page; this.items = []; this.g = null; this.sel = 0; }
	async load() {
		const box = $("#nf"), info = $("#info");
		try {
			const [j, sy] = await Promise.all([ss1({ cmd: "games", filter: "not_game", limit: 5000 }), ss1({ cmd: "systems", all: true })]);
			const names = new Map(sy.systems.map(x => [x.id, x.name]));
			this.items = j.games;
			box.innerHTML = this.items.length ? "" : `<div class="empty">Nothing set aside.</div>`;
			info.innerHTML = `<h2>Not games</h2><p>Boot files, test ROMs, cheat discs and tools: out of Not found, and no box art is looked up for them. They still play from the library.</p>
				<p class="note">Set aside by Match all automatically, or with Not a game on the Not found tab.</p>`;
			let last = null;
			for (const g of this.items) {
				if (g.system !== last) {
					last = g.system;
					box.appendChild(h(`<div class="grp"><span>${spaced(names.get(g.system) || g.system)}</span><span>${this.items.filter(x => x.system === g.system).length}</span></div>`).firstChild);
				}
				const el = h(`<div class="row" data-nav><span class="n">${esc(g.title)}</span></div>`).firstChild;
				el._onfocus = () => { this.g = g; this.sel = this.items.indexOf(g); this.panel(g); };
				el.onclick = () => { const b = $("#ngback"); if (b) focus(b); };
				box.appendChild(el);
			}
			this.page.hints = [["A", "Choose"], ["X", "It's a game"], ["LB/RB", "Tabs"], ["B", "Back"]];
			setHints(this.page.hints);
			const rows = [...box.querySelectorAll(".row")];
			if (rows.length) focus(rows[Math.min(this.sel, rows.length - 1)]);
		} catch (e) { box.innerHTML = `<div class="empty">${esc(e.message === "unknown cmd" || /filter/.test(e.message) ? "Update Phosphor on the SS1 (0.27.33 or later) for this" : e.message)}</div>`; }
	}
	panel(g) {
		$("#info").innerHTML = `<h2>${esc(g.title)}</h2><p>Set aside as not a game: out of Not found, no box art looked up.</p>
			<div class="acts"><button class="btn" id="ngback" data-nav>It's a game</button></div>`;
		$("#ngback").onclick = () => this.back();
	}
	/* back to Not found (an automatic one is undone, so it isn't set aside again) */
	async back() {
		const g = this.g;
		if (!g) return;
		try {
			const am = (await api("/api/automatch")).matches.find(m => m.id === g.id && m.how === "skip");
			if (am) { const r = await api("/api/automatch", { do: "undo", id: g.id }); if (!r.ok) return toast(r.error); }
			else await ss1({ cmd: "not_game", id: g.id, on: false });
			toast(`${g.title}: back in Not found`);
			this.page.bar(); this.load();
		} catch (e) { toast(e.message); }
	}
}

/* ---------- Match > Matched automatically: what automatic matching did, to keep or undo ---------- */
class AutoMatch {
	constructor(page) { this.page = page; this.items = []; this.m = null; this.sel = 0; }
	async load() {
		const box = $("#nf"), info = $("#info");
		try {
			const [j, sy, sc] = await Promise.all([api("/api/automatch"), ss1({ cmd: "systems", all: true }), api("/api/scrape")]);
			const names = new Map(sy.systems.map(x => [x.id, x.name]));
			this.items = j.matches;
			const run = sc.state && sc.state.running;
			clearTimeout(this.poll);
			if (run) this.poll = setTimeout(() => { if ($("#nf") && this.page.auto === this && topScr() === this.page) { this.page.bar(); this.load(); } }, 2500);
			box.innerHTML = "";
			if (!this.items.length) {
				box.innerHTML = `<div class="empty">${run ? esc(sc.state.status || "Working") + (sc.state.game ? ": " + esc(sc.state.game) : "") : "Nothing matched automatically yet."}</div>`;
				info.innerHTML = `<h2>Matched automatically</h2>
					<p>Translations, hacks and fan versions are matched to the game they're based on, by what their name says: <b>[T-En]</b>, <b>(Hack)</b>, <b>Redux</b>, <b>FastROM</b> and so on. When the base game is in your library they're linked to it; when it isn't, its box art and details are looked up.</p>
					<p class="note">It runs at the end of Look up box art on this PC, or now. Every match is listed here to keep or undo.</p>
					<div class="acts"><button class="btn primary" id="amrun" data-nav ${run ? "disabled" : ""}>Match automatically</button></div>`;
				const b = $("#amrun");
				if (b) { b.onclick = () => this.run(); focus(b); }
				this.page.hints = [["A", "Choose"], ["LB/RB", "Tabs"], ["B", "Back"]];
				setHints(this.page.hints);
				return;
			}
			let last = null;
			const pending = this.items.filter(m => !m.kept).length;
			for (const m of this.items) {
				const grp = m.kept ? "kept" : m.sys;
				if (grp !== last) {
					last = grp;
					const cnt = this.items.filter(x => (x.kept ? "kept" : x.sys) === grp).length;
					box.appendChild(h(`<div class="grp"><span>${spaced(m.kept ? "Kept" : names.get(m.sys) || m.system || m.sys)}</span><span>${cnt}</span></div>`).firstChild);
				}
				const el = h(`<div class="row" data-nav><span class="n">${esc(m.title)}</span><span class="tag">${esc(m.kept ? "Kept" : AM_KIND[m.kind] || "Match")}</span></div>`).firstChild;
				el._m = m;
				el.onclick = () => { const k = $("#amkeep"); if (k) focus(k); };
				el._onfocus = () => { this.m = m; this.sel = this.items.indexOf(m); this.panel(m); };
				box.appendChild(el);
			}
			this.page.hints = [["A", "Choose"], ["X", "Undo"], ["Y", "Pick another"], ...(pending ? [["Start", "Keep all"]] : []), ["LB/RB", "Tabs"], ["B", "Back"]];
			setHints(this.page.hints);
			const rows = [...box.querySelectorAll(".row")];
			focus(rows[Math.min(this.sel, rows.length - 1)]);
		} catch (e) { box.innerHTML = `<div class="empty">${esc(e.message)}</div>`; }
	}
	panel(m) {
		const info = $("#info");
		if (m.how === "skip") {
			const when = new Date(m.at * 1000).toLocaleString(undefined, { day: "numeric", month: "short", hour: "2-digit", minute: "2-digit" });
			info.innerHTML = `<h2>${esc(m.title)}</h2><p class="note">${esc(m.system || m.sys)}</p>
				<div class="kv"><span>Why</span><span><b>${esc(m.tag)}</b> in the name: a boot file, test ROM or tool</span><span>Done</span><span>Set aside as not a game, ${esc(when)}</span></div>
				<p>It's out of Not found, and no box art is looked up for it. It still plays from the library.</p>
				<div class="acts">${m.kept ? "" : `<button class="btn primary" id="amkeep" data-nav>Keep</button>`}<button class="btn" id="amundo" data-nav>It's a game</button></div>`;
			if ($("#amkeep")) $("#amkeep").onclick = () => this.keep(m);
			$("#amundo").onclick = () => this.undo();
			return;
		}
		const lib = m.how === "library";
		const when = new Date(m.at * 1000).toLocaleString(undefined, { day: "numeric", month: "short", hour: "2-digit", minute: "2-digit" });
		const why = { translation: "a translation", hack: "a hack", fan: "a fan version", spelling: "the same game, its name written a little differently", name: "the same game under a longer or shorter name", related: "most likely a game built on it (another subtitle)", port: "a port of it from another system" }[m.kind] || "a version of another game";
		const tagTxt = m.tag ? `<b>${esc(m.tag)}</b> in the name: ${why}` : `The lists know it as ${esc(m.base_title)}: ${why}`;
		const added = [m.art ? "box art" : "", m.fields && m.fields.length ? "details" : ""].filter(Boolean).join(" and ");
		const did = lib ? `Linked, ${when}` : added ? `${added[0].toUpperCase() + added.slice(1)} added, ${when}` : `Looked up, ${when}`;
		info.innerHTML = `<h2>${esc(m.title)}</h2><p class="note">${esc(m.system || m.sys)}</p>
			<div class="sub" style="margin-top:1.2rem">${spaced("Based on")}</div>
			<div class="cand"><div class="thumb" style="background-image:url('${artURL(lib ? m.base_id : m.id)}')"></div><div><b>${esc(m.base_title)}</b><small>${lib ? "In your library · its box art and details" : "Looked up" + (m.found_by && m.found_by.length ? " on " + esc(m.found_by.join(", ")) : "") + " · its box art and details"}</small></div></div>
			<div class="kv"><span>Why</span><span>${tagTxt}</span><span>How</span><span>${lib ? "Same name with the tags taken out" : "Not in your library: looked up by the base game's name"}</span><span>Done</span><span>${esc(did)}</span></div>
			<div class="acts">${m.kept ? "" : `<button class="btn primary" id="amkeep" data-nav>Keep</button>`}<button class="btn" id="amundo" data-nav>Undo</button><button class="btn" id="ampick" data-nav>Pick another…</button></div>
			<p class="note">Undo takes out only what this match added. It won't be matched again.</p>`;
		if ($("#amkeep")) $("#amkeep").onclick = () => this.keep(m);
		$("#amundo").onclick = () => this.undo();
		$("#ampick").onclick = () => this.pick();
	}
	async run() {
		try {
			const j = await api("/api/automatch", { do: "run" });
			if (!j.ok) return toast(j.error);
			toast("Matching to base games");
			const wait = async () => {
				const sc = await api("/api/scrape").catch(() => null);
				if (!$("#nf") || this.page.auto !== this) return;
				if (sc && sc.state.running) { const e = $("#info .note"); if (e) e.textContent = sc.state.status + (sc.state.game ? ": " + sc.state.game : ""); setTimeout(wait, 1000); return; }
				if (sc) toast(`${sc.state.matched || 0} matched`);
				this.page.bar(); this.load();
			};
			setTimeout(wait, 800);
		} catch (e) { toast(e.message); }
	}
	async keep(m) {
		try { await api("/api/automatch", { do: "keep", id: m.id }); gameCache.delete(m.id); this.sel++; this.page.bar(); this.load(); } catch (e) { toast(e.message); }
	}
	async keepAll() {
		if (!this.items.some(m => !m.kept)) return;
		try { await api("/api/automatch", { do: "keep_all" }); toast("All kept"); this.page.bar(); this.load(); } catch (e) { toast(e.message); }
	}
	async undo() {
		const m = this.m;
		if (!m) return;
		try {
			const j = await api("/api/automatch", { do: "undo", id: m.id });
			if (!j.ok) return toast(j.error);
			gameCache.delete(m.id);
			toast(`${m.title}: undone, back in Not found`);
			this.page.bar(); this.load();
		} catch (e) { toast(e.message); }
	}
	pick() { const m = this.m; if (m) push(Game, m.id, 1); }
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
