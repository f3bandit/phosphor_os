// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

import (
	"os"
	"testing"
)

func TestFindSoundtracks(t *testing.T) {
	d := os.Getenv("STEAMTEST")
	if d == "" {
		t.Skip("STEAMTEST not set")
	}
	os.Setenv("PHOSPHOR_STEAM", d)
	l := findSoundtracks()
	if len(l) != 1 || l[0].Title != "Glacier Post Original Soundtrack" || l[0].Format != "FLAC" || l[0].Tracks != 1 || !l[0].HasArt {
		t.Fatalf("got %+v", l)
	}
}
