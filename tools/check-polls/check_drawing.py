#!/usr/bin/env python3
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
"""Drawing and lifetime rules for the TV's shell (frontend/src/*.c), checked on every change (GitHub Actions
"Shell checks"). Each one is a mistake that made it into a release (docs/menu-rules.md has the why):

  deferred   A timer, animation or async call runs later, maybe after its screen was cleared. Its callback
             must check the screen is still showing (cur == SCR_... / cur != SCR_...), or be stopped when its
             objects go (lv_anim_delete / lv_timer_delete in an LV_EVENT_DELETE callback), or say why it's
             fine: /* deferred-ok: why */. (0.27.65: the Home cards' slide ran on deleted cards: a crash.)
  see-through  A background that's partly see-through (bg_opa between 0 and 255), an object faded with
             style_opa, or a see-through (ARGB8888) tile: everything under it is drawn too and then blended,
             7x a plain fill on the SS1 (20 ns a pixel against 2.75). Only for something small or short-lived,
             with /* opa-ok: why */. (0.27.65: the background texture blended on every redraw: the menu lag.)

  same-text  Code that runs over and over (the main loop's timers, *_tick / *_poll, the status bar, list fill
             callbacks, and what they call, *_apply included) sets a label with label_set_if (only when the
             text is different), not lv_label_set_text: the same text still repaints the label, and a status
             answer every second or a controller 16 times a second kept whole panels repainting with nobody
             touching the menu (the controller test, Front ports, Nov 1 2026). /* text-ok: why */
  rebuild    Code that runs over and over clears and rebuilds widgets (lv_obj_clean) only when what they show
             changed: Get box art rebuilt its systems list twice a second, 7 screens' worth of repainting a
             second with nobody touching it (Nov 1 2026). Compare first (sysrows_show), or /* rebuild-ok: why */
  build-first  A screen's show_ function sets cur = SCR_... and then clears the body: a call in between that
             can redraw widgets (a vlist_refresh, lv_obj_/lv_label_ calls, reached through the screen's update
             functions, which pass their cur check by then) redraws the last visit's widgets, already deleted.
             Build the screen first, then ask. /* build-ok: why */ (Discs, Nov 1 2026: a crash.)

A marker goes on the line or the line above (for a callback: its create call or the callback itself).

  check_drawing.py [src_dir]
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_polls import functions, strip_comments_keep_lines  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SCREEN_GUARD = re.compile(r"\bcur\s*[!=]=|[!=]=\s*cur\b|\bui_screen\s*\(")
SOLID = {"LV_OPA_COVER", "LV_OPA_TRANSP", "LV_OPA_0", "LV_OPA_100", "0", "255"}


def marked(raw_lines, ln, word):
    return any(word in raw_lines[i - 1] for i in (ln, ln - 1) if 0 < i <= len(raw_lines))


# what runs over and over: the same roots as check_polls.py, followed into *_apply / *_take too (they redraw
# what an answer says), not into what only runs on a press or once (a screen opened, a job started)
ONCE = re.compile(r"^(show_|open_)|_(key|start|begin|run|do|open|close|init|create|build|install|uninstall|pair|"
                  r"play|launch|confirm|save)$")


def repeating(src_dir):
    funcs, fill_cbs = {}, set()
    for f in sorted(os.listdir(src_dir)):
        if not f.endswith(".c") or f == "prof.c":
            continue
        code, _ = strip_comments_keep_lines(open(os.path.join(src_dir, f), encoding="utf-8", errors="replace").read())
        for name, (line, body) in functions(code).items():
            funcs.setdefault(name, (f, line, body))
        fill_cbs.update(re.findall(r"\.fill\s*=\s*([A-Za-z_]\w*)\s*;", code))
    roots = set(fill_cbs) | {"status_update", "si_update", "ui_tick"}
    roots |= {n for n in funcs if n.endswith("_tick") or n.endswith("_poll")}
    seen, todo = set(), [r for r in roots if r in funcs]
    while todo:
        fn = todo.pop()
        if fn in seen:
            continue
        seen.add(fn)
        for c in re.findall(r"\b([A-Za-z_]\w*)\s*\(", funcs[fn][2]):
            if c in funcs and c not in seen and not ONCE.search(c):
                todo.append(c)
    return seen


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    src_dir = args[0] if args else os.path.join(HERE, "..", "..", "frontend", "src")
    problems = []
    nd = no = nt = 0
    rep = repeating(src_dir)
    for f in sorted(os.listdir(src_dir)):
        if not f.endswith(".c") or f == "prof.c":
            continue
        raw = open(os.path.join(src_dir, f), encoding="utf-8", errors="replace").read()
        rl = raw.split("\n")
        code, _ = strip_comments_keep_lines(raw)
        funcs = functions(code)

        def line_of(pos):
            return code.count("\n", 0, pos) + 1

        # callbacks stopped when their objects go: lv_anim_delete(.., cb) / lv_timer_delete in a DELETE callback
        stopped = set()
        for m in re.finditer(r"lv_obj_add_event_cb\s*\([^;]*?,\s*([A-Za-z_]\w*)\s*,\s*LV_EVENT_DELETE", code):
            body = funcs.get(m.group(1), (0, ""))[1]
            stopped |= set(re.findall(r"lv_anim_delete\s*\([^;]*?,\s*([A-Za-z_]\w*)\s*\)", body))
            if "lv_timer_delete" in body:
                stopped.add("*timer*")

        # deferred callbacks
        for m in re.finditer(r"\b(lv_timer_create|lv_anim_set_exec_cb|lv_anim_set_completed_cb|lv_async_call)\s*\(([^;]*?)\)\s*;", code):
            call, a = m.group(1), m.group(2)
            parts = [p.strip() for p in a.split(",")]
            cb = parts[1] if call.startswith("lv_anim") else parts[0]
            cb = re.sub(r"^\(.*?\)\s*", "", cb)
            if not re.fullmatch(r"[A-Za-z_]\w*", cb) or cb in ("NULL",):
                continue
            nd += 1
            ln = line_of(m.start())
            cl, body = funcs.get(cb, (0, ""))
            ok = (marked(rl, ln, "deferred-ok") or (cl and marked(rl, cl, "deferred-ok"))
                  or SCREEN_GUARD.search(body) or cb in stopped)
            if call.startswith("lv_anim") and not ok:
                # an animation on an object is stopped by LVGL when the object is deleted
                var = re.search(r"lv_anim_set_var\s*\(\s*&?\w+\s*,\s*([^)]+)\)", code[max(0, m.start() - 600):m.start()])
                if var and not var.group(1).strip().startswith("&"):
                    ok = True
            if not ok:
                problems.append(f"{f}:{ln}: {call}({cb}): {cb}() can run after its screen is gone. Check the screen "
                                f"(cur == SCR_...), stop it in an LV_EVENT_DELETE callback, or /* deferred-ok: why */")

        # see-through
        for m in re.finditer(r"lv_obj_set_style_(bg_opa|opa|bg_image_opa)\s*\(\s*[^,]+,\s*([^;]+?)\s*,\s*[^,;]+\)\s*;", code):
            vals = set(re.findall(r"\bLV_OPA_\w+|\b\d+\b|\b[a-z_]\w*\s*\(\s*\)", m.group(2)))
            vals = {v.replace(" ", "") for v in vals}
            if not vals or vals <= SOLID:
                continue
            no += 1
            ln = line_of(m.start())
            if not marked(rl, ln, "opa-ok"):
                problems.append(f"{f}:{ln}: lv_obj_set_style_{m.group(1)}(.., {m.group(2).strip()}): see-through, so "
                                f"everything under it is drawn and blended too. Make it solid, or /* opa-ok: why */")
        for m in re.finditer(r"lv_draw_buf_create\s*\([^;]*LV_COLOR_FORMAT_ARGB8888", code):
            no += 1
            ln = line_of(m.start())
            if not marked(rl, ln, "opa-ok"):
                problems.append(f"{f}:{ln}: an ARGB8888 (see-through) buffer: blended on every redraw. "
                                f"XRGB8888 with the colours mixed in once, or /* opa-ok: why */")
        # same-text: lv_label_set_text in what runs over and over
        for name, (fl, body) in funcs.items():
            if name not in rep or name == "label_set_if":
                continue
            for m in re.finditer(r"\blv_label_set_text\s*\(", body):
                nt += 1
                ln = fl + body.count("\n", 0, m.start())
                if marked(rl, ln, "text-ok"):
                    continue
                problems.append(f"{f}:{ln}: lv_label_set_text in {name}(), which runs over and over: the same text "
                                f"repaints it too. label_set_if(label, text), or /* text-ok: why */")

        # rebuild: lv_obj_clean in what runs over and over
        for name, (fl, body) in funcs.items():
            if name not in rep or name.startswith("show_"):
                continue
            for m in re.finditer(r"\blv_obj_clean\s*\(", body):
                ln = fl + body.count("\n", 0, m.start())
                if not marked(rl, ln, "rebuild-ok"):
                    problems.append(f"{f}:{ln}: lv_obj_clean in {name}(), which runs over and over: rebuilt each time, "
                                    f"repainted with nobody touching it. Rebuild only on a change, or /* rebuild-ok: why */")

        # build-first: show_* that call, between cur = SCR_ and lv_obj_clean(body), something that redraws
        def reach(fn, seen):
            if fn in seen or fn not in funcs:
                return
            seen.add(fn)
            for c in re.findall(r"\b([a-z_]\w*)\s*\(", funcs[fn][1]):
                reach(c, seen)
        for name, (fl, body) in funcs.items():
            if not name.startswith("show_"):
                continue
            k = re.search(r"\bcur\s*=\s*SCR_\w+\s*;", body)
            c = body.find("lv_obj_clean(body)")
            if not k or c < 0 or k.start() > c:
                continue
            for cm in re.finditer(r"\b([a-z_]\w*)\s*\(", body[k.end():c]):
                seen = set()
                reach(cm.group(1), seen)
                bad = sorted(g for g in seen if g not in ("toast_show", "plog") and
                             re.search(r"\bvlist_refresh\s*\(|\blv_(obj|label|image)_\w+\s*\(", funcs[g][1]))
                if not bad:
                    continue
                ln = fl + body.count("\n", 0, k.end() + cm.start())
                if marked(rl, ln, "build-ok"):
                    continue
                problems.append(f"{f}:{ln}: {name}() calls {cm.group(1)}() before it clears and builds the screen, and "
                                f"that can redraw widgets ({', '.join(bad[:3])}): the last visit's, deleted. Build first, "
                                f"or /* build-ok: why */")
    for p in problems:
        print(p)
    print(f"check_drawing: {nd} deferred callbacks, {no} see-through styles, {nt} labels set over and over, {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
