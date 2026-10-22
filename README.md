# Phosphor OS

A console-style frontend for MiSTer FPGA, plus small ARM-side modules that run only when needed.
Working name; private while in early development.

Phosphor is built **with** stock MiSTer, not against it: current MiSTer Linux and Main, no
kernel changes, no `/dev/mem` from the frontend, nothing that polls. It should keep working
through every MiSTer update.

## What it is

| Part | What it does | Where |
|---|---|---|
| Shell | The interface: library shelf, game details, settings, in-game sheet | `frontend/` |
| System service | `phosphord`: library database, indexer, artwork store, play history, launching; `phos` command line | `system/` |
| Main fork | MiSTer Main with a small local API for the frontend (launch, pause, snapshots, pads, memory cards) | separate private repo `phosphor-main` (GPL-3.0, published at release) |
| Menu core fork | MiSTer's menu core plus SNAC controller and PS1 memory card logic, so those screens need no core swap | separate private repo `phosphor-menu` (GPL-3.0, published at release) |
| Modules | One-job ARM programs that start on an event or on request, then exit | separate private repo [`phosphor-modules`](https://github.com/f3bandit/phosphor-modules) |
| Android app | Phosphor for Android: library, box art, details, Based on, play on TV, over the home network | `apps/android/` |
| SD card image | Ready-to-flash card: stock MiSTer + Phosphor, built from Mr. Fusion | `image/` |
| Mockups | Design explorations | `mockups/` |

## Modules

The modules live in [`phosphor-modules`](https://github.com/f3bandit/phosphor-modules) so they can be
installed on their own (SS1 Tool can offer them to stock MiSTer users) and released separately.
First one: `cifs-helper`.

## Rules

- One job per module, clean install and uninstall, a short log in `/tmp`.
- Files live on the SD card (`/media/fat/phosphor/`), never inside `linux.img`; hooks are reinstalled from `user-startup.sh` after Linux updates.
- Nothing busy-waits: blocking reads with timeouts, so idle means zero CPU.
- Only MiSTer's own interfaces to the FPGA: `/dev/MiSTer_cmd`, MGL, core-mounted files, and the Main fork's API.
- Private settings (WiFi networks, Bluetooth pairings, share passwords, keys, device keys) stay on the
  SS1, the NVMe, a NAS or the phone app: never over the internet. Remote access answers the home network
  only and can't read or change WiFi.
- Original design only: own name, palette, fonts (open licence), icons, sounds and terms.

See [`docs/architecture.md`](docs/architecture.md) and [`docs/roadmap.md`](docs/roadmap.md).

## Licence

[PolyForm Noncommercial 1.0.0](LICENSE.md): use, modify and share for any non-commercial purpose;
selling it or a product built on it is not permitted. The Main and menu core forks are separate
repositories under GPL-3.0.
