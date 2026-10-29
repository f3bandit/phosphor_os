#!/bin/sh
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
# Builds dist/Phosphor.exe the one way it should be: a windowed program (no cmd window), no debug info.
# Refuses to finish if the result would open a console.
set -e
cd "$(dirname "$0")"
# Phosphor's own ScreenScraper developer login: a build secret (PHOSPHOR_SS_DEVID / PHOSPHOR_SS_DEVPASSWORD in
# the environment), written to a Go file only for this build and deleted after, never in the repo.
# Without it, people use their own developer ID.
SS=""
if [ -n "$PHOSPHOR_SS_DEVID" ] && [ -n "$PHOSPHOR_SS_DEVPASSWORD" ]; then
	SS=1
	python3 ../../tools/secrets/ss_scramble.py go ss_secret_gen.go    # scrambled, not plain text in the exe
	trap 'rm -f ss_secret_gen.go' EXIT
fi
# the open-source licences, served for Settings > About > Open-source licences (the copy isn't kept in git)
cp ../../THIRD_PARTY_NOTICES.md web/THIRD_PARTY_NOTICES.md
CGO_ENABLED=0 GOOS=windows GOARCH=amd64 go build -trimpath -ldflags "-H windowsgui -s -w" -o dist/Phosphor.exe .
python3 - <<'PY'
import struct, sys
b = open("dist/Phosphor.exe", "rb").read(4096)
pe = struct.unpack_from("<I", b, 0x3c)[0]
sub = struct.unpack_from("<H", b, pe + 24 + 68)[0]
if sub != 2:
    sys.exit("dist/Phosphor.exe would open a console window (subsystem %d): not built as a windowed program" % sub)
PY
v=$(sed -n 's/^const appVersion = "\(.*\)"/\1/p' main.go)
echo "dist/Phosphor.exe $v (windowed$( [ -n "$SS" ] && echo ", Phosphor's ScreenScraper developer ID built in" ))"; sha256sum dist/Phosphor.exe
