// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// The "Local files" source, in Look up on this PC. Two places, both no network:
//
//  1. The SS1's own drives (gamelist.xml, media/ folders, Update All's artwork packs): the SS1 reads them
//     itself (scrape_local: phosphord's src_local.c) and stores what it finds; this just asks, game by game.
//  2. A folder on this PC, if one is set (Settings > Box art > Local files): a Skraper or ES-DE media
//     folder, a LaunchBox Images folder, an art pack someone downloaded. Its pictures are matched to a
//     game by name: the same file name first, else the same title words in a folder whose name says the
//     system ("snes", "Super Nintendo", "Mega Drive"...). Box-front folders (box2d, covers, Box - Front)
//     before 3D boxes; screenshots, title screens, logos, marquees and videos are never used.
//
// The folder is read once a run. Nothing in it is changed.

import (
	"context"
	"io/fs"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"strings"
	"sync"
	"time"
)

// ---------- the SS1's drives ----------

// localLookup: scrape_local on the SS1, then this PC's folder for the art if the SS1 had none
func localLookup(ctx context.Context, s *scrapeRun, g *sgame, r *sres) int {
	found := 0
	var res struct {
		Found  bool              `json:"found"`
		Art    bool              `json:"art"`
		Fields map[string]string `json:"fields"`
	}
	if err := ss1Call(map[string]any{"cmd": "scrape_local", "id": g.ID}, &res); err == nil {
		if res.Found {
			found = 1
		}
		if res.Art {
			g.HasArt = true // stored on the SS1 already: no picture to send
		}
		for i, c := range fieldCols {
			if v := res.Fields[c]; v != "" {
				r.set(i, v)
			}
		}
	} else if !strings.Contains(err.Error(), "unknown") && !strings.Contains(err.Error(), "not allowed") {
		s.note("Local files on the SS1: " + err.Error())
	}
	if !g.HasArt {
		if p := s.pcFolder().find(g); p != "" {
			r.addArt("local", "file:"+p)
			found = 1
		}
	}
	return found
}

// ---------- a folder on this PC ----------

type picEntry struct {
	path, base, key string // the file, its name without the extension (lower case), its title words (amKey)
	rank            int    // 2 box front, 1 3D box or not said, 0 the rest
	segs            string // the folder names on the way, lower case, letters and digits only, "|snes|box2d|"
}

type picIndex struct {
	byBase map[string][]*picEntry
	byKey  map[string][]*picEntry
	n      int
}

var (
	picSkipDir  = regexp.MustCompile(`(?i)^(screenshots?|snaps?|snap|titles?|titlescreens?|title ?screens?|logos?|wheels?|marquees?|fanart|videos?|manuals?|maps?|backgrounds?|bezels?|clear ?logo|banners?|cart|cartridges?|discs?|physicalmedia|mixrbv\d*|miximage)$`)
	picFrontDir = regexp.MustCompile(`(?i)^(box ?2d|boxart|box ?- ?front|box ?front|covers?|front|boxfront|box-front-reconstructed|fronts?)$`)
	picBackDir  = regexp.MustCompile(`(?i)^(box ?3d|3dboxes|box ?- ?3d|backs?|box ?- ?back)$`)
	alnumOnly   = regexp.MustCompile(`[^a-z0-9]+`)
)

const picMax = 400000

func indexPics(root string) *picIndex {
	ix := &picIndex{byBase: map[string][]*picEntry{}, byKey: map[string][]*picEntry{}}
	root = filepath.Clean(root)
	_ = filepath.WalkDir(root, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return nil
		}
		if d.IsDir() {
			if p != root && (picSkipDir.MatchString(d.Name()) || strings.HasPrefix(d.Name(), ".")) {
				return filepath.SkipDir
			}
			if rel, _ := filepath.Rel(root, p); strings.Count(rel, string(os.PathSeparator)) >= 7 {
				return filepath.SkipDir
			}
			return nil
		}
		ext := strings.ToLower(filepath.Ext(d.Name()))
		if ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".gif" && ext != ".bmp" {
			return nil
		}
		if ix.n >= picMax {
			return filepath.SkipAll
		}
		rel, _ := filepath.Rel(root, filepath.Dir(p))
		e := &picEntry{path: p, base: strings.ToLower(strings.TrimSuffix(d.Name(), filepath.Ext(d.Name()))), rank: 1, segs: "|"}
		e.key = amKey(strings.TrimSuffix(d.Name(), filepath.Ext(d.Name())))
		for _, seg := range strings.Split(rel, string(os.PathSeparator)) {
			if seg == "." || seg == "" {
				continue
			}
			if picFrontDir.MatchString(seg) {
				e.rank = 2
			} else if picBackDir.MatchString(seg) {
				e.rank = 1
			}
			e.segs += alnumOnly.ReplaceAllString(strings.ToLower(seg), "") + "|"
		}
		ix.byBase[e.base] = append(ix.byBase[e.base], e)
		if e.key != "" {
			ix.byKey[e.key] = append(ix.byKey[e.key], e)
		}
		ix.n++
		return nil
	})
	return ix
}

// the words folders use for a system (lower case, letters and digits only)
var sysWords = map[string][]string{
	"SNES":       {"snes", "supernintendo", "supernes", "superfamicom", "sfc", "supernintendoentertainmentsystem"},
	"NES":        {"nes", "nintendo", "famicom", "nintendoentertainmentsystem", "fds", "famicomdisksystem"},
	"Genesis":    {"genesis", "megadrive", "md", "segagenesis", "segamegadrive", "segagenesismegadrive"},
	"GAMEBOY":    {"gb", "gameboy", "nintendogameboy", "gbc", "gameboycolor", "nintendogameboycolor"},
	"GBA":        {"gba", "gameboyadvance", "nintendogameboyadvance"},
	"PSX":        {"psx", "ps1", "playstation", "sonyplaystation", "playstation1"},
	"SMS":        {"sms", "mastersystem", "segamastersystem"},
	"GameGear":   {"gg", "gamegear", "segagamegear"},
	"TGFX16":     {"pce", "pcengine", "turbografx16", "tg16", "turbografx", "necturbografx16", "necpcengine"},
	"TGFX16CD":   {"pcecd", "pcenginecd", "turbografxcd", "tg16cd", "necturbografxcd", "turboduo"},
	"MegaCD":     {"segacd", "megacd", "segamegacd"},
	"Saturn":     {"saturn", "segasaturn"},
	"N64":        {"n64", "nintendo64"},
	"Arcade":     {"arcade", "mame", "fbneo", "fba"},
	"ATARI7800":  {"atari7800", "7800", "a7800"},
	"AtariLynx":  {"lynx", "atarilynx"},
	"WonderSwan": {"ws", "wonderswan", "wonderswancolor", "wsc", "bandaiwonderswan"},
	"C64":        {"c64", "commodore64", "commodore"},
	"MSX":        {"msx", "msx1", "msx2"},
	"Atari800":   {"atari800", "atari8bit", "800"},
}

// sysTokens: what a folder of the game's system may be called
func sysTokens(g *sgame) map[string]bool {
	t := map[string]bool{}
	add := func(s string) {
		if s = alnumOnly.ReplaceAllString(strings.ToLower(s), ""); s != "" {
			t[s] = true
		}
	}
	add(g.Sys)
	add(g.System)
	for _, part := range strings.Split(g.System, "/") { // "Game Boy / Color"
		add(part)
	}
	for _, w := range sysWords[g.Sys] {
		t[w] = true
	}
	return t
}

func (e *picEntry) inSystem(tok map[string]bool) bool {
	for _, seg := range strings.Split(strings.Trim(e.segs, "|"), "|") {
		if tok[seg] {
			return true
		}
	}
	return false
}

// best: a front over a 3D box, the game's system's folder over others
func pickPic(list []*picEntry, tok map[string]bool, needSys bool) *picEntry {
	var best *picEntry
	bestScore := -1
	for _, e := range list {
		in := e.inSystem(tok)
		if needSys && !in {
			continue
		}
		sc := e.rank * 2
		if in {
			sc += 10
		}
		if sc > bestScore {
			best, bestScore = e, sc
		}
	}
	return best
}

// find: the picture for a game, "" if none. The same file name (region and all) is trusted anywhere in
// the folder, a system's folder first; the same title words only in a folder named for the system
func (ix *picIndex) find(g *sgame) string {
	if ix == nil || ix.n == 0 {
		return ""
	}
	tok := sysTokens(g)
	if e := pickPic(ix.byBase[strings.ToLower(g.Title)], tok, false); e != nil {
		return e.path
	}
	if g.Official != "" {
		if e := pickPic(ix.byBase[strings.ToLower(g.Official)], tok, false); e != nil {
			return e.path
		}
	}
	if k := amKey(g.Title); k != "" {
		if e := pickPic(ix.byKey[k], tok, true); e != nil {
			return e.path
		}
	}
	return ""
}

// pcFolder: this run's index of the PC's folder (nil when none is set or it isn't there)
func (s *scrapeRun) pcFolder() *picIndex {
	s.picOnce.Do(func() {
		cfgMu.Lock()
		dir := cfg.ArtFolder
		cfgMu.Unlock()
		if dir == "" {
			return
		}
		if st, err := os.Stat(dir); err != nil || !st.IsDir() {
			s.note("Local files: the folder on this PC isn't there: " + dir)
			return
		}
		s.status("Reading the box art folder on this PC")
		s.pics = indexPics(dir)
	})
	return s.pics
}

// ---------- the setting ----------

// GET: the folder and how many pictures are in it; POST {folder} sets it ("" clears), {do: "browse"}
// opens Windows' folder picker and returns the folder chosen
func apiArtFolder(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodPost {
		var req struct{ Folder, Do string }
		if readJSON(r, &req) != nil {
			fail(w, "bad request")
			return
		}
		if req.Do == "browse" {
			p, err := browseFolder()
			if err != nil {
				fail(w, err.Error())
				return
			}
			writeJSON(w, map[string]any{"ok": true, "folder": p})
			return
		}
		f := strings.TrimSpace(strings.Trim(strings.TrimSpace(req.Folder), `"`))
		if f != "" {
			f = filepath.Clean(f)
			if st, err := os.Stat(f); err != nil || !st.IsDir() {
				fail(w, "That folder isn't there")
				return
			}
		}
		cfgMu.Lock()
		cfg.ArtFolder = f
		cfgMu.Unlock()
		if err := saveConfig(); err != nil {
			fail(w, err.Error())
			return
		}
	}
	cfgMu.Lock()
	f := cfg.ArtFolder
	cfgMu.Unlock()
	out := map[string]any{"ok": true, "folder": f, "browse": runtime.GOOS == "windows"}
	if f != "" {
		ix := artFolderCount(f)
		out["pictures"] = ix
	}
	writeJSON(w, out)
}

// how many pictures the folder has (read at most every minute: the page asks when it's shown)
var afCount struct {
	sync.Mutex
	dir string
	n   int
	at  time.Time
}

func artFolderCount(dir string) int {
	afCount.Lock()
	defer afCount.Unlock()
	if afCount.dir == dir && time.Since(afCount.at) < time.Minute {
		return afCount.n
	}
	afCount.dir, afCount.n, afCount.at = dir, indexPics(dir).n, time.Now()
	return afCount.n
}
