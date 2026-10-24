package main

// Box art looked up on the PC (Settings > Box art > Look up on: This PC). Much faster than the SS1:
// the PC fetches the lists and pictures, matches with the same rules as phosphord's scraper.c, makes
// the box art and sends the finished picture (PUT /art) and the details (scrape_result) to the SS1.
// The SS1's own lookup waits meanwhile, and the TV says who's doing it. Several games at once;
// services that ask for a pace (ArcadeDB, IGDB, ScreenScraper) get it.

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

const (
	fDesc = iota
	fDev
	fPub
	fGenre
	fYear
	fPlayers
	fRegion
	fCount
)

var fieldCols = [fCount]string{"description", "developer", "publisher", "genre", "year", "players", "region"}

type sgame struct {
	ID       int64    `json:"id"`
	Sys      string   `json:"sys"`
	System   string   `json:"system"`
	Key      string   `json:"key"`
	Title    string   `json:"title"`
	CRC      string   `json:"crc"`
	Serial   string   `json:"serial"`
	Size     int64    `json:"size"`
	Disc     bool     `json:"disc"`
	Arcade   bool     `json:"arcade"`
	HasArt   bool     `json:"has_art"`
	Has      []string `json:"has"`
	Official string   `json:"-"`
	LRSys    string   `json:"-"`
}

type artURL struct{ src, url string }

type sres struct {
	f       [fCount]*string
	art     []artURL
	foundBy []string
}

func (r *sres) set(i int, v string) {
	if r.f[i] != nil {
		return
	}
	v = trimField(v)
	if v == "" {
		return
	}
	r.f[i] = &v
}

func (r *sres) addArt(src, u string) {
	if u == "" || len(r.art) >= 8 {
		return
	}
	r.art = append(r.art, artURL{src, u})
}

func (r *sres) complete(hasArt bool) bool {
	for i := 0; i < fCount; i++ {
		if r.f[i] == nil && i != fRegion {
			return false
		}
	}
	return hasArt || len(r.art) > 0
}

type scrapeSource struct {
	id, name string
	ready    func(s *scrapeRun) bool
	lookup   func(ctx context.Context, s *scrapeRun, g *sgame, r *sres) int
}

var scrapeSources = []scrapeSource{
	{"libretro", "libretro", func(*scrapeRun) bool { return true }, lrLookup},
	{"arcadedb", "ArcadeDB", func(*scrapeRun) bool { return true }, adbLookup},
	{"launchbox", "LaunchBox", func(*scrapeRun) bool { return true }, lbLookup},
	{"thegamesdb", "TheGamesDB", func(s *scrapeRun) bool { return s.acct("thegamesdb", "apikey") != "" }, tgdbLookup},
	{"screenscraper", "ScreenScraper", func(s *scrapeRun) bool {
		return s.acct("screenscraper", "devid") != "" && s.acct("screenscraper", "devpassword") != ""
	}, ssLookup},
	{"igdb", "IGDB", func(s *scrapeRun) bool {
		return s.acct("igdb", "client_id") != "" && s.acct("igdb", "client_secret") != ""
	}, igdbLookup},
}

func scrapeCacheDir() string {
	d, err := os.UserCacheDir()
	if err != nil {
		d = configDir()
	}
	return filepath.Join(d, "Phosphor", "boxart")
}

// ---------- one run ----------

type scrapeRun struct {
	ctx      context.Context
	cancel   context.CancelFunc
	accounts map[string]map[string]string
	order    []*scrapeSource
	mu       sync.Mutex
	st       scrapeState
}

type scrapeState struct {
	Running  bool     `json:"running"`
	Status   string   `json:"status"`
	Total    int      `json:"total"`
	Done     int      `json:"done"`
	Found    int      `json:"found"`
	NotFound int      `json:"not_found"`
	Later    int      `json:"later"`
	Game     string   `json:"game"`
	System   string   `json:"system"`
	Notes    []string `json:"notes"`
	Started  int64    `json:"started"`
	Ended    int64    `json:"ended"`
}

var (
	scrapeMu   sync.Mutex
	scrapeCur  *scrapeRun
	scrapeLast scrapeState
)

func (s *scrapeRun) acct(sec, key string) string { return s.accounts[sec][key] }

func (s *scrapeRun) status(t string) {
	s.mu.Lock()
	s.st.Status = t
	s.mu.Unlock()
}

func (s *scrapeRun) note(t string) {
	s.mu.Lock()
	for _, n := range s.st.Notes {
		if n == t {
			s.mu.Unlock()
			return
		}
	}
	s.st.Notes = append(s.st.Notes, t)
	s.mu.Unlock()
}

func ss1Call(v any, out any) error {
	b, _ := json.Marshal(v)
	raw, code, err := call(b)
	if err != nil {
		return err
	}
	if code != 200 && len(raw) == 0 {
		return fmt.Errorf("the SS1 answered %d", code)
	}
	var head struct {
		OK    bool   `json:"ok"`
		Error string `json:"error"`
	}
	_ = json.Unmarshal(raw, &head)
	if !head.OK {
		if head.Error == "" {
			head.Error = fmt.Sprintf("the SS1 answered %d", code)
		}
		return errors.New(head.Error)
	}
	if out != nil {
		return json.Unmarshal(raw, out)
	}
	return nil
}

func putArt(id int64, jpg []byte) error {
	r, err := authed("PUT", fmt.Sprintf("/art/%d", id), bytes.NewReader(jpg), 2*time.Minute)
	if err != nil {
		return err
	}
	defer r.Body.Close()
	b, _ := io.ReadAll(io.LimitReader(r.Body, 1<<20))
	var head struct {
		OK    bool   `json:"ok"`
		Error string `json:"error"`
	}
	_ = json.Unmarshal(b, &head)
	if r.StatusCode != 200 || !head.OK {
		if head.Error == "" {
			head.Error = fmt.Sprintf("HTTP %d", r.StatusCode)
		}
		return errors.New(head.Error)
	}
	return nil
}

// the first picture that downloads and decodes, made into box art and sent; 1 sent, 0 none, -1 trouble
func (s *scrapeRun) fetchArt(g *sgame, r *sres, from int) int {
	trouble := false
	for i := from; i < len(r.art); i++ {
		b, rc, _ := netRequest(s.ctx, "GET", r.art[i].url, nil, nil, 32<<20)
		if rc < 0 {
			trouble = true
			continue
		}
		if rc != 0 {
			continue
		}
		jpg, err := makeBoxArt(b)
		if err != nil {
			continue
		}
		if err := putArt(g.ID, jpg); err != nil {
			s.note("Couldn't send box art to the SS1: " + err.Error())
			trouble = true
			continue
		}
		return 1
	}
	if trouble {
		return -1
	}
	return 0
}

// as scrape_one in phosphord: 1 found (with art, or it had some), 0 nobody has it, -1 trouble
func (s *scrapeRun) one(g *sgame) (int, *sres) {
	r := &sres{}
	empty := ""
	for _, h := range g.Has {
		for i, c := range fieldCols {
			if c == h {
				r.f[i] = &empty
			}
		}
	}
	art := g.HasArt
	found, trouble, tried := false, false, 0
	for _, src := range s.order {
		if s.ctx.Err() != nil {
			return -1, r
		}
		if !src.ready(s) {
			continue
		}
		if r.complete(art) {
			if art {
				break
			}
			a := s.fetchArt(g, r, tried)
			tried = len(r.art)
			if a > 0 {
				art = true
				break
			}
			if a < 0 {
				trouble = true
			}
		}
		switch src.lookup(s.ctx, s, g, r) {
		case 1:
			found = true
			r.foundBy = append(r.foundBy, src.name)
		case -1:
			trouble = true
		}
	}
	if found && !art {
		switch s.fetchArt(g, r, tried) {
		case 1:
			art = true
		case -1:
			trouble = true
		}
	}
	if !found {
		// nothing filled in: the SS1 still tries Disc 1's art
		r = &sres{}
	}
	if art {
		return 1, r
	}
	if trouble {
		return -1, r
	}
	return 0, r
}

func (s *scrapeRun) report(g *sgame, rc int, r *sres) error {
	fields := map[string]string{}
	for i := 0; i < fCount; i++ {
		if r.f[i] != nil && *r.f[i] != "" {
			fields[fieldCols[i]] = *r.f[i]
		}
	}
	return ss1Call(map[string]any{"cmd": "scrape_result", "id": g.ID, "found": len(r.foundBy) > 0, "later": rc < 0,
		"official": g.Official, "fields": fields, "system": g.System}, nil)
}

func (s *scrapeRun) run() {
	defer func() {
		s.mu.Lock()
		s.st.Running = false
		s.st.Ended = time.Now().Unix()
		st := s.st
		s.mu.Unlock()
		msg := "Done"
		switch {
		case s.ctx.Err() != nil:
			msg = "Stopped"
		case strings.HasPrefix(st.Status, "No connection"), strings.HasPrefix(st.Status, "The SS1"):
			msg = st.Status
		}
		s.status(msg)
		_ = ss1Call(map[string]any{"cmd": "scrape_pc_done", "status": fmt.Sprintf("%s (looked up on the PC: %d found, %d not found)", msg, st.Found, st.NotFound)}, nil)
		scrapeMu.Lock()
		s.mu.Lock()
		scrapeLast = s.st
		s.mu.Unlock()
		scrapeCur = nil
		scrapeMu.Unlock()
	}()
	first := true
	var fails int32
	seen := map[int64]bool{}
	for s.ctx.Err() == nil {
		var todo struct {
			Total  int     `json:"total"`
			Games  []sgame `json:"games"`
			Config struct {
				Order    []string                     `json:"order"`
				Accounts map[string]map[string]string `json:"accounts"`
			} `json:"config"`
		}
		if err := ss1Call(map[string]any{"cmd": "scrape_todo", "start": first, "limit": 400}, &todo); err != nil {
			s.status("The SS1 didn't answer: " + err.Error())
			return
		}
		if first {
			s.accounts = todo.Config.Accounts
			for _, id := range todo.Config.Order {
				for i := range scrapeSources {
					if scrapeSources[i].id == id {
						s.order = append(s.order, &scrapeSources[i])
					}
				}
			}
			s.mu.Lock()
			s.st.Total = todo.Total
			s.mu.Unlock()
			first = false
		}
		var games []sgame
		for _, g := range todo.Games {
			if !seen[g.ID] {
				seen[g.ID] = true
				games = append(games, g)
			}
		}
		if len(games) == 0 {
			return
		}
		// Disc 2 and later after Disc 1: they borrow its art
		sort.SliceStable(games, func(a, b int) bool { return discNo(games[a].Title) <= 1 && discNo(games[b].Title) > 1 })
		s.status("Looking up box art")
		jobs := make(chan *sgame)
		var wg sync.WaitGroup
		stopped := atomic.Bool{}
		for w := 0; w < 6; w++ {
			wg.Add(1)
			go func() {
				defer wg.Done()
				for g := range jobs {
					s.mu.Lock()
					s.st.Game, s.st.System = g.Title, g.System
					s.mu.Unlock()
					rc, r := s.one(g)
					if s.ctx.Err() != nil {
						continue
					}
					if err := s.report(g, rc, r); err != nil {
						s.status("The SS1 didn't answer: " + err.Error())
						stopped.Store(true)
						continue
					}
					s.mu.Lock()
					s.st.Done++
					switch {
					case rc < 0:
						s.st.Later++
					case rc > 0:
						s.st.Found++
					default:
						s.st.NotFound++
					}
					s.mu.Unlock()
					if rc < 0 {
						if atomic.AddInt32(&fails, 1) >= 12 {
							s.status("No connection: stopped (Look up box art to try again)")
							stopped.Store(true)
						}
					} else {
						atomic.StoreInt32(&fails, 0)
					}
				}
			}()
		}
		for i := range games {
			if s.ctx.Err() != nil || stopped.Load() {
				break
			}
			jobs <- &games[i]
		}
		close(jobs)
		wg.Wait()
		if stopped.Load() {
			return
		}
	}
}

// ---------- the app's API ----------

// GET: the state; POST {do: "start"|"stop"}
func apiScrape(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodPost {
		var req struct{ Do string }
		_ = readJSON(r, &req)
		scrapeMu.Lock()
		switch req.Do {
		case "start":
			if scrapeCur == nil {
				ctx, cancel := context.WithCancel(context.Background())
				s := &scrapeRun{ctx: ctx, cancel: cancel}
				s.st = scrapeState{Running: true, Status: "Starting", Started: time.Now().Unix()}
				scrapeCur = s
				go s.run()
			}
		case "stop":
			if scrapeCur != nil {
				scrapeCur.cancel()
			}
		}
		scrapeMu.Unlock()
	}
	scrapeMu.Lock()
	st := scrapeLast
	if scrapeCur != nil {
		scrapeCur.mu.Lock()
		st = scrapeCur.st
		scrapeCur.mu.Unlock()
	}
	scrapeMu.Unlock()
	writeJSON(w, map[string]any{"ok": true, "state": st})
}
