// Phosphor Link: picking a way to reach the MiSTer, keeping it open, and healing it when it dies.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"net"
	"os"
	"path/filepath"
	"strings"
	"sync"
)

// sshHost: host= from the ini, or else the SS1 the Phosphor app is paired with.
func sshHost(cfg Config) string {
	if cfg.Host != "" {
		return cfg.Host
	}
	d, err := os.UserConfigDir()
	if err != nil {
		return ""
	}
	b, err := os.ReadFile(filepath.Join(d, "Phosphor", "config.json"))
	if err != nil {
		return ""
	}
	var c struct {
		Address string `json:"address"`
	}
	if json.Unmarshal(b, &c) != nil || c.Address == "" {
		return ""
	}
	if h, _, err := net.SplitHostPort(c.Address); err == nil {
		return h
	}
	return c.Address
}

func connect(cfg Config) (Link, error) {
	switch strings.ToLower(cfg.Transport) {
	case "serial":
		return OpenSerial(cfg)
	case "ssh":
		cfg.Host = sshHost(cfg)
		return OpenSSH(cfg)
	}
	// auto: SSH first when the SS1 is on the network (fast, no console noise, big files are quick),
	// the debug port otherwise (works with no network, and shows the boot)
	var herr error
	if h := sshHost(cfg); h != "" {
		c := cfg
		c.Host = h
		l, err := OpenSSH(c)
		if err == nil {
			return l, nil
		}
		herr = err
	}
	s, serr := OpenSerial(cfg)
	if serr == nil {
		return s, nil
	}
	if herr == nil {
		return nil, fmt.Errorf("serial: %v (SSH not tried: no host= in phosphor-link.ini and no paired Phosphor app)", serr)
	}
	return nil, fmt.Errorf("ssh: %v; serial: %v", herr, serr)
}

// Checker is optional: a link that can tell whether it still answers.
type Checker interface {
	Alive() bool
}

// session keeps one link open between calls, plus the serial console on demand when the main
// link is SSH (for the boot log and reboots), and reopens either one if it dies.
type session struct {
	mu     sync.Mutex
	cfg    Config
	link   Link
	serial *SerialLink
}

func (s *session) get() (Link, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.link != nil {
		if c, ok := s.link.(Checker); ok && !c.Alive() {
			log.Printf("watchdog: %s stopped answering, reconnecting", s.link.Name())
			_ = s.link.Close()
			s.link = nil
		} else {
			return s.link, nil
		}
	}
	l, err := connect(s.cfg)
	if err != nil {
		return nil, err
	}
	s.link = l
	return l, nil
}

// console: the serial link, for the tools only it can do. Reuses the main link when that is serial.
func (s *session) console() (*SerialLink, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if sl, ok := s.link.(*SerialLink); ok {
		return sl, nil
	}
	if s.serial != nil && s.serial.Alive() {
		return s.serial, nil
	}
	if s.serial != nil {
		_ = s.serial.Close()
		s.serial = nil
	}
	sl, err := OpenSerial(s.cfg)
	if err != nil {
		return nil, fmt.Errorf("%v: %w", errNotSerial, err)
	}
	s.serial = sl
	return sl, nil
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

func (s *session) dropConsole() {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.serial != nil {
		_ = s.serial.Close()
		s.serial = nil
	}
	if _, ok := s.link.(*SerialLink); ok {
		_ = s.link.Close()
		s.link = nil
	}
}

func (s *session) close() {
	s.drop()
	s.dropConsole()
}

var errNotSerial = errors.New("this needs the serial debug port (the dock's USB-C debug port connected to this PC)")
