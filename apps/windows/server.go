package main

// The app's own requests (from its window only: the session key, and 127.0.0.1 as the host so a web
// page elsewhere can't reach them through DNS tricks).

import (
	"bytes"
	"crypto/subtle"
	"encoding/json"
	"io"
	"net"
	"net/http"
	"regexp"
	"strconv"
	"strings"
	"time"
)

func guard(f http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		k := r.Header.Get("X-Phosphor-Key")
		if k == "" {
			k = r.URL.Query().Get("k")
		}
		if !strings.HasPrefix(r.Host, "127.0.0.1:") || subtle.ConstantTimeCompare([]byte(k), []byte(sessionKey)) != 1 {
			http.Error(w, "forbidden", http.StatusForbidden)
			return
		}
		f(w, r)
	}
}

func writeJSON(w http.ResponseWriter, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-store")
	_ = json.NewEncoder(w).Encode(v)
}

func fail(w http.ResponseWriter, msg string) { writeJSON(w, map[string]any{"ok": false, "error": msg}) }

func readJSON(r *http.Request, v any) error {
	return json.NewDecoder(io.LimitReader(r.Body, 1<<20)).Decode(v)
}

func routes(mux *http.ServeMux) {
	h := func(p string, f http.HandlerFunc) { mux.HandleFunc(p, guard(f)) }
	h("/api/hello", func(w http.ResponseWriter, r *http.Request) { writeJSON(w, map[string]any{"ok": true}) })
	h("/api/state", apiState)
	h("/api/discover", func(w http.ResponseWriter, r *http.Request) {
		writeJSON(w, map[string]any{"ok": true, "found": discover()})
	})
	h("/api/pair", apiPair)
	h("/api/forget", func(w http.ResponseWriter, r *http.Request) {
		_ = setPairing("", "", "", "")
		writeJSON(w, map[string]any{"ok": true})
	})
	h("/api/theme", apiTheme)
	h("/api/explore", func(w http.ResponseWriter, r *http.Request) { // the SS1's shared folders in Explorer
		if !openShare() {
			fail(w, "not paired")
			return
		}
		writeJSON(w, map[string]any{"ok": true})
	})
	h("/api/ping", func(w http.ResponseWriter, r *http.Request) { // the page, every few seconds while it's shown
		warn, all := unseenWarnings()
		writeJSON(w, map[string]any{"ok": true, "view": takeView(), "warn": warn, "unseen": all})
	})
	h("/api/notes", func(w http.ResponseWriter, r *http.Request) { // GET the list; POST {do: seen|clear}
		if r.Method == http.MethodPost {
			var req struct{ Do string }
			_ = readJSON(r, &req)
			switch req.Do {
			case "seen":
				markSeen()
			case "clear":
				clearNotes()
			}
		}
		notes.Lock()
		l := append([]note{}, notes.list...)
		notes.Unlock()
		cfgMu.Lock()
		quiet := cfg.QuietPC
		cfgMu.Unlock()
		writeJSON(w, map[string]any{"ok": true, "notes": l, "quiet_pc": quiet})
	})
	h("/api/quiet", func(w http.ResponseWriter, r *http.Request) { // Windows notifications on / off
		var req struct{ Quiet bool }
		if readJSON(r, &req) != nil {
			fail(w, "bad request")
			return
		}
		cfgMu.Lock()
		cfg.QuietPC = req.Quiet
		cfgMu.Unlock()
		_ = saveConfig()
		writeJSON(w, map[string]any{"ok": true})
	})
	h("/api/reconnect", func(w http.ResponseWriter, r *http.Request) { // GET: the setting; POST {auto} or {now: true}
		if r.Method == http.MethodPost {
			var req struct {
				Auto *bool
				Now  bool
			}
			_ = readJSON(r, &req)
			if req.Auto != nil {
				cfgMu.Lock()
				cfg.NoReconnect = !*req.Auto
				cfgMu.Unlock()
				_ = saveConfig()
			}
			if req.Now {
				if keyWorksAt(func() string { cfgMu.Lock(); defer cfgMu.Unlock(); return cfg.Address }()) {
					writeJSON(w, map[string]any{"ok": true, "found": true, "moved": false})
					return
				}
				a, ok := relocate()
				writeJSON(w, map[string]any{"ok": true, "found": ok, "moved": ok, "address": a})
				return
			}
		}
		writeJSON(w, map[string]any{"ok": true, "auto": autoReconnect()})
	})
	h("/api/upload", apiUpload) // a Phosphor package to the SS1 (Settings > Update Phosphor)
	h("/api/quit", func(w http.ResponseWriter, r *http.Request) { // Settings > About > Exit Phosphor
		writeJSON(w, map[string]any{"ok": true})
		go func() { time.Sleep(200 * time.Millisecond); quitApp() }()
	})
	h("/api/call", apiCall)
	h("/api/sshkeys", apiSSHKeys) // this PC's SSH keys, to offer to the SS1 (yes on the TV)
	h("/api/art/", apiArt)
}

func apiState(w http.ResponseWriter, r *http.Request) {
	cfgMu.Lock()
	c := cfg
	cfgMu.Unlock()
	st := map[string]any{"ok": true, "app_version": appVersion, "address": c.Address, "name": c.Name,
		"paired": c.TokenEnc != "", "accent": c.Accent, "texture": c.Texture}
	if c.Address != "" && c.TokenEnc != "" {
		if b, code, err := call(json.RawMessage(`{"cmd":"status"}`)); err == nil && code == 200 {
			st["connected"] = true
			st["status"] = json.RawMessage(b)
		} else {
			st["connected"] = false
			if err != nil {
				st["error"] = err.Error()
				st["forgotten"] = err == errForgotten
			}
		}
	}
	writeJSON(w, st)
}

func apiPair(w http.ResponseWriter, r *http.Request) {
	var req struct{ Address, Code string }
	if readJSON(r, &req) != nil || req.Address == "" || req.Code == "" {
		fail(w, "give the SS1's address and the code on the TV")
		return
	}
	addr := withPort(req.Address)
	in, err := getInfo(addr, 4*time.Second)
	if err != nil {
		fail(w, "no Phosphor at "+addr+": check the address on the TV (Settings > Remote access)")
		return
	}
	id, tok, err := pair(addr, req.Code)
	if err != nil {
		fail(w, err.Error())
		return
	}
	if err := setPairing(addr, in.Name, id, tok); err != nil {
		fail(w, "paired, but couldn't save it: "+err.Error())
		return
	}
	writeJSON(w, map[string]any{"ok": true})
}

var accents = map[string]bool{"amber": true, "red": true, "green": true, "blue": true, "purple": true}

func apiTheme(w http.ResponseWriter, r *http.Request) {
	var req struct {
		Accent  string `json:"accent"`
		Texture *int   `json:"texture"`
	}
	if readJSON(r, &req) != nil {
		fail(w, "bad request")
		return
	}
	cfgMu.Lock()
	if accents[req.Accent] {
		cfg.Accent = req.Accent
	}
	if req.Texture != nil && *req.Texture >= 0 && *req.Texture < 64 {
		cfg.Texture = *req.Texture
	}
	cfgMu.Unlock()
	_ = saveConfig()
	writeJSON(w, map[string]any{"ok": true})
}

// apiCall: a phosphord command, passed through (the SS1 decides what a paired device may do)
func apiCall(w http.ResponseWriter, r *http.Request) {
	body, err := io.ReadAll(io.LimitReader(r.Body, 1<<20))
	if err != nil || !json.Valid(body) {
		fail(w, "bad request")
		return
	}
	b, code, err := call(body)
	if err != nil {
		writeJSON(w, map[string]any{"ok": false, "error": err.Error(), "offline": true, "forgotten": err == errForgotten})
		return
	}
	if code != 200 && !json.Valid(b) {
		fail(w, "the SS1 answered "+strconv.Itoa(code))
		return
	}
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-store")
	_, _ = w.Write(b)
}

// apiArt: a game's box art, from the SS1 (GET) or to it (PUT)
func apiArt(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(strings.TrimPrefix(r.URL.Path, "/api/art/"))
	if err != nil || id < 0 {
		http.NotFound(w, r)
		return
	}
	switch r.Method {
	case "GET":
		resp, err := authed("GET", "/art/"+strconv.Itoa(id), nil, 20*time.Second)
		if err != nil {
			http.Error(w, err.Error(), http.StatusBadGateway)
			return
		}
		defer resp.Body.Close()
		if resp.StatusCode != 200 {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", "image/jpeg")
		w.Header().Set("Cache-Control", "private, max-age=300")
		_, _ = io.Copy(w, io.LimitReader(resp.Body, 16<<20))
	case "PUT":
		body, err := io.ReadAll(io.LimitReader(r.Body, 16<<20))
		if err != nil || len(body) == 0 {
			fail(w, "no image")
			return
		}
		resp, err := authed("PUT", "/art/"+strconv.Itoa(id), strings.NewReader(string(body)), 60*time.Second)
		if err != nil {
			fail(w, err.Error())
			return
		}
		defer resp.Body.Close()
		w.Header().Set("Content-Type", "application/json")
		_, _ = io.Copy(w, io.LimitReader(resp.Body, 1<<20))
	default:
		http.Error(w, "method", http.StatusMethodNotAllowed)
	}
}

// apiUpload: a Phosphor release package from this PC to the SS1 (phosphor/updates), for Settings >
// Update Phosphor. ?name=phosphor-<version>-armhf.tar.gz, the file as the body.
func apiUpload(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "method", http.StatusMethodNotAllowed)
		return
	}
	name := r.URL.Query().Get("name")
	if !regexp.MustCompile(`^phosphor-[A-Za-z0-9._-]+-armhf\.tar\.gz$`).MatchString(name) {
		fail(w, "That isn't a Phosphor package (phosphor-<version>-armhf.tar.gz)")
		return
	}
	body, err := io.ReadAll(io.LimitReader(r.Body, 64<<20))
	if err != nil || len(body) < 1024 {
		fail(w, "couldn't read the file")
		return
	}
	resp, err := authed("PUT", "/update/"+name, bytes.NewReader(body), 5*time.Minute)
	if err != nil {
		fail(w, err.Error())
		return
	}
	defer resp.Body.Close()
	w.Header().Set("Content-Type", "application/json")
	_, _ = io.Copy(w, io.LimitReader(resp.Body, 1<<20))
}

// openShare: the SS1's Samba share (\\<its address>) in File Explorer; false when not paired
func openShare() bool {
	cfgMu.Lock()
	host := cfg.Address
	cfgMu.Unlock()
	if i := strings.LastIndex(host, ":"); i > 0 {
		host = host[:i]
	}
	if net.ParseIP(host) == nil {
		return false
	}
	openFolder(`\\` + host)
	return true
}
