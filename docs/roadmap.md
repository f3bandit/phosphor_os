# Roadmap

Each step is useful on its own; later steps reuse earlier ones.

| # | Feature | What it means | Notes |
|---|---|---|---|
| 1 | **MiSTer-devel Linux** | Run on official MiSTer Linux and Main, always current | No Linux fork. Phosphor never ships its own `linux.img` or kernel; it follows MiSTer-devel's releases. |
| 2 | **SD card image** | A ready-to-flash image: stock MiSTer + Phosphor preinstalled, first-boot setup, card expanded to full size | Built from MiSTer's own installer approach (like mr-fusion); one exFAT partition; rebuilt in CI for each MiSTer release. |
| 3 | **Headless menu core** | Fork of MiSTer's menu core with SNAC pad reader + PS1 memory card logic built in, so Memory Cards and Controllers screens need no core swap | `phosphor-menu` (GPL-3.0), reached through the Main fork's API. Logic ported from the SS1 Controller Tester and SS1 MemCard cores. |
| 4 | **Modules** | One-job ARM programs started by events: cifs-helper, backup-agent, health-watch, update-guard | cifs-helper first. |
| 5 | **Screen filters** | Scanlines, shadow masks, CRT-style filters | In games: manage MiSTer's own filter and shadow-mask files per core (presets, preview). In the menu: drawn by the frontend. |
| 6 | **Themes** | Accent colours (amber default, red, green, blue, purple) and wallpaper patterns (zigzags, X's, squares, triangles, circles, hexagons, 8-bit tiles), all recolourable with the theme | **8-bit tiles must be original art drawn for Phosphor**, not tiles taken from NES/SNES games (those are the games' copyrighted graphics). An 8-bit *style* is fine. |
| 7 | **Android app integration** | The phone app talks to Phosphor: remote control, library, settings, memory cards, backups | Over the same local API (network-exposed, paired with a code). Fits SS1 Tool for Android or a dedicated companion app. |
| 8 | **Cloud storage** | Saves and settings synced to the user's own cloud (Google Drive, Dropbox, OneDrive, WebDAV, SMB) | Through a module using rclone (MIT) on the ARM side; no Phosphor-run servers needed. |
| 9 | **In-menu WiFi** | Scan networks with signal strength, band and security; connect, forget, reorder; all from the menu with a controller | Uses `wpa_supplicant`/`iw` like SS1 Tool's WiFi page; on-screen keyboard for passwords. |
| 10 | **Status bar** | Top right, beside the clock: network share, Ethernet, WiFi (signal bars), Bluetooth (with count of connected controllers) | Live from `/sys/class/net`, `iw`, BlueZ and `/proc/self/mounts` (event-driven, no polling loop). Dim = off/disconnected, bright = active; tap through to the matching settings page. |

Supporting work along the way: the Main fork (`phosphor-main`, GPL-3.0) with the local API; the frontend
MVP (library shelf, game list, launch, settings, mount-change rescan); the in-game sheet (HDMI/scaler
first, OSD fallback for direct-video analog).

Release prep: name and trademark check, publish the GPL forks, own downloader database for installs.

## Open questions

- Frontend language/toolkit: Rust + Slint (like Degauss and Zaparoo), or Go with a framebuffer renderer?
- Final name (Phosphor is a working name).
- Android: extend SS1 Tool for Android, or a separate companion app?
