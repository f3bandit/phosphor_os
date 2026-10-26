// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync/atomic"
	"testing"
)

// the proof as phosphord makes it (system/phosphord/remote.c): sha256hex(sha256hex(key) ":" nonce)
func TestKeyProof(t *testing.T) {
	if p := keyProof("abc123", "00112233445566778899aabbccddeeff"); p != "c723f68927e5b119fa03d8b0e0338e51dca917e04a076905f3583b40e3c33874" {
		t.Fatal(p)
	}
}

// a host on the network gets the key only after it proved it knows it; no proofs (older SS1) or a 403: never
func TestProvesKey(t *testing.T) {
	cfgMu.Lock()
	old := cfg.TokenEnc
	cfg.TokenEnc, _ = protect("secret-key")
	cfgMu.Unlock()
	defer func() { cfgMu.Lock(); cfg.TokenEnc = old; cfgMu.Unlock() }()
	for _, c := range []struct {
		name   string
		proofs func(nonce string) []string
		gen    int
		want   bool
	}{
		{"right proof", func(n string) []string { return []string{keyProof("other", n), keyProof("secret-key", n)} }, 200, true},
		{"wrong proof", func(n string) []string { return []string{keyProof("other", n)} }, 200, false},
		{"no proofs", func(string) []string { return nil }, 200, false},
		{"proof, gen 403", func(n string) []string { return []string{keyProof("secret-key", n)} }, 403, false},
	} {
		var sawKey atomic.Bool
		srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			if r.Header.Get("Authorization") != "" {
				sawKey.Store(true)
			}
			if r.URL.Path == "/info" {
				n := r.URL.Query().Get("nonce")
				if len(n) != 32 || strings.ToLower(n) != n {
					t.Errorf("%s: nonce %q", c.name, n)
				}
				_ = json.NewEncoder(w).Encode(map[string]any{"ok": true, "name": "Phosphor", "proofs": c.proofs(n)})
				return
			}
			w.WriteHeader(c.gen)
		}))
		addr := strings.TrimPrefix(srv.URL, "http://")
		got := provesKey(addr) && keyWorksAt(addr)
		srv.Close()
		if got != c.want {
			t.Errorf("%s: adopted %v", c.name, got)
		}
		if !strings.HasPrefix(c.name, "right") && !strings.HasSuffix(c.name, "403") && sawKey.Load() {
			t.Errorf("%s: the key was sent", c.name)
		}
	}
}

func TestSysbkNames(t *testing.T) {
	for _, id := range []string{"MiSTer.ini", "linux+u-boot.txt", "MiSTer_alt_1.ini"} {
		if !sbSafeID(id) {
			t.Error("refused", id)
		}
	}
	for _, id := range []string{"", "..", "../x", `a\b`, "a/b", "c:x", ".x", "x.", " x", "a\x00b"} {
		if sbSafeID(id) {
			t.Errorf("allowed %q", id)
		}
	}
	if !sbSafeHash(strings.Repeat("ab", 32)) || sbSafeHash(strings.Repeat("AB", 32)) || sbSafeHash("../"+strings.Repeat("a", 61)) || sbSafeHash("abc") {
		t.Error("hash check")
	}
}

// putting back while a backup runs: refused, with a message for the page
func TestSysbkRestoreBusy(t *testing.T) {
	sb.Lock()
	sb.busy, sb.job = true, "save"
	sb.Unlock()
	defer func() { sb.Lock(); sb.busy, sb.job = false, ""; sb.Unlock() }()
	if err := sbRestore("MiSTer.ini", strings.Repeat("a", 64)); err == nil || !strings.Contains(err.Error(), "backup is running") {
		t.Fatal(err)
	}
}
