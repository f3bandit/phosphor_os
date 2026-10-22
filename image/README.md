# SD card image

A ready-to-flash image: stock MiSTer (the MiSTer-devel release) with Phosphor already installed.

## Using it

1. Flash `phosphor-<version>-sdcard-<mister date>.img.zip` with balenaEtcher, Raspberry Pi Imager,
   Win32 Disk Imager or `dd` (any size card from 2 GB; everything on the card is erased).
2. Optional, before putting it in the MiSTer: the card shows a `MRFUSION` drive. Put a
   `wpa_supplicant.conf` in it for WiFi, a `samba.sh` to turn on file sharing, or scripts in `Scripts/`.
3. Put the card in and power on. The install screen shows while the card is expanded to its full size
   and MiSTer and Phosphor are copied onto it (a few minutes); then it restarts into MiSTer.

Afterwards it's a normal MiSTer: run `update_all` or the downloader to get cores; Phosphor's library
service starts at boot (block "phosphor" in `linux/user-startup.sh`). `/media/fat/phosphor/uninstall.sh`
removes Phosphor like on any other install. The Phosphor release is also kept in `/media/fat/phosphor/`
for installing modules later.

## How it's made

`image/build-image.sh [phosphor-<v>-armhf.tar.gz] [--mister release_YYYYMMDD]` (Linux or WSL, no root;
needs curl, git, 7z, sfdisk, mkfs.vfat, mtools, zip). Also the "SD card image" workflow in GitHub Actions
(run by hand, from a Phosphor release tag).

It starts from [Mr. Fusion](https://github.com/MiSTer-devel/mr-fusion), MiSTer-devel's universal
installer image, and changes only what Mr. Fusion's installer copies onto the card:

- `release.7z`: the MiSTer release from
  [SD-Installer-Win64_MiSTer](https://github.com/MiSTer-devel/SD-Installer-Win64_MiSTer), unpacked, with
  Phosphor installed into it by Phosphor's own `install.sh --boot`, packed again (LZMA2, no filters, so
  Mr. Fusion's 7-Zip 16.02 unpacks it);
- `splash.png`: Phosphor's install screen (`image/make-splash.py`);
- the first partition is made bigger to fit.

Mr. Fusion's kernel, installer and bootloader are used as they are: no Linux, kernel or Main of our own.
The build checks Mr. Fusion's layout and that the boot block points at `/media/fat/phosphor`.

## Licences

Mr. Fusion is GPL-3.0 (sources, and the licences of the Linux kernel and buildroot tools inside it, in
[its repository](https://github.com/MiSTer-devel/mr-fusion), including `legal-info/`). The MiSTer release
is MiSTer-devel's (GPL; sources in the MiSTer-devel repositories). Each build writes a `.txt` next to the
image naming the exact versions. Phosphor itself is PolyForm Noncommercial 1.0.0.
