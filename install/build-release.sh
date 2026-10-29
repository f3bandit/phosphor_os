#!/bin/sh
# Builds dist/phosphor-<version>-armhf.tar.gz: ARM binaries, scripts, example settings, modules.
# Needs the ARM builds (frontend/build-arm, system/build-arm) and, for modules, ../phosphor-modules.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
set -e
cd "$(dirname "$0")/.."
V="$(sed -n 's/#define PHOS_VERSION "\(.*\)"/\1/p' frontend/src/settings.h)"
D="dist/phosphor-$V"
rm -rf "$D"
mkdir -p "$D/bin" "$D/config"
# a failed build leaves the old binary behind: refuse anything older than its sources
pairs="frontend/build-arm/phosphor:frontend/src system/build-arm/phosphord:system/phosphord"
[ -x ../phosphor-main/bin/MiSTer ] && pairs="$pairs ../phosphor-main/bin/MiSTer:../phosphor-main"
for pair in $pairs; do
	set -- "${pair%%:*}" "${pair#*:}"
	newer="$(find "$2" -newer "$1" -type f \( -name '*.c' -o -name '*.h' -o -name '*.cpp' \) | head -1)"
	[ -z "$newer" ] || { echo "$1 is older than $newer: build it first" >&2; exit 1; }
done
cp frontend/build-arm/phosphor system/build-arm/phosphord system/build-arm/phos system/build-arm/phos-nvme "$D/bin/"
# phosphord with Phosphor's ScreenScraper developer login (GitHub Actions "Secret builds", downloaded to
# PHOS_SECRET_BUILDS): the login is a repo secret, so release builds take that phosphord
if [ -n "$PHOS_SECRET_BUILDS" ]; then
	[ -x "$PHOS_SECRET_BUILDS/phosphord" ] || { echo "PHOS_SECRET_BUILDS has no phosphord" >&2; exit 1; }
	cp "$PHOS_SECRET_BUILDS/phosphord" "$D/bin/phosphord"
	echo "phosphord: the Secret builds one (Phosphor's ScreenScraper developer ID built in)"
fi
# phosphor-main (GPL-3.0, its own repo): copied in when built next to this repo; never started by the installer
if [ -x ../phosphor-main/bin/MiSTer ]; then cp ../phosphor-main/bin/MiSTer "$D/bin/MiSTer_phosphor"; fi
# RetroAchievements' unlock sound (from odelot's RetroAchievements Main, in phosphor-main)
if [ -f ../phosphor-main/achievement.wav ]; then cp ../phosphor-main/achievement.wav "$D/achievement.wav"; fi
# phosphor-menu (the menu core with the front ports): its GitHub Actions build, saved next to the repo
# (with menu_phosphor.rbf.commit: the commit it was built from, for SOURCE.md)
if [ -f ../phosphor-menu/menu_phosphor.rbf ]; then mkdir -p "$D/cores"; cp ../phosphor-menu/menu_phosphor.rbf "$D/cores/"; fi
for f in "$D"/bin/*; do case "$f" in *.sh) continue ;; esac; file "$f" | grep -q "ARM" || { echo "$f isn't an ARM build" >&2; exit 1; }; done
# Phosphor's own programs are linked with musl, not glibc (LGPL): a glibc build would need its source
# and relinking terms shipped too (frontend/cmake/armhf.cmake, tools/musl/build-toolchain.sh)
for f in phosphor phosphord phos phos-nvme; do
	if grep -qa "GNU C Library" "$D/bin/$f"; then echo "bin/$f is linked with glibc: build with tools/musl first" >&2; exit 1; fi
done
cp install/install.sh install/uninstall.sh install/phosphor.sh install/update.sh install/ss1_video_ini.sh LICENSE.md THIRD_PARTY_NOTICES.md "$D/"
# run from the NVMe / a USB drive: the start-up lines for the SD card's u-boot.txt and their script
mkdir -p "$D/nvmeboot" && cp install/nvmeboot/nvmeboot.sh install/nvmeboot/u-boot.phosphor.txt "$D/nvmeboot/"
# the GPL builds (Phosphor's Main, the menu core): their licence, and where their exact source is
mkdir -p "$D/licenses"
if [ -f "$D/bin/MiSTer_phosphor" ] || [ -d "$D/cores" ]; then
	cp ../phosphor-main/LICENSE "$D/licenses/GPL-3.0.txt"
	src_line() {   # repo dir, repo name[, file naming the commit it was built from]: the commit, on GitHub already
		c="$(git -C "$1" rev-parse HEAD)"
		if [ -n "${3:-}" ] && [ -f "$3" ]; then c="$(head -1 "$3")"; fi
		on_gh=""
		for h in $(git -C "$1" ls-remote origin 'refs/heads/*' | cut -f1); do
			if git -C "$1" merge-base --is-ancestor "$c" "$h" 2> /dev/null; then on_gh=1; break; fi
		done
		[ -n "$on_gh" ] || { echo "$2 $c isn't on GitHub: push it first (its source has to be public)" >&2; exit 1; }
		printf 'https://github.com/f3bandit/%s/tree/%s' "$2" "$c"
	}
	main_src="" menu_src=""
	if [ -f "$D/bin/MiSTer_phosphor" ]; then main_src="$(src_line ../phosphor-main phosphor-main)"; fi
	if [ -d "$D/cores" ]; then menu_src="$(src_line ../phosphor-menu phosphor-menu ../phosphor-menu/menu_phosphor.rbf.commit)"; fi
	{
		echo "# Source of the GPL programs in this release"
		echo
		echo "Phosphor $V. These two are free software under the GNU General Public License version 3"
		echo "(licenses/GPL-3.0.txt). Their complete source, the exact version built for this release:"
		echo
		[ -z "$main_src" ] || echo "- bin/MiSTer_phosphor (Phosphor's Main, a fork of MiSTer's Main with odelot's RetroAchievements): $main_src"
		[ -z "$menu_src" ] || echo "- cores/menu_phosphor.rbf (Phosphor's menu core, a fork of MiSTer's Menu core, GPL-2.0-or-later, with GPL-3.0 additions): $menu_src"
		[ -z "$menu_src" ] || echo "  Its CRT modes use the native video path (rtl/native_video_*) from Retro-Remake's menu_ConsoleMode (https://github.com/Retro-Remake/menu_consolemode, GPL-3.0-or-later)."
		echo
		echo "achievement.wav, the RetroAchievements unlock sound, comes with odelot's RetroAchievements Main"
		echo "(https://github.com/odelot/Main_MiSTer); see THIRD_PARTY_NOTICES.md."
		echo
		echo "The rest of Phosphor: https://github.com/f3bandit/phosphor_os/tree/$(git rev-parse HEAD) (LICENSE.md)."
	} > "$D/SOURCE.md"
fi
mkdir -p "$D/wallpaper" && cp install/wallpaper/phosphor-wallpaper.png "$D/wallpaper/"
# MiSTer's updaters, started from Phosphor: a restore point of MiSTer's system files first (when
# update-guard is installed), then the updater itself, one after the other in MiSTer's terminal
mkdir -p "$D/scripts"
for u in update update_all; do
	label=before-get-cores; [ "$u" = update_all ] && label=before-update-all
	cat > "$D/scripts/$u.sh" <<SH
#!/bin/bash
# Phosphor: $u.sh from MiSTer's Scripts folder, after a restore point (update-guard), so nothing
# copies files in the background while it runs
G=/media/fat/phosphor/modules/update-guard/update-guard.sh
if [ -x "\$G" ]; then
	echo "Phosphor: saving a restore point of MiSTer's system files first..."
	"\$G" save-point $label || echo "(couldn't save one; the update goes ahead)"
	echo
fi
cd /media/fat/Scripts && ./$u.sh "\$@"
rc=\$?
# the downloader puts MiSTer's own menu core back: Phosphor's again, so going back shows Phosphor's
# loading screen, not MiSTer's static (a restart by the downloader does the same at shutdown)
/media/fat/phosphor/phosphor.sh menu-core > /dev/null 2>&1
exit \$rc
SH
done
cp system/config/systems.ini "$D/config/"
cat > "$D/config/scraper.ini" <<'INI'
[sources]
; asked in this order; for each field the first answer wins
order = libretro, arcadedb, launchbox, thegamesdb, screenscraper, igdb
off =

; Sources that need an account or key (Phosphor works without them):
[screenscraper]
user =
password =
devid =
devpassword =

[thegamesdb]
apikey =

[igdb]
client_id =
client_secret =
INI
M=../phosphor-modules
if [ -d "$M/lib" ]; then
	mkdir -p "$D/modules"
	cp -r "$M/lib" "$D/modules/"
	for m in diag storage cifs-helper backup-agent health-watch update-guard cloud-sync; do [ -d "$M/$m" ] && cp -r "$M/$m" "$D/modules/"; done
fi
printf '%s (%s)\n' "$V" "$(git rev-parse --short HEAD)" > "$D/VERSION"
chmod 755 "$D"/*.sh "$D"/bin/*
# checksums of every file, so the updater can tell a damaged package from a good one
(cd "$D" && find . -type f ! -name SHA256SUMS | sed 's|^\./||' | sort | xargs sha256sum > SHA256SUMS)
tar -C dist -czf "dist/phosphor-$V-armhf.tar.gz" "phosphor-$V"
ls -la "dist/phosphor-$V-armhf.tar.gz"
