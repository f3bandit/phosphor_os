# Architecture

Phosphor is a complete console operating system for MiSTer. It runs on MiSTer-devel's own kernel
and Linux (it never ships a `linux.img` or kernel), and owns everything above them: the system
service, the library, artwork, settings, the shell (frontend), the hardware link (Main fork) and the
menu core. It doesn't read or depend on other frontends' files.

## Layers

```
 ┌──────────────────────── Shell: phosphor (frontend, Linux framebuffer) ──────────────┐
 │  Home · Library · Game details · Settings · Memory Cards · Controllers · In-game     │
 └──────────────▲───────────────────────────────────────────────▲──────────────────────┘
                │ phosphord API (local socket, JSON lines)       │ Main API
 ┌──────────────┴─────────── System service: phosphord ──────┐   │
 │  library database · indexer · artwork store · scraper     │   │
 │  play history · favourites · settings · module control    │───┤ launch / pause / pads
 │  same API for the SS1 Tool and the Android app            │   │
 └───────────────────────────────────────────────────────────┘   │
 ┌───────────────────────────── Main fork (owns the ARM↔FPGA link) ┴──────────────────┐
 │  launch/MGL · pause · save-state slots · fb ↔ scaler switch · last-frame grab      │
 │  pad state · memory card sector read/write (relayed to the menu core)             │
 └──────────────▲─────────────────────────────────────────────────────────────────────┘
                │ HPS bus (same channel stock Main uses)
 ┌──────────────┴──────────── Menu core fork (FPGA) ──────────────────────────────────┐
 │  stock menu video (shows the Linux framebuffer) + SNAC pad reader + PS1 card SIO   │
 └────────────────────────────────────────────────────────────────────────────────────┘

 MiSTer-devel kernel and Linux underneath, updated by MiSTer; update-guard keeps restore points.
 Modules (ARM, event-started, exit when done) are controlled by phosphord.
```

## System service: phosphord

One process owns the system's data, so every client (shell, SS1 Tool, phone app) sees the same
thing and no client scans folders itself.

- **Library database**: `/media/fat/phosphor/data/library.db` (SQLite, public domain).
  - `systems`: id, name, category.
  - `games`: system, identity key, title, region, year, developer, publisher, genre, players,
    description, box art, favourite, play count, play time, last played.
  - `files`: path, root (SD / USB n / share), size, modified time, CRC32 or disc serial, online.
- **Identity by content.** Each game is matched by content: CRC32 for cartridge images, the
  disc serial for CDs (No-Intro / Redump data). Renaming or moving a file keeps its art, details
  and history. Until a file is hashed, its name stands in.
- **Based on (hacks, translations, mods).** A game can point to the base game it was made from
  (`based_on`). It then shows the base game's box art and description, while its own title stays the
  file name, so the user still sees which version it is. Its own art or description, if added, wins.
  Linked from the games list (Y → "Based on…"): a window lists the system's games, closest name
  first. The scraper skips linked games (they use the base game's art) and skips games it doesn't
  recognise rather than guessing. Links are kept by game identity, so they survive renames and moves.
  Later: a base game can be one from the No-Intro/Redump title list that isn't in the library.
- **Indexer.** Runs in the background at start and whenever the mount table changes (a USB drive or
  the share appears or goes). Uses size and modified time, so unchanged files are never re-read.
  Files on a drive that's away are marked offline, not forgotten.
- **Artwork store**: `/media/fat/phosphor/data/art/<system>/<game key>/` with `box.png` (and later
  screenshot, title, logo), stored pre-sized so the ARM side never decodes a huge image.
- **Scraper**: a background job queue. Sources: libretro thumbnails (no account), ScreenScraper
  (your account), TheGamesDB. Matched by content identity, cached, works offline afterwards.
- **Importers, run once on request**: bring an existing `gamelist.xml` / Console Mode `media/` art
  into the library when someone moves to Phosphor. After that, only Phosphor's data is read.
- **Launching** goes through phosphord: it writes the MGL, asks Main to load it and records the
  play (start, and time played when the core exits back to the menu).
- **API**: a local socket in `/tmp/phosphor/`, one JSON request per line, one JSON reply per line.

## Why only Main touches the FPGA link

Main drives the ARM↔FPGA link. A second program doing the same would race it. Every
feature that needs the FPGA goes through Main's API, which relays it to the menu core.

## In-game sheet

On the menu button: pause the core (cores with OSD pause), grab the scaler's last frame,
switch the output to the Linux framebuffer, draw the sheet over the dimmed frame. On close,
switch back. Save-state slots map to MiSTer's 4 per-core slots, on cores that support them.
Direct-video / 15 kHz analog falls back to MiSTer's OSD.

## Storage layout

```
/media/fat/phosphor/
  bin/          phosphor (shell), phosphord, phos (command line client)
  data/         library.db, art/
  config/       frontend.ini, systems.ini, module settings
  modules/      phosphor-modules
  logs/         what's worth keeping across reboots
/tmp/phosphor/  socket, runtime logs
```

Games stay where MiSTer keeps them (`games/<System>` on the SD card, USB drives and the network
share; `_Arcade` for arcade). Phosphor never writes into games folders.

## Licences

- Main fork and menu core fork: GPL-3.0 (derived from MiSTer-devel). Source published with any release.
- Shell, phosphord, modules and everything else in this repo: PolyForm Noncommercial 1.0.0 (`LICENSE.md`).
  Keep the boundary clean: no MiSTer GPL code copied in; talk to the Main fork only through its API.
  Dependencies must be permissive (MIT, BSD, Apache, zlib, public domain), never GPL:
  LVGL (MIT), SQLite (public domain), cJSON (MIT), stb (public domain / MIT).
