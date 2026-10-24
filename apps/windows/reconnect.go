package main

// Reconnecting by itself: when the SS1 stops answering at its address (a restart, or the router gave it
// a new address), Phosphor looks on the home network for it and moves over, without pairing again.
// Only the SS1 this PC is paired with can answer with this PC's key, so another Phosphor on the same
// network is never picked up. On by default (Settings > Connection).

import (
	"bytes"
	"io"
	"net/http"
	"sync"
	"time"
)

var relocMu sync.Mutex

// keyWorksAt: does the SS1 at addr know this PC's key? (one cheap request, nothing changed)
func keyWorksAt(addr string) bool {
	tok := token()
	if tok == "" {
		return false
	}
	req, err := http.NewRequest("POST", "http://"+withPort(addr)+"/api", bytes.NewReader([]byte(`{"cmd":"gen"}`)))
	if err != nil {
		return false
	}
	req.Header.Set("Authorization", "Bearer "+tok)
	r, err := client(3 * time.Second).Do(req)
	if err != nil {
		return false
	}
	_, _ = io.Copy(io.Discard, io.LimitReader(r.Body, 4096))
	r.Body.Close()
	return r.StatusCode == 200 || r.StatusCode == 403 // 403: an older SS1 that knows the key but not "gen" from afar
}

func autoReconnect() bool {
	cfgMu.Lock()
	defer cfgMu.Unlock()
	return !cfg.NoReconnect
}

// relocate: find this PC's SS1 on the network; true (and the new address saved) when it moved
func relocate() (string, bool) {
	relocMu.Lock()
	defer relocMu.Unlock()
	cfgMu.Lock()
	cur := cfg.Address
	cfgMu.Unlock()
	if cur == "" || token() == "" {
		return "", false
	}
	for _, f := range discover() {
		a := withPort(f.Address)
		if a == withPort(cur) {
			continue
		}
		if keyWorksAt(a) {
			cfgMu.Lock()
			cfg.Address = a
			cfgMu.Unlock()
			_ = saveConfig()
			return a, true
		}
	}
	return "", false
}
