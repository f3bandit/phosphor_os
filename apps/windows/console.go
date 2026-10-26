// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// Talking to the SS1: Phosphor's remote access (docs/remote-api.md). Plain HTTP on the home network,
// one request per connection, the pairing key as a Bearer token.

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"
)

const remotePort = "8767"

var (
	errNotPaired = errors.New("not paired")
	errForgotten = errors.New("the SS1 doesn't know this PC any more: pair again")
)

func client(timeout time.Duration) *http.Client {
	return &http.Client{Timeout: timeout, Transport: &http.Transport{DisableKeepAlives: true, Proxy: nil}}
}

func withPort(addr string) string {
	addr = strings.TrimSpace(strings.TrimPrefix(strings.TrimPrefix(addr, "http://"), "https://"))
	addr = strings.TrimSuffix(addr, "/")
	if addr != "" && !strings.Contains(addr, ":") {
		addr += ":" + remotePort
	}
	return addr
}

type info struct {
	OK      bool   `json:"ok"`
	Name    string `json:"name"`
	Version string `json:"version"`
	API     int    `json:"api"`
}

// getInfo: is there a Phosphor at addr? (no key needed)
func getInfo(addr string, timeout time.Duration) (info, error) {
	var in info
	r, err := client(timeout).Get("http://" + withPort(addr) + "/info")
	if err != nil {
		return in, err
	}
	defer r.Body.Close()
	if err := json.NewDecoder(io.LimitReader(r.Body, 4096)).Decode(&in); err != nil || !in.OK || in.Name != "Phosphor" {
		return in, errors.New("not a Phosphor")
	}
	return in, nil
}

// pair: the code from the TV for a key
func pair(addr, code string) (id, tok string, err error) {
	host, _ := os.Hostname()
	if host == "" {
		host = "Windows PC"
	}
	body, _ := json.Marshal(map[string]string{"code": strings.TrimSpace(code), "name": host + " (Phosphor for Windows)"})
	r, err := client(8*time.Second).Post("http://"+withPort(addr)+"/pair", "application/json", bytes.NewReader(body))
	if err != nil {
		return "", "", fmt.Errorf("can't reach the SS1: %v", err)
	}
	defer r.Body.Close()
	var out struct {
		OK    bool   `json:"ok"`
		Token string `json:"token"`
		ID    string `json:"id"`
		Error string `json:"error"`
	}
	_ = json.NewDecoder(io.LimitReader(r.Body, 4096)).Decode(&out)
	switch {
	case r.StatusCode == 401:
		return "", "", errors.New("that code didn't work: check it on the TV")
	case r.StatusCode == 403:
		return "", "", errors.New("pairing isn't open: choose Pair a device on the TV first")
	case r.StatusCode == 429:
		return "", "", errors.New("the SS1 already has 16 paired devices: forget one in Settings > Remote access")
	case !out.OK || out.Token == "":
		if out.Error != "" {
			return "", "", errors.New(out.Error)
		}
		return "", "", fmt.Errorf("pairing failed (%d)", r.StatusCode)
	}
	return out.ID, out.Token, nil
}

func authed(method, path string, body io.Reader, timeout time.Duration) (*http.Response, error) {
	cfgMu.Lock()
	addr := cfg.Address
	cfgMu.Unlock()
	tok := token()
	if addr == "" || tok == "" {
		return nil, errNotPaired
	}
	req, err := http.NewRequest(method, "http://"+addr+path, body)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Authorization", "Bearer "+tok)
	r, err := client(timeout).Do(req)
	if err != nil {
		return nil, err
	}
	if r.StatusCode == 401 {
		r.Body.Close()
		return nil, errForgotten
	}
	return r, nil
}

// call: one phosphord command (JSON), its reply (JSON)
func call(cmd json.RawMessage) (json.RawMessage, int, error) {
	r, err := authed("POST", "/api", bytes.NewReader(cmd), 30*time.Second)
	if err != nil {
		return nil, 0, err
	}
	defer r.Body.Close()
	b, err := io.ReadAll(io.LimitReader(r.Body, 32<<20))
	return b, r.StatusCode, err
}
