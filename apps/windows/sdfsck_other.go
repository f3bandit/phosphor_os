// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

//go:build !windows

package main

// Not Windows (tests): PHOS_TEST_FSCK is the fsck.exfat to run (fsck-exfat/fsck.exfat.exe under wine,
// or Linux's own), on the image PHOS_TEST_DISK.

import (
	"errors"
	"os"
	"os/exec"
	"path/filepath"
)

func fsckExe() (string, func(), error) {
	p := os.Getenv("PHOS_TEST_FSCK")
	if p == "" {
		return "", nil, errors.New("no PHOS_TEST_FSCK")
	}
	return p, func() {}, nil
}

func fsckTarget(d disk) (string, error) {
	if p := os.Getenv("PHOS_TEST_DISK"); p != "" {
		return p, nil
	}
	return "", errors.New("no test disk")
}

func fsckName(target string) string { return filepath.Base(target) }

func hidden(c *exec.Cmd) *exec.Cmd { return c }
