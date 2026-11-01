// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// RetroSpy (retro-spy.com) on this PC: Settings > Controllers > RetroSpy finds it and opens it with the
// SS1's address and MiSTer's login already typed in. RetroSpy keeps its settings in .NET's user.config
// (%LOCALAPPDATA%\RetroSpy*\RetroSpy_Url_*\<version>\user.config): Hostname, MisterUsername and
// MisterPassword are written there before it starts (only when RetroSpy isn't already running, which would
// write its own over them). The source (MiSTer) and the controller number are picked in RetroSpy: its
// source list is the user's own (some hidden, reordered), so it isn't guessed here.
//
// RetroSpy's Phosphor edition (f3bandit/RetroSpy, GPL-3.0, a separate program and its own release) is installed
// into RetroSpy\ next to Phosphor.exe from this page (retrospy_install.go) and then updates itself. That one is
// opened with --phosphor <address> --go: it finds the SS1's active controller itself and draws it as Phosphor does.

import (
	"errors"
	"html"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

// the edition bundled with Phosphor: RetroSpy\RetroSpy.exe next to Phosphor.exe
func retrospyBundled() string {
	exe, err := os.Executable()
	if err != nil {
		return ""
	}
	if p, err := filepath.EvalSymlinks(exe); err == nil {
		exe = p
	}
	p := filepath.Join(filepath.Dir(exe), "RetroSpy", "RetroSpy.exe")
	if st, err := os.Stat(p); err == nil && !st.IsDir() {
		return p
	}
	return ""
}

func retrospyExe() string {
	if b := retrospyBundled(); b != "" {
		return b
	}
	var dirs []string
	for _, e := range []string{"ProgramFiles", "ProgramW6432", "ProgramFiles(x86)", "LOCALAPPDATA", "APPDATA"} {
		if v := os.Getenv(e); v != "" {
			dirs = append(dirs, filepath.Join(v, "RetroSpy"), filepath.Join(v, "Programs", "RetroSpy"))
		}
	}
	for _, d := range dirs {
		p := filepath.Join(d, "RetroSpy.exe")
		if st, err := os.Stat(p); err == nil && !st.IsDir() {
			return p
		}
	}
	return ""
}

// the newest of RetroSpy's user.config files, "" when it has never saved its settings
func retrospyConfig() string {
	la := os.Getenv("LOCALAPPDATA")
	if la == "" {
		return ""
	}
	m, _ := filepath.Glob(filepath.Join(la, "RetroSpy*", "RetroSpy*", "*", "user.config"))
	if len(m) == 0 {
		return ""
	}
	sort.Slice(m, func(i, j int) bool {
		a, _ := os.Stat(m[i])
		b, _ := os.Stat(m[j])
		return a != nil && b != nil && a.ModTime().After(b.ModTime())
	})
	return m[0]
}

// sets one <setting name="K" serializeAs="String"><value>V</value></setting>, adding it when it's missing
func setUserConfig(doc, key, val string) string {
	v := html.EscapeString(val)
	re := regexp.MustCompile(`(?s)(<setting name="` + regexp.QuoteMeta(key) + `"[^>]*>\s*<value>)(.*?)(</value>)`)
	if re.MatchString(doc) {
		return re.ReplaceAllStringFunc(doc, func(s string) string {
			p := re.FindStringSubmatch(s)
			return p[1] + v + p[3]
		})
	}
	sec := "</RetroSpy.Properties.Settings>"
	if i := strings.Index(doc, sec); i >= 0 {
		add := `    <setting name="` + key + `" serializeAs="String">` + "\r\n" + `                <value>` + v + `</value>` + "\r\n" + `            </setting>` + "\r\n        "
		return doc[:i] + add + doc[i:]
	}
	return doc
}

func retrospyRunning() bool {
	out, err := hidden(exec.Command("tasklist", "/FI", "IMAGENAME eq RetroSpy.exe", "/NH")).Output()
	return err == nil && strings.Contains(strings.ToLower(string(out)), "retrospy.exe")
}

func retrospyOpen(host, user, pass string, player int) error {
	if b := retrospyBundled(); b != "" {
		args := []string{"--phosphor", host, "--go"}
		if player > 0 {
			args = append(args, "--player", strconv.Itoa(player))
		}
		cmd := hidden(exec.Command(b, args...))
		cmd.Dir = filepath.Dir(b)
		if err := cmd.Start(); err != nil {
			return err
		}
		go cmd.Wait()
		return nil
	}
	exe := retrospyExe()
	if exe == "" {
		return errors.New("RetroSpy isn't installed on this PC: get it from retro-spy.com")
	}
	filled := false
	if cfg := retrospyConfig(); cfg != "" && !retrospyRunning() {
		if b, err := os.ReadFile(cfg); err == nil {
			doc := string(b)
			doc = setUserConfig(doc, "Hostname", host)
			doc = setUserConfig(doc, "MisterUsername", user)
			doc = setUserConfig(doc, "MisterPassword", pass)
			if doc != string(b) {
				tmp := cfg + ".phosphor"
				if os.WriteFile(tmp, []byte(doc), 0o644) == nil && os.Rename(tmp, cfg) == nil {
					filled = true
				}
			} else {
				filled = true
			}
		}
	}
	cmd := hidden(exec.Command(exe))
	cmd.Dir = filepath.Dir(exe)
	if err := cmd.Start(); err != nil {
		return err
	}
	go cmd.Wait()
	if !filled {
		return errNotFilled
	}
	return nil
}

var errNotFilled = errors.New("not filled")

func apiRetrospy(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodGet {
		out := map[string]any{"ok": true, "found": retrospyExe() != "", "edition": retrospyBundled() != "", "running": retrospyRunning(),
			"version": retrospyVersion()}
		if r.URL.Query().Get("latest") != "" {
			if rel, err := retrospyLatest(r.URL.Query().Get("latest") == "force"); err == nil {
				out["latest"], out["page"] = rel.Version, rel.Page
			} else {
				out["latest_error"] = err.Error()
			}
		}
		writeJSON(w, out)
		return
	}
	if r.URL.Query().Get("do") == "install" {
		v, err := retrospyInstall()
		if err != nil {
			fail(w, err.Error())
			return
		}
		writeJSON(w, map[string]any{"ok": true, "version": v})
		return
	}
	var q struct {
		Host, User, Password string
		Player               int
	}
	if err := readJSON(r, &q); err != nil {
		fail(w, "bad request")
		return
	}
	err := retrospyOpen(q.Host, q.User, q.Password, q.Player)
	if err == errNotFilled {
		writeJSON(w, map[string]any{"ok": true, "filled": false})
		return
	}
	if err != nil {
		fail(w, err.Error())
		return
	}
	writeJSON(w, map[string]any{"ok": true, "filled": true})
}
