package main

// Fan-made box art (fanart/fanart.json): for games no database has, such as ROM hacks and homebrew. The
// list is read from GitHub when box art is looked up (the copy built into the app when GitHub can't be
// reached), so new entries reach everyone without an update. Each cover is fetched from where its maker
// posted it; when that link is gone, from Phosphor's copy in fanart/pack.zip (downloaded once, only then).
// Every cover is credited (hack_by, cover_from; fanart/README.md). A game matches by its name with tags,
// versions and fan-version words taken out ("Roll-chan 2 (MM8)" -> "Roll-chan 2"), on its own system.

import (
	"archive/zip"
	"bytes"
	"context"
	_ "embed"
	"encoding/json"
	"io"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

//go:embed fanart/fanart.json
var fanartBuiltIn []byte

const (
	fanartURL  = "https://raw.githubusercontent.com/f3bandit/phosphor_os/main/apps/windows/fanart/fanart.json"
	fanpackURL = "https://raw.githubusercontent.com/f3bandit/phosphor_os/main/apps/windows/fanart/pack.zip"
)

type fanGame struct {
	Sys         string   `json:"sys"`
	Names       []string `json:"names"`
	Art         string   `json:"art"`
	Pack        string   `json:"pack"`       // its copy in pack.zip
	HackBy      string   `json:"hack_by"`    // credit
	CoverFrom   string   `json:"cover_from"` // where the cover was posted
	Developer   string   `json:"developer"`
	Year        string   `json:"year"`
	Genre       string   `json:"genre"`
	Description string   `json:"description"`
}

var fanart struct {
	sync.Mutex
	at    time.Time
	byKey map[string]*fanGame // sys|key
}

func fanartIndex(b []byte) map[string]*fanGame {
	var f struct {
		Games []*fanGame `json:"games"`
	}
	if json.Unmarshal(b, &f) != nil {
		return nil
	}
	// "=" + the exact name (tags too: "(Part 3)"), and the loose name (tags and versions out: every
	// version of a hack) unless two entries share it
	m := map[string]*fanGame{}
	clash := map[string]bool{}
	for _, g := range f.Games {
		if g.Art == "" {
			continue
		}
		for _, n := range g.Names {
			if k := fanExact(n); k != "" {
				m["="+g.Sys+"|"+k] = g
			}
			k := g.Sys + "|" + fanKey(n)
			if fanKey(n) == "" {
				continue
			}
			if o, ok := m[k]; ok && o != g {
				clash[k] = true
			}
			m[k] = g
		}
	}
	for k := range clash {
		delete(m, k)
	}
	return m
}

// fanKey: the name as fan art is matched by, loosely
func fanKey(t string) string { return amKey(amStrip(amClean(t))) }

// fanExact: the name with only spaces, case and punctuation ignored
func fanExact(t string) string {
	var b strings.Builder
	for _, c := range strings.ToLower(t) {
		if c >= 'a' && c <= 'z' || c >= '0' && c <= '9' || c > 127 {
			b.WriteRune(c)
		}
	}
	return b.String()
}

// fanFind: a game's entry: its exact name first, then the loose one
func fanFind(m map[string]*fanGame, sys, title string) *fanGame {
	if g := m["="+sys+"|"+fanExact(title)]; g != nil {
		return g
	}
	return m[sys+"|"+fanKey(title)]
}

// fanartFor: the fan art for a game, if the list has it (the list is read again after an hour)
func fanartFor(ctx context.Context, sys, title string) *fanGame {
	fanart.Lock()
	defer fanart.Unlock()
	if fanart.byKey == nil || time.Since(fanart.at) > time.Hour {
		m := fanartIndex(fanartBuiltIn)
		if b, rc, _ := netRequest(ctx, "GET", fanartURL, nil, nil, 4<<20); rc == 0 {
			if n := fanartIndex(b); len(n) > 0 {
				m = n
			}
		}
		fanart.byKey, fanart.at = m, time.Now()
	}
	return fanFind(fanart.byKey, sys, title)
}

// fanartLookup: a source like the others (scrape_run.go asks it first)
func fanartLookup(ctx context.Context, s *scrapeRun, g *sgame, r *sres) int {
	f := fanartFor(ctx, g.Sys, g.Title)
	if f == nil || strings.TrimSpace(f.Art) == "" {
		return 0
	}
	desc := f.Description
	if f.HackBy != "" && !strings.Contains(desc, f.HackBy) {
		desc = strings.TrimSpace(desc + " Hack by " + f.HackBy + ".")
	}
	r.set(fDesc, desc)
	r.set(fDev, f.Developer)
	r.set(fYear, f.Year)
	r.set(fGenre, f.Genre)
	r.addArt("fan art", f.Art)
	if f.Pack != "" {
		r.addArt("fan art pack", "pack:"+f.Pack) // only used when the link above doesn't answer
	}
	return 1
}

// ---------- the backup pack ----------

var fanpack struct {
	sync.Mutex
	zr *zip.Reader
}

func fanpackPath() string { return filepath.Join(scrapeCacheDir(), "fanart-pack.zip") }

// fanpackGet: one cover from pack.zip; the pack is downloaded once (and again when a cover isn't in it)
func fanpackGet(ctx context.Context, name string) ([]byte, int) {
	fanpack.Lock()
	defer fanpack.Unlock()
	read := func(zr *zip.Reader) []byte {
		for _, f := range zr.File {
			if f.Name == name {
				rc, err := f.Open()
				if err != nil {
					return nil
				}
				defer rc.Close()
				b, _ := io.ReadAll(io.LimitReader(rc, 16<<20))
				return b
			}
		}
		return nil
	}
	if fanpack.zr == nil {
		if b, err := os.ReadFile(fanpackPath()); err == nil {
			fanpack.zr, _ = zip.NewReader(bytes.NewReader(b), int64(len(b)))
		}
	}
	if fanpack.zr != nil {
		if b := read(fanpack.zr); b != nil {
			return b, 0
		}
	}
	b, rc, _ := netRequest(ctx, "GET", fanpackURL, nil, nil, 64<<20)
	if rc != 0 {
		return nil, rc
	}
	zr, err := zip.NewReader(bytes.NewReader(b), int64(len(b)))
	if err != nil {
		return nil, 1
	}
	_ = os.MkdirAll(filepath.Dir(fanpackPath()), 0o755)
	_ = os.WriteFile(fanpackPath(), b, 0o644)
	fanpack.zr = zr
	if b := read(zr); b != nil {
		return b, 0
	}
	return nil, 1
}
