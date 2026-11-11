// Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md).
//go:build !windows

package main

import "os/exec"

func openWindow(url string) { _ = exec.Command("xdg-open", url).Start() }
