# Things to test on the SuperStation One

Everything here was built and tested in the sandbox (and some of it on the SS1 over the debug
port), but still needs checking on the real thing. Work down the list; tick items off with the date.
Phosphor Link (debug port) does most of the setup, so most of this can be driven from the Claude app.

Legend: **[ ]** not tested yet · **[~]** partly tested on the SS1 · **[x]** done

## 0. Before starting

- [ ] Back up the SD card (or at least `/media/fat/MiSTer.ini`, `linux/`, `ConsoleMode/`, `Scripts/`).
- [ ] Phosphor Link connects (`phosphor-link status`) and the Claude app sees it.
- [ ] Note the stock state for comparison: `phosphor-link run "bash diag.sh report"` (diag module).
- [ ] After installing or flashing: `/media/fat/phosphor/phosphor.sh check` gives a one-screen summary (what
      runs, what answers: phosphord, the shell, Phosphor's Main, front ports, Bluetooth, WiFi, logs).

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

## 4b. Favorites, Recently played, Search

- [ ] X on a game in any list → "Added to Favorites", the row says Favorite; X again removes it.
- [ ] Home shelf: Favorites and Recently played cards after the categories, with counts; A opens them
      (system name on each row); B goes back to the home screen.
- [ ] Play a game, come back → it's first in Recently played.
- [ ] Y on the home screen → keyboard; type part of a name, Done → matching games from every system; a name
      with no match says so; B on the keyboard cancels.

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

## 5b. Bluetooth (Settings > Bluetooth)

- [ ] The list shows the controllers already paired (from MiSTer's own pairing) with Connected / Not connected.
- [ ] Pair a controller: put one in pairing mode → "Pairing...", "Connecting...", then "Paired <name>"; it works in
      the shell and in a game. The same controller paired again later works too (MiSTer's tool re-pairs it).
- [ ] Stop looking while nothing is in pairing mode → "Cancelled"; MiSTer's OSD BT pairing still works afterwards.
- [ ] A on a controller → A Disconnect (it reconnects when turned on again) / X Forget (gone from the list; MiSTer's
      OSD no longer knows it either).
- [ ] The status bar's Bluetooth count matches the connected controllers.
- [ ] Phone app 0.5 > Settings > Bluetooth: same list; Pair a controller from the phone while holding the controller in pairing mode; Forget.

## 6. Screen filters (Settings > Screen filters)

Before: copy `/media/fat/config/*_scaler.cfg`, `*_shmask.cfg`, `*_gamma.cfg` somewhere safe.
NES already has its own settings (a scanlines file in the vertical slot), so it's a good first test.

- [ ] The list shows each system with an installed core; ones with their own settings show the filter names,
      the rest say "MiSTer.ini defaults".
- [ ] NES screen shows the same values the OSD shows for NES now.
- [ ] Apply a preset (e.g. one in Presets/General Hardware), start the NES core: the OSD's Video processing page
      shows the preset's filters, and the picture matches.
- [ ] Change one slot only (e.g. Shadow mask) → the other slots stay as they were.
- [ ] Set a slot to Off → the OSD shows it off for that core.
- [ ] Shadow mask size (left/right): off, 1x, 2x, 1x rotated, 2x rotated → matches the OSD.
- [ ] SELECT (undo) after each change → files go back exactly (compare with the copies); for a system that had
      no files of its own, undo removes them again.
- [ ] Change something in the OSD while in a game, back in the menu → Phosphor shows the new values.
- [ ] Arcade isn't listed (MiSTer keeps arcade settings per game, not per core).
- [ ] Backups in `phosphor/data/backup/video/`.

## 7. SD card image (spare card only)

Use a spare card, not the SS1's card. Image: `dist/phosphor-<v>-sdcard-<date>.img.zip` (`image/build-image.sh`).

- [ ] Flash with balenaEtcher (or Raspberry Pi Imager); the PC then shows a `MRFUSION` drive.
- [ ] Optional: put a `wpa_supplicant.conf` on it → WiFi works after the install.
- [ ] Boot: Phosphor install screen shows on HDMI (the SS1's video path), card is expanded, it restarts
      by itself (under ~2 minutes).
- [ ] After that restart it boots into Phosphor: MiSTer's menu shows for a few seconds, then the Welcome
      screen (first start). Every later boot goes straight to the library.
- [ ] `ps` shows `MiSTer_phosphor`, not `MiSTer`; `/media/fat/MiSTer` is unchanged (same size as MiSTer's).
- [ ] Quit the shell (`killall phosphor`) → MiSTer's menu; OSD > Launch Phosphor brings it back.
- [ ] Card in the PC afterwards: one big `MiSTer_Data` exFAT partition, the full size of the card.
- [ ] `/media/fat/phosphor` is there, `linux/user-startup.sh` has the phosphor block,
      `phosphor.sh status` shows phosphord running after boot.
- [ ] `update_all` / downloader runs normally afterwards and doesn't remove Phosphor.
- [ ] `/media/fat/phosphor/uninstall.sh` leaves a stock MiSTer (next boot: MiSTer's Main and menu only).
- [ ] SS1 specifics after update_all (SS1 cores, dock port, NVMe) work as on a normal stock install.

## 8. Remote access (Settings > Remote access)

From the PC (same network), with curl; the commands are in `docs/remote-api.md`.

- [ ] Off by default: `curl http://<ip>:8767/info` gets no answer.
- [ ] Turn it on: the screen shows the MiSTer's address; `/info` answers.
- [ ] Pair a device: code on screen; `POST /pair` with it → token; the screen closes and says "Paired".
- [ ] Wrong code 5 times → pairing closes. Code after 2 minutes → refused.
- [ ] With the token: `games`, `game`, `edit` (check the new details on the TV), `PUT /art` with a phone
      photo (shows on the TV), `GET /art`, `launch` (the game starts).
- [ ] `wifi` and `remote` refused remotely.
- [ ] Forget the device (X) → its token gets 401.
- [ ] Reboot: still on, device still paired (`phosphor/config/remote.ini` has hashes, not tokens).
- [ ] While a game runs, the remote API still answers and doesn't slow the game.

## 9. Claude library tools (Phosphor Link 0.2, Claude desktop app on the PC)

Replace `phosphor-link.exe` with 0.2.0 (quit Claude first), restart Claude.

- [ ] Ask Claude to pair: it asks for the address and code from the TV; pairing works.
- [ ] "How's my library?" → library_status numbers match the TV.
- [ ] "Find games without box art and fix three of them": Claude checks CRC/product code, finds the right
      region's front cover, uploads it; the covers show on the TV.
- [ ] "Fill in the details for <game>": description in its own words, year/developer/publisher correct.
- [ ] A hack (e.g. an FFT TLW version): Claude links it to the original instead of copying art.
- [ ] Forget the PC on the TV → the next tool call says to pair again.

## 10. Phosphor for Android (Pixel 10)

Install `phosphor-android-0.1.0-debug.apk` (allow installing from the file manager). Phone on the same WiFi.

- [ ] Connect: the MiSTer shows up under "On this WiFi" (or type the address); pairing with the TV's code works;
      wrong code shows "wrong code".
- [ ] Home: game count, "Needs attention" numbers match the TV; systems list.
- [ ] A system's games: covers load while scrolling; search and the filter chips.
- [ ] Game: details and files; Play on TV starts it; Favorite shows on the TV.
- [ ] Change box art from a phone photo → shows in the app and on the TV.
- [ ] Edit details, save → shows on the TV; clearing a field clears it.
- [ ] Based on: link a hack to its original, then remove the link.
- [ ] Forget the phone on the TV (X) → the app goes back to pairing on its next call.
- [ ] Close and reopen the app: still paired (key kept encrypted).
- [ ] Dark status/navigation bars, text readable, big font setting doesn't break layouts.
- [ ] (0.4) Settings > Controllers & memory cards: with the stock menu core "Turn on the front ports" loads the
      Phosphor one; the controller lines follow buttons and sticks; Back up port 1 shows progress and the new
      backup appears; Put back > Port 1 on a spare card works (backed up first).

## 11. Cloud saves (cloud-sync module)

On the MiSTer: `modules/cloud-sync/install.sh`, then `cloud-sync.sh setup`. A test place: a folder on the PC
shared by WebDAV or SFTP, or a Google Drive token (`rclone authorize "drive"` on the PC).

- [ ] `setup` downloads rclone (arm-v7) and says the checksum checked; `rclone version` runs on the SS1.
- [ ] `add` + `use` + `test` → OK. Memory while sending (`top`): rclone stays well under 120 MB.
- [ ] `push`: saves, savestates, config appear under `Phosphor/MiSTer-xxxxxx/current`.
- [ ] Play a game that saves (same size save), back to the menu → sent ~10 s later (log in `/tmp/phosphor/cloud-sync.log`);
      the old version is in `history/<date>`.
- [ ] While a game runs nothing is sent; a pull is refused.
- [ ] `pull --dry-run`, then `pull` after deleting a save locally → it comes back; replaced files in `phosphor/backup/cloud-sync`.
- [ ] Unplug the network, leave a game → "pending"; plug it back → sent within the hour.
- [ ] Boot: sends once after the network is up; boot time not slower.
- [ ] `uninstall.sh` → user-startup.sh as before; settings and cloud copies kept.

## 12. Cloud saves screens (TV and phone)

- [ ] TV, Settings > Cloud saves: "Needs rclone" → Download rclone → ready.
- [ ] Add a WebDAV/SFTP/SMB place with the on-screen keyboard → chosen automatically; Check the connection → OK.
- [ ] Send now → "Saves sent"; the line under the title shows the time.
- [ ] Get saves back → this MiSTer → preview list → A → "Saves are back". Nothing to restore → "OK" only.
- [ ] Phone, Settings > Cloud saves: same status; Send now; Get saves back with preview.
- [ ] Phone: add Google Drive with a token from `rclone authorize "drive"` on the PC → sends to Drive.
- [ ] A wrong password → Check the connection says why; nothing else changes.

## 13. Welcome screen (first start)

Rename `/media/fat/phosphor/config/frontend.ini` (or use the SD card image) and start the shell.

- [ ] Welcome shows instead of the library; Network shows Ethernet / the WiFi name.
- [ ] Network → WiFi screen; B comes back to Welcome (not Settings). Same for phone app and Cloud saves.
- [ ] Box art On/Off and Accent colour change right away.
- [ ] Start → library; restarting the shell goes straight to the library.
- [ ] An existing install (frontend.ini present) never shows Welcome.

## 14. Backups screens (TV and phone; backup-agent module 0.2)

Set the backup folder to the NVMe first (`DEST` in `phosphor/config/backup-agent.ini`).

- [ ] TV, Settings > Backups: list with date, reason (After SNES, Daily, ...) and size; Back up now adds one.
- [ ] Pick one: the saves in it are listed; A restores saves only (WiFi/settings unchanged), a "Before a restore" backup appears.
- [ ] X (Everything) puts settings back too; restoring the "Before a restore" one undoes it.
- [ ] While a game runs, a restore is refused.
- [ ] Phone, Settings > Backups: same list; Back up now; Restore saves / Everything.

## 15. phosphor-main (Main fork, phase 1)

Only by hand, undone by a reboot. `MiSTer_phosphor` is in `/media/fat/phosphor/bin/` after installing the release.

- [ ] `killall MiSTer MiSTer_ConsoleMode; /media/fat/phosphor/bin/MiSTer_phosphor &` → MiSTer's menu comes up as usual
      (the stock menu; Console Mode is back after a reboot).
- [ ] `/media/fat/phosphor/phosphor.sh start` → the shell shows **without F9**; the log says "the shell has the screen".
- [ ] Controller in the shell: Main's menu doesn't react behind it (no file browser popping up, no double moves).
- [ ] Menu button in the shell: Main's OSD doesn't open.
- [ ] Start a game from the shell; in the game the menu button opens the OSD as usual; quitting the core
      comes back to the shell (not Main's file browser).
- [ ] `killall phosphor` (the shell) → Main's own menu comes back by itself.
- [ ] MiSTer's OSD list (Core, Define buttons, Video processing...) has **Launch Phosphor** under Video processing:
      in the menu it starts the shell and the shell takes the screen.
- [ ] In a game: OSD > Launch Phosphor → the menu core loads and the shell comes up a few seconds later.
- [ ] System Settings is unchanged (Reboot, hold for cold reboot, Exit as usual).
- [ ] Start the shell again → it takes the screen again.
- [ ] `echo status | socat - UNIX:/tmp/phosphor-main.sock` (or `nc -U`) answers with the core and owner.
- [ ] Reboot → stock/Console Mode boot as before (nothing was changed on the card).

## 16. Front ports in the menu core (phosphor-menu + phosphor-main)

Needs `MiSTer_phosphor` running (section 15) and `menu_phosphor.rbf` in `/media/fat/phosphor/cores/`.
Nothing on the card is replaced: the Phosphor menu core is loaded by hand, and a reboot or loading
any other core puts the stock one back. Use a memory card whose saves are backed up.

- [ ] `echo "launch /media/fat/phosphor/cores/menu_phosphor.rbf" | socat - UNIX:/tmp/phosphor-main.sock`
      → the menu comes back as usual (same look; the shell takes the screen again if it's running).
- [ ] `ports` → `"available":true,"on":false`. With the stock menu core (after a reboot) → `"available":false`.
- [ ] With an MT32-pi or MIDI cable (if you have one): works as before while the ports are off.
- [ ] Keep one socat open (`socat - UNIX:/tmp/phosphor-main.sock`), type `ports on`, then `pads` a few times
      while pressing buttons and moving the sticks on the controller in port 1, then port 2:
      buttons and IDs change (65 digital, 115 analog).
- [ ] `pad_analog 1` with a DualShock in digital mode → `pads` shows 115 and the sticks move.
- [ ] Unplug a controller → `"connected":false` within a second; plug it back → true.
- [ ] `card_read 1 0` with a card in port 1 → data starting `4d43` ("MC"). Port without a card → `no card`.
- [ ] `card_read 1 1023` works; a few hundred reads in a row without errors (no `bad reply`).
- [ ] Write test on a **spare** card only: `card_read 1 1022`, write back the same data with
      `card_write 1 1022 <data>` → `ok`; read again → same data. The card still works in a game.
- [ ] Close the socat (Ctrl+C) → the ports go off by themselves (`ports` from a new socat says `"on":false`).
- [ ] Load a game core and come back: the ports are off and the menu core is the stock one again
      unless Phosphor loads it again.
- [ ] Controllers keep working in the shell the whole time (front ports on or off).
- [ ] Shell: Settings > Controllers & memory cards with the stock menu core → "Loading the Phosphor menu
      core...", the screen goes to MiSTer's menu for a moment and the shell comes back by itself.
- [ ] The two port lines follow the buttons and sticks live; unplugging shows "nothing plugged in".
- [ ] "Switch DualShocks to analog" with a DualShock in digital mode → the line says DualShock (analog).
- [ ] "Back up the memory card in port 1" → progress, then "Saved as port1-....mcd"; the file is 128 KB in
      `/media/fat/phosphor/memcards/` and loads as a card in the PSX core (copy it to `saves/PSX/` to try).
- [ ] Spare card: pick a backup → A (port 1) → it backs up the card first, then writes; the card works in a
      game afterwards. The backup made just before is in the list.
- [ ] Leave the screen with "Use them to move around Phosphor" Off: about 20 s later `ports` (socat) says `"on":false`.
- [ ] With it On (default): a PS1 controller in a front port moves around Phosphor (D-pad and left stick,
      Cross confirm, Circle back, Square X, Triangle Y, Start = Settings, L1/R1 page); held directions repeat;
      USB/BT controllers still work at the same time. Fresh boot: the shell loads the Phosphor menu core once
      (MiSTer's menu shows for a moment).
- [ ] With phosphor-main and the Phosphor menu core installed, leaving a game loads it instead of `menu.rbf`
      (front ports work right away); deleting `phosphor/cores/menu_phosphor.rbf` brings the stock one back.

## 17. Safety checks (each time something is installed)

- [ ] Console Mode still works exactly as before (no Console Mode files touched).
- [ ] `MiSTer.ini`, `downloader.ini`, `user-startup.sh`: only changed where an installer said so,
      backed up first, and put back by the uninstaller.
- [ ] Reboot with Phosphor installed: boots normally, no slow boot.
- [ ] Uninstall everything: SD card back to how it was (compare with the diag report from step 0).
