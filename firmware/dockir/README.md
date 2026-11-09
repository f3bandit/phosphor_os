# Dock IR remote firmware

The SuperDock's IR receiver is an RP2040 running Retro-Remake's [DockIR](https://github.com/Retro-Remake/DockIR)
(based on pico-ir-keyboard; NEC decoding by mjcross, BSD-3-Clause; TinyUSB by Ha Thach, MIT). It decodes the
remote's NEC infrared and sends USB keyboard keys; it shows up as `1c4f:0002` "TinyUSB pico_ir_keyboard" on the
dock's USB hub. Its key for each button is the table in DockIR's README (PR #4 by davewongillies, Oct 2026).

`ss1update.patch` (f3bandit) makes it updatable from the SuperStation, with no PC and no buttons:

- A HID **feature report** `DIRBOOT!` sent to its keyboard interface (Linux: `HIDIOCSFEATURE` on its
  `/dev/hidraw`, with report number 0 first) restarts it into the RP2040's ROM bootloader: the `RPI-RP2` drive
  appears on the SS1 (MiSTer's usbmount mounts it as a `/media/usbN`), the next `.uf2` is copied onto it, and it
  restarts with that firmware. Nothing else sends that report; keyboard LED changes are output reports, not
  feature reports.
- A feature report **read** (`HIDIOCGFEATURE`) answers its id, `DIRUP001`, so an app can tell it can be
  updated this way; its USB device version (bcdDevice) is 1.01 (DockIR's own is 1.00).
- Both are 8 bytes: TinyUSB's HID control buffer is 8 (`CFG_TUD_HID_EP_BUFSIZE`), and a longer report is
  refused.

The bootloader is in the RP2040's ROM, so it can't be lost: holding **BOOTSEL** and tapping **MCU RST** (two
small buttons inside the dock's NVMe slot) always brings up `RPI-RP2` on a PC connected to the dock's USB port,
for the first flash or to go back.

Tested Nov 4 2026 on an SS1 with a SuperDock: flashed by hand, then the update command and a reflash from the SS1.

    ./build.sh      # fetches DockIR at the pinned commit and pico-sdk 2.3.0, patches, builds dockir-DIRUP001.uf2

Retro-Remake's source isn't copied here: the DockIR repository has no licence file, so only this patch and the
build script are. The built `.uf2` contains their code too: it isn't shipped with Phosphor until Retro Remake
says it can be.

## For Retro Remake

Retro Remake and Taki Udon may use all of this (the patch, the build script, the updater in
`system/phosphord/dockir.c` and the button mapping), in Console Mode and in DockIR itself, commercial use included,
with credit to f3bandit: see [LICENSE-RETRO-REMAKE.md](../../LICENSE-RETRO-REMAKE.md).

To add it to DockIR and Console Mode:

1. **DockIR:** apply `ss1update.patch` to DockIR at `9403adc` (or merge it into your repo) and build as usual (pico-sdk
   2.3.0). It adds the `DIRBOOT!` feature report (restart into the ROM bootloader) and the `DIRUP001` id, and sets
   bcdDevice to 1.01. Nothing else changes: the keymap is yours.
2. **First flash, once per dock:** from a PC on the dock's USB port, hold BOOTSEL, tap MCU RST (inside the NVMe
   slot), copy the `.uf2` onto `RPI-RP2`. Docks that ship with the patched firmware skip this.
3. **Updates from the SuperStation (Console Mode):** what `system/phosphord/dockir.c` does, in order:
   - find the receiver: USB `1c4f:0002`, its keyboard interface's `/dev/hidrawN` (`/sys/class/hidraw/*/device`);
   - `HIDIOCGFEATURE` (report 0, 8 bytes) must answer `DIRUP...`: only then can it be updated this way;
   - `HIDIOCSFEATURE` with `00` + `DIRBOOT!`: it restarts as `RPI-RP2` (MiSTer's usbmount mounts it as a
     `/media/usbN` with `INFO_UF2.TXT`; wait up to ~10 s);
   - copy the `.uf2` onto that drive and `sync`: it restarts with the new firmware by itself;
   - check it's back as `1c4f:0002` and answers the new id.
4. **Remote buttons in Console Mode (issue #95, no Select):** map the receiver's keys to Console Mode's buttons the way
   Phosphor's `map_ir` does (`frontend/src/input.c`): OK/Play = A, Exit/Cancel/Stop = B, the page keys = L/R, Info
   (`KEY_S`) = X, L/R (`KEY_TAB`) = Y, Menu (`KEY_F12`) = menu. Console Mode needs one of the free remote keys on
   Select so Favorites works from the remote.
