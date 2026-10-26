//go:build windows

package main

import "golang.org/x/sys/windows/registry"

// where Steam is installed (its own registry entry)
func steamPath() string {
	k, err := registry.OpenKey(registry.CURRENT_USER, `Software\Valve\Steam`, registry.QUERY_VALUE)
	if err != nil {
		return ""
	}
	defer k.Close()
	p, _, err := k.GetStringValue("SteamPath")
	if err != nil {
		return ""
	}
	return p
}
