# Phosphor OS

A console-style frontend for MiSTer FPGA, plus small ARM-side modules that run only when needed.
Working name; private while in early development.

Phosphor is built **with** stock MiSTer, not against it: current MiSTer Linux and Main, no
kernel changes, no `/dev/mem` from the frontend, nothing that polls. It should keep working
through every MiSTer update.

## What it is

| Part | What it does | Where |
|---|---|---|
| Frontend | The interface: library shelf, game details, settings, in-game sheet | `frontend/` |
| Main fork | MiSTer Main with a small local API for the frontend (launch, pause, snapshots, pads, memory cards) | separate private repo `phosphor-main` (GPL-3.0, published at release) |
| Menu core fork | MiSTer's menu core plus SNAC controller and PS1 memory card logic, so those screens need no core swap | separate private repo `phosphor-menu` (GPL-3.0, published at release) |
| Modules | One-job ARM programs that start on an event or on request, then exit | `modules/` |
| Mockups | Design explorations | `mockups/` |

## Modules (planned)

| Module | Job | Starts when |
|---|---|---|
| `cifs-helper` | Mount network shares as soon as the network is up; retry; health check; boot log | DHCP bound (WiFi or Ethernet) |
| `backup-agent` | Save and settings backups | A game exits, or on a schedule |
| `health-watch` | SD free space, NVMe temperature, brownouts | On a timer that sleeps between checks |
| `update-guard` | Checks before Linux or frontend updates | Before an update runs |

## Rules

- One job per module, clean install and uninstall, a short log in `/tmp`.
- Files live on the SD card (`/media/fat/phosphor/`), never inside `linux.img`; hooks are reinstalled from `user-startup.sh` after Linux updates.
- Nothing busy-waits: blocking reads with timeouts, so idle means zero CPU.
- Only MiSTer's own interfaces to the FPGA: `/dev/MiSTer_cmd`, MGL, core-mounted files, and the Main fork's API.
- Original design only: own name, palette, fonts (open licence), icons, sounds and terms.

See [`docs/architecture.md`](docs/architecture.md) and [`docs/roadmap.md`](docs/roadmap.md).
