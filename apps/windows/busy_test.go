package main

import (
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync/atomic"
	"testing"
)

// a busy SS1 (503 with Retry-After) is asked again; any other 503 isn't
func TestAuthedRetriesBusy(t *testing.T) {
	var n, other atomic.Int32
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		b, _ := io.ReadAll(r.Body)
		if r.URL.Path == "/other" {
			other.Add(1)
			w.WriteHeader(503)
			io.WriteString(w, `{"ok":false,"error":"phosphord didn't answer"}`)
			return
		}
		if n.Add(1) <= 2 {
			w.Header().Set("Retry-After", "1")
			w.WriteHeader(503)
			io.WriteString(w, `{"ok":false,"error":"busy: try again"}`)
			return
		}
		io.WriteString(w, `{"ok":true,"echo":`+string(b)+`}`)
	}))
	defer srv.Close()
	enc, err := protect("tok")
	if err != nil {
		t.Skip("no token store here:", err)
	}
	cfgMu.Lock()
	saved := cfg
	cfg.Address, cfg.TokenEnc = strings.TrimPrefix(srv.URL, "http://"), enc
	cfgMu.Unlock()
	defer func() { cfgMu.Lock(); cfg = saved; cfgMu.Unlock() }()

	var out struct {
		Echo struct{ Cmd string } `json:"echo"`
	}
	if err := ss1Call(map[string]any{"cmd": "ping"}, &out); err != nil {
		t.Fatal(err)
	}
	if n.Load() != 3 || out.Echo.Cmd != "ping" {
		t.Fatalf("tries %d, body sent again %q", n.Load(), out.Echo.Cmd)
	}
	r, err := authed("POST", "/other", strings.NewReader("{}"), 0)
	if err != nil {
		t.Fatal(err)
	}
	r.Body.Close()
	if other.Load() != 1 || r.StatusCode != 503 {
		t.Fatalf("other 503 sent %d times", other.Load())
	}
}
