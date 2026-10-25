#!/bin/sh
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
	python3 -c 'import json,os; open("ss_secret_gen.go","w").write("package main\n\nfunc init() { ssBuiltinID, ssBuiltinPw = %s, %s }\n" % (json.dumps(os.environ["PHOSPHOR_SS_DEVID"]), json.dumps(os.environ["PHOSPHOR_SS_DEVPASSWORD"])))'
	trap 'rm -f ss_secret_gen.go' EXIT
fi
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
