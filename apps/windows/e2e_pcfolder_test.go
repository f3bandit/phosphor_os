// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// End to end: Look up on this PC with the Local files source and a box art folder on this PC (a Skraper /
// ES-DE / LaunchBox style tree), against a real phosphord. The SS1's own drives have nothing here.
//
//	PHOSD_BIN=/path/to/phosphord go test -run TestE2ELocalPCFolder -v

import (
	"context"
	"path/filepath"
	"testing"
	"time"
)

func TestE2ELocalPCFolder(t *testing.T) {
	addr, token, _ := e2eSS1With(t, func(fat string) {
		snes := filepath.Join(fat, "games", "SNES")
		for _, n := range []string{"Super Mario World (USA).sfc", "Zelda (USA).sfc", "Star Fox (USA).sfc", "F-Zero (Japan).sfc", "Pilotwings (USA).sfc"} {
			writeGame(t, filepath.Join(snes, n))
		}
	})
	e2eUse(t, addr, token)
	pc := t.TempDir()
	// the title words, in a folder named for the system: box2d ahead of box3d
	writePic(t, filepath.Join(pc, "Super Nintendo", "box2d", "Super Mario World.png"), cRed)
	writePic(t, filepath.Join(pc, "Super Nintendo", "box3d", "Super Mario World.png"), cBlue)
	// the exact file name, in a folder that doesn't say the system (LaunchBox's "Box - Front")
	writePic(t, filepath.Join(pc, "Images", "Box - Front", "Zelda (USA).png"), cGreen)
	// the title words in another system's folder: not used
	writePic(t, filepath.Join(pc, "megadrive", "covers", "Star Fox.png"), cWhite)
	// only a screenshot: not used
	writePic(t, filepath.Join(pc, "snes", "screenshots", "F-Zero (Japan).png"), cWhite)
	// the system's short name, a JPEG
	writePic(t, filepath.Join(pc, "snes", "covers", "Pilotwings.jpg"), cOrange)

	cfgMu.Lock()
	cfg.ArtFolder = pc
	cfgMu.Unlock()
	t.Cleanup(func() { cfgMu.Lock(); cfg.ArtFolder = ""; cfgMu.Unlock() })
	fanart.Lock()
	fanart.byKey, fanart.at = map[string]*fanGame{}, time.Now()
	fanart.Unlock()
	t.Cleanup(func() { fanart.Lock(); fanart.byKey = nil; fanart.Unlock() })

	ids := localReady(t, 5) // only Local files on
	ctx, cancel := context.WithTimeout(context.Background(), time.Minute)
	defer cancel()
	s := &scrapeRun{ctx: ctx, cancel: cancel}
	s.st = scrapeState{Running: true, Started: time.Now().Unix()}
	if !s.lookupAll() {
		t.Fatalf("the run stopped: %q", s.st.Status)
	}
	t.Logf("%d found, %d not found; notes %q", s.st.Found, s.st.NotFound, s.st.Notes)
	want := map[string]*rgb{"Super Mario World (USA)": &cRed, "Zelda (USA)": &cGreen, "Star Fox (USA)": nil, "F-Zero (Japan)": nil, "Pilotwings (USA)": &cOrange}
	for title, w := range want {
		got := artColour(t, ids[title])
		switch {
		case w == nil && got != nil:
			t.Errorf("%s: box art %v, want none", title, *got)
		case w != nil && got == nil:
			t.Errorf("%s: no box art", title)
		case w != nil && !near(*got, *w):
			t.Errorf("%s: box art %v, want %v", title, *got, *w)
		}
	}
}
