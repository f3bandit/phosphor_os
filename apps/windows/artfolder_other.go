// Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md).

//go:build !windows

package main

import "errors"

func browseFolder() (string, error) { return "", errors.New("type or paste the folder's path") }
