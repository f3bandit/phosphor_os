#!/bin/sh
# Builds dist/phosphor-<version>-armhf.tar.gz: ARM binaries, scripts, example settings, modules.
# Needs the ARM builds (frontend/build-arm, system/build-arm) and, for modules, ../phosphor-modules.
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
# phosphor-main (GPL-3.0, its own repo): copied in when built next to this repo; never started by the installer
if [ -x ../phosphor-main/bin/MiSTer ]; then cp ../phosphor-main/bin/MiSTer "$D/bin/MiSTer_phosphor"; fi
# phosphor-menu (the menu core with the front ports): its GitHub Actions build, saved next to the repo
if [ -f ../phosphor-menu/menu_phosphor.rbf ]; then mkdir -p "$D/cores"; cp ../phosphor-menu/menu_phosphor.rbf "$D/cores/"; fi
for f in "$D"/bin/*; do file "$f" | grep -q "ARM" || { echo "$f isn't an ARM build" >&2; exit 1; }; done
cp install/install.sh install/uninstall.sh install/phosphor.sh install/update.sh LICENSE.md "$D/"
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
