#!/bin/sh
# Builds dist/Phosphor.exe the one way it should be: a windowed program (no cmd window), no debug info.
# Refuses to finish if the result would open a console.
set -e
cd "$(dirname "$0")"
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
echo "dist/Phosphor.exe $v (windowed)"; sha256sum dist/Phosphor.exe
