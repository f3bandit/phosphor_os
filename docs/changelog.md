# Changelog

Changes between test images. The image build copies the newest section into the notes file it
writes next to each image (`dist/phosphor-<version>-sdcard-*.txt`).

## 0.27.42

### New
- **Wallpapers:** Settings > Wallpapers turns them on (Background texture waits while they're on), and
  Choose wallpaper opens a picker: L and R switch between your own (in `wallpapers_user`) and each
  Update All creator's collection you have (Ranny Snice, Anime0t4ku, PCN Challenge, RGarciaLago), Y
  shows 16:9, 4:3 or all, and X picks where it goes: both menus, Phosphor's, or MiSTer's menu (OSD).
- **Online updates:** Phosphor checks GitHub for a newer release (a minute after starting, then every
  half hour, never in a game). A yellow icon in the top bar says there's one; Settings > Updates
  downloads it, checks it, and installs it with the usual restore point.
- **Home > Other and Utility:** the cores in `_Other` and `_Utility` get their own cards. Utility starts
  with **Register this SuperStation**, which opens Update All so you can sign in and Link your supporter
  cores (Coin-Op Collection, JOTEGO and others).
- **Start with...:** SELECT on a game lists every core that can run it (the newest, older versions,
  unstable builds, another system's core, the RetroAchievements one); X makes your pick the default for
  that system.
- **Power menu > MiSTer menu (OSD)** opens MiSTer's own menu and settings.
- **Box-art scraping uses Phosphor's own ScreenScraper developer ID**, so scraping works with just your
  ScreenScraper account.
- **CRT mode (Settings > Video):** Phosphor's menu on a CRT, out of the analog port at the TV's own
  timing: 240p, 288p, 480i or 576i (15 kHz) for a TV, 480p (31 kHz) for a VGA monitor or PVM / BVM. HDMI
  shows the same picture. A new mode goes back by itself in 20 seconds unless you keep it, and the picture
  can be moved left, right, up and down. At 240p and 288p the menu has its own layout with bigger text,
  in three looks (Text: Smooth, Sharp, Legible); Overscan keeps it inside a TV's visible area. In a game,
  the in-game menu shows in MiSTer's menu window, so it's on the CRT too. Needs this release's Main and
  menu core.
- **4:3 screens:** at 640x480, 1024x768 and other 4:3 resolutions every screen lays itself out for the
  narrower screen, and Phosphor starts again by itself when MiSTer.ini's video mode changes.
- **Save a copy of a disc:** with a PlayStation, Saturn, Mega-CD or PC Engine CD disc in the drive, X on
  the disc row saves it as .bin / .cue next to that system's games, checked against Redump's list and
  named after it when it matches. B twice stops it.
- **Phosphor for Windows updates itself:** it checks GitHub for a newer version, and About downloads,
  checks and installs it. Both apps also say when there's a new Phosphor for the SuperStation, and can
  download it.

### Changed
- **The menu button in a game opens Phosphor's in-game menu on every press;** holding it no longer opens
  MiSTer's menu (use the in-game menu or the power menu for that).
- RetroAchievements signs in after the game has loaded and been checked, so starting a game is quicker.
- **Wallpapers load much faster:** the picker's previews are made in the background, a page in a few
  seconds, and the menu redraws as soon as you pick one.
- Fixed: a hybrid ARM core (Street Fighter III 3rd Strike) could restart the SuperStation after an update.
- Fixed: **an NVMe or USB drive stayed "marked dirty" on every start** after one restart without a clean
  stop (Windows then asks to scan it). Linux never clears that flag on a drive that was dirty when it was
  mounted, so Phosphor now checks such a drive (fsck.exfat) when it starts, before using it
  (logs/drives.log). The SD card can't be checked while MiSTer runs from it: let Windows scan it once.

## 0.27.41, Phosphor for Windows 0.2.43, Phosphor for Android 0.9.8

### New
- **In-game menu > Load game:** every installed core, then that core's games (Y searches them); A twice
  starts one straight from the game you're in. B goes back a step at a time (games, cores, the menu).
- **In-game menu > MiSTer menu (OSD)** opens MiSTer's menu at once (it needed another press).
- **RetroAchievements without waiting:** your progress and a game's achievements show at once from what
  was last fetched, and refresh while RetroAchievements is asked again; a slow or unreachable site no
  longer holds up the TV or the apps.
- **.chd discs** (PlayStation, Mega-CD, PC Engine CD...) are matched to RetroAchievements.
- **Remote access serves several devices at once;** a device that sends wrong keys is locked out for a
  moment without slowing anyone else.
- **Box-art logins stay off the network:** the SS1 no longer sends your ScreenScraper, IGDB or
  TheGamesDB passwords and keys to the PC. Phosphor for Windows keeps its own copy (in Windows'
  protected store): type each one once in Settings > Box art.


### Licences
- Every download now carries the licences of what's in it: THIRD_PARTY_NOTICES.md (the libraries and
  fonts in Phosphor and the apps, in full), and the GPL-3.0 text with SOURCE.md, which links the exact
  source of Phosphor's Main and menu core. Both forks (phosphor-main, phosphor-menu) are public;
  SOURCES.md in this repository does the same for every earlier release. The SD card image has a
  LICENSES folder: where the source of MiSTer's Linux, kernel, u-boot, Main, menu core, Mr. Fusion and
  Update All is. About (TV, Windows, Android) points to all of it.
- Phosphor's programs on the SS1 are built with musl (MIT) instead of glibc.
- The "mario" and "zelda" background textures are gone (Nintendo's art); if one was picked, it's none.
- The fan-made box art and the RetroAchievements unlock sound are credited to their makers, with a
  disclaimer that they aren't Phosphor's work.

### Fixed
- Phosphor for Windows: the window opens again after Exit (Edge kept it hidden).
- Security: the apps send their key only to an SS1 that proves it already has it (when they look for it
  on the network); the remote API answers that proof without the key being sent.
- Library: a scan stopped partway, or a drive with folders it couldn't read, no longer removes games.
- WiFi: a network name or password with } in it no longer breaks the saved networks.
- Controllers: a backup that couldn't be written (a full card) stops restore and reset from deleting.
- Memory cards: on a damaged card, deleting one save no longer clears another's; a restore writes the
  card's directory last.
- Updates: rolling back to the oldest kept version works; an update's menu core never replaces
  MiSTer's saved menu.rbf; update.sh isn't overwritten while it runs.
- The shell: R on an empty list (a crash), a direction held while a game ended repeating forever, a
  launch sent twice, Left on Get box art starting a scrape, the keyboard or a menu left open over the
  in-game menu, and a few smaller ones.
- Phosphor's Main: a direction held as the in-game menu closed kept repeating; a mapping could be
  cancelled by a Start press left over from the last one.
- phosphord: network lookups no longer hang while music plays; box art undo keeps a cover other discs
  use; RetroAchievements tries again games it couldn't fingerprint; bad disc images are refused safely.
- Phosphor for Windows: Steam lists only soundtracks and reports every album that failed; SD card
  writing checks the disk's size and id and fails on a short image; backup names are checked.
- Phosphor for Android: turning the phone no longer reloads the app; photos upload the right way up;
  About shows the real version.

## 0.27.40

### Fixed
- **The dock's NVMe drive is found on a cold start.** A drive that took more than 15 seconds to come
  up after switching on was removed as the dock's empty-slot "phantom" drive by a rule in the SS1's own
  Linux image, and stayed missing until a restart. Now:
  - that rule is turned off (put back by uninstalling Phosphor), and Phosphor waits a minute for a
    drive before it treats the slot as empty;
  - if the drive goes missing anyway, Phosphor asks the dock to look again, and it comes back and
    mounts without a restart (once; an empty slot's phantom is still removed);
  - the kernel's log of each boot is kept in `phosphor/logs/boot-kernel.log` (and the boot before in
    `boot-kernel.prev.log`), so a drive that doesn't show up can be looked into afterwards.

## 0.27.39, Phosphor for Windows 0.2.38, Phosphor for Android 0.9.7

### Added
- **Screenshots.** Press both sticks in (L3 + R3) to save a picture of the screen. In a game it's
  MiSTer's own screenshot of the game, with its "Screenshot saved" note; on Phosphor's screens, and with
  the in-game menu up, Phosphor saves what's shown. They're on the SD card in `screenshots/<system>/`.
  The presses still reach the game. Settings › Screenshots: turn the combo off, see them in a grid,
  one full screen, delete. Phosphor for Windows (Tools › Screenshots) and Android (More › Screenshots):
  take one of the TV, see them, save them to the PC or the phone's photos, share, delete.
- **Settings › Shut down safely**, between Restart and About: the same safe shutdown as Select + Start's
  power window (drives finished with first, then "Safe to switch off"). Press A twice.

### Changed
- **Album covers** are made TV-sized from the album's picture, or from the art inside its tracks (FLAC
  and MP3), so big covers show and albums with art only in their files get one.
- **Steam soundtracks** come with Steam's own details: the English title, the artist (and composer and
  label) and Steam's album cover. Albums sent before get them too, without sending the music again.
- **The in-game menu** shows the game that's really running, also one loaded from MiSTer's own menu
  (its box art and details when it's in the library, else its file name, or the core's name).

### Fixed
- RetroAchievements: a Web API key RetroAchievements doesn't accept says so, instead of matching 0
  games; games that couldn't be looked up are looked up again; a key has to be 32 letters and numbers.
- Library › Disc: a music CD says it plays in Music.

## 0.27.38, Phosphor for Windows 0.2.31, Phosphor for Android 0.9.6

### Added
- **Music.** Phosphor's own music player, with its own tile on the home screen and a Now playing card:
  albums from a `Music` folder on the SD card or a USB drive, a music CD in the dock's drive (names and
  year from MusicBrainz, the cover from the Cover Art Archive), and Steam soundtracks sent from Phosphor
  for Windows (it reads Steam's own records on the PC; nothing signs in to Steam). Plays only while no
  game is running. Copy a music CD to FLAC from the TV (Music › the CD › X). Control it from the TV,
  the remote, Phosphor for Android (More › Music) and Phosphor for Windows (Tools › Music).
  See [docs/music.md](music.md).
- **RetroAchievements.** odelot's RetroAchievements Main and cores, with rcheevos: achievements while
  you play NES, SNES, Game Boy, GBA, N64, PlayStation, Mega Drive, Mega-CD, Master System, Game Gear,
  PC Engine and Atari 7800 games. Settings › RetroAchievements on the TV: log in (only a token is
  kept, never the password), hardcore, pop-ups, unlock sound, get the cores, match the library, your
  progress. The in-game menu lists the game's achievements; game pages show "Achievements N / M".
  Phosphor for Windows and Android: the same settings and progress, and a game's achievements.
  See [docs/retroachievements.md](retroachievements.md).

### Removed
- **Phosphor's own SSH settings.** Pairing with the code is all remote access needs, and the apps
  reconnect by themselves. Gone: SSH Off / Password / Keys only, the SSH password and SSH keys in
  Settings › Remote access, keys offered from a paired device (and the TV's "let this key log in?"), and
  the copy of remote access kept on another drive. Phosphor for Windows: the SSH section in Settings ›
  Connection. Updating puts MiSTer's own SSH back (its own password and keys) and removes
  `phos-ssh.sh` and `phosphor/config/ssh`. A copy kept on a drive (`phosphor-keep`) stays there.
  After reflashing an SD card, pair the phone and PC again with the code.

## 0.27.37, Phosphor for Windows 0.2.30, Phosphor for Android 0.9.5

### New
- **Discs from the dock's CD/DVD drive** (#135). PlayStation, Saturn, Mega-CD, PC Engine CD, Neo Geo CD,
  CD-i and 3DO discs, and MSU-1 / MD+ discs. Disc support is Main_MiSTer_Physical_Disc by **Anime0t4ku and
  bonna79** (GPL-3.0), merged into Phosphor's Main.
  - Put a disc in and open **Library › Disc**: it's at the top as **In the drive**, with its name (your
    library's game with the same disc code, else libretro's lists) and box art. A plays it, Y ejects.
  - A disc icon in the status bar while one is in. Nothing starts by itself, also at power-on.
  - **Play last** (X) plays the disc when it was the last game and is still in; otherwise the last game
    from storage.
  - **Settings › Discs**: Discs on/off (off: the drive isn't watched or read at all), the drive, Eject.
  - Music CDs and DVDs are recognised but not played yet.
  - Phosphor for Windows and Android show the disc in Library › Disc (Play on TV, Eject) and have
    Settings › Discs. phosphord's remote access has `disc`, `disc_play`, `disc_eject`, `disc_enable` and
    `GET /art/disc`.
  - Upstream's own launcher (Auto Disc Discovery) stays off: `PHYSICAL_DISC_MGL=1` in MiSTer.ini turns it on.
- **Phosphor's in-game menu.** In a game, a short press of the menu button opens Phosphor's menu over it:
  the game's box art and description, Back to the game, Change disc (disc games), MiSTer menu, Quit to
  Phosphor. The game keeps running behind it (the screen behind the menu is black). Hold the button for a
  count of 3 Mississippi and let go for MiSTer's own menu, as before. Not much longer on an 8BitDo
  controller: it turns itself off.
- How it fits together and what was decided: `docs/discs.md`.

### Not tested yet
- Game discs: only a music CD has been in the drive so far. Tests with PlayStation discs come next.

## 0.27.36, Phosphor for Windows 0.2.29, Phosphor for Android 0.9.4

### Changed
- **The SS1's video ini files are set to Taki's settings** (#165). The SuperStation has its own video
  hardware: a Sony encoder for S-Video and composite that needs `vga_mode=subcarrier`. Taki ships
  `MiSTer.ini` plus four profiles (RGHV, RGsB, SVID, YPbP). The installer now edits just the video lines
  (`vga_mode`, `composite_sync`, `vga_sog`, `video_info`, `logo`) to his values, in place, adds a missing
  profile, and renames `MiSTer_example.ini` to `MiSTer_example.ini.txt`. Everything else in each file
  (`main=`, HDMI settings, per-core sections) stays. Copies go to `phosphor/backup/video-ini-<date>`. It
  runs once per card: a fresh install, the first update with it, and the SD image. After that your own
  changes stay. The list of changes is in `phosphor/backup/video-ini-taki.done`.
- **Four video profiles** instead of three. MiSTer's Main offers only the first three `MiSTer_*.ini` it
  finds, so one of Taki's four was always hidden (two, with the example file). Phosphor's Main and
  Settings › Video (TV, Windows, Android) now offer all four. MiSTer's own OSD picker still shows three.
- New card: `MiSTer.ini` starts from MiSTer's example (either name) and gets Taki's video settings.
- Why and where, for anyone changing this later: `docs/ss1-video.md`.

### Fixed
- **Update Phosphor from MiSTer's Scripts menu** (`update_phosphor.sh`) ran MiSTer's own updater instead
  of Phosphor's. It looked for `*/update.sh` in the package, which since 0.26.4 also matched
  `scripts/update.sh`. It stopped there without changing anything; it now takes the package's own
  `update.sh` by its exact path. Settings › Update Phosphor (TV and apps) wasn't affected.

## 0.27.35, Phosphor for Windows 0.2.28, Phosphor for Android 0.9.3

### Changed
- **ScreenScraper's login works like Console Mode's** (#137). Every lookup needs a developer ID (Phosphor's
  own once ScreenScraper gives it, built in from a build secret; until then your own) and your
  screenscraper.fr login. Your login is no longer sent as the developer ID: ScreenScraper always refused
  that. A missing part is named; a refused developer ID, a refused login, a used-up day, maintenance and a
  blocked version each get their own message and stop ScreenScraper for the rest of the run; three failed
  requests in a row stop it too. The other sources carry on. **Check login** (Windows and Android, Box art ›
  Sources › ScreenScraper) asks ScreenScraper whether it works and how many lookups are used today.
  `scrape_progress` gives the SS1's own run a `note` with the reason a source stopped.
- **Translated titles find the original** (#142), in matching to base games and in the SS1's "Based on"
  list: the same subtitle under another main title (Demon Child Zenki / Kishin Douji Zenki - Battle
  Raiden), two telling words in both parts with the numbers matching (Mysterious Dungeon 2 - Shiren /
  Fushigi no Dungeon 2 - Fuurai no Shiren), one long word spelt one letter differently (Nankoku /
  Nangoku). Each only when it points at one game; checked against a real list of 151 misses (7 found, no
  wrong ones).

## Phosphor for Windows 0.2.27

### Changed
- The hints at the bottom name the PC's keys (Enter, Esc, X, Y, Q/E, M), drawn as keys: the controller
  in the SuperStation's ports can't drive the Windows app. A controller plugged into the PC still
  works; once it's used, the hints name its buttons, until a key is pressed again. M now does what Start
  does on a controller (Keep all on Matched automatically), and opens Settings elsewhere as before.

## 0.27.34, Phosphor for Windows 0.2.26, Phosphor for Android 0.9.2

### New
- **Ports**: a game ported from another system (`Super Mario Bros. (SNES Port)`, `Zelda II NES2SNES`,
  `(NES Conversion)`) is matched as the game it came from: linked to the NES original when it's in the
  library, otherwise looked up in the NES lists for its box art and details. Listed in Matched
  automatically as "Port", with Keep and Undo. `link` takes `"port":true` to link across systems.
- **The SS1 asks the fan art list too** (its own lookup, before the other sources), with the backup pack
  when a cover's link is gone.
- **Android catches up with Windows**: fan art first, matching to base games (Match all automatically,
  Matched automatically with Keep, Undo and Pick another, Not a game / It's a game on a game, the Not
  games filter), LaunchBox names compared loosely, libretro's Satellaview list for SNES.

## Phosphor for Windows 0.2.25

### New
- Fan art list: 66 more covers for ROM hacks, fan translations, homebrew and prototypes (Satellaview
  games, SNES ports of NES games with MSU-1, Game Boy homebrew from itch.io, RiQa's N64 prototypes ...),
  each credited in `fanart/README.md`. A game's exact name is matched first ("BS Zelda ... (Part 3)"),
  then its name with tags and versions out, only when that points at one cover.

## Phosphor for Windows 0.2.24

### New
- **Fan-made box art** for games no database has (ROM hacks, homebrew): `apps/windows/fanart/fanart.json`
  lists each game, where its cover was posted, and who made it. Box art lookups ask it first, by the
  game's name with tags and versions taken out, so every version of a hack gets its cover (Roll-chan 2
  (MM1), (MM8) and (MM11) all get Roll-chan 2's). The list is read from GitHub, so new entries reach
  everyone without an update. When a cover's link stops working, Phosphor's copy in `fanart/pack.zip` is
  used (downloaded once, only then). Every cover is credited in `fanart/README.md` and in the pack.
- First entries: the Roll-chan series (1–6 by Zynk Oxhyde, No Constancy by Sfon).


### New
- **Better matching to base games** (Phosphor for Windows 0.2.23), from a dry run against a real library:
  - names made readable first: underscores, dots, joined words and trailing version numbers
    (`Zeldas.Adventure.DX.v2.0.0`, `SuperMarioBrosMini`, `Glory Hunters 1.3`), a system's short name stuck
    on the end (`kid-icarusnes`), "- Disc 2";
  - compared loosely: Roman and Arabic numbers the same (Final Fantasy 2 / II), hyphens and spaces ignored
    (Romancing SaGa / Sa-Ga, Dragon-Ball), a subtitle spelt a little differently (Super Saiyan / Saiya
    Densetsu); a main title alone only when the lists have one game under it, and never when both
    subtitles share nothing (Tengai Makyou - Ziria isn't Deden no Den); the same disc number first;
  - more tags: `(English v1.2)`, ENG DUB, Rebalanced, Major Fixes, Black Label, SA-1, Special Version,
    Arcade Colors, 6-Button Support, Master Patched, Un-Worked Designs;
  - every source is asked by name while matching (LaunchBox's alternate names too, compared loosely:
    Treasure Of Rudras -> Rudra no Hihou), the game's own name included; libretro's Satellaview list for
    SNES (also on the SS1).
- **Not games**: boot files, test ROMs, cheat discs and tools (`mister-boot`, BtnTest, PadTest, tonyhax,
  UNIROM, GameShark, service discs, trainers) are set aside by matching, or by hand with **Not a game**:
  out of Not found, no box art looked up, still in the library. Library › Match has a **Not games** tab to
  put one back. phosphord: `not_game`, filter `not_game`.

### Fixed
- Library › Match: the tab could switch back to Not found by itself while a game was chosen.

## 0.27.32

### New
- **Matching to base games, automatically** (Phosphor for Windows 0.2.20, Library › Match). Translations
  (`[T-En]`), hacks (`(Hack)`, `[h]`, Kaizo) and fan versions (Redux, FastROM, Undub, MSU-1, Redesign,
  v1.1 ...) that box art couldn't find are matched by what their name says: linked to the base game when
  it's in the library, otherwise given the base game's box art and details from the sources. Games the
  lists know under a longer or shorter name are matched too (only when the lists leave one game). Runs at
  the end of Look up box art on this PC, or from the Match page (Match all automatically).
- **Matched automatically** tab: every match with what said so and what was done; Keep, Undo, Pick
  another, Keep all (Start). Undo takes out only what the match added (the link, or the box art and the
  details it filled in) and the game isn't matched automatically again. Kept on the PC in
  `%APPDATA%\Phosphor\automatch.json`.
- phosphord: `scrape_todo` with `miss` (the not-found games) and `scrape_undo`.

### Changed
- Library › Match: LB/RB switch between Not found and Matched automatically (they jumped between systems).
- Settings › Box art: a **Review N matches** button under the lookup buttons, straight to the matches
  (Phosphor for Windows 0.2.21).

### Fixed
- Phosphor for Windows 0.2.22: a tile under the mouse turned see-through, showing the background texture
  through it; the hover tint now sits on the tile's own colour.

## 0.27.31

### Changed
- **The heartbeat log is gone.** phosphord no longer writes `phosphor/logs/heartbeat.log` every 10 seconds
  (a testing aid for the sudden resets, now covered by Diagnostics > Power). The old log files are deleted
  when phosphord starts.

## 0.27.30

### New
- **MiSTer & Linux backups in both apps** (like SS1 Tool's): every version of the settings files
  (MiSTer.ini and its profiles, downloader.ini, u-boot.txt, user-startup.sh, Phosphor's settings, the
  network address, name and SSH settings) and the system files (MiSTer's and Phosphor's Main, the menu
  core, the Linux kernel, and on the PC Linux itself), kept on the PC (Settings > Power & updates) and on
  the phone (More > MiSTer & Linux backups). Checked every half hour while the app runs; only files that
  changed are copied. A settings file that changed when Linux did "looks reset" and gets a notification.
  Any version can be put back: phosphord checks it arrived whole and keeps the file as it was first
  (phosphor/backups/sysbk). WiFi, SSH keys, pairings and logins are never copied (phosphord's `sysbk`).
- Phosphor for Windows 0.2.19, Phosphor for Android 0.9.1.

## 0.27.27

### New
- **Video on the TV** (Settings > Video): MiSTer's own video settings, each explained (the same list as
  the apps and SS1 Tool). Left and right pick a value, A tries it, X tries the settings from before the
  last change, L and R switch the profile shown (MiSTer.ini and the MiSTer_*.ini ones); one not in use
  can be tried as a whole. Analog settings say which DIP switches on the side of the SS1 they need.
- **The Keep window**: whenever a video change is being tried (from the TV, the phone or the PC), the TV
  shows what's being tried and the seconds left: A keeps it, B puts it back now.
- **Phosphor for Windows 0.2.18**: Settings > Video is the same try-out (the screen filters are gone).

### Changes
- The screen filters (per-system filters, scanlines, shadow masks, gamma) are gone from phosphord and the
  apps: MiSTer's own OSD still has them for each core.
- **The apps take the TV's look**: phosphord's `look` gives the accent colour and background texture picked
  on the TV (Settings > Look), and Phosphor for Android (and next Phosphor for Windows) use them.
- **Phosphor for Android 0.9.0**: rebuilt in SS1 Tool for Android's layout (app bar, chips, two-column buttons
  and tiles, a bottom bar with More) in the TV's look; the screen filters are gone and Video is MiSTer's
  own video settings, tried safely (below).
- **Trying video settings safely** (phosphord; the menu, the Windows app and the Android app get
  their screens for it next): `vset` changes MiSTer's own video settings (resolution, vsync, scaling,
  HDMI, analog output...) in the ini in use, or switches profile (MiSTer.ini and the MiSTer_*.ini
  profiles, such as the SuperStation's RGHV, RGsB, SVID and YPbP). The menu restarts with the
  change, and unless Keep comes within 35 seconds the old file or profile goes back by itself, even
  with no picture or no app connected. If the power goes before Keep, it goes back at the next start.
  The old file is kept in phosphor/backups/ini first, like every ini save.

## 0.27.24

### Fixes
- **MiSTer's login prompt flashed in the top left corner between screens** (a game starting, MiSTer's
  menu, a script, coming back): Phosphor wipes that text console whenever it takes or hands over the
  screen, so a switch shows black.

## 0.27.23

### New
- **Controller test** (Settings > Controllers, Y on a controller): the controller live, in the
  layout chosen for it. Every button lights as it's pressed, the sticks and triggers show how far
  they go, and "MiSTer gets" shows what MiSTer's menu and the cores receive after the mapping.
  Select + Start closes it.

## 0.27.22

### Fixes
- **After an update, MiSTer's menu came up instead of the restart**: the update restarts
  phosphord, and Phosphor's screen then asked for the restart on its old connection, which ended the
  screen. It reconnects now (and restarts the SuperStation itself if phosphord doesn't answer).
- **Mapping a controller from Phosphor skipped its sticks** (Settings › Controllers, or from the app):
  as a stick is pushed, MiSTer makes a D-pad press from it halfway (dpad_threshold), before the stick
  is far enough to be recorded, and Phosphor took any press on a stick step as "no stick here". Only
  a real button skips a stick step now, so the sticks are saved as in MiSTer's own mapping.
- **Update Phosphor showed only the first 8 packages by file name**, so a newer one could be
  missing: the newest 8 are shown, newest first.

## 0.27.21

### Fixes
- **A controller mapping with no sticks in it** (the 8BitDo Ultimate 2's, for one) gets its sticks
  when it's loaded: from MiSTer's controller database, or else the controller's own stick axes. The
  sticks work in Phosphor's menu and in games without mapping again.

## 0.27.20

### New
- **Updating screen**: Phosphor's screen stays on through an update or roll back and shows each
  step (checking the package, keeping a copy of the old version, installing, the modules counted)
  with a progress bar, then restarts the SuperStation. MiSTer's menu no longer shows up in the
  middle. If an update stops, the screen says why and goes back to Phosphor. (The update to 0.27.20
  itself still looks the old way: the version doing it doesn't know this screen yet.)

### Fixes
- **Controller sticks weren't saved when mapping some controllers** (an 8BitDo Ultimate 2, for
  one): the stick's own button, which some pads send as the stick moves, counted as "no stick here"
  and skipped the stick steps, so the sticks did nothing in Phosphor and in games. Map the
  controller again after this update.

## 0.27.19

### Fixes
- **Pressing Update Phosphor could start the same update several times at once**: the TV gave up
  waiting for the updater's answer after a few seconds and said "The updater didn't start" though
  it had, so pressing again started another one; together they broke each other's backup and left
  MiSTer's menu on the screen instead of restarting. Now only one update runs at a time (another
  press says "An update is already running"), the answer comes back at once, and the TV waits for
  it.

## 0.27.18

### Fixes
- **The TV froze for a few seconds now and then while the Windows app was open**: the app checks
  the SS1 once a minute, and the drive list (Storage) took about 7 seconds to work out, while
  phosphord answers one thing at a time. The drive list, the update packages and the cloud saves
  status, the backups list and the restore points are worked out in the background now and
  kept, so every answer is instant (the menu cursor no longer stops while one is worked out).
- **Box art showed "Not run yet" after every restart**, even when lookups had run: the last run's
  result is kept now, with when it ran ("Done (last run Oct 23, 23:40)").

## 0.27.17

### Fixes
- **An update (or going back to a version) ends with a full restart of the SuperStation**, through
  the same safe stop as Restart in the power window: MiSTer's Main, the controllers and every
  module start fresh with the new version. Before, only Phosphor's screen restarted, and a
  controller could stop answering until the next restart.

## 0.27.16

### New
- **Memory cards, save by save** (TV: Settings > Front ports & memory cards > Memory cards; app 0.2.15:
  Saves & memory cards): SS1 Tool's memory card manager. Both front ports side by side; read a card
  into the library (phosphor/memcards, the same layout as SS1 Tool's), copy, move or remove single
  saves between cards, put saves back onto a real card. Every change is a new version of the card
  and every write to a real card backs it up first. The app also adds .mcs saves and downloads .mcs
  and .mcd files.
- **Controller test** (TV: Settings > Front ports & memory cards > Controller test): the controller
  in either front port, live, drawn like SS1 Tool's (DualShock or digital).
- **Storage** (TV: Settings > Storage): every drive with its space and last check; quick check of
  everything, speed test, drive health, file system check, repair of NVMe / USB drives (asks twice),
  mounting a drive that dropped off, and backing up saves & settings now.
- **The app (0.2.15)** has the rest of the SS1's settings:
  - Devices: Bluetooth, front ports and USB, view only (pairing stays on the SS1).
  - Network: connections, addresses, WiFi network and signal, view only.
  - Video: each system's screen filters, scanlines, shadow mask, gamma and presets, with undo.
  - Remote control: Phosphor's screen on the TV from the PC (buttons or the keyboard), and starting
    a game. WiFi, Bluetooth, remote access and file sharing settings can't be opened from it.
  - Advanced: MiSTer.ini (every save keeps the old file in phosphor/backups/ini, and any kept copy
    can be put back) and Scripts (run on the TV, in MiSTer's terminal).
  - SD card flash & health: the official SuperStation SD card image onto a card in the PC's reader
    (written and read back like SS1 Tool does), and a read test of a card. Windows asks for
    permission only when the card is written or tested.

### Fixes
- **The SS1's dock no longer leaves a phantom drive** (the JMicron bridge's empty second disk) once
  the NVMe drive is up; the rule lives in phosphord, so a MiSTer Linux update doesn't undo it.
- **The app's "SS1 isn't answering"** only shows when the SS1 really doesn't answer.

## 0.27.15

### Fixes
- **Arcade games without a MAME set name** (Dragon's Lair and the other laserdisc games, Cliff
  Hanger, Computer Space and other TTL machines) were never looked up: they're looked up by their
  .mra name now, and the ones in an `_alternatives` folder get linked to their main game like the rest.

## 0.27.14

### New
- **Circuit board background** (Settings > Background texture, on the TV and in the app 0.2.13):
  random traces winding across the whole screen like a maze, 45-degree bends, vias at the ends,
  nothing crossing or looping back. One picture for the whole screen rather than a repeating tile,
  drawn once at the TV's resolution in the accent colour. Also "Circuit board, bold".

## 0.27.13

### Fixes
- **Arcade games "not found" that are obviously known:**
  - A clone set (another region or revision: Raiden (Japan), Pengo (set 3), Cameltry (US)) has no
    flyer of its own on ArcadeDB: its parent's flyer is used, and its details fill in the gaps.
  - A game in MiSTer's `_Arcade/_alternatives/_<Game>/` folder that nobody knows (HBMame hacks,
    MiSTer-only sets like R-Type II (inv)) is linked to <Game> automatically, so it shows that game's
    box art and details (unlink it on the game's page if that's wrong).
  - An arcade set that isn't a MAME set is looked up by its .mra name on LaunchBox, TheGamesDB and
    IGDB (never a hack's).
  - On the PC, LaunchBox's arcade flyers are used when there's no box (its database is read again
    once for that).
  - Arcade games not found before are looked up again on the next run.

## 0.27.12

### Fixes
- **ScreenScraper works with your own screenscraper.fr login alone**; a developer login is only
  needed if you'd rather use one (on the SS1 and in the app).
- **Phosphor for Windows 0.2.11: starting a new version while an older one is still running** (in the
  tray) only showed the old one, so new features seemed missing. The old one is closed now and the
  new one takes over.

## 0.27.11

### New
- **Box art looked up on the PC** (Phosphor for Windows 0.2.9, Settings > Box art > Look up on this
  PC). The PC fetches the game lists, the LaunchBox database and the pictures, matches each game with
  the SS1's own rules and sources (in your order, with your logins), makes the box art and sends it
  finished to the SS1; several games at once. Much faster than on the SS1. Fields a game already has
  are never replaced; Disc 2 and later still borrow Disc 1's art. The SS1's own lookup waits while
  the PC works, and the TV's box art screen says who's doing it. "Look up on the SS1" stays for when
  there's no PC.

## 0.27.10

### New
- **SSH in Settings > Remote access:** Off, Password or Keys only.
  - **SSH password:** MiSTer's own (1), or one you set with the on-screen keyboard (X puts MiSTer's
    back). Only its hash is kept.
  - **SSH keys:** the keys that can log in as root; X removes one. The Phosphor app adds this PC's keys
    (Settings > Connection > Add this PC's SSH key), and each only gets in after a yes on the TV.
  - Keys only can't be chosen without a key, and the last key can't be removed while it's on, so
    nobody gets locked out. Another device can't change any of this.
  - MiSTer's own system files aren't edited (they're read-only, and a Linux update replaces them);
    uninstalling Phosphor gives back MiSTer's own SSH.
- **Keep a copy:** remote access (paired devices, SSH mode, password, keys) copied to the NVMe or a
  USB drive after each change. A freshly flashed SD card gets it back on first boot, with a notice.
  Backup snapshots now include it too (so a NAS gets a copy when snapshots go there).
- phosphor-link 0.4.1 logs in with the Phosphor app's key, so it still works with Keys only.

## 0.27.9

### Fixes
- **MiSTer's logo showed over Phosphor's wallpaper** in MiSTer's menu. Phosphor's Main leaves it out
  (MiSTer.ini isn't touched; after uninstalling, stock Main shows it again as set there).
- **The Linux login prompt flashed between transitions** (starting a game, going to MiSTer's menu).
  The console only goes back to text mode for a script now, the one case that needs it.

### Changed
- **Settings wraps around:** Up on the first item goes to the last, Down on the last to the first.

## 0.27.8

### Fixes
- **MiSTer's menu showed no wallpaper.** MiSTer's background setting starts at "none"; when
  Phosphor hands the screen to MiSTer's menu and there's a menu.png or menu.jpg, it's switched to
  the wallpaper (F1 in MiSTer's menu still changes it, and the choice is kept).
- **Last update's log was cut off.** The whole log shows now, starting at the end; Up/Down
  scroll, L/R (or Left/Right) page, and the line count shows where you are.

### Changed
- **Launch Phosphor sits under Reboot** on MiSTer's System page, where the cursor starts.

### New
- **Cancel the mapping wizard:** Select + Start together (or Esc on a keyboard) stops it and saves
  nothing; the mapping that was there stays.

## 0.27.7

### Fixes
- **A script from MiSTer's menu (Update All, say) took minutes to start, or never did.** Phosphor's
  screen kept the Linux console in graphics mode, and Linux then refused the switch to MiSTer's script
  terminal while Main waited for it. Phosphor hands the console back whenever MiSTer's menu, a game or
  a script has the screen, and Phosphor's Main puts it in text mode itself before switching.
- **Phosphor's loading screen showed behind MiSTer's own menu.** MiSTer's own background is back
  there; the loading screen only shows while Phosphor has the screen (power-on, between cores).
- Update Phosphor: the date of a version to roll back to, and the app gets its answer before
  Phosphor restarts.

### New
- **Back to Phosphor from MiSTer's menu:** Launch Phosphor is the first item on MiSTer's System page
  (where the cursor starts), and holding the menu button for 2 seconds anywhere in MiSTer's menu
  does the same.
- **Phosphor's wallpaper behind MiSTer's menu** (MiSTer's menu.png), only when there's no menu.png or
  menu.jpg of your own. Uninstalling takes it away.

## 0.27.6

### New
- **Controllers** (Settings, and Settings > SuperStation > Controllers in the app): your USB and
  Bluetooth controllers with SS1 Tool's button layouts, picked automatically or by hand (kept per
  controller), lit up live. **Map buttons** opens a locked window where MiSTer's own mapping, run by
  Phosphor's Main, records the buttons: the same mapping file as Define joystick buttons, for MiSTer's
  menu, Phosphor and every core. Reset to automatic, backups and restore; every change backed up first.
- **Update Phosphor** (Settings, and Settings > Power & updates in the app): installs a release
  package from phosphor/updates, the SD card, a USB drive, or sent from the app; the installed version
  is backed up first and one press rolls back. Your library, box art and settings stay.
- **MiSTer menu (OSD)** moved up under Controllers. The front-port screen is now "Front ports &
  memory cards".

### Windows app 0.2.6, 0.2.7
- Controllers, and Update Phosphor (send a package from the PC, install, roll back).

## 0.27.5

### Fixes
- **No NVMe temperature, and the drive showed as "USB".** The installer left out `phos-nvme`, so the
  Dashboard had no temperature and the storage checks fell back to SS1 Tool's helper. It's installed
  now, and a drive behind the dock's bridge shows as NVMe.
- **Opening the storage page could reset the dock.** Listing the drives asked the NVMe drive for its
  model (Identify), which the dock's bridge times out on before USB resets it. It's never asked now.
- **X and Y did the wrong thing** (Save a report in Diagnostics, among others). MiSTer's Main sends
  the 8BitDo pad's X and Y as "select" and "start"; Phosphor now reads Select, Start, X and Y from
  the controller itself.
- **A drive left unmounted vanished from the app** ("No NVMe or USB drive"). Unmounted drives are now
  listed, with **Mount it again**.

### Windows app 0.2.5
- Reconnects by itself when the SS1 restarts or gets a new address (Settings > Connection).

## 0.27.4

### Fixes
- **The power window didn't open.** MiSTer's Main doesn't pass the controller's Select (and not
  always Start) on to Phosphor; both are now read from the controller as well.
- **NVMe drive health didn't work in Phosphor** (it needed SS1 Tool's helper). Phosphor now reads the
  drive behind the dock's JMicron bridge itself (`phos-nvme`): temperature, wear, spare, hours,
  unsafe shutdowns, errors. It never sends the drive an Identify request: the bridge times out on it
  and USB resets the dock.

### Changes
- **Power window: Select + Start together** (either order), instead of holding Select. On their own
  they're still Select and Menu (they act when let go).
- **Home shows five whole cards;** moving past the fifth, the first goes and the next comes in.
- **The NVMe drive's temperature** is on Phosphor for Windows' Dashboard.

## 0.27.3

### Fixes
- **Windows said the card was dirty after every power-off.** exFAT marks a drive "dirty" while it's
  in use and only clears it when the drive is made read-only or unmounted; switching off at the power
  button never did that. Shut down and Restart now make every drive read-only first (modules stopped,
  everything written; the kernel's emergency remount when something still has a file open), read the
  flag back, and say how each drive came out.

### Changes
- **Power window** (opened with Select + Start from 0.27.4).
  Shut down safely, Restart, Reload the screen; d-pad or stick to choose, A to do it, B to cancel.
  Shut down ends on a "Safe to switch off" screen with each drive's state; Settings > Restart and
  Phosphor for Windows' Shut down and Restart go the same way (the TV shows it).

## 0.27.2

### Fixes
- **MiSTer's static after Update All.** The downloader puts MiSTer's own menu core back as
  `menu.rbf` and then restarts the SS1, and a restart loads `menu.rbf` straight from the card, so
  the static showed until Phosphor was up. Phosphor's menu core now goes back in at every shutdown
  (the downloader's restart included) and right after Update / Update All, so the restart shows
  Phosphor's loading screen. MiSTer's copy is kept in `phosphor/backup/menu.rbf.mister`.

### Changes
- **The loading screen:** between cores, video modes and scripts, and while the SS1 starts, the
  menu core shows the PHOSPHOR wordmark with a scanner bar and NOW LOADING, instead of static.
- **Home looks like Phosphor for Windows:** the categories in a row from the left, the highlight
  moving along it and the highlighted card a little bigger; the row slides only when the highlight
  reaches the right edge (no more centred carousel).
- **Dashboard for Phosphor for Windows:** phosphord answers a read-only `dashboard` request
  (temperatures, CPU per core, memory, uptime, the running core, drives and their read / write
  traffic, network addresses and traffic, the busiest processes, and the kernel log's last lines).

## 0.27.0

### Fixes
- **Box art: Dragon Warrior VII and others weren't found.** Some PlayStation discs boot a file with
  an extra letter (`SLUSP012.06`), so the product code came out wrong and the lookup gave up. The code
  is read right now, and codes already read are fixed.
- **Box art: a file named like the official list is found by its name** when its content doesn't
  match (another dump, a revision, a disc whose code can't be read): an exact list name, or exactly the
  same title words with the same disc number, the file's region first. Nothing is guessed.
- **Box art: a missing picture falls through.** When libretro has no picture under a name, the name
  with one region and without the disc number is tried, then the next source. Disc 2 and later use
  Disc 1's picture. A game found without any picture now goes to Match to base games instead of
  looking done. Everything not found before is looked up again.
- **Base game suggestions** no longer pair games that only share a number ("Dragon Warrior VII" and
  "Final Fantasy VII").
- **Remote access refused PCs on home networks outside the usual ranges** (for example
  200.200.200.x): devices on the SS1's own network are accepted now.

### Changes
- **Storage checks on the SS1:** the SD card, NVMe and USB drives (quick check, will Windows
  complain, speed, drive health, file system check and repair), the PCIe generation of NVMe drives
  (Gen 4 drives cause trouble in the dock), and removal (with undo) of the faulty overlap fix tool.
  New `storage` module, always installed.
- **For Phosphor for Windows:** restart, shut down and reload the TV screen; file sharing on and off;
  storage checks; box art sources and logins; updates and restore points (network settings are put
  back only on the SS1 itself, and restore points are deleted only there).
- `phosphor.sh reload` restarts the TV screen without restarting the SS1.

## 0.26.6

### Changes
- **Getting box art and details has its own screen**, like Looking for games: the source it's asking,
  the system and game it's on, a bar with time left, found / not found / no connection, and the
  systems with how many have box art. Everything else waits until it's done; B twice stops it, and
  what's found is kept. Settings > Get box art and details opens it.

## 0.26.5

### Fixes
- **Cloud saves: the rclone download always failed.** MiSTer's Linux has a certificate bundle, but
  curl can't find it unless it's named, so the download stopped at "unable to get local issuer
  certificate". The screen showed "Downloading rclone…" as if it had worked, so the storage place
  options never appeared. cloud-sync now names the bundle (the downloader's current copy, else
  `/etc/ssl/certs/cacert.pem`), for the download and for rclone's own HTTPS.
- **Cloud saves: a failed step now says why** (for example a certificate problem or no network)
  instead of showing its first line.
- **Cloud saves: setup leads on.** After rclone is ready the screen goes straight to adding a storage
  place; with none chosen, the cursor starts on Storage place.
- **Look for games could sit on "waiting" forever** when a system had files that can't be read (seen
  as "PlayStation waiting 0-7"). One pass is the job now; unreadable files are tried on the next scan.
- **Update guard copied the 375 MB `linux.img` two minutes after every boot and left Linux's file
  system writable.** Boot checks are now cheap (no copies, no writes), and copying an image puts the
  file system back the way it was.
- **The Update and Update All terminal was small and bunched at the left** at 1080p (240 columns for
  80-column output). Scripts now run with a 16x32 font: 120 columns, twice the size, still sharp.
  MiSTer's own Scripts menu keeps its usual font.
- **Cores didn't install to the NVMe.** The image now carries the SS1 documentation's recommended
  `downloader.ini` (`storage_priority = prefer_external`), with `update_linux = true`.

### Changes
- Updates & restore points: an alert in the status bar when an update reset settings, and a Settings
  screen to restore them, keep the new ones, roll back, or delete restore points.
- **Power check.** A brownout leaves no trace: no crash, no error, no log. Phosphor marks heavy work
  (Update, Update All, scripts, Look for games, cloud saves, backups) while it runs; a mark
  still there at the next start-up, with no normal shutdown in between, means the power went. After
  3, the status bar says "Power supply problem?" and Settings > Diagnostics lists them with what helps
  (a single-port USB-C PD adapter, 9 V / 3 A or more). Dismiss starts the count again. Idle resets
  aren't counted: they look the same as switching off. Nor is box art, which can run for hours
  unattended; one power cut counts once, however many jobs it interrupted. `phosphor/logs/power.log` goes in the support
  report.
- Settings: an info panel beside the list shows what the highlighted setting is set to and what it
  does. "Get or update cores (MiSTer's downloader)" is now just "Update".
- Background textures: 8-bit tiles and circles removed; packed, bold, filled and inverted versions
  added, plus diamonds and square grids (21 in all).
- Settings > Screen filters removed (each core's OSD has them).
