//go:build !windows

package main

import "os/exec"

func openWindow(url string) { _ = exec.Command("xdg-open", url).Start() }
