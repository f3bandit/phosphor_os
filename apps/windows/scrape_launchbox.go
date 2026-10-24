package main

// Box art on the PC: the LaunchBox Games Database (no account), as phosphord's src_launchbox.c.
// Metadata.zip (~110 MB) is fetched once into the app's cache and read as a stream; what's kept is
// the games of Phosphor's systems (names, alternate names, details, front box file names), saved
// in the cache (fresh again after 30 days). Looked up by the official name a content match gave.

import (
	"archive/zip"
	"context"
	"encoding/gob"
	"encoding/xml"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"time"
)

const (
	lbZipURL = "https://gamesdb.launchbox-app.com/Metadata.zip"
	lbImages = "https://images.launchbox-app.com/"
)

var lbPlatforms = map[string][]string{
	"PSX": {"Sony Playstation"}, "Saturn": {"Sega Saturn"}, "MegaCD": {"Sega CD"}, "TGFX16CD": {"NEC TurboGrafx-CD"},
	"NES": {"Nintendo Entertainment System", "Nintendo Famicom Disk System"}, "SNES": {"Super Nintendo Entertainment System"},
	"Genesis": {"Sega Genesis"}, "N64": {"Nintendo 64"}, "SMS": {"Sega Master System", "Sega SG-1000"},
	"TGFX16": {"NEC TurboGrafx-16"}, "ATARI7800": {"Atari 7800", "Atari 2600"},
	"GAMEBOY": {"Nintendo Game Boy", "Nintendo Game Boy Color"}, "GBA": {"Nintendo Game Boy Advance"},
	"GameGear": {"Sega Game Gear"}, "WonderSwan": {"WonderSwan", "WonderSwan Color"}, "AtariLynx": {"Atari Lynx"},
	"Arcade": {"Arcade"}, "C64": {"Commodore 64"}, "MSX": {"Microsoft MSX", "Microsoft MSX2"}, "Atari800": {"Atari 800"},
}

type LBGame struct {
	ID                                       int64
	Platform, Norm, Name                     string
	Overview, Dev, Pub, Genre, Year, Players string
	Order                                    int
}

type LBImage struct{ Region, File string }

type lbData struct {
	Saved  time.Time
	Games  map[int64]*LBGame
	Alt    map[string][]int64
	Images map[int64][]LBImage
	byNorm map[string][]*LBGame // platform|norm
}

var lb struct {
	mu   sync.Mutex
	load sync.Mutex
	d    *lbData
}

func lbPath() string { return filepath.Join(scrapeCacheDir(), "launchbox.gob") }

func (d *lbData) index() {
	d.byNorm = map[string][]*LBGame{}
	for _, g := range d.Games {
		k := g.Platform + "|" + g.Norm
		d.byNorm[k] = append(d.byNorm[k], g)
	}
	for _, l := range d.byNorm {
		sort.Slice(l, func(a, b int) bool { return l[a].Order < l[b].Order })
	}
}

func lbGet(ctx context.Context, status func(string)) (*lbData, int) {
	lb.mu.Lock()
	if lb.d != nil && time.Since(lb.d.Saved) < 30*24*time.Hour {
		d := lb.d
		lb.mu.Unlock()
		return d, 0
	}
	lb.mu.Unlock()
	lb.load.Lock()
	defer lb.load.Unlock()
	lb.mu.Lock()
	if lb.d != nil && time.Since(lb.d.Saved) < 30*24*time.Hour {
		d := lb.d
		lb.mu.Unlock()
		return d, 0
	}
	lb.mu.Unlock()
	if f, err := os.Open(lbPath()); err == nil {
		var d lbData
		err := gob.NewDecoder(f).Decode(&d)
		f.Close()
		if err == nil && time.Since(d.Saved) < 30*24*time.Hour {
			d.index()
			lb.mu.Lock()
			lb.d = &d
			lb.mu.Unlock()
			return &d, 0
		}
	}
	zipPath := filepath.Join(scrapeCacheDir(), "launchbox-metadata.zip")
	_ = os.MkdirAll(scrapeCacheDir(), 0o755)
	status("Getting the LaunchBox games database (one time, about 110 MB)")
	rc := netGetFile(ctx, lbZipURL, zipPath, func(done, total int64) {
		if total > 0 && done%(8<<20) < 256<<10 {
			status(fmt.Sprintf("Getting the LaunchBox games database: %d of %d MB", done>>20, total>>20))
		}
	})
	if rc < 0 {
		return nil, -1
	}
	if rc != 0 {
		return nil, 1
	}
	status("Reading the LaunchBox games database (one time)")
	d, err := lbImport(zipPath)
	os.Remove(zipPath)
	if err != nil {
		return nil, 1
	}
	if f, err := os.Create(lbPath() + ".new"); err == nil {
		if gob.NewEncoder(f).Encode(d) == nil && f.Close() == nil {
			_ = os.Rename(lbPath()+".new", lbPath())
		} else {
			f.Close()
		}
	}
	d.index()
	lb.mu.Lock()
	lb.d = d
	lb.mu.Unlock()
	return d, 0
}

func lbImport(zipPath string) (*lbData, error) {
	zr, err := zip.OpenReader(zipPath)
	if err != nil {
		return nil, err
	}
	defer zr.Close()
	var xf *zip.File
	for _, f := range zr.File {
		if f.Name == "Metadata.xml" {
			xf = f
		}
	}
	if xf == nil {
		return nil, fmt.Errorf("no Metadata.xml in the zip")
	}
	rd, err := xf.Open()
	if err != nil {
		return nil, err
	}
	defer rd.Close()
	keep := map[string]bool{}
	for _, ps := range lbPlatforms {
		for _, p := range ps {
			keep[p] = true
		}
	}
	d := &lbData{Saved: time.Now(), Games: map[int64]*LBGame{}, Alt: map[string][]int64{}, Images: map[int64][]LBImage{}}
	dec := xml.NewDecoder(rd)
	dec.Strict = false
	type gameX struct {
		Name, Platform, Overview, Developer, Publisher, Genres, MaxPlayers, ReleaseYear, ReleaseDate string
		DatabaseID                                                                                   int64
	}
	type altX struct {
		DatabaseID    int64
		AlternateName string
	}
	type imgX struct {
		DatabaseID             int64
		Type, FileName, Region string
	}
	order := 0
	for {
		tok, err := dec.Token()
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, err
		}
		se, ok := tok.(xml.StartElement)
		if !ok {
			continue
		}
		switch se.Name.Local {
		case "Game":
			var g gameX
			if dec.DecodeElement(&g, &se) != nil || !keep[g.Platform] {
				continue
			}
			year := g.ReleaseYear
			if year == "" && len(g.ReleaseDate) >= 4 {
				year = g.ReleaseDate[:4]
			}
			pl := g.MaxPlayers
			if pl == "0" {
				pl = ""
			}
			order++
			d.Games[g.DatabaseID] = &LBGame{ID: g.DatabaseID, Platform: g.Platform, Norm: titleNorm(g.Name), Name: g.Name,
				Overview: g.Overview, Dev: g.Developer, Pub: g.Publisher, Genre: strings.ReplaceAll(g.Genres, ";", ","),
				Year: year, Players: pl, Order: order}
		case "GameAlternateName":
			var a altX
			if dec.DecodeElement(&a, &se) != nil || d.Games[a.DatabaseID] == nil {
				continue
			}
			if n := titleNorm(a.AlternateName); n != "" {
				d.Alt[n] = append(d.Alt[n], a.DatabaseID)
			}
		case "GameImage":
			var im imgX
			if dec.DecodeElement(&im, &se) != nil || im.Type != "Box - Front" || d.Games[im.DatabaseID] == nil {
				continue
			}
			d.Images[im.DatabaseID] = append(d.Images[im.DatabaseID], LBImage{im.Region, im.FileName})
		}
	}
	return d, nil
}

func lbLookup(ctx context.Context, s *scrapeRun, g *sgame, r *sres) int {
	plats, ok := lbPlatforms[g.Sys]
	if !ok || g.Official == "" {
		return 0
	}
	d, rc := lbGet(ctx, s.status)
	if rc < 0 {
		return -1
	}
	if rc != 0 {
		return 0
	}
	norm := titleNorm(nameBase(g.Official))
	if norm == "" {
		return 0
	}
	// the name itself first, then an alternate name; the first platform first
	var best *LBGame
	rank := func(x *LBGame) int {
		v := 0
		if x.Norm == norm {
			v += 2
		}
		if x.Platform == plats[0] {
			v++
		}
		return v
	}
	consider := func(x *LBGame) {
		if x == nil {
			return
		}
		in := false
		for _, p := range plats {
			if x.Platform == p {
				in = true
			}
		}
		if !in {
			return
		}
		if best == nil || rank(x) > rank(best) || (rank(x) == rank(best) && x.Order < best.Order) {
			best = x
		}
	}
	for _, p := range plats {
		for _, x := range d.byNorm[p+"|"+norm] {
			consider(x)
		}
	}
	for _, id := range d.Alt[norm] {
		consider(d.Games[id])
	}
	if best == nil {
		return 0
	}
	r.set(fDesc, best.Overview)
	r.set(fDev, best.Dev)
	r.set(fPub, best.Pub)
	r.set(fGenre, best.Genre)
	r.set(fYear, best.Year)
	r.set(fPlayers, best.Players)
	want := "North America"
	switch {
	case strings.Contains(g.Official, "(USA") || strings.Contains(g.Official, "USA)"):
	case strings.Contains(g.Official, "Europe"):
		want = "Europe"
	case strings.Contains(g.Official, "Japan"):
		want = "Japan"
	}
	imgs := append([]LBImage(nil), d.Images[best.ID]...)
	score := func(i LBImage) int {
		v := 0
		if i.Region == want {
			v += 4
		}
		if i.Region == "World" {
			v += 2
		}
		if i.Region == "" {
			v++
		}
		return v
	}
	sort.SliceStable(imgs, func(a, b int) bool { return score(imgs[a]) > score(imgs[b]) })
	for i := 0; i < len(imgs) && i < 2; i++ {
		r.addArt("launchbox", lbImages+netEscape(imgs[i].File))
	}
	return 1
}
