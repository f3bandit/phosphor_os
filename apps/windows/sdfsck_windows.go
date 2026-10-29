// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

//go:build windows

package main

import (
	_ "embed"
	"errors"
	"os"
	"path/filepath"
	"strings"
)

// exfatprogs' fsck.exfat for Windows (fsck-exfat/build.sh; GPL-2.0, source in fsck-exfat/)
//
//go:embed fsck-exfat/fsck.exfat.exe
var fsckExeBytes []byte

func fsckExe() (string, func(), error) {
	dir, err := os.MkdirTemp("", "phosphor-fsck-")
	if err != nil {
		return "", nil, err
	}
	p := filepath.Join(dir, "fsck.exfat.exe")
	if err := os.WriteFile(p, fsckExeBytes, 0o755); err != nil {
		os.RemoveAll(dir)
		return "", nil, err
	}
	return p, func() { os.RemoveAll(dir) }, nil
}

// the card's exFAT volume (the SuperStation's card has one, with a drive letter)
func fsckTarget(d disk) (string, error) {
	for _, l := range strings.Fields(d.Letters) {
		l = strings.TrimSuffix(strings.ToUpper(l), `\`)
		if len(l) != 2 || l[1] != ':' {
			continue
		}
		t := `\\.\` + l
		if st, err := exfatState(t); err == nil && st != "not-exfat" {
			return t, nil
		}
	}
	if strings.TrimSpace(d.Letters) == "" {
		return "", errors.New("Windows hasn't given the card a drive letter, so its file system can't be checked (a freshly flashed card has none until the SuperStation sets it up)")
	}
	return "", errors.New("the card has no exFAT volume (the SuperStation's card has one): nothing was done")
}

func fsckName(target string) string { return strings.TrimPrefix(target, `\\.\`) }
