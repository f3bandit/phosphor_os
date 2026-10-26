// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// Box art on the PC: downloads, with the same answers phosphord's net.c gives (0 ok, 1 not there,
// 2 refused, -1 trouble), and a gentle pace per service.

import (
	"context"
	"io"
	"net/http"
	"os"
	"sync"
	"time"
)

var scrapeHTTP = &http.Client{Timeout: 5 * time.Minute}

const scrapeUA = "Phosphor/0.4 (MiSTer frontend; Phosphor for Windows)"

func httpResult(code int) int {
	switch {
	case code >= 200 && code < 300:
		return 0
	case code == 404:
		return 1
	case code == 401 || code == 403:
		return 2
	}
	return -1
}

// netRequest: the body, the result code, the HTTP status
func netRequest(ctx context.Context, method, url string, headers map[string]string, body io.Reader, max int64) ([]byte, int, int) {
	req, err := http.NewRequestWithContext(ctx, method, url, body)
	if err != nil {
		return nil, -1, 0
	}
	req.Header.Set("User-Agent", scrapeUA)
	for k, v := range headers {
		req.Header.Set(k, v)
	}
	r, err := scrapeHTTP.Do(req)
	if err != nil {
		return nil, -1, 0
	}
	defer r.Body.Close()
	b, err := io.ReadAll(io.LimitReader(r.Body, max))
	if err != nil {
		return nil, -1, r.StatusCode
	}
	return b, httpResult(r.StatusCode), r.StatusCode
}

// netGetFile: a download into a file (through .part); 0 ok, 1 not there, -1 trouble
func netGetFile(ctx context.Context, url, dest string, progress func(done, total int64)) int {
	req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
	if err != nil {
		return -1
	}
	req.Header.Set("User-Agent", scrapeUA)
	cl := &http.Client{} // big files: no overall time limit, the context stops it
	r, err := cl.Do(req)
	if err != nil {
		return -1
	}
	defer r.Body.Close()
	if res := httpResult(r.StatusCode); res != 0 {
		return res
	}
	f, err := os.Create(dest + ".part")
	if err != nil {
		return -1
	}
	var done int64
	buf := make([]byte, 256<<10)
	for {
		n, er := r.Body.Read(buf)
		if n > 0 {
			if _, ew := f.Write(buf[:n]); ew != nil {
				f.Close()
				return -1
			}
			done += int64(n)
			if progress != nil {
				progress(done, r.ContentLength)
			}
		}
		if er == io.EOF {
			break
		}
		if er != nil {
			f.Close()
			os.Remove(dest + ".part")
			return -1
		}
	}
	if f.Close() != nil || os.Rename(dest+".part", dest) != nil {
		return -1
	}
	return 0
}

// pace: at most one request every gap, for services that ask for it (ArcadeDB, IGDB: 4 a second)
type pace struct {
	mu   sync.Mutex
	next time.Time
	gap  time.Duration
}

func (p *pace) wait(ctx context.Context) {
	p.mu.Lock()
	now := time.Now()
	at := p.next
	if at.Before(now) {
		at = now
	}
	p.next = at.Add(p.gap)
	p.mu.Unlock()
	if d := time.Until(at); d > 0 {
		select {
		case <-time.After(d):
		case <-ctx.Done():
		}
	}
}
