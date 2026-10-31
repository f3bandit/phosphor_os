// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

//go:build !windows

package main

import "errors"

func browseFolder() (string, error) { return "", errors.New("type or paste the folder's path") }
