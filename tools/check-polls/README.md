# Menu lag checks

The menu must never wait for phosphord on something it asks again and again: phosphord answers one
request at a time, so a synchronous poll stops the cursor whenever phosphord is busy, and the presses
then arrive all at once (fixed Oct 29 2026; it kept coming back as screens were added).

- `check_polls.py`: the source check, run by GitHub Actions ("Shell checks") on every change to
  `frontend/`. Every `api_call` / `api_call_slow` / `api_call_json` reachable from a repeating path (the
  main loop's timers, the status bar, the Settings side panel, `*_tick` / `*_poll`, list fill callbacks)
  fails unless it's marked `/* poll-ok: why */` or listed in `check_polls_baseline.txt`. Repeated polls
  use `api_poll(key, ...)` (or `api_bg_ask` / `api_bg_take`): the newest answer from a background thread.
- `menu_walk.py`: the same on the SuperStation (copy it there, `python3 menu_walk.py --cold`): walks the
  Settings list, opens every Settings screen that's safe to open (never Look for games, Update, MiSTer
  menu, Restart, Shut down or a toggle), the Home cards and a library list, and reports every stall of
  250 ms or more the shell logged meanwhile ("slow:" / "poll:" in /tmp/phosphor/frontend.log). It finds
  its way by the shell's `/tmp/phosphor/screen` (the screen and the Settings row). Run it before a release.
- The shell's own log: "slow: N ms (keys, timers, drawing, % of the screen repainted) (screen N)" for any
  stall, "slow: phosphord took N ms for ..." for a slow answer, "poll: ..." for a request the menu waited
  for 5 times in 10 s, and how the shell ended (who stopped it, or where it crashed) in phosphor/logs/phosphor.log.
