/* Phosphor for Windows: Settings. Laid out like the console's Settings: the sections in a list on the
   left, the highlighted one's tools on the right, with tabs when a section has several (LB/RB or Q/E).
   A (Enter) goes into a section's tools, B (Esc) comes back to the list. Everything advanced lives
   here; the main screens stay the simple console view. */
"use strict";

const SET_SECTIONS = [
	{ id: "look", name: "Look", tabs: [["Accent & texture", "tLook"]] },
	{ id: "boxart", name: "Box art", tabs: [["Sources & logins", "tArt"]] },
	{ grp: "SuperStation" },
	{ id: "power", name: "Power & updates", tabs: [["Power", "tPower"], ["Update Phosphor", "tPhosUpd"], ["Updates & restore points", "tGuard"], ["File sharing", "tSamba"]] },
	{ id: "storage", name: "Storage", tabs: [["SD card", "tSD"], ["NVMe / USB", "tExt"], ["Backups", "tSnaps"]] },
	{ id: "controllers", name: "Controllers", tabs: [["Controllers", "tCtrl"], ["Backups", "tCtrlBk"]] },
	{ grp: "Tools" },
	{ id: "diagnostics", name: "Diagnostics", tabs: [["Dashboard", "tDash"]] },
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

	/* ---------- Look ---------- */
	async setTheme(p) { Object.assign(S, p); applyTheme(); await api("/api/theme", p); }
	tLook(b) {
		const alive = this.alive;
		b.append(h(`<div class="list grow"><div class="sub" style="margin-top:.2rem">${spaced("Accent colour")}</div><div class="swatches" id="sw"></div>
			<div class="sub" style="margin-top:1.4rem">${spaced("Background texture")}</div><div class="tex" id="tx"></div></div>`));
		for (const [name, col] of Object.entries(ACCENTS)) {
			const el = h(`<button class="sw${S.accent === name ? " on" : ""}" data-nav title="${name}" style="background:${col}"></button>`).firstChild;
			el.onclick = async () => { await this.setTheme({ accent: name }); this.drawTool(true); };
			$("#sw").appendChild(el);
		}
		const [r, g, bl] = rgb(ACCENTS[S.accent] || ACCENTS.amber);
		for (const t of Texture.order) {
			const url = Texture.tile(t, [r, g, bl], 1);
			const el = h(`<div class="tx${S.texture === t ? " on" : ""}" data-nav style="${url ? `background-image:url(${url})` : ""}">${esc(Texture.name(t))}</div>`).firstChild;
			el.onclick = async () => { await this.setTheme({ texture: t }); this.drawTool(); focus([...$("#tx").children].find(e => e.classList.contains("on"))); };
			$("#tx").appendChild(el);
		}
	}

	/* ---------- Box art: the SS1's sources (order, on/off) and their logins ---------- */
	async tArt(b) {
		const alive = this.alive;
		b.append(h(`<div class="split"><div class="col"><div class="sub">${spaced("Sources, asked in this order")}</div><div class="list" id="srcs"><div class="empty"><span class="spin"></span></div></div>
			<div class="acts"><button class="btn primary" id="runpc" data-nav>Look up on this PC</button><button class="btn" id="runart" data-nav>Look up on the SS1</button></div>
			<div class="note" id="scr_st" style="margin-top:.6rem"></div></div>
			<div class="panel" id="acct"></div></div>`));
		/* this PC: lists and pictures fetched here, matched with the SS1's own rules, finished box art sent to
		   the SS1 (much faster); the SS1: its own run, for when the PC is off */
		const scrDraw = st => {
			const el = $("#scr_st");
			if (!el || !st) return;
			const pct = st.total ? Math.round(100 * st.done / st.total) : 0;
			el.innerHTML = st.running
				? `<b>${esc(st.status || "Looking up box art")}</b>${st.total ? ` \u00B7 ${n(st.done)} of ${n(st.total)} (${pct}%)` : ""}<br>${n(st.found)} found, ${n(st.not_found)} not found${st.later ? `, ${n(st.later)} for later` : ""}${st.game ? `<br><span class="dim">${esc(st.system || "")} \u203A ${esc(st.game)}</span>` : ""}`
				: st.started ? `${esc(st.status || "Done")}: ${n(st.found)} found, ${n(st.not_found)} not found${st.later ? `, ${n(st.later)} for later` : ""}` : "";
			if (st.notes && st.notes.length) el.innerHTML += `<br>${st.notes.map(esc).join("<br>")}`;
			$("#runpc").textContent = st.running ? "Stop" : "Look up on this PC";
		};
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
			const st = !x.on ? "Off" : x.ready ? "On" : x.id === "screenscraper" && a("user") ? "Needs the password" : "Needs a login";
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
			screenscraper: [["user", "User name", "text"], ["password", "Password", "password"], ["devid", "Developer id (only if you have one)", "text"], ["devpassword", "Developer password", "password"]],
			thegamesdb: [["apikey", "API key (free, from thegamesdb.net)", "password"]],
			igdb: [["client_id", "Client ID (Twitch developer app)", "text"], ["client_secret", "Client secret", "password"]],
		}[x.id];
		const ABOUT = {
			libretro: "No account. Box art from libretro's thumbnails, matched by each file's content (CRC or product code), then by its exact name.",
			arcadedb: "No account. Arcade games by their MAME set name.",
			launchbox: "No account. LaunchBox's games database, downloaded once (about 110 MB) and kept on the SS1.",
			thegamesdb: "Needs a free API key from thegamesdb.net.",
			screenscraper: "Your screenscraper.fr login is enough (a free account). A developer login works too, if you have one; with both, the developer login is used for the program and yours for your lookups a day.",
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
			html += `<div class="acts"><button class="btn primary" id="acsave" data-nav>Save</button></div><p class="note">Saved on the SS1 (phosphor/config/scraper.ini), sent over your home network only.</p>`;
		}
		p.innerHTML = html;
		if (!F) return;
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
			<p class="note">A release package is phosphor-&lt;version&gt;-armhf.tar.gz. It's checked, the installed version is backed up, and Phosphor starts again by itself, with your library, box art and settings as they were. MiSTer's own files aren't touched.</p>
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
			<p class="note">After the SS1's SD card is reflashed it doesn't know this PC any more: pair again then, unless the SS1 keeps a copy of its remote access on the NVMe or a USB drive (Settings \u203A Remote access on the TV), which a fresh card brings back by itself.</p></div></div>
			<div class="split" style="margin-top:1.5rem"><div class="list"><dl class="kv" style="max-width:34rem" id="ssh_kv"><dt>SSH</dt><dd>…</dd></dl>
			<div class="acts"><button class="btn" id="ssh_add" data-nav>Add this PC's SSH key…</button></div></div>
			<div class="panel"><h2>SSH</h2><p class="note">SSH logs in to the SS1's Linux as root: for phosphor-link, a terminal or WinSCP. Off, Password (MiSTer's own, or one you set) or Keys only are chosen on the TV, in Settings \u203A Remote access: another device can't change them.</p>
			<p class="note"><b>Add this PC's SSH key</b> offers this PC's keys to the SS1 (Phosphor's own, which phosphor-link uses too, and any in your .ssh folder). Each one only gets in after you say yes on the TV. Private keys never leave this PC.</p></div></div>`));
		const sshDraw = async () => {
			let j;
			try { j = (await ss1({ cmd: "remote_ssh" })).ssh; } catch (e) { $("#ssh_kv").innerHTML = `<dt>SSH</dt><dd>${esc(e.message)}</dd>`; return; }
			if (!alive()) return;
			const mode = { off: "Off", password: "Password", keys: "Keys only" }[j.mode] || j.mode;
			const pw = j.mode === "password" ? (j.own_password ? "your own" : "MiSTer's own") : "not used";
			const keys = (j.keys || []).map(k => `${esc(k.name)} <span class="dim">${esc(k.type)} \u2026${esc(k.tail)}</span>`).join("<br>") || "none";
			const wait = (j.pending || []).length ? `<dt>Waiting on the TV</dt><dd>${j.pending.length} key${j.pending.length > 1 ? "s" : ""}: say yes there</dd>` : "";
			const keep = j.keep && j.keep.dir ? `${esc(j.keep.dir.replace("/media/", ""))}${j.keep.saved ? ", saved " + new Date(j.keep.saved * 1000).toLocaleString() : ""}` : "off";
			$("#ssh_kv").innerHTML = `<dt>SSH</dt><dd>${mode}</dd><dt>Password</dt><dd>${pw}</dd><dt>Keys</dt><dd>${keys}</dd>${wait}<dt>Copy kept on</dt><dd>${keep}</dd>`;
		};
		sshDraw();
		$("#ssh_add").onclick = async () => {
			try {
				const k = await api("/api/sshkeys");
				if (!k.keys || !k.keys.length) { toast("No SSH keys on this PC"); return; }
				const j = await ss1({ cmd: "remote_ssh", offer: k.keys });
				toast(`Look at the TV: say yes to add ${j.offered === 1 ? "the key" : j.offered + " keys"}`);
			} catch (e) { toast(e.message); }
			sshDraw();
			for (let i = 0; i < 20 && alive(); i++) { await new Promise(r => setTimeout(r, 3000)); if (alive()) await sshDraw(); }
		};
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
