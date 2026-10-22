# Architecture

## Layers

```
 ┌──────────────────────────── Frontend (ARM, draws to the Linux framebuffer) ─────┐
 │  Library · Details · Settings · Memory Cards · Controllers · In-game sheet     │
 └───────────────▲─────────────────────────────────────────────────────────────────┘
                 │ local API (socket/pipe, blocking, request → reply)
 ┌───────────────┴──────────── Main fork (owns the ARM↔FPGA link) ─────────────────┐
 │  launch/MGL · pause · save-state slots · fb ↔ scaler switch · last-frame grab   │
 │  pad state · memory card sector read/write (relayed to the menu core)          │
 └───────────────▲─────────────────────────────────────────────────────────────────┘
                 │ HPS bus (same channel stock Main uses)
 ┌───────────────┴──────────── Menu core fork (FPGA) ───────────────────────────────┐
 │  stock menu video (shows the Linux framebuffer) + SNAC pad reader + PS1 card SIO│
 └──────────────────────────────────────────────────────────────────────────────────┘

 Modules (ARM, event-started, exit when done) talk to Main's API or plain files in /tmp.
```

## Why only Main touches the FPGA link

Main drives the ARM↔FPGA link. A second program doing the same would race it. Every
feature that needs the FPGA goes through Main's API, which relays it to the menu core.

## In-game sheet

On the menu button: pause the core (cores with OSD pause), grab the scaler's last frame,
switch the output to the Linux framebuffer, draw the sheet over the dimmed frame. On close,
switch back. Save-state slots map to MiSTer's 4 per-core slots, on cores that support them.
Direct-video / 15 kHz analog falls back to MiSTer's OSD.

## Network shares

The frontend watches mount changes (poll `/proc/self/mounts` for `POLLPRI`) and adds games
when a share appears, so a late mount never needs a reboot or a fixed delay.

## Licences

- Main fork and menu core fork: GPL-3.0 (derived from MiSTer-devel). Source published with any release.
- Frontend and modules: our code; licence to be decided before the first release.
