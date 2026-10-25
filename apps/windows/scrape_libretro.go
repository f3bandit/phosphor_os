package main

// Box art on the PC: libretro (no account), as phosphord's src_libretro.c. Each system's official list
// (No-Intro, Redump) and libretro's details lists are fetched once and kept in the app's cache (fresh
// again after 30 days). Matched by CRC (cartridges) or product code + size (discs), else the exact
// list name or the same title words. Gives the game its official name; box art: libretro's
// thumbnails, by that name.

import (
	"context"
	"encoding/gob"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

const (
	lrDB     = "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat"
	lrThumbs = "https://thumbnails.libretro.com"
)

type lrMap struct {
	lr   []string
	disc bool
}

var lrSystems = map[string]lrMap{
	"PSX":        {[]string{"Sony - PlayStation"}, true},
	"Saturn":     {[]string{"Sega - Saturn"}, true},
	"MegaCD":     {[]string{"Sega - Mega-CD - Sega CD"}, true},
	"TGFX16CD":   {[]string{"NEC - PC Engine CD - TurboGrafx-CD"}, true},
	"NES":        {[]string{"Nintendo - Nintendo Entertainment System", "Nintendo - Family Computer Disk System"}, false},
	"SNES":       {[]string{"Nintendo - Super Nintendo Entertainment System", "Nintendo - Satellaview"}, false},
	"Genesis":    {[]string{"Sega - Mega Drive - Genesis"}, false},
	"N64":        {[]string{"Nintendo - Nintendo 64"}, false},
	"SMS":        {[]string{"Sega - Master System - Mark III", "Sega - SG-1000"}, false},
	"TGFX16":     {[]string{"NEC - PC Engine - TurboGrafx 16", "NEC - PC Engine SuperGrafx"}, false},
	"ATARI7800":  {[]string{"Atari - 7800", "Atari - 2600"}, false},
	"GAMEBOY":    {[]string{"Nintendo - Game Boy", "Nintendo - Game Boy Color"}, false},
	"GBA":        {[]string{"Nintendo - Game Boy Advance"}, false},
	"GameGear":   {[]string{"Sega - Game Gear"}, false},
	"WonderSwan": {[]string{"Bandai - WonderSwan", "Bandai - WonderSwan Color"}, false},
	"AtariLynx":  {[]string{"Atari - Lynx"}, false},
}

var lrDetailLists = []string{"developer", "publisher", "genre", "releaseyear", "maxusers"}

type LREntry struct {
	Name, CRC, Serial                    string
	Size                                 int64
	Desc, Dev, Pub, Genre, Year, Players string
	LRSys                                string
}

type lrList struct {
	Saved   time.Time
	Entries []LREntry
}

type lrCache struct {
	mu    sync.Mutex
	lists map[string]*lrList
	load  sync.Map // lrsys -> *sync.Mutex: one download at a time per list
}

var libretro = &lrCache{lists: map[string]*lrList{}}

// ---------- the DAT files: clrmamepro format ----------

type datEntry struct {
	name, comment, serial, crc, desc, dev, pub, genre, year, players string
	size                                                             int64
	haveROM                                                          bool
}

type datTok struct {
	s      string
	quoted bool
}

func datTokens(b []byte) []datTok {
	var out []datTok
	i := 0
	for i < len(b) {
		c := b[i]
		if c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f' {
			i++
			continue
		}
		if c == '(' || c == ')' {
			out = append(out, datTok{string(c), false})
			i++
			continue
		}
		if c == '"' {
			i++
			var sb strings.Builder
			for i < len(b) && b[i] != '"' {
				if b[i] == '\\' && i+1 < len(b) {
					i++
				}
				sb.WriteByte(b[i])
				i++
			}
			i++
			out = append(out, datTok{sb.String(), true})
			continue
		}
		j := i
		for j < len(b) && !(b[j] == ' ' || b[j] == '\t' || b[j] == '\n' || b[j] == '\r' || b[j] == '(' || b[j] == ')') {
			j++
		}
		out = append(out, datTok{string(b[i:j]), false})
		i = j
	}
	return out
}

func parseDat(b []byte, cb func(*datEntry)) {
	t := datTokens(b)
	for i := 0; i < len(t); i++ {
		if t[i].quoted || t[i].s != "game" {
			continue
		}
		if i+1 >= len(t) || t[i+1].s != "(" {
			continue
		}
		i += 2
		e := &datEntry{}
		depth, inROM := 1, false
		key := ""
		for ; i < len(t) && depth > 0; i++ {
			tk := t[i]
			if !tk.quoted && tk.s == "(" {
				depth++
				if key == "rom" {
					inROM = !e.haveROM
				}
				key = ""
				continue
			}
			if !tk.quoted && tk.s == ")" {
				depth--
				if inROM && depth == 1 {
					inROM = false
					e.haveROM = true
				}
				continue
			}
			if key == "" {
				key = tk.s
				continue
			}
			v := tk.s
			if depth == 1 {
				switch key {
				case "name":
					e.name = v
				case "comment":
					e.comment = v
				case "serial":
					e.serial = v
				case "description":
					e.desc = v
				case "developer":
					e.dev = v
				case "publisher":
					e.pub = v
				case "genre":
					e.genre = v
				case "releaseyear":
					e.year = v
				case "users", "maxusers":
					e.players = v
				}
			} else if inROM {
				switch key {
				case "crc":
					if len(v) > 8 {
						v = v[:8]
					}
					e.crc = strings.ToLower(v)
				case "size":
					fmt.Sscan(v, &e.size)
				case "serial":
					if e.serial == "" {
						e.serial = v
					}
				}
			}
			key = ""
		}
		i--
		cb(e)
	}
}

// "???" words out: "Nintendo ???" -> "Nintendo"
func lrTidy(s string) string {
	var out []string
	for _, w := range strings.Fields(s) {
		if strings.Trim(w, "?") == "" {
			continue
		}
		out = append(out, w)
	}
	return strings.Join(out, " ")
}

func (c *lrCache) path(lr string) string {
	return filepath.Join(scrapeCacheDir(), "libretro", strings.ReplaceAll(lr, " ", "_")+".gob")
}

// one list, from the cache or fetched; nil and -1 on network trouble
func (c *lrCache) get(ctx context.Context, lr string, disc bool, status func(string)) (*lrList, int) {
	c.mu.Lock()
	if l := c.lists[lr]; l != nil && time.Since(l.Saved) < 30*24*time.Hour {
		c.mu.Unlock()
		return l, 0
	}
	c.mu.Unlock()
	m, _ := c.load.LoadOrStore(lr, &sync.Mutex{})
	lk := m.(*sync.Mutex)
	lk.Lock()
	defer lk.Unlock()
	c.mu.Lock()
	if l := c.lists[lr]; l != nil && time.Since(l.Saved) < 30*24*time.Hour {
		c.mu.Unlock()
		return l, 0
	}
	c.mu.Unlock()
	if f, err := os.Open(c.path(lr)); err == nil {
		var l lrList
		if gob.NewDecoder(f).Decode(&l) == nil && time.Since(l.Saved) < 30*24*time.Hour {
			f.Close()
			c.mu.Lock()
			c.lists[lr] = &l
			c.mu.Unlock()
			return &l, 0
		}
		f.Close()
	}
	status("Getting the " + lr + " game list")
	kind := "no-intro"
	if disc {
		kind = "redump"
	}
	b, rc, _ := netRequest(ctx, "GET", fmt.Sprintf("%s/%s/%s.dat", lrDB, kind, netEscape(lr)), nil, nil, 256<<20)
	if rc < 0 {
		return nil, -1
	}
	l := &lrList{Saved: time.Now()}
	if rc == 0 {
		parseDat(b, func(e *datEntry) {
			if e.name == "" {
				return
			}
			l.Entries = append(l.Entries, LREntry{Name: e.name, CRC: e.crc, Serial: serialNorm(e.serial), Size: e.size, LRSys: lr})
		})
	}
	byCRC, bySer, byName := map[string][]int{}, map[string][]int{}, map[string][]int{}
	for i, e := range l.Entries {
		if e.CRC != "" {
			byCRC[e.CRC] = append(byCRC[e.CRC], i)
		}
		if e.Serial != "" {
			bySer[e.Serial] = append(bySer[e.Serial], i)
		}
		byName[e.Name] = append(byName[e.Name], i)
	}
	for _, d := range lrDetailLists {
		db, rc, _ := netRequest(ctx, "GET", fmt.Sprintf("%s/%s/%s.dat", lrDB, d, netEscape(lr)), nil, nil, 256<<20)
		if rc < 0 {
			return nil, -1
		}
		if rc != 0 {
			continue
		}
		parseDat(db, func(e *datEntry) {
			e.dev, e.pub, e.genre = lrTidy(e.dev), lrTidy(e.pub), lrTidy(e.genre)
			ser := serialNorm(e.serial)
			nm := e.name
			if nm == "" {
				nm = e.comment
			}
			var idx []int
			switch {
			case e.crc != "":
				idx = byCRC[e.crc]
			case ser != "":
				idx = bySer[ser]
			case nm != "":
				idx = byName[nm]
			default:
				return
			}
			for _, i := range idx {
				x := &l.Entries[i]
				set := func(dst *string, v string) {
					if v != "" {
						*dst = v
					}
				}
				set(&x.Desc, e.desc)
				set(&x.Dev, e.dev)
				set(&x.Pub, e.pub)
				set(&x.Genre, e.genre)
				set(&x.Year, e.year)
				set(&x.Players, e.players)
			}
		})
	}
	_ = os.MkdirAll(filepath.Dir(c.path(lr)), 0o755)
	if f, err := os.Create(c.path(lr) + ".new"); err == nil {
		if gob.NewEncoder(f).Encode(l) == nil && f.Close() == nil {
			_ = os.Rename(c.path(lr)+".new", c.path(lr))
		} else {
			f.Close()
		}
	}
	c.mu.Lock()
	c.lists[lr] = l
	c.mu.Unlock()
	return l, 0
}

// no product code or CRC in the lists: the exact list name, or the same title words with the same
// disc number, the file's region first
func lrByName(all []*LREntry, title string) *LREntry {
	for _, e := range all {
		if strings.EqualFold(e.Name, title) {
			return e
		}
	}
	want := nameKey(title)
	if len(want) < 2 {
		return nil
	}
	wreg := strings.ToLower(regionOf(title))
	wdisc := discNo(title)
	best := -1
	var pick *LREntry
	for _, e := range all {
		if nameKey(e.Name) != want {
			continue
		}
		d := discNo(e.Name)
		if d != wdisc && !(wdisc == 0 && d == 1) {
			continue
		}
		reg := strings.ToLower(regionOf(e.Name))
		score := 1
		switch {
		case wreg != "" && reg == wreg:
			score = 4
		case wreg != "" && strings.Contains(reg, wreg):
			score = 3
		case wreg == "" && strings.Contains(reg, "usa"):
			score = 2
		}
		if strings.Contains(e.Name, "(Beta") || strings.Contains(e.Name, "(Proto") || strings.Contains(e.Name, "(Demo") {
			score--
		}
		if score > best {
			best, pick = score, e
		}
	}
	return pick
}

func lrThumbURLs(lrsys, official string) []string {
	var out []string
	for k := 0; k < 3; k++ {
		v := official
		if k == 1 { // "(USA, Canada)" -> "(USA)"
			p := strings.IndexByte(v, '(')
			if p < 0 {
				continue
			}
			c, e := strings.IndexByte(v[p:], ','), strings.IndexByte(v[p:], ')')
			if c < 0 || e < 0 || c > e {
				continue
			}
			v = v[:p+c] + v[p+e:]
		} else if k == 2 { // without " (Disc 1)"
			d := strings.Index(v, " (Disc ")
			if d < 0 {
				continue
			}
			e := strings.IndexByte(v[d+1:], ')')
			if e < 0 {
				continue
			}
			v = v[:d] + v[d+1+e+1:]
		}
		out = append(out, fmt.Sprintf("%s/%s/Named_Boxarts/%s.png", lrThumbs, netEscape(lrsys), netEscape(thumbName(v))))
	}
	return out
}

func lrLookup(ctx context.Context, s *scrapeRun, g *sgame, r *sres) int {
	m, ok := lrSystems[g.Sys]
	if !ok {
		return 0
	}
	var all []*LREntry
	for _, lr := range m.lr {
		l, rc := libretro.get(ctx, lr, m.disc, s.status)
		if rc < 0 {
			return -1
		}
		for i := range l.Entries {
			all = append(all, &l.Entries[i])
		}
	}
	var hit *LREntry
	ser := serialNorm(g.Serial)
	if m.disc && ser != "" {
		for _, e := range all { // same product code, this size first
			if e.Serial == ser && (hit == nil || (e.Size == g.Size && hit.Size != g.Size)) {
				hit = e
			}
		}
		if hit == nil && g.Size > 0 {
			for _, e := range all {
				if strings.Contains(e.Serial, ser) && e.Size == g.Size {
					hit = e
					break
				}
			}
		}
	} else if !m.disc && g.CRC != "" {
		crc := strings.ToLower(g.CRC)
		for _, e := range all {
			if e.CRC == crc {
				hit = e
				break
			}
		}
	}
	if hit == nil {
		hit = lrByName(all, g.Title)
	}
	if hit == nil {
		return 0
	}
	g.Official, g.LRSys = hit.Name, hit.LRSys
	r.set(fDesc, hit.Desc)
	r.set(fDev, hit.Dev)
	r.set(fPub, hit.Pub)
	r.set(fGenre, hit.Genre)
	r.set(fYear, hit.Year)
	r.set(fPlayers, hit.Players)
	r.set(fRegion, regionOf(hit.Name))
	for _, u := range lrThumbURLs(hit.LRSys, hit.Name) {
		r.addArt("libretro", u)
	}
	return 1
}
