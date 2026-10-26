/* Phosphor for Windows: Settings. Laid out like the console's Settings: the sections in a list on the
   left, the highlighted one's tools on the right, with tabs when a section has several (LB/RB or Q/E).
   A (Enter) goes into a section's tools, B (Esc) comes back to the list. Everything advanced lives
   here; the main screens stay the simple console view. */
"use strict";

const SET_SECTIONS = [
	{ id: "look", name: "Look", tabs: [["The TV's look", "tLook"]] },
	{ id: "boxart", name: "Box art", tabs: [["Sources & logins", "tArt"]] },
	{ grp: "SuperStation" },
	{ id: "power", name: "Power & updates", tabs: [["Power", "tPower"], ["Update Phosphor", "tPhosUpd"], ["Updates & restore points", "tGuard"], ["File sharing", "tSamba"], ["MiSTer & Linux backups", "tSysbk"]] },
	{ id: "storage", name: "Storage", tabs: [["SD card", "tSD"], ["NVMe / USB", "tExt"]] },
	{ id: "saves", name: "Saves & memory cards", tabs: [["Memory cards", "tMc"], ["Backups", "tSnaps"], ["Cloud saves", "tCloud"]] },
	{ id: "controllers", name: "Controllers", tabs: [["Controllers", "tCtrl"], ["Backups", "tCtrlBk"]] },
	{ id: "devices", name: "Devices", tabs: [["Bluetooth", "tDevBt"], ["Front ports", "tDevPorts"], ["USB", "tDevUsb"]] },
	{ id: "discs", name: "Discs", tabs: [["Discs", "tDiscs"]] },
	{ id: "ra", name: "RetroAchievements", tabs: [["Account & options", "tRa"], ["Your progress", "tRaProg"]] },
	{ id: "network", name: "Network", tabs: [["Network", "tNet"]] },
	{ id: "video", name: "Video", tabs: [["Video settings", "tVideo"]] },
	{ id: "remote", name: "Remote control", tabs: [["Remote control", "tRemote"]] },
	{ id: "advanced", name: "Advanced", tabs: [["MiSTer.ini", "tIni"], ["Scripts", "tScripts"]] },
	{ grp: "Tools" },
	{ id: "diagnostics", name: "Diagnostics", tabs: [["Dashboard", "tDash"]] },
	{ id: "sdcard", name: "SD card flash & health", tabs: [["Flash a card", "tFlash"], ["Card health", "tCardHealth"]] },
	{ grp: "This PC" },
	{ id: "notifications", name: "Notifications", tabs: [["Notifications", "tNotes"]] },
	{ id: "connection", name: "Connection", tabs: [["Connection", "tConn"]] },
	{ id: "about", name: "About", tabs: [["About", "tAbout"]] },
];
const SECS = SET_SECTIONS.filter(s => s.id);

/* "press again": a button that asks once before doing something that can't wait or be undone */
function twice(btn, label, fn) {
	const orig = btn.textContent;
	btn.onclick = async () => {
		if (btn.dataset.sure !== "1") {
			btn.dataset.sure = "1"; btn.textContent = label;
			setTimeout(() => { if (document.body.contains(btn)) { btn.dataset.sure = ""; btn.textContent = orig; } }, 4000);
			return;
		}
		btn.dataset.sure = ""; btn.textContent = orig;
		await fn();
	};
}
/* a name typed in the page (the app's window has no browser dialogs): resolves to the text, or null */
function askText(title, value) {
	return new Promise(res => {
		const w = h(`<div class="lockwin"><div class="win" style="max-width:30rem"><div><div class="sub">${spaced(title)}</div>
			<input id="at_in" style="width:100%;margin:.6rem 0" data-nav value="${esc(value || "")}">
			<div class="acts"><button class="btn primary" id="at_ok" data-nav>OK</button><button class="btn" id="at_no" data-nav>Cancel</button></div></div></div></div>`).firstChild;
		document.body.appendChild(w);
		const prev = lockwin; lockwin = w;
		const done = v => { lockwin = prev; w.remove(); res(v); };
		w._esc = () => done(null);
		$("#at_ok", w).onclick = () => done($("#at_in", w).value.trim() || null);
		$("#at_no", w).onclick = () => done(null);
		$("#at_in", w).onkeydown = e => { if (e.key === "Enter") done($("#at_in", w).value.trim() || null); };
		setTimeout(() => { $("#at_in", w).focus(); $("#at_in", w).select(); }, 30);
	});
}
const kb = k => { k = Number(k || 0); return k >= 1048576 ? (k / 1048576).toFixed(1) + " GB" : k >= 1024 ? Math.round(k / 1024) + " MB" : k + " KB"; };
const gb = k => (Number(k || 0) / 1048576).toFixed(1) + " GB";
/* sd_integrity's results, coloured like the TV: PASS green, FAIL red, WARN / INFO amber */
function resultHTML(text) {
	return esc(text || "").split("\n").map(l => {
		const c = /FAIL|MEDIA FAILURE|DIRTY|ERROR/.test(l) ? "bad" : /WARN|UNVERIFIED|SKIP|older/.test(l) ? "warn" : /PASS|clean|\bOK\b/.test(l) ? "good" : "";
		return c ? `<span class="${c}">${l}</span>` : l;
	}).join("\n");
}
function ago(sec) {
	if (!sec) return "";
	const d = new Date(sec * 1000), now = new Date();
	return d.toDateString() === now.toDateString() ? "today " + d.toLocaleTimeString([], { hour: "numeric", minute: "2-digit" }) : d.toLocaleString([], { month: "short", day: "numeric", hour: "numeric", minute: "2-digit" });
}

class AppSettings {
	/* sec: a section id ("storage") or number; tab: that section's tab */
	constructor(sec = 0, tab = 0) {
		this.si = typeof sec === "string" ? Math.max(0, SECS.findIndex(s => s.id === sec)) : Math.min(sec, SECS.length - 1);
		this.ti = tab;
		this.enter = typeof sec === "string";       /* opened for a tool: straight into it */
		this.hints = [["A", "Open"], ["B", "Back"]];
	}
	render(root) {
		root.append(h(`<div class="sub">${spaced("Settings")}</div><h1>Settings</h1>
			<div class="set"><div class="sl list" id="sl"></div><div class="stool" id="tool"><div id="tb"></div><div class="tbody" id="tbody"></div></div></div>`));
		const sl = $("#sl");
		let k = 0;
		for (const s of SET_SECTIONS) {
			if (s.grp) { sl.appendChild(h(`<div class="grp">${spaced(s.grp)}</div>`).firstChild); continue; }
			const i = k++;
			const el = h(`<div class="row sec" data-nav><span class="n">${esc(s.name)}</span><span class="v" id="sv_${s.id}"></span></div>`).firstChild;
			el._onfocus = () => {
				this.listHints();
				if (i === this.si && this.drawn === i) return;
				this.si = i; this.ti = 0;
				clearTimeout(this.dt); this.dt = setTimeout(() => this.drawTool(), 120);
			};
			el.onclick = () => this.into();
			sl.appendChild(el);
		}
		this.badges();
		const row = sl.querySelectorAll(".sec")[this.si];
		focus(row, true);
		this.drawTool(this.enter);
		this.listHints();
	}
	restore() { this.badges(); }
	listHints() { setHints(this.hints = [["A", "Open"], ["B", "Back"]]); }
	toolHints(extra = []) {
		const t = SECS[this.si].tabs.length > 1 ? [["LB/RB", "Tabs"]] : [];
		setHints(this.hints = [["A", "Choose"], ...extra, ...t, ["B", "Sections"]]);
	}
	async badges() {
		try { const p = await api("/api/ping"); const b = $("#sv_notifications"); if (b) b.textContent = p.unseen ? p.unseen + " new" : ""; } catch (e) {}
	}
	inTool() { return !!(cur && $("#tool") && $("#tool").contains(cur)); }
	/* the hints follow the focus (mouse included): the list, or a section's tools */
	focused() {
		const t = this.inTool();
		if (t === this.wasTool) return;
		this.wasTool = t;
		if (t) this.toolHints(this.extra); else this.listHints();
	}
	/* into the tools: the first thing to choose there (not the tabs) */
	into() {
		const f = [...$("#tbody").querySelectorAll("[data-nav]")].find(e => e.offsetParent !== null) || $("#tb .tab.on");
		if (f) { focus(f); this.toolHints(this.extra); }
	}
	back() {
		if (!this.inTool()) return false;
		focus($("#sl").querySelectorAll(".sec")[this.si]);
		return true;
	}
	tab(d) {
		const n = SECS[this.si].tabs.length;
		if (n < 2) return;
		this.ti = (this.ti + d + n) % n;
		this.drawTool(true);
	}
	x() { this.onX && this.inTool() && this.onX(); }
	y() { this.onY && this.inTool() ? this.onY() : push(Search); }
	drawTool(enter) {
		const s = SECS[this.si];
		this.drawn = this.si;
		$("#sl").querySelectorAll(".sec").forEach((r, i) => r.classList.toggle("sel", i === this.si));
		this.onX = this.onY = null; this.extra = [];
		const tb = $("#tb"), body = $("#tbody");
		tb.innerHTML = "";
		if (s.tabs.length > 1) tabs(tb, s.tabs.map(t => t[0]), this.ti, i => { this.ti = i; this.drawTool(true); });
		body.innerHTML = "";
		const tok = this.tok = {};
		this.alive = () => this.tok === tok && document.body.contains(body);
		clearInterval(this.poll);
		const r = this[s.tabs[this.ti][1]](body);
		const alive = this.alive;
		const after = () => { if (enter && alive()) this.into(); else if (this.inTool()) this.toolHints(this.extra); };
		if (r && r.then) r.then(after); else after();
	}
	/* refresh a tool every ms while fn says so (a check or a job running) */
	every(ms, fn) {
		clearInterval(this.poll);
		const alive = this.alive, me = setInterval(async () => { if (!alive() || !(await fn())) clearInterval(me); }, ms);
		this.poll = me;
	}

	/* ---------- Look: the TV's, shown here (the app follows it) ---------- */
	tLook(b) {
		const [r, g, bl] = rgb(ACCENTS[S.accent] || ACCENTS.amber);
		const url = Texture.tile(S.texture, [r, g, bl], 1);
		b.append(h(`<div class="list grow"><div class="sub" style="margin-top:.2rem">${spaced("Accent colour")}</div>
			<div class="swatches"><span class="sw on" style="background:${ACCENTS[S.accent] || ACCENTS.amber}"></span><span style="align-self:center;margin-left:.8rem">${esc(S.accent[0].toUpperCase() + S.accent.slice(1))}</span></div>
			<div class="sub" style="margin-top:1.4rem">${spaced("Background texture")}</div><div class="tex"><div class="tx on" style="${url ? `background-image:url(${url})` : ""}">${esc(Texture.name(S.texture))}</div></div>
			<div class="note" style="margin-top:1.2rem">Phosphor for Windows uses the TV's own look. Change it on the SS1 (Settings &gt; Look) and the app follows.</div></div>`));
	}

	/* ---------- Box art: the SS1's sources (order, on/off) and their logins ---------- */
	async tArt(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="col"><div class="sub">${spaced("Sources, asked in this order")}</div><div class="list" id="srcs"><div class="empty"><span class="spin"></span></div></div>
			<div class="acts"><button class="btn primary" id="runpc" data-nav>Look up on this PC</button><button class="btn" id="runart" data-nav>Look up on the SS1</button><button class="btn" id="amrev" data-nav hidden>Review matches</button></div>
			<div class="note" id="scr_st" style="margin-top:.6rem"></div></div>
			<div class="panel" id="acct"></div></div>`));
		/* this PC: lists and pictures fetched here, matched with the SS1's own rules, finished box art sent to
		   the SS1 (much faster); the SS1: its own run, for when the PC is off */
		const scrDraw = st => {
			const el = $("#scr_st");
			if (!el || !st) return;
			const pct = st.total ? Math.round(100 * st.done / st.total) : 0;
			el.innerHTML = st.running
				? `<b>${esc(st.status || "Looking up box art")}</b>${st.total ? ` \u00B7 ${n(st.done)} of ${n(st.total)} (${pct}%)` : ""}<br>${n(st.found)} found, ${n(st.not_found)} not found${st.later ? `, ${n(st.later)} for later` : ""}${st.matched ? `, ${n(st.matched)} matched to base games` : ""}${st.game ? `<br><span class="dim">${esc(st.system || "")} \u203A ${esc(st.game)}</span>` : ""}`
				: st.started ? `${esc(st.status || "Done")}: ${n(st.found)} found, ${n(st.not_found)} not found${st.later ? `, ${n(st.later)} for later` : ""}${st.matched ? `, ${n(st.matched)} matched to base games` : ""}` : "";
			if (st.notes && st.notes.length) el.innerHTML += `<br>${st.notes.map(esc).join("<br>")}`;
			$("#runpc").textContent = st.running ? "Stop" : "Look up on this PC";
			if (!st.running) amRev();
		};
		/* the automatic matches waiting for a look: straight to them */
		const amRev = async () => {
			let k = 0;
			try { k = (await api("/api/automatch")).matches.filter(m => !m.kept).length; } catch (e) {}
			const b = $("#amrev");
			if (!b || !alive()) return;
			b.hidden = !k;
			b.textContent = `Review ${n(k)} match${k === 1 ? "" : "es"}`;
		};
		$("#amrev").onclick = () => push(Match, 1);
		const scrPoll = async () => {
			while (alive()) {
				let j;
				try { j = await api("/api/scrape"); } catch (e) { return; }
				if (!alive()) return;
				scrDraw(j.state);
				if (!j.state.running) return;
				await new Promise(r => setTimeout(r, 1000));
			}
		};
		$("#runpc").onclick = async () => {
			try {
				const cur = await api("/api/scrape");
				if (cur.state.running) { await api("/api/scrape", { do: "stop" }); toast("Stopping"); }
				else { await api("/api/scrape", { do: "start" }); toast("Looking up box art on this PC"); }
			} catch (e) { toast(e.message); }
			scrPoll();
		};
		$("#runart").onclick = async () => { try { await ss1({ cmd: "scrape", on: true }); toast("Looking up box art on the SS1 (slower)"); } catch (e) { toast(e.message); } };
		scrPoll();
		try {
			const [src, acc] = await Promise.all([ss1({ cmd: "scrape_sources" }), ss1({ cmd: "scrape_accounts" })]);
			this.srcs = src.sources; this.acc = acc.accounts;
		} catch (e) { if (!alive()) return false; $("#srcs").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
		if (!alive()) return;
		this.extra = [["X", "Move up"], ["Y", "Move down"]];
		this.onX = () => this.moveSrc(-1); this.onY = () => this.moveSrc(1);
		this.drawSources(undefined, true);
	}
	drawSources(keep, quiet) {
		const box = $("#srcs");
		if (!box) return;
		box.innerHTML = "";
		this.srcs.forEach((x, i) => {
			const a = k => (this.acc.find(q => q.sec === x.id && q.key === k) || {}).set;
			const st = !x.on ? "Off" : x.ready ? "On" : x.id === "screenscraper" ? (!a("user") ? "Needs your login" : !a("password") ? "Needs the password" : "Needs a developer ID") : "Needs a login";
			const el = h(`<div class="row src" data-nav><span class="n"><b>${i + 1}. ${esc(x.name)}</b></span><span class="v">${st}</span></div>`).firstChild;
			el.onclick = () => { this.srcs[i].on = !this.srcs[i].on; this.saveSources(i); };
			el._onfocus = () => { this.sx = i; this.drawAccount(x); };
			box.appendChild(el);
		});
		if (!quiet) focus(box.children[Math.min(keep ?? 0, box.children.length - 1)]);
		this.drawAccount(this.srcs[keep ?? 0]);
	}
	async saveSources(sel) {
		try {
			const j = await ss1({ cmd: "scrape_sources", order: this.srcs.map(x => x.id), off: this.srcs.filter(x => !x.on).map(x => x.id) });
			this.srcs = j.sources;
		} catch (e) { toast(e.message); }
		this.drawSources(sel);
	}
	moveSrc(d) {
		if (!cur || !cur.classList.contains("src")) return;
		const i = this.sx, j = i + d;
		if (j < 0 || j >= this.srcs.length) return;
		[this.srcs[i], this.srcs[j]] = [this.srcs[j], this.srcs[i]];
		this.saveSources(j);
	}
	drawAccount(x) {
		const p = $("#acct");
		if (!p || !x) return;
		const F = {
			screenscraper: [["user", "User name", "text"], ["password", "Password", "password"], ["devid", S.ss_builtin ? "Developer ID (only if you have your own)" : "Developer ID", "text"], ["devpassword", "Developer password", "password"]],
			thegamesdb: [["apikey", "API key (free, from thegamesdb.net)", "password"]],
			igdb: [["client_id", "Client ID (Twitch developer app)", "text"], ["client_secret", "Client secret", "password"]],
		}[x.id];
		const ABOUT = {
			libretro: "No account. Box art from libretro's thumbnails, matched by each file's content (CRC or product code), then by its exact name.",
			arcadedb: "No account. Arcade games by their MAME set name.",
			launchbox: "No account. LaunchBox's games database, downloaded once (about 110 MB) and kept on the SS1.",
			thegamesdb: "Needs a free API key from thegamesdb.net.",
			screenscraper: "Needs your free screenscraper.fr login and a developer ID. Phosphor's own developer ID is on its way from ScreenScraper; until it comes, use your own if you have one (ask on the screenscraper.fr forum). Your login counts the lookups against your account (20,000 a day for a member).",
			igdb: "Needs a free Twitch developer app (dev.twitch.tv): its Client ID and secret.",
		};
		let html = `<h2>${esc(x.name)}</h2><p class="note">${esc(ABOUT[x.id] || "")}</p>`;
		if (F) {
			for (const [k, label, type] of F) {
				const a = this.acc.find(q => q.sec === x.id && q.key === k) || {};
				const val = type === "password" ? "" : a.value || "";
				const ph = type === "password" ? (a.set ? "Saved (type to change)" : "Not set") : "Not set";
				html += `<div class="field"><label for="ac_${k}">${esc(label)}</label><input id="ac_${k}" type="${type}" data-nav autocomplete="off" placeholder="${esc(ph)}" value="${esc(val)}"></div>`;
			}
			html += `<div class="acts"><button class="btn primary" id="acsave" data-nav>Save</button>${x.id === "screenscraper" ? `<button class="btn" id="accheck" data-nav>Check login</button>` : ""}</div>${x.id === "screenscraper" ? `<p class="note" id="acst"></p>` : ""}<p class="note">Saved on the SS1 (phosphor/config/scraper.ini), sent over your home network only.</p>`;
		}
		p.innerHTML = html;
		if (!F) return;
		if ($("#accheck")) $("#accheck").onclick = async () => {
			const st = $("#acst");
			st.className = "note"; st.textContent = "Asking ScreenScraper…";
			try {
				const j = await api("/api/sscheck");
				if (!j.ok) throw new Error(j.error);
				st.className = "note " + (j.works ? "good" : "bad"); st.textContent = j.message;
			} catch (e) { st.className = "note bad"; st.textContent = e.message; }
		};
		$("#acsave").onclick = async () => {
			try {
				for (const [k, , type] of F) {
					const v = $("#ac_" + k).value;
					if (type === "password" && !v) continue;        /* empty: keep what's saved */
					const j = await ss1({ cmd: "scrape_accounts", set: { sec: x.id, key: k, value: v } });
					this.acc = j.accounts;
				}
				this.srcs = (await ss1({ cmd: "scrape_sources" })).sources;
				toast("Saved on the SS1");
				this.drawSources(this.sx);
			} catch (e) { toast(e.message); }
		};
	}

	/* ---------- Power: restart, shut down, reload the TV screen ---------- */
	tPower(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list">
			<div class="card2"><div><b>Restart</b><small>The drives are marked clean first, then the SS1 starts again. A game that's running is closed.</small></div><button class="btn primary" id="p_reboot" data-nav>Restart</button></div>
			<div class="card2"><div><b>Shut down safely</b><small>Writes everything to the drives and marks them clean, so Windows won't ask to scan the card. The TV says when it's safe to switch off.</small></div><button class="btn" id="p_off" data-nav>Shut down</button></div>
			<div class="card2"><div><b>Reload the TV screen</b><small>Restarts Phosphor's screen on the TV, not the SS1: for when the screen looks stuck. A running game keeps running.</small></div><button class="btn" id="p_ui" data-nav>Reload</button></div>
			</div><div class="panel"><h2>Power</h2><p class="note">The same as Select + Start on the SS1: modules stop, everything is written, and the drives are made read-only so they're marked clean. On the SS1 itself, press Select + Start together anywhere in Phosphor for this window.</p></div></div>`));
		const go = async (what, msg) => {
			try { await ss1({ cmd: "system", do: what }); toast(msg); setTimeout(refreshState, 4000); } catch (e) { toast(e.message); }
		};
		twice($("#p_reboot"), "Press again to restart", () => go("reboot", "Restarting the SS1…"));
		twice($("#p_off"), "Press again to shut down", () => go("shutdown", "Shutting down: the TV says when it's safe to switch off"));
		twice($("#p_ui"), "Press again to reload", () => go("reload_ui", "Reloading the TV screen…"));
	}

	/* ---------- Update Phosphor: a release package from this PC (or found on the SS1), and roll back ---------- */
	async tPhosUpd(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="upl"><div class="empty"><span class="spin"></span></div></div>
			<div class="panel"><h2>Update Phosphor</h2><p class="note" id="upv"></p>
			<div class="acts"><button class="btn primary" data-nav id="up_pick">Send a package from this PC…</button><input type="file" id="up_file" accept=".gz" hidden></div>
			<p class="note">A release package is phosphor-&lt;version&gt;-armhf.tar.gz. It's checked, the installed version is backed up, and the SuperStation restarts by itself to finish, with your library, box art and settings as they were. MiSTer's own files aren't touched.</p>
			<p class="note" id="upst"></p></div></div>`));
		const draw = async () => {
			let s;
			try { s = await ss1({ cmd: "update" }); } catch (e) { if (!alive()) return false; $("#upl").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return false; }
			if (!alive()) return false;
			if (!s.available) { $("#upl").innerHTML = `<div class="empty">This SS1's Phosphor is older than the updater: install 0.27.6 once with the update script, then updates work from here.</div>`; return false; }
			$("#upv").textContent = "Installed on the SS1: Phosphor " + (s.version || "?");
			const st = s.state || {}, busy = st.state === "working";
			$("#upst").innerHTML = st.msg && (busy || Date.now() / 1000 - (st.at || 0) < 600) ? `<span class="${st.state === "failed" ? "bad" : busy ? "" : "good"}">${busy ? `<span class="spin"></span> ` : ""}${esc(st.msg)}</span>` : "";
			const l = $("#upl");
			l.innerHTML = "";
			const rows = [...(s.packages || []).map(p => ({ t: "Install Phosphor " + p.version, v: p.file.includes("/media/usb") ? "USB drive" : "On the SS1", q: { do: "install", file: p.file } })),
				...(s.rollbacks || []).filter(r => r.version !== s.version).map(r => ({ t: "Roll back to Phosphor " + r.version, v: r.date, q: { do: "rollback", version: r.version } }))];
			if (!rows.length) l.innerHTML = `<div class="empty">No packages on the SS1 yet, and no earlier version kept. Send one from this PC.</div>`;
			for (const x of rows) {
				const el = h(`<div class="row" data-nav><span class="n">${esc(x.t)}</span><span class="v">${esc(x.v)}</span></div>`).firstChild;
				const v = $(".v", el);
				el.onclick = async () => {
					if (busy) return toast("An update is running");
					if (el.dataset.sure !== "1") { el.dataset.sure = "1"; v.textContent = "Press again"; setTimeout(() => { el.dataset.sure = ""; v.textContent = x.v; }, 4000); return; }
					el.dataset.sure = "";
					try { await ss1(Object.assign({ cmd: "update" }, x.q)); toast("Updating: the TV closes Phosphor and opens it again"); } catch (e) { toast(e.message); }
					setTimeout(draw, 1500);
				};
				l.appendChild(el);
			}
			return busy;
		};
		const busy = await draw();
		$("#up_pick").onclick = () => $("#up_file").click();
		$("#up_file").onchange = async () => {
			const f = $("#up_file").files[0];
			if (!f) return;
			if (!/^phosphor-.+-armhf\.tar\.gz$/.test(f.name)) return toast("That isn't a Phosphor package (phosphor-<version>-armhf.tar.gz)");
			toast("Sending " + f.name + "…");
			try {
				const r = await fetch("/api/upload?name=" + encodeURIComponent(f.name), { method: "POST", headers: { "X-Phosphor-Key": KEY }, body: await f.arrayBuffer() });
				const j = await r.json();
				if (!j.ok) throw new Error(j.error || "the SS1 didn't take it");
				toast("Sent. Choose Install " + f.name.replace(/^phosphor-|-armhf\.tar\.gz$/g, "") + " to update");
			} catch (e) { toast(e.message); }
			$("#up_file").value = "";
			draw();
		};
		this.every(2000, async () => { await draw(); return true; });
		return busy;
	}

	/* ---------- Updates & restore points (update-guard) ---------- */
	async tGuard(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="gl"><div class="empty"><span class="spin"></span></div></div><div class="panel" id="gp"></div></div>`));
		const draw = async () => {
			let g;
			try { g = await ss1({ cmd: "guard" }); } catch (e) { if (!alive()) return false; $("#gl").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return false; }
			if (!alive()) return false;
			const gl = $("#gl"), gp = $("#gp");
			if (!g.installed) { gl.innerHTML = `<div class="empty">The update-guard module isn't installed on the SS1.</div>`; gp.innerHTML = ""; return false; }
			const reset = (g.files || []).filter(f => f.reset);
			const keepSel = cur && gl.contains(cur) ? [...gl.querySelectorAll("[data-nav]")].indexOf(cur) : -1;
			gl.innerHTML = "";
			gl.append(h(`<dl class="kv"><dt>MiSTer Linux</dt><dd>${esc(g.linux || "–")}</dd>${g.updated_at ? `<dt>Last update</dt><dd>${esc(g.updated_at)}${g.updated_from ? " (from " + esc(g.updated_from) + ")" : ""}</dd>` : ""}</dl>`));
			if (g.awaiting && reset.length) {
				gl.append(h(`<div class="sub" style="margin-top:1rem">${spaced("Reset by the last update")}</div>`));
				for (const f of reset) gl.append(h(`<div class="row"><span class="n">${esc(f.path)}</span></div>`));
				gl.append(h(`<div class="acts"><button class="btn primary" id="g_restore" data-nav>Put my settings back</button><button class="btn" id="g_keep" data-nav>Keep the new ones</button></div>`));
			}
			gl.append(h(`<div class="sub" style="margin-top:1rem">${spaced("Restore points")}</div>`));
			const pts = g.points || [];
			if (!pts.length) gl.append(h(`<div class="empty">None yet. One is saved before every update.</div>`));
			pts.forEach(p => {
				const el = h(`<div class="row" data-nav><span class="n">${esc(p.label || p.name)}</span><span class="v">${esc(p.created || "")}${p.complete ? "" : " · partial"}</span></div>`).firstChild;
				el._onfocus = () => {
					gp.innerHTML = `<h2>${esc(p.label || p.name)}</h2><dl class="kv"><dt>Saved</dt><dd>${esc(p.created)}</dd><dt>MiSTer Linux</dt><dd>${esc(p.linux || "–")}</dd></dl>
						<p class="note">Rolling back puts MiSTer's system files as they were then (it may download that Linux release). Your games, saves and Phosphor settings aren't touched.</p>
						<div class="acts"><button class="btn" id="g_rb" data-nav>Roll back to this</button></div>`;
					twice($("#g_rb"), "Press again to roll back", async () => { try { await ss1({ cmd: "guard", do: "rollback", items: [p.name] }); toast("Rolling back…"); draw(); this.every(3000, draw); } catch (e) { toast(e.message); } });
				};
				gl.appendChild(el);
			});
			if (g.job && g.job.name) gl.append(h(`<div class="sub" style="margin-top:1rem">${spaced(g.job.busy ? "Working…" : "Last job: " + g.job.name)}</div><pre class="out">${resultHTML(g.job.output)}</pre>`));
			const rs = $("#g_restore");
			if (rs) rs.onclick = async () => {
				const items = reset.map(f => f.path).filter(p => !/dhcpcd|hostname|\/etc\/hosts|network|wpa_/.test(p));
				if (!items.length) { toast("Only network settings were reset: put them back on the SS1 (Settings > Updates)"); return; }
				try { await ss1({ cmd: "guard", do: "restore", items }); toast(items.length < reset.length ? "Putting your settings back (network ones only on the SS1 itself)" : "Putting your settings back"); this.every(2000, draw); } catch (e) { toast(e.message); }
			};
			const kp = $("#g_keep");
			if (kp) twice(kp, "Press again to keep the new ones", async () => { try { await ss1({ cmd: "guard", do: "keep" }); draw(); } catch (e) { toast(e.message); } });
			if (!gp.innerHTML) gp.innerHTML = `<h2>Updates & restore points</h2><p class="note">Before every update Phosphor saves a restore point of MiSTer's system files. If an update resets your own settings (SSH keys, Samba and so on), they can be put back here; a whole update can be rolled back.</p>`;
			if (keepSel >= 0) focus(gl.querySelectorAll("[data-nav]")[keepSel] || gl.querySelector("[data-nav]"));
			return !!(g.job && g.job.busy);
		};
		if (await draw()) this.every(2000, draw);
	}

	/* ---------- File sharing (MiSTer's Samba) ---------- */
	async tSamba(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="sm"><div class="empty"><span class="spin"></span></div></div>
			<div class="panel"><h2>File sharing</h2><p class="note">MiSTer's own Samba file sharing: the SS1's SD card (and drives) show up on this PC's network as <b>\\\\${esc((S.address || "").replace(/:\d+$/, ""))}</b>. Anyone on your home network can open it, so turn it off when you don't need it.</p></div></div>`));
		const draw = async () => {
			let s;
			try { s = await ss1({ cmd: "samba" }); } catch (e) { if (!alive()) return false; $("#sm").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return false; }
			if (!alive()) return false;
			const sm = $("#sm"), had = cur && sm.contains(cur) ? cur.id : "";
			if (!s.available) { sm.innerHTML = `<div class="empty">MiSTer's Samba isn't on this SD card.</div>`; return false; }
			const state = s.busy ? (s.want ? "Starting…" : "Stopping…") : s.running ? "On" : "Off";
			sm.innerHTML = `<dl class="kv"><dt>File sharing</dt><dd>${state}</dd><dt>At start-up</dt><dd>${s.on ? "starts with the SS1" : "off"}</dd>${s.error ? `<dt>Problem</dt><dd class="bad">${esc(s.error)}</dd>` : ""}</dl>
				<div class="acts"><button class="btn primary" id="sm_t" data-nav>${s.on || s.running ? "Turn off" : "Turn on"}</button><button class="btn" id="sm_x" data-nav${s.running ? "" : " disabled"}>Open in File Explorer</button></div>`;
			$("#sm_t").onclick = async () => { try { await ss1({ cmd: "samba", on: !(s.on || s.running) }); draw(); this.every(1500, draw); } catch (e) { toast(e.message); } };
			$("#sm_x").onclick = async () => { const j = await api("/api/explore", {}); if (!j.ok) toast(j.error); };
			if (had && $("#" + had)) focus($("#" + had));
			return s.busy;
		};
		if (await draw()) this.every(1500, draw);
	}

	/* ---------- Discs: the dock's CD/DVD drive (as Settings › Discs on the TV) ---------- */
	async tDiscs(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="dc"><div class="empty"><span class="spin"></span></div></div>
			<div class="panel"><h2>Discs</h2><p class="note">Play games from the dock's CD/DVD drive: PlayStation, Saturn, Mega-CD, PC Engine CD, Neo Geo CD, CD-i and 3DO, and MSU-1 / MD+ discs. Off: Phosphor doesn't use the drive.</p>
			<p class="note">A disc that's in shows at the top of Library › Disc, here and on the TV.</p><p class="note" style="color:var(--faint)">Disc support by Anime0t4ku and bonna79.</p></div></div>`));
		const draw = async () => {
			let d;
			try { d = (await ss1({ cmd: "disc" })).disc; } catch (e) { if (!alive()) return false; $("#dc").innerHTML = `<div class="empty">${esc(/unknown|no such|only works/i.test(e.message) ? "This SS1's Phosphor doesn't have disc support yet: update Phosphor." : e.message)}</div>`; return false; }
			if (!alive()) return false;
			const dc = $("#dc"), had = cur && dc.contains(cur) ? cur.id : "";
			if (!d) { dc.innerHTML = `<div class="empty">This SS1's Main doesn't have disc support (Phosphor's Main is needed).</div>`; return false; }
			const drive = !d.enabled ? "not used" : (d.state === "none" || !d.state) ? "no drive found" : d.state === "empty" ? "empty" : discLine(d);
			const canEject = d.enabled && d.state !== "none" && d.state;
			dc.innerHTML = `<dl class="kv"><dt>Discs</dt><dd>${d.enabled ? "On" : "Off"}</dd><dt>Drive</dt><dd>${esc(drive)}</dd>${discIn(d) && d.system ? `<dt>System</dt><dd>${esc(d.system)}${d.serial ? " · " + esc(d.serial) : ""}</dd>` : ""}${discIn(d) && !d.playable && d.reason ? `<dt>Note</dt><dd class="warn">${esc(d.reason)}</dd>` : ""}</dl>
				<div class="acts"><button class="btn primary" id="dc_t" data-nav>${d.enabled ? "Turn off" : "Turn on"}</button><button class="btn" id="dc_e" data-nav${canEject ? "" : " disabled"}>Eject the disc</button></div>`;
			$("#dc_t").onclick = async () => { try { await ss1({ cmd: "disc_enable", on: !d.enabled }); toast(d.enabled ? "Discs off: the drive isn't used" : "Discs on"); draw(); } catch (e) { toast(e.message); } };
			$("#dc_e").onclick = async () => { await discEject(); draw(); };
			if (had && $("#" + had)) focus($("#" + had));
			return true;
		};
		if (await draw()) this.every(2000, async () => { await draw(); return true; });
	}

	/* ---------- RetroAchievements: odelot's RA Main and cores; the login is on the TV ---------- */
	async tRa(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="rac"><div class="empty"><span class="spin"></span></div></div>
			<div class="panel"><h2>RetroAchievements</h2><p class="note">Earn achievements while you play: NES, SNES, Game Boy, GBA, N64, PlayStation, Mega Drive, Mega-CD, Master System, Game Gear, PC Engine and Atari 7800. They run inside a game, through RetroAchievements-enabled cores.</p>
			<p class="note">Log in on the TV (Settings › RetroAchievements). Only a login token is kept on the SS1, never your password.</p>
			<p class="note" style="color:var(--faint)">RetroAchievements support: odelot's MiSTer RA Main and cores, and the RetroAchievements rcheevos library. Achievements and badges from retroachievements.org.</p></div></div>`));
		const yes = v => (v ? "On" : "Off");
		const draw = async () => {
			let r;
			try { r = (await ss1({ cmd: "ra" })).ra; } catch (e) { if (!alive()) return false; $("#rac").innerHTML = `<div class="empty">${esc(/unknown|no such/i.test(e.message) ? "This SS1's Phosphor doesn't have RetroAchievements yet: update Phosphor." : e.message)}</div>`; return false; }
			if (!alive()) return false;
			const box = $("#rac"), had = cur && box.contains(cur) ? cur.id : "";
			const busy = r.job.busy || r.match.busy;
			box.innerHTML = `<dl class="kv"><dt>Account</dt><dd>${r.logged_in ? esc(r.user) : r.user ? `${esc(r.user)} <span class="warn">(not logged in)</span>` : "Not logged in: log in on the TV"}</dd>
				<dt>Cores</dt><dd>${r.cores_installed} of ${r.cores.length} installed</dd>
				<dt>Web API key</dt><dd>${r.web_key ? "Set" : "Not set (needed for Your progress and the library's badges)"}</dd></dl>
				<div class="acts"><button class="btn" id="ra_hc" data-nav>Hardcore: ${yes(r.hardcore)}</button><button class="btn" id="ra_pp" data-nav>Pop-ups: ${esc(r.popups)}</button><button class="btn" id="ra_sd" data-nav>Unlock sound: ${yes(r.sound)}</button></div>
				<div class="acts"><button class="btn primary" id="ra_gc" data-nav${busy ? " disabled" : ""}>${r.cores_installed ? "Update the cores" : "Get the cores"}</button><button class="btn" id="ra_mt" data-nav${busy || !r.web_key ? " disabled" : ""}>Match the library</button><button class="btn" id="ra_key" data-nav>${r.web_key ? "Change the Web API key" : "Set the Web API key"}</button></div>
				${r.job.message || r.match.message ? `<p class="note">${busy ? `<span class="spin"></span>` : ""}${esc([r.job.message, r.match.message].filter(Boolean).join(" · "))}</p>` : ""}
				<p class="note">Hardcore: no save states, rewind or cheats, and the achievements count double on your profile. Your Web API key is on retroachievements.org › Settings › Keys.</p>`;
			const set = async o => { try { await ss1(Object.assign({ cmd: "ra_set" }, o)); } catch (e) { toast(e.message); } draw(); };
			$("#ra_hc").onclick = () => set({ hardcore: !r.hardcore });
			$("#ra_pp").onclick = () => set({ popups: { left: "center", center: "right", right: "left" }[r.popups] || "left" });
			$("#ra_sd").onclick = () => set({ sound: !r.sound });
			$("#ra_gc").onclick = async () => { try { await ss1({ cmd: "ra_cores" }); toast("Getting the cores from GitHub"); } catch (e) { toast(e.message); } draw(); };
			$("#ra_mt").onclick = async () => { try { await ss1({ cmd: "ra_match" }); toast("Looking up the library on RetroAchievements"); } catch (e) { toast(e.message); } draw(); };
			$("#ra_key").onclick = async () => {
				const k = await askText("Your RetroAchievements Web API key (empty: remove it)", "");
				if (k === null && !r.web_key) return;
				try { await ss1({ cmd: "ra_key", key: k || "" }); toast(k ? "Web API key saved on the SS1" : "Web API key removed"); } catch (e) { toast(e.message); }
				draw();
			};
			if (had && $("#" + had)) focus($("#" + had));
			return true;
		};
		if (await draw()) this.every(2000, async () => { await draw(); return true; });
	}
	async tRaProg(b) {
		const alive = this.alive;
		b.append(h(`<div class="list grow" id="rap"><div class="empty"><span class="spin"></span>Asking RetroAchievements…</div></div>`));
		let p;
		try { p = (await ss1({ cmd: "ra_profile" })).profile; } catch (e) { if (alive()) $("#rap").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
		if (!alive()) return;
		const box = $("#rap");
		box.innerHTML = `<div style="display:flex;gap:1rem;align-items:center;margin-bottom:.8rem">${p.picture ? `<img src="${esc(p.picture)}" alt="" style="width:3.5rem;height:3.5rem;border-radius:6px">` : ""}
			<div><h2 style="margin:0">${esc(p.user)}</h2><p class="note" style="margin:.2rem 0 0">${n(p.points)} points${p.softcore_points ? ` · ${n(p.softcore_points)} softcore` : ""}${p.rank ? ` · rank ${n(p.rank)}` : ""} · ${n(p.achievements)} achievements · ${n(p.mastered)} mastered</p></div></div>`;
		if (!(p.games || []).length) { box.append(h(`<div class="empty">No games played with achievements yet.</div>`)); return; }
		p.games.forEach(g => {
			const pct = g.total ? Math.round(g.unlocked * 100 / g.total) : 0;
			const el = h(`<div class="row" data-nav style="gap:.8rem">${g.icon ? `<img src="${esc(g.icon)}" alt="" style="width:2.2rem;height:2.2rem;border-radius:4px;flex:none">` : ""}
				<span class="n">${esc(g.title)}<small class="note" style="display:block">${esc(g.console)}${g.award ? ` · ${esc(g.award)}` : ""}${g.game_id ? "" : " · not in your library"}</small></span>
				<span class="v">${g.unlocked} / ${g.total} · ${pct}%</span></div>`).firstChild;
			if (g.game_id) el.onclick = () => push(Game, g.game_id);
			box.appendChild(el);
		});
	}

	/* ---------- Storage: SD card, NVMe / USB (sd_integrity checks), backups ---------- */
	async storageState() {
		const s = await ss1({ cmd: "storage" });
		this.st = s;
		return s;
	}
	checksFor(d) {
		const C = [["quick", "Quick check", "Partitions, file system and the error log."], ["windows", "Will Windows complain?", "Whether the drive is marked dirty (Windows offers to fix it when it's plugged into a PC)."],
			["speed", "Speed test", "Reads and writes one temporary file."], ["fscheck", "Check the file system", "Unmounts the drive and checks it, read only. Games on it stop while it runs."]];
		if (d.kind === "nvme") C.splice(2, 0, ["health", "Drive health", "The NVMe drive's own health report (wear, errors, temperature)."]);
		return C;
	}
	driveTool(box, d, s) {
		const L = s.last || {}, mine = L.target === d.path && L.job;
		const busy = L.busy;
		if (d.mounted === false) {
			box.innerHTML = `<dl class="kv"><dt>Drive</dt><dd>${esc((d.kind === "nvme" ? "NVMe" : "USB") + (d.model ? " · " + d.model : ""))}</dd><dt>Size</dt><dd>${gb(d.size_kb)}</dd><dt>File system</dt><dd>${esc(d.fs)}</dd></dl>
				<p class="note warn">Not mounted: MiSTer can't see the games on it right now. This happens when a check or a hiccup leaves it unmounted.</p>
				<div class="acts"><button class="btn primary" id="ck_mnt" data-nav${busy ? " disabled" : ""}>Mount it again</button></div>
				${mine ? `<pre class="out">${busy ? `<span class="spin"></span>` : ""}${resultHTML(L.output)}</pre>` : ""}`;
			$("#ck_mnt", box).onclick = async () => {
				try {
					await ss1({ cmd: "storage", do: "mount", target: d.path });
					toast("Mounting…");
					this.every(1500, async () => { const t = await this.storageState(); if (t.last && t.last.busy) return true; this.drawTool(); return false; });
				} catch (e) { toast(e.message); }
			};
			return;
		}
		const warnGen = d.gen === "gen4" ? `<p class="note bad">Gen 4 NVMe drive: these cause boot and file system trouble in the SS1 dock. A Gen 3 drive is the safe choice.</p>` : "";
		const watts = d.watts && Number(d.watts) > 7 ? `<p class="note warn">Rated at ${esc(d.watts)} W: more than the dock likes to give. If the SS1 restarts under load, this drive is a likely cause.</p>` : "";
		box.innerHTML = `<dl class="kv"><dt>Drive</dt><dd>${esc(d.kind === "sd" ? "SD card" : (d.kind === "nvme" ? "NVMe" : "USB") + (d.model ? " · " + d.model : ""))}</dd><dt>Space</dt><dd>${gb(d.free_kb)} free of ${gb(d.size_kb)}</dd><dt>File system</dt><dd>${esc(d.fs)}</dd>${d.kind === "nvme" ? `<dt>PCIe</dt><dd>${esc({ gen4: "Gen 4", gen3: "Gen 3" }[d.gen] || "not known")}</dd>` : ""}</dl>${warnGen}${watts}
			<div class="checks" id="cks"></div>
			<div class="acts"><button class="btn" id="ck_fix" data-nav${busy ? " disabled" : ""}>Repair the file system…</button></div>
			${mine ? `<div class="sub" style="margin-top:1rem">${spaced(busy ? "Checking: " + L.job : "Last: " + L.job + " · " + ago(L.at))}</div><pre class="out">${busy ? `<span class="spin"></span>` : ""}${resultHTML(L.output)}</pre>` : ""}`;
		for (const [t, label, what] of this.checksFor(d)) {
			const el = h(`<button class="btn chk" data-nav title="${esc(what)}"${busy ? " disabled" : ""}>${esc(label)}</button>`).firstChild;
			el.onclick = () => this.runCheck(t, d.path);
			$("#cks", box).appendChild(el);
		}
		twice($("#ck_fix", box), "Press again: it unmounts and repairs", () => this.runCheck("fsrepair", d.path));
	}
	async runCheck(test, target) {
		try { await ss1({ cmd: "storage", do: "check", test, target }); toast("Checking…"); this.drawTool(); this.every(2000, async () => { const s = await this.storageState(); this.redrawStorage(s); return s.last && s.last.busy; }); }
		catch (e) { toast(e.message); }
	}
	redrawStorage(s) {
		if (!this.alive()) return;
		const keep = cur && cur.id;
		if (this.ti === 0 && $("#sdbox")) { const d = (s.drives || []).find(x => x.kind === "sd"); if (d) this.driveTool($("#sdbox"), d, s); this.overlap(s); }
		if (this.ti === 1 && $("#exbox") && this.exSel) { const d = (s.drives || []).find(x => x.path === this.exSel); if (d) this.driveTool($("#exbox"), d, s); }
		if (keep && $("#" + keep)) focus($("#" + keep), true);
	}
	overlap(s) {
		const o = $("#ovl");
		if (!o) return;
		const f = s.overlap_tool || [];
		o.innerHTML = f.length ? `<p class="note bad"><b>Faulty overlap "fix" tool found</b> (${f.map(x => esc(x.split("/").pop())).join(", ")}): it misreads the SS1's correct card layout and can destroy the card.</p><div class="acts"><button class="btn primary" id="ov_rm" data-nav>Remove it</button></div>`
			: `<p class="note">No faulty overlap "fix" tool on the card.${this.ovUndo ? ` <button class="btn" id="ov_un" data-nav>Put it back</button>` : ""}</p>`;
		const rm = $("#ov_rm");
		if (rm) rm.onclick = async () => { try { await ss1({ cmd: "storage", do: "overlap-remove" }); this.ovUndo = true; toast("Moved into phosphor/backups/overlap-tool"); setTimeout(async () => this.overlap(await this.storageState()), 1500); } catch (e) { toast(e.message); } };
		const un = $("#ov_un");
		if (un) un.onclick = async () => { try { await ss1({ cmd: "storage", do: "overlap-undo" }); this.ovUndo = false; setTimeout(async () => this.overlap(await this.storageState()), 1500); } catch (e) { toast(e.message); } };
	}
	async tSD(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="sdbox"><div class="empty"><span class="spin"></span></div></div>
			<div class="panel"><h2>SD card</h2><p class="note">The checks only read, except the speed test (one temporary file) and Repair, which asks first. Results also go to the SS1's /media/fat/sd_integrity.log.</p><div id="ovl"></div></div></div>`));
		let s;
		try { s = await this.storageState(); } catch (e) { if (!alive()) return false; $("#sdbox").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
		if (!alive()) return;
		if (!s.installed) { $("#sdbox").innerHTML = `<div class="empty">The storage module isn't on the SS1 yet (it comes with Phosphor 0.27).</div>`; return; }
		const d = (s.drives || []).find(x => x.kind === "sd");
		if (!d) { $("#sdbox").innerHTML = `<div class="empty">No SD card found.</div>`; return; }
		this.driveTool($("#sdbox"), d, s);
		this.overlap(s);
		if (s.last && s.last.busy) this.every(2000, async () => { const t = await this.storageState(); this.redrawStorage(t); return t.last && t.last.busy; });
	}
	async tExt(b) {
		const alive = this.alive;
		b.append(h(`<div class="split" style="grid-template-columns:18rem 1fr"><div class="list" id="exl"><div class="empty"><span class="spin"></span></div></div><div class="list" id="exbox"></div></div>`));
		let s;
		try { s = await this.storageState(); } catch (e) { if (!alive()) return false; $("#exl").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
		if (!alive()) return;
		const ds = (s.drives || []).filter(x => x.kind !== "sd");
		const l = $("#exl");
		if (!s.installed) { l.innerHTML = `<div class="empty">The storage module isn't on the SS1 yet.</div>`; return; }
		if (!ds.length) { l.innerHTML = `<div class="empty">No NVMe or USB drive is plugged in.</div>`; return; }
		l.innerHTML = "";
		for (const d of ds) {
			const el = h(`<div class="row" data-nav><span class="n"><b>${esc(d.kind === "nvme" ? "NVMe" : "USB")}${d.gen === "gen4" || d.mounted === false ? " ⚠" : ""}</b><small>${esc(d.mounted === false ? "Not mounted" : d.model || d.path)} · ${gb(d.size_kb)}</small></span></div>`).firstChild;
			el.classList.add("net");
			el._onfocus = () => { this.exSel = d.path; this.driveTool($("#exbox"), d, this.st || s); };
			l.appendChild(el);
		}
		this.exSel = ds[0].path;
		this.driveTool($("#exbox"), ds[0], s);
		if (s.last && s.last.busy) this.every(2000, async () => { const t = await this.storageState(); this.redrawStorage(t); return t.last && t.last.busy; });
	}
	/* snapshots of saves and settings (backup-agent) on the SD card, NVMe or NAS */
	async tSnaps(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="bk"><div class="empty"><span class="spin"></span></div></div><div class="panel" id="bkp"></div></div>`));
		const draw = async () => {
			let s;
			try { s = await ss1({ cmd: "backup" }); } catch (e) { if (!alive()) return false; $("#bk").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return false; }
			if (!alive()) return false;
			const bk = $("#bk"), bp = $("#bkp");
			if (!s.installed) { bk.innerHTML = `<div class="empty">The backup-agent module isn't installed on the SS1.</div>`; bp.innerHTML = ""; return false; }
			const st = s.status || {}, busy = s.job && s.job.busy;
			bk.innerHTML = `<dl class="kv"><dt>Kept on</dt><dd>${esc(st.dest || st.target || "–")}</dd>${st.last ? `<dt>Last backup</dt><dd>${esc(st.last)}</dd>` : ""}</dl>
				<div class="acts"><button class="btn primary" id="bk_now" data-nav${busy ? " disabled" : ""}>${busy ? "Working…" : "Back up now"}</button></div>
				<div class="sub" style="margin-top:1rem">${spaced("Snapshots")}</div>`;
			const snaps = s.snapshots || [];
			if (!snaps.length) bk.append(h(`<div class="empty">No snapshots yet.</div>`));
			snaps.forEach(x => {
				const el = h(`<div class="row" data-nav><span class="n">${esc(x.when || x.name)}</span><span class="v">${esc(x.reason || "")} · ${kb(x.kb)}</span></div>`).firstChild;
				el._onfocus = () => {
					bp.innerHTML = `<h2>${esc(x.when || x.name)}</h2><p class="note">${esc(x.reason || "")} · ${kb(x.kb)}</p><p class="note">From here only the saves and save states can be put back; a whole snapshot (with settings) is restored on the SS1 itself.</p>
						<div class="acts"><button class="btn" id="bk_rs" data-nav>Put these saves back</button></div>`;
					twice($("#bk_rs"), "Press again: today's saves are kept in a snapshot first", async () => {
						try { await ss1({ cmd: "backup", do: "restore", name: x.name, only: "saves|savestates" }); toast("Putting the saves back…"); draw(); this.every(2000, draw); } catch (e) { toast(e.message); }
					});
				};
				bk.appendChild(el);
			});
			if (s.job && s.job.name && s.job.output) bk.append(h(`<pre class="out">${esc(s.job.output)}</pre>`));
			$("#bk_now").onclick = async () => { try { await ss1({ cmd: "backup", do: "now" }); toast("Backing up…"); draw(); this.every(2000, draw); } catch (e) { toast(e.message); } };
			if (!bp.innerHTML) bp.innerHTML = `<h2>Backups</h2><p class="note">Snapshots of your saves, save states and settings, made by the SS1 itself on the SD card, the NVMe drive or your NAS. No PC needed.</p>`;
			return busy;
		};
		if (await draw()) this.every(2000, draw);
	}

	/* ---------- Memory cards: SS1 Tool's Files > Memory cards, through the SS1's own front ports.
	   The card in a port is backed up into the library on the SS1 (phosphor/memcards, the same layout
	   as SS1 Tool's); saves move between library cards (each change a new version, nothing
	   overwritten); a card is written only from the library, always after a fresh backup. ---------- */
	async tMc(b) {
		const alive = this.alive, me = this;
		const MCCOL = ["#343a50", "#38c058", "#e8c030", "#e84040", "#d070e8", "#4a9cf0"];
		const MCLEG = [[1, "read OK"], [2, "needed a retry"], [3, "failed"], [5, "written and checked"], [0, "not done yet"]];
		const verName = v => { const m = /^(\d{4})-(\d\d)-(\d\d)_(\d\d)(\d\d)(\d\d)/.exec(v || ""); return m ? new Date(+m[1], m[2] - 1, +m[3], +m[4], +m[5], +m[6]).toLocaleString() : (v || ""); };
		const icon = d => {
			if (!d) return "";
			const bin = atob(d), c = document.createElement("canvas"); c.width = c.height = 16;
			const g = c.getContext("2d"), im = g.createImageData(16, 16);
			for (let i = 0; i < bin.length && i < 1024; i++) im.data[i] = bin.charCodeAt(i);
			g.putImageData(im, 0, 0);
			return `<img src="${c.toDataURL()}" width="32" height="32" style="image-rendering:pixelated" alt="">`;
		};
		const M = this.mc || (this.mc = { sel: "", ver: "", port: 1 });
		b.append(h(`<div class="mcw list grow">
			<p class="note">Back up the PlayStation memory cards in the SuperStation's front ports, keep every card in a library on the SS1 (phosphor/memcards, the same as SS1 Tool's), move saves between cards, and copy saves back onto a real card. Writing to a card is only done from the library, and always after a fresh backup.</p>
			<div class="mcard"><h3>The card in front port <select id="mc_port" data-nav><option value="1">1</option><option value="2">2</option></select></h3>
				<div class="mrow"><label for="mc_as">Keep the backup as</label><select id="mc_as" data-nav></select><input id="mc_new" placeholder="Card name" style="display:none;width:12rem" data-nav>
				<button class="btn primary" id="mc_backup" data-nav>Back up the card</button></div>
				<div id="mc_job" style="display:none;margin-top:12px"><div class="mrow" style="align-items:flex-start;gap:16px"><canvas id="mc_map" width="256" height="256" style="width:256px;height:256px;image-rendering:pixelated;border-radius:4px"></canvas>
				<div style="flex:1;min-width:220px"><b id="mc_title"></b><div id="mc_step" style="margin:4px 0"></div><div id="mc_leg" class="muted" style="margin-top:8px;font-size:12px"></div><div id="mc_msg" style="margin-top:8px"></div>
				<div id="mc_ask" style="display:none;margin-top:8px"><label for="mc_askname">A card Phosphor hasn't seen before: name it</label> <input id="mc_askname" style="width:12rem" data-nav> <button class="btn" id="mc_askok" data-nav>Keep it</button></div></div></div></div></div>
			<div class="mcard"><h3>Library</h3>
				<div class="mrow" style="margin-bottom:8px"><button class="btn" id="mc_newcard" data-nav>New blank card</button><span class="muted">One folder per card, one file per version (raw 128 KB .mcd, which PC emulators open too). Nothing is ever overwritten.</span></div>
				<div id="mc_lib"><span class="muted">Loading…</span></div></div>
			<div class="mcard" id="mc_box" style="display:none"><h3 id="mc_name"></h3>
				<div class="mrow" style="margin-bottom:8px"><label for="mc_ver">Version</label><select id="mc_ver" data-nav></select><span class="muted" id="mc_info"></span></div>
				<div id="mc_saves"></div>
				<div class="mrow" style="margin-top:10px"><label for="mc_to">Ticked saves:</label><select id="mc_to" data-nav></select><button class="btn" id="mc_copy" data-nav>Copy to that card</button><button class="btn" id="mc_move" data-nav>Move to that card</button><button class="btn danger" id="mc_rm" data-nav>Remove</button></div>
				<div class="mrow" style="margin-top:8px"><button class="btn primary" id="mc_toport" data-nav>Copy ticked saves to the card in front port&nbsp;<span class="pn">1</span></button><button class="btn" id="mc_whole" data-nav>Put this whole card on the card in front port&nbsp;<span class="pn">1</span></button></div>
				<div class="mrow" style="margin-top:8px"><button class="btn" id="mc_import" data-nav>Add a save (.mcs)…</button><input type="file" id="mc_file" accept=".mcs" hidden><button class="btn" id="mc_dl" data-nav>Download this version (.mcd)</button><button class="btn" id="mc_ren" data-nav>Rename</button><button class="btn danger" id="mc_delv" data-nav>Delete this version</button><button class="btn danger" id="mc_del" data-nav>Delete the card</button></div></div>
			</div>`));
		let L = [], card = null, polling = false;
		const save = (data, file) => { const a = document.createElement("a"); a.href = "data:application/octet-stream;base64," + data; a.download = file; a.click(); };
		const ticked = () => [...document.querySelectorAll(".mcSv:checked")].map(x => +x.value);
		const portSpans = () => document.querySelectorAll("#mc_box .pn").forEach(e => e.textContent = M.port);
		const renderLib = () => {
			const keep = $("#mc_as").value, real = L.filter(c => c.real);
			$("#mc_as").innerHTML = `<option value="">Recognise it (or a new card)</option>` + real.map(c => `<option value="${esc(c.name)}">${esc(c.name)}</option>`).join("") + `<option value="+">New card...</option>`;
			if ([...$("#mc_as").options].some(o => o.value === keep)) $("#mc_as").value = keep;
			$("#mc_to").innerHTML = L.filter(c => c.name !== M.sel).map(c => `<option value="${esc(c.name)}">${esc(c.name)} (${c.free} free)</option>`).join("");
			$("#mc_lib").innerHTML = L.length ? `<table><thead><tr><th>Card</th><th>Saves</th><th>Free blocks</th><th>Versions</th><th>Latest</th></tr></thead><tbody>` +
				L.map((c, i) => `<tr class="pick${c.name === M.sel ? " sel" : ""}" data-i="${i}" data-nav tabindex="0"><td><b>${esc(c.name)}</b> ${c.real ? '<span class="mpill good">real card</span>' : '<span class="mpill muted">made here</span>'}</td><td>${c.saves ?? ""}</td><td>${c.free ?? 0} / 15</td><td>${c.versions.length}</td><td>${esc(verName(c.latest))}</td></tr>`).join("") + "</tbody></table>"
				: `<span class="muted">No cards yet: back up a card, or make a blank one.</span>`;
			$("#mc_lib").querySelectorAll("tr.pick").forEach(tr => tr.onclick = () => show(L[+tr.dataset.i].name));
		};
		const loadLib = async () => { try { L = (await ss1({ cmd: "memcard", do: "library" })).library || []; } catch (e) { $("#mc_lib").innerHTML = `<span class="muted">${esc(e.message)}</span>`; return; } if (!alive()) return; renderLib(); if (M.sel && !L.some(c => c.name === M.sel)) { M.sel = ""; $("#mc_box").style.display = "none"; } };
		const show = async (name, ver) => {
			M.sel = name; M.ver = ver || "";
			let d;
			try { d = await ss1({ cmd: "memcard", do: "card", name, version: M.ver }); } catch (e) { toast(e.message); return; }
			if (!alive()) return;
			card = d.card; M.ver = d.version;
			const V = d.versions || [];
			$("#mc_box").style.display = ""; $("#mc_name").textContent = name; portSpans();
			$("#mc_ver").innerHTML = V.slice().reverse().map((v, i) => `<option value="${esc(v)}">${esc(verName(v))}${i == 0 ? " (latest)" : ""}</option>`).join(""); $("#mc_ver").value = M.ver;
			$("#mc_info").textContent = card.formatted ? `${card.saves.length} saves · ${card.free} of 15 blocks free` : "Not formatted";
			const S = card.saves || [];
			$("#mc_saves").innerHTML = S.length ? `<table><thead><tr><th><input type="checkbox" id="mc_all" data-nav></th><th></th><th>Save</th><th>Game</th><th>Blocks</th><th></th></tr></thead><tbody>` +
				S.map(s => `<tr><td><input type="checkbox" class="mcSv" value="${s.slot}" data-nav></td><td>${icon(s.icon)}</td><td><b>${esc(s.title)}</b>${s.broken ? ' <span class="mpill bad">damaged</span>' : ""}<div class="muted" style="font-size:12px">${esc(s.name)}</div></td><td>${esc(s.product || "")}${s.region ? ` <span class="muted">(${esc(s.region)})</span>` : ""}</td><td>${s.blocks.length}</td><td><button class="btn sm" data-slot="${s.slot}" data-nav>Download (.mcs)</button></td></tr>`).join("") + "</tbody></table>"
				: `<span class="muted">No saves on this card.</span>`;
			if ($("#mc_all")) $("#mc_all").onchange = e => document.querySelectorAll(".mcSv").forEach(x => x.checked = e.target.checked);
			$("#mc_saves").querySelectorAll("button[data-slot]").forEach(bt => bt.onclick = async () => { try { const x = await ss1({ cmd: "memcard", do: "export", name: M.sel, version: M.ver, slot: +bt.dataset.slot }); save(x.data, x.file); } catch (e) { toast(e.message); } });
			renderLib();
		};
		const edit = async (o, msg) => { try { const r = await ss1(Object.assign({ cmd: "memcard", do: "edit" }, o)); if (msg) toast(msg); await loadLib(); return r; } catch (e) { toast(e.message); return null; } };
		/* the job in a port: SS1 Tool's live card map, one square a sector */
		const drawJob = j => {
			if (!j || !j.name) { $("#mc_job").style.display = "none"; return; }
			$("#mc_job").style.display = "";
			$("#mc_title").textContent = ({ backup: "Backing up", restore: "Writing the card", check: "Checking", prepare: "Getting the front ports ready" }[j.name] || j.name) + ` · front port ${j.port || M.port}`;
			$("#mc_step").textContent = j.busy ? `${j.total ? Math.round(100 * j.done / j.total) : 0}%` : (j.ok ? "Done" : "Stopped");
			$("#mc_msg").textContent = j.message || "";
			const g = $("#mc_map").getContext("2d"), map = j.map || "";
			for (let i = 0; i < 1024; i++) { g.fillStyle = MCCOL[+(map[i] || 0)] || MCCOL[0]; g.fillRect((i % 32) * 8, Math.floor(i / 32) * 8, 7, 7); }
			$("#mc_leg").innerHTML = MCLEG.map(([k, t]) => `<span style="display:inline-block;width:10px;height:10px;background:${MCCOL[k]};margin:0 4px 0 10px"></span>${t}`).join("");
			$("#mc_ask").style.display = j.ask_name && !j.busy ? "" : "none";
			if (j.ask_name && !$("#mc_askname").value) $("#mc_askname").value = j.suggest || "";
		};
		const poll = async () => {
			if (polling) return; polling = true;
			while (alive()) {
				let s;
				try { s = await ss1({ cmd: "memcard" }); } catch (e) { break; }
				if (!alive()) break;
				drawJob(s.job);
				if (!s.job || !s.job.busy) { await loadLib(); if (s.job && s.job.ok && M.after) { const f = M.after; M.after = null; await f(s.job); } break; }
				await new Promise(r => setTimeout(r, 700));
			}
			polling = false;
		};
		const job = async (o, after) => { M.after = after || null; try { await ss1(Object.assign({ cmd: "memcard", port: M.port }, o)); } catch (e) { toast(e.message); return; } poll(); };
		$("#mc_port").value = M.port;
		$("#mc_port").onchange = e => { M.port = +e.target.value; portSpans(); };
		$("#mc_as").onchange = e => $("#mc_new").style.display = e.target.value === "+" ? "" : "none";
		$("#mc_backup").onclick = () => { const as = $("#mc_as").value === "+" ? $("#mc_new").value.trim() : $("#mc_as").value; job({ do: "backup", name: as }); };
		$("#mc_askok").onclick = async () => { try { await ss1({ cmd: "memcard", do: "name", name: $("#mc_askname").value.trim() }); $("#mc_askname").value = ""; poll(); } catch (e) { toast(e.message); } };
		$("#mc_newcard").onclick = async () => { const n = await askText("New blank card", ""); if (!n) return; const r = await edit({ op: "new", new_name: n }); if (r) show(r.name); };
		$("#mc_ver").onchange = e => show(M.sel, e.target.value);
		$("#mc_copy").onclick = $("#mc_move").onclick = async e => {
			const op = e.currentTarget.id === "mc_move" ? "move" : "copy", slots = ticked(), to = $("#mc_to").value;
			if (!slots.length) return toast("Tick the saves first");
			if (!to) return toast("Make or back up another card first");
			if (await edit({ op, name: M.sel, version: M.ver, slots, to }, op === "move" ? `Moved to ${to}` : `Copied to ${to}`)) show(M.sel);
		};
		twice($("#mc_rm"), "Press again: a new version without them", async () => { const slots = ticked(); if (!slots.length) return toast("Tick the saves first"); if (await edit({ op: "remove", name: M.sel, version: M.ver, slots }, "Removed (new version: the old one stays)")) show(M.sel); });
		/* to a real card: back the port's card up (into its own library card), add the ticked saves to that, write it back */
		twice($("#mc_toport"), "Press again: the card is backed up first", () => {
			const slots = ticked(); if (!slots.length) return toast("Tick the saves first");
			const from = M.sel, ver = M.ver;
			job({ do: "backup", name: "" }, async j => {
				const target = (j.file || "").split("/")[0];
				if (!target) return toast("The card wasn't recognised: name it, then copy again");
				if (target === from) return toast("That's the same card");
				if (!await edit({ op: "copy", name: from, version: ver, slots, to: target })) return;
				const v = (await ss1({ cmd: "memcard", do: "card", name: target })).version;
				job({ do: "restore", name: `${target}/${v}.mcd` });
			});
		});
		twice($("#mc_whole"), "Press again: what's on it now is backed up first", () => job({ do: "restore", name: `${M.sel}/${M.ver}.mcd` }));
		$("#mc_import").onclick = () => $("#mc_file").click();
		$("#mc_file").onchange = async e => {
			const f = e.target.files[0]; if (!f) return;
			const buf = new Uint8Array(await f.arrayBuffer()); let bin = ""; for (let i = 0; i < buf.length; i++) bin += String.fromCharCode(buf[i]);
			if (await edit({ op: "import", name: M.sel, data: btoa(bin) }, "Save added (new version)")) show(M.sel);
			e.target.value = "";
		};
		$("#mc_dl").onclick = async () => { try { const x = await ss1({ cmd: "memcard", do: "export", name: M.sel, version: M.ver }); save(x.data, x.file); } catch (e) { toast(e.message); } };
		$("#mc_ren").onclick = async () => { const n = await askText("Rename the card", M.sel); if (!n || n === M.sel) return; const r = await edit({ op: "rename", name: M.sel, new_name: n }); if (r) show(r.name); };
		twice($("#mc_delv"), "Press again to delete this version", async () => { if (await edit({ op: "delversion", name: M.sel, version: M.ver })) show(M.sel); });
		twice($("#mc_del"), "Press again: every version (the real card isn't touched)", async () => { if (await edit({ op: "delete", name: M.sel })) { M.sel = ""; $("#mc_box").style.display = "none"; renderLib(); } });
		await loadLib();
		if (M.sel && alive()) show(M.sel, M.ver);
		poll();
	}

	/* ---------- Cloud saves (the cloud-sync module): status, send now, what's there ---------- */
	async tCloud(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="cl"><div class="empty"><span class="spin"></span></div></div><div class="panel"><h2>Cloud saves</h2><p class="note">Your saves, save states and core settings are sent to your own storage after each game, with older versions kept. WiFi, Bluetooth pairings and passwords are never sent. Storage is added on the TV (Settings › Cloud saves) or in the phone app; getting saves back is done on the TV, with a preview first.</p></div></div>`));
		const draw = async () => {
			let s;
			try { s = await ss1({ cmd: "cloud" }); } catch (e) { if (alive()) $("#cl").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return false; }
			if (!alive()) return false;
			if (!s.installed) { $("#cl").innerHTML = `<div class="empty">The cloud-sync module isn't installed on the SS1.</div>`; return false; }
			const st = s.status || {}, busy = s.job && s.job.busy, inuse = (s.remotes || []).find(r => r.in_use);
			$("#cl").innerHTML = `<dl class="kv"><dt>Storage</dt><dd>${esc(inuse ? `${inuse.name} (${inuse.type})` : st.remote || "not set up")}</dd>${st.last_push ? `<dt>Last sent</dt><dd>${esc(st.last_push)}</dd>` : ""}${st.last_error ? `<dt>Last problem</dt><dd>${esc(st.last_error)}</dd>` : ""}</dl>
				<div class="acts"><button class="btn primary" id="cl_push" data-nav${busy || !inuse ? " disabled" : ""}>${busy ? "Working…" : "Send saves now"}</button></div>
				${(s.remotes || []).length ? `<div class="sub" style="margin-top:1rem">${spaced("Storage set up")}</div>` + s.remotes.map(r => `<div class="row"><span class="n">${esc(r.name)}</span><span class="v">${esc(r.type)}${r.in_use ? " · in use" : ""}</span></div>`).join("") : ""}
				${s.job && s.job.output ? `<pre class="out">${esc(s.job.output)}</pre>` : ""}`;
			if ($("#cl_push")) $("#cl_push").onclick = async () => { try { await ss1({ cmd: "cloud", do: "push" }); toast("Sending…"); draw(); this.every(2000, draw); } catch (e) { toast(e.message); } };
			return busy;
		};
		if (await draw()) this.every(2000, draw);
	}

	/* ---------- Controllers: USB and Bluetooth pads, SS1 Tool's drawings, live; mapping in a locked
	   window (MiSTer's own, run on the SS1: you press the buttons on the controller) ---------- */
	ctWhere(w) { return { bluetooth: "Bluetooth", dock: "Dock USB", superstation: "SS1 USB" }[w] || "USB"; }
	ctTag(c) {
		const u = (c.mapping || {}).uses;
		return u === "custom" ? `<span class="tag c">Custom mapping</span>` : u === "database" ? `<span class="tag">Automatic</span>` : `<span class="tag w">Not in MiSTer's list</span>`;
	}
	async tCtrl(b) {
		const alive = this.alive;
		b.append(h(`<div class="split" style="grid-template-columns:20rem 1fr"><div class="list" id="ctl"><div class="empty"><span class="spin"></span></div></div><div class="panel" id="ctp"></div></div>`));
		let s;
		try { s = await ss1({ cmd: "controllers" }); } catch (e) { if (!alive()) return false; $("#ctl").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
		if (!alive()) return;
		this.ct = s;
		const l = $("#ctl"), cs = s.controllers || [];
		if (!cs.length) { l.innerHTML = `<div class="empty">No USB or Bluetooth controller found. Plug one into the SS1 or switch it on, then come back here.</div>`; $("#ctp").innerHTML = ""; return; }
		l.innerHTML = "";
		for (const c of cs) {
			const el = h(`<div class="row net" data-nav><span class="n"><b>${esc(c.name)}</b><small>${esc(this.ctWhere(c.where))} · ${esc(c.vidpid)}</small></span>${this.ctTag(c)}</div>`).firstChild;
			el._onfocus = () => { this.ctSel = c.id; this.ctPanel(c); };
			l.appendChild(el);
		}
		const first = cs.find(c => c.id === this.ctSel) || cs[0];
		this.ctSel = first.id;
		this.ctPanel(first);
		/* live: what's pressed on the highlighted controller */
		this.every(150, async () => {
			if (lockwin) return true;
			const pad = $("#ctpad");
			if (!pad || !this.ctSel) return true;
			try { const r = await ss1({ cmd: "controllers", live: this.ctSel }); const c = (this.ct.controllers || []).find(x => x.id === this.ctSel); padPaint(pad, Object.assign({ has: c ? c.has : null }, r.live || {})); }
			catch (e) { /* unplugged: the list catches up on the next visit */ }
			return true;
		});
	}
	ctPanel(c) {
		const p = $("#ctp");
		if (!p) return;
		const m = c.mapping || {}, style = c.style || "generic";
		p.innerHTML = `<div class="padhead"><h2 style="margin:0;font-size:1.1rem;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;min-width:0">${esc(c.name)}</h2>
				<div class="stylepick"><button class="btn" data-nav id="st_l">‹</button><b>${esc((PTL[style] || {}).n || style)}${c.style_set ? "" : " (automatic)"}</b><button class="btn" data-nav id="st_r">›</button></div></div>
			<div class="pad" id="ctpad">${ptSVG(style)}</div>
			<dl class="kv"><dt>Buttons</dt><dd>${m.uses === "custom" ? "Its own mapping" + (m.cores && m.cores.length ? ", and its own in " + esc(m.cores.join(", ")) : "") : m.uses === "database" ? "MiSTer's automatic mapping" : "Not mapped yet: MiSTer doesn't know it"}</dd>
				<dt>MiSTer's list</dt><dd>${c.db_name ? esc(c.db_name) + (c.db_user ? " (your own entry)" : "") : "Not in it"}</dd><dt>Has</dt><dd>${c.buttons} buttons, ${c.axes} axes</dd></dl>
			<div class="acts"><button class="btn primary" data-nav id="ct_map">Map buttons…</button><button class="btn" data-nav id="ct_reset"${m.uses === "custom" ? "" : " disabled"}>Reset to automatic</button><button class="btn" data-nav id="ct_bk">Back up</button></div>
			<p class="note">Map buttons works with the controller in your hand: the TV and this window show which button to press. Every change is backed up first (Backups tab).</p>`;
		padPaint($("#ctpad"), { has: c.has });
		const step = async d => {
			const keys = [null, ...PAD_ORDER];
			let i = c.style_set ? keys.indexOf(style) : 0;
			i = (i + d + keys.length) % keys.length;
			try { await ss1({ cmd: "controllers", do: "style", vidpid: c.vidpid, style: keys[i] || "auto" }); this.drawTool(); } catch (e) { toast(e.message); }
		};
		$("#st_l").onclick = () => step(-1);
		$("#st_r").onclick = () => step(1);
		$("#ct_map").onclick = () => this.ctMap(c);
		twice($("#ct_reset"), "Press again: back to automatic", async () => {
			try { const r = await ss1({ cmd: "controllers", do: "reset", vidpid: c.vidpid }); toast(`Back to automatic. The old mapping is in Backups (${r.backup})`); this.drawTool(); } catch (e) { toast(e.message); }
		});
		$("#ct_bk").onclick = async () => { try { const r = await ss1({ cmd: "controllers", do: "backup" }); toast(`Backed up ${r.saved} file${r.saved === 1 ? "" : "s"}`); } catch (e) { toast(e.message); } };
	}
	/* the locked mapping window: started here, run by the SS1 (the TV shows it too) */
	async ctMap(c) {
		try { await ss1({ cmd: "controllers", do: "map", vidpid: c.vidpid }); } catch (e) { toast(e.message); return; }
		const back = cur;
		const w = h(`<div class="lockwin"><div class="win"><div><div class="sub">${spaced("Mapping")}</div><div class="step" id="mw_step">STARTING ON THE TV…</div>
			<div class="ask" id="mw_ask">One moment</div><div class="hint" id="mw_hint">Press the buttons on the controller itself. Select + Start together cancels, nothing saved.</div>
			<div class="bar"><i id="mw_bar"></i></div><div class="acts"><button class="btn" data-nav id="mw_skip">Skip</button><button class="btn" data-nav id="mw_cancel">Cancel</button></div></div>
			<div class="pad" id="mw_pad">${ptSVG(c.style || "generic")}</div></div></div>`).firstChild;
		document.body.appendChild(w);
		lockwin = w;
		focus($("#mw_cancel", w));
		let done = false, seen = new Set(), mapped = new Set(), lastStep = null, started = Date.now();
		const close = msg => {
			if (done) return;
			done = true;
			if (msg) toast(msg);
			setTimeout(() => { w.remove(); lockwin = null; if (back && document.body.contains(back)) focus(back); this.drawTool(); }, msg ? 1800 : 0);
		};
		w._esc = () => ss1({ cmd: "controllers", do: "map_cancel" }).catch(() => close("Cancelled"));
		$("#mw_cancel", w).onclick = w._esc;
		$("#mw_skip", w).onclick = () => ss1({ cmd: "controllers", do: "map_skip" }).catch(e => toast(e.message));
		const tick = async () => {
			if (done) return;
			let r;
			try { r = await ss1({ cmd: "controllers", live: c.id }); } catch (e) { close("The controller went away: nothing was saved"); return; }
			const m = r.map || {}, live = r.live || {};
			if (m.dev === c.vidpid && m.done) {
				$("#mw_step", w).textContent = m.saved ? "SAVED" : "CANCELLED";
				$("#mw_ask", w).textContent = m.saved ? "All set" : "Nothing changed";
				$("#mw_bar", w).style.width = m.saved ? "100%" : "0";
				close(m.saved ? "Mapped: it works this way in MiSTer's menu, Phosphor and every core" : m.error || "Cancelled: the controller keeps its old mapping");
				return;
			}
			if (m.on && m.dev === c.vidpid) {
				if (m.step !== lastStep) { seen.forEach(x => mapped.add(x)); seen.clear(); lastStep = m.step; }
				$("#mw_step", w).textContent = `STEP ${m.n} OF ${m.of}`;
				$("#mw_ask", w).textContent = m.ask || "";
				$("#mw_hint", w).textContent = m.step < 0 ? "No stick or D-pad like that? Skip, or press any button." : m.step >= 6 && m.step <= 11 ? "Not on this controller? Skip it." : "On the controller in your hand.";
				$("#mw_bar", w).style.width = (100 * (m.n - 1) / m.of) + "%";
			} else if (Date.now() - started > 8000 && !m.on) { close("The TV didn't start the mapping (is Phosphor on screen?)"); return; }
			(live.on || []).forEach(x => seen.add(x));
			padPaint($("#mw_pad", w), Object.assign({ has: c.has, target: m.target, done: [...mapped] }, live));
			setTimeout(tick, 150);
		};
		tick();
	}
	/* backups of every controller's mapping files, on the SS1 */
	async tCtrlBk(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="cbl"><div class="empty"><span class="spin"></span></div></div><div class="panel" id="cbp"><h2>Backups</h2>
			<p class="note">Each backup holds every controller's mapping files and your own controller profiles (gamecontrollerdb_user.txt), in phosphor/backups/controller-maps on the SD card. Resets and restores make one first.</p>
			<div class="acts"><button class="btn primary" data-nav id="cb_now">Back up now</button></div></div></div>`));
		let s;
		try { s = await ss1({ cmd: "controllers" }); } catch (e) { if (!alive()) return false; $("#cbl").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return; }
		if (!alive()) return;
		$("#cb_now").onclick = async () => { try { const r = await ss1({ cmd: "controllers", do: "backup" }); toast(`Backed up ${r.saved} file${r.saved === 1 ? "" : "s"}`); this.drawTool(); } catch (e) { toast(e.message); } };
		const l = $("#cbl"), bks = s.backups || [];
		if (!bks.length) { l.innerHTML = `<div class="empty">No backups yet.</div>`; return; }
		l.innerHTML = "";
		const when = n => { const m = /^(\d{4})(\d\d)(\d\d)-(\d\d)(\d\d)(\d\d)_?(.*)$/.exec(n); return m ? [new Date(+m[1], m[2] - 1, +m[3], +m[4], +m[5]).toLocaleString([], { month: "short", day: "numeric", hour: "numeric", minute: "2-digit" }), m[7].replace(/[_-]/g, " ")] : [n, ""]; };
		for (const k of bks) {
			const [d, lab] = when(k.name);
			const el = h(`<div class="row net" data-nav><span class="n"><b>${esc(d)}${lab ? " · " + esc(lab) : ""}</b><small>${k.files} file${k.files === 1 ? "" : "s"}</small></span><span class="v">Restore</span></div>`).firstChild;
			const v = $(".v", el);
			el.onclick = async () => {        /* asks once: "Press again" */
				if (el.dataset.sure !== "1") { el.dataset.sure = "1"; v.textContent = "Press again to restore"; setTimeout(() => { el.dataset.sure = ""; v.textContent = "Restore"; }, 4000); return; }
				el.dataset.sure = "";
				try { const r = await ss1({ cmd: "controllers", do: "restore", name: k.name }); toast(`Put back ${r.restored} file${r.restored === 1 ? "" : "s"}. The ones before are in ${r.backup}`); this.drawTool(); } catch (e) { toast(e.message); }
			};
			l.appendChild(el);
		}
	}
	/* ---------- Diagnostics: the dashboard (live, every 2 s while it's shown) ---------- */
	async tDash(b) {
		const alive = this.alive;
		b.append(h(`<div class="dash" id="dash">
			<div class="dcard"><div class="sub">${spaced("Temperature")}</div><div class="big" id="d_temp">–</div><div class="note" id="d_tempn"></div><svg class="spark" id="s_temp" viewBox="0 0 100 24" preserveAspectRatio="none"></svg></div>
			<div class="dcard"><div class="sub">${spaced("CPU")}</div><div class="big" id="d_cpu">–</div><div class="note" id="d_cpun"></div><svg class="spark" id="s_cpu" viewBox="0 0 100 24" preserveAspectRatio="none"></svg><div id="d_cores"></div></div>
			<div class="dcard"><div class="sub">${spaced("Memory")}</div><div class="big" id="d_mem">–</div><div class="mbar"><i id="d_memb"></i></div><div class="note" id="d_memn"></div></div>
			<div class="dcard"><div class="sub">${spaced("System")}</div><dl class="kv" id="d_sys"></dl></div>
			<div class="dcard wide"><div class="sub">${spaced("Storage")}</div><div id="d_disks"></div></div>
			<div class="dcard wide"><div class="sub">${spaced("Network")}</div><div id="d_net"></div></div>
			<div class="dcard full"><div class="sub">${spaced("Processes · busiest first")}</div><table class="ptab"><thead><tr><th>PID</th><th>CPU</th><th>Memory</th><th>Name</th></tr></thead><tbody id="d_procs"></tbody></table></div>
			<div class="dcard full"><div class="sub">${spaced("Kernel log")}</div><p class="note" style="margin:.2rem 0 .6rem">The latest hardware and driver messages: useful when a controller, WiFi adapter or drive keeps disconnecting.</p>
				<div class="acts" style="margin-top:0"><button class="btn" id="d_klog" data-nav>Show the kernel log</button></div><pre class="out" id="d_kern" hidden></pre></div>
			</div>`));
		this.prev = null; this.hist = { cpu: [], temp: [] };
		$("#d_klog").onclick = async () => {
			try { const j = await ss1({ cmd: "dashboard", kernel: true }); const k = $("#d_kern"); k.hidden = false; k.textContent = j.kernel || "(empty)"; k.scrollTop = k.scrollHeight; $("#d_klog").textContent = "Refresh the kernel log"; }
			catch (e) { toast(e.message); }
		};
		const tick = async () => {
			if (document.visibilityState !== "visible") return alive();     /* nobody's looking: don't ask */
			let d;
			try { d = await ss1({ cmd: "dashboard" }); } catch (e) { if (alive()) $("#d_cpun").textContent = e.message; return alive(); }
			if (!alive()) return false;
			this.drawDash(d);
			return true;
		};
		await tick();
		this.every(2000, tick);
	}
	drawDash(d) {
		const p = this.prev, now = performance.now(), dt = p ? (now - p.t) / 1000 : 0;
		const spark = (id, arr, max) => {
			const pts = arr.map((v, i) => `${(i / Math.max(1, 29)) * 100},${24 - Math.min(1, v / max) * 22 - 1}`).join(" ");
			$("#" + id).innerHTML = arr.length > 1 ? `<polyline points="${pts}" fill="none" stroke="currentColor" stroke-width="1.5" vector-effect="non-scaling-stroke"/>` : "";
		};
		const push = (a, v) => { a.push(v); if (a.length > 30) a.shift(); };
		/* temperature: the hottest sensor */
		const t = (d.temps || []).slice().sort((a, b) => b.c - a.c)[0];
		if (t) {
			$("#d_temp").textContent = t.c.toFixed(1) + " °C";
			$("#d_temp").className = "big " + (t.c >= 80 ? "bad" : t.c >= 65 ? "warn" : "good");
			$("#d_tempn").textContent = (d.temps.length > 1 ? `hottest of ${d.temps.length}: ` : "") + t.name;
			push(this.hist.temp, t.c); spark("s_temp", this.hist.temp, 100);
		} else { $("#d_temp").textContent = "–"; $("#d_tempn").textContent = "No temperature sensor on this board"; }
		/* CPU: busy share since the last ask, all and each core */
		const cpu = Object.fromEntries((d.cpu || []).map(([n, busy, tot]) => [n, [busy, tot]]));
		if (p) {
			const pc = n => { const a = cpu[n], b = p.cpu[n]; return a && b && a[1] > b[1] ? (100 * (a[0] - b[0])) / (a[1] - b[1]) : 0; };
			const all = pc("cpu");
			$("#d_cpu").textContent = Math.round(all) + " %";
			push(this.hist.cpu, all); spark("s_cpu", this.hist.cpu, 100);
			$("#d_cores").innerHTML = Object.keys(cpu).filter(k => k !== "cpu").map(k => `<div class="core"><span>${esc(k.replace("cpu", "Core "))}</span><div class="mbar"><i style="width:${pc(k).toFixed(0)}%"></i></div><span>${Math.round(pc(k))}%</span></div>`).join("");
		}
		$("#d_cpun").textContent = `${d.cpu_model || ""}${d.load ? " · load " + d.load.map(x => x.toFixed(2)).join(" ") : ""}`;
		/* memory */
		const m = d.mem || {}, used = (m.total || 0) - (m.available || 0), mp = m.total ? (100 * used) / m.total : 0;
		$("#d_mem").textContent = Math.round(mp) + " %";
		$("#d_memb").style.width = mp.toFixed(0) + "%";
		$("#d_memn").textContent = `${kb(used)} used of ${kb(m.total)}`;
		/* system */
		const up = d.uptime || 0, dd = Math.floor(up / 86400), hh = Math.floor((up % 86400) / 3600), mm = Math.floor((up % 3600) / 60);
		$("#d_sys").innerHTML = `<dt>Up</dt><dd>${dd ? dd + "d " : ""}${hh}h ${mm}m</dd><dt>Running</dt><dd>${esc(!d.core || d.core === "MENU" ? "Phosphor (menu)" : d.core)}</dd>` +
			(d.ips || []).map(x => `<dt>${esc(x.if === "wlan0" ? "WiFi" : x.if.startsWith("eth") ? "Ethernet" : x.if)}</dt><dd>${esc(x.ip)}</dd>`).join("");
		/* storage, with read / write speed since the last ask */
		const io = Object.fromEntries((d.io || []).map(([n, r, w]) => [n, [r, w]]));
		const rate = v => v >= 1048576 ? (v / 1048576).toFixed(1) + " MB/s" : v >= 1024 ? Math.round(v / 1024) + " KB/s" : Math.round(v) + " B/s";
		$("#d_disks").innerHTML = (d.disks || []).map(x => {
			const u = x.size_kb ? (100 * (x.size_kb - x.free_kb)) / x.size_kb : 0;
			const name = x.path === "/media/fat" ? "SD card" : x.path === "/media/fat/cifs" ? "Network share" : "Drive " + x.path.replace("/media/", "");
			return `<div class="disk"><div class="dl"><b>${esc(name)}</b><span>${gb(x.free_kb)} free of ${gb(x.size_kb)} · ${esc(x.fs)}</span></div><div class="mbar${u > 92 ? " full" : ""}"><i style="width:${u.toFixed(0)}%"></i></div></div>`;
		}).join("") + (p ? `<p class="note">${Object.keys(io).map(k => { const a = io[k], b = p.io[k]; return b && dt ? `${esc(k)}: read ${rate((a[0] - b[0]) / dt)}, write ${rate((a[1] - b[1]) / dt)}` : ""; }).filter(Boolean).join(" · ")}</p>` : "");
		/* network traffic */
		const net = Object.fromEntries((d.net || []).map(([n, r, w]) => [n, [r, w]]));
		$("#d_net").innerHTML = Object.keys(net).filter(k => !/^(ifb|dummy|sit|tunl)/.test(k)).map(k => {
			const a = net[k], b = p && p.net[k];
			const label = k === "wlan0" ? "WiFi" : k.startsWith("eth") ? "Ethernet" : k;
			return `<div class="kvrow"><b>${esc(label)}</b><span>${b && dt ? `↓ ${rate((a[0] - b[0]) / dt)}   ↑ ${rate((a[1] - b[1]) / dt)}` : "…"}</span><span class="note">${kb(a[0] / 1024)} in · ${kb(a[1] / 1024)} out since start</span></div>`;
		}).join("") || `<p class="note">No network connection.</p>`;
		/* processes: CPU since the last ask, busiest first */
		const hz = d.hz || 100, prevP = p ? Object.fromEntries(p.procs.map(x => [x[0], x[2]])) : {};
		const rows = (d.procs || []).map(([pid, name, j, mem]) => ({ pid, name, mem, cpu: p && dt && prevP[pid] !== undefined ? (100 * (j - prevP[pid])) / hz / dt : 0 }))
			.sort((a, b) => b.cpu - a.cpu || b.mem - a.mem).slice(0, 14);
		$("#d_procs").innerHTML = rows.map(r => `<tr><td>${r.pid}</td><td>${r.cpu.toFixed(1)}%</td><td>${kb(r.mem)}</td><td>${esc(r.name)}</td></tr>`).join("");
		this.prev = { t: now, cpu, io, net, procs: d.procs || [] };
	}

	/* ---------- Notifications (this PC) ---------- */
	async tNotes(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="col"><div class="list" id="nl"><div class="empty"><span class="spin"></span></div></div>
			<div class="acts"><button class="btn" id="n_seen" data-nav>Mark all as seen</button><button class="btn" id="n_clr" data-nav>Clear</button><button class="btn" id="n_q" data-nav></button></div></div>
			<div class="panel" id="np"><h2>Notifications</h2><p class="note">What Phosphor noticed on the SS1 while it ran, window open or not: power trouble, settings an update reset, storage problems, box art finishing. New ones also show as a Windows notification from the tray icon, which gets a red dot until they're seen.</p></div></div>`));
		const draw = async (j) => {
			j = j || await api("/api/notes");
			if (!alive()) return;
			const nl = $("#nl");
			nl.innerHTML = j.notes.length ? "" : `<div class="empty">Nothing yet.</div>`;
			for (const x of j.notes) {
				const el = h(`<div class="row net note-${x.level}${x.seen ? "" : " new"}" data-nav><span class="n"><b>${x.level === "warn" ? "⚠ " : ""}${esc(x.title)}</b><small>${esc(x.body)}</small></span><span class="v">${esc(ago(x.at))}</span></div>`).firstChild;
				el.onclick = () => { if (x.go) goTo(x.go); };
				nl.appendChild(el);
			}
			$("#n_q").textContent = j.quiet_pc ? "Windows notifications: off" : "Windows notifications: on";
			$("#n_q").onclick = async () => { await api("/api/quiet", { quiet: !j.quiet_pc }); draw(); };
		};
		await draw();
		$("#n_seen").onclick = async () => { draw(await api("/api/notes", { do: "seen" })); this.badges(); bell(); };
		twice($("#n_clr"), "Press again to clear", async () => { draw(await api("/api/notes", { do: "clear" })); this.badges(); bell(); });
		/* looking at them counts as seeing them */
		setTimeout(async () => { if (alive()) { await api("/api/notes", { do: "seen" }); this.badges(); bell(); } }, 2500);
	}

	/* ---------- Connection, About ---------- */
	async tConn(b) {
		const alive = this.alive;
		let rc = { auto: true };
		try { rc = await api("/api/reconnect"); } catch (e) {}
		if (!alive()) return;
		b.append(h(`<div class="split"><div class="list"><dl class="kv" style="max-width:34rem">
			<dt>SS1</dt><dd>${esc(S.address || "not paired")}</dd><dt>Status</dt><dd>${S.connected ? "connected" : S.paired ? (rc.auto ? "not answering: looking for it" : "not answering") : "not paired"}</dd>
			<dt>Phosphor on the SS1</dt><dd>${esc(S.status ? S.status.version || "" : "")}</dd><dt>Games</dt><dd>${S.status ? n(S.status.games) : ""}</dd></dl>
			<div class="acts"><button class="btn primary" id="rc_now" data-nav>Reconnect now</button><button class="btn" id="rc_auto" data-nav>Reconnect automatically: ${rc.auto ? "on" : "off"}</button></div>
			<div class="acts"><button class="btn" id="repair" data-nav>Pair again…</button><button class="btn" id="forget" data-nav>Forget this SS1</button></div></div>
			<div class="panel"><h2>Connection</h2><p class="note">This PC reaches the SS1 over your home network with Phosphor's remote access, paired with a code. The pairing key stays on this PC, protected for your Windows account. Nothing goes over the internet.</p>
			<p class="note"><b>Reconnect automatically:</b> when the SS1 restarts or your router gives it a new address, Phosphor finds it again on your network by itself, no pairing needed. Only the SS1 this PC is paired with can answer, so another one on the network is never picked up.</p>
			<p class="note">After the SS1's SD card is reflashed it doesn't know this PC any more: pair again then, with the code from Settings \u203A Remote access on the TV.</p></div></div>`));
		$("#rc_now").onclick = async () => {
			$("#rc_now").textContent = "Looking…";
			try {
				const j = await api("/api/reconnect", { now: true });
				toast(j.found ? (j.moved ? "Found it at " + j.address : "Connected") : "Couldn't find it: is it on, and on the same network?");
			} catch (e) { toast(e.message); }
			await refreshState();
			if (alive()) this.drawTool(true);
		};
		$("#rc_auto").onclick = async () => { await api("/api/reconnect", { auto: !rc.auto }); this.drawTool(true); };
		$("#repair").onclick = () => push(Connect);
		twice($("#forget"), "Press again to forget", async () => { await api("/api/forget", {}); await refreshState(); toast("Forgotten. To use it again, pair again."); replace(Connect); });
	}
	tAbout(b) {
		const alive = this.alive;
		b.append(h(`<div class="panel" style="max-width:46rem"><h2>Phosphor for Windows ${esc(S.appVersion || "")}</h2>
			<p class="note">The PC companion for Phosphor on the SuperStation One: the library, box art and details, base games, and the SS1's settings and tools, from your PC.</p>
			<p class="note">Unofficial. Not made or supported by MiSTer-devel or the makers of the SuperStation One. Free for personal, non-commercial use (PolyForm Noncommercial 1.0.0).</p>
			<p class="note">Closing the window keeps Phosphor in the tray (bottom right, by the clock), watching for warnings. To stop it completely:</p>
			<div class="acts"><button class="btn" id="quit" data-nav>Exit Phosphor</button></div></div>`));
		$("#quit").onclick = async () => { await api("/api/quit", {}); window.close(); };
	}

	/* ---------- Devices (view only: pairing and changes are made on the SS1 itself) ---------- */
	async tDevBt(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="bt"><div class="empty"><span class="spin"></span></div></div>
			<div class="panel"><h2>Bluetooth</h2><p class="note">What's paired with the SuperStation, and what's connected now. Pairing, forgetting and disconnecting are done on the SS1 itself (Settings › Bluetooth), so nothing here can change them.</p></div></div>`));
		const draw = async () => {
			let s;
			try { s = await ss1({ cmd: "bluetooth" }); } catch (e) { if (alive()) $("#bt").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return false; }
			if (!alive()) return false;
			const l = $("#bt");
			if (!s.present) { l.innerHTML = `<div class="empty">No Bluetooth adapter on the SS1.</div>`; return true; }
			const D = s.devices || [];
			l.innerHTML = D.length ? "" : `<div class="empty">Nothing paired yet.</div>`;
			for (const d of D) {
				const el = h(`<div class="row net" data-nav><span class="n"><b>${esc(d.name || d.mac)}</b><small>${esc(d.input ? "Controller / input" : d.icon || "Device")} · ${esc(d.mac)}</small></span><span class="v">${d.connected ? '<span class="mpill good">connected</span>' : '<span class="mpill muted">not connected</span>'}</span></div>`).firstChild;
				l.appendChild(el);
			}
			return true;
		};
		if (await draw()) this.every(3000, draw);
	}
	async tDevPorts(b) {
		const alive = this.alive;
		const T = { 0x41: "Digital controller", 0x73: "DualShock (analog)", 0x53: "Analog joystick", 0x23: "neGcon", 0x12: "Mouse", 0x63: "GunCon", 0xF3: "Controller (setting up)" };
		b.append(h(`<div class="split"><div class="list" id="fp"><div class="empty"><span class="spin"></span></div></div>
			<div class="panel"><h2>Front ports</h2><p class="note">The PlayStation controllers in the SuperStation's two front ports. The live controller test is on the TV (Settings › Front ports & memory cards › Controller test); memory cards are in Saves & memory cards.</p></div></div>`));
		const draw = async () => {
			if (document.visibilityState !== "visible") return alive();
			let p, pads = null;
			try { p = (await ss1({ cmd: "ports" })).ports || {}; } catch (e) { if (alive()) $("#fp").innerHTML = `<div class="empty">${esc(e.message)}</div>`; return false; }
			if (p.available) try { pads = (await ss1({ cmd: "pads" })).pads || []; } catch (e) {}
			if (!alive()) return false;
			const l = $("#fp");
			if (!pads) {
				l.innerHTML = `<div class="empty">${p.in_game ? "A game is running: the front ports belong to it." : "The front ports are read through Phosphor's Main while the menu core is loaded. Open Settings › Front ports & memory cards on the TV once to get them ready."}</div>`;
				return true;
			}
			l.innerHTML = [1, 2].map(n => {
				const x = pads.find(q => q.port === n) || {};
				return `<div class="row net"><span class="n"><b>Port ${n}</b><small>${x.connected ? esc(T[x.id] || "Controller") : "Nothing plugged in"}</small></span><span class="v">${x.connected ? '<span class="mpill good">connected</span>' : ""}</span></div>`;
			}).join("");
			return true;
		};
		if (await draw()) this.every(1500, draw);
	}
	async tDevUsb(b) {
		const alive = this.alive;
		const K = { input: "Controller / keyboard", storage: "Drive", bluetooth: "Bluetooth adapter", network: "Network adapter", audio: "Sound", vendor: "Device", other: "Device" };
		b.append(h(`<div class="split"><div class="list" id="usb"><div class="empty"><span class="spin"></span></div></div>
			<div class="panel"><h2>USB</h2><p class="note">Everything plugged into the SuperStation's USB ports and dock (hubs left out). Drives are in Storage, controllers in Controllers.</p></div></div>`));
		let s;
		try { s = await ss1({ cmd: "usb" }); } catch (e) { if (alive()) $("#usb").innerHTML = `<div class="empty">${esc(/only works|unknown/i.test(e.message) ? "Update Phosphor on the SS1 to see USB devices here." : e.message)}</div>`; return; }
		if (!alive()) return;
		const D = s.devices || [];
		$("#usb").innerHTML = D.length ? D.map(d => `<div class="row net" data-nav><span class="n"><b>${esc(d.product || d.id)}</b><small>${esc(K[d.kind] || "Device")}${d.maker ? " · " + esc(d.maker) : ""} · ${esc(d.id)}</small></span><span class="v">${d.mbps >= 5000 ? "USB 3" : d.mbps >= 480 ? "USB 2" : d.mbps ? "USB 1" : ""}</span></div>`).join("") : `<div class="empty">Nothing plugged in.</div>`;
	}

	/* ---------- Network (view only: WiFi and remote access are changed on the SS1 itself) ---------- */
	async tNet(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="net"><div class="empty"><span class="spin"></span></div></div>
			<div class="panel"><h2>Network</h2><p class="note">How the SuperStation is connected. WiFi, its passwords and remote access are set on the SS1 itself (Settings › WiFi, Settings › Remote access): they never leave it, and nothing here can change them.</p></div></div>`));
		let s;
		try { s = await ss1({ cmd: "network" }); } catch (e) { if (alive()) $("#net").innerHTML = `<div class="empty">${esc(/only works|unknown/i.test(e.message) ? "Update Phosphor on the SS1 to see this." : e.message)}</div>`; return; }
		if (!alive()) return;
		const w = s.wifi || {}, bars = sig => sig >= -55 ? "excellent" : sig >= -67 ? "good" : sig >= -75 ? "fair" : "weak";
		let html = `<dl class="kv"><dt>Name on the network</dt><dd>${esc(s.hostname || "")}</dd><dt>This app talks to it at</dt><dd>${esc(S.address || "")}</dd>${s.gateway ? `<dt>Router</dt><dd>${esc(s.gateway)} (${esc(s.gateway_if || "")})</dd>` : ""}${(s.dns || []).length ? `<dt>DNS</dt><dd>${(s.dns || []).map(esc).join(", ")}</dd>` : ""}</dl>`;
		if (w.present) html += `<div class="sub" style="margin-top:1.2rem">${spaced("WiFi")}</div><dl class="kv">${w.connected ? `<dt>Network</dt><dd>${esc(w.ssid)}</dd><dt>Signal</dt><dd>${w.signal} dBm (${bars(w.signal)})</dd>${w.freq ? `<dt>Band</dt><dd>${w.freq >= 4900 ? "5 GHz" : "2.4 GHz"}</dd>` : ""}` : `<dt>WiFi</dt><dd>not connected</dd>`}</dl>`;
		html += `<div class="sub" style="margin-top:1.2rem">${spaced("Connections")}</div>`;
		for (const i of s.interfaces || []) html += `<div class="row net"><span class="n"><b>${i.kind === "wifi" ? "WiFi" : "Wired"} (${esc(i.name)})</b><small>${(i.ips || []).map(esc).join(", ") || "no address"} · ${esc(i.mac || "")}${i.mbps ? ` · ${i.mbps} Mb/s` : ""}</small></span><span class="v">${i.up ? '<span class="mpill good">up</span>' : '<span class="mpill muted">down</span>'}</span></div>`;
		$("#net").innerHTML = html;
	}

	/* ---------- MiSTer & Linux backups: every version of the system and settings files, on this PC (sysbk.go) ---------- */
	async tSysbk(b) {
		const alive = this.alive, me = this;
		b.append(h(`<div class="col grow"><div class="acts" style="margin:0 0 .6rem;align-items:center"><button class="btn primary" id="sb_now" data-nav>Back up now</button><span class="note" id="sb_st"></span></div>
			<div class="split"><div class="list" id="sb_l"><div class="empty"><span class="spin"></span></div></div><div class="panel" id="sb_p"></div></div></div>`));
		let S0 = null, sel = null;
		const STAT = { "changed": ["muted", "changed"], "reset": ["bad", "looks reset"], "kept": ["good", "kept"], "not kept yet": ["muted", "not kept yet"], "not on the SS1": ["muted", "not on the SS1"] };
		const when = t => t ? new Date(t * 1000).toLocaleString([], { dateStyle: "medium", timeStyle: "short" }) : "";
		const size = n => n >= 1048576 ? (n / 1048576).toFixed(1) + " MB" : n >= 1024 ? Math.round(n / 1024) + " KB" : n + " bytes";
		const panel = it => {
			const p = $("#sb_p");
			if (!it) { p.innerHTML = ""; return; }
			const vs = it.versions || [];
			p.innerHTML = `<h2>${esc(it.name)}</h2><p class="note">${it.kind === "system" ? "A system file: put one back and restart the SS1 to use it." : "A settings file."} ${it.exists ? size(it.size) + " on the SS1." : "Not on the SS1 now."}</p>
				${it.status === "reset" ? `<p class="note" style="color:#e3b341">It changed when MiSTer Linux was updated: the update probably put it back to stock. The one marked "before the update" is the version to put back.</p>` : ""}
				<div class="sub" style="margin-top:1rem">${spaced("Versions on this PC")}</div>
				${vs.length ? vs.map((v, i) => `<div class="row net"><span class="n"><b>${esc(when(v.seen))}${v.hash === it.hash ? " (on the SS1 now)" : it.before && v.hash === it.before.hash ? " (before the update)" : ""}</b><small>${size(v.size)} · MiSTer Linux ${esc(v.linux || "?")}</small></span>${v.hash === it.hash ? "" : `<button class="btn" data-nav data-put="${esc(v.hash)}">Put back</button>`}</div>`).join("")
					: `<div class="empty">None yet: Back up now.</div>`}
				<p class="note" style="margin-top:1rem">Putting one back keeps the file as it is on the SS1 first (phosphor/backups/sysbk there), so it can be undone.</p>`;
			p.querySelectorAll("[data-put]").forEach(bt => twice(bt, "Press again to put it back", async () => {
				try { await api("/api/sysbk", { do: "restore", id: it.id, hash: bt.dataset.put }); toast("Putting it back…"); me.every(1000, load); } catch (e) { toast(e.message); }
			}));
		};
		const load = async () => {
			try { S0 = await api("/api/sysbk"); } catch (e) { return false; }
			if (!alive()) return false;
			const st = $("#sb_st");
			const pc = S0.total ? Math.round(S0.done * 100 / S0.total) : 0;
			st.textContent = S0.busy ? (S0.job === "restore" ? "Putting it back… " : "Backing up ") + (S0.file || "") + (S0.total ? ` ${pc}%` : "…")
				: S0.offline ? S0.offline : S0.error ? "Last try: " + S0.error : S0.last ? "Last checked " + when(S0.last) + (S0.restart ? " · restart the SS1 to use what was put back" : "") : "Not backed up yet";
			$("#sb_now").disabled = !!S0.busy;
			const l = $("#sb_l");
			l.innerHTML = "";
			let first = null;
			for (const kind of ["settings", "system"]) {
				const list = (S0.items || []).filter(x => x.kind === kind);
				if (!list.length) continue;
				l.appendChild(h(`<div class="sub" style="margin:.6rem 0 .2rem 1.5rem">${spaced(kind === "settings" ? "Settings files" : "System files")}</div>`));
				for (const it of list) {
					const [cls, word] = STAT[it.status] || ["muted", it.status];
					const el = h(`<div class="row" data-nav><span class="n">${esc(it.name)}</span><span class="v"><span class="mpill ${cls}">${esc(word)}</span></span></div>`).firstChild;
					el._onfocus = () => { sel = it.id; panel(it); };
					el.onclick = () => { sel = it.id; panel(it); };
					l.appendChild(el);
					if (!first || it.id === sel) first = { el, it };
				}
			}
			if (first) panel((S0.items || []).find(x => x.id === sel) || first.it);
			return !!S0.busy;
		};
		$("#sb_now").onclick = async () => { try { await api("/api/sysbk", { do: "save" }); me.every(1000, load); } catch (e) { toast(e.message); } };
		if (await load()) this.every(1000, load);
	}

	/* ---------- Video: MiSTer's own video settings (MiSTer.ini and its profiles), tried with Keep:
	   the SS1 keeps the old file, restarts the menu with the change and puts it back by itself after
	   35 seconds unless Keep comes (phosphord's vset) ---------- */
	async tVideo(b) {
		const alive = this.alive, me = this;
		b.append(h(`<div class="col grow"><div class="acts" id="vprof" style="margin:0 0 .8rem"></div>
			<div class="split" style="grid-template-columns:minmax(0,1fr) 30rem"><div class="list" id="vl"><div class="empty"><span class="spin"></span></div></div><div class="panel" id="vp"></div></div></div>`));
		let v = null, file = null, sel = null, edits = {};
		const now0 = k => { const s = v.settings[k]; return s && s.set ? s.value : ""; };
		const vname = (k, val) => { const o = VOPT[k]; const x = o && o.v.find(e => e[0] === val); return x ? x[1] : (val === "" ? "Not set (" + (o ? o.d : "default") + ")" : val); };
		const dipHTML = rows => `<div class="dipbox"><b>Set on the side of the SS1 first</b><dl class="kv">${rows.map(([n, p]) => `<dt>DIP ${n}</dt><dd>${esc(p)}</dd>`).join("")}</dl><div class="note">${esc(DIP_NOTE_UNITS)} The ini and the switches have to agree: otherwise the picture is black and white, or missing.</div></div>`;
		const files = () => (v.files || []).filter(f => f.profile >= 0);
		const shown = () => files().find(f => f.name === v.file) || files()[0];
		const load = async () => {
			try { v = await ss1(file ? { cmd: "vset", file } : { cmd: "vset" }); if (!file) file = v.file; }
			catch (e) { if (alive()) $("#vl").innerHTML = `<div class="empty">${esc(/unknown/i.test(e.message) ? "Update Phosphor on the SS1 (0.27.25 or later) for this." : e.message)}</div>`; return; }
			if (!alive()) return;
			draw();
			if (v.pending) keepWin();
		};
		const draw = () => {
			const sh = shown(), using = sh && sh.active;
			$("#vprof").innerHTML = files().map(f => `<button class="btn${f.name === v.file ? " primary" : ""}" data-nav data-file="${esc(f.name)}">${esc(f.label)}${f.active ? " ✓" : ""}</button>`).join("")
				+ `<button class="btn" data-nav id="vundo"${!using || v.game_running ? " disabled" : ""}>Try the earlier settings</button>`;
			$("#vprof").querySelectorAll("[data-file]").forEach(x => x.onclick = () => { file = x.dataset.file; edits = {}; sel = null; load(); });
			$("#vundo").onclick = () => tryIt({ undo: true });
			const vl = $("#vl");
			vl.innerHTML = "";
			if (!using) {
				vl.innerHTML = `<div class="empty" style="text-align:left">${esc(sh ? sh.label : "")} isn't in use: Main uses ${esc((files().find(f => f.active) || {}).label || "Main")} now.</div>`;
				panelProfile(sh);
				return;
			}
			const grp = {};
			Object.keys(VOPT).forEach(k => { (grp[VOPT[k].g] = grp[VOPT[k].g] || []).push(k); });
			let first = null;
			VGROUPS.forEach(([g, name]) => {
				if (!(grp[g] || []).length) return;
				vl.appendChild(h(`<div class="sub" style="margin:.9rem 0 .2rem 1.5rem">${spaced(name.split(" (")[0].split(" - ")[0])}</div>`));
				grp[g].forEach(k => {
					const val = k in edits ? edits[k] : now0(k), ch = k in edits;
					const el = h(`<div class="row net" data-nav><span class="n"><b>${esc(VOPT[k].n)}</b><small${ch ? ' style="color:var(--acc)"' : ""}>${esc(vname(k, val))}</small></span></div>`).firstChild;
					el._onfocus = () => { sel = k; panel(k); };
					el.onclick = () => { sel = k; panel(k); const p = $("#vp [data-nav]"); if (p) focus(p); };
					vl.appendChild(el);
					if (k === sel || !first) first = first || el;
				});
			});
			panel(sel || Object.keys(VOPT)[0]);
		};
		const panelProfile = sh => {
			const vals = Object.fromEntries(Object.entries(v.settings).map(([k, s]) => [k, s.set ? s.value : ""]));
			$("#vp").innerHTML = `<h2>Use ${esc(sh.label)}</h2><p class="note">${esc(sh.name)}. Trying it restarts the menu with this profile; it goes back by itself in ${v.secs || 35} seconds unless you keep it.</p>${dipHTML(dipFor(vals, sh.label))}
				<div class="acts"><button class="btn primary" data-nav id="vtryp"${v.pending || v.game_running ? " disabled" : ""}>Try ${esc(sh.label)}</button></div>`;
			$("#vtryp").onclick = () => tryIt({ profile: sh.profile });
		};
		const panel = k => {
			const o = VOPT[k], now = now0(k), val = k in edits ? edits[k] : now, ch = k in edits;
			const rk = VRISK[o.r] || ["muted", ""], ov = (v.settings[k] && v.settings[k].overrides) || [];
			const all = Object.assign(Object.fromEntries(Object.keys(VOPT).map(x => [x, now0(x)])), edits);
			$("#vp").innerHTML = `<h2>${esc(o.n)} ${rk[1] ? `<span class="mpill ${rk[0] === "warn" ? "muted" : rk[0]}">${esc(rk[1])}</span>` : ""}</h2>
				<p class="note">${esc(o.t || "")}</p>${o.w ? `<p class="note" style="color:#e3b341">${esc(o.w)}</p>` : ""}
				${ov.length ? `<p class="note" style="color:#e3b341">Some cores set their own: ${esc(ov.join(", "))}</p>` : ""}
				<div class="acts" style="flex-wrap:nowrap;align-items:center"><button class="btn" data-nav id="vprev" aria-label="Previous value">◀</button><span style="flex:1;text-align:center;${ch ? "color:var(--acc)" : ""}">${esc(vname(k, val))}</span><button class="btn" data-nav id="vnext" aria-label="Next value">▶</button></div>
				${ANALOG_KEYS.has(k) && ch ? dipHTML(dipFor(all, (shown() || {}).label)) : ""}
				<div class="acts"><button class="btn primary" data-nav id="vtry"${!ch || v.pending || v.game_running ? " disabled" : ""}>Try it</button>${ch ? `<button class="btn" data-nav id="vcancel">Cancel</button>` : ""}</div>
				${v.game_running ? `<p class="note">A game is running: close it first (trying restarts the menu).</p>` : `<p class="note">Goes back by itself in ${v.secs || 35} seconds unless you keep it.</p>`}`;
			const step = d => {
				const list = o.v.map(e => e[0]), at = list.indexOf(k in edits ? edits[k] : now);
				edits[k] = list[at < 0 ? 0 : (at + d + list.length) % list.length];
				if (edits[k] === now) delete edits[k];
				const keep = document.querySelector(".f") && document.querySelector(".f").id;
				draw(); panel(k);
				const again = keep && $("#" + keep); if (again) focus(again, true);
			};
			$("#vprev").onclick = () => step(-1);
			$("#vnext").onclick = () => step(1);
			$("#vtry").onclick = () => {
				const t = { [k]: edits[k] };
				if (ANALOG_KEYS.has(k)) Object.keys(edits).filter(x => ANALOG_KEYS.has(x)).forEach(x => { t[x] = edits[x]; });
				tryIt({ try: t }, () => Object.keys(t).forEach(x => delete edits[x]));
			};
			if (ch) $("#vcancel").onclick = () => { delete edits[k]; draw(); };
		};
		const tryIt = async (req, done) => {
			try { v = await ss1(Object.assign({ cmd: "vset", file }, req)); if (done) done(); if (!alive()) return; draw(); keepWin(); }
			catch (e) { toast(e.message); }
		};
		/* Keep? A window over everything until Keep, Put back, or the SS1 put it back by itself */
		const keepWin = () => {
			if (!v.pending || $(".lockwin.vkeep")) return;
			const p = v.pending, prof = files().find(f => f.profile === (p.changes || {}).profile);
			const what = p.kind === "profile" ? "profile " + (prof ? prof.label : p.changes.profile) : p.changes.undo ? "the settings from before"
				: Object.keys(p.changes).map(k => (VOPT[k] ? VOPT[k].n : k) + ": " + vname(k, p.changes[k])).join(", ");
			const w = h(`<div class="lockwin vkeep"><div class="win" style="max-width:34rem"><div><div class="sub">${spaced("Trying")}</div>
				<h2 style="margin:.4rem 0 .6rem">${esc(what)}</h2><p class="note">Picture looks right on the TV? Keep it. Otherwise the old setting comes back by itself, even with no picture or this PC off.</p>
				<div style="display:flex;align-items:center;gap:1rem;margin:1rem 0"><div style="flex:1;height:6px;border-radius:3px;background:var(--line);overflow:hidden"><div id="kbar" style="height:6px;background:var(--acc);width:100%"></div></div><span class="mono" id="ksec" style="color:var(--acc)"></span></div>
				<div class="acts"><button class="btn primary" id="kkeep" data-nav>Keep</button><button class="btn" id="kback" data-nav>Put back now</button></div></div></div></div>`).firstChild;
			document.body.appendChild(w);
			const prev = lockwin; lockwin = w;
			let left = p.secs_left, total = v.secs || 35;
			const close = () => { clearInterval(t); lockwin = prev; w.remove(); };
			const tick = () => { $("#ksec", w).textContent = Math.max(0, left) + " s"; $("#kbar", w).style.width = Math.max(0, left) * 100 / total + "%"; };
			tick();
			const t = setInterval(async () => {
				left--; tick();
				if (left <= 0) { close(); toast("Put back: the SS1 went back to the old setting"); setTimeout(load, 2500); }
			}, 1000);
			const act = async what => {
				try { v = await ss1({ cmd: "vset", file, [what]: true }); toast(what === "keep" ? "Kept" : "Put back"); } catch (e) { toast(e.message); }
				close(); if (what === "keep") { file = null; edits = {}; } load();
			};
			$("#kkeep", w).onclick = () => act("keep");
			$("#kback", w).onclick = () => act("revert");
			w._esc = () => act("revert");
			focus($("#kkeep", w));
		};
		await load();
	}

	/* ---------- Remote control: Phosphor's screen on the TV, from here (not network, pairing or remote access settings) ---------- */
	async tRemote(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list">
			<div class="remote" id="rc">
				<div class="rc-sh"><button class="btn" data-k="l" data-nav>L</button><span></span><button class="btn" data-k="r" data-nav>R</button></div>
				<div class="rc-main">
					<div class="rc-dpad"><span></span><button class="btn" data-k="up" data-nav>▲</button><span></span><button class="btn" data-k="left" data-nav>◀</button><span></span><button class="btn" data-k="right" data-nav>▶</button><span></span><button class="btn" data-k="down" data-nav>▼</button><span></span></div>
					<div class="rc-mid"><button class="btn" data-k="select" data-nav>Select</button><button class="btn" data-k="start" data-nav>Menu</button></div>
					<div class="rc-face"><span></span><button class="btn" data-k="y" data-nav>Y</button><span></span><button class="btn" data-k="x" data-nav>X</button><span></span><button class="btn" data-k="b" data-nav>B</button><span></span><button class="btn primary" data-k="a" data-nav>A</button><span></span></div>
				</div>
				<p class="note" style="margin-top:1rem">Or use this PC's keyboard while this box is selected: arrows, Enter (A), Backspace (B), X, Y, M (Menu).</p>
			</div>
			<div class="sub" style="margin-top:1.4rem">${spaced("Start a game on the TV")}</div>
			<input type="search" id="rc_q" placeholder="Type part of a game's name" data-nav style="max-width:28rem;margin:.5rem 0">
			<div id="rc_res"></div></div>
			<div class="panel"><h2>Remote control</h2><p class="note">Works Phosphor's screen on the TV like a controller: while a game is running, the game has the controllers. WiFi, Bluetooth, remote access and file sharing settings can't be opened from here: they're changed on the SS1 itself.</p></div></div>`));
		const press = async k => { try { await ss1({ cmd: "press", key: k }); } catch (e) { toast(/only works|unknown/i.test(e.message) ? "Update Phosphor on the SS1 for remote control" : e.message); } };
		$("#rc").querySelectorAll("[data-k]").forEach(bt => bt.onclick = () => press(bt.dataset.k));
		const KM = { ArrowUp: "up", ArrowDown: "down", ArrowLeft: "left", ArrowRight: "right", Enter: "a", Backspace: "b", x: "x", X: "x", y: "y", Y: "y", m: "start", M: "start" };
		$("#rc").tabIndex = 0;
		$("#rc").onkeydown = e => { const k = KM[e.key]; if (!k) return; e.preventDefault(); e.stopPropagation(); press(k); };
		let t;
		$("#rc_q").oninput = e => {
			clearTimeout(t);
			t = setTimeout(async () => {
				const q = e.target.value.trim(), box = $("#rc_res");
				if (q.length < 2) { box.innerHTML = ""; return; }
				try {
					const j = await ss1({ cmd: "games", q, limit: 30 });
					if (!alive()) return;
					box.innerHTML = j.games.length ? "" : `<div class="empty">No games match.</div>`;
					for (const g of j.games) {
						const el = h(`<div class="row" data-nav><span class="n">${esc(g.title)}</span><span class="v">${esc(g.system)} · Start on the TV</span></div>`).firstChild;
						el.onclick = () => launch(g);
						box.appendChild(el);
					}
				} catch (er) { box.innerHTML = `<div class="empty">${esc(er.message)}</div>`; }
			}, 250);
		};
	}

	/* ---------- Advanced: MiSTer.ini (the old one kept on every save), Scripts (run on the TV) ---------- */
	async tIni(b) {
		const alive = this.alive, me = this;
		const M = this.ini || (this.ini = { file: "MiSTer.ini" });
		b.append(h(`<div class="split"><div class="list mcw grow" style="display:flex;flex-direction:column">
				<div class="mrow"><label for="ini_f">File</label><select id="ini_f" data-nav></select><span class="muted" id="ini_st"></span></div>
				<textarea id="ini_t" data-nav data-free spellcheck="false" style="flex:1;min-height:22rem;margin-top:.6rem;font-family:var(--mono, monospace);font-size:.85rem;white-space:pre"></textarea>
				<div class="acts"><button class="btn primary" id="ini_save" data-nav>Save</button><button class="btn" id="ini_rel" data-nav>Discard my changes</button></div></div>
			<div class="panel"><h2>MiSTer.ini</h2><p class="note">MiSTer's own settings file (video mode, scaling, controllers, ...). Every save keeps the file as it was first, in phosphor/backups/ini on the SD card: put any of those back below. Most changes show after the SS1 restarts. In the editor every key types: Esc leaves it.</p>
				<div class="sub" style="margin-top:1.2rem">${spaced("Kept copies")}</div><div id="ini_k"></div></div></div>`));
		let orig = "";
		const draw = async (o = {}) => {
			let r;
			try { r = await ss1({ cmd: "ini", file: M.file, ...o }); } catch (e) { if (alive()) { $("#ini_st").textContent = /only works|unknown/i.test(e.message) ? "Update Phosphor on the SS1 to edit this here." : e.message; } return null; }
			if (!alive()) return null;
			M.file = r.file;
			$("#ini_f").innerHTML = (r.files.length ? r.files : [r.file]).map(f => `<option${f === r.file ? " selected" : ""}>${esc(f)}</option>`).join("");
			orig = r.text; $("#ini_t").value = r.text;
			$("#ini_st").textContent = r.exists ? `${r.text.split("\n").length} lines` : "Not on the SD card yet: saving makes it";
			const K = r.kept_copies || [];
			$("#ini_k").innerHTML = K.length ? "" : `<p class="note">None yet.</p>`;
			for (const k of K.slice(0, 30)) {
				const m = /\.(\d{4})(\d\d)(\d\d)-(\d\d)(\d\d)(\d\d)/.exec(k), when = m ? new Date(+m[1], m[2] - 1, +m[3], +m[4], +m[5], +m[6]).toLocaleString() : k;
				const el = h(`<div class="row"><span class="n">${esc(when)}</span><button class="btn sm" data-nav>Put back</button></div>`).firstChild;
				twice(el.querySelector("button"), "Press again (today's is kept too)", async () => { if (await draw({ do: "restore", name: k })) toast("Put back. The one before is kept too."); });
				$("#ini_k").appendChild(el);
			}
			return r;
		};
		$("#ini_f").onchange = e => { M.file = e.target.value; draw(); };
		$("#ini_rel").onclick = () => { $("#ini_t").value = orig; };
		twice($("#ini_save"), "Press again: the old one is kept first", async () => {
			const t = $("#ini_t").value;
			if (t === orig) return toast("Nothing changed");
			if (!/^\s*\[/m.test(t)) return toast("That doesn't look like an ini file ([MiSTer] is missing): not saved");
			if (await draw({ do: "save", text: t })) toast("Saved. The old one is kept in phosphor/backups/ini");
		});
		await draw();
	}
	async tScripts(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="list" id="sc"><div class="empty"><span class="spin"></span></div></div>
			<div class="panel"><h2>Scripts</h2><p class="note">The scripts in the SD card's Scripts folder (as MiSTer's Scripts menu). Run one here and it runs on the TV, in MiSTer's own terminal: answer its questions with the controller there. Phosphor comes back when it's done.</p></div></div>`));
		let s;
		try { s = await ss1({ cmd: "scripts" }); } catch (e) { if (alive()) $("#sc").innerHTML = `<div class="empty">${esc(/only works|unknown/i.test(e.message) ? "Update Phosphor on the SS1 to run scripts from here." : e.message)}</div>`; return; }
		if (!alive()) return;
		const l = $("#sc"), L = s.scripts || [];
		l.innerHTML = L.length ? "" : `<div class="empty">No scripts in the Scripts folder.</div>`;
		for (const x of L) {
			const el = h(`<div class="row" data-nav><span class="n">${esc(x.name.replace(/\.sh$/, "").replace(/_/g, " "))}</span><span class="v">Run on the TV</span></div>`).firstChild;
			el.onclick = async () => {
				if (el.dataset.sure !== "1") { el.dataset.sure = "1"; el.querySelector(".v").textContent = "Press again to run it"; setTimeout(() => { el.dataset.sure = ""; el.querySelector(".v").textContent = "Run on the TV"; }, 4000); return; }
				el.dataset.sure = "";
				try { await ss1({ cmd: "scripts", do: "run", name: x.name }); toast(`${x.name} is running on the TV`); } catch (e) { toast(e.message); }
			};
			l.appendChild(el);
		}
	}

	/* ---------- SD card flash & health: a card in this PC's reader ---------- */
	sdBar(st) {
		const pct = st.total ? Math.min(100, Math.round(100 * st.done / st.total)) : 0;
		const ph = { download: "Downloading", prepare: "Getting ready", write: "Writing", verify: "Checking what was written", read: "Reading the card", done: "Done", failed: "Didn't finish", ready: "Ready" }[st.phase] || st.phase || "";
		return `<div class="sub" style="margin-top:1.2rem">${spaced(ph)}${st.running && st.total ? ` · ${pct}%` : ""}${st.mbps ? ` · ${st.mbps.toFixed(0)} MB/s` : ""}</div>
			${st.running ? `<div class="mbar" style="margin:.5rem 0"><i style="width:${pct}%"></i></div>` : ""}
			${st.message ? `<p class="note">${esc(st.message)}</p>` : ""}${st.error ? `<p class="note bad">${esc(st.error)}</p>` : ""}`;
	}
	async tFlash(b) {
		const alive = this.alive, me = this;
		const F = this.fl || (this.fl = { image: "", disk: null });
		b.append(h(`<div class="split"><div class="list mcw grow">
			<div class="mcard"><h3>1 · The image</h3><div id="fl_imgs"><span class="muted">Looking for the latest release…</span></div>
				<div class="mrow" style="margin-top:.6rem"><button class="btn" id="fl_pick" data-nav>Use an image on this PC…</button><span class="muted" id="fl_img"></span></div></div>
			<div class="mcard"><h3>2 · The card</h3><div id="fl_disks"><span class="muted">Looking…</span></div>
				<div class="mrow" style="margin-top:.6rem"><button class="btn" id="fl_rescan" data-nav>Look again</button><button class="btn" id="fl_why" data-nav>Why isn't my card listed?</button></div><div id="fl_all"></div></div>
			<div class="mcard"><h3>3 · Write it</h3><div class="mrow"><label for="fl_conf">Type the card's disk number to confirm</label><input id="fl_conf" style="width:5rem" data-nav><button class="btn primary" id="fl_go" data-nav>Write the card</button></div>
				<div id="fl_st"></div></div></div>
			<div class="panel"><h2>Flash an SD card</h2><p class="note">Puts the official SuperStation One SD card image on a card in this PC's card reader, the same way SS1 Tool does: everything on the card is replaced, the partition table is written last, and then the whole card is read back and compared.</p>
				<p class="note">Phosphor runs on the Regular MiSTer image: once the SuperStation has set the card up, install Phosphor on it as usual.</p>
				<p class="note">Windows asks "allow this app to make changes" when the writing starts: writing to a disk needs it. Only SD cards and USB card readers are offered, never this PC's own drives.</p></div></div>`));
		const showImg = () => { $("#fl_img").textContent = F.image ? F.image.split(/[\\/]/).pop() : "No image chosen yet"; };
		const imgs = async () => {
			let j;
			try { j = await api("/api/sd/images"); } catch (e) { j = { ok: false, error: e.message }; }
			if (!alive()) return;
			if (!j.ok) { $("#fl_imgs").innerHTML = `<span class="muted">${esc(j.error)} (you can still use an image on this PC)</span>`; return; }
			$("#fl_imgs").innerHTML = `<table><tbody>${j.images.map(a => `<tr><td><b>${esc(a.label)}</b><div class="muted" style="font-size:12px">${esc(a.release)} · ${esc(a.name)}</div></td><td>${a.size ? (a.size / 1073741824).toFixed(2) + " GB" : ""}</td><td><button class="btn sm" data-kind="${esc(a.kind)}" data-nav>${a.have ? "Use it (on this PC)" : "Download"}</button></td></tr>`).join("")}</tbody></table>`;
			$("#fl_imgs").querySelectorAll("button[data-kind]").forEach(bt => bt.onclick = async () => {
				const r = await api("/api/sd/download", { kind: bt.dataset.kind });
				if (!r.ok) return toast(r.error);
				if (r.image) { F.image = r.image; showImg(); toast("Image ready"); } else poll();
			});
		};
		const disks = async () => {
			const j = await api("/api/sd/disks");
			if (!alive()) return;
			const D = j.ok ? j.disks : [];
			if (!j.ok) { $("#fl_disks").innerHTML = `<span class="muted">${esc(j.error)}</span>`; return; }
			$("#fl_disks").innerHTML = D.length ? `<table><tbody>${D.map(d => `<tr class="pick${F.disk === d.Number ? " sel" : ""}" data-n="${d.Number}" data-nav tabindex="0"><td><b>Disk ${d.Number}</b> ${esc(d.FriendlyName)}</td><td>${(d.Size / 1e9).toFixed(1)} GB</td><td>${esc(d.Letters || "")}</td><td class="muted">${esc(d.BusType)}</td></tr>`).join("")}</tbody></table>`
				: `<span class="muted">No SD card found. Put one in this PC's card reader (or a USB card reader).</span>`;
			$("#fl_disks").querySelectorAll("tr.pick").forEach(tr => tr.onclick = () => { F.disk = +tr.dataset.n; disks(); });
		};
		const poll = async () => {
			if (me.flPolling) return; me.flPolling = true;
			while (alive()) {
				const j = await api("/api/sd/state").catch(() => null);
				if (!alive() || !j) break;
				const st = j.state;
				$("#fl_st").innerHTML = st.job && st.job !== "readtest" ? me.sdBar(st) : "";
				if (st.job === "download" && st.phase === "ready" && st.image) { F.image = st.image; showImg(); }
				if (!st.running) { if (st.job === "download") imgs(); break; }
				await new Promise(r => setTimeout(r, 700));
			}
			me.flPolling = false;
		};
		$("#fl_pick").onclick = async () => { const r = await api("/api/sd/pick", {}); if (!r.ok) return toast(r.error); if (r.image) { F.image = r.image; showImg(); } };
		$("#fl_rescan").onclick = disks;
		$("#fl_why").onclick = async () => {
			const j = await api("/api/sd/disks?all=1");
			if (!j.ok) return toast(j.error);
			$("#fl_all").innerHTML = `<table style="margin-top:.6rem"><tbody>${j.disks.map(d => `<tr><td><b>Disk ${d.number}</b> ${esc(d.name)}</td><td>${d.size ? (d.size / 1e9).toFixed(1) + " GB" : ""}</td><td>${esc(d.bus)}</td><td class="${d.offered ? "" : "muted"}">${esc(d.reason)}</td></tr>`).join("")}</tbody></table>`;
		};
		twice($("#fl_go"), "Press again: everything on the card is replaced", async () => {
			if (!F.image) return toast("Choose the image first");
			if (F.disk === null) return toast("Choose the card first");
			const r = await api("/api/sd/start", { job: "flash", disk: F.disk, image: F.image, confirm: $("#fl_conf").value.trim() });
			if (!r.ok) return toast(r.error);
			$("#fl_conf").value = "";
			poll();
		});
		showImg();
		await Promise.all([imgs(), disks()]);
		poll();
	}
	async tCardHealth(b) {
		const alive = this.alive, me = this;
		b.append(h(`<div class="split"><div class="list mcw grow">
			<div class="mcard"><h3>Cards in this PC</h3><div id="ch_d"><span class="muted">Looking…</span></div>
				<div class="mrow" style="margin-top:.6rem"><button class="btn" id="ch_re" data-nav>Look again</button></div></div>
			<div class="mcard"><h3>Read test</h3><p class="muted" style="margin:0 0 .6rem">Reads every part of the chosen card once and times it. Nothing is written. A 64 GB card takes a few minutes.</p>
				<div class="mrow"><button class="btn primary" id="ch_go" data-nav>Test the chosen card</button></div><div id="ch_st"></div></div></div>
			<div class="panel"><h2>Card health</h2><p class="note">For an SD card in this PC's card reader: what Windows says about it, and a read test that finds parts of the card that can't be read, and cards that are much slower than they should be (worn or fake).</p>
				<p class="note">The SD card in the SuperStation is checked from Storage › SD card, without taking it out.</p></div></div>`));
		let pick = null;
		const disks = async () => {
			const j = await api("/api/sd/disks?all=1");
			if (!alive()) return;
			if (!j.ok) { $("#ch_d").innerHTML = `<span class="muted">${esc(j.error)}</span>`; return; }
			const D = j.disks.filter(d => d.offered);
			if (pick === null && D.length) pick = D[0].number;
			$("#ch_d").innerHTML = D.length ? `<table><thead><tr><th>Card</th><th>Size</th><th>Windows says</th><th></th></tr></thead><tbody>${D.map(d => `<tr class="pick${pick === d.number ? " sel" : ""}" data-n="${d.number}" data-nav tabindex="0"><td><b>Disk ${d.number}</b> ${esc(d.name)}</td><td>${(d.size / 1e9).toFixed(1)} GB</td><td>${d.health === "Healthy" ? '<span class="mpill good">healthy</span>' : d.health ? `<span class="mpill bad">${esc(d.health)}</span>` : ""} <span class="muted">${esc(d.status || "")}</span></td><td>${esc(d.letters || "")}</td></tr>`).join("")}</tbody></table>`
				: `<span class="muted">No SD card found. Put one in this PC's card reader.</span>`;
			$("#ch_d").querySelectorAll("tr.pick").forEach(tr => tr.onclick = () => { pick = +tr.dataset.n; disks(); });
		};
		const poll = async () => {
			if (me.chPolling) return; me.chPolling = true;
			while (alive()) {
				const j = await api("/api/sd/state").catch(() => null);
				if (!alive() || !j) break;
				const st = j.state;
				$("#ch_st").innerHTML = st.job === "readtest" ? me.sdBar(st) + ((st.bad || []).length ? `<p class="note bad">Couldn't read at: ${st.bad.slice(0, 20).map(x => x + " MB").join(", ")}${st.bad.length > 20 ? "…" : ""}</p>` : "") : "";
				if (!st.running) break;
				await new Promise(r => setTimeout(r, 700));
			}
			me.chPolling = false;
		};
		$("#ch_re").onclick = disks;
		$("#ch_go").onclick = async () => {
			if (pick === null) return toast("Choose a card first");
			const r = await api("/api/sd/start", { job: "readtest", disk: pick });
			if (!r.ok) return toast(r.error);
			poll();
		};
		await disks();
		poll();
	}
}

/* a place in the app, from a notification or the tray: "settings/storage/1", "notifications", "match" */
function goTo(where) {
	const [a, b, c] = (where || "").split("/");
	if (a === "notifications") return push(AppSettings, "notifications");
	if (a === "match") return push(Match);
	if (a === "settings") {
		const id = SECS.some(s => s.id === b) ? b : "notifications";
		return push(AppSettings, id, Number(c || 0));
	}
}

/* the bell in the status bar: how many new notifications */
async function bell() {
	try {
		const p = await api("/api/ping");
		const el = $("#bell");
		el.hidden = !p.unseen;
		el.classList.toggle("warn", p.warn > 0);
		el.querySelector("span").textContent = p.unseen;
		if (p.view) goTo(p.view);
		return p;
	} catch (e) { return null; }
}
