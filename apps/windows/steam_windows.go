// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

//go:build windows

package main

import (
	"path/filepath"

	"golang.org/x/sys/windows/registry"
)

// where Steam is installed (its own registry entry)
func steamPath() string {
	k, err := registry.OpenKey(registry.CURRENT_USER, `Software\Valve\Steam`, registry.QUERY_VALUE)
	if err != nil {
		return ""
	}
	defer k.Close()
	p, _, err := k.GetStringValue("SteamPath")
	if err != nil || p == "" {
		return ""
	}
	return filepath.Clean(p) // the registry's has forward slashes (c:/program files (x86)/steam)
}
