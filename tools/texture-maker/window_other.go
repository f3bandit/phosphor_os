// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
//go:build !windows

package main

import "os/exec"

func openWindow(url string) { _ = exec.Command("xdg-open", url).Start() }
