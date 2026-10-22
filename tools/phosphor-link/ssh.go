// Phosphor Link over the network (SSH), when the serial port isn't there.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package main

import (
	"bytes"
	"errors"
	"fmt"
	"strings"
	"time"

	"golang.org/x/crypto/ssh"
)

type SSHLink struct {
	host string
	c    *ssh.Client
}

func OpenSSH(cfg Config) (*SSHLink, error) {
	if cfg.Host == "" {
		return nil, errors.New("no host set (host= in phosphor-link.ini)")
	}
	addr := cfg.Host
	if !strings.Contains(addr, ":") {
		addr += ":22"
	}
	conf := &ssh.ClientConfig{
		User: cfg.SSHUser,
		Auth: []ssh.AuthMethod{ssh.Password(cfg.SSHPassword), ssh.KeyboardInteractive(
			func(user, instr string, qs []string, echos []bool) ([]string, error) {
				a := make([]string, len(qs))
				for i := range a {
					a[i] = cfg.SSHPassword
				}
				return a, nil
			})},
		// every stock MiSTer ships the same host keys, so pinning them proves nothing
		HostKeyCallback: ssh.InsecureIgnoreHostKey(),
		Timeout:         8 * time.Second,
	}
	c, err := ssh.Dial("tcp", addr, conf)
	if err != nil {
		return nil, fmt.Errorf("ssh %s: %w", addr, err)
	}
	return &SSHLink{host: addr, c: c}, nil
}

func (l *SSHLink) Name() string { return "ssh " + l.host }

func (l *SSHLink) session() (*ssh.Session, error) { return l.c.NewSession() }

func (l *SSHLink) Run(cmd string, timeout time.Duration) (string, int, error) {
	s, err := l.session()
	if err != nil {
		return "", -1, err
	}
	defer s.Close()
	var out bytes.Buffer
	s.Stdout, s.Stderr = &out, &out
	s.Stdin = strings.NewReader(cmd + "\n")
	done := make(chan error, 1)
	go func() { done <- s.Run("bash -s") }()
	select {
	case err = <-done:
	case <-time.After(timeout):
		_ = s.Signal(ssh.SIGKILL)
		return out.String(), -1, fmt.Errorf("command timed out after %s", timeout)
	}
	if err == nil {
		return out.String(), 0, nil
	}
	var ee *ssh.ExitError
	if errors.As(err, &ee) {
		return out.String(), ee.ExitStatus(), nil
	}
	return out.String(), -1, err
}

func (l *SSHLink) Push(data []byte, remote, mode string) error {
	s, err := l.session()
	if err != nil {
		return err
	}
	defer s.Close()
	if mode == "" {
		mode = "644"
	}
	r := sq(remote)
	s.Stdin = bytes.NewReader(data)
	var out bytes.Buffer
	s.Stdout, s.Stderr = &out, &out
	if err := s.Run("mkdir -p \"$(dirname " + r + ")\" && cat > " + r + ".pltmp && mv -f " + r + ".pltmp " + r + " && chmod " + mode + " " + r + " && md5sum < " + r); err != nil {
		return fmt.Errorf("writing %s: %v %s", remote, err, out.String())
	}
	if !strings.HasPrefix(out.String(), md5hex(data)) {
		return fmt.Errorf("%s arrived damaged", remote)
	}
	return nil
}

func (l *SSHLink) Pull(remote string) ([]byte, error) {
	s, err := l.session()
	if err != nil {
		return nil, err
	}
	defer s.Close()
	var out, errb bytes.Buffer
	s.Stdout, s.Stderr = &out, &errb
	if err := s.Run("cat " + sq(remote)); err != nil {
		return nil, fmt.Errorf("reading %s: %s", remote, strings.TrimSpace(errb.String()))
	}
	return out.Bytes(), nil
}

func (l *SSHLink) Close() error { return l.c.Close() }
