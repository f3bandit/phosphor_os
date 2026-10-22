# Roadmap

Each step is useful on its own; later steps reuse earlier ones.

| # | Feature | What it means | Notes |
|---|---|---|---|
| 1 | **MiSTer-devel Linux** | Run on official MiSTer Linux and Main, always current | No Linux fork. Phosphor never ships its own `linux.img` or kernel; it follows MiSTer-devel's releases. |
| 2 | **SD card image** (built, 0.7) | A ready-to-flash image: stock MiSTer + Phosphor preinstalled, first-boot setup, card expanded to full size | Mr. Fusion (MiSTer-devel's installer image) with Phosphor installed into the MiSTer release it carries; `image/build-image.sh`, GitHub Actions workflow run by hand. See `image/README.md`. The shell shows a Welcome screen on first start (network, box art, accent, phone app, cloud saves). |
| 3 | **Headless menu core** | Fork of MiSTer's menu core with SNAC pad reader + PS1 memory card logic built in, so Memory Cards and Controllers screens need no core swap | `phosphor-menu` (GPL-3.0), reached through the Main fork's API. Logic ported from the SS1 Controller Tester and SS1 MemCard cores. |
| 4 | **Modules** | One-job ARM programs started by events: cifs-helper, backup-agent, health-watch, update-guard | cifs-helper first. |
| 5 | **Screen filters** (built, 0.7) | Scanlines, shadow masks, CRT-style filters | Settings > Screen filters: per system, pick a preset or each slot (video, vertical, scanlines, interlaced filter, shadow mask and size, gamma) from MiSTer's own folders. Phosphor writes the same per-core files Main writes from the OSD, backs them up first, SELECT undoes. Shows the next time the core starts. Still to do: a live preview. |
| 6 | **Themes** (built) | Accent colours (amber default, red, green, blue, purple) and background textures (zigzags, X's, squares, triangles, circles, hexagons, 8-bit tiles, or none), recoloured with the accent | Something the Analogue consoles don't have. Subtle, so the menu still reads as a plain dark screen. **8-bit tiles are original art drawn for Phosphor**, not tiles from NES/SNES games. |
| 7 | **Android app integration** (built: phosphord 0.8, Phosphor for Android 0.1) | The phone app talks to Phosphor: remote control, library, settings, memory cards, backups | Over the same local API (network-exposed, paired with a code): built in phosphord 0.8, see `docs/remote-api.md`. Phosphor for Android (`apps/android`, separate from SS1 Tool for Android): library, box art from the phone, details, Based on, play on TV. Backups (0.3): snapshots, back up now, restore saves or everything. Next: memory cards (needs the menu core fork). Fits SS1 Tool for Android or a dedicated companion app. |
| 8 | **Cloud storage** (module built: cloud-sync 0.1) | Saves and settings synced to the user's own cloud (Google Drive, Dropbox, OneDrive, WebDAV, SMB) | The `cloud-sync` module (phosphor-modules) with rclone (MIT): sends saves after each game with history, restores and shares between MiSTers on request. Settings > Cloud saves on the TV (phosphord 0.9) and in the Android app (0.2): setup, send now, preview and restore, storage places. |
| 9 | **In-menu WiFi** (built, phosphord 0.6) | Scan networks with signal strength, band and security; connect, forget, reorder; all from the menu with a controller | Uses `wpa_supplicant`/`iw` like SS1 Tool's WiFi page; on-screen keyboard for passwords. |
| 11 | **Claude plugin for the library** (Windows side built: Phosphor Link 0.2) | A plugin for the Claude app (phone, tablet or PC) with clear instructions for managing the Phosphor library: find and fix box art and details, link hacks to their base games, fill in missing descriptions, review "not found" games | **Decided**: no tunnel or relay. Claude in the phone app runs its tools from Anthropic's servers, which can't reach a MiSTer on the home network, so Claude library work is on the PC (Phosphor Link); on the phone, the Phosphor Android app does it by hand. **Windows**: a Phosphor PC app reaching the MiSTer over the SS1 dock's debug port, Ethernet or WiFi; the PC does the searching and image work, so it's much faster than the ARM side, and sends finished art and details to phosphord. Builds on Phosphor Link (MCP server, already working over the debug port) and phosphord's API. |
| 10 | **Status bar** | Top right, beside the clock: network share, Ethernet, WiFi (signal bars), Bluetooth (with count of connected controllers) | Live from `/sys/class/net`, `iw`, BlueZ and `/proc/self/mounts` (event-driven, no polling loop). Dim = off/disconnected, bright = active; tap through to the matching settings page. |

Library: content identity, "Based on" links and the scraper with six sources (libretro, ArcadeDB,
LaunchBox, TheGamesDB, ScreenScraper, IGDB; user's choice and order) are done (phosphord 0.4).
To do: register Phosphor with ScreenScraper for a developer id; test the keyed sources with real
keys; then the Claude plugin (#11) for remote library management.

Supporting work along the way: the Main fork (`phosphor-main`, GPL-3.0) with the local API; the frontend
MVP (library shelf, game list, launch, settings, mount-change rescan); the in-game sheet (HDMI/scaler
first, OSD fallback for direct-video analog).

Release prep: name and trademark check, publish the GPL forks, own downloader database for installs.

## Decided

- Frontend toolkit: **C + LVGL 9.6** (MIT). Slint was ruled out: its royalty-free licence excludes
  embedded systems, leaving GPLv3 or paid, neither compatible with PolyForm Noncommercial.

## Open questions

- Final name (Phosphor is a working name).
- Android: extend SS1 Tool for Android, or a separate companion app?
