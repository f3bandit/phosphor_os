# Working on Phosphor

- **Changing the TV menu (`frontend/`)**: read [docs/menu-rules.md](docs/menu-rules.md) first. It covers drawing
  opaque, setting styles only on change, timers and animations outliving their screen, and never waiting on
  phosphord in repeating code. Run `python3 tools/check-polls/check_polls.py` and `check_drawing.py` before
  committing; CI runs both.
- **Before a release**: walk the menus on the SS1 with `tools/check-polls/menu_walk.py --cold`, with a
  controller connected. It should report 0 stalls, no idle repaints and no crashes.
- **Commits**: authored as f3bandit (`4943324+f3bandit@users.noreply.github.com`), with no co-author lines.
