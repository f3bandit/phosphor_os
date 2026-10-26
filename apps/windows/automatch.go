// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

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
	amTransRe = regexp.MustCompile(`(?i)\[T[-+]?[A-Za-z]{2,3}\b[^\]]*\]|\((?:[^)]*\b)?(?:translat(?:ed|ion)|english)\b[^)]*\)|\beng(?:lish)? ?(?:dub(?:bed)?|patch(?:ed)?|trans(?:lation)?)\b`)
	amHackRe  = regexp.MustCompile(`(?i)\[h\d*[^\]]*\]|\((?:[^)]*\b)?(?:rom ?)?hack(?:ed)?\b[^)]*\)|\b(?:rom ?hack|kaizo)\b`)
	// words fan versions add to a game's name (SS1 Tool's list, and more seen in real libraries)
	amFanRe = regexp.MustCompile(`(?i)\b(fast ?rom|restoration|restored|complete|deluxe|perfect edition|special edition|special version|` +
		`enhanced( colou?rs)?|enhancement|improvement|undub|purist|redux|remastered|msu-?1|sram|redesign(ed)?|rebalanced|` +
		`major fixes( for)?|black label|sa-?1|arcade colou?rs|6-button( support)?|master patched|patched|un-?worked designs|` +
		`grind-be-gone|v\d+(\.\d+)*[a-z]?)\b`)
	amWordRe = regexp.MustCompile(`(?i)\b(rom ?hack|hack(ed)?|kaizo|translat(ed|ion)|eng(lish)? ?(dub(bed)?|patch(ed)?|trans(lation)?)|` +
		`(nes|famicom|fc|snes)[ -]?(port(ed)?|conversion)|nes ?2 ?snes)\b`)
	// a game ported from another system: "Super Mario Bros. (SNES Port)", "Zelda II NES2SNES"
	amPortRe = regexp.MustCompile(`(?i)\((?:[^)]*\b)?(?:port(?:ed)?|conversion)\b[^)]*\)|\b(?:nes|famicom|fc|snes)[ -]?(?:port(?:ed)?|conversion)\b|\bnes ?2 ?snes\b`)
	amStop   = map[string]bool{"the": true, "of": true, "a": true, "an": true, "and": true}
	// not games at all: boot files, test ROMs, cheat discs, tools
	amNotGameRe = regexp.MustCompile(`(?i)^(mister[- ](boot|demo)|boot ?\d*|empty|bootdisc)$|\b(btn ?test|pad ?test|controller (tester|test)|test ?rom|service disc|game ?shark|action replay|tonyhax|unirom|rom flasher|boot ?disc|trainer)\b`)
	amVerRe     = regexp.MustCompile(`(?i)[\s._-]*v?\d+(\.\d+)+[a-z]?$`)
	amDotRe     = regexp.MustCompile(`([A-Za-z0-9])\.([A-Za-z])`)
	amCamelRe   = regexp.MustCompile(`([a-z])([A-Z])`)
	amDiscRe    = regexp.MustCompile(`(?i)\s*-?\s*\bdisc\s*(\d+)\b`)
	amRoman     = map[string]string{"ii": "2", "iii": "3", "iv": "4", "v": "5", "vi": "6", "vii": "7", "viii": "8", "ix": "9", "x": "10",
		"xi": "11", "xii": "12", "xiii": "13", "xiv": "14", "xv": "15", "xvi": "16"}
)

// amClean: a file-style name made readable: "Zeldas.Adventure.DX.v2.0.0" -> "Zeldas Adventure DX",
// "SuperMarioBrosMini" -> "Super Mario Bros Mini", "Valis_IV_ ENG_DUB" -> "Valis IV ENG DUB"
func amClean(t string) string {
	t = strings.ReplaceAll(t, "_", " ")
	t = amVerRe.ReplaceAllString(t, "")
	t = amDotRe.ReplaceAllString(t, "$1 $2")
	if !strings.Contains(strings.ToLower(t), "(disc") {
		t = amDiscRe.ReplaceAllString(t, " (Disc $1)") // "Star Ocean - The Second Story - Disc 2" -> "... (Disc 2)"
	}
	if !strings.Contains(strings.TrimSpace(t), " ") { // one word: joined by capitals or hyphens
		t = amCamelRe.ReplaceAllString(t, "$1 $2")
		if !strings.Contains(t, " ") {
			t = strings.ReplaceAll(t, "-", " ")
		}
	}
	return strings.Join(strings.Fields(t), " ")
}

// amGlued: a system's short name stuck on the end of a word ("icarusnes" -> "icarus"); each way it
// can be read ("...snes" or "...s" + "nes"), none when there's nothing stuck on
func amGlued(t string) []string {
	ws := strings.Fields(t)
	if len(ws) == 0 {
		return nil
	}
	last := strings.ToLower(ws[len(ws)-1])
	var out []string
	for _, suf := range []string{"snes", "nes", "gbc", "gba", "sfc", "smd", "pce"} {
		if strings.HasSuffix(last, suf) && len(last) >= len(suf)+4 {
			w := append(append([]string{}, ws[:len(ws)-1]...), ws[len(ws)-1][:len(last)-len(suf)])
			out = append(out, strings.Join(w, " "))
		}
	}
	return out
}

// amNotGame: a boot file, test ROM or tool, and the words that said so
func amNotGame(t string) string {
	return amNotGameRe.FindString(strings.TrimSpace(amTagRe.ReplaceAllString(t, " ")))
}

// amKind: what the name says the game is, and the words that said it
func amKind(title string) (kind, tag string) {
	if m := amPortRe.FindString(title); m != "" {
		return "port", strings.TrimSpace(m)
	}
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

// amPortFrom: the system a port came from ("" when it isn't known): the SNES ports are NES games
func amPortFrom(sys, tag string) string {
	t := strings.ToLower(tag)
	switch {
	case sys == "SNES" && !strings.Contains(t, "snes to"):
		return "NES"
	case strings.Contains(t, "nes") && !strings.Contains(t, "snes") && sys != "NES":
		return "NES"
	}
	return ""
}

// amWords: the title's words for comparing, no tags, no "the", "of" ...; Roman numbers as digits
func amWords(t string) string {
	var out []string
	for _, w := range strings.Fields(nameKey(t)) {
		if amStop[w] {
			continue
		}
		if d, ok := amRoman[w]; ok {
			w = d
		}
		out = append(out, w)
	}
	return strings.Join(out, " ")
}

// amKey: amWords without the spaces, so "Sa-Ga" and "SaGa", "Pac Man" and "Pacman" are the same
func amKey(t string) string { return strings.ReplaceAll(amWords(t), " ", "") }

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
			if amKey(cands[i].Title) != key {
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
	base := amStrip(amClean(g.Title))
	if b := pick(amKey(base)); b != nil {
		return b
	}
	if m := amMain(base); m != base {
		return pick(amKey(m))
	}
	return nil
}

// ---------- in the lists: the same game under a longer or shorter name ----------

// amLoose (SS1 Tool's lrLoose, tightened): a game in the lists under the same name written differently.
//
//	exact: the same words ("Legend of Zelda, The", "Sa-Ga", "II" for "2");
//	main:  the same main title, when the list has one game under it ("Rockman 7" -> "Rockman 7 - Shukumei
//	       no Taiketsu!"); more means a series ("Castlevania - ..." is several games);
//	fuzzy: the same main title and a subtitle sharing most of its words with one game only ("Super Saiyan
//	       Densetsu" -> "Super Saiya Densetsu").
//
// The same disc number and the plain release (fewest tags) are preferred.
func amLoose(all []*LREntry, title string) (*LREntry, string) {
	base := amStrip(amClean(title))
	want := amKey(base)
	if want == "" {
		return nil, ""
	}
	main := amMain(base)
	mainK := amKey(main)
	wreg := strings.ToLower(regionOf(title))
	wdisc := discNo(amClean(title))
	// a subtitle's telling words: no numbers, no "super" (every other game has those)
	words := func(t string) []string {
		var out []string
		for _, w := range strings.Fields(amWords(t)) {
			if w != "super" && strings.Trim(w, "0123456789") != "" {
				out = append(out, w)
			}
		}
		return out
	}
	var sub []string // our subtitle's words
	if main != base {
		sub = words(strings.TrimSpace(strings.TrimPrefix(base, main)))
	}
	type sharing struct{ n, theirs int }
	type cand struct {
		e     *LREntry
		score int
	}
	score := func(e *LREntry) int {
		v := amRegionScore(e.Name, wreg) - 8*(len(amTagRe.FindAllString(e.Name, -1))-1)
		if d := discNo(e.Name); d == wdisc || (wdisc == 0 && d <= 1) {
			v += 60
		}
		return v
	}
	var exact []cand
	groups := map[string][]cand{}
	shared := map[string]sharing{}
	for _, e := range all {
		if k, _ := amKind(e.Name); k != "" {
			continue
		}
		nb := amStrip(e.Name)
		nk := amKey(nb)
		if nk == want {
			exact = append(exact, cand{e, score(e)})
			continue
		}
		if len(mainK) < 4 || amKey(amMain(nb)) != mainK {
			continue
		}
		if _, ok := groups[nk]; !ok {
			tw := words(strings.TrimPrefix(nb, amMain(nb)))
			n := 0
			for _, w := range sub {
				for _, x := range tw {
					if amSameWord(w, x) {
						n++
						break
					}
				}
			}
			shared[nk] = sharing{n, len(tw)}
		}
		groups[nk] = append(groups[nk], cand{e, score(e)})
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
		return best(exact), "exact"
	}
	if len(groups) == 1 {
		for k, cs := range groups {
			// both have a subtitle and they share nothing: another game in the series ("Ziria", "Deden no Den")
			if len(sub) > 0 && shared[k].theirs > 0 && shared[k].n == 0 {
				return nil, ""
			}
			return best(cs), "main"
		}
	}
	// several games under the main title: the one whose subtitle shares most of ours, if only one does,
	// and most of its own words are ours too
	if len(sub) >= 2 {
		top, second, pick := 0, 0, ""
		for k, x := range shared {
			if x.n > top {
				second, top, pick = top, x.n, k
			} else if x.n > second {
				second = x.n
			}
		}
		if top >= 2 && top > second && top*3 >= len(sub)*2 && top*2 >= shared[pick].theirs {
			return best(groups[pick]), "fuzzy"
		}
	}
	if len(groups) == 0 {
		// a translated title: no main title in common (#142)
		if gs := amTranslated(all, base); len(gs) > 0 {
			return best(func() []cand {
				var cs []cand
				for _, e := range gs {
					cs = append(cs, cand{e, score(e)})
				}
				return cs
			}()), "translated"
		}
	}
	return nil, ""
}

// ---------- translated titles (#142) ----------

// amParts: a name's words (amWords) in its main title and its subtitle
func amParts(t string) (main, sub []string) {
	m := amMain(t)
	main = strings.Fields(amWords(m))
	if m != t {
		sub = strings.Fields(amWords(strings.TrimSpace(strings.TrimPrefix(t, m))))
	}
	return
}

func amNums(ws []string) string {
	var n []string
	for _, w := range ws {
		if strings.Trim(w, "0123456789") == "" {
			n = append(n, w)
		}
	}
	return strings.Join(n, " ")
}

// amRare: a word that tells games apart: 5 letters or more, not a number, not "super"
func amRare(w string) bool { return len(w) >= 5 && w != "super" && strings.Trim(w, "0123456789") != "" }

// amOneLetter: two words of 6 letters or more with one letter changed, added or left out ("Nankoku" / "Nangoku")
func amOneLetter(a, b string) bool {
	if len(a) < 6 || len(b) < 6 || a == b {
		return false
	}
	if len(a) > len(b) {
		a, b = b, a
	}
	if len(b)-len(a) > 1 {
		return false
	}
	i := 0
	for i < len(a) && a[i] == b[i] {
		i++
	}
	if len(a) == len(b) {
		return a[i+1:] == b[i+1:]
	}
	return a[i:] == b[i+1:]
}

// amTranslated: the list's entries for the one game a translated or re-spelt title is, by the first
// of these that points at exactly one game:
//  1. the same words with one long word spelt one letter differently (Nankoku / Nangoku Shounen Papuwa-kun)
//  2. the same subtitle of two words or more, the numbers matching (Demon Child Zenki / Kishin Douji
//     Zenki - Battle Raiden)
//  3. two telling words in common, one in the main title and one in the subtitle (a main title's word
//     in their main title too), half or more of each name's words, the numbers matching (Mysterious Dungeon 2 - Shiren / Fushigi no Dungeon 2 -
//     Fuurai no Shiren)
func amTranslated(all []*LREntry, base string) []*LREntry {
	om, os := amParts(base)
	ow := append(append([]string{}, om...), os...)
	if len(ow) < 2 {
		return nil
	}
	nums := amNums(ow)
	osK := strings.Join(os, "")
	rule := [3]map[string][]*LREntry{{}, {}, {}}
	for _, e := range all {
		if k, _ := amKind(e.Name); k != "" {
			continue
		}
		nb := amStrip(e.Name)
		tm, ts := amParts(nb)
		tw := append(append([]string{}, tm...), ts...)
		if amNums(tw) != nums {
			continue
		}
		key := strings.Join(tw, " ")
		// 1. one letter
		if len(tw) == len(ow) {
			diff, ok := 0, true
			for i := range ow {
				if ow[i] == tw[i] {
					continue
				}
				if amOneLetter(ow[i], tw[i]) {
					diff++
				} else {
					ok = false
					break
				}
			}
			if ok && diff == 1 {
				rule[0][key] = append(rule[0][key], e)
			}
		}
		// 2. the same subtitle
		if len(os) >= 2 && len(osK) >= 10 && strings.Join(ts, "") == osK {
			rule[1][key] = append(rule[1][key], e)
		}
		// 3. telling words in both parts
		if len(os) > 0 && len(ts) > 0 {
			has := func(ws []string, w string) bool {
				for _, x := range ws {
					if x == w {
						return true
					}
				}
				return false
			}
			inMain, inSub, n, moved := false, false, 0, false
			for _, w := range ow {
				if amRare(w) && has(tw, w) {
					n++
					inMain = inMain || has(om, w)
					inSub = inSub || has(os, w)
					moved = moved || (has(om, w) && !has(tm, w)) // our main title's word only in their subtitle: another game
				}
			}
			if n >= 2 && inMain && inSub && !moved && 2*n >= len(amRareOnly(ow)) && 2*n >= len(amRareOnly(tw)) {
				rule[2][key] = append(rule[2][key], e)
			}
		}
	}
	for _, r := range rule {
		if len(r) == 1 {
			for _, es := range r {
				return es
			}
		}
	}
	return nil
}

func amRareOnly(ws []string) []string {
	var out []string
	for _, w := range ws {
		if amRare(w) {
			out = append(out, w)
		}
	}
	return out
}

// amSameWord: the same word, or one spelt a little shorter ("Saiya" / "Saiyan", "Gokuu" / "Goku")
func amSameWord(a, b string) bool {
	if a == b {
		return true
	}
	if len(a) > len(b) {
		a, b = b, a
	}
	return len(a) >= 4 && len(b)-len(a) <= 2 && strings.HasPrefix(b, a)
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
		if s.matchOne(g, libs) {
			return
		}
	}
}

// matchOne: one game; true when the run has to stop (the SS1 or the network stopped answering)
func (s *scrapeRun) matchOne(g *sgame, libs map[string][]amLibGame) bool {
	clean := amClean(g.Title)
	e := &amEntry{ID: g.ID, Title: g.Title, Sys: g.Sys, System: g.System, At: time.Now().Unix()}
	// 0. not a game at all: out of "not found"
	if w := amNotGame(clean); w != "" {
		if err := ss1Call(map[string]any{"cmd": "not_game", "id": g.ID, "on": true}, nil); err == nil {
			e.Kind, e.Tag, e.How = "notgame", w, "skip"
			s.amAdd(e)
		} else if strings.Contains(err.Error(), "unknown cmd") {
			s.note("Update Phosphor on the SS1 (0.27.33 or later) to set test ROMs and tools aside")
		}
		return false
	}
	kind, tag := amKind(clean)
	e.Kind, e.Tag = kind, tag
	// a port is looked up as the game it was ported from: in that system's library and lists
	look := g.Sys
	from := ""
	if kind == "port" {
		if from = amPortFrom(g.Sys, tag); from != "" {
			look = from
		}
	}
	// 1. the base game is in the library: link it (a port: the original first)
	if kind != "" {
		systems := []string{g.Sys}
		if from != "" {
			systems = []string{from, g.Sys}
		}
		for _, sys := range systems {
			lib, ok := libs[sys]
			if !ok {
				var j struct {
					Games []amLibGame `json:"games"`
				}
				if err := ss1Call(map[string]any{"cmd": "games", "system": sys, "limit": 100000}, &j); err != nil {
					s.status("The SS1 didn't answer: " + err.Error())
					return true
				}
				lib = j.Games
				libs[sys] = lib
			}
			if b := amLibBase(g, lib); b != nil {
				if err := ss1Call(map[string]any{"cmd": "link", "id": g.ID, "base": b.ID, "port": sys != g.Sys}, nil); err == nil {
					e.How, e.BaseID, e.BaseTitle = "library", b.ID, b.Title
					s.amAdd(e)
					return false
				}
			}
		}
	}
	// 2. the box art sources, asked by the base game's name, or by the name made readable
	reg := ""
	if r := regionOf(g.Title); r != "" && !strings.ContainsAny(r, "[]") {
		reg = " (" + r + ")"
	}
	type try struct{ name, kind string }
	var tries []try
	// its own name counts too: the first lookup didn't search the sources by name (only by checksum,
	// disc code and libretro's names)
	seen := map[string]bool{}
	add := func(n, k string) {
		if n = strings.TrimSpace(n); n != "" && !seen[strings.ToLower(n)] {
			seen[strings.ToLower(n)] = true
			tries = append(tries, try{n, k})
		}
	}
	base := amStrip(clean)
	if kind != "" {
		add(base+reg, kind)
		if m := amMain(base); m != base && len(amWords(m)) >= 4 {
			add(m+reg, kind)
		}
	} else {
		add(clean, "spelling") // "SuperMarioBrosMini" -> "Super Mario Bros Mini", "Glory Hunters 1.3" -> "Glory Hunters"
	}
	glued := amGlued(base)
	for _, gl := range glued {
		add(gl+reg, map[bool]string{true: kind, false: "spelling"}[kind != ""])
	}
	// 3. the lists: the same words written differently, or the same main title
	if m, ok := lrSystems[look]; ok {
		var all []*LREntry
		for _, lr := range m.lr {
			l, rc := libretro.get(s.ctx, lr, m.disc, s.status)
			if rc < 0 {
				return true
			}
			for k := range l.Entries {
				all = append(all, &l.Entries[k])
			}
		}
		for _, n := range append([]string{clean}, glued...) {
			if n == "" {
				continue
			}
			if hit, how := amLoose(all, n); hit != nil {
				k := kind
				if k == "" {
					switch {
					case how == "exact":
						k = "spelling" // the same words, written a little differently ("Legend of Zelda, The", "Sa-Ga")
					case how == "main" && amMain(base) != base:
						k = "related" // its own subtitle, and the list's game another (or none): built on it, most likely
					default:
						k = "name" // a longer or shorter name, or a subtitle spelt differently
					}
				}
				add(hit.Name, k)
				break
			}
		}
	}
	for _, t := range tries {
		g2 := *g
		// the name is given as the official one, so every source looks it up (LaunchBox's alternate names
		// too: "Treasure of the Rudras"); libretro replaces it with its own when it knows the game. Every
		// match here is listed for review, so a name lookup is safe.
		g2.Title, g2.CRC, g2.Serial, g2.Official, g2.LRSys = t.name, "", "", t.name, ""
		g2.Sys = look // a port: the original's system
		rc, r := s.one(&g2)
		if rc > 0 && len(r.foundBy) == 0 { // one says 1 for a game that had art already, found or not
			rc = 0
		}
		if rc <= 0 {
			if rc < 0 {
				s.mu.Lock()
				s.st.Later++
				s.mu.Unlock()
				if s.ctx.Err() != nil {
					return true
				}
			}
			continue
		}
		if err := s.report(&g2, rc, r); err != nil {
			s.status("The SS1 didn't answer: " + err.Error())
			return true
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
		e.How, e.Art, e.FoundBy, e.Kind = "lookup", !g.HasArt, r.foundBy, t.kind
		e.BaseTitle = g2.Official
		if e.BaseTitle == "" {
			e.BaseTitle = t.name
		}
		s.amAdd(e)
		return false
	}
	return false
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
			} else if e.How == "skip" {
				err = ss1Call(map[string]any{"cmd": "not_game", "id": e.ID, "on": false}, nil)
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
