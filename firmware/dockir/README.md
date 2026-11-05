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
