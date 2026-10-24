package main

// Box art on the PC: ArcadeDB (no account) and the sources that need an account or key (TheGamesDB,
// ScreenScraper, IGDB), as phosphord's src_arcadedb.c and src_accounts.c. The logins are the ones
// saved on the SS1 (sent to the app over the home network) and only go to those services.

import (
	"context"
	"encoding/json"
	"fmt"
	"strconv"
	"strings"
	"sync"
	"time"
)

type platNums struct {
	tgdb, ss int
	igdb     []int
}

var platOf = map[string]platNums{
	"PSX": {10, 57, []int{7}}, "Saturn": {17, 22, []int{32}}, "MegaCD": {21, 20, []int{78}}, "TGFX16CD": {4955, 114, []int{150}},
	"NES": {7, 3, []int{18, 51}}, "SNES": {6, 4, []int{19}}, "Genesis": {18, 1, []int{29}}, "N64": {3, 14, []int{4}},
	"SMS": {35, 2, []int{64, 84}}, "TGFX16": {34, 31, []int{86}}, "ATARI7800": {27, 41, []int{60, 59}},
	"GAMEBOY": {4, 9, []int{33, 22}}, "GBA": {5, 12, []int{24}}, "GameGear": {20, 21, []int{35}},
	"WonderSwan": {4925, 45, []int{57, 123}}, "AtariLynx": {4924, 28, []int{61}}, "Arcade": {23, 75, []int{52}},
	"C64": {40, 66, []int{15}}, "MSX": {4929, 113, []int{27}}, "Atari800": {4943, 43, []int{65}},
}

func jstr(m map[string]any, k string) string {
	if v, ok := m[k].(string); ok {
		return v
	}
	return ""
}
func jobj(m map[string]any, k string) map[string]any {
	v, _ := m[k].(map[string]any)
	return v
}
func jarr(m map[string]any, k string) []any {
	v, _ := m[k].([]any)
	return v
}

// ---------- ArcadeDB ----------

var adbPace = &pace{gap: 250 * time.Millisecond}

func adbHistory(h string) string {
	if h == "" {
		return ""
	}
	s := h
	if strings.HasPrefix(s, "Arcade Video game published") {
		if i := strings.IndexByte(s, '\n'); i >= 0 {
			s = s[i+1:]
		}
	}
	s = strings.ReplaceAll(s, "\r", "")
	for _, c := range []string{"- TECHNICAL -", "- TRIVIA -", "- CONTRIBUTE -", "- UPDATES -", "- SERIES -", "- STAFF -"} {
		if i := strings.Index(s, c); i >= 0 {
			s = s[:i]
		}
	}
	return strings.Trim(s, "\n ")
}

func adbLookup(ctx context.Context, s *scrapeRun, g *sgame, r *sres) int {
	if !g.Arcade || g.Serial == "" {
		return 0
	}
	adbPace.wait(ctx)
	b, rc, _ := netRequest(ctx, "GET", "https://adb.arcadeitalia.net/service_scraper.php?ajax=query_mame&lang=en&game_name="+netEscape(g.Serial), nil, nil, 8<<20)
	if rc < 0 {
		return -1
	}
	if rc != 0 {
		return 0
	}
	var j map[string]any
	if json.Unmarshal(b, &j) != nil {
		return 0
	}
	res := jarr(j, "result")
	if len(res) == 0 {
		return 0
	}
	x, _ := res[0].(map[string]any)
	if x == nil {
		return 0
	}
	if t := jstr(x, "title"); t != "" && g.Official == "" {
		g.Official = t
	}
	r.set(fDev, jstr(x, "manufacturer"))
	r.set(fPub, jstr(x, "manufacturer"))
	r.set(fYear, jstr(x, "year"))
	r.set(fGenre, jstr(x, "genre"))
	if p, ok := x["players"].(float64); ok && p > 0 {
		r.set(fPlayers, strconv.Itoa(int(p)))
	}
	r.set(fDesc, adbHistory(jstr(x, "history")))
	r.addArt("arcadedb", jstr(x, "url_image_flyer"))
	r.addArt("arcadedb", jstr(x, "url_image_title"))
	return 1
}

// ---------- TheGamesDB ----------

func tgdbLookup(ctx context.Context, s *scrapeRun, g *sgame, r *sres) int {
	p, ok := platOf[g.Sys]
	if !ok || g.Official == "" {
		return 0
	}
	base := nameBase(g.Official)
	u := fmt.Sprintf("https://api.thegamesdb.net/v1.1/Games/ByGameName?apikey=%s&name=%s&fields=players,overview&filter%%5Bplatform%%5D=%d&include=boxart",
		netEscape(s.acct("thegamesdb", "apikey")), netEscape(base), p.tgdb)
	b, rc, _ := netRequest(ctx, "GET", u, nil, nil, 8<<20)
	if rc == 2 {
		s.note("TheGamesDB refused the API key")
		return 0
	}
	if rc != 0 {
		if rc < 0 {
			return -1
		}
		return 0
	}
	var j map[string]any
	if json.Unmarshal(b, &j) != nil {
		return 0
	}
	want := titleNorm(base)
	var best map[string]any
	for _, x := range jarr(jobj(j, "data"), "games") {
		m, _ := x.(map[string]any)
		if m != nil && titleNorm(jstr(m, "game_title")) == want {
			best = m
			break
		}
	}
	if best == nil {
		return 0
	}
	r.set(fDesc, jstr(best, "overview"))
	if rd := jstr(best, "release_date"); len(rd) >= 4 {
		r.set(fYear, rd[:4])
	}
	if pl, ok := best["players"].(float64); ok && pl > 0 {
		r.set(fPlayers, strconv.Itoa(int(pl)))
	}
	bx := jobj(jobj(j, "include"), "boxart")
	baseURL := jstr(jobj(bx, "base_url"), "original")
	id := -1
	if v, ok := best["id"].(float64); ok {
		id = int(v)
	}
	if imgs, ok := jobj(bx, "data")[strconv.Itoa(id)].([]any); ok {
		for _, x := range imgs {
			m, _ := x.(map[string]any)
			if m != nil && baseURL != "" && jstr(m, "filename") != "" && jstr(m, "side") == "front" {
				r.addArt("thegamesdb", baseURL+jstr(m, "filename"))
				break
			}
		}
	}
	return 1
}

// ---------- ScreenScraper ----------

var ssLock sync.Mutex // one request at a time (a user's thread limit)

// a developer login, or the regular screenscraper.fr login on its own (used for both then)
func ssDev(s *scrapeRun, k, userK string) string {
	if s.acct("screenscraper", "devid") != "" {
		return s.acct("screenscraper", k)
	}
	return s.acct("screenscraper", userK)
}

func langText(arr []any, field string) string {
	var first map[string]any
	for _, x := range arr {
		m, _ := x.(map[string]any)
		if m == nil {
			continue
		}
		if first == nil {
			first = m
		}
		if jstr(m, "langue") == "en" {
			return jstr(m, field)
		}
	}
	if first != nil {
		return jstr(first, field)
	}
	return ""
}

func ssLookup(ctx context.Context, s *scrapeRun, g *sgame, r *sres) int {
	p, ok := platOf[g.Sys]
	if !ok || (g.CRC == "" && g.Serial == "") {
		return 0
	}
	nm := g.Title
	if g.Arcade {
		nm = g.Serial
	}
	u := fmt.Sprintf("https://api.screenscraper.fr/api2/jeuInfos.php?devid=%s&devpassword=%s&softname=Phosphor&output=json&ssid=%s&sspassword=%s&systemeid=%d&romtype=rom&romnom=%s",
		netEscape(ssDev(s, "devid", "user")), netEscape(ssDev(s, "devpassword", "password")),
		netEscape(s.acct("screenscraper", "user")), netEscape(s.acct("screenscraper", "password")), p.ss, netEscape(nm))
	if g.CRC != "" {
		u += "&crc=" + g.CRC
	}
	if g.Disc && g.Serial != "" {
		u += "&serialnum=" + netEscape(g.Serial)
	}
	if g.Size > 0 {
		u += "&romtaille=" + strconv.FormatInt(g.Size, 10)
	}
	ssLock.Lock()
	b, rc, http := netRequest(ctx, "GET", u, nil, nil, 16<<20)
	ssLock.Unlock()
	if rc == 2 {
		s.note(fmt.Sprintf("ScreenScraper refused the login (HTTP %d)", http))
		return 0
	}
	if http == 430 || http == 429 {
		return -1
	}
	if rc != 0 {
		if rc < 0 {
			return -1
		}
		return 0
	}
	var j map[string]any
	if json.Unmarshal(b, &j) != nil {
		return 0
	}
	jeu := jobj(jobj(j, "response"), "jeu")
	if jeu == nil {
		return 0
	}
	if g.Official == "" {
		if t := langText(jarr(jeu, "noms"), "text"); t != "" {
			g.Official = t
		}
	}
	r.set(fDesc, langText(jarr(jeu, "synopsis"), "text"))
	r.set(fDev, jstr(jobj(jeu, "developpeur"), "text"))
	r.set(fPub, jstr(jobj(jeu, "editeur"), "text"))
	r.set(fPlayers, jstr(jobj(jeu, "joueurs"), "text"))
	if gs := jarr(jeu, "genres"); len(gs) > 0 {
		if g0, _ := gs[0].(map[string]any); g0 != nil {
			r.set(fGenre, langText(jarr(g0, "noms"), "text"))
		}
	}
	if ds := jarr(jeu, "dates"); len(ds) > 0 {
		if d0, _ := ds[0].(map[string]any); d0 != nil && len(jstr(d0, "text")) >= 4 {
			r.set(fYear, jstr(d0, "text")[:4])
		}
	}
	media := jarr(jeu, "medias")
	for _, reg := range []string{"us", "wor", "eu", "ss", "jp"} {
		for _, x := range media {
			m, _ := x.(map[string]any)
			if m != nil && jstr(m, "type") == "box-2D" && jstr(m, "region") == reg && jstr(m, "url") != "" {
				r.addArt("screenscraper", jstr(m, "url"))
				return 1
			}
		}
	}
	return 1
}

// ---------- IGDB ----------

var igdb struct {
	mu      sync.Mutex
	token   string
	expires time.Time
}
var igdbPace = &pace{gap: 260 * time.Millisecond}

func igdbAuth(ctx context.Context, s *scrapeRun) int {
	igdb.mu.Lock()
	defer igdb.mu.Unlock()
	if igdb.token != "" && time.Now().Before(igdb.expires.Add(-time.Minute)) {
		return 0
	}
	u := fmt.Sprintf("https://id.twitch.tv/oauth2/token?client_id=%s&client_secret=%s&grant_type=client_credentials",
		netEscape(s.acct("igdb", "client_id")), netEscape(s.acct("igdb", "client_secret")))
	b, rc, _ := netRequest(ctx, "POST", u, nil, strings.NewReader(""), 1<<20)
	if rc != 0 {
		if rc == 1 || rc == 2 {
			s.note("IGDB: Twitch refused the client id/secret")
		}
		if rc < 0 {
			return -1
		}
		return 1
	}
	var j map[string]any
	if json.Unmarshal(b, &j) != nil || jstr(j, "access_token") == "" {
		return 1
	}
	igdb.token = jstr(j, "access_token")
	ex := 3600.0
	if v, ok := j["expires_in"].(float64); ok {
		ex = v
	}
	igdb.expires = time.Now().Add(time.Duration(ex) * time.Second)
	return 0
}

func igdbLookup(ctx context.Context, s *scrapeRun, g *sgame, r *sres) int {
	p, ok := platOf[g.Sys]
	if !ok || g.Official == "" {
		return 0
	}
	if a := igdbAuth(ctx, s); a != 0 {
		if a < 0 {
			return -1
		}
		return 0
	}
	base := strings.ReplaceAll(nameBase(g.Official), `"`, "'")
	ps := make([]string, len(p.igdb))
	for i, n := range p.igdb {
		ps[i] = strconv.Itoa(n)
	}
	q := fmt.Sprintf(`search "%s"; fields name,summary,first_release_date,genres.name,involved_companies.company.name,involved_companies.developer,involved_companies.publisher,cover.image_id; where platforms = (%s); limit 10;`, base, strings.Join(ps, ","))
	igdb.mu.Lock()
	tok := igdb.token
	igdb.mu.Unlock()
	igdbPace.wait(ctx)
	b, rc, _ := netRequest(ctx, "POST", "https://api.igdb.com/v4/games", map[string]string{
		"Client-ID": s.acct("igdb", "client_id"), "Authorization": "Bearer " + tok, "Accept": "application/json"}, strings.NewReader(q), 8<<20)
	if rc == 2 {
		igdb.mu.Lock()
		igdb.token = ""
		igdb.mu.Unlock()
		return 0
	}
	if rc != 0 {
		if rc < 0 {
			return -1
		}
		return 0
	}
	var arr []map[string]any
	if json.Unmarshal(b, &arr) != nil {
		return 0
	}
	want := titleNorm(nameBase(g.Official))
	var best map[string]any
	for _, x := range arr {
		if titleNorm(jstr(x, "name")) == want {
			best = x
			break
		}
	}
	if best == nil {
		return 0
	}
	r.set(fDesc, jstr(best, "summary"))
	if t, ok := best["first_release_date"].(float64); ok {
		r.set(fYear, strconv.Itoa(time.Unix(int64(t), 0).UTC().Year()))
	}
	if gs := jarr(best, "genres"); len(gs) > 0 {
		if g0, _ := gs[0].(map[string]any); g0 != nil {
			r.set(fGenre, jstr(g0, "name"))
		}
	}
	for _, x := range jarr(best, "involved_companies") {
		m, _ := x.(map[string]any)
		if m == nil {
			continue
		}
		co := jstr(jobj(m, "company"), "name")
		if v, _ := m["developer"].(bool); v {
			r.set(fDev, co)
		}
		if v, _ := m["publisher"].(bool); v {
			r.set(fPub, co)
		}
	}
	if img := jstr(jobj(best, "cover"), "image_id"); img != "" {
		r.addArt("igdb", "https://images.igdb.com/igdb/image/upload/t_cover_big/"+img+".jpg")
	}
	return 1
}
