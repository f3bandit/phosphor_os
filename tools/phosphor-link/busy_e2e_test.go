// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// End to end against a real phosphord (a PC build of it): with every connection the MiSTer serves from this
// address taken (as Phosphor for Windows looking up box art does), the library tools wait their turn instead
// of failing with "busy: try again" (0.27.58).
//
//	PHOSD_BIN=/path/to/phosphord go test -run TestE2EBusy -v
//
// Skipped without PHOSD_BIN. CI (.github/workflows/tests.yml) builds phosphord for the PC and runs it.

import (
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
	"time"
)

func TestE2EBusy(t *testing.T) {
	bin := os.Getenv("PHOSD_BIN")
	if bin == "" {
		t.Skip("PHOSD_BIN not set (a PC build of phosphord)")
	}
	root := t.TempDir()
	fat, tmp := filepath.Join(root, "fat"), filepath.Join(root, "tmp")
	for _, d := range []string{filepath.Join(fat, "phosphor", "config"), filepath.Join(fat, "games"), tmp} {
		os.MkdirAll(d, 0o755)
	}
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	port := l.Addr().(*net.TCPAddr).Port
	l.Close()
	tb := make([]byte, 32)
	rand.Read(tb)
	token := hex.EncodeToString(tb)
	h := sha256.Sum256([]byte(token))
	now := time.Now().Unix()
	os.WriteFile(filepath.Join(fat, "phosphor", "config", "remote.ini"),
		[]byte(fmt.Sprintf("on=1\nport=%d\ndevice=%s %d %d E2E test\n", port, hex.EncodeToString(h[:]), now, now)), 0o600)
	cmd := exec.Command(bin)
	cmd.Dir = root
	cmd.Env = append(os.Environ(), "PHOS_FAT="+fat, "PHOS_TMP="+tmp, "PHOS_GAMES_ROOTS="+filepath.Join(fat, "games"),
		"PHOS_NO_REBOOT=1", "PHOS_CORENAME="+filepath.Join(tmp, "CORENAME"), "PHOS_MISTER_CMD="+filepath.Join(tmp, "MiSTer_cmd"))
	if err := cmd.Start(); err != nil {
		t.Fatal(err)
	}
	defer func() { cmd.Process.Kill(); cmd.Wait() }()
	addr := fmt.Sprintf("127.0.0.1:%d", port)
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
	c := libConfig{Address: addr, Token: token}

	// every connection the MiSTer serves from one address: open, saying nothing (it gives up on them after
	// 3 seconds, as on a slow network)
	var held []net.Conn
	for i := 0; i < 6; i++ {
		conn, err := net.Dial("tcp", addr)
		if err != nil {
			t.Fatal(err)
		}
		held = append(held, conn)
	}
	defer func() {
		for _, conn := range held {
			conn.Close()
		}
	}()
	time.Sleep(200 * time.Millisecond) // all six taken

	start := time.Now()
	r, err := libAPI(c, map[string]any{"cmd": "status"})
	if err != nil {
		t.Fatalf("with every connection taken: %v", err)
	}
	waited := time.Since(start)
	t.Logf("answered after %v: %v", waited.Round(time.Millisecond), r["version"])
	if waited < time.Second {
		t.Fatalf("answered after %v: the connections weren't all taken, so this didn't test the wait", waited)
	}
}
