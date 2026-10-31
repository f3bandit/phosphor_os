// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// End to end: the SS1's "Local files" source (phosphord's src_local.c) on a pretend SD card with every
// kind of local art: gamelist.xml, media/ folders (box2d before box3d, names with and without region),
// media/<name>.txt, and an Update All artwork pack (docs/<System>/Artwork: the exact name, the CRC of a
// renamed file, a bare title that means one game, one that means two, Game Boy's shared GBC folder).
// The SS1's own run, one system at a time, and the PC app's way in (scrape_local) must agree.
//
//	PHOSD_BIN=/path/to/phosphord go test -run TestE2ELocal -v

import (
	"bytes"
	"context"
	"crypto/rand"
	"fmt"
	"hash/crc32"
	"image"
	"image/color"
	"image/jpeg"
	"image/png"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

type rgb struct{ r, g, b uint8 }

var (
	cRed     = rgb{220, 30, 30}
	cBlue    = rgb{30, 30, 220}
	cGreen   = rgb{30, 200, 30}
	cYellow  = rgb{230, 220, 30}
	cMagenta = rgb{220, 30, 220}
	cCyan    = rgb{30, 220, 220}
	cOrange  = rgb{240, 140, 20}
	cGray    = rgb{128, 128, 128}
	cWhite   = rgb{250, 250, 250}
)

func writePic(t *testing.T, path string, c rgb) {
	os.MkdirAll(filepath.Dir(path), 0o755)
	img := image.NewRGBA(image.Rect(0, 0, 120, 160))
	for y := 0; y < 160; y++ {
		for x := 0; x < 120; x++ {
			img.Set(x, y, color.RGBA{c.r, c.g, c.b, 255})
		}
	}
	var b bytes.Buffer
	if strings.HasSuffix(strings.ToLower(path), ".png") {
		png.Encode(&b, img)
	} else {
		jpeg.Encode(&b, img, &jpeg.Options{Quality: 95})
	}
	if err := os.WriteFile(path, b.Bytes(), 0o644); err != nil {
		t.Fatal(err)
	}
}

// a game file with random content; its CRC32 (lower-case hex)
func writeGame(t *testing.T, path string) string {
	os.MkdirAll(filepath.Dir(path), 0o755)
	b := make([]byte, 4096)
	rand.Read(b)
	if err := os.WriteFile(path, b, 0o644); err != nil {
		t.Fatal(err)
	}
	return fmt.Sprintf("%08x", crc32.ChecksumIEEE(b))
}

func localCard(t *testing.T) func(fat string) {
	return func(fat string) {
		w := func(rel, s string) {
			p := filepath.Join(fat, rel)
			os.MkdirAll(filepath.Dir(p), 0o755)
			os.WriteFile(p, []byte(s), 0o644)
		}
		snes := filepath.Join(fat, "games", "SNES")
		// media/box2d by title words (no region), ahead of media/box3d's exact name
		writeGame(t, filepath.Join(snes, "Super Mario World (USA).sfc"))
		writePic(t, filepath.Join(snes, "media", "box2d", "Super Mario World.png"), cRed)
		writePic(t, filepath.Join(snes, "media", "box3d", "Super Mario World (USA).png"), cBlue)
		// gamelist.xml: its picture and details
		writeGame(t, filepath.Join(snes, "Zelda (USA).sfc"))
		writePic(t, filepath.Join(snes, "media", "images", "zelda-cover.png"), cGreen)
		w("games/SNES/gamelist.xml", `<?xml version="1.0"?>
<gameList>
 <game><path>./Zelda (USA).sfc</path><name>Zelda</name><desc>A gamelist description &amp; more.</desc>
  <image>./media/images/zelda-cover.png</image><developer>Nintendo</developer><releasedate>19920413T000000</releasedate></game>
</gameList>`)
		// media/ itself, the exact name, and a .txt description
		writeGame(t, filepath.Join(snes, "F-Zero (Japan).sfc"))
		writePic(t, filepath.Join(snes, "media", "F-Zero (Japan).jpg"), cYellow)
		w("games/SNES/media/F-Zero (Japan).txt", "A media description.")
		// the artwork pack
		art := filepath.Join(fat, "docs", "SNES", "Artwork")
		crcMK := writeGame(t, filepath.Join(snes, "MK.sfc")) // renamed: found by CRC and size
		writePic(t, filepath.Join(art, "Super Mario Kart (USA).jpg"), cMagenta)
		writeGame(t, filepath.Join(snes, "Star Fox (USA).sfc")) // the exact name
		writePic(t, filepath.Join(art, "Star Fox (USA).jpg"), cCyan)
		writeGame(t, filepath.Join(snes, "Pilotwings (Europe).sfc")) // a bare title only one game has
		writePic(t, filepath.Join(art, "Pilotwings (USA).jpg"), cOrange)
		writeGame(t, filepath.Join(snes, "Tetris (World).sfc")) // a bare title two games have: none
		writePic(t, filepath.Join(art, "Tetris (Japan).jpg"), cWhite)
		writePic(t, filepath.Join(art, "Tetris (USA).jpg"), cWhite)
		writeGame(t, filepath.Join(snes, "Nothing Here (USA).sfc"))
		// above the system folders: games/media is no system's, never used
		writePic(t, filepath.Join(fat, "games", "media", "box2d", "Nothing Here (USA).png"), cWhite)
		writePic(t, filepath.Join(fat, "games", "media", "Nothing Here.png"), cWhite)
		w("docs/SNES/Artwork/index.tsv", "#name\tcrc\tsize\tkey\n"+
			"Super Mario Kart (USA)\t"+strings.ToUpper(crcMK)+"\t4096\tSuper Mario Kart (USA)\n"+
			"Star Fox (USA)\t00000000\t1\tStar Fox (USA)\n"+
			"Pilotwings (USA)\t\t\tPilotwings (USA)\n"+
			"Tetris (Japan)\t\t\tTetris (Japan)\n"+
			"Tetris (USA)\t\t\tTetris (USA)\n")
		w("docs/SNES/Artwork/gameinfo.tsv", "#key\tname\tyear\tgenre\tdeveloper\tplayers\n"+
			"Super Mario Kart (USA)\tSuper Mario Kart\t1992\tRacing\tNintendo EAD\t1-2\n")
		w("docs/SNES/Artwork/synopsis_en.tsv", "#key\tsynopsis\nSuper Mario Kart (USA)\tA pack synopsis.\n")
		// Game Boy: its pack is in GBC's folder
		writeGame(t, filepath.Join(fat, "games", "GAMEBOY", "Kirby (USA).gb"))
		writePic(t, filepath.Join(fat, "docs", "GBC", "Artwork", "Kirby (USA).jpg"), cGray)
	}
}

type localWant struct {
	art  *rgb
	desc string
	more map[string]string
}

var localWants = map[string]localWant{
	"Super Mario World (USA)": {art: &cRed},
	"Zelda (USA)":             {art: &cGreen, desc: "A gamelist description & more.", more: map[string]string{"developer": "Nintendo", "year": "1992"}},
	"F-Zero (Japan)":          {art: &cYellow, desc: "A media description."},
	"MK":                      {art: &cMagenta, desc: "A pack synopsis.", more: map[string]string{"genre": "Racing", "developer": "Nintendo EAD", "players": "1-2", "year": "1992"}},
	"Star Fox (USA)":          {art: &cCyan},
	"Pilotwings (Europe)":     {art: &cOrange},
	"Tetris (World)":          {},
	"Nothing Here (USA)":      {},
	"Kirby (USA)":             {art: &cGray},
}

func e2eUse(t *testing.T, addr, token string) {
	enc, err := protect(token)
	if err != nil {
		t.Skip("no token store here:", err)
	}
	cfgMu.Lock()
	saved := cfg
	cfg.Address, cfg.TokenEnc = addr, enc
	cfgMu.Unlock()
	t.Cleanup(func() { cfgMu.Lock(); cfg = saved; cfgMu.Unlock() })
}

// the library's games by title (only Local files on; everything scanned)
func localReady(t *testing.T, n int) map[string]int64 {
	if err := ss1Call(map[string]any{"cmd": "scrape_sources", "order": []string{"local", "libretro", "arcadedb", "launchbox", "thegamesdb", "screenscraper", "igdb"},
		"off": []string{"libretro", "arcadedb", "launchbox", "thegamesdb", "screenscraper", "igdb"}}, nil); err != nil {
		t.Fatal(err)
	}
	ss1Call(map[string]any{"cmd": "rescan"}, nil)
	for i := 0; ; i++ {
		var r struct {
			Games []struct {
				ID    int64  `json:"id"`
				Title string `json:"title"`
			} `json:"games"`
		}
		m := map[string]int64{}
		for _, sys := range []string{"SNES", "GAMEBOY"} {
			r.Games = nil
			if err := ss1Call(map[string]any{"cmd": "games", "system": sys, "limit": 100}, &r); err != nil {
				t.Fatal(err)
			}
			for _, g := range r.Games {
				m[g.Title] = g.ID
			}
		}
		if len(m) >= n {
			return m
		}
		if i > 100 {
			t.Fatalf("%d games in the library, want %d", len(m), n)
		}
		time.Sleep(100 * time.Millisecond)
	}
}

// the game's box art on the SS1, as an average colour; nil if none
func artColour(t *testing.T, id int64) *rgb {
	r, err := authed("GET", fmt.Sprintf("/art/%d", id), nil, 20*time.Second)
	if err != nil {
		t.Fatal(err)
	}
	defer r.Body.Close()
	b, _ := io.ReadAll(r.Body)
	if r.StatusCode != 200 {
		return nil
	}
	img, _, err := image.Decode(bytes.NewReader(b))
	if err != nil {
		t.Fatalf("game %d: art doesn't decode: %v", id, err)
	}
	var sr, sg, sb, n uint64
	bd := img.Bounds()
	for y := bd.Min.Y; y < bd.Max.Y; y += 4 {
		for x := bd.Min.X; x < bd.Max.X; x += 4 {
			r, g, b, _ := img.At(x, y).RGBA()
			sr += uint64(r >> 8)
			sg += uint64(g >> 8)
			sb += uint64(b >> 8)
			n++
		}
	}
	return &rgb{uint8(sr / n), uint8(sg / n), uint8(sb / n)}
}

func near(a, b rgb) bool {
	d := func(x, y uint8) int {
		if x > y {
			return int(x - y)
		}
		return int(y - x)
	}
	return d(a.r, b.r) < 24 && d(a.g, b.g) < 24 && d(a.b, b.b) < 24
}

func checkLocal(t *testing.T, ids map[string]int64) {
	for title, w := range localWants {
		id, ok := ids[title]
		if !ok {
			t.Errorf("%s: not in the library", title)
			continue
		}
		got := artColour(t, id)
		switch {
		case w.art == nil && got != nil:
			t.Errorf("%s: box art %v, want none", title, *got)
		case w.art != nil && got == nil:
			t.Errorf("%s: no box art, want %v", title, *w.art)
		case w.art != nil && !near(*got, *w.art):
			t.Errorf("%s: box art %v, want %v (the wrong picture)", title, *got, *w.art)
		}
		var g struct {
			Game map[string]any `json:"game"`
		}
		if err := ss1Call(map[string]any{"cmd": "game", "id": id}, &g); err != nil {
			t.Fatal(err)
		}
		if w.desc != "" && g.Game["description"] != w.desc {
			t.Errorf("%s: description %q, want %q", title, g.Game["description"], w.desc)
		}
		for k, v := range w.more {
			if fmt.Sprint(g.Game[k]) != v {
				t.Errorf("%s: %s %q, want %q", title, k, g.Game[k], v)
			}
		}
	}
}

func scrapeWait(t *testing.T) {
	for i := 0; ; i++ {
		var s struct {
			Running bool   `json:"running"`
			On      bool   `json:"on"`
			Status  string `json:"status"`
		}
		ss1Call(map[string]any{"cmd": "scrape"}, &s)
		if i > 5 && !s.Running && !s.On {
			return
		}
		if i > 300 {
			t.Fatalf("the SS1's run didn't finish: %q", s.Status)
		}
		time.Sleep(100 * time.Millisecond)
	}
}

// games waiting to be looked up (the status, not scrape_todo: that's the PC app taking over the run)
func pending(t *testing.T) int {
	var s struct {
		Pending int `json:"pending"`
	}
	if err := ss1Call(map[string]any{"cmd": "scrape"}, &s); err != nil {
		t.Fatal(err)
	}
	return s.Pending
}

// the SS1's own run: one system first (only it is done), then the rest
func TestE2ELocalOnTheSS1(t *testing.T) {
	addr, token, _ := e2eSS1With(t, localCard(t))
	e2eUse(t, addr, token)
	ids := localReady(t, len(localWants))
	if err := ss1Call(map[string]any{"cmd": "scrape", "on": true, "system": "GAMEBOY"}, nil); err != nil {
		t.Fatal(err)
	}
	scrapeWait(t)
	if c := artColour(t, ids["Kirby (USA)"]); c == nil || !near(*c, cGray) {
		t.Fatalf("Look up this system (Game Boy): Kirby's art %v", c)
	}
	if n := pending(t); n != len(localWants)-1 {
		t.Fatalf("after the Game Boy run, %d SNES games waiting, want all %d", n, len(localWants)-1)
	}
	if err := ss1Call(map[string]any{"cmd": "scrape", "on": true}, nil); err != nil {
		t.Fatal(err)
	}
	scrapeWait(t)
	checkLocal(t, ids)
	// one game again, now: it's looked up on its own
	if err := ss1Call(map[string]any{"cmd": "rescrape", "id": ids["Tetris (World)"], "now": true}, nil); err != nil {
		t.Fatal(err)
	}
	scrapeWait(t)
	if n := pending(t); n != 0 {
		t.Fatalf("%d games still waiting after Look up now", n)
	}
}

// the PC app's way: scrape_local for each game (what Look up on this PC does for the Local files source)
func TestE2ELocalFromThePC(t *testing.T) {
	addr, token, _ := e2eSS1With(t, localCard(t))
	e2eUse(t, addr, token)
	ids := localReady(t, len(localWants))
	for title, id := range ids {
		var r struct {
			Found bool `json:"found"`
			Art   bool `json:"art"`
		}
		if err := ss1Call(map[string]any{"cmd": "scrape_local", "id": id}, &r); err != nil {
			t.Fatal(title, err)
		}
		if w := localWants[title]; r.Art != (w.art != nil) {
			t.Errorf("%s: scrape_local art %v, want %v", title, r.Art, w.art != nil)
		}
	}
	checkLocal(t, ids)
}

// Look up on this PC itself (Local files first, the SS1 reading its drives): one game, then everything
func TestE2ELocalLookUpOnThisPC(t *testing.T) {
	addr, token, _ := e2eSS1With(t, localCard(t))
	e2eUse(t, addr, token)
	fanart.Lock()
	fanart.byKey, fanart.at = map[string]*fanGame{}, time.Now()
	fanart.Unlock()
	t.Cleanup(func() { fanart.Lock(); fanart.byKey = nil; fanart.Unlock() })
	ids := localReady(t, len(localWants))
	run := func(id int64) *scrapeRun {
		ctx, cancel := context.WithTimeout(context.Background(), time.Minute)
		t.Cleanup(cancel)
		s := &scrapeRun{ctx: ctx, cancel: cancel, gameID: id}
		s.st = scrapeState{Running: true, Started: time.Now().Unix()}
		if !s.lookupAll() {
			t.Fatalf("the run stopped: %q", s.st.Status)
		}
		return s
	}
	// Look it up now: this game only
	s := run(ids["Star Fox (USA)"])
	if s.st.Found != 1 || s.st.Done != 1 {
		t.Fatalf("one game: %d done, %d found, want 1 and 1", s.st.Done, s.st.Found)
	}
	if c := artColour(t, ids["F-Zero (Japan)"]); c != nil {
		t.Fatal("one game's run looked up another")
	}
	s = run(0)
	t.Logf("%d done, %d found, %d not found; notes %q", s.st.Done, s.st.Found, s.st.NotFound, s.st.Notes)
	checkLocal(t, ids)
}

// a folder a game (as disc games are kept), each with its own gamelist.xml: more folders than the SS1
// keeps read at once (64), all still used
func TestE2ELocalManyFolders(t *testing.T) {
	const n = 90
	addr, token, _ := e2eSS1With(t, func(fat string) {
		for i := 1; i <= n; i++ {
			d := filepath.Join(fat, "games", "SNES", fmt.Sprintf("Game %03d", i))
			writeGame(t, filepath.Join(d, fmt.Sprintf("Game %03d (USA).sfc", i)))
			writePic(t, filepath.Join(d, "cover.png"), cGreen)
			os.WriteFile(filepath.Join(d, "gamelist.xml"), []byte(fmt.Sprintf(`<gameList><game><path>./Game %03d (USA).sfc</path><image>./cover.png</image><desc>Game %d.</desc></game></gameList>`, i, i)), 0o644)
		}
	})
	e2eUse(t, addr, token)
	if err := ss1Call(map[string]any{"cmd": "scrape_sources", "order": []string{"local", "libretro", "arcadedb", "launchbox", "thegamesdb", "screenscraper", "igdb"},
		"off": []string{"libretro", "arcadedb", "launchbox", "thegamesdb", "screenscraper", "igdb"}}, nil); err != nil {
		t.Fatal(err)
	}
	ss1Call(map[string]any{"cmd": "rescan"}, nil)
	for i := 0; pending(t) < n; i++ {
		if i > 100 {
			t.Fatalf("%d games waiting, want %d", pending(t), n)
		}
		time.Sleep(100 * time.Millisecond)
	}
	ss1Call(map[string]any{"cmd": "scrape", "on": true}, nil)
	scrapeWait(t)
	var r struct {
		Games []struct {
			ID    int64  `json:"id"`
			Title string `json:"title"`
		} `json:"games"`
	}
	ss1Call(map[string]any{"cmd": "games", "system": "SNES", "limit": 500}, &r)
	missing := 0
	for _, g := range r.Games {
		if c := artColour(t, g.ID); c == nil || !near(*c, cGreen) {
			missing++
		}
	}
	if len(r.Games) != n || missing > 0 {
		t.Fatalf("%d games, %d without their gamelist.xml's box art", len(r.Games), missing)
	}
}
