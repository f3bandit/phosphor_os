/* Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md). */
/* SS1 Tool's controller layouts and their drawing (its web/index.html: PSH, PTL, ptSVG), reused as they are.
   PTL: each layout's items [type, control, x, y, size, height, label] on a 600 x 345 drawing. */
"use strict";
const PSH=[["t","l2",110,22,90,26,"L2"],["t","r2",400,22,90,26,"R2"],["r","l1",105,58,100,20,"L1"],["r","r1",395,58,100,20,"R1"]];
const PTL={
 generic:{n:"Generic (every input)",g:"Modern",b:"pad",it:[...PSH,["d","",150,160,26],["r","back",234,150,46,16,"Select"],["r","start",320,150,46,16,"Start"],["c","guide",300,112,15,0,"OSD"],
  ["s","ls",215,262,32,0,"L3"],["s","rs",385,262,32,0,"R3"],["c","x",415,140,19,0,"X"],["c","y",460,140,19,0,"Y"],["c","z",505,140,19,0,"Z"],["c","a",425,190,19,0,"A"],["c","b",470,190,19,0,"B"],["c","c",515,190,19,0,"C"]]},
 xbox:{n:"Xbox-style",g:"Modern",b:"pad",it:[["t","l2",110,22,90,26,"LT"],["t","r2",400,22,90,26,"RT"],["r","l1",105,58,100,20,"LB"],["r","r1",395,58,100,20,"RB"],["s","ls",150,160,32,0,"LS"],["d","",225,248,24],["s","rs",375,252,32,0,"RS"],
  ["c","back",255,158,11,0,"View"],["c","start",345,158,11,0,"Menu"],["c","guide",300,115,16,0,"Guide"],["c","y",470,118,18,0,"Y"],["c","b",510,158,18,0,"B"],["c","a",470,198,18,0,"A"],["c","x",430,158,18,0,"X"]]},
 dualshock:{n:"PlayStation (DualShock)",g:"Modern",b:"pad",it:[...PSH,["d","",150,160,26],["r","back",246,150,42,14,"Select"],["r","start",312,150,42,14,"Start"],["c","guide",300,192,10,0,"PS"],
  ["s","ls",215,258,32,0,"L3"],["s","rs",385,258,32,0,"R3"],["c","y",465,118,18,0,"△"],["c","b",505,158,18,0,"○"],["c","a",465,198,18,0,"✕"],["c","x",425,158,18,0,"□"]]},
 switchpro:{n:"Switch Pro-style",g:"Modern",b:"pad",it:[["t","l2",110,22,90,26,"ZL"],["t","r2",400,22,90,26,"ZR"],["r","l1",105,58,100,20,"L"],["r","r1",395,58,100,20,"R"],["s","ls",150,160,32,0,"LS"],["d","",225,248,24],["s","rs",375,252,32,0,"RS"],
  ["c","back",250,128,10,0,"−"],["c","start",350,128,10,0,"+"],["c","guide",330,176,11,0,"⌂"],["c","y",470,118,18,0,"X"],["c","b",510,158,18,0,"A"],["c","a",470,198,18,0,"B"],["c","x",430,158,18,0,"Y"]]},
 nes:{n:"NES / Famicom",g:"Retro",b:"brick",it:[["d","",130,200,26],["r","back",250,210,44,14,"Select"],["r","start",320,210,44,14,"Start"],["c","a",440,210,24,0,"B"],["c","b",510,210,24,0,"A"]]},
 snes:{n:"SNES / Super Famicom",g:"Retro",b:"bone",it:[["r","l1",95,72,110,20,"L"],["r","r1",395,72,110,20,"R"],["d","",150,195,26],["r","back",245,208,42,14,"Select"],["r","start",313,208,42,14,"Start"],
  ["c","y",470,150,19,0,"X"],["c","b",512,195,19,0,"A"],["c","a",470,240,19,0,"B"],["c","x",428,195,19,0,"Y"]]},
 pce:{n:"PC Engine / TurboGrafx-16",g:"Retro",b:"brick",it:[["d","",130,200,26],["r","back",250,218,42,14,"Select"],["r","start",312,218,42,14,"Run"],["c","a",440,205,22,0,"II"],["c","b",505,205,22,0,"I"]]},
 gen3:{n:"Genesis / Mega Drive (3-button)",g:"Retro",b:"bone",it:[["d","",150,195,28],["r","start",278,150,44,14,"Start"],["c","a",410,218,22,0,"A"],["c","b",460,203,22,0,"B"],["c","c|r1",510,188,22,0,"C"]]},
 gen6:{n:"Genesis / Mega Drive (6-button)",g:"Retro",b:"bone",it:[["d","",150,195,28],["r","start",278,150,44,14,"Start"],["r","back|guide",405,100,50,12,"Mode"],
  ["c","x",398,180,18,0,"X"],["c","y",446,166,18,0,"Y"],["c","z|l1",494,152,18,0,"Z"],["c","a",410,232,20,0,"A"],["c","b",460,218,20,0,"B"],["c","c|r1",510,204,20,0,"C"]]},
 neogeo:{n:"Neo Geo CD pad",g:"Retro",b:"bone",it:[["d","",150,195,28],["r","back",248,140,40,14,"Select"],["r","start",310,140,40,14,"Start"],["c","a",400,228,19,0,"A"],["c","b",444,206,19,0,"B"],["c","x",488,188,19,0,"C"],["c","y",532,172,19,0,"D"]]},
 jaguar:{n:"Atari Jaguar",g:"Retro",b:"bone",it:[["d","",150,180,28],["r","back",248,140,40,14,"Pause"],["r","start",310,140,40,14,"Option"],["c","c|r1",420,195,20,0,"C"],["c","b",465,180,20,0,"B"],["c","a",510,165,20,0,"A"]]},
 saturn:{n:"Sega Saturn",g:"Retro",b:"bone",it:[["r","l2",95,72,110,20,"L"],["r","r2",395,72,110,20,"R"],["d","",150,195,28],["c","start",300,215,11,0,"Start"],
  ["c","x",398,175,18,0,"X"],["c","y",446,161,18,0,"Y"],["c","z|l1",494,147,18,0,"Z"],["c","a",410,228,20,0,"A"],["c","b",460,214,20,0,"B"],["c","c|r1",510,200,20,0,"C"]]},
 psx:{n:"PlayStation (digital)",g:"Retro",b:"pad",it:[...PSH,["d","",150,165,26],["r","back",246,150,42,14,"Select"],["r","start",312,150,42,14,"Start"],
  ["c","y",465,123,18,0,"△"],["c","b",505,163,18,0,"○"],["c","a",465,203,18,0,"✕"],["c","x",425,163,18,0,"□"]]},
 n64:{n:"Nintendo 64",g:"Retro",b:"n64",it:[["r","l1",110,72,90,18,"L"],["r","r1",400,72,90,18,"R"],["r","l2",262,226,76,16,"Z"],["d","",130,165,24],["c","start",300,160,12,0,"Start"],["s","ls",300,272,24,0,""],
  ["c","a",450,212,17,0,"A"],["c","b|x",412,180,17,0,"B"],["c","rsu",500,128,11,0,"C↑"],["c","rsd",500,180,11,0,"C↓"],["c","rsl",474,154,11,0,"C←"],["c","rsr",526,154,11,0,"C→"]]},
 dreamcast:{n:"Dreamcast",g:"Retro",b:"pad",it:[["t","l2",110,22,90,26,"L"],["t","r2",400,22,90,26,"R"],["s","ls",150,150,30,0,""],["d","",210,248,24],["c","start",300,215,10,0,"Start"],
  ["c","y",465,128,17,0,"Y"],["c","b",505,168,17,0,"B"],["c","a",465,208,17,0,"A"],["c","x",425,168,17,0,"X"]]},
 gamecube:{n:"GameCube",g:"Retro",b:"pad",it:[["t","l2",110,22,90,26,"L"],["t","r2",400,22,90,26,"R"],["r","r1",400,58,90,18,"Z"],["s","ls",150,150,30,0,""],["d","",215,250,22],["s","rs",385,258,24,0,"C"],["c","start",300,150,10,0,"Start"],
  ["c","a",470,178,24,0,"A"],["c","b",425,215,14,0,"B"],["c","x",522,165,15,0,"X"],["c","y",462,132,14,0,"Y"]]},
 fight6:{n:"6-button fight pad (8BitDo M30, Retro-bit style)",g:"Fight",b:"pad",it:[["r","l2",105,58,100,20,"L"],["r","r2",395,58,100,20,"R"],["d","",150,170,26],["r","back",240,150,40,14,"Select"],["r","start",320,150,40,14,"Start"],["c","guide",300,118,12,0,"Home"],
  ["c","x",395,150,19,0,"X"],["c","y",442,140,19,0,"Y"],["c","z|l1",489,140,19,0,"Z"],["c","a",405,200,19,0,"A"],["c","b",452,190,19,0,"B"],["c","c|r1",499,190,19,0,"C"]]},
 fight8:{n:"8-button fight pad (Hori Fighting Commander, PowerA Fusion style)",g:"Fight",b:"pad",it:[["d","",150,170,26],["r","back",236,150,40,14,"Select"],["r","start",316,150,40,14,"Start"],["c","guide",300,118,12,0,"Home"],
  ["c","x",380,150,18,0,"X"],["c","y",422,140,18,0,"Y"],["c","r1",464,140,18,0,"RB"],["c","l1",506,146,18,0,"LB"],["c","a",388,196,18,0,"A"],["c","b",430,186,18,0,"B"],["c","r2",472,186,18,0,"RT"],["c","l2",514,192,18,0,"LT"]]},
 leverless:{n:"Leverless / all-button (Haute42, Hit Box, Snack Box style)",g:"Fight",b:"box",it:[["c","left",75,190,22,0,"←"],["c","down",130,175,22,0,"↓"],["c","right",188,190,22,0,"→"],["c","up",300,290,24,0,"↑"],
  ["c","back",50,92,10,0,"Sel"],["c","start",85,92,10,0,"St"],["c","guide",120,92,10,0,"Home"],["c","l3",515,92,10,0,"L3"],["c","r3",550,92,10,0,"R3"],
  ["c","x",290,172,22,0,"X"],["c","y",345,152,22,0,"Y"],["c","r1",400,152,22,0,"RB"],["c","l1",455,162,22,0,"LB"],["c","a",300,227,22,0,"A"],["c","b",355,207,22,0,"B"],["c","r2",410,207,22,0,"RT"],["c","l2",465,217,22,0,"LT"]]},
 stick8:{n:"Arcade stick, 8 buttons (Mayflash F300/F500, Hori RAP style)",g:"Fight",b:"box",it:[["l","",140,205,55],["c","back",50,92,10,0,"Sel"],["c","start",85,92,10,0,"St"],["c","guide",120,92,10,0,"Home"],
  ["c","x",310,175,22,0,"X"],["c","y",365,155,22,0,"Y"],["c","r1",420,155,22,0,"RB"],["c","l1",475,165,22,0,"LB"],["c","a",320,230,22,0,"A"],["c","b",375,210,22,0,"B"],["c","r2",430,210,22,0,"RT"],["c","l2",485,220,22,0,"LT"]]},
 stick6:{n:"Arcade stick, 6 buttons (Astro City style)",g:"Fight",b:"box",it:[["l","",150,205,55],["c","back",50,92,10,0,"Sel"],["c","start",85,92,10,0,"St"],["c","guide",120,92,10,0,"Home"],
  ["c","x",335,175,24,0,"X"],["c","y",395,160,24,0,"Y"],["c","r1",455,160,24,0,"RB"],["c","a",345,235,24,0,"A"],["c","b",405,220,24,0,"B"],["c","r2",465,220,24,0,"RT"]]}};
function ptSVG(k){const L=PTL[k];let s=`<svg viewBox="0 0 600 345" role="img" aria-label="${esc(L.n)}">`;
 for(const[t,c,x,y,a,b,lab]of L.it){const sm=(lab||"").length>2?" sm":"";s+=`<g class="ptg">`;
  if(t==="c")s+=`<circle class="ptc" data-c="${c}" cx="${x}" cy="${y}" r="${a}"/><text class="ptl${sm}" x="${x}" y="${a<14&&(lab||"").length>1?y+a+10:y}">${lab||""}</text>`;
  else if(t==="r")s+=`<rect class="ptc" data-c="${c}" x="${x}" y="${y}" width="${a}" height="${b}" rx="${Math.min(b/2,9)}"/><text class="ptl${sm}" x="${x+a/2}" y="${y+b/2}">${lab||""}</text>`;
  else if(t==="t")s+=`<rect class="ptc" data-c="${c}" x="${x}" y="${y}" width="${a}" height="${b}" rx="8"/><rect class="ptfill" data-f="${c}" x="${x+3}" y="${y+3}" width="0" height="${b-6}" rx="5"/><text class="ptl" x="${x+a/2}" y="${y+b/2}">${lab}</text>`;
  else if(t==="s")s+=`<circle class="ptc" data-c="${c}" cx="${x}" cy="${y}" r="${a}"/><circle class="ptknob" data-k="${c}" data-x="${x}" data-y="${y}" data-r="${a*.55}" cx="${x}" cy="${y}" r="${a*.42}"/><text class="ptl sm" x="${x}" y="${y}">${lab||""}</text>`;
  else if(t==="d"){const w=a*.9,h=w/2,e=a+h,P=(dx,dy)=>`${x+dx},${y+dy}`;
   // one cross outline (no overlapping edges); each arm is a fill-only zone inside it
   s+=`<polygon class="ptdbase" points="${[P(-h,-e),P(h,-e),P(h,-h),P(e,-h),P(e,h),P(h,h),P(h,e),P(-h,e),P(-h,h),P(-e,h),P(-e,-h),P(-h,-h)].join(" ")}"/><rect class="ptarm" data-c="dpad" x="${x-h}" y="${y-h}" width="${w}" height="${w}"/>`
    +`<rect class="ptarm" data-c="up" x="${x-h}" y="${y-e}" width="${w}" height="${e-h}"/><rect class="ptarm" data-c="down" x="${x-h}" y="${y+h}" width="${w}" height="${e-h}"/>`
    +`<rect class="ptarm" data-c="left" x="${x-e}" y="${y-h}" width="${e-h}" height="${w}"/><rect class="ptarm" data-c="right" x="${x+h}" y="${y-h}" width="${e-h}" height="${w}"/>`
    +`<polygon class="ptc ptcross" data-c="dpad" points="${[P(-h,-e),P(h,-e),P(h,-h),P(e,-h),P(e,h),P(h,h),P(h,e),P(-h,e),P(-h,h),P(-e,h),P(-e,-h),P(-h,-h)].join(" ")}" stroke-linejoin="round"/>`}
  else if(t==="l")s+=`<circle class="ptc" data-c="lever" cx="${x}" cy="${y}" r="${a}"/><circle class="ptknob" data-k="lever" data-x="${x}" data-y="${y}" data-r="${a*.5}" cx="${x}" cy="${y}" r="${a*.38}"/>`;
  s+="</g>"}
 return s+"</svg>"}

/* Phosphor's additions: controllers drawn as they really are (positions from their GP2040-CE board
   layout, configs/<board>/BoardConfig.h, scaled to the 600 x 345 drawing) */
PTL.haute42m = { n: "Haute42 COSMOX M Ultra", g: "Fight", b: "box", it: [
	["c", "turbo", 370, 20, 9, 0, "T"], ["c", "a2", 412, 20, 9, 0, "-"], ["c", "guide", 456, 20, 9, 0, "Hm"],
	["c", "back", 498, 20, 9, 0, "Bk"], ["c", "start", 541, 20, 9, 0, "St"],
	["c", "up", 169, 91, 29, 0, "M1"], ["c", "l3", 290, 91, 29, 0, "M2"],
	["c", "left", 93, 156, 29, 0, "←"], ["c", "down", 155, 153, 29, 0, "↓"], ["c", "right", 212, 179, 29, 0, "→"],
	["c", "x", 283, 153, 29, 0, "X"], ["c", "y", 340, 129, 29, 0, "Y"], ["c", "r1", 402, 129, 29, 0, "RB"], ["c", "l1", 463, 141, 29, 0, "LB"],
	["c", "a", 278, 217, 29, 0, "A"], ["c", "b", 334, 191, 29, 0, "B"], ["c", "r2", 396, 191, 29, 0, "RT"], ["c", "l2", 456, 204, 29, 0, "LT"],
	["c", "l3", 180, 292, 29, 0, "LS"], ["c", "up", 244, 308, 31, 0, "↑"], ["c", "r3", 308, 292, 29, 0, "RS"]] };

/* Phosphor: light a drawing up. st: { has: [controls] (null: draw everything), on: [controls pressed],
   ls/rs: [x, y] -1..1, l2/r2: 0..1, target: the control asked for (glows), done: [controls mapped] } */
function padPaint(el, st) {
	const inL = (l, n) => !!l && n.split("|").some(x => l.includes(x));
	const has = n => !st.has || inL(st.has, n) || (/^rs[udlr]$/.test(n) && st.has.includes("rsx"));
	const rsd = n => { const r = st.rs || [0, 0]; return ({ u: r[1] < -.5, d: r[1] > .5, l: r[0] < -.5, r: r[0] > .5 })[n[2]]; };
	el.querySelectorAll("[data-c]").forEach(e => {
		const c = e.dataset.c;
		let h, on, tg = !!st.target && inL([st.target], c), dn = inL(st.done, c);
		if (c === "dpad") { h = has("up") || has("left"); on = false; tg = ["up", "down", "left", "right"].includes(st.target); }
		else if (c === "lever") { h = true; on = false; tg = ["up", "down", "left", "right"].includes(st.target); }
		else if (c === "ls" || c === "rs") { h = has(c + "x"); on = inL(st.on, c === "ls" ? "l3" : "r3"); }
		else if (/^rs[udlr]$/.test(c)) { h = has(c); on = rsd(c); }
		else { h = has(c); on = inL(st.on, c); }
		e.classList.toggle("no", !h); e.classList.toggle("on", !!on); e.classList.toggle("tgt", tg && !on); e.classList.toggle("done", dn && !on && !tg);
	});
	el.querySelectorAll("[data-f]").forEach(e => {
		const n = e.dataset.f, v = n === "l2" ? st.l2 || 0 : n === "r2" ? st.r2 || 0 : 0, base = e.parentNode.querySelector(`[data-c="${n}"]`);
		e.setAttribute("width", Math.max(0, Math.min(1, inL(st.on, n) ? Math.max(v, 1) : v)) * (+base.getAttribute("width") - 6));
	});
	el.querySelectorAll("[data-k]").forEach(e => {
		const k = e.dataset.k, r = +e.dataset.r;
		let x = 0, y = 0;
		if (k === "lever") { x = (inL(st.on, "right") ? 1 : 0) - (inL(st.on, "left") ? 1 : 0); y = (inL(st.on, "down") ? 1 : 0) - (inL(st.on, "up") ? 1 : 0); if (!x && !y && st.ls) [x, y] = st.ls; }
		else [x, y] = st[k] || [0, 0];
		e.setAttribute("cx", +e.dataset.x + x * r); e.setAttribute("cy", +e.dataset.y + y * r);
		e.classList.toggle("on", Math.hypot(x, y) > .3 || inL(st.on, k === "ls" ? "l3" : k === "rs" ? "r3" : ""));
	});
}
/* the drawings in the order the TV steps through them (Automatic first) */
const PAD_ORDER = Object.keys(PTL);
