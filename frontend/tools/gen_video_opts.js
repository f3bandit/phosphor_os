// Makes src/video_opts.h from apps/windows/web/video-options.js (MiSTer's video settings, explained:
// the same list the apps show), so the TV's Video screen says the same things.
//   node frontend/tools/gen_video_opts.js
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
"use strict";
const fs = require("fs"), path = require("path");
const root = path.join(__dirname, "../..");
const src = fs.readFileSync(path.join(root, "apps/windows/web/video-options.js"), "utf8");
const { VOPT, VGROUPS, VRISK } = new Function(src + "; return { VOPT, VGROUPS, VRISK };")();
const c = s => JSON.stringify(String(s == null ? "" : s)).replace(/\\u([0-9a-f]{4})/gi, (m, h) => {
	const b = Buffer.from(String.fromCharCode(parseInt(h, 16)), "utf8");
	return [...b].map(x => "\\x" + x.toString(16).padStart(2, "0")).join("") + '""';
});
let out = `/* MiSTer's video settings, explained: made by tools/gen_video_opts.js from apps/windows/web/video-options.js.
 * Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
 * Don't edit: change video-options.js and run it again. */
#ifndef PHOS_VIDEO_OPTS_H
#define PHOS_VIDEO_OPTS_H

typedef struct { const char *v, *label; } vopt_val_t;
typedef struct {
	const char *key, *name, *group, *def, *risk, *text, *warn;
	int nv;
	const vopt_val_t *vals;
} vopt_t;

`;
const keys = Object.keys(VOPT);
keys.forEach((k, i) => {
	out += `static const vopt_val_t VOPT_V${i}[] = { ${VOPT[k].v.map(([v, l]) => `{ ${c(v)}, ${c(l)} }`).join(", ")} };\n`;
});
out += `\nstatic const vopt_t VOPTS[] = {\n`;
keys.forEach((k, i) => {
	const o = VOPT[k];
	out += `\t{ ${c(k)}, ${c(o.n)}, ${c(o.g)}, ${c(o.d)}, ${c(o.r)}, ${c(o.t)}, ${o.w ? c(o.w) : "NULL"}, ${o.v.length}, VOPT_V${i} },\n`;
});
out += `};\n#define NVOPTS ((int)(sizeof(VOPTS) / sizeof(VOPTS[0])))\n\n/* the groups, in order: id, heading */\nstatic const char *const VGROUPS_[][2] = {\n`;
VGROUPS.forEach(([g, n]) => { out += `\t{ ${c(g)}, ${c(n.split(" (")[0].split(" - ")[0])} },\n`; });
out += `};\n#define NVGROUPS ((int)(sizeof(VGROUPS_) / sizeof(VGROUPS_[0])))\n\n/* risk id: words for it, 1 when it's a warning */\nstatic const struct { const char *id, *words; int warn; } VRISKS[] = {\n`;
Object.entries(VRISK).forEach(([id, [cls, w]]) => { out += `\t{ ${c(id)}, ${c(w)}, ${cls === "good" ? 0 : 1} },\n`; });
out += `};\n#define NVRISKS ((int)(sizeof(VRISKS) / sizeof(VRISKS[0])))\n\n#endif\n`;
fs.writeFileSync(path.join(root, "frontend/src/video_opts.h"), out);
console.log("video_opts.h:", keys.length, "settings");
