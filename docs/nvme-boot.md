# Running from the NVMe (or a USB drive)

Status: built, not switched on anywhere yet. `install/nvmeboot/` is installed with Phosphor but does nothing
until `nvmeboot.sh enable`. The TV screens are mockups only (the "Phosphor: Run from NVMe" design).

## What stays on the SD card

The SuperStation can only start from the SD card, so it keeps:
- MiSTer's u-boot (its own partition)
- `linux/zImage_dtb` (the kernel) and `linux/u-boot.txt`
- `menu.rbf` (u-boot loads the menu core into the FPGA before Linux starts)

Everything else runs from the drive: `linux/linux.img`, MiSTer's Main, the cores, games, saves, settings
and Phosphor. The SD card isn't even mounted while running from the drive, except for a moment when
`nvmeboot.sh sync` copies a new kernel or menu core to it, so it stays clean.

## No custom u-boot or kernel

Both of MiSTer's already do what's needed (checked in their source, u-boot_MiSTer 8dcc348,
Linux-Kernel_MiSTer init/do_mounts.c):
- **u-boot** imports `/linux/u-boot.txt` from the SD card before it starts Linux (`scrtest` in
  `include/configs/socfpga_de10_nano.h`). Phosphor's lines there (`u-boot.phosphor.txt`) replace its
  `mmcboot` with one that boots with `root=PARTUUID=<the drive's partition>`.
- **The kernel** (6.18) mounts that partition as exFAT and `linux/linux.img` on it as `/` (MiSTer's `loop=`
  patch), with the partition at `/media/fat`, exactly as it does with the SD card. `rootwait=20` gives up
  after 20 s (rootwait with a time limit is Linux 6.5+), `panic=3` restarts. USB storage, UAS and dwc2 are
  built in (`CONFIG_USB_STORAGE=y` and so on), so no initramfs is needed (and the kernel has none:
  `CONFIG_BLK_DEV_INITRD` is off).

## Falling back to the SD card

A word at `0x1FFFFF10`, in the page of memory MiSTer's Main and u-boot already share across a restart
(Main uses 0x000-0xEFF for the next core's settings and 0xF00-0xF0B for SDRAM, the video profile and the
warm-start flag), records the attempt:

| At start-up, u-boot finds | It starts from | And leaves |
|---|---|---|
| `/phosphor_boot_sd` on the SD card | the SD card | "forced" |
| "trying" (the last try didn't come up) | the SD card | "fell back" |
| anything else (a normal start, a cold start) | the drive | "trying" |

Phosphor (`nvmeboot.sh boot`, first thing in `phosphor.sh boot`) clears the word once it's up. If the drive
isn't found, its `linux.img` is missing or broken, or nothing of Phosphor's starts, the kernel or the user
restarts and the next start-up is from the SD card, with `phosphor.boot=fallback` on the kernel's command
line for Phosphor to tell the user. The restart after that tries the drive again.

## A drive that wasn't shut down safely

The drive can't be checked while the system runs from it (`/` and `/media/fat` are on it), and the kernel
has no initramfs to check it before mounting. So when Linux logs "Volume was not properly unmounted" for it
at start-up, `nvmeboot.sh boot` sets the word to "check" and restarts (a safe stop first). u-boot then
starts once from the SD card (`phosphor.boot=check`), where the drive is just storage: Phosphor's start-up
dirty-drive check repairs it (`fsck.exfat -p`), and `nvmeboot.sh after-check` restarts from the drive. If the
check couldn't clean it, it stays on the SD card. At most one such round in 10 minutes, so a drive that
can't be cleaned never loops.

| Word at start-up | Starts from | Leaves |
|---|---|---|
| "check" | the SD card (`phosphor.boot=check`) | "checked" (the next start-up tries the drive) |

## Finding the right drive

By the partition's PARTUUID (from its GPT), never by its Linux name: with another drive plugged in the NVMe
can be `sdb`. `check` refuses a drive whose PARTUUID another partition also has (a cloned drive), and
`status` reports it.

While running from the drive, `usbmount` finds it again and mounts it a second time at `/media/usbN`;
`nvmeboot.sh boot` unmounts that (the same file system twice: every game listed twice), and the dirty-drive
check at start-up never touches the system's own drive.

## Update All and updates

- `downloader.ini` on the drive gets `storage_priority = 'prefer_sd'` (the drive is `/media/fat`: nothing
  external to prefer; otherwise a USB stick plugged in later would get the downloads). The old value is kept
  in `phosphor/state/nvmeboot.state` and put back by `disable`.
- A Linux update lands on the drive's `/media/fat/linux/`; `sync` copies the kernel, `menu.rbf` and
  `u-boot.txt`'s own lines (USB polling, `ethaddr`) to the SD card, at every start-up and after
  `phosphor.sh menu-core` (shutdown and after updates).

## Undo

- `copyback`: running from the drive, newer files go back to the SD card (saves, settings, updated cores
  and Linux), after checking the SD card has room. The drive's games stay on it.
- `disable`: the start-up lines off the SD card, `downloader.ini` back as it was.
- From a PC: delete the `phos_*` lines and the `mmcboot=if run phos_force…` line from `linux/u-boot.txt`,
  or create an empty file `phosphor_boot_sd` on the SD card.

## Tests

- The u-boot lines, run in U-Boot's sandbox (v2024.01 built here, with `bootz` replaced by an echo): a cold
  start boots the drive and leaves "trying"; "trying" at start-up boots the SD card and leaves "fell back";
  `phosphor_boot_sd` boots the SD card ("forced"); `$v` (USB polling) and `memmap=513M$511M` come through
  as in MiSTer's own `mmcboot`. MiSTer's 2017 u-boot has the same commands (`itest.l *addr`, `test -e`,
  `mw.l`, `if … elif`; `CONFIG_HUSH_PARSER`, `CONFIG_CMD_ITEST`).
- `nvmeboot.sh` under BusyBox with a fake SD card and drive: check, copy (games merged, nothing on the drive
  replaced, the copy checked), enable twice (one set of lines), status, boot from the drive and after a
  fall-back, sync, copyback (including an SD card too small), disable; a dirty drive at start-up ("check"
  set, a second time within 10 minutes left alone), after-check (cleaned: back to the drive; still dirty:
  stays; an older boot's log line ignored). The "check" word in the sandbox too.
- On the SS1, read only: `check /media/usb0` passes (exFAT, PARTUUID 7a06eb63-…, room, kernel 6.18.38),
  `status`, `boot` with nothing set up.
- **Not tested yet: an actual start-up from the drive** (needs the SD card's `u-boot.txt` changed and a
  restart).
