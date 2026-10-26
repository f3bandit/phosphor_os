//go:build !windows

package main

import (
	"os"
	"path/filepath"
)

// away from Windows (development): Steam's usual place on Linux, or PHOSPHOR_STEAM
func steamPath() string {
	if p := os.Getenv("PHOSPHOR_STEAM"); p != "" {
		return p
	}
	h, _ := os.UserHomeDir()
	for _, p := range []string{filepath.Join(h, ".steam", "steam"), filepath.Join(h, ".local", "share", "Steam")} {
		if fi, err := os.Stat(p); err == nil && fi.IsDir() {
			return p
		}
	}
	return ""
}
