#!/usr/bin/env python3
# The "Circuit board" background (texture.c, the app's texture.js): one whole screen of random
# traces winding like a maze on an 8 px grid (160 x 90 cells on the 1280x720 base screen), 45-degree
# bends, vias at the ends, never crossing, never turning back on themselves.
# Laid out once here (a fixed seed) and kept as data, so the TV and the app draw the same board.
#   python3 frontend/tools/gen_circuit.py   -> frontend/src/circuit_data.h, apps/windows/web/circuit.js
import os, random
GW, GH = 160, 90
SEED = 5
DIRS = [(1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1)]

def build(seed, attempts=20000, lmin=6, lmax=70, turn=0.22, branch=0.0):
    rnd = random.Random(seed); occ = set(); traces = []; vias = []; joints = []
    def inb(x, y): return 0 <= x < GW and 0 <= y < GH
    def free(x, y, own, ok):
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                c = (x + dx, y + dy)
                if c in ok: continue
                if c in occ or c in own: return False
        return True
    def walk(x, y, d, L, ok0):
        path = [(x, y)]; own = {(x, y)}; straight = 0; d0 = d
        for s in range(L):
            if d % 2 == 1: opts = [(d + rnd.choice([1, -1])) % 8, d] if straight >= rnd.randint(1, 3) else [d]
            else: opts = [d] if straight < 3 or rnd.random() > turn else [(d + rnd.choice([1, -1])) % 8, d]
            opts += [(d + k) % 8 for k in (1, -1) if (d + k) % 8 not in opts]
            opts = [o for o in opts if min((o - d0) % 8, (d0 - o) % 8) <= 2]   # never turns back
            moved = False
            for nd in opts:
                dx, dy = DIRS[nd]; nx, ny = x + dx, y + dy
                ok = set(path[-2:]) | (ok0 if len(path) <= 2 else set())
                if not inb(nx, ny) or (nx, ny) in own or not free(nx, ny, own, ok): continue
                if dx and dy and not (free(x + dx, y, own, ok) and free(x, y + dy, own, ok)): continue
                x, y = nx, ny; path.append((x, y)); own.add((x, y))
                straight = straight + 1 if nd == d else 0; d = nd; moved = True; break
            if not moved: break
        return path, own
    def corners(path):
        pts = [path[0]]
        for i in range(1, len(path) - 1):
            a, b, c = path[i - 1], path[i], path[i + 1]
            if (b[0] - a[0], b[1] - a[1]) != (c[0] - b[0], c[1] - b[1]): pts.append(b)
        pts.append(path[-1]); return pts
    def grow(path, depth):
        if depth > 2 or branch <= 0: return
        for i in range(3, len(path) - 3):
            if rnd.random() > branch: continue
            (ax, ay), (bx, by) = path[i - 1], path[i]
            pd = DIRS.index((bx - ax, by - ay))
            if pd % 2: continue
            bd = (pd + rnd.choice([2, -2, 1, -1])) % 8
            dx, dy = DIRS[bd]; sx, sy = bx + dx, by + dy
            ok0 = set(path[max(0, i - 2):i + 3])
            if not inb(sx, sy) or not free(sx, sy, set(), ok0): continue
            p, o = walk(sx, sy, bd, rnd.randint(lmin, lmax // 2), ok0)
            if len(p) < 4: continue
            occ.update(o)
            traces.append(corners([(bx, by)] + p)); vias.append(p[-1]); joints.append((bx, by))
            grow(p, depth + 1)
    for a in range(attempts):
        x, y = rnd.randrange(GW), rnd.randrange(GH)
        if not free(x, y, set(), set()): continue
        d = rnd.randrange(8) & ~1 if rnd.random() < 0.8 else rnd.randrange(8)
        p, o = walk(x, y, d, rnd.randint(lmin, lmax), set())
        if len(p) < lmin: continue
        occ.update(o); traces.append(corners(p)); vias += [p[0], p[-1]]
        grow(p, 0)
    return traces, vias, joints

def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
    traces, vias, joints = build(SEED)
    pts, idx = [], []
    for t in traces:
        idx.append((len(pts), len(t))); pts += t
    flat = lambda l: ",".join(f"{x},{y}" for x, y in l)
    def wrap(s, n=24):
        v = s.split(","); return ",\n\t".join(",".join(v[i:i + n]) for i in range(0, len(v), n))
    with open(os.path.join(root, "frontend/src/circuit_data.h"), "w") as f:
        f.write("/* The \"Circuit board\" background's layout: made by frontend/tools/gen_circuit.py, don't edit.\n"
                " * Cells of an 8 px grid on the 1280x720 base screen (160 x 90). */\n")
        f.write(f"#define CIRCUIT_W {GW}\n#define CIRCUIT_H {GH}\n#define CIRCUIT_CELL 8\n")
        f.write(f"static const unsigned char CIRCUIT_PTS[] = {{\n\t{wrap(flat(pts))}\n}};\n")
        f.write(f"static const unsigned short CIRCUIT_TRACES[] = {{\n\t{wrap(','.join(f'{a},{n}' for a, n in idx))}\n}};   /* first point, points */\n")
        f.write(f"static const unsigned char CIRCUIT_VIAS[] = {{\n\t{wrap(flat(vias))}\n}};\n")
        f.write(f"static const unsigned char CIRCUIT_JOINTS[] = {{\n\t{wrap(flat(joints))}\n}};\n")
    with open(os.path.join(root, "apps/windows/web/circuit.js"), "w") as f:
        f.write("/* The \"Circuit board\" background's layout (the console's circuit_data.h): made by\n"
                "   frontend/tools/gen_circuit.py, don't edit. Cells of an 8 px grid on 1280x720 (160 x 90). */\n\"use strict\";\n")
        f.write(f"const CIRCUIT = {{ w: {GW}, h: {GH}, cell: 8,\n\tpts: [{flat(pts)}],\n\ttraces: [{','.join(f'{a},{n}' for a, n in idx)}],\n"
                f"\tvias: [{flat(vias)}],\n\tjoints: [{flat(joints)}] }};\n")
    print(f"{len(traces)} traces, {len(pts)} points, {len(vias)} vias, {len(joints)} branches")
main()
