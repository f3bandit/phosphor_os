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
  - `files`: path, root (SD / USB n / share), size, modified time, CRC32, disc serial, online.
- **Identity by content.** Each file is identified in the background (only while the menu is up,
  never while a core is running from the same drive):
  - cartridges and computer files: CRC32 of the ROM data the way No-Intro counts it (iNES/FDS,
    SNES copier, Atari 7800 and Lynx headers left out; N64 read in .z64 byte order);
  - discs (.cue/.bin, .iso): the product code (PlayStation SYSTEM.CNF, Saturn and Mega-CD
    headers) for looking the game up, and as identity the product code + data track size + CRC32 of
    its first megabyte. Under a second per disc; hashing whole discs took ~40 s each over the
    SuperStation's USB, 2-3 hours for a typical library. Size and sample keep mods and patched
    discs (which usually keep the original's product code) apart from the original;
  - .chd: the SHA-1 of the original data stored in the .chd header; the product code is read from the
    disc inside (libchdr) for lookups;
  - arcade: the MAME set name in the .mra.
  Copies of the same game on the SD card, a USB drive and the share become one game with several
  files (favourite, play history and details are combined). Renaming or moving a file keeps its
  art, details, history and links. Until a file is identified, its name stands in.
- **Based on (hacks, translations, mods).** A game can point to the base game it was made from
  (`based_on`). It then shows the base game's box art and description (and year, publisher and so
  on), while its own title stays the file name, so the user still sees which version it is. Its own
  art or description, if added, wins. Linked from the games list (Y → "Based on"): a window lists
  the system's games, the most alike title first, with "Not based on another game" to undo. Links
  don't chain: linking to a hack links to that hack's base. A base game with linked games is kept
  even when its own file goes.
- **What the scraper does with each game** (`scrape` in the API): `ready` (identified, look it up),
  `linked` (skip: uses its base's art), `unidentified` (no content identity yet: not looked up, no
  guessing by name), `not_found` (looked up and not recognised: left alone rather than guessing).
  Later: a base game can be one from the No-Intro/Redump title list that isn't in the library.
- **Indexer.** Runs in the background at start and whenever the mount table changes (a USB drive or
  the share appears or goes). Uses size and modified time, so unchanged files are never re-read.
  Files on a drive that's away are marked offline, not forgotten.
- **Artwork store**: `/media/fat/phosphor/data/art/<system>/<game key>/` with `box.jpg` (and later
  screenshot, title, logo), stored pre-sized so the ARM side never decodes a huge image.
- **Scraper** (phosphord 0.4): a background queue, only while the menu is up, one game at a time,
  turned on in Settings ("Get box art and details"). Several sources, asked in the user's order
  (Settings > Box art sources, saved in `config/scraper.ini`); for each field (description,
  developer, publisher, genre, year, players, region, box art) the first answer wins, and fields a
  game already has are never replaced. The title stays the file name.

  | Source | Account | Matched by | Gives |
  |---|---|---|---|
  | libretro | none | CRC32 / product code (+ size) against No-Intro & Redump | official name, details, PlayStation descriptions, box art |
  | ArcadeDB | none | MAME set name in the .mra | title, maker, year, genre, players, history, flyer |
  | LaunchBox | none | official name (from a content match) | descriptions, details, box art |
  | TheGamesDB | API key | official name | descriptions, year, players, box art |
  | ScreenScraper | Phosphor's developer id (+ user login) | CRC32 / product code | descriptions, details, box art |
  | IGDB | Twitch developer app | official name | descriptions, details, cover |

  Nothing is ever looked up by file name: a game nobody recognises by content is "not found" and
  left alone (link it with "Based on"). libretro's lists and LaunchBox's database (one 110 MB
  download, read as a stream, only the systems with games kept) are fetched once and kept in the
  library database. Long imports commit in small batches so identifying and the API never wait.
  Box art is stored as 512 px JPEG (~60 KB). One unreachable source only delays that game; several
  failures in a row pause the queue for a few minutes. Changing the sources looks again at games
  still missing art or a description.
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
