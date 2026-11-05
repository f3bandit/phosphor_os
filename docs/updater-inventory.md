# Phosphor's own updater: inventory of Update All 2.11

What Update All 2.11 (theypsilon, GPL-3, 2026-09-18) installs and does, and what Phosphor's own updater does with
each: a GitHub Action checks every database daily and publishes Phosphor's checked database; the SS1 runs MiSTer's
own Downloader (2.4) against it, with the Linux-update style warnings. Update All itself isn't run on the SS1.

Verdicts:
- **include**: in Phosphor's database as it is (pinned to the hashes CI checked).
- **in Phosphor**: already built in; the database's own files would replace Phosphor's, so it's left out.
- **patch**: rebuilt by CI with Phosphor's hooks, published in place of the original (GPL/MIT only).
- **launcher**: kept as it is, started from Phosphor, which hands the screen over and takes it back.
- **warn**: installed only after a warning (and a restore point), like the Linux update.
- **opt-in**: not in Phosphor's database; the user turns it on and it runs from its own database.
- **out**: not offered.

Checked 2026-11-05 against Update All 10f9b9a (2026-10-09).

## Databases (236 entries, 117 artwork and 127 manuals)

| Database | Verdict | Notes |
|---|---|---|
| Main Distribution: MiSTer-devel (pinned Linux) | include | Cores, menu and Main; Phosphor's Main and menu core swapped in for `MiSTer` and `menu.rbf` |
| Main Distribution: MiSTer-devel (Edge Linux) | warn | Untested Linux on the SS1: test, then warn as for the Linux update |
| Main Distribution: DB9 / SNAC8, AitorGomez fork | in Phosphor / out | DB9 / SNAC8 is built into Phosphor's Main; the AitorGomez Main would replace it |
| JTCORES (public) | include | |
| JTCORES private releases | include, with the user's RetroAccount | See Logins |
| Coin-Op Collection (public) | include | |
| Coin-Op Collection beta / alpha | include, with the user's RetroAccount | See Logins |
| theypsilon Unofficial Distribution, LLAPI, Arcade Offset, Y/C Builds, Alt Cores, Dual RAM Console Cores, Paprium, MMS2 GB, MegaVGMDrive, 240p Test Suites, Uberyoji Boot ROMs, GBA Borders | include | Ordinary cores, MRAs and files |
| Names TXT (12), Arcade Names TXT (3) | include | One of each, by the user's choice |
| Wallpapers: Ranny Snice, Anime0t4ku, PCN Challenge | include | |
| Game artwork (chipster6502, 117) and manuals (ajgowans, 127) | include | By system, by the user's choice; the artwork also feeds Phosphor's Local files source |
| RetroAchievements Cores | in Phosphor | odelot's RA Main is in Phosphor's Main; phosphord fetches his cores |
| Physical CD Support | in Phosphor | Anime0t4ku's disc Main is in Phosphor's Main |
| Sonic Mania, 3S-ARM, Duke3D, Quake, Maldita Castilla | in Phosphor | Game ports with Phosphor's Main |
| NBlood | patch | Its own Main (source not published): launcher until the source is out |
| Solarus | patch, ready | Hybrid core: Phosphor's Main starts and stops its engine itself (tested on the SS1, 2026-11-05) |
| Diablo, DreamSTer | patch | Same as the game ports: their Mains rebuilt with Phosphor's hooks (licences to check per item) |
| MiSTer Hi-Fi, MiSTerFin | launcher | Started from Phosphor; MiSTerFin's boot hook patched out |
| mrext (MiSTer Extensions), Shmup Deck | patch | Launch games through phosphord's API |
| CIFS Scripts | patch | Mount points kept clear of Phosphor's library and backup folders |
| Anime0t4ku MiSTer Scripts | patch | FTP save sync, Syncthing, DAV and clock made to take turns with Phosphor's |
| MiSTer DVD, Disc Tools, MiSTer Monitor, tty2oled, i2c2oled, MiSTer SAM, RetroSpy, Zaparoo Core | include | Phosphor already works with each |
| BIOS Database, Arcade ROMs Database | opt-in | Copyrighted files: never in Phosphor's database |
| Zaparoo Frontend, Degauss, MiSTer Frontier | out | Other frontends (Phosphor has its own Zaparoo support) |
| Update All files | out | Update All itself isn't run |

## Update All features

| Feature | Verdict | Notes |
|---|---|---|
| Database list and settings, Database Manager (update only, uninstall) | port | Phosphor's updater screen, the TV and the Windows app |
| Arcade Organizer | port | Phosphor's library already skips its `_Organized` folders |
| MiSTer.ini edits, CRT video mode and screen position | in Phosphor | Settings › Video (left as Update All sets them today) |
| Mirrors (mysticalrealm.org, mister.cc.cd) | port | Database URLs rewritten, as Update All does |
| Linux Updates on/off | in Phosphor | The existing Linux update warning and restore point |
| Frontends menu | out | |
| RetroAchievements settings | in Phosphor | |
| Analogue Pocket firmware and backup | out | Not an SS1 feature (could come back in the Windows app) |
| Chip ID linker (`linker.rbf` reads the FPGA's chip ID) | port | Needed for RetroAccount; it crashed on a SuperStation before, so test it there |
| Self-update, migrations, log viewer, countdown, autoreboot | in Phosphor | Phosphor's own update, logs and safe stop |
| Update All supporter timeline (`timeline_plus.enc`) | port | Decrypted with the supporter's Patreon key (`encryption.py`) |

## Logins (all on the user's SS1 or in their Windows app, never in CI or in Phosphor's database)

- **RetroAccount** (theypsilon's account service): a device code login (`/api/auth/device/code`, then the token
  poll at `/api/auth/token`), a QR code to sign in on a phone, and a sync (`/api/mister/sync`) tied to the FPGA's
  chip ID (`/api/mister/device/hardware-id`). Files under `Scripts/.config/retroaccount/`.
- **Coin-Op Collection beta / alpha:** the tier follows the user's Coin-Op benefit in RetroAccount; the Coin-Op
  licence key bound to the linked FPGA ID is installed as `coinopkey.zip` and renewed at each sync.
- **JOTEGO private releases:** follow the user's JOTEGO benefit in RetroAccount (Auto), or Yes/No by hand.
- **Update All supporter:** the Patreon key file, checked by md5, decrypts the supporter timeline.
- The ported login code stays GPL-3, in its own module next to `ua_bridge.py`, not in Phosphor's PolyForm code.
- CI checks the supporter-only databases with f3bandit's own access (a GitHub secret) and publishes **only the
  verdicts** for them, never their file lists or URLs.

## To add: NES netplay

[vivitheblackmage/misternetplay](https://github.com/vivitheblackmage/misternetplay) (GPL-3, v0.1.1, last commit
2026-11-01): two MiSTers play NES games in lockstep over the internet or a LAN, with an optional lobby and relay
server. It's patches on MiSTer-devel's NES core (`49a0a66`, 3 patches) and on Main_MiSTer (`0827165`, 12 patches:
`support/netplay/`, plus `fpga_io.cpp`, `user_io.cpp`, `support.h` and the OSD option), and it ships its own Main,
`MiSTer_online`.

- **The core** (`NES_netplay.rbf`): include.
- **The Main patches:** patch: ported into phosphor-main, so netplay works without replacing Phosphor's Main (the
  same way the game ports went in), with Host / Join from Phosphor's menu and its lobby rooms in the TV menu.
- **The lobby server:** its own release; Phosphor only needs the address in `netplay.ini`.

## Upstream changes to merge (2026-11-05)

| Upstream | State | New |
|---|---|---|
| MiSTer-devel Main_MiSTer | merged up to `a1d1d9d` (2026-10-29) | 11 commits: **joymap v4 controller mapping (#1405)** and its fix (#1408), touching input and the menu where Phosphor's Main has its own changes; **MRA/MGL mount root path and submenus (#1406)**; 8 Minimig/Akiko CD32 drive commits |
| odelot Main_MiSTer (RetroAchievements) | merged at `0135156` (2026-10-12), his latest | Nothing new; rcheevos 12.5.0 in both. His cores come from his releases at run time |
| Anime0t4ku physical disc Main | | 3 commits (2026-10-31 to 11-01): **don't freeze the main loop while a slow drive seeks**, log slow reads and give the drive more time, don't prefetch the audio pregap at the end of a data track |
| MiSTer-devel Menu_MiSTer | merged (latest upstream `fa83dc1`, 2026-10-14) | Nothing new |
| MiSTer Linux | 260912 (pinned by the default distribution) | Nothing new |

## Built (2026-11-05)

- **Stage 1, the Database watch** (`.github/workflows/db-watch.yml`, `tools/updater/db_watch.py`,
  `tools/updater/rules.json`): daily, every database in Update All's list gets a category and a tag; files on
  Phosphor's paths, new databases and changes open one `db-watch` issue.
- **Stage 2, the catalog** (`tools/updater/build_catalog.py`): published on the `updater-catalog` branch after each
  watch: `catalog.json`, the patch databases (`dbs/`: the original with Phosphor's parts in place, same path with
  Phosphor's md5, size and URL, never left out) and Phosphor's parts (`parts/`, from its latest release).
- **Stage 3, the updater on the SS1** (phosphor-modules `updater/`, phosphord `{"cmd":"updater"}`): the user's
  categories and databases (their `downloader.ini` imported the first time, its options kept), warn/block/review
  only once OK'd, Linux updates off until OK'd, a restore point, then MiSTer's own Downloader.

## Next

1. The screens: Settings in Phosphor's menu and in Phosphor for Windows (mockups first), then a test on the SS1.
2. Merge the upstream Main changes (joymap v4 first: it's the one that touches Phosphor's input).
3. The NES netplay port into phosphor-main.
4. The patches and launchers, item by item, with each item's licence checked first.
5. Phosphor's updater on the SS1 (Downloader with Phosphor's database, the warnings, the ported logins), then
   retire `ua_bridge.py`.
