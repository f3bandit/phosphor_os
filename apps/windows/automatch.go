package main

// Automatic matching (Library > Match > Matched automatically): the games box art couldn't find,
// matched to the game they're based on by their name. A translation ([T-En]), a hack ((Hack), [h],
// Kaizo) or a fan version (Redux, FastROM, Undub, v1.1 ...) is the base game with something added:
// take that out and what's left is the base game's name.
//   - The base game is in the library: linked to it ("Based on"), so it uses its box art and details.
//   - It isn't: the box art sources are asked for the base game, and its box art and details are used.
//   - No tags, but the lists know the game under a longer or shorter name (Rockman 7 -> Rockman 7 -
//     Shukumei no Taiketsu!): the same, only when the lists leave one game (SS1 Tool 1.28.25's rules).
// Runs at the end of "Look up box art on this PC", or by itself from the Match page. Every match is
// listed for review (Keep, Undo); Undo takes out only what the match put in, and that game isn't
// matched automatically again. Kept in %APPDATA%\Phosphor\automatch.json.

import (
	"encoding/json"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"sync"
	"time"
)

type amEntry struct {
	ID        int64    `json:"id"`
	Title     string   `json:"title"`
	Sys       string   `json:"sys"`
	System    string   `json:"system"`
	Kind      string   `json:"kind"` // translation | hack | fan | name
	Tag       string   `json:"tag"`  // what in the name said so: "[T-En by DeJap]", "(Hack)", "Redux"
	How       string   `json:"how"`  // library | lookup
	BaseID    int64    `json:"base_id,omitempty"`
	BaseTitle string   `json:"base_title"`
	FoundBy   []string `json:"found_by,omitempty"`
	Fields    []string `json:"fields,omitempty"` // lookup: the details it filled in
	Art       bool     `json:"art,omitempty"`    // lookup: the box art came from it
	At        int64    `json:"at"`
	Kept      bool     `json:"kept"`
}

var am struct {
	sync.Mutex
	loaded  bool
	Entries map[int64]*amEntry `json:"entries"`
	Undone  map[int64]bool     `json:"undone"` // not matched automatically again
}

func amPath() string { return filepath.Join(configDir(), "automatch.json") }

func amLoad() {
	if am.loaded {
		return
	}
	am.loaded = true
	am.Entries, am.Undone = map[int64]*amEntry{}, map[int64]bool{}
	if b, err := os.ReadFile(amPath()); err == nil {
		var f struct {
			Entries map[int64]*amEntry `json:"entries"`
			Undone  map[int64]bool     `json:"undone"`
		}
		if json.Unmarshal(b, &f) == nil {
			if f.Entries != nil {
				am.Entries = f.Entries
			}
			if f.Undone != nil {
				am.Undone = f.Undone
			}
		}
	}
}

func amSave() {
	b, _ := json.MarshalIndent(map[string]any{"entries": am.Entries, "undone": am.Undone}, "", " ")
	_ = os.MkdirAll(configDir(), 0o700)
	tmp := amPath() + ".tmp"
	if os.WriteFile(tmp, b, 0o600) == nil {
		_ = os.Rename(tmp, amPath())
	}
}

// ---------- what a name says ----------

var (
	amTagRe   = regexp.MustCompile(`\s*[\(\[][^\)\]]*[\)\]]`)
	amTransRe = regexp.MustCompile(`(?i)\[T[-+]?[A-Za-z]{2,3}\b[^\]]*\]|\((?:[^)]*\b)?(?:translat(?:ed|ion)|english patch(?:ed)?)\b[^)]*\)`)
	amHackRe  = regexp.MustCompile(`(?i)\[h\d*[^\]]*\]|\((?:[^)]*\b)?(?:rom ?)?hack(?:ed)?\b[^)]*\)|\b(?:rom ?hack|kaizo)\b`)
	// words fan versions add to a game's name (SS1 Tool's list)
	amFanRe  = regexp.MustCompile(`(?i)\b(fast ?rom|restoration|restored|complete|deluxe|perfect edition|special edition|enhanced( colou?rs)?|enhancement|improvement|undub|purist|redux|remastered|msu-?1|sram|redesign(ed)?|v\d+(\.\d+)*[a-z]?)\b`)
	amWordRe = regexp.MustCompile(`(?i)\b(rom ?hack|hack(ed)?|kaizo|translat(ed|ion))\b`)
	amStop   = map[string]bool{"the": true, "of": true, "a": true, "an": true, "and": true}
)

// amKind: what the name says the game is, and the words that said it
func amKind(title string) (kind, tag string) {
	if m := amTransRe.FindString(title); m != "" {
		return "translation", strings.TrimSpace(m)
	}
	if m := amHackRe.FindString(title); m != "" {
		return "hack", strings.TrimSpace(m)
	}
	if m := amFanRe.FindString(title); m != "" { // in the name or in brackets: "(Undub)"
		return "fan", m
	}
	return "", ""
}

// amWords: the title's words for comparing, no tags, no "the", "of" ...
func amWords(t string) string {
	var out []string
	for _, w := range strings.Fields(nameKey(t)) {
		if !amStop[w] {
			out = append(out, w)
		}
	}
	return strings.Join(out, " ")
}

// amStrip: the base game's name: tags and the fan version's words out ("" when nothing is left)
func amStrip(t string) string {
	t = amTagRe.ReplaceAllString(t, " ")
	t = strings.ReplaceAll(t, "_", " ")
	t = amFanRe.ReplaceAllString(t, " ")
	t = amWordRe.ReplaceAllString(t, " ")
	t = strings.Join(strings.Fields(t), " ")
	return strings.Trim(t, " -:")
}

// amMain: the main title, before " - " (most subtitles)
func amMain(t string) string {
	if i := strings.Index(t, " - "); i > 0 {
		return t[:i]
	}
	return t
}

// regions in the order a base game is picked by, after the game's own
func amRegionScore(name, want string) int {
	reg := strings.ToLower(regionOf(name))
	score := 0
	if want != "" && reg != "" && (reg == want || strings.Contains(reg, want) || strings.Contains(want, reg)) {
		score = 100
	}
	for i, r := range []string{"usa", "world", "europe", "japan"} {
		if strings.Contains(reg, r) {
			score += 40 - 10*i
			break
		}
	}
	for _, bad := range []string{"beta", "proto", "demo", "sample", "pirate"} {
		if strings.Contains(reg+" "+strings.ToLower(name), bad) {
			score -= 30
		}
	}
	return score
}

// ---------- in the library ----------

type amLibGame struct {
	ID     int64  `json:"id"`
	Title  string `json:"title"`
	Linked bool   `json:"linked"`
	Art    bool   `json:"art"`
}

// amLibBase: the game in the library this one is based on: the same name with the tags out, or (a hack
// or translation with its own subtitle) the main title, when the library has one game under it
func amLibBase(g *sgame, lib []amLibGame) *amLibGame {
	want := strings.ToLower(regionOf(g.Title))
	var cands []amLibGame
	for _, x := range lib {
		if x.ID == g.ID || x.Linked {
			continue
		}
		if k, _ := amKind(x.Title); k != "" { // not another fan version
			continue
		}
		cands = append(cands, x)
	}
	pick := func(key string) *amLibGame {
		if len(key) < 2 {
			return nil
		}
		var best *amLibGame
		bs := -1 << 30
		for i := range cands {
			if amWords(cands[i].Title) != key {
				continue
			}
			s := amRegionScore(cands[i].Title, want)
			if cands[i].Art {
				s += 5
			}
			if s > bs {
				best, bs = &cands[i], s
			}
		}
		return best
	}
	base := amStrip(g.Title)
	if b := pick(amWords(base)); b != nil {
		return b
	}
	if m := amMain(base); m != base {
		return pick(amWords(m))
	}
	return nil
}

// ---------- in the lists: the same game under a longer or shorter name ----------

// amLoose (SS1 Tool's lrLoose): the same words without "the", "of" ...; or the same main title when the
// list has at most two games under it (more means a series: "Castlevania - ..." is several games).
// exact: the same words; otherwise the same main title only (another subtitle, or none)
func amLoose(all []*LREntry, title string) (*LREntry, bool) {
	base := amStrip(title)
	want := amWords(base)
	if want == "" {
		return nil, false
	}
	main := amMain(base)
	mainW := amWords(main)
	wreg := strings.ToLower(regionOf(title))
	type cand struct {
		e     *LREntry
		score int
	}
	var exact []cand
	groups := map[string][]cand{}
	tw := map[string]bool{}
	for _, w := range strings.Fields(want) {
		tw[w] = true
	}
	for _, e := range all {
		if k, _ := amKind(e.Name); k != "" {
			continue
		}
		nb := amStrip(e.Name)
		nw := amWords(nb)
		if nw == want {
			exact = append(exact, cand{e, amRegionScore(e.Name, wreg)})
			continue
		}
		if len(mainW) < 4 || amWords(amMain(nb)) != mainW {
			continue
		}
		shared := 0
		for _, w := range strings.Fields(nw) {
			if tw[w] {
				shared++
			}
		}
		groups[nw] = append(groups[nw], cand{e, shared*100 + amRegionScore(e.Name, wreg)})
	}
	best := func(cs []cand) *LREntry {
		var b *LREntry
		bs := -1 << 30
		for _, c := range cs {
			if c.score > bs {
				b, bs = c.e, c.score
			}
		}
		return b
	}
	if len(exact) > 0 {
		return best(exact), true
	}
	limit := 2
	if main == base {
		limit = 1
	}
	if len(groups) == 0 || len(groups) > limit {
		return nil, false
	}
	var all2 []cand
	for _, cs := range groups {
		all2 = append(all2, cs...)
	}
	return best(all2), false
}

// ---------- the run ----------

// matchPhase: every game nothing was found for, matched by its name
func (s *scrapeRun) matchPhase() {
	am.Lock()
	amLoad()
	undone := map[int64]bool{}
	for k, v := range am.Undone {
		undone[k] = v
	}
	am.Unlock()
	s.status("Matching to base games")
	var games []sgame
	for off := 0; s.ctx.Err() == nil; off += 1000 {
		var todo struct {
			Miss  bool    `json:"miss"`
			Games []sgame `json:"games"`
		}
		if err := ss1Call(map[string]any{"cmd": "scrape_todo", "miss": true, "limit": 1000, "offset": off}, &todo); err != nil {
			s.status("The SS1 didn't answer: " + err.Error())
			return
		}
		if !todo.Miss {
			s.status("Update Phosphor on the SS1 (0.27.32 or later) for matching to base games")
			return
		}
		games = append(games, todo.Games...)
		if len(todo.Games) < 1000 {
			break
		}
	}
	libs := map[string][]amLibGame{}
	for i := range games {
		g := &games[i]
		if s.ctx.Err() != nil {
			return
		}
		if undone[g.ID] || g.Arcade {
			continue
		}
		s.mu.Lock()
		s.st.Game, s.st.System = g.Title, g.System
		s.mu.Unlock()
		kind, tag := amKind(g.Title)
		e := &amEntry{ID: g.ID, Title: g.Title, Sys: g.Sys, System: g.System, Kind: kind, Tag: tag, At: time.Now().Unix()}
		// 1. the base game is in the library: link it
		if kind != "" {
			lib, ok := libs[g.Sys]
			if !ok {
				var j struct {
					Games []amLibGame `json:"games"`
				}
				if err := ss1Call(map[string]any{"cmd": "games", "system": g.Sys, "limit": 100000}, &j); err != nil {
					s.status("The SS1 didn't answer: " + err.Error())
					return
				}
				lib = j.Games
				libs[g.Sys] = lib
			}
			if b := amLibBase(g, lib); b != nil {
				if err := ss1Call(map[string]any{"cmd": "link", "id": g.ID, "base": b.ID}, nil); err == nil {
					e.How, e.BaseID, e.BaseTitle = "library", b.ID, b.Title
					s.amAdd(e)
					continue
				}
			}
		}
		// 2. the box art sources, asked for the base game
		var names []string
		if kind != "" {
			base := amStrip(g.Title)
			reg := ""
			if r := regionOf(g.Title); r != "" && !strings.ContainsAny(r, "[]") {
				reg = " (" + r + ")"
			}
			if base != "" {
				names = append(names, base+reg)
				if m := amMain(base); m != base && len(amWords(m)) >= 4 {
					names = append(names, m+reg)
				}
			}
		} else if m, ok := lrSystems[g.Sys]; ok {
			var all []*LREntry
			for _, lr := range m.lr {
				l, rc := libretro.get(s.ctx, lr, m.disc, s.status)
				if rc < 0 {
					return
				}
				for k := range l.Entries {
					all = append(all, &l.Entries[k])
				}
			}
			if hit, exact := amLoose(all, g.Title); hit != nil {
				names = append(names, hit.Name)
				e.Kind, e.Tag = "name", ""
				if exact { // the same words, written a little differently ("Legend of Zelda, The")
					e.Kind = "spelling"
				} else if b := amStrip(g.Title); amMain(b) != b {
					e.Kind = "related" // its own subtitle, and the list's game another (or none): built on it, most likely
				}
			}
		}
		for _, n := range names {
			g2 := *g
			g2.Title, g2.CRC, g2.Serial, g2.Official, g2.LRSys = n, "", "", "", ""
			rc, r := s.one(&g2)
			if rc <= 0 {
				if rc < 0 {
					s.mu.Lock()
					s.st.Later++
					s.mu.Unlock()
				}
				continue
			}
			if err := s.report(&g2, rc, r); err != nil {
				s.status("The SS1 didn't answer: " + err.Error())
				return
			}
			has := map[string]bool{}
			for _, h := range g.Has {
				has[h] = true
			}
			for i := 0; i < fCount; i++ {
				if r.f[i] != nil && *r.f[i] != "" && !has[fieldCols[i]] {
					e.Fields = append(e.Fields, fieldCols[i])
				}
			}
			e.How, e.Art, e.FoundBy = "lookup", !g.HasArt, r.foundBy
			e.BaseTitle = g2.Official
			if e.BaseTitle == "" {
				e.BaseTitle = n
			}
			if e.Kind == "" {
				e.Kind = "name"
			}
			s.amAdd(e)
			break
		}
	}
}

func (s *scrapeRun) amAdd(e *amEntry) {
	am.Lock()
	amLoad()
	am.Entries[e.ID] = e
	amSave()
	am.Unlock()
	s.mu.Lock()
	s.st.Matched++
	s.mu.Unlock()
}

// ---------- the app's API ----------

// GET: the matches to review; POST {do: run | keep | keep_all | undo | forget, id}
func apiAutomatch(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodPost {
		var req struct {
			Do string `json:"do"`
			ID int64  `json:"id"`
		}
		if readJSON(r, &req) != nil {
			fail(w, "bad request")
			return
		}
		switch req.Do {
		case "run":
			if !scrapeStart(true) {
				fail(w, "Box art is being looked up: matching runs at the end of it")
				return
			}
		case "keep", "keep_all", "forget":
			am.Lock()
			amLoad()
			for id, e := range am.Entries {
				if req.Do == "keep_all" || id == req.ID {
					if req.Do == "forget" {
						delete(am.Entries, id) // the user picked another base game
					} else {
						e.Kept = true
					}
				}
			}
			amSave()
			am.Unlock()
		case "undo":
			am.Lock()
			amLoad()
			e := am.Entries[req.ID]
			am.Unlock()
			if e == nil {
				fail(w, "no such match")
				return
			}
			var err error
			if e.How == "library" {
				err = ss1Call(map[string]any{"cmd": "unlink", "id": e.ID}, nil)
			} else {
				err = ss1Call(map[string]any{"cmd": "scrape_undo", "id": e.ID, "fields": e.Fields, "art": e.Art}, nil)
			}
			if err != nil {
				fail(w, err.Error())
				return
			}
			am.Lock()
			delete(am.Entries, e.ID)
			am.Undone[e.ID] = true
			amSave()
			am.Unlock()
		default:
			fail(w, "do: run, keep, keep_all, undo or forget")
			return
		}
	}
	am.Lock()
	amLoad()
	list := make([]*amEntry, 0, len(am.Entries))
	for _, e := range am.Entries {
		list = append(list, e)
	}
	undone := len(am.Undone)
	am.Unlock()
	sort.Slice(list, func(i, j int) bool {
		if list[i].Kept != list[j].Kept {
			return !list[i].Kept
		}
		if list[i].Sys != list[j].Sys {
			return list[i].Sys < list[j].Sys
		}
		return strings.ToLower(list[i].Title) < strings.ToLower(list[j].Title)
	})
	writeJSON(w, map[string]any{"ok": true, "matches": list, "undone": undone})
}
