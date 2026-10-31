// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// End to end: Look up on this PC against a real phosphord (a PC build of it), with a pretend box art
// source, while the app's page keeps asking the SS1 how things are going, as it does.
//
//	PHOSD_BIN=/path/to/phosphord go test -run TestE2E -v
//
// Skipped without PHOSD_BIN. CI (.github/workflows/tests.yml) builds phosphord for the PC and runs it.
// PHOS_E2E_NO_RETRY=1 turns off sending "busy" requests again: the run should then fail, which shows
// the test still catches what n00dlez ran into (0.27.58).

import (
	"bytes"
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

const e2eGames = 60

// e2eSS1: a phosphord on a pretend SD card with e2eGames SNES games, remote access on, this test paired
func e2eSS1(t *testing.T) (addr, token string) {
	bin := os.Getenv("PHOSD_BIN")
	if bin == "" {
		t.Skip("PHOSD_BIN not set (a PC build of phosphord)")
	}
	root := t.TempDir()
	fat, tmp := filepath.Join(root, "fat"), filepath.Join(root, "tmp")
	games := filepath.Join(fat, "games", "SNES")
	for _, d := range []string{games, filepath.Join(fat, "phosphor", "config"), tmp} {
		if err := os.MkdirAll(d, 0o755); err != nil {
			t.Fatal(err)
		}
	}
	for i := 1; i <= e2eGames; i++ {
		b := make([]byte, 4096)
		rand.Read(b)
		if err := os.WriteFile(filepath.Join(games, fmt.Sprintf("E2E Game %d (USA).sfc", i)), b, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	port := l.Addr().(*net.TCPAddr).Port
	l.Close()
	tb := make([]byte, 32)
	rand.Read(tb)
	token = hex.EncodeToString(tb)
	h := sha256.Sum256([]byte(token))
	now := time.Now().Unix()
	ini := fmt.Sprintf("on=1\nport=%d\ndevice=%s %d %d E2E test\n", port, hex.EncodeToString(h[:]), now, now)
	if err := os.WriteFile(filepath.Join(fat, "phosphor", "config", "remote.ini"), []byte(ini), 0o600); err != nil {
		t.Fatal(err)
	}

	var log bytes.Buffer
	cmd := exec.Command(bin)
	cmd.Dir = root
	cmd.Env = append(os.Environ(), "PHOS_FAT="+fat, "PHOS_TMP="+tmp, "PHOS_GAMES_ROOTS="+filepath.Join(fat, "games"),
		"PHOS_NO_REBOOT=1", "PHOS_CORENAME="+filepath.Join(tmp, "CORENAME"), "PHOS_MISTER_CMD="+filepath.Join(tmp, "MiSTer_cmd"))
	cmd.Stdout, cmd.Stderr = &log, &log
	if err := cmd.Start(); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() {
		cmd.Process.Kill()
		cmd.Wait()
		if t.Failed() {
			t.Logf("phosphord's log:\n%s", log.String())
		}
	})
	addr = fmt.Sprintf("127.0.0.1:%d", port)
	for i := 0; ; i++ {
		if r, err := http.Get("http://" + addr + "/info"); err == nil {
			r.Body.Close()
			break
		}
		if i > 100 {
			t.Fatal("phosphord didn't start")
		}
		time.Sleep(100 * time.Millisecond)
	}
	return addr, token
}

// a box art picture, and a source that finds every game and points at it
func e2eSource(t *testing.T) (scrapeSource, *atomic.Int32) {
	img := image.NewRGBA(image.Rect(0, 0, 300, 420))
	for y := 0; y < 420; y++ {
		for x := 0; x < 300; x++ {
			img.Set(x, y, color.RGBA{uint8(x), uint8(y), 120, 255})
		}
	}
	var pic bytes.Buffer
	png.Encode(&pic, img)
	var served atomic.Int32
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		served.Add(1)
		w.Header().Set("Content-Type", "image/png")
		w.Write(pic.Bytes())
	}))
	t.Cleanup(srv.Close)
	return scrapeSource{"libretro", "E2E source", func(*scrapeRun) bool { return true },
		func(ctx context.Context, s *scrapeRun, g *sgame, r *sres) int {
			for i, v := range []string{"A game for the test.", "E2E Dev", "E2E Pub", "Action", "1994", "1", "USA"} {
				r.set(i, v)
			}
			r.addArt("e2e", srv.URL+"/box/"+fmt.Sprint(g.ID)+".png")
			return 1
		}}, &served
}

func TestE2ELookUpOnThisPC(t *testing.T) {
	addr, token := e2eSS1(t)
	enc, err := protect(token)
	if err != nil {
		t.Skip("no token store here:", err)
	}
	cfgMu.Lock()
	saved := cfg
	cfg.Address, cfg.TokenEnc = addr, enc
	cfgMu.Unlock()
	src, served := e2eSource(t)
	oldSources, oldTries := scrapeSources, busyTries
	scrapeSources = []scrapeSource{src}
	if os.Getenv("PHOS_E2E_NO_RETRY") != "" {
		busyTries = 0
	}
	fanart.Lock() // no fan art list from GitHub: none of these games are in it
	fanart.byKey, fanart.at = map[string]*fanGame{}, time.Now()
	fanart.Unlock()
	t.Cleanup(func() {
		cfgMu.Lock()
		cfg = saved
		cfgMu.Unlock()
		scrapeSources, busyTries = oldSources, oldTries
		fanart.Lock()
		fanart.byKey = nil
		fanart.Unlock()
	})

	// the library first, as on a SuperStation that has scanned its games
	if err := ss1Call(map[string]any{"cmd": "rescan"}, nil); err != nil {
		t.Fatal("rescan:", err)
	}
	for i := 0; ; i++ {
		var todo struct{ Total int }
		if err := ss1Call(map[string]any{"cmd": "scrape_todo", "start": true, "limit": 1}, &todo); err != nil {
			t.Fatal("scrape_todo:", err)
		}
		if todo.Total == e2eGames {
			break
		}
		if i > 100 {
			t.Fatalf("the library has %d games waiting, not %d", todo.Total, e2eGames)
		}
		time.Sleep(100 * time.Millisecond)
	}

	// the app's page: it keeps asking the SS1 while the run goes on (two at once, to be sure of it)
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Minute)
	defer cancel()
	stopPoll := make(chan struct{})
	var wg sync.WaitGroup
	var polls, pollFails atomic.Int32
	var pollErr atomic.Value
	for p := 0; p < 2; p++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for {
				select {
				case <-stopPoll:
					return
				default:
				}
				if err := ss1Call(map[string]any{"cmd": "scrape_progress"}, nil); err != nil {
					pollFails.Add(1)
					pollErr.Store(err.Error())
				}
				polls.Add(1)
				time.Sleep(5 * time.Millisecond)
			}
		}()
	}

	retriesBefore := busyRetries.Load()
	s := &scrapeRun{ctx: ctx, cancel: cancel}
	s.st = scrapeState{Running: true, Status: "Starting", Started: time.Now().Unix()}
	all := s.lookupAll()
	close(stopPoll)
	wg.Wait()

	s.mu.Lock()
	st := s.st
	s.mu.Unlock()
	t.Logf("notes: %q", st.Notes)
	t.Logf("status %q: %d found, %d not found, %d later; %d pictures served; page asked %d times (%d failed); %d busy answers sent again",
		st.Status, st.Found, st.NotFound, st.Later, served.Load(), polls.Load(), pollFails.Load(), busyRetries.Load()-retriesBefore)
	if !all || strings.HasPrefix(st.Status, "The SS1") || strings.HasPrefix(st.Status, "No connection") {
		t.Fatalf("the run stopped: %q", st.Status)
	}
	if st.Found != e2eGames {
		t.Fatalf("%d of %d games found", st.Found, e2eGames)
	}
	if pollFails.Load() > 0 {
		t.Fatalf("the page's questions failed %d times: %v", pollFails.Load(), pollErr.Load())
	}

	// on the SS1: nothing left to look up, and every game has its box art
	var todo struct{ Total int }
	if err := ss1Call(map[string]any{"cmd": "scrape_todo", "start": true, "limit": 1}, &todo); err != nil {
		t.Fatal(err)
	}
	if todo.Total != 0 {
		t.Fatalf("%d games still waiting on the SS1", todo.Total)
	}
	for id := 1; id <= e2eGames; id++ {
		r, err := authed("GET", fmt.Sprintf("/art/%d", id), nil, 20*time.Second)
		if err != nil {
			t.Fatal(err)
		}
		b, _ := io.ReadAll(r.Body)
		r.Body.Close()
		if r.StatusCode != 200 || len(b) < 3 || b[0] != 0xFF || b[1] != 0xD8 {
			t.Fatalf("game %d: no box art on the SS1 (HTTP %d, %d bytes)", id, r.StatusCode, len(b))
		}
	}
}
