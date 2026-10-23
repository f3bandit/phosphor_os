# Changelog

Changes between test images. The image build copies the newest section into the notes file it
writes next to each image (`dist/phosphor-<version>-sdcard-*.txt`).

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
- Settings: an info panel beside the list shows what the highlighted setting is set to and what it
  does. "Get or update cores (MiSTer's downloader)" is now just "Update".
- Background textures: 8-bit tiles and circles removed; packed, bold, filled and inverted versions
  added, plus diamonds and square grids (21 in all).
- Settings > Screen filters removed (each core's OSD has them).
