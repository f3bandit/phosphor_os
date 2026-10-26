# Sources, licences and credits for Phosphor's releases

Phosphor's own work is under the PolyForm Noncommercial License 1.0.0 (LICENSE.md). Each release also
contains free software by other people. This page says where its source is and who it belongs to, for
every release, including the ones published before their packages carried this information
themselves (from 0.27.41 on, each package has SOURCE.md, THIRD_PARTY_NOTICES.md and the licence texts).

## GPL source of Phosphor's Main and menu core

`bin/MiSTer_phosphor` (Phosphor's Main) and `cores/menu_phosphor.rbf` (Phosphor's menu core) in each
release are builds of two forks of MiSTer's own programs, free software under the GNU General Public
License version 3. Their complete source, the exact version in each release:

| Release | Phosphor's Main (phosphor-main) | Phosphor's menu core (phosphor-menu) |
|---|---|---|
| 0.27.24-beta | [source/phosphor-0.27.24](https://github.com/f3bandit/phosphor-main/tree/source/phosphor-0.27.24) | [source/phosphor-0.27.24](https://github.com/f3bandit/phosphor-menu/tree/source/phosphor-0.27.24) |
| 0.27.36-beta | [source/phosphor-0.27.36](https://github.com/f3bandit/phosphor-main/tree/source/phosphor-0.27.36) | [source/phosphor-0.27.36](https://github.com/f3bandit/phosphor-menu/tree/source/phosphor-0.27.36) |
| 0.27.37-beta | [source/phosphor-0.27.37](https://github.com/f3bandit/phosphor-main/tree/source/phosphor-0.27.37) | [source/phosphor-0.27.37](https://github.com/f3bandit/phosphor-menu/tree/source/phosphor-0.27.37) |
| 0.27.38-beta | [source/phosphor-0.27.38](https://github.com/f3bandit/phosphor-main/tree/source/phosphor-0.27.38) | [source/phosphor-0.27.38](https://github.com/f3bandit/phosphor-menu/tree/source/phosphor-0.27.38) |
| 0.27.39-beta | [source/phosphor-0.27.39](https://github.com/f3bandit/phosphor-main/tree/source/phosphor-0.27.39) | [source/phosphor-0.27.39](https://github.com/f3bandit/phosphor-menu/tree/source/phosphor-0.27.39) |
| 0.27.40-beta | [source/phosphor-0.27.40](https://github.com/f3bandit/phosphor-main/tree/source/phosphor-0.27.40) | [source/phosphor-0.27.40](https://github.com/f3bandit/phosphor-menu/tree/source/phosphor-0.27.40) |

The licence text is in each repository (phosphor-main: `LICENSE`; phosphor-menu: `COPYING`). MiSTer's
Main is GPL-3.0; MiSTer's menu core is GPL-2.0-or-later, and Phosphor's version is GPL-3.0.

## Other free software in the releases

- **Phosphor's programs on the SS1** (`phosphor`, `phosphord`, `phos`, `phos-nvme`) are built with LVGL
  (with LodePNG), SQLite, cJSON, rcheevos, libchdr (with the LZMA SDK, miniz and Zstandard), stb and
  dr_libs. Up to 0.27.40 they were linked with the GNU C Library (glibc, LGPL-2.1): glibc 2.39 (Ubuntu's libc6-armhf-cross
  2.39-0ubuntu8cross1); its source is at https://sourceware.org/glibc/ and
  https://launchpad.net/ubuntu/+source/glibc, and Phosphor's complete source for each release, which they can be linked again from, is
  this repository at that release's tag. From 0.27.41 they are built with musl (MIT) instead.
- **Phosphor for Windows** is written in Go, with golang.org/x/sys and golang.org/x/image (BSD-3-Clause).
- **Phosphor for Android** uses AndroidX, Kotlin and kotlinx.coroutines (Apache-2.0).
- **Fonts:** Inter (SIL Open Font License 1.1) and DejaVu Sans Mono (Bitstream Vera licence).
- **The SD card image** passes on, unmodified, MiSTer's Linux, kernel, u-boot, Main and menu core from
  the MiSTer release it names (https://github.com/MiSTer-devel/SD-Installer-Win64_MiSTer; source:
  https://github.com/MiSTer-devel/Linux-Kernel_MiSTer, https://github.com/MiSTer-devel/u-boot_MiSTer,
  https://github.com/MiSTer-devel/Main_MiSTer, https://github.com/MiSTer-devel/Menu_MiSTer), Mr. Fusion
  (https://github.com/MiSTer-devel/mr-fusion, GPL-3.0; its de10_nano_hdmi_config.bin's background colour
  changed from blue to dark) and Update All (https://github.com/theypsilon/Update_All_MiSTer, GPL-3.0).

The full notices and licence texts of all of these: THIRD_PARTY_NOTICES.md (from 0.27.41 in this
repository and in every package).

## Credits and disclaimers

- **The RetroAchievements unlock sound** (`achievement.wav`) comes from odelot's RetroAchievements Main
  for MiSTer (https://github.com/odelot/Main_MiSTer), credited to odelot. It is not Phosphor's work and
  not under Phosphor's licence; its origin and licence are odelot's.
- **Fan-made box art** that Phosphor for Windows can fetch (apps/windows/fanart) belongs to the people
  who made it, each credited with where it was posted in apps/windows/fanart/README.md. The games,
  characters and names on the covers belong to their owners. None of it is Phosphor's work or under
  Phosphor's licence, and Phosphor isn't affiliated with any of them.
- **Releases up to 0.27.40** included two background textures drawn after characters and items from
  Nintendo's games ("mario" and "zelda"). Those are Nintendo's; they were not Phosphor's to include and
  are removed from 0.27.41 on. Phosphor is not affiliated with or endorsed by Nintendo.
- **MiSTer, the SuperStation One:** Phosphor is unofficial. It is not made or supported by MiSTer-devel
  or by the makers of the SuperStation One (Taki and Retro Remake), whom it thanks.

To have something credited differently or taken out, open an issue:
https://github.com/f3bandit/phosphor_os/issues
