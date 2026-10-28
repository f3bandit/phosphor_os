// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// Phosphor for Windows updating itself. It looks at Phosphor's releases on GitHub (a minute after it
// starts, then every 6 hours) for a Phosphor_win_<version>.zip newer than this copy, listed in the
// release's SHA256SUMS.txt. A newer one is a notification; Settings > About installs it: the zip is
// downloaded and checked against SHA256SUMS, its Phosphor.exe goes next to this one, this one is
// renamed to Phosphor.exe.old (Windows lets a running exe be renamed, not replaced), the new one takes
// its name and starts, and asks this one to quit (replaceOlder). The .old copy is deleted at the next
// start. Nothing is sent anywhere: it only reads the public release list.

import (
	"archive/zip"
	"bufio"
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"
)

const appReleases = "https://api.github.com/repos/f3bandit/phosphor_os/releases?per_page=10"

var appZipRe = regexp.MustCompile(`^Phosphor_win_(\d+(?:\.\d+)+)\.zip$`)

type appUpd struct {
	Latest  string `json:"latest,omitempty"`  // the newest Phosphor for Windows released
	Newer   bool   `json:"newer"`             // newer than this copy
	Page    string `json:"page,omitempty"`    // its release page
	Checked int64  `json:"checked,omitempty"` // unix seconds
	State   string `json:"state,omitempty"`   // "" | working | failed | done
	Msg     string `json:"msg,omitempty"`
	zip     string // its download address
	sums    string // SHA256SUMS.txt's
}

var appUpdate struct {
	sync.Mutex
	s appUpd
}

// newerVersion: a is newer than b ("0.2.44" > "0.2.43")
func newerVersion(a, b string) bool {
	x, y := strings.Split(a, "."), strings.Split(b, ".")
	for i := 0; i < len(x) || i < len(y); i++ {
		var p, q int
		if i < len(x) {
			p, _ = strconv.Atoi(x[i])
		}
		if i < len(y) {
			q, _ = strconv.Atoi(y[i])
		}
		if p != q {
			return p > q
		}
	}
	return false
}

var updClient = &http.Client{Timeout: 30 * time.Second}

func ghGet(url string) (*http.Response, error) {
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", "Phosphor for Windows/"+appVersion)
	req.Header.Set("Accept", "application/vnd.github+json")
	return updClient.Do(req)
}

// appCheck: the newest Phosphor for Windows on GitHub
func appCheck() error {
	r, err := ghGet(appReleases)
	if err != nil {
		return errors.New("GitHub didn't answer: check the PC's internet connection")
	}
	defer r.Body.Close()
	if r.StatusCode != 200 {
		return fmt.Errorf("GitHub answered %d", r.StatusCode)
	}
	var rel []struct {
		Draft  bool   `json:"draft"`
		Page   string `json:"html_url"`
		Assets []struct {
			Name string `json:"name"`
			URL  string `json:"browser_download_url"`
		} `json:"assets"`
	}
	if err := json.NewDecoder(io.LimitReader(r.Body, 4<<20)).Decode(&rel); err != nil {
		return errors.New("GitHub's answer couldn't be read")
	}
	best := appUpd{}
	for _, x := range rel {
		if x.Draft {
			continue
		}
		var v, zipURL, sums string
		for _, a := range x.Assets {
			if m := appZipRe.FindStringSubmatch(a.Name); m != nil {
				v, zipURL = m[1], a.URL
			} else if a.Name == "SHA256SUMS.txt" {
				sums = a.URL
			}
		}
		if v != "" && sums != "" && (best.Latest == "" || newerVersion(v, best.Latest)) {
			best = appUpd{Latest: v, Page: x.Page, zip: zipURL, sums: sums}
		}
	}
	appUpdate.Lock()
	defer appUpdate.Unlock()
	appUpdate.s.Checked = time.Now().Unix()
	if best.Latest == "" {
		return errors.New("no Phosphor for Windows release found")
	}
	appUpdate.s.Latest, appUpdate.s.Page, appUpdate.s.zip, appUpdate.s.sums = best.Latest, best.Page, best.zip, best.sums
	appUpdate.s.Newer = newerVersion(best.Latest, appVersion)
	return nil
}

// watchAppUpdates: a minute after starting, then every 6 hours; a newer one is a notification (once)
func watchAppUpdates() {
	appCleanOld()
	time.Sleep(time.Minute)
	for {
		if appCheck() == nil {
			appUpdate.Lock()
			s := appUpdate.s
			appUpdate.Unlock()
			if s.Newer {
				addNote("appupd-"+s.Latest, "info", "Phosphor for Windows "+s.Latest+" is out",
					"You have "+appVersion+". Settings > About updates it (Phosphor restarts by itself).", "settings/about")
			}
		}
		time.Sleep(6 * time.Hour)
	}
}

func appSetState(st, msg string) {
	appUpdate.Lock()
	appUpdate.s.State, appUpdate.s.Msg = st, msg
	appUpdate.Unlock()
}

// appInstall: download, check, put in place, start the new one (which closes this one)
func appInstall() error {
	appUpdate.Lock()
	s := appUpdate.s
	busy := s.State == "working"
	if !busy {
		appUpdate.s.State, appUpdate.s.Msg = "working", "Downloading Phosphor for Windows "+s.Latest+"..."
	}
	appUpdate.Unlock()
	if busy {
		return errors.New("an update is already running")
	}
	if !s.Newer || s.zip == "" {
		appSetState("", "")
		return errors.New("this is the newest Phosphor for Windows")
	}
	go func() {
		if err := appDoInstall(s); err != nil {
			appSetState("failed", err.Error())
		}
	}()
	return nil
}

func appDoInstall(s appUpd) error {
	exe, err := os.Executable()
	if err != nil {
		return errors.New("couldn't find where Phosphor.exe is")
	}
	if p, err := filepath.EvalSymlinks(exe); err == nil {
		exe = p
	}
	dir := filepath.Dir(exe)
	// the checksum first: SHA256SUMS.txt lists the zip
	r, err := ghGet(s.sums)
	if err != nil || r.StatusCode != 200 {
		if r != nil {
			r.Body.Close()
		}
		return errors.New("couldn't get the release's checksums")
	}
	want := ""
	name := "Phosphor_win_" + s.Latest + ".zip"
	sc := bufio.NewScanner(io.LimitReader(r.Body, 1<<20))
	for sc.Scan() {
		f := strings.Fields(sc.Text())
		if len(f) == 2 && strings.TrimPrefix(f[1], "*") == name {
			want = strings.ToLower(f[0])
		}
	}
	r.Body.Close()
	if want == "" {
		return errors.New("the release's checksums don't list " + name)
	}
	r, err = ghGet(s.zip)
	if err != nil || r.StatusCode != 200 {
		if r != nil {
			r.Body.Close()
		}
		return errors.New("the download didn't start: check the PC's internet connection")
	}
	data, err := io.ReadAll(io.LimitReader(r.Body, 200<<20))
	r.Body.Close()
	if err != nil {
		return errors.New("the download didn't finish")
	}
	sum := sha256.Sum256(data)
	if hex.EncodeToString(sum[:]) != want {
		return errors.New("the download was damaged (checksum): try again")
	}
	zr, err := zip.NewReader(bytes.NewReader(data), int64(len(data)))
	if err != nil {
		return errors.New("the download isn't a zip")
	}
	var newExe []byte
	for _, f := range zr.File {
		if strings.EqualFold(filepath.Base(f.Name), "Phosphor.exe") {
			rc, err := f.Open()
			if err != nil {
				return errors.New("couldn't read Phosphor.exe in the zip")
			}
			newExe, err = io.ReadAll(io.LimitReader(rc, 100<<20))
			rc.Close()
			if err != nil {
				return errors.New("couldn't read Phosphor.exe in the zip")
			}
		}
	}
	if len(newExe) < 2 || newExe[0] != 'M' || newExe[1] != 'Z' {
		return errors.New("the zip has no Phosphor.exe")
	}
	appSetState("working", "Installing Phosphor for Windows "+s.Latest+"...")
	tmp := exe + ".new"
	if err := os.WriteFile(tmp, newExe, 0o755); err != nil {
		return errors.New("couldn't write next to Phosphor.exe (" + dir + "): move Phosphor to a folder you can write to, or download it from the release page")
	}
	old := exe + ".old"
	os.Remove(old)
	if err := os.Rename(exe, old); err != nil {
		os.Remove(tmp)
		return errors.New("couldn't set this copy aside: close other Phosphor windows and try again")
	}
	if err := os.Rename(tmp, exe); err != nil {
		_ = os.Rename(old, exe) // put this one back
		os.Remove(tmp)
		return errors.New("couldn't put the new Phosphor.exe in place")
	}
	appSetState("done", "Phosphor for Windows "+s.Latest+" is installed: it's starting")
	cmd := exec.Command(exe)
	cmd.Dir = dir
	if err := cmd.Start(); err != nil {
		return errors.New("installed: start Phosphor again to use " + s.Latest)
	}
	// the new copy asks this one to quit (replaceOlder); if it doesn't within a while, quit anyway
	go func() {
		time.Sleep(20 * time.Second)
		quitApp()
	}()
	return nil
}

// appCleanOld: the copy set aside by the last update
func appCleanOld() {
	if exe, err := os.Executable(); err == nil {
		for i := 0; i < 10; i++ { // (the old one may still be closing)
			if err := os.Remove(exe + ".old"); err == nil || os.IsNotExist(err) {
				return
			}
			time.Sleep(time.Second)
		}
	}
}

// /api/app_update: GET the state; POST {do: check|install}
func apiAppUpdate(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodPost {
		var req struct{ Do string }
		_ = readJSON(r, &req)
		switch req.Do {
		case "check":
			if err := appCheck(); err != nil {
				fail(w, err.Error())
				return
			}
		case "install":
			if err := appInstall(); err != nil {
				fail(w, err.Error())
				return
			}
		default:
			fail(w, "bad request")
			return
		}
	}
	appUpdate.Lock()
	s := appUpdate.s
	appUpdate.Unlock()
	writeJSON(w, map[string]any{"ok": true, "version": appVersion, "update": s})
}
