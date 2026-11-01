# Rules for the TV menu (frontend/)

Every screen, setting or redraw added to the shell follows these. Each one is a mistake that reached a release.
The checks that enforce them are in `tools/check-polls/` (README there). Run them before a release.

## Why the SS1 is different

- **The CPU is a dual Cortex-A9 at 800 MHz, with 512 KB of L2 cache shared by both cores.**
- **Memory bandwidth is the real limit.** Measured on an SS1 (Oct 31 2026, `fbbench`): RAM to RAM about
  430 MB/s, writing the framebuffer about 660 MB/s, reading it back 26 MB/s. A 1080p frame is 8 MB, so every
  pixel drawn twice costs real time.
- **Blending is about 7× a plain copy**, measured per pixel with the drawing profiler (`-DPHOS_PROFILE=ON`):

  | What is drawn | Cost per pixel |
  |---|---|
  | Solid fill | 2.75 ns |
  | Opaque image (XRGB8888) | 8–10 ns |
  | See-through fill | 20.6 ns |
  | Rounded corner (masked) | 40–45 ns |

## The rules

1. **Never wait for phosphord on something asked repeatedly.** Use `api_poll` / `api_bg_ask` / `api_bg_take`
   instead. Enforced by `check_polls.py` in CI.

2. **Draw opaque.** A see-through background (bg_opa between 0 and 255), an object faded with `style_opa`, or
   an ARGB8888 image makes LVGL draw everything underneath and then blend it. That costs 7× a plain fill, plus
   the drawing hidden underneath.
   - Mix the colour in once instead. The texture tile is pre-mixed into its background as XRGB8888, and
     decoded JPEGs are XRGB8888.
   - Keep see-through for small or short-lived things (a dimmed modal backdrop, a 32×32 icon), marked
     `/* opa-ok: why */`.
   - Enforced by `check_drawing.py` in CI.

3. **Set a style or position only when it changes.** Every `lv_obj_set_style_*`, `set_pos`, `set_size` or
   `label_set_text` repaints that object, even when the value is the same.
   - Code that runs on every update (a tick, a poll, input state 10–16 times a second) keeps the last value
     and returns early. See `padview_state`'s `look` / `knob_to` / `fillw`.
   - A screen nobody is touching should repaint almost nothing. Enforced on the SS1 by `menu_walk.py`'s idle
     check (`<tmp>/draw`).

4. **Delayed callbacks must not outlive their screen.** A timer, animation or `lv_async_call` can fire after
   another screen has cleared `body`. Each callback must do one of these:
   - check that its screen is still showing (`cur == SCR_...`);
   - be stopped in an `LV_EVENT_DELETE` callback on its objects (`shelf_gone_cb`);
   - be marked `/* deferred-ok: why */`.

   An animation whose `var` is an `lv_obj_t` is safe: LVGL deletes it with the object. Enforced by
   `check_drawing.py` in CI; `menu_walk.py`'s quick pass tries to trigger these crashes on the SS1.

5. **Set a label's text through `label_set_if`, never `lv_label_set_text`.** The same text set again still
   repaints the label. A status answer every second, or a controller 16 times a second, kept whole panels
   repainting with nobody touching the menu (the controller test at 156% of the screen a second, Front ports
   at 247%, Nov 1 2026). Every label in the shell goes through it; `check_drawing.py` flags a plain
   `lv_label_set_text` in code that runs over and over (`/* text-ok: why */` for a new label's first text).

6. **Build a screen before asking for anything that redraws it.** A `show_` function sets `cur = SCR_...`,
   which lets that screen's update functions through their `cur` check. A call before `lv_obj_clean(body)`
   and the new widgets can redraw the last visit's widgets, which were deleted when the screen was left.
   Discs did this and crashed when opened and left quickly (Nov 1 2026). Clear and build first, then ask.
   Enforced by `check_drawing.py` (`/* build-ok: why */`).

7. **Keep repaints small.** Change only the rows or labels that changed (the `vlist` does this). Don't
   invalidate a whole panel to update one number.

## Before a release

- `python3 tools/check-polls/check_polls.py` and `python3 tools/check-polls/check_drawing.py` (CI runs both).
- On the SS1, `python3 menu_walk.py --cold`. It should report 0 stalls, no idle repaints and no crashes.
  Connect a controller first so the controller test is walked too.
- After a big drawing change, use the profiling build: `cmake -DPHOS_PROFILE=ON`, then on the SS1
  `kill -USR2 $(pidof phosphor)` to reset the counters, use the menu, then `kill -USR1 $(pidof phosphor)` and
  read `/tmp/phosphor/prof`.
