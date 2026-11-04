# Working on Phosphor

- **Changing the TV menu (`frontend/`)**: read [docs/menu-rules.md](docs/menu-rules.md) first. It covers drawing
  opaque, setting styles only on change, timers and animations outliving their screen, and never waiting on
  phosphord in repeating code. Run `python3 tools/check-polls/check_polls.py` and `check_drawing.py` before
  committing; CI runs both.
- **Before a release**: walk the menus on the SS1 with `tools/check-polls/menu_walk.py --cold`, with a
  controller connected. It should report 0 stalls, no idle repaints and no crashes.
- **Port, don't rewrite**: a feature that already exists in SS1 Tool or Phosphor is ported (its code copied over), not
  written again, and works the same way on the SuperStation in every app.
- **Bluetooth pairings (MiSTer keeps them in the image `/media/fat/linux/bluetooth`, mounted at
  `/var/lib/bluetooth`)**: this broke a user's Bluetooth once (Nov 2026), so:
  - Never run `S45bluetooth stop` (or unmount `/var/lib/bluetooth`) before writing pairings: it unmounts the image,
    and the files go into the bare folder under it (lost at restart, or "writing the pairing files failed" on a
    read-only Linux). Stop only the bluetoothd process, write into the mounted image, start it again the way it ran
    (SS1 Tool's `btStopSh` / `btStartSh`; Phosphor pairs and forgets through `btctl` with bluetoothd running).
  - Never write the image file (`linux/bluetooth`) while it's mounted: stop Bluetooth and unmount it first, write,
    then `S45bluetooth start` (SS1 Tool's `btImgStopSh` / `btImgStartSh`).
  - Check the folder is writable before writing; wait for a running `btctl pair`; set `/tmp/ss1_bt.busy` while
    Bluetooth is changed (Console Mode for Windows and Phosphor wait on it).
  - Broken Bluetooth is repaired with phosphor-modules' `bt-repair/bt-repair.sh` (SS1 Tool's `scripts/ss1_bt_repair.sh`): change it in SS1 Tool and copy it, never write another.
- **Commits**: authored as f3bandit (`4943324+f3bandit@users.noreply.github.com`), with no co-author lines.
