# Things to test on the SuperStation One

Everything here was built and tested in the sandbox (and some of it on the SS1 over the debug
port), but still needs checking on the real thing. Work down the list; tick items off with the date.
Phosphor Link (debug port) does most of the setup, so most of this can be driven from the Claude app.

Legend: **[ ]** not tested yet · **[~]** partly tested on the SS1 · **[x]** done

## 0. Before starting

- [ ] Back up the SD card (or at least `/media/fat/MiSTer.ini`, `linux/`, `ConsoleMode/`, `Scripts/`).
- [ ] Phosphor Link connects (`phosphor-link status`) and the Claude app sees it.
- [ ] Note the stock state for comparison: `phosphor-link run "bash diag.sh report"` (diag module).

## Installing for the tests

Build: `install/build-release.sh` → `dist/phosphor-<version>-armhf.tar.gz` (~2.3 MB).
Copy it to the SS1 over the network share (`\\200.200.200.23\sdcard`, Samba is on) or with
Phosphor Link (`push_file`, ~6 min over the debug port), then on the SS1:

```
cd /media/fat && tar xzf phosphor-<version>-armhf.tar.gz && phosphor-<version>/install.sh
/media/fat/phosphor/phosphor.sh service     # phosphord only (library, scraper, diagnostics)
/media/fat/phosphor/phosphor.sh start       # + the shell (press F9 on a keyboard if not visible)
/media/fat/phosphor/phosphor.sh status      # stop / log
```

- [ ] `install.sh` (no boot entry) → only `/media/fat/phosphor` created.
- [ ] `install.sh --boot` → block "phosphor" in `linux/user-startup.sh`, backup in `phosphor/backup/`; reboot → phosphord running.
- [ ] `uninstall.sh` → boot block gone, `user-startup.sh` byte-for-byte as before, library kept; `--all` → folder gone.

## 1. Phosphor Link (PC tool, debug port)

- [x] Connects on COM5, logs in, `status` and `selftest` pass (2026-10-22).
- [x] Claude desktop app uses it (status, run, push_file) (2026-10-22).
- [ ] `reboot`: reboots the SS1 and returns the whole boot log; nothing typed during U-Boot.
- [ ] `watch` / `serial_log`: shows Main's messages while browsing the menu and starting a core.
- [ ] `term`: interactive console, Ctrl-] quits, console left as it was (`dmesg -n` restored).
- [ ] SSH fallback: add `host=200.200.200.23` to `phosphor-link.ini`, unplug the debug cable, `status`.
- [ ] Big file over SSH vs serial (speed).

## 2. Modules (phosphor-modules)

Install each with its `install.sh`, check, then `uninstall.sh` and check everything is back.

- [ ] **diag**: `diag.sh` all areas on the SS1 (ran once without install: OK, 3 warnings).
  - [ ] `diag.sh input`: every button and stick on the 8BitDo and the SS1 pad shows up.
  - [ ] `diag.sh speed /media/usb0`: NVMe numbers look right.
  - [ ] `diag.sh edid 0` (or the right bus): TV's EDID reads.
  - [ ] `diag.sh bundle`: .tar.gz made, no passwords inside (check `settings/`).
- [ ] **cifs-helper**: share mounts at boot faster than the stock script; unplug the network → no hang;
      plug back → remounts; reboot with the NAS off → menu still comes up quickly.
- [ ] **backup-agent**: play a game, quit → a snapshot appears; daily snapshot; restore one; undo it.
- [ ] **health-watch**: status file updates; fill a drive above the warning level → warning shows.
- [ ] **update-guard**: restore point made; run `update_all` through it; roll back; undo the rollback.
      `--hold` stops Linux updates; `unhold` puts the setting back exactly.
- [ ] Every installer: makes a backup first, has an undo, nothing runs until asked.

## 3. phosphord (library service)

Run from `/tmp` first (nothing written to the SD card), then installed.

- [x] Scan of the whole library: 5,155 files in 4 s (2026-10-22).
- [x] Identity: 5,148 files in 4 min 35 s; 222 duplicates merged (NES on the share, SNES .sfc/.smc) (2026-10-22).
- [x] PlayStation product codes read from real discs; mods with the same code kept apart (2026-10-22).
- [~] Scraper: libretro + LaunchBox on NES/SNES OK; no database errors under load (2026-10-22).
- [ ] Scraper: whole library (expect ~30-40 min, Arcade through ArcadeDB ~15 min of it).
- [ ] Scraper: PlayStation collection discs (Arc the Lad Collection) now found.
- [ ] Scraper: pause when a game starts, carry on when back in the menu.
- [ ] Scraper: pull the network cable mid-run → pauses, carries on later, nothing marked "not found".
- [ ] .chd product codes on a real .chd PlayStation disc (convert one with chdman on the PC).
- [ ] Library on the SD card (installed): size after a full scrape (expect ~35 MB + ~65 KB per cover).
- [ ] USB drive unplugged → its games hidden, plugged back → back with art and history.
- [ ] Network share down at boot → games on it come back when it mounts.
- [ ] Rename a game file → same game, same art, favourite and play time.
- [ ] Launch a game from phosphord (`phos launch <id>`) for each system (MGL slot numbers in
      `config/systems.ini` are unverified: PSX, Saturn, MegaCD, PCE CD, NES, SNES, Genesis, N64,
      SMS, PCE, Atari 7800, GB/GBC, GBA, Game Gear, WonderSwan, Lynx, C64, MSX, Atari 800, Arcade).
- [ ] Play time counted when the game is quit back to the menu.
- [ ] Diagnostics through phosphord (`phos '{"cmd":"diag","run":true}'`) with the module installed.
- [ ] Keyed scraper sources, once keys exist: TheGamesDB, ScreenScraper (after registering Phosphor
      for a developer id), IGDB. Their platform numbers are untested.

## 4. Shell (the Phosphor interface)

- [ ] Starts on the framebuffer (1280x720) over Console Mode's menu without breaking it; quits cleanly.
- [ ] Controllers: SS1 pad, 8BitDo Ultimate 2, keyboard; D-pad, stick, A/B/X/Y, shoulders, Start.
- [ ] Home: category counts, "pick up where you left off" with art and play time.
- [ ] Systems → games: fast scrolling through ~300 SNES games; art loads after the selection settles.
- [ ] Game details: JPEG covers (scraped) and PNG covers (imported) both show.
- [ ] Y "Based on" picker: link a TLW Final Fantasy Tactics hack to FFT (USA) → art and description show.
- [ ] Background textures on the TV: each one faint but visible, no seams, follows the accent colour;
      scrolling as fast with a texture as without.
- [ ] Settings: accent colour, clock, box art on/off with progress, Box art sources (order, on/off).
- [ ] Settings → Diagnostics: runs, problems first, X saves a report.
- [ ] Status bar: share, Ethernet, WiFi signal, Bluetooth count, clock (12/24 h).
- [ ] Starting a game: shell sleeps, ignores input; back in the menu: wakes up.
- [ ] Memory and CPU while idle (should be near zero) and while scrolling.

## 5. WiFi (Settings > WiFi)

Back up `/media/fat/linux/wpa_supplicant.conf` by hand first (phosphord also keeps the last 10 in
`phosphor/data/backup/wifi/`). Have an Ethernet cable or the debug port ready in case WiFi drops.

- [ ] Screen shows the current network, signal, band, IP; the list groups the mesh's access points.
- [ ] Join a saved network (A) → stays/reconnects; dhcpcd gets an address.
- [ ] Join a new WPA2 network with the on-screen keyboard; the file has `psk=<64 hex>`, not the password.
- [ ] Wrong password → doesn't connect; SELECT (undo) puts the old file back and WiFi reconnects.
- [ ] Forget a network (X), then undo (SELECT).
- [ ] "Other network..." for a hidden network (`scan_ssid=1`).
- [ ] Open network (e.g. phone hotspot without password).
- [ ] WPA3-only network → says it isn't supported (no change made).
- [ ] wpa_supplicant not running (e.g. after `killall wpa_supplicant`) → joining starts it the way MiSTer does.
- [ ] On-screen keyboard with each controller: D-pad, A types, X deletes, Y switches layers, Start = Done, B cancels.

## 6. Safety checks (each time something is installed)

- [ ] Console Mode still works exactly as before (no Console Mode files touched).
- [ ] `MiSTer.ini`, `downloader.ini`, `user-startup.sh`: only changed where an installer said so,
      backed up first, and put back by the uninstaller.
- [ ] Reboot with Phosphor installed: boots normally, no slow boot.
- [ ] Uninstall everything: SD card back to how it was (compare with the diag report from step 0).
