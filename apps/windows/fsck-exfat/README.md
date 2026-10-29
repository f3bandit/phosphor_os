# fsck.exfat for Windows

Phosphor for Windows' **Card health › Check and repair** runs this program on an SD card in the PC's
card reader. It is **exfatprogs' fsck.exfat** (https://github.com/exfatprogs/exfatprogs), the exFAT
checker maintained alongside Linux's exFAT driver, built for Windows. It's a separate program,
**GPL-2.0-or-later** like exfatprogs (COPYING here), not under Phosphor's licence.

- **Upstream:** exfatprogs **1.4.3** (tag `1.4.3`, commit `a37d327`), unchanged. `build.sh` fetches
  exactly that and checks the commit.
- **The Windows layer:** `win32/`. exfatprogs makes POSIX calls on the drive (open, pread, pwrite,
  fsync, lseek, fstat, ioctl, close); `win32/compat/win_compat.h` sends them to `win32/win32_dev.c`. That
  opens a volume (`E:`) unbuffered and write-through, **locks and dismounts it** (it refuses to repair
  a card something else has open), and does every read and write as whole sectors through an aligned
  buffer, reading partial sectors first. An image file works too (tests). The other files in
  `win32/compat/` stand in for Linux headers.
- **Build:** `sh build.sh` (needs git and `x86_64-w64-mingw32-gcc`: Debian/Ubuntu
  `gcc-mingw-w64-x86-64`). `fsck.exfat.exe` lands here; `../build-exe.sh` runs this first and
  Phosphor.exe embeds it (`../sdfsck_windows.go`) and runs it from a temporary folder.
- **Source of the binary in a Phosphor release:** this folder plus exfatprogs at the tag above.

## Tests

- exfatprogs' own test suite (`tests/test_fsck.sh`: corrupted images, each must be found and then
  repaired clean) passes with this build under Wine, with `FSCK_STRICT_ALIGN=1`, which holds an image
  file to a raw volume's rules (whole sectors at sector offsets, aligned memory) and stops on any
  transfer that breaks them: **19 of the 19 image tests**. (The 4 `upcase_table` script tests need a
  newer mkfs.exfat and dump.exfat than the test machine had to make their images.)
- `../sdfsck_test.go`: a card marked dirty, checked (left as it is) and repaired (the mark cleared);
  a card that isn't exFAT is left alone.
- On Windows itself, with a real card: see the release notes of the Phosphor version that brings it.
