# Phosphor frontend

The Phosphor interface: a console-style library for MiSTer, drawn on the Linux framebuffer with
[LVGL](https://lvgl.io) 9.6 (MIT). One static ARM binary, about 1.4 MB, with no dependencies on
MiSTer's Linux.

![Home](docs/home.png)

| Systems in a category | Games | Settings |
|---|---|---|
| ![](docs/systems.png) | ![](docs/games.png) | ![](docs/settings.png) |

## What works (0.1.0)

- **Home**: category shelf (Disc, Cartridge, Handheld, Arcade, Computer) with game counts, and
  "Pick up where you left off" for the last game played (X plays it).
- **Systems → games**: only systems that have games are listed; a game list in natural order
  ("Quest 2" before "Quest 10") that stays fast with thousands of games (rows are reused, not created
  per game), and details: file, where it lives (SD card, USB/NVMe, network share), size.
- **Launching**: writes an MGL file and sends `load_core` to MiSTer Main, the same way MiSTer's own
  tools do. Arcade `.mra` files are loaded directly.
- **Library scan** in the background; it runs again by itself when a drive or network share is
  mounted or unmounted (the mount table is watched; there's no timer).
- **Status bar**: network share, Ethernet, WiFi signal, Bluetooth with connected count, clock. Read
  from `/proc` and `/sys`; nothing is spawned.
- **Controllers and keyboards** through evdev, picked up when plugged in, with key repeat. D-pad,
  hat or left stick; south button / Enter = A, east / Esc = B, Start / Guide / F1 = Menu, shoulders
  or PgUp/PgDn = page.
- **Settings**: accent colour (Amber, Red, Green, Blue, Purple), 12/24-hour clock, look for games
  again. Saved in `/media/fat/phosphor/config/frontend.ini`.
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

renders the interface in memory after pressing those keys and saves a picture. For a test library:
`PHOS_FAT=/path/to/fake/fat PHOS_GAMES_ROOTS=/path/to/games PHOS_MISTER_CMD=/tmp/cmd.txt`;
`PHOS_FAKE_STATUS="share=1,eth=0,wifi=3,bt=2"` and `PHOS_FAKE_CLOCK="8:41 PM"` fix the status bar.
The ARM build renders the same picture pixel for pixel under `qemu-arm-static`.

## Trying it on a MiSTer (stock Main)

Until the Phosphor Main fork exists, the Linux framebuffer has to be switched on by hand:

1. Copy `build-arm/phosphor` to `/media/fat/phosphor/bin/phosphor`.
2. In the MiSTer menu, press **F9** on a keyboard to show the Linux screen.
3. Over SSH: `/media/fat/phosphor/bin/phosphor` (add `--log-serial` to watch the log on the debug
   port). It asks Main for a 1280x720 framebuffer (`fb_cmd1`).
4. If red and blue look swapped, start it with `--fb-rb 0`. `--fb-mode none` leaves the mode alone.

Stock Main still reacts to the controller in the menu at the same time; the Main fork will hand
input to the frontend.

## Systems and launch settings

Built in: PlayStation, Saturn, Mega-CD, PC Engine CD, NES, SNES, Mega Drive/Genesis, N64, Master
System, PC Engine, Atari 7800/2600, Game Boy/Color, GBA, Game Gear, WonderSwan, Lynx, Arcade, C64,
MSX, Atari 800. `config/systems.ini` explains how to change one or add more without rebuilding.
The MGL slot numbers (type/index/delay) still need checking on hardware, core by core.

## Layout

```
src/main.c       entry, framebuffer loop, snapshot mode
src/ui.c         screens, shelf animation, virtual list, footer, toast
src/library.c    systems, scanning, MGL launching
src/input.c      evdev, hot-plug, repeat
src/sysinfo.c    status bar data, mount-change watch
src/icons.c      outline icons drawn as vectors (our own designs)
src/theme.c      palette, accents, scaling (designed at 1280x720)
src/settings.c   frontend.ini
fonts/           pre-converted fonts
third_party/lvgl LVGL 9.6.0 (MIT; see its LICENCE.txt and COPYRIGHTS.md)
```

## Licence

PolyForm Noncommercial 1.0.0, like the rest of this repository. LVGL is MIT; Inter is SIL OFL 1.1;
DejaVu Sans Mono is under the Bitstream Vera licence. Nothing GPL is linked: LVGL's FreeType and
FrogFS integrations stay switched off.
