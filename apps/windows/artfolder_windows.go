// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

//go:build windows

package main

import (
	"errors"
	"strings"
)

// browseFolder: Windows' own folder picker; "" and no error when it's closed without a choice
func browseFolder() (string, error) {
	out, err := powershell(`$f = (New-Object -ComObject Shell.Application).BrowseForFolder(0, 'Choose the folder with your box art', 0x51, 0); if ($f) { $f.Self.Path }`)
	if err != nil {
		return "", errors.New("couldn't open the folder picker: type or paste the folder's path")
	}
	return strings.TrimSpace(out), nil
}
