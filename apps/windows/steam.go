package main

// Steam soundtracks to the SS1 (Music › Steam soundtracks): the soundtracks Steam has installed on this PC,
// found from Steam's own records (libraryfolders.vdf for every Steam library, appmanifest_*.acf for each
// app), sent to the SS1 over the pairing (PUT /music/<album>/<file>, into <drive>/Music/Steam/<album>).
// Phosphor doesn't sign in to Steam or change anything there; Steam itself downloads soundtracks
// (steamapps/music, or a "Soundtrack" folder of a game in steamapps/common for older ones).

import (
	"encoding/json"
	"fmt"
	"image"
	_ "image/jpeg"
	_ "image/png"
	"io"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"sync"
	"time"
)

// ---- Steam's KeyValues text (.vdf, .acf): "key" "value" and "key" { ... }
func vdfParse(s string) map[string]any {
	toks := []string{}
	for i := 0; i < len(s); i++ {
		c := s[i]
		switch {
		case c == '{' || c == '}':
			toks = append(toks, string(c))
		case c == '"':
			j := i + 1
			var b strings.Builder
			for ; j < len(s) && s[j] != '"'; j++ {
				if s[j] == '\\' && j+1 < len(s) {
					j++
				}
				b.WriteByte(s[j])
			}
			toks = append(toks, "\x00"+b.String())
			i = j
		case c == '/' && i+1 < len(s) && s[i+1] == '/':
			for i < len(s) && s[i] != '\n' {
				i++
			}
		}
	}
	var parse func(k *int) map[string]any
	parse = func(k *int) map[string]any {
		m := map[string]any{}
		for *k < len(toks) {
			t := toks[*k]
			if t == "}" {
				*k++
				return m
			}
			if !strings.HasPrefix(t, "\x00") {
				*k++
				continue
			}
			key := strings.ToLower(t[1:])
			*k++
			if *k >= len(toks) {
				break
			}
			if toks[*k] == "{" {
				*k++
				m[key] = parse(k)
			} else {
				m[key] = toks[*k][1:]
				*k++
			}
		}
		return m
	}
	k := 0
	return parse(&k)
}

func vdfFile(p string) map[string]any {
	b, err := os.ReadFile(p)
	if err != nil {
		return nil
	}
	return vdfParse(string(b))
}

func vdfStr(m map[string]any, keys ...string) string {
	var cur any = m
	for _, k := range keys {
		mm, ok := cur.(map[string]any)
		if !ok {
			return ""
		}
		cur = mm[strings.ToLower(k)]
	}
	s, _ := cur.(string)
	return s
}

// every Steam library folder on this PC
func steamLibraries() []string {
	root := steamPath()
	if root == "" {
		return nil
	}
	libs := []string{root}
	if lf := vdfFile(filepath.Join(root, "steamapps", "libraryfolders.vdf")); lf != nil {
		if top, ok := lf["libraryfolders"].(map[string]any); ok {
			for _, v := range top {
				if e, ok := v.(map[string]any); ok {
					if p, _ := e["path"].(string); p != "" {
						libs = append(libs, filepath.Clean(p))
					}
				}
			}
		}
	}
	seen := map[string]bool{}
	out := []string{}
	for _, l := range libs {
		k := strings.ToLower(l)
		if !seen[k] {
			seen[k] = true
			out = append(out, l)
		}
	}
	return out
}

type stTrack struct {
	Path string `json:"-"`
	Name string `json:"name"`
	Size int64  `json:"size"`
}
type soundtrack struct {
	AppID    string    `json:"appid"`
	Title    string    `json:"title"`  // English when Steam has it
	Artist   string    `json:"artist"` // the album's artist, from Steam's record
	Format   string    `json:"format"` // FLAC, MP3, ...
	Tracks   int       `json:"tracks"`
	Size     int64     `json:"size"`
	Cover    string    `json:"-"`
	HasArt   bool      `json:"cover"`
	Embedded bool      `json:"-"` // the art is inside the tracks: the SS1 takes it from there
	OnSS1    bool      `json:"on_ss1"`
	files    []stTrack // the tracks (one format) and the cover
	dir      string
	name     string // Steam's plain name (can be in the original language): how older sends were titled
	info     albumInfo
}

var audioExt = map[string]bool{".flac": true, ".mp3": true, ".ogg": true, ".wav": true}

// the tracks under dir: FLAC when Steam has the lossless version, else the rest
func soundtrackFiles(dir string) (tracks []stTrack, format, cover string) {
	byExt := map[string][]stTrack{}
	var imgs []img
	filepath.WalkDir(dir, func(p string, d os.DirEntry, err error) error {
		if err != nil || d.IsDir() {
			return nil
		}
		ext := strings.ToLower(filepath.Ext(p))
		if audioExt[ext] {
			if fi, e := d.Info(); e == nil {
				byExt[ext] = append(byExt[ext], stTrack{Path: p, Name: filepath.Base(p), Size: fi.Size()})
			}
		} else if ext == ".jpg" || ext == ".jpeg" || ext == ".png" {
			if fi, e := d.Info(); e == nil {
				imgs = append(imgs, img{p, fi.Size()})
			}
		}
		return nil
	})
	cover = pickCover(imgs)
	for _, e := range []string{".flac", ".mp3", ".ogg", ".wav"} {
		if len(byExt[e]) > 0 {
			tracks = byExt[e]
			format = strings.ToUpper(e[1:])
			break
		}
	}
	sort.Slice(tracks, func(i, j int) bool { return strings.ToLower(tracks[i].Path) < strings.ToLower(tracks[j].Path) })
	return
}

type img struct {
	path string
	size int64
}

var coverName = regexp.MustCompile(`(?i)cover|folder|front|album|jacket|artwork`)

// the album's picture: one named like a cover, else the biggest (a booklet page is usually smaller)
func pickCover(imgs []img) string {
	best, bestNamed := img{}, false
	for _, m := range imgs {
		named := coverName.MatchString(filepath.Base(m.path))
		if best.path == "" || (named && !bestNamed) || (named == bestNamed && m.size > best.size) {
			best, bestNamed = m, named
		}
	}
	return best.path
}

// art inside a track: a FLAC picture block or an MP3's ID3 picture (the SS1 makes the cover from it)
func hasEmbeddedArt(path string) bool {
	f, err := os.Open(path)
	if err != nil {
		return false
	}
	defer f.Close()
	b := make([]byte, 1<<20)
	n, _ := io.ReadFull(f, b)
	b = b[:n]
	if len(b) >= 4 && string(b[:4]) == "fLaC" {
		for o, last := 4, false; !last && o+4 <= len(b); {
			last = b[o]&0x80 != 0
			if b[o]&0x7f == 6 {
				return true
			}
			o += 4 + (int(b[o+1])<<16 | int(b[o+2])<<8 | int(b[o+3]))
		}
		return false
	}
	return len(b) >= 10 && string(b[:3]) == "ID3" && strings.Contains(string(b), "APIC")
}

// Steam's own picture of the soundtrack (what its library shows), kept by Steam on this PC; newer Steam
// keeps them in a folder per app, older Steam beside each other. Only Steam's own files: nothing online.
func steamArt(appid string) string {
	sp := steamPath()
	if sp == "" || appid == "" {
		return ""
	}
	lc := filepath.Join(sp, "appcache", "librarycache")
	// the soundtrack's album cover: Steam keeps it square (1000 x 1000) under a hashed name in the
	// app's folder, beside header.jpg (a 460 x 215 banner) and a small icon
	if sq := squareArt(filepath.Join(lc, appid)); sq != "" {
		return sq
	}
	prefs := []string{"library_600x900", "library_capsule", "header", "capsule"}
	for _, p := range prefs {
		for _, f := range []string{appid + "_" + p + ".jpg", filepath.Join(appid, p+".jpg")} {
			if fi, err := os.Stat(filepath.Join(lc, f)); err == nil && fi.Size() > 0 {
				return filepath.Join(lc, f)
			}
		}
	}
	found := map[string]string{}
	filepath.WalkDir(filepath.Join(lc, appid), func(p string, d os.DirEntry, err error) error {
		if err == nil && !d.IsDir() {
			n := strings.ToLower(d.Name())
			for _, x := range prefs {
				if strings.HasPrefix(n, x) && found[x] == "" {
					found[x] = p
				}
			}
		}
		return nil
	})
	for _, x := range prefs {
		if found[x] != "" {
			return found[x]
		}
	}
	return ""
}

// the biggest square picture (at least 300 px) in dir and below
func squareArt(dir string) string {
	best, bestW := "", 0
	filepath.WalkDir(dir, func(p string, d os.DirEntry, err error) error {
		if err != nil || d.IsDir() {
			return nil
		}
		ext := strings.ToLower(filepath.Ext(p))
		if ext != ".jpg" && ext != ".jpeg" && ext != ".png" {
			return nil
		}
		f, e := os.Open(p)
		if e != nil {
			return nil
		}
		c, _, e := image.DecodeConfig(f)
		f.Close()
		if e != nil || c.Width < 300 || c.Width <= bestW {
			return nil
		}
		if diff := c.Width - c.Height; diff*20 > c.Width || -diff*20 > c.Width { // more than 5% off square
			return nil
		}
		best, bestW = p, c.Width
		return nil
	})
	return best
}

var artMu sync.Mutex
var artCache = map[string]string{}
var artTried = map[string]time.Time{}

// steamArt, remembered: the list is asked for every second or two while sending
func steamArtCached(appid string) string {
	artMu.Lock()
	if p, ok := artCache[appid]; ok {
		artMu.Unlock()
		return p
	}
	if t, ok := artTried[appid]; ok && time.Since(t) < 10*time.Minute {
		artMu.Unlock()
		return ""
	}
	artTried[appid] = time.Now()
	artMu.Unlock()
	p := steamArt(appid)
	artMu.Lock()
	if p != "" {
		artCache[appid] = p
	}
	artMu.Unlock()
	return p
}

var soundtrackWord = regexp.MustCompile(`(?i)soundtrack|\bost\b|original score|music`)

func findSoundtracks() []*soundtrack {
	out := []*soundtrack{}
	seen := map[string]bool{}
	for _, lib := range steamLibraries() {
		sa := filepath.Join(lib, "steamapps")
		mans, _ := filepath.Glob(filepath.Join(sa, "appmanifest_*.acf"))
		for _, m := range mans {
			a := vdfFile(m)
			if a == nil {
				continue
			}
			id, name, dir := vdfStr(a, "appstate", "appid"), vdfStr(a, "appstate", "name"), vdfStr(a, "appstate", "installdir")
			if id == "" || dir == "" || seen[id] {
				continue
			}
			// a soundtrack app: in steamapps/music; an older one: a game folder whose name says so
			cand := filepath.Join(sa, "music", dir)
			if fi, err := os.Stat(cand); err != nil || !fi.IsDir() {
				cand = filepath.Join(sa, "common", dir)
				if !soundtrackWord.MatchString(name) && !soundtrackWord.MatchString(dir) {
					cand = ""
				}
			}
			if cand == "" {
				continue
			}
			tr, f, cv := soundtrackFiles(cand)
			if len(tr) == 0 {
				continue
			}
			// Steam's own record: the English title, the artist, and its album cover (in Steam's
			// library cache), which comes first; then a picture in the folder, the art inside the
			// tracks (the SS1 takes it from there), the biggest square picture Steam keeps for it
			info, _ := steamAlbumInfo(id)
			title := strings.TrimSpace(info.Title)
			if title == "" {
				title = name
			}
			if sc := steamCoverFile(id, info.CoverID); sc != "" {
				cv = sc
			}
			emb := cv == "" && hasEmbeddedArt(tr[0].Path)
			if cv == "" && !emb {
				cv = steamArtCached(id)
			}
			s := &soundtrack{AppID: id, Title: title, Artist: strings.TrimSpace(info.Artist), Format: f, Tracks: len(tr), Cover: cv, HasArt: cv != "", Embedded: emb, files: tr, dir: cand, name: name, info: info}
			for _, t := range tr {
				s.Size += t.Size
			}
			seen[id] = true
			out = append(out, s)
		}
	}
	sort.Slice(out, func(i, j int) bool { return strings.ToLower(out[i].Title) < strings.ToLower(out[j].Title) })
	return out
}

// the album's folder name on the SS1
func albumFolder(title string) string {
	r := strings.NewReplacer("/", "_", "\\", "_", ":", "_", "*", "_", "?", "_", "\"", "_", "<", "_", ">", "_", "|", "_")
	t := strings.TrimRight(strings.TrimSpace(r.Replace(title)), ". ")
	if t == "" {
		t = "Soundtrack"
	}
	return t
}

// ---- sending

var stMu sync.Mutex
var stJob struct {
	Busy    bool   `json:"busy"`
	Title   string `json:"title"`
	Done    int64  `json:"done"`
	Total   int64  `json:"total"`
	Message string `json:"message"`
	queue   []string
}

func putMusicFile(album, name string, r io.Reader, size int64) error {
	cfgMu.Lock()
	addr := cfg.Address
	cfgMu.Unlock()
	tok := token()
	if addr == "" || tok == "" {
		return errNotPaired
	}
	u := "http://" + addr + "/music/" + url.PathEscape(album) + "/" + url.PathEscape(name)
	req, err := http.NewRequest("PUT", u, r)
	if err != nil {
		return err
	}
	req.ContentLength = size
	req.Header.Set("Authorization", "Bearer "+tok)
	resp, err := client(time.Duration(60+size/(256<<10)) * time.Second).Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		b, _ := io.ReadAll(io.LimitReader(resp.Body, 4096))
		var e struct{ Error string }
		json.Unmarshal(b, &e)
		if e.Error == "" {
			e.Error = resp.Status
		}
		return fmt.Errorf("%s", e.Error)
	}
	return nil
}

type countReader struct {
	r io.Reader
}

func (c *countReader) Read(p []byte) (int, error) {
	n, err := c.r.Read(p)
	stMu.Lock()
	stJob.Done += int64(n)
	stMu.Unlock()
	return n, err
}

func sendSoundtrack(s *soundtrack) error {
	album := albumFolder(s.Title)
	used := map[string]bool{}
	for _, t := range s.files {
		name := t.Name
		for i := 2; used[strings.ToLower(name)]; i++ { // the same name in two folders (disc 1, disc 2)
			name = fmt.Sprintf("%s (%d)%s", strings.TrimSuffix(t.Name, filepath.Ext(t.Name)), i, filepath.Ext(t.Name))
		}
		used[strings.ToLower(name)] = true
		f, err := os.Open(t.Path)
		if err != nil {
			return err
		}
		err = putMusicFile(album, name, &countReader{f}, t.Size)
		f.Close()
		if err != nil {
			return err
		}
	}
	if s.Cover != "" {
		if f, err := os.Open(s.Cover); err == nil {
			fi, _ := f.Stat()
			putMusicFile(album, "cover"+strings.ToLower(filepath.Ext(s.Cover)), f, fi.Size())
			f.Close()
		}
	}
	return putAlbumJSON(album, s)
}

func steamWorker() {
	for {
		stMu.Lock()
		if len(stJob.queue) == 0 {
			stJob.Busy = false
			stMu.Unlock()
			call(json.RawMessage(`{"cmd":"music_albums","rescan":true}`))
			return
		}
		id := stJob.queue[0]
		stJob.queue = stJob.queue[1:]
		stMu.Unlock()
		var s *soundtrack
		for _, x := range findSoundtracks() {
			if x.AppID == id {
				s = x
			}
		}
		if s == nil {
			continue
		}
		stMu.Lock()
		stJob.Title, stJob.Done, stJob.Total, stJob.Message = s.Title, 0, s.Size, ""
		stMu.Unlock()
		err := sendSoundtrack(s)
		stMu.Lock()
		if err != nil {
			stJob.Message = s.Title + ": " + err.Error()
		} else {
			stJob.Message = s.Title + " is on the SS1"
		}
		stMu.Unlock()
	}
}

// GET: the soundtracks, with which are on the SS1 already; POST {"send":[appid...]}: send them
func apiSteam(w http.ResponseWriter, r *http.Request) {
	if r.Method == "POST" {
		var q struct {
			Send []string `json:"send"`
		}
		if json.NewDecoder(io.LimitReader(r.Body, 1<<16)).Decode(&q) != nil {
			fail(w, "bad request")
			return
		}
		stMu.Lock()
		stJob.queue = append(stJob.queue, q.Send...)
		start := !stJob.Busy
		stJob.Busy = true
		stMu.Unlock()
		if start {
			go steamWorker()
		}
		writeJSON(w, map[string]any{"ok": true})
		return
	}
	list := findSoundtracks()
	// on the SS1: its Steam albums, by app id (older sends: by title, Steam's plain name or the English one)
	type ss1Album struct {
		Title, Artist, Source, Cover, Folder string
		AppID                                string `json:"steam_appid"`
	}
	byID, byTitle := map[string]ss1Album{}, map[string]ss1Album{}
	if b, _, err := call(json.RawMessage(`{"cmd":"music_albums"}`)); err == nil {
		var j struct {
			Albums []ss1Album `json:"albums"`
		}
		json.Unmarshal(b, &j)
		for _, a := range j.Albums {
			if a.Source != "steam" {
				continue
			}
			if a.AppID != "" {
				byID[a.AppID] = a
			}
			byTitle[strings.ToLower(a.Title)] = a
		}
	}
	var fixes []fixup
	for _, s := range list {
		a, ok := byID[s.AppID]
		if !ok {
			a, ok = byTitle[strings.ToLower(s.Title)]
		}
		if !ok {
			a, ok = byTitle[strings.ToLower(s.name)]
		}
		s.OnSS1 = ok
		if !ok || a.Folder == "" {
			continue
		}
		f := fixup{s: s, folder: a.Folder}
		f.cover = a.Cover == "" && s.Cover != "" && !s.Embedded
		f.meta = a.Title != s.Title || a.Artist != s.Artist || a.AppID != s.AppID
		if f.cover || f.meta {
			fixes = append(fixes, f)
		}
	}
	sendFixups(fixes)
	stMu.Lock()
	job := map[string]any{"busy": stJob.Busy, "title": stJob.Title, "done": stJob.Done, "total": stJob.Total, "message": stJob.Message, "waiting": len(stJob.queue)}
	stMu.Unlock()
	writeJSON(w, map[string]any{"ok": true, "steam": steamPath() != "", "libraries": len(steamLibraries()), "soundtracks": list, "job": job})
}

// album.json: what the SS1 shows for the album
func albumJSON(s *soundtrack) []byte {
	m := map[string]string{"title": s.Title, "source": "steam", "steam_appid": s.AppID}
	for k, v := range map[string]string{"artist": s.Artist, "composer": strings.TrimSpace(s.info.Composer), "label": strings.TrimSpace(s.info.Label)} {
		if v != "" {
			m[k] = v
		}
	}
	b, _ := json.Marshal(m)
	return b
}

func putAlbumJSON(folder string, s *soundtrack) error {
	b := albumJSON(s)
	return putMusicFile(folder, "album.json", strings.NewReader(string(b)), int64(len(b)))
}

// the soundtrack's album cover in Steam's library cache, named in Steam's record
func steamCoverFile(appid, coverID string) string {
	sp := steamPath()
	if sp == "" || coverID == "" || strings.ContainsAny(coverID, `/\.`) {
		return ""
	}
	for _, e := range []string{".jpg", ".png"} {
		p := filepath.Join(sp, "appcache", "librarycache", appid, coverID+e)
		if fi, err := os.Stat(p); err == nil && fi.Size() > 0 {
			return p
		}
	}
	return ""
}

// albums sent before Phosphor read Steam's record: their English title, artist and cover, sent on
// their own (not the music again), once per album while the app runs
type fixup struct {
	s           *soundtrack
	folder      string
	cover, meta bool
}

var fixed = map[string]bool{}

func sendFixups(list []fixup) {
	stMu.Lock()
	var todo []fixup
	for _, f := range list {
		if !fixed[f.s.AppID] && !stJob.Busy {
			fixed[f.s.AppID] = true
			todo = append(todo, f)
		}
	}
	stMu.Unlock()
	if len(todo) == 0 {
		return
	}
	go func() {
		for _, f := range todo {
			if f.cover {
				if fh, err := os.Open(f.s.Cover); err == nil {
					fi, _ := fh.Stat()
					putMusicFile(f.folder, "cover"+strings.ToLower(filepath.Ext(f.s.Cover)), fh, fi.Size())
					fh.Close()
				}
			}
			if f.meta {
				putAlbumJSON(f.folder, f.s)
			}
		}
		call(json.RawMessage(`{"cmd":"music_albums","rescan":true}`))
	}()
}

// a soundtrack's cover from this PC (the list's pictures)
func apiSteamCover(w http.ResponseWriter, r *http.Request) {
	id := strings.TrimPrefix(r.URL.Path, "/api/steam/cover/")
	for _, s := range findSoundtracks() {
		if s.AppID == id && s.Cover != "" {
			http.ServeFile(w, r, s.Cover)
			return
		}
	}
	http.NotFound(w, r)
}
