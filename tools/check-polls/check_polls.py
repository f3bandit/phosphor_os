#!/usr/bin/env python3
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
"""The menu must never wait for phosphord on something it asks again and again.

phosphord answers one request at a time. When the shell asks it something from a path that runs over and
over (the main loop's timers, a screen's tick, the status bar, a list's fill callback, the Settings info
panel), with api_call / api_call_slow / api_call_json, the menu stands still until the answer comes: if
phosphord is busy with something else (the Windows app's request, a slow job), the cursor stops and the
presses all arrive at once. That was the menu lag fixed on Oct 29 2026 (commit 75979ae): repeated polls
go through api_bg_ask / api_bg_take, a thread with its own connection, and the screen takes the newest answer.

This finds every synchronous call reachable from those repeating paths in frontend/src/*.c:
  roots  the function with the main loop's timers (it calls status_update every 2 s), status_update,
         si_update, every function named *_tick / *_poll, and every vlist .fill callback
  calls  followed through the shell's own functions (a function called from a root, and so on)
A finding fails the check unless its line (or the line above) says why it's fine:
  /* poll-ok: <why> */     e.g. it only runs once, on a change, or after the user pressed something
or it's in check_polls_baseline.txt (the ones that were there when this check was added: each one is
"<function>: <the line's text>", to be moved to api_bg_ask one by one). New screens and settings must not add any.

  check_polls.py [--update-baseline] [src_dir]
"""
import os
import re
import sys

SYNC = ("api_call", "api_call_slow", "api_call_json")
HERE = os.path.dirname(os.path.abspath(__file__))
BASELINE = os.path.join(HERE, "check_polls_baseline.txt")

FUNC_RE = re.compile(r"^(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\(([^;{]*)\)\s*\n?\{", re.M)
CALL_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "do", "else"}


def strip_comments_keep_lines(src):
    # comments out (their newlines kept, so line numbers stay), but poll-ok markers remembered per line
    ok_lines = set()
    out = []
    i, line = 0, 1
    n = len(src)
    while i < n:
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            c = src[i:j]
            if "poll-ok" in c:
                ok_lines.add(line)
            out.append("\n" * c.count("\n"))
            line += c.count("\n")
            i = j
        elif src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
            if "poll-ok" in src[i:j]:
                ok_lines.add(line)
            i = j
        elif src[i] == '"':
            j = i + 1
            while j < n and src[j] != '"':
                j += 2 if src[j] == "\\" else 1
            out.append('""')
            i = j + 1
        elif src[i] == "'":
            j = i + 1
            while j < n and src[j] != "'":
                j += 2 if src[j] == "\\" else 1
            out.append("''")
            i = j + 1
        else:
            if src[i] == "\n":
                line += 1
            out.append(src[i])
            i += 1
    return "".join(out), ok_lines


def functions(code):
    # name -> (start line, body text); a body is the braces after the signature, matched by depth
    found = {}
    for m in FUNC_RE.finditer(code):
        name = m.group(1)
        if name in KEYWORDS:
            continue
        start = m.end() - 1
        depth, i = 0, start
        while i < len(code):
            if code[i] == "{":
                depth += 1
            elif code[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        line = code.count("\n", 0, start) + 1
        found.setdefault(name, (line, code[start:i + 1]))
    return found


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    update = "--update-baseline" in sys.argv
    src_dir = args[0] if args else os.path.join(HERE, "..", "..", "frontend", "src")
    funcs, okl, fill_cbs, raws = {}, {}, set(), {}
    for f in sorted(os.listdir(src_dir)):
        if not f.endswith(".c"):
            continue
        raw = open(os.path.join(src_dir, f), encoding="utf-8", errors="replace").read()
        code, ok_lines = strip_comments_keep_lines(raw)
        raws[f] = raw.split("\n")
        for name, (line, body) in functions(code).items():
            funcs.setdefault(name, (f, line, body))
        okl[f] = ok_lines
        fill_cbs.update(re.findall(r"\.fill\s*=\s*([A-Za-z_]\w*)\s*;", code))

    roots = set(fill_cbs) | {"status_update", "si_update"}
    roots |= {n for n in funcs if n.endswith("_tick") or n.endswith("_poll")}
    roots |= {n for n, (_, _, b) in funcs.items() if "last_status" in b and "status_update" in b}

    # the functions those call, all the way down (the visited set stops loops): what a tick does each time
    # round. Two levels used to be the limit, and a call three helpers deep slipped through. Not followed: what only
    # happens once, on a change or a press (a screen opened, a key handled, a job started or asked about)
    ONE_OFF = re.compile(r"^(show_|open_)|_(key|ask|start|begin|run|do|open|close|apply|take|init|create|build|load|save|"
                         r"check|confirm|install|uninstall|pair|play|launch)$")
    via = {}
    todo = [(r, r, 0) for r in sorted(roots) if r in funcs]
    while todo:
        fn, root, depth = todo.pop()
        if fn in via:
            continue
        via[fn] = root
        for c in CALL_RE.findall(funcs[fn][2]):
            if c in funcs and c not in via and c not in SYNC and not ONE_OFF.search(c):
                todo.append((c, root, depth + 1))

    findings = []
    for fn in sorted(via):
        f, line0, body = funcs[fn]
        for m in re.finditer(r"\b(" + "|".join(SYNC) + r")\s*\(", body):
            ln = line0 + body.count("\n", 0, m.start())
            if ln in okl[f] or ln - 1 in okl[f]:
                continue
            findings.append((f, ln, fn, m.group(1), via[fn]))

    # a finding is known by its function and its line's own text (so a new call in a function that had one
    # already, the main loop's polls above all, is still new)
    def key(x):
        f, ln, fn, call, _ = x
        text = re.sub(r"\s+", " ", raws[f][ln - 1].strip())
        return f"{fn}: {text}"
    keys = sorted({key(x) for x in findings})
    if update:
        with open(BASELINE, "w") as o:
            o.write("# check_polls.py: synchronous phosphord calls on repeating paths that were there when the check\n")
            o.write("# was added (Oct 31 2026), as '<function>: <its line>'. Move each to api_bg_ask / api_bg_take, then take its line out.\n")
            for k in keys:
                o.write(k + "\n")
        print(f"baseline: {len(keys)} entries")
        return 0
    base = set()
    if os.path.exists(BASELINE):
        base = {l.strip() for l in open(BASELINE) if l.strip() and not l.startswith("#")}
    new = [x for x in findings if key(x) not in base]
    for f, ln, fn, call, root in new:
        print(f"{f}:{ln}: {call}() in {fn}(), which runs over and over (from {root}): the menu waits for phosphord "
              f"there. Use api_bg_ask / api_bg_take, or say why it's fine with /* poll-ok: ... */")
    gone = sorted(base - set(keys))
    if gone:
        print("baseline entries no longer found (take them out of check_polls_baseline.txt): " + ", ".join(gone))
    print(f"check_polls: {len(via)} functions on repeating paths, {len(findings)} synchronous calls "
          f"({len(findings) - len(new)} in the baseline), {len(new)} new")
    return 1 if new else 0


if __name__ == "__main__":
    sys.exit(main())
