# Phosphor system service

`phosphord` owns Phosphor's data: the game library, artwork, play history and favourites. The
shell (frontend), and later SS1 Tool and the phone app, ask it for everything; none of them scan
folders themselves. `phos` talks to it from the command line.

## What it does (0.2.0)

- **Library database** in `/media/fat/phosphor/data/library.db` (SQLite): systems, games, files.
- **Indexer** in the background at start and whenever the mount table changes (a USB drive or the
  network share comes or goes). Unchanged files (same size and date) are only marked as seen.
  Games on a drive that's away are kept, offline and hidden, and come back with the same id, art
  and play history. A game whose files are deleted is removed unless it has history or details.
- **Identity by content**, in the background and only while the menu is up: CRC32 for cartridges
  (copier headers left out, N64 in .z64 order), product code + track size + a CRC of the first
  megabyte for discs (under a second each), the header SHA-1 for .chd, the set name for arcade. Copies on several drives become one
  game; renamed or moved files keep their history and links.
- **Based on**: a hack, mod or translation can point to its base game and then shows the base's
  art and details under its own name. Suggestions rank the system's games by title likeness.
- **Artwork store** in `/media/fat/phosphor/data/art/<system>/<game>/box.png`, scaled down to
  512 px when stored so the shell never decodes a huge scan.
- **Import** (`phos import`, run once on request): reads `gamelist.xml` and Console Mode-style
  `media/` folders in the games folders, fills only empty fields (title, description, year,
  developer, publisher, genre, players) and copies box art into the store. The originals aren't
  touched, and after that only Phosphor's own data is read.
- **Launching** writes an MGL and sends `load_core` to MiSTer Main; records play count, last
  played, and play time (added when MiSTer's running core goes back to `MENU`).
- **API** on `/tmp/phosphor/phosphord.sock`: one JSON request per line, one JSON reply per line.
  Sleeps in `poll()`; wakes every 2 s only to keep play time.

Next: the scraper (libretro thumbnails, ScreenScraper, TheGamesDB) as a background queue, using
the identities and the `scrape` state (it skips linked and unrecognised games); product codes from
inside .chd files.

## API

| Request | Reply |
|---|---|
| `{"cmd":"status"}` | version, indexing, identifying, scan generation, game count |
| `{"cmd":"categories"}` | `[{id, name, games}]` |
| `{"cmd":"systems", "category":1, "all":false}` | `[{id, name, category, games, core}]` |
| `{"cmd":"games", "system":"SNES", "offset":0, "limit":100}` | `{total, games:[{id, title, favorite, linked}]}` in natural order |
| `{"cmd":"game", "id":3}` | title, details, art (the base game's where it has none), play history, `based_on` `{id, title}`, `scrape`, files with CRC32 and serial |
| `{"cmd":"link", "id":3, "base":7}` | game 3 is based on game 7 (same system); replies with the game |
| `{"cmd":"unlink", "id":3}` | not based on another game any more |
| `{"cmd":"candidates", "id":3, "q":"mario", "limit":200}` | games 3 could be based on, most alike first: `{total, current, games:[{id, title, score}]}` |
| `{"cmd":"last"}` | the last game played, or null |
| `{"cmd":"launch", "id":3}` | starts it |
| `{"cmd":"favorite", "id":3, "on":true}` | |
| `{"cmd":"rescan"}` / `{"cmd":"import"}` | |

Every reply has `"ok"`, and `"error"` when it's false.

## Command line

```
phos status | categories | rescan | import | last
phos systems <category number>
phos games <system>
phos game <id> | launch <id>
phos '{"cmd":"games","system":"PSX","limit":5}'
```

## Build

```
cmake -B build && cmake --build build -j
cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=../frontend/cmake/armhf.cmake && cmake --build build-arm -j
```

Static ARM binaries: `phosphord` about 1.5 MB with SQLite built in, `phos` about 340 KB.

Paths can be redirected for tests: `PHOS_FAT`, `PHOS_TMP`, `PHOS_GAMES_ROOTS`, `PHOS_MISTER_CMD`,
`PHOS_CORENAME`. Tested on a sandbox library on this machine and, as ARM builds, under
`qemu-arm-static` together with the shell. Not yet run on MiSTer hardware.

## Third-party code

`third_party/`: SQLite 3.50.4 (public domain), cJSON 1.7.19 (MIT), stb_image and stb_image_write
(public domain or MIT). Nothing GPL.
