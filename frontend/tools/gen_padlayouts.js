// Generates frontend/src/padlayouts.h (the shell's controller layouts) from the app's
// apps/windows/web/padlayouts.js (SS1 Tool's layouts plus Phosphor's additions): node gen_padlayouts.js
const fs=require("fs"), path=require("path"), os=require("os");
const ROOT=path.join(__dirname, "..", "..");
const src="const esc=s=>s;\n"+fs.readFileSync(path.join(ROOT, "apps/windows/web/padlayouts.js"),"utf8").replace("\"use strict\";","")+";\nmodule.exports={PTL};";
const tmp=path.join(os.tmpdir(), "ptl_mod.js");
fs.writeFileSync(tmp,src);
const {PTL}=require(tmp);
let out="/* SS1 Tool's controller layouts (its web/index.html PTL) and Phosphor's additions, for the shell:\n * generated from apps/windows/web/padlayouts.js (frontend/tools/gen_padlayouts.js). x, y on a 600 x 345 drawing; t: c circle, r pill, t trigger, s stick, d D-pad, l lever. */\n";
out+="typedef struct { char t; const char *id; short x, y, a, b; const char *lab; } pl_item_t;\n";
out+="typedef struct { const char *key, *name, *group; int n; const pl_item_t *it; } pl_layout_t;\n";
let lays=[];
for (const [k,L] of Object.entries(PTL)) {
  out+=`static const pl_item_t PL_${k}[] = {\n`;
  for (const [t,c,x,y,a,b,l] of L.it) out+=`\t{ '${t}', "${c}", ${Math.round(x)}, ${Math.round(y)}, ${Math.round(a||0)}, ${Math.round(b||0)}, ${JSON.stringify(l||"")} },\n`;
  out+="};\n";
  lays.push(`\t{ "${k}", ${JSON.stringify(L.n)}, "${L.g}", (int)(sizeof(PL_${k}) / sizeof(PL_${k}[0])), PL_${k} },`);
}
out+="static const pl_layout_t PLAYOUTS[] = {\n"+lays.join("\n")+"\n};\n#define PL_COUNT ((int)(sizeof(PLAYOUTS) / sizeof(PLAYOUTS[0])))\n";
fs.writeFileSync(path.join(ROOT, "frontend/src/padlayouts.h"),out);
