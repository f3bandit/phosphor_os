#!/bin/sh
# Builds Windows and Linux versions (macOS needs cgo for USB port details: build it on a Mac) into dist/. Needs Go 1.26+ (go.mod pins toolchain go1.27.3).
set -e
cd "$(dirname "$0")"
mkdir -p dist
export CGO_ENABLED=0
GOOS=windows GOARCH=amd64 go build -trimpath -ldflags="-s -w" -o dist/phosphor-link.exe .
GOOS=linux   GOARCH=amd64 go build -trimpath -ldflags="-s -w" -o dist/phosphor-link-linux .
ls -l dist
