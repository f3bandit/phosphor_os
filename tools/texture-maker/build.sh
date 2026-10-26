#!/bin/sh
# Builds Phosphor Texture Maker for Windows (and Linux) into dist/.
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
set -e
cd "$(dirname "$0")"
mkdir -p dist
export CGO_ENABLED=0
GOOS=windows GOARCH=amd64 go build -trimpath -ldflags="-s -w -H windowsgui" -o dist/PhosphorTextureMaker.exe .
GOOS=linux   GOARCH=amd64 go build -trimpath -ldflags="-s -w" -o dist/phosphor-texture-maker-linux .
ls -l dist
