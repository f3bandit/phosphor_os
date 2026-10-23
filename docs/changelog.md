# Changelog

Changes between test images. The image build copies the newest section into the notes file it
writes next to each image (`dist/phosphor-<version>-sdcard-*.txt`).

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
