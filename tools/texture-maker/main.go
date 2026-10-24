// Phosphor Texture Maker: makes Phosphor's sprite background textures from your own pixel-art PNGs.
// A developer tool (not part of Phosphor): the page does the drawing (web/, the same sprite drawing
// as Phosphor's), this program serves it, writes the exports and adds a texture to Phosphor's source.
//
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package main

import (
	"crypto/rand"
	"embed"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"io/fs"
	"log"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strings"
	"sync"
	"time"
)

const appVersion = "1.0.2"

//go:embed web
var webFS embed.FS

var (
	pingMu   sync.Mutex
	lastPing time.Time
	token    string
	cfgMu    sync.Mutex
	cfg      config
)

type config struct {
	Out      string `json:"out"`      // where exports go (a folder per texture)
	Phosphor string `json:"phosphor"` // Phosphor's source (the folder with frontend/ and apps/)
}

func exeDir() string {
	p, err := os.Executable()
	if err != nil {
		return "."
	}
	if r, err := filepath.EvalSymlinks(p); err == nil {
		p = r
	}
	return filepath.Dir(p)
}

func cfgPath() string { return filepath.Join(exeDir(), "texture-maker.json") }

func loadCfg() {
	b, err := os.ReadFile(cfgPath())
	if err == nil {
		_ = json.Unmarshal(b, &cfg)
	}
	if cfg.Out == "" {
		cfg.Out = filepath.Join(exeDir(), "textures")
	}
	if cfg.Phosphor == "" {
		cfg.Phosphor = findPhosphor(exeDir())
	}
}

func saveCfg() {
	b, _ := json.MarshalIndent(cfg, "", "  ")
	_ = os.WriteFile(cfgPath(), b, 0o644)
}

// findPhosphor: Phosphor's source above dir (the tool usually sits in tools/texture-maker)
func findPhosphor(dir string) string {
	for d := dir; ; {
		if isPhosphor(d) {
			return d
		}
		p := filepath.Dir(d)
		if p == d {
			return ""
		}
		d = p
	}
}

func isPhosphor(d string) bool {
	_, e1 := os.Stat(filepath.Join(d, "frontend", "src", "texture.c"))
	_, e2 := os.Stat(filepath.Join(d, "apps", "windows", "web", "texture.js"))
	return e1 == nil && e2 == nil
}

func writeJSON(w http.ResponseWriter, v any) {
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(v)
}

func fail(w http.ResponseWriter, code int, msg string) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	_ = json.NewEncoder(w).Encode(map[string]string{"error": msg})
}

func guard(f http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.Header.Get("X-Token") != token {
			fail(w, 403, "bad token")
			return
		}
		f(w, r)
	}
}

var idRe = regexp.MustCompile(`^[a-z][a-z0-9_]{0,31}$`)

// the files the page made for one texture: name -> text, or a data: URL for pictures
type exportReq struct {
	ID    string            `json:"id"`
	Name  string            `json:"name"`
	Files map[string]string `json:"files"`
}

func (e exportReq) data(name string) ([]byte, error) {
	s, ok := e.Files[name]
	if !ok {
		return nil, fmt.Errorf("%s is missing", name)
	}
	if strings.HasPrefix(s, "data:") {
		i := strings.Index(s, ",")
		if i < 0 {
			return nil, fmt.Errorf("%s: bad picture", name)
		}
		return base64.StdEncoding.DecodeString(s[i+1:])
	}
	return []byte(s), nil
}

func apiConfig(w http.ResponseWriter, r *http.Request) {
	cfgMu.Lock()
	defer cfgMu.Unlock()
	if r.Method == "POST" {
		var in config
		if err := json.NewDecoder(r.Body).Decode(&in); err != nil {
			fail(w, 400, "bad request")
			return
		}
		if in.Out != "" {
			cfg.Out = in.Out
		}
		if in.Phosphor != "" {
			if !isPhosphor(in.Phosphor) {
				fail(w, 400, "That folder isn't Phosphor's source: it needs frontend\\src\\texture.c and apps\\windows\\web\\texture.js.")
				return
			}
			cfg.Phosphor = in.Phosphor
		}
		saveCfg()
	}
	res := map[string]any{"out": cfg.Out, "phosphor": cfg.Phosphor, "version": appVersion}
	if cfg.Phosphor != "" {
		if info, err := phosphorInfo(cfg.Phosphor); err == nil {
			res["info"] = info
		} else {
			res["phosphor_error"] = err.Error()
		}
	}
	writeJSON(w, res)
}

// export: the files go to <out>/<id>/
func apiExport(w http.ResponseWriter, r *http.Request) {
	var e exportReq
	if err := json.NewDecoder(r.Body).Decode(&e); err != nil {
		fail(w, 400, "bad request")
		return
	}
	if !idRe.MatchString(e.ID) {
		fail(w, 400, "bad texture id")
		return
	}
	cfgMu.Lock()
	dir := filepath.Join(cfg.Out, e.ID)
	cfgMu.Unlock()
	if err := os.MkdirAll(dir, 0o755); err != nil {
		fail(w, 500, err.Error())
		return
	}
	var names []string
	for n := range e.Files {
		if strings.ContainsAny(n, `/\:`) || strings.HasPrefix(n, ".") {
			fail(w, 400, "bad file name "+n)
			return
		}
		b, err := e.data(n)
		if err != nil {
			fail(w, 400, err.Error())
			return
		}
		if err := os.WriteFile(filepath.Join(dir, n), b, 0o644); err != nil {
			fail(w, 500, err.Error())
			return
		}
		names = append(names, n)
	}
	sort.Strings(names)
	writeJSON(w, map[string]any{"ok": true, "dir": dir, "files": names})
}

func apiOpen(w http.ResponseWriter, r *http.Request) {
	var in struct{ Path string }
	_ = json.NewDecoder(r.Body).Decode(&in)
	if in.Path == "" {
		fail(w, 400, "no folder")
		return
	}
	openPath(in.Path)
	writeJSON(w, map[string]any{"ok": true})
}

func openPath(p string) {
	switch runtime.GOOS {
	case "windows":
		_ = exec.Command("explorer", p).Start()
	case "darwin":
		_ = exec.Command("open", p).Start()
	default:
		_ = exec.Command("xdg-open", p).Start()
	}
}

func main() {
	loadCfg()
	b := make([]byte, 16)
	_, _ = rand.Read(b)
	token = hex.EncodeToString(b)
	sub, _ := fs.Sub(webFS, "web")
	mux := http.NewServeMux()
	mux.Handle("/", http.FileServer(http.FS(sub)))
	mux.HandleFunc("/api/config", guard(apiConfig))
	mux.HandleFunc("/api/export", guard(apiExport))
	mux.HandleFunc("/api/install", guard(apiInstall))
	mux.HandleFunc("/api/open", guard(apiOpen))
	mux.HandleFunc("/api/last", guard(apiLast))
	// the window pings every few seconds (once a minute when it is minimised); when it has been closed for
	// 3 minutes, the program ends too
	mux.HandleFunc("/api/ping", guard(func(w http.ResponseWriter, r *http.Request) {
		pingMu.Lock()
		lastPing = time.Now()
		pingMu.Unlock()
		writeJSON(w, map[string]any{"ok": true})
	}))
	go func() {
		for range time.Tick(5 * time.Second) {
			pingMu.Lock()
			gone := !lastPing.IsZero() && time.Since(lastPing) > 3*time.Minute
			pingMu.Unlock()
			if gone {
				os.Exit(0)
			}
		}
	}()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		log.Fatal(err)
	}
	url := "http://" + ln.Addr().String() + "/?k=" + token
	fmt.Println("Phosphor Texture Maker " + appVersion + ": " + url)
	if os.Getenv("TEXMAKER_NO_WINDOW") == "" {
		openWindow(url)
	}
	log.Fatal(http.Serve(ln, mux))
}

// the project as it was last: opened again at the next start
func apiLast(w http.ResponseWriter, r *http.Request) {
	p := filepath.Join(exeDir(), "texture-maker-last.ptm")
	if r.Method == "POST" {
		b, err := io.ReadAll(io.LimitReader(r.Body, 64<<20))
		if err != nil {
			fail(w, 400, err.Error())
			return
		}
		_ = os.WriteFile(p+".tmp", b, 0o644)
		_ = os.Rename(p+".tmp", p)
		writeJSON(w, map[string]any{"ok": true})
		return
	}
	b, err := os.ReadFile(p)
	if err != nil {
		writeJSON(w, map[string]any{})
		return
	}
	w.Header().Set("Content-Type", "application/json")
	_, _ = w.Write(b)
}
