// Copyright (C) 2026 f3bandit. All rights reserved; licensed to Retro Remake (see LICENSE.md).

package main

// RetroSpy's Phosphor edition is its own package (f3bandit/RetroSpy, GPL-3.0, a separate program) and keeps
// itself up to date (its Help > Check for updates, and a banner when there's a newer one). Phosphor for Windows
// only puts it in place: Settings > Controllers > RetroSpy installs it into RetroSpy\ next to Phosphor.exe, or
// updates a copy that's older than the newest release (one from before it updated itself: 6.10.1 came inside
// Phosphor's zip). Phosphor's own update doesn't touch RetroSpy\ any more.
//
// Its release zip (RetroSpy-Phosphor-edition-win64.zip, folder RetroSpy\) is checked against the release's
// SHA256SUMS.txt, then put in place the way the bundled one was (appUpdateRetroSpy: RetroSpy.new, then the swap).
// RetroSpy\version.txt (in its zips from 6.11.0) says which version is installed.

import (
	"archive/zip"
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"io"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

const rsRepo = "f3bandit/RetroSpy"
const rsAsset = "RetroSpy-Phosphor-edition-win64.zip"

type rsRelease struct {
	Version, Tag, Zip, Sums, Page string
}

var rsLatest struct {
	sync.Mutex
	rel *rsRelease
	at  time.Time
	err string
}

// the installed edition's version: RetroSpy\version.txt; "" when it isn't installed, "6.10.1" for one from
// before version.txt (the bundled one, or older)
func retrospyVersion() string {
	b := retrospyBundled()
	if b == "" {
		return ""
	}
	if v, err := os.ReadFile(filepath.Join(filepath.Dir(b), "version.txt")); err == nil && len(strings.TrimSpace(string(v))) > 0 {
		return strings.TrimSpace(string(v))
	}
	return "6.10.1"
}

// the newest release of the edition (phosphor-<version>, not a draft or a pre-release, with its zip and
// checksums); asked again after 30 minutes
func retrospyLatest(force bool) (*rsRelease, error) {
	rsLatest.Lock()
	defer rsLatest.Unlock()
	if !force && time.Since(rsLatest.at) < 30*time.Minute {
		if rsLatest.err != "" {
			return nil, errors.New(rsLatest.err)
		}
		return rsLatest.rel, nil
	}
	rsLatest.at = time.Now()
	rel, err := retrospyAsk()
	rsLatest.rel, rsLatest.err = rel, ""
	if err != nil {
		rsLatest.err = err.Error()
	}
	return rel, err
}

func retrospyAsk() (*rsRelease, error) {
	resp, err := ghGet("https://api.github.com/repos/" + rsRepo + "/releases?per_page=30")
	if err != nil {
		return nil, errors.New("couldn't reach GitHub")
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return nil, errors.New("GitHub didn't answer (" + resp.Status + ")")
	}
	var rels []struct {
		Tag        string `json:"tag_name"`
		Draft      bool   `json:"draft"`
		Prerelease bool   `json:"prerelease"`
		Page       string `json:"html_url"`
		Assets     []struct {
			Name string `json:"name"`
			URL  string `json:"browser_download_url"`
		} `json:"assets"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&rels); err != nil {
		return nil, errors.New("GitHub's answer couldn't be read")
	}
	var best *rsRelease
	for _, r := range rels {
		if r.Draft || r.Prerelease || !strings.HasPrefix(r.Tag, "phosphor-") {
			continue
		}
		x := &rsRelease{Version: strings.TrimPrefix(r.Tag, "phosphor-"), Tag: r.Tag, Page: r.Page}
		for _, a := range r.Assets {
			switch a.Name {
			case rsAsset:
				x.Zip = a.URL
			case "SHA256SUMS.txt":
				x.Sums = a.URL
			}
		}
		if x.Zip == "" || x.Sums == "" {
			continue
		}
		if best == nil || newerVersion(x.Version, best.Version) {
			best = x
		}
	}
	if best == nil {
		return nil, errors.New("no release of RetroSpy's Phosphor edition found")
	}
	return best, nil
}

// downloads the newest release, checks it, and puts it in RetroSpy\ next to Phosphor.exe
func retrospyInstall() (string, error) {
	if retrospyRunning() {
		return "", errors.New("RetroSpy is open: close it and try again")
	}
	rel, err := retrospyLatest(true)
	if err != nil {
		return "", err
	}
	sums, err := ghGet(rel.Sums)
	if err != nil {
		return "", errors.New("couldn't download the checksums")
	}
	sb, _ := io.ReadAll(io.LimitReader(sums.Body, 1<<16))
	sums.Body.Close()
	want := ""
	for _, l := range strings.Split(string(sb), "\n") {
		f := strings.Fields(strings.ReplaceAll(l, "*", " "))
		if len(f) >= 2 && f[len(f)-1] == rsAsset {
			want = strings.ToLower(f[0])
		}
	}
	if want == "" {
		return "", errors.New("the release has no checksum for " + rsAsset)
	}
	resp, err := ghGetLong(rel.Zip)
	if err != nil {
		return "", errors.New("couldn't download RetroSpy")
	}
	data, err := io.ReadAll(io.LimitReader(resp.Body, 400<<20))
	resp.Body.Close()
	if err != nil || resp.StatusCode != 200 {
		return "", errors.New("RetroSpy's download didn't finish")
	}
	sum := sha256.Sum256(data)
	if hex.EncodeToString(sum[:]) != want {
		return "", errors.New("the download didn't match the release's checksum, so it wasn't installed")
	}
	zr, err := zip.NewReader(bytes.NewReader(data), int64(len(data)))
	if err != nil {
		return "", errors.New("RetroSpy's download isn't a zip")
	}
	exe, err := os.Executable()
	if err != nil {
		return "", err
	}
	if p, err := filepath.EvalSymlinks(exe); err == nil {
		exe = p
	}
	if note := appUpdateRetroSpy(zr, filepath.Dir(exe)); note != "" {
		return "", errors.New(strings.Trim(strings.TrimSpace(note), "()"))
	}
	if retrospyBundled() == "" {
		return "", errors.New("RetroSpy couldn't be put next to Phosphor.exe")
	}
	return rel.Version, nil
}
