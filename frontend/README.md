# Phosphor shell

The Phosphor interface: a console-style library for MiSTer, drawn on the Linux framebuffer with
[LVGL](https://lvgl.io) 9.6 (MIT). One static ARM binary, about 1.4 MB, with no dependencies on
MiSTer's Linux. Everything it shows comes from [`phosphord`](../system/), the system service; the
shell starts it if it isn't running.

![Home](docs/home.png)

| Systems in a category | Games | Settings |
|---|---|---|
| ![](docs/systems.png) | ![](docs/games.png) | ![](docs/settings.png) |

## What works

- **Home**: category shelf (Disc, Cartridge, Handheld, Arcade, Computer) with game counts, and
  "Pick up where you left off" with the last game's box art and play time (X plays it).
- **Systems → games**: only systems with games are listed; systems without their core are dimmed.
  The game list stays fast with thousands of games (rows are reused).
- **Game details**: box art at the top, then title, year · publisher · players · genre, the
  description, and where the file lives (SD card, USB/NVMe, network share; "not connected" when
  its drive is away). Art loads a moment after the selection settles, so scrolling stays quick.
- **Based on** (Y in the games list): for a hack, mod or translation, pick the game it was made
  from; the most alike titles are listed first. It then shows that game's box art and description
  under its own name, with "Based on …" in its details. "Not based on another game" undoes it.

  ![Based on](docs/based-on.png)
- **Box art** is shown whether phosphord stored it as JPEG (scraped) or PNG (imported); JPEGs are
  decoded whole (stb_image) so they scale to fit.
- **Launching** goes through phosphord, which records play count and time.
- **Refreshes by itself** when phosphord finishes a scan (a drive or share was plugged in) or finds
  that two entries are the same game.
- **Status bar**: network share, Ethernet, WiFi signal, Bluetooth with connected count, clock.
- **Controllers and keyboards** through evdev, picked up when plugged in, with key repeat. D-pad,
  hat or left stick; south button / Enter = A, east / Esc = B, north / Y key = Y, Start / Guide / F1 = Menu, shoulders
  or PgUp/PgDn = page.
- **Settings**: accent colour (Amber, Red, Green, Blue, Purple), 12/24-hour clock, get box art and
  details (on/off, with progress: found, to go, not found), look for games again. Saved in `/media/fat/phosphor/config/frontend.ini`.
- **While a game runs** (MiSTer's `/tmp/CORENAME` isn't `MENU`) it sleeps and ignores input.
- Logs to `/tmp/phosphor/frontend.log`, and to the serial console with `--log-serial`.

## Build

```
cmake -B build && cmake --build build -j                     # this machine: snapshots and checks
cmake -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/armhf.cmake && cmake --build build-arm -j
                                                              # MiSTer: build-arm/phosphor
```

Needs `gcc`, `cmake` and, for the MiSTer build, `arm-linux-gnueabihf-gcc`. The MiSTer binary is
linked statically because MiSTer's Linux has glibc 2.31. Fonts are pre-converted in `fonts/`;
`tools/make_fonts.sh` regenerates them (Inter, SIL OFL; DejaVu Sans Mono).

## Screenshots without a screen

```
build/phosphor --snapshot out.ppm --keys right,a,down,a
```

renders the interface in memory after pressing those keys and saves a picture. It waits for
phosphord's first scan. For a test library point both at the same folders:
`PHOS_FAT=/path/to/fake/fat PHOS_GAMES_ROOTS=/path/to/games PHOS_TMP=/tmp/test PHOS_MISTER_CMD=/tmp/cmd.txt`,
and `PHOSD_BIN=../system/build/phosphord` so the shell can start it.
`PHOS_FAKE_STATUS="share=1,eth=0,wifi=3,bt=2"` and `PHOS_FAKE_CLOCK="8:41 PM"` fix the status bar.
The ARM shell and ARM phosphord, run together under `qemu-arm-static`, render the same picture
pixel for pixel as the host builds.

## Trying it on a MiSTer (stock Main)

Until the Phosphor Main fork exists, the Linux framebuffer has to be switched on by hand:

1. Copy `build-arm/phosphor` and `../system/build-arm/phosphord` to `/media/fat/phosphor/bin/`.
2. In the MiSTer menu, press **F9** on a keyboard to show the Linux screen.
3. Over SSH: `/media/fat/phosphor/bin/phosphor` (add `--log-serial` to watch the log on the debug
   port). It asks Main for a 1280x720 framebuffer (`fb_cmd1`).
4. If red and blue look swapped, start it with `--fb-rb 0`. `--fb-mode none` leaves the mode alone.

Stock Main still reacts to the controller in the menu at the same time; the Main fork will hand
input to the frontend.

## Systems and launch settings

These live in phosphord now: see [`../system/config/systems.ini`](../system/config/systems.ini).

## Layout

```
src/main.c       entry, framebuffer loop, snapshot mode
src/ui.c         screens, shelf animation, virtual list, footer, toast
src/api.c        phosphord client (starts it if needed)
src/input.c      evdev, hot-plug, repeat
src/sysinfo.c    status bar data
src/icons.c      outline icons drawn as vectors (our own designs)
src/theme.c      palette, accents, scaling (designed at 1280x720)
src/settings.c   frontend.ini
fonts/           pre-converted fonts
third_party/lvgl LVGL 9.6.0 (MIT; see its LICENCE.txt and COPYRIGHTS.md)
```

## Licence

PolyForm Noncommercial 1.0.0, like the rest of this repository. LVGL and cJSON are MIT; Inter is SIL OFL 1.1;
DejaVu Sans Mono is under the Bitstream Vera licence. Nothing GPL is linked: LVGL's FreeType and
FrogFS integrations stay switched off.
