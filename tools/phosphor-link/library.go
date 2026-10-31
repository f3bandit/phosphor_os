// Library tools: manage a Phosphor library (box art, details, "Based on" links) over the home network,
// through phosphord's remote access (Settings > Remote access on the MiSTer; see docs/remote-api.md).
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
//
// These don't use the debug port: they talk HTTP to port 8767. The pairing (address + device key) is
// kept in the user's settings folder (library.json, readable only by this user), not next to the program.
package main

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math/rand"
	"net"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"
)

const remotePort = 8767

type libConfig struct {
	Address string `json:"address"` // host:port
	Token   string `json:"token"`
	ID      string `json:"id"`
}

func libConfigPath() string {
	d, err := os.UserConfigDir()
	if err != nil {
		d = exeDir()
	}
	return filepath.Join(d, "PhosphorLink", "library.json")
}

func loadLib() (libConfig, error) {
	var c libConfig
	data, err := os.ReadFile(libConfigPath())
	if err != nil {
		return c, errors.New("not paired with a MiSTer yet: on the MiSTer open Settings > Remote access, turn it on, " +
			"choose Pair a device, then use library_pair with the address and code shown on the TV")
	}
	if err := json.Unmarshal(data, &c); err != nil || c.Address == "" || c.Token == "" {
		return c, errors.New("library.json is damaged: pair again with library_pair")
	}
	return c, nil
}

func saveLib(c libConfig) error {
	p := libConfigPath()
	if err := os.MkdirAll(filepath.Dir(p), 0700); err != nil {
		return err
	}
	data, _ := json.MarshalIndent(c, "", "  ")
	tmp := p + ".new"
	if err := os.WriteFile(tmp, data, 0600); err != nil {
		return err
	}
	return os.Rename(tmp, p)
}

var httpc = &http.Client{Timeout: 150 * time.Second}

func normAddress(a string) (string, error) {
	a = strings.TrimSpace(a)
	a = strings.TrimPrefix(strings.TrimPrefix(a, "http://"), "https://")
	a = strings.TrimSuffix(a, "/")
	if a == "" {
		return "", errors.New("address is empty (it's shown on the MiSTer's Remote access screen, e.g. 192.168.1.23:8767)")
	}
	if _, _, err := net.SplitHostPort(a); err != nil {
		a = net.JoinHostPort(a, strconv.Itoa(remotePort))
	}
	return a, nil
}

// busyTries: a request the MiSTer is too busy for is sent again this many times (about 30 seconds in all)
var busyTries = 20

// busyWait: a short wait, a little longer each time, with some jitter
func busyWait(try int) time.Duration {
	ms := 250 * (try + 1)
	if ms > 2000 {
		ms = 2000
	}
	return time.Duration(ms+rand.Intn(250)) * time.Millisecond
}

// notSent: the connection died before the request went out (Go: "readLoopPeekFailLocked", an answer
// arrived before the request was written, as a busy SS1 used to send it): nothing reached the SS1, so
// any request can be sent again
func notSent(err error) bool {
	return strings.Contains(err.Error(), "readLoopPeekFailLocked")
}

// cutOff: the connection was made and then reset or closed before an answer (not: no connection at all,
// or no answer in time)
func cutOff(err error) bool {
	var op *net.OpError
	if errors.As(err, &op) && op.Op == "dial" {
		return false
	}
	var ne net.Error
	if errors.As(err, &ne) && ne.Timeout() {
		return false
	}
	return errors.Is(err, syscall.ECONNRESET) || errors.Is(err, syscall.EPIPE) || errors.Is(err, io.EOF) ||
		errors.Is(err, io.ErrUnexpectedEOF) || strings.Contains(err.Error(), "readLoopPeekFailLocked") ||
		strings.Contains(err.Error(), "server closed") || strings.Contains(err.Error(), "forcibly closed") ||
		strings.Contains(err.Error(), "connection reset")
}

// do sends a request to phosphord and returns the body (JSON or image). The MiSTer serves a few connections
// from one address at once (the PC app looking up box art can use them all) and answers more with 503 "busy",
// before reading them: those are sent again after a moment. So is an upload whose connection was cut off
// (a busy MiSTer drops the rest of a big upload, which can cut off its answer); an upload only replaces a file.
func libDo(c libConfig, method, path string, body io.Reader, ctype string) ([]byte, string, error) {
	var data []byte
	if body != nil {
		b, err := io.ReadAll(body)
		if err != nil {
			return nil, "", err
		}
		data = b
	}
	var resp *http.Response
	for try := 0; ; try++ {
		var rd io.Reader
		if body != nil {
			rd = bytes.NewReader(data)
		}
		req, err := http.NewRequest(method, "http://"+c.Address+path, rd)
		if err != nil {
			return nil, "", err
		}
		if c.Token != "" {
			req.Header.Set("Authorization", "Bearer "+c.Token)
		}
		if ctype != "" {
			req.Header.Set("Content-Type", ctype)
		}
		resp, err = httpc.Do(req)
		if err != nil {
			if try < busyTries && (notSent(err) || (method == http.MethodPut && cutOff(err))) {
				time.Sleep(busyWait(try))
				continue
			}
			return nil, "", fmt.Errorf("can't reach the MiSTer at %s (is it on, on this network, with Remote access turned on?): %v", c.Address, err)
		}
		if resp.StatusCode != 503 || resp.Header.Get("Retry-After") == "" || try >= busyTries {
			break
		}
		io.Copy(io.Discard, io.LimitReader(resp.Body, 4096))
		resp.Body.Close()
		time.Sleep(busyWait(try))
	}
	defer resp.Body.Close()
	data, err := io.ReadAll(io.LimitReader(resp.Body, 64<<20))
	if err != nil {
		return nil, "", err
	}
	if resp.StatusCode != 200 {
		var e struct {
			Error string `json:"error"`
		}
		if json.Unmarshal(data, &e) == nil && e.Error != "" {
			if resp.StatusCode == 401 && path != "/pair" {
				return nil, "", errors.New(e.Error + " (this PC's pairing was removed or is wrong: pair again with library_pair)")
			}
			return nil, "", errors.New(e.Error)
		}
		return nil, "", fmt.Errorf("MiSTer answered %s", resp.Status)
	}
	return data, resp.Header.Get("Content-Type"), nil
}

// api sends one phosphord command and returns the reply, or its error text.
func libAPI(c libConfig, cmd map[string]any) (map[string]any, error) {
	b, _ := json.Marshal(cmd)
	data, _, err := libDo(c, "POST", "/api", bytes.NewReader(b), "application/json")
	if err != nil {
		return nil, err
	}
	var r map[string]any
	if err := json.Unmarshal(data, &r); err != nil {
		return nil, errors.New("the MiSTer's answer wasn't JSON")
	}
	if ok, _ := r["ok"].(bool); !ok {
		msg, _ := r["error"].(string)
		if msg == "" {
			msg = "the MiSTer said no"
		}
		return nil, errors.New(msg)
	}
	delete(r, "ok")
	return r, nil
}

func pretty(v any) string {
	b, _ := json.MarshalIndent(v, "", " ")
	return clip(string(b), maxText)
}

// image from a local file or a web address, at most 16 MB
func fetchImage(file, link string) ([]byte, error) {
	const max = 16 << 20
	var data []byte
	switch {
	case file != "":
		st, err := os.Stat(file)
		if err != nil {
			return nil, err
		}
		if st.Size() > max {
			return nil, errors.New("image is bigger than 16 MB")
		}
		if data, err = os.ReadFile(file); err != nil {
			return nil, err
		}
	case link != "":
		u, err := url.Parse(link)
		if err != nil || (u.Scheme != "http" && u.Scheme != "https") {
			return nil, errors.New("url must be http(s)")
		}
		req, _ := http.NewRequest("GET", link, nil)
		req.Header.Set("User-Agent", "PhosphorLink/"+version)
		resp, err := (&http.Client{Timeout: 60 * time.Second}).Do(req)
		if err != nil {
			return nil, err
		}
		defer resp.Body.Close()
		if resp.StatusCode != 200 {
			return nil, fmt.Errorf("download failed: %s", resp.Status)
		}
		if data, err = io.ReadAll(io.LimitReader(resp.Body, max+1)); err != nil {
			return nil, err
		}
		if len(data) > max {
			return nil, errors.New("image is bigger than 16 MB")
		}
	default:
		return nil, errors.New("give file (a path on this PC) or url")
	}
	if !strings.HasPrefix(http.DetectContentType(data), "image/") {
		return nil, errors.New("that isn't an image (it may be a web page: use the direct link to the picture)")
	}
	return data, nil
}

// content blocks for an MCP reply
type block = map[string]any

func textBlock(s string) block { return block{"type": "text", "text": s} }

func libTool(name string, a map[string]any) ([]block, bool) {
	fail := func(err error) ([]block, bool) { return []block{textBlock(err.Error())}, true }
	okText := func(s string) ([]block, bool) { return []block{textBlock(s)}, false }

	if name == "library_pair" {
		addr, err := normAddress(argS(a, "address", ""))
		if err != nil {
			return fail(err)
		}
		code := strings.ReplaceAll(strings.ReplaceAll(argS(a, "code", ""), " ", ""), "-", "")
		host, _ := os.Hostname()
		devName := argS(a, "name", "")
		if devName == "" {
			devName = "Phosphor Link on " + host
		}
		b, _ := json.Marshal(map[string]string{"code": code, "name": devName})
		data, _, err := libDo(libConfig{Address: addr}, "POST", "/pair", bytes.NewReader(b), "application/json")
		if err != nil {
			return fail(err)
		}
		var r struct {
			Token string `json:"token"`
			ID    string `json:"id"`
		}
		if json.Unmarshal(data, &r) != nil || len(r.Token) != 64 {
			return fail(errors.New("pairing failed: unexpected answer"))
		}
		if err := saveLib(libConfig{Address: addr, Token: r.Token, ID: r.ID}); err != nil {
			return fail(fmt.Errorf("paired, but couldn't save the key: %v", err))
		}
		return okText(fmt.Sprintf("Paired with the MiSTer at %s as %q. The key is saved in %s.", addr, devName, libConfigPath()))
	}

	c, err := loadLib()
	if err != nil {
		return fail(err)
	}
	id := int64(argN(a, "id", 0))
	needID := func() error {
		if id <= 0 {
			return errors.New("id is needed (a game id from library_games)")
		}
		return nil
	}

	switch name {
	case "library_status":
		r, err := libAPI(c, map[string]any{"cmd": "status"})
		if err != nil {
			return fail(err)
		}
		sc, _ := libAPI(c, map[string]any{"cmd": "scrape"})
		out := map[string]any{"address": c.Address, "phosphord": r, "scraper": sc}
		for _, f := range []string{"missing_art", "missing_details", "not_found", "unidentified"} {
			if g, err := libAPI(c, map[string]any{"cmd": "games", "filter": f, "limit": 0}); err == nil {
				out[f] = g["total"]
			}
		}
		return okText(pretty(out))
	case "library_systems":
		r, err := libAPI(c, map[string]any{"cmd": "systems"})
		if err != nil {
			return fail(err)
		}
		return okText(pretty(r["systems"]))
	case "library_games":
		cmd := map[string]any{"cmd": "games", "offset": argN(a, "offset", 0), "limit": argN(a, "limit", 50)}
		for _, k := range []string{"system", "filter", "q"} {
			if v := argS(a, k, ""); v != "" {
				cmd[k] = v
			}
		}
		r, err := libAPI(c, cmd)
		if err != nil {
			return fail(err)
		}
		return okText(pretty(r))
	case "library_game":
		if err := needID(); err != nil {
			return fail(err)
		}
		r, err := libAPI(c, map[string]any{"cmd": "game", "id": id})
		if err != nil {
			return fail(err)
		}
		return okText(pretty(r["game"]))
	case "library_edit":
		if err := needID(); err != nil {
			return fail(err)
		}
		set := map[string]any{}
		for _, k := range []string{"title", "description", "year", "developer", "publisher", "genre", "players", "region"} {
			if v, ok := a[k]; ok {
				set[k] = v
			}
		}
		if len(set) == 0 {
			return fail(errors.New("nothing to change: give title, description, year, developer, publisher, genre, players or region"))
		}
		r, err := libAPI(c, map[string]any{"cmd": "edit", "id": id, "set": set})
		if err != nil {
			return fail(err)
		}
		return okText("Saved.\n" + pretty(r["game"]))
	case "library_set_art":
		if err := needID(); err != nil {
			return fail(err)
		}
		img, err := fetchImage(argS(a, "file", ""), argS(a, "url", ""))
		if err != nil {
			return fail(err)
		}
		data, _, err := libDo(c, "PUT", fmt.Sprintf("/art/%d", id), bytes.NewReader(img), http.DetectContentType(img))
		if err != nil {
			return fail(err)
		}
		var r map[string]any
		if json.Unmarshal(data, &r) != nil {
			return fail(errors.New("unexpected answer"))
		}
		if ok, _ := r["ok"].(bool); !ok {
			msg, _ := r["error"].(string)
			return fail(errors.New(msg))
		}
		g, _ := r["game"].(map[string]any)
		return okText(fmt.Sprintf("Box art stored for %v (%d KB sent; the MiSTer scaled it to 512 px).", g["title"], len(img)/1024))
	case "library_get_art":
		if err := needID(); err != nil {
			return fail(err)
		}
		data, _, err := libDo(c, "GET", fmt.Sprintf("/art/%d", id), nil, "")
		if err != nil {
			return fail(err)
		}
		blocks := []block{{"type": "image", "data": base64.StdEncoding.EncodeToString(data), "mimeType": "image/jpeg"}}
		msg := fmt.Sprintf("Box art of game %d (%d KB).", id, len(data)/1024)
		if p := argS(a, "save_to", ""); p != "" {
			if err := os.WriteFile(p, data, 0644); err != nil {
				return fail(err)
			}
			msg += " Saved to " + p
		}
		return append(blocks, textBlock(msg)), false
	case "library_candidates":
		if err := needID(); err != nil {
			return fail(err)
		}
		cmd := map[string]any{"cmd": "candidates", "id": id, "limit": argN(a, "limit", 15)}
		if q := argS(a, "q", ""); q != "" {
			cmd["q"] = q
		}
		r, err := libAPI(c, cmd)
		if err != nil {
			return fail(err)
		}
		return okText(pretty(r))
	case "library_link":
		base := int64(argN(a, "base", 0))
		if err := needID(); err != nil || base <= 0 {
			return fail(errors.New("id (the hack, mod or translation) and base (the original game) are needed"))
		}
		r, err := libAPI(c, map[string]any{"cmd": "link", "id": id, "base": base})
		if err != nil {
			return fail(err)
		}
		return okText("Linked.\n" + pretty(r))
	case "library_unlink":
		if err := needID(); err != nil {
			return fail(err)
		}
		r, err := libAPI(c, map[string]any{"cmd": "unlink", "id": id})
		if err != nil {
			return fail(err)
		}
		return okText("Unlinked.\n" + pretty(r))
	case "library_rescrape":
		if err := needID(); err != nil {
			return fail(err)
		}
		if _, err := libAPI(c, map[string]any{"cmd": "rescrape", "id": id}); err != nil {
			return fail(err)
		}
		return okText("Queued: the MiSTer looks this game up again with its scraper sources (only empty fields are filled).")
	}
	return fail(errors.New("unknown tool " + name))
}

var libraryTools = []tool{
	{"library_pair", "Pair this PC with a MiSTer's Phosphor library (once). On the MiSTer: Settings > Remote access, turn it on, choose Pair a device; the TV shows the address and a 6-digit code (valid 2 minutes). Ask the user for both.", obj(map[string]any{
		"address": str("Address shown on the TV, e.g. 192.168.1.23:8767"),
		"code":    str("The 6-digit code shown on the TV"),
		"name":    str("Name for this PC in the MiSTer's device list (optional)"),
	}, "address", "code")},
	{"library_status", "Overview of the paired MiSTer's library: game count, scraper state, and how many games are missing box art, missing details, not found by the scraper, or not identified.", obj(map[string]any{})},
	{"library_systems", "Systems in the library with game counts.", obj(map[string]any{})},
	{"library_games", "List games: one system's, or with filter/q across all systems. Each has id, title, system, art (has box art), details (has a description), linked (uses a base game's art), scrape (found, not_found, linked, ...).", obj(map[string]any{
		"system": str("System id from library_systems, e.g. PSX, SNES (optional with filter or q)"),
		"filter": str("missing_art | missing_details | not_found | linked | unidentified"),
		"q":      str("Text in the title"),
		"offset": num("Skip this many (paging)"),
		"limit":  num("At most this many (default 50)"),
	})},
	{"library_game", "Everything about one game: details, files (name, size, CRC32 / product code), based_on, scrape state, play history.", obj(map[string]any{
		"id": num("Game id"),
	}, "id")},
	{"library_edit", "Change a game's details on the MiSTer. Only the fields given change; an empty string clears one. The title can't be empty.", obj(map[string]any{
		"id":          num("Game id"),
		"title":       str("Display title"),
		"description": str("A short description, in your own words (2-4 sentences)"),
		"year":        str("Release year, e.g. 1997"),
		"developer":   str("Developer"),
		"publisher":   str("Publisher"),
		"genre":       str("Genre, e.g. Platformer, Strategy RPG"),
		"players":     str("Players, e.g. 1, 1-2, 1-4"),
		"region":      str("Region of this release, e.g. USA, Europe, Japan"),
	}, "id")},
	{"library_set_art", "Set a game's box art from an image file on this PC or a direct image URL (PNG/JPEG...; front cover). The MiSTer scales and stores it.", obj(map[string]any{
		"id":   num("Game id"),
		"file": str("Path of an image on this PC"),
		"url":  str("Direct link to the image (not a web page)"),
	}, "id")},
	{"library_get_art", "Show a game's current box art (returned as an image), optionally saving it on this PC.", obj(map[string]any{
		"id":      num("Game id"),
		"save_to": str("Save it here too (optional)"),
	}, "id")},
	{"library_candidates", "Games that a hack, mod or translation could be based on, most alike first (for library_link).", obj(map[string]any{
		"id":    num("The hack/mod's game id"),
		"q":     str("Search text, e.g. the original game's name"),
		"limit": num("Default 15"),
	}, "id")},
	{"library_link", "Mark a game as based on another (a hack, mod, translation or re-release): it then uses the base game's box art and details unless it has its own.", obj(map[string]any{
		"id":   num("The hack/mod's game id"),
		"base": num("The original game's id"),
	}, "id", "base")},
	{"library_unlink", "Remove a game's Based on link.", obj(map[string]any{
		"id": num("Game id"),
	}, "id")},
	{"library_rescrape", "Ask the MiSTer's own scraper to look a game up again (it only fills empty fields).", obj(map[string]any{
		"id": num("Game id"),
	}, "id")},
}

const libraryInstructions = `

Phosphor library (the library_* tools, over the home network; pair once with library_pair):
- Start with library_status, then library_games with a filter (missing_art, missing_details, not_found) to find work.
- Identify the exact release before changing anything: library_game shows the files, CRC32 and product code
  (e.g. SLUS-00994) and the region in the title. Use them to search; don't guess from the title alone.
- Box art: the front cover of that release and region, an official scan or the libretro-thumbnails image
  (https://thumbnails.libretro.com/<system>/Named_Boxarts/<name>.png), not fan art, screenshots or logos.
  Give library_set_art the direct image link (or download it and give the file). Check it with library_get_art.
- Details: short and factual. Write descriptions in your own words (2-4 sentences); don't paste text from
  websites. Year, developer, publisher, genre, players from reliable sources (MobyGames, Wikipedia, the game's box).
- Hacks, mods, translations and betas: link them to their original with library_candidates and library_link
  instead of giving them copies of the same art, unless the user wants their own art.
- Don't overwrite details or art the user already has without asking. For more than about 20 games, show the
  plan and a few examples first, then go ahead.
- Tell the user what changed (titles, not ids). Changes show on the TV straight away.`
