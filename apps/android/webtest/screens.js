// Screenshots of every screen of the app's page (assets/web), with replies the SS1 really gave (fx/) behind a stand-in PhosNative.
// cd apps/android && ./gradlew preBuild && node webtest/screens.js   (W=360 Z=1.35: a narrow phone with large text; TRY=1 MORE=1 extras)
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
const { chromium } = require(process.env.PLAYWRIGHT || "playwright");
const fs = require("fs"), path = require("path"), http = require("http");
const T = __dirname, WEB = path.join(__dirname, "../app/src/main/assets/web"), GEN = path.join(__dirname, "../app/build/generated/webshared/web");
const fx = {};
for (const f of fs.readdirSync(T + "/fx")) fx[f.replace(/\.json$/, "")] = fs.readFileSync(T + "/fx/" + f, "utf8");
fx.look = JSON.stringify({ ok: true, accent: process.env.ACCENT || "amber", texture: Number(process.env.TEX || 1) });
const srv = http.createServer((q, r) => {
	let p = decodeURIComponent(q.url.split("?")[0]);
	if (p === "/") p = "/index.html";
	if (p.startsWith("/art/")) { r.setHeader("Content-Type", "image/jpeg"); r.end(fs.readFileSync(T + "/box.jpg")); return; }
	for (const d of [WEB, GEN]) { const f = path.join(d, p); if (fs.existsSync(f) && fs.statSync(f).isFile()) { r.end(fs.readFileSync(f)); return; } }
	r.statusCode = 404; r.end();
});
const mock = fx => `
window.__fx = ${JSON.stringify(fx)};
window.__vset = JSON.parse(__fx.vset);
window.PhosNative = {
	call(id, json) {
		const q = JSON.parse(json); let k = q.cmd;
		if (k === "games") k = q.filter === "missing_art" || q.limit === 0 ? "games_missing_art" : q.q ? "games_q" : "games_psx";
		let out = __fx[k] || JSON.stringify({ ok: true });
		if (k === "vset") {
			const v = __vset;
			if (q.try) { v.pending = { secs_left: 35, kind: "file", file: "MiSTer.ini", changes: q.try }; }
			if (q.profile != null) v.pending = { secs_left: 35, kind: "profile", from_profile: 0, changes: { profile: q.profile } };
			if (q.keep || q.revert) v.pending = null;
			if (q.file) v.file = q.file;
			out = JSON.stringify(v);
		}
		setTimeout(() => { const o = JSON.parse(out); o.ok === false ? __phos(id, false, o.error) : __phos(id, true, out); }, 30);
	},
	app(id, action, json) {
		const r = { state: { paired: true, address: "200.200.200.23:8767", reachable: true, version: "0.27.25", appVersion: "0.9.0", notesUnseen: 1, look: { accent: "amber", texture: 0 } },
			discover: { found: [{ address: "200.200.200.23:8767", version: "0.27.25" }] },
			phoneScrape: { running: false, started: 0 },
			ssCheck: { works: false, message: "ScreenScraper needs a developer ID and password (Phosphor's own is on its way from ScreenScraper; until then, your own if you have one)" },
			automatch: { undone: 0, matches: [
				{ id: 11, title: "Super Mario Bros. (SNES Port)", sys: "SNES", system: "SNES", kind: "port", tag: "(SNES Port)", how: "library", base_id: 3, base_title: "Super Mario Bros. (World)", found_by: [], fields: [], art: false, at: 1792904000, kept: false },
				{ id: 12, title: "Bahamut Lagoon (Japan) [T-En by DeJap]", sys: "SNES", system: "SNES", kind: "translation", tag: "[T-En by DeJap]", how: "lookup", base_id: 0, base_title: "Bahamut Lagoon (Japan)", found_by: ["libretro", "LaunchBox"], fields: ["description", "year"], art: true, at: 1792904000, kept: false },
				{ id: 13, title: "mister-boot", sys: "NES", system: "NES", kind: "notgame", tag: "mister-boot", how: "skip", base_id: 0, base_title: "", found_by: [], fields: [], art: false, at: 1792904000, kept: true }] },
			sysbk: { linux: "260912", state: { busy: false, last: 1792904000, error: "" }, items: [
				{ id: "MiSTer.ini", name: "MiSTer.ini (video, input and more)", kind: "settings", exists: true, size: 21055, hash: "a".repeat(64), status: "changed", before: null, versions: [{ hash: "a".repeat(64), size: 21055, linux: "260912", seen: 1792904000 }, { hash: "b".repeat(64), size: 20442, linux: "260912", seen: 1792800000 }] },
				{ id: "etc+dhcpcd.conf", name: "Fixed network address (dhcpcd.conf)", kind: "settings", exists: true, size: 1428, hash: "c".repeat(64), status: "reset", before: { hash: "d".repeat(64), size: 1500, linux: "260801", seen: 1792000000 }, versions: [{ hash: "c".repeat(64), size: 1428, linux: "260912", seen: 1792904000 }, { hash: "d".repeat(64), size: 1500, linux: "260801", seen: 1792000000 }] },
				{ id: "MiSTer", name: "MiSTer's Main", kind: "system", exists: true, size: 1313992, hash: "e".repeat(64), status: "kept", before: null, versions: [{ hash: "e".repeat(64), size: 1313992, linux: "260912", seen: 1792904000 }] },
				{ id: "linux+linux.img", name: "Linux", kind: "system", exists: true, size: 393216000, hash: "f".repeat(64), status: "on the PC only", before: null, versions: [] }] },
			phoneStorage: { art: [{ id: 5, title: "Analog Controller Service Disc (USA)", system: "PSX", at: 0, sent: false, bytes: 52000 }], artBytes: 52000, listsBytes: 120000000, autoDelete: true },
			notes: { list: [{ id: "a", at: Date.now(), level: "warn", title: "SD card nearly full", body: "3 GB free", seen: false, go: "storage/0" }], notify: true, allowed: false },
		}[action] || {};
		setTimeout(() => __phos(id, true, JSON.stringify(r)), 20);
	},
};`;
(async () => {
	await new Promise(r => srv.listen(8099, r));
	const b = await chromium.launch();
	const W = Number(process.env.W || 390), Z = Number(process.env.Z || 1);
	const ctx = await b.newContext({ viewport: { width: W, height: 844 }, deviceScaleFactor: 2 });
	await ctx.addInitScript(mock(fx));
	if (Z !== 1) await ctx.addInitScript(`document.addEventListener("DOMContentLoaded",()=>{document.documentElement.style.fontSize="${Z * 100}%";document.body.style.zoom="${Z}"})`);
	const page = await ctx.newPage();
	const errs = [];
	page.on("pageerror", e => errs.push(String(e)));
	page.on("console", m => { if (m.type() === "error") errs.push(m.text()); });
	const routes = (process.env.ROUTES || "home,sysbk,sysbk/etc+dhcpcd.conf,library,games?system=PSX&title=PlayStation,game/1,edit/1,based/1,saves/cards,saves/backups,saves/cloud,video,boxart/lookup,boxart/sources,look,power/power,power/update,power/guard,storage/sd,storage/drives,storage/phone,bluetooth,diag,notes,connection,about,connect").split(",");
	const out = T + "/shots" + (process.env.TAG || "");
	fs.mkdirSync(out, { recursive: true });
	await page.goto("http://localhost:8099/#home");
	await page.waitForTimeout(800);
	for (const r of routes) {
		await page.evaluate(h => { location.hash = h; }, r);
		await page.waitForTimeout(700);
		const over = await page.evaluate(() => { const vw = document.documentElement.clientWidth; return [...document.querySelectorAll("main *")].filter(e => e.getBoundingClientRect().right > vw + 1 && e.offsetParent).map(e => e.tagName + "." + e.className).slice(0, 5); });
		const fixed = await page.evaluate(() => document.querySelectorAll(".a1col,.afit").length);
		await page.screenshot({ path: `${out}/${r.replace(/[^a-z0-9]+/gi, "_")}.png`, fullPage: true });
		console.log(r, over.length ? "OVER " + over.join(" ") : "ok", fixed ? "netfixed " + fixed : "");
	}
	if (process.env.MORE) { await page.evaluate(() => { location.hash = "home"; }); await page.waitForTimeout(500); await page.click('[data-nav="more"]'); await page.waitForTimeout(300); await page.screenshot({ path: out + "/more_sheet.png" }); }
	if (process.env.TRY) {
		await page.evaluate(() => { location.hash = "video"; }); await page.waitForTimeout(600);
		await page.click('[data-act="next"][data-arg="vga_mode"]'); await page.waitForTimeout(200);
		const el = await page.$('[data-arg="vga_mode"]');
		await el.scrollIntoViewIfNeeded(); await page.evaluate(() => scrollBy(0, 150)); await page.waitForTimeout(200);
		await page.screenshot({ path: out + "/video_change.png" });
		await page.click('[data-act="try"]'); await page.waitForTimeout(1500);
		await page.screenshot({ path: out + "/video_trying.png" });
	}
	if (process.env.SSCHECK) {   /* Box art › Sources › ScreenScraper, then Check login */
		await page.evaluate(() => { location.hash = "boxart/sources"; }); await page.waitForTimeout(700);
		await page.click('[data-act="sel"][data-arg="4"]'); await page.waitForTimeout(300);
		await page.click('[data-act="sscheck"]'); await page.waitForTimeout(500);
		await page.screenshot({ path: out + "/sources_sscheck.png", fullPage: true });
	}
	console.log("errors:", errs.length ? errs.join("\n") : "none");
	await b.close(); srv.close();
})();
