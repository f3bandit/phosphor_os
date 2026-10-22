#!/bin/sh
# Builds dist/phosphor-<version>-armhf.tar.gz: ARM binaries, scripts, example settings, modules.
# Needs the ARM builds (frontend/build-arm, system/build-arm) and, for modules, ../phosphor-modules.
set -e
cd "$(dirname "$0")/.."
V="$(sed -n 's/#define PHOS_VERSION "\(.*\)"/\1/p' frontend/src/settings.h)"
D="dist/phosphor-$V"
rm -rf "$D"
mkdir -p "$D/bin" "$D/config"
cp frontend/build-arm/phosphor system/build-arm/phosphord system/build-arm/phos "$D/bin/"
# phosphor-main (GPL-3.0, its own repo): copied in when built next to this repo; never started by the installer
if [ -x ../phosphor-main/bin/MiSTer ]; then cp ../phosphor-main/bin/MiSTer "$D/bin/MiSTer_phosphor"; fi
# phosphor-menu (the menu core with the front ports): its GitHub Actions build, saved next to the repo
if [ -f ../phosphor-menu/menu_phosphor.rbf ]; then mkdir -p "$D/cores"; cp ../phosphor-menu/menu_phosphor.rbf "$D/cores/"; fi
for f in "$D"/bin/*; do file "$f" | grep -q "ARM" || { echo "$f isn't an ARM build" >&2; exit 1; }; done
cp install/install.sh install/uninstall.sh install/phosphor.sh install/menu-entry.sh LICENSE.md "$D/"
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
	for m in diag cifs-helper backup-agent health-watch update-guard cloud-sync; do [ -d "$M/$m" ] && cp -r "$M/$m" "$D/modules/"; done
fi
printf '%s (%s)\n' "$V" "$(git rev-parse --short HEAD)" > "$D/VERSION"
chmod 755 "$D"/*.sh "$D"/bin/*
tar -C dist -czf "dist/phosphor-$V-armhf.tar.gz" "phosphor-$V"
ls -la "dist/phosphor-$V-armhf.tar.gz"
