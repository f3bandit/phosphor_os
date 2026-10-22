// Phosphor Link: picking a way to reach the MiSTer, and keeping one connection open.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package main

import (
	"errors"
	"fmt"
	"strings"
	"sync"
)

func connect(cfg Config) (Link, error) {
	switch strings.ToLower(cfg.Transport) {
	case "serial":
		return OpenSerial(cfg)
	case "ssh":
		return OpenSSH(cfg)
	}
	// auto: the debug port first (works with no network and shows the boot), then SSH
	s, serr := OpenSerial(cfg)
	if serr == nil {
		return s, nil
	}
	if cfg.Host == "" {
		return nil, fmt.Errorf("serial: %v (SSH not tried: no host= in phosphor-link.ini)", serr)
	}
	h, herr := OpenSSH(cfg)
	if herr == nil {
		return h, nil
	}
	return nil, fmt.Errorf("serial: %v; ssh: %v", serr, herr)
}

// session keeps one link open between calls and reopens it if it breaks.
type session struct {
	mu   sync.Mutex
	cfg  Config
	link Link
}

func (s *session) get() (Link, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.link != nil {
		return s.link, nil
	}
	l, err := connect(s.cfg)
	if err != nil {
		return nil, err
	}
	s.link = l
	return l, nil
}

// drop forgets a link that failed, so the next call reconnects.
func (s *session) drop() {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.link != nil {
		_ = s.link.Close()
		s.link = nil
	}
}

func (s *session) close() { s.drop() }

var errNotSerial = errors.New("this needs the serial debug port (connected over SSH now)")
