# Additional permission for Retro Remake

Copyright (C) 2026 f3bandit. Phosphor OS (the TV interface, phosphord, the apps, tools, scripts and documentation) is
f3bandit's; the memory card manager and the controller tester are f3bandit's idea and design.

This repository is licensed to everyone under the PolyForm Noncommercial License 1.0.0 (LICENSE.md),
which doesn't allow commercial use. In addition, f3bandit grants **Retro Remake** and **Taki Udon**
(GitHub: Takiiiiiiii) the following, for f3bandit's own code in this repository:

- Permission to use, copy, modify, merge, publish, distribute, sublicense and sell it, in whole or in
  part, in any product of theirs (including closed-source software such as Console Mode, and their
  hardware), without the conditions of the PolyForm Noncommercial License (commercial use included),
  free of charge.
- The only condition: a mention of f3bandit in the credits of anything that uses it.

This includes, by name:

- **The dock IR remote upgrade**: firmware/dockir/ss1update.patch and build.sh (DockIR updatable from the
  SuperStation: the `DIRBOOT!` feature report, the `DIRUP001` id), system/phosphord/dockir.c (the updater on
  the SuperStation: restarts the receiver into its bootloader and copies the .uf2 onto it), and the remote's
  button mapping in frontend/src/input.c (`map_ir`). Retro Remake may also merge the patch into DockIR itself.
- **The safe shutdown**: install/phosphor.sh `safe-stop`, phosphord's `safe_stop` command (system/phosphord/
  api.c) and the TV shell's power window (frontend/src/ui.c `power_run`).

Not covered: what LICENSE.md and THIRD_PARTY_NOTICES.md list as someone else's: frontend/third_party/
and system/third_party/ (LVGL, cJSON, SQLite, rcheevos, libchdr, stb, dr_libs ...), the fonts, the
fan-made box art in apps/windows/fanart/, and achievement.wav. phosphor-main and phosphor-menu (the
MiSTer Main and menu core forks) are separate repositories under the GPL and aren't covered here.

This permission is personal to Retro Remake and Taki Udon and isn't transferable, except to people
working on Retro Remake products for them. Everyone else gets this code under LICENSE.md only.

The code is provided "as is", without warranty of any kind.
