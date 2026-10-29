// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// Tools > Card health: check and repair the exFAT file system on an SD card in this PC's reader, with
// exfatprogs' fsck.exfat built for Windows (fsck-exfat/: GPL-2.0, its own program, run as a separate
// process). Windows' own scan (chkdsk) often leaves the card marked dirty and dumps FOUND.000 folders
// on it; fsck.exfat is the tool Linux's exFAT driver is maintained alongside, the same one Phosphor runs
// on the SS1's NVMe and USB drives at start-up.
//
//   check   fsck.exfat -n   reports, changes nothing
//   repair  fsck.exfat -p   repairs what's safe to repair without asking (and clears the dirty mark)
//   deep    fsck.exfat -y   repairs everything it finds (broken entries are deleted)
//
// Runs in the administrator helper (opening a volume directly needs it), like the flash.

import (
	"bufio"
	"bytes"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"
	"time"
)

// exFAT's boot sector: "EXFAT   " at 3, VolumeFlags at 106 (bit 1 dirty, bit 2 media failure)
func exfatState(target string) (string, error) {
	f, err := os.Open(target)
	if err != nil {
		return "", err
	}
	defer f.Close()
	b := make([]byte, 512)
	if _, err := f.ReadAt(b, 0); err != nil && err != io.EOF {
		return "", err
	}
	if !bytes.Equal(b[3:11], []byte("EXFAT   ")) {
		return "not-exfat", nil
	}
	switch fl := b[106]; {
	case fl&4 != 0:
		return "media-failure", nil
	case fl&2 != 0:
		return "dirty", nil
	}
	return "clean", nil
}

var fsckFlags = map[string][]string{"check": {"-n"}, "repair": {"-p"}, "deep": {"-y"}}

// fsck.exfat's exit codes (exfatprogs fsck.h)
func fsckResult(code int, mode string) (string, error) {
	switch {
	case code == 0:
		return "No problems found.", nil
	case code&8 != 0 || code&16 != 0 || code&128 != 0:
		return "", errors.New("the check couldn't run on this card (see the details)")
	case code&4 != 0 && mode == "check":
		return "Problems found: repair the card to fix them.", nil
	case code&4 != 0:
		return "", errors.New("some problems are still there: try Deep repair")
	case code&1 != 0:
		return "Problems found and fixed.", nil
	}
	return fmt.Sprintf("fsck.exfat finished (code %d).", code), nil
}

func fsckCard(d disk, mode string) error {
	flags, ok := fsckFlags[mode]
	if !ok {
		return errors.New("mode: check, repair or deep")
	}
	target, err := fsckTarget(d)
	if err != nil {
		return err
	}
	before, _ := exfatState(target)
	sdSet(func(s *sdState) { s.Dirty = before })
	if before == "not-exfat" {
		return errors.New("that card isn't exFAT (the SuperStation's card is): nothing was done")
	}
	exe, cleanup, err := fsckExe()
	if err != nil {
		return err
	}
	defer cleanup()
	what := map[string]string{"check": "Checking", "repair": "Repairing", "deep": "Repairing everything on"}[mode]
	sdSet(func(s *sdState) {
		s.Phase, s.Message = "check", fmt.Sprintf("%s %s (%s)", what, fsckName(target), d.FriendlyName)
	})
	cmd := hidden(exec.Command(exe, append(flags, target)...))
	out, _ := cmd.StdoutPipe()
	cmd.Stderr = cmd.Stdout
	if err := cmd.Start(); err != nil {
		return fmt.Errorf("couldn't start the check: %v", err)
	}
	// still here: the status file's time keeps moving while a long, quiet part runs
	tick := time.NewTicker(5 * time.Second)
	done := make(chan struct{})
	go func() {
		for {
			select {
			case <-tick.C:
				sdSet(func(*sdState) {})
			case <-done:
				return
			}
		}
	}()
	sc := bufio.NewScanner(out)
	for sc.Scan() {
		l := strings.TrimRight(sc.Text(), "\r")
		if strings.TrimSpace(l) == "" || strings.HasPrefix(l, "exfatprogs version") {
			continue
		}
		sdSet(func(s *sdState) {
			if len(s.Log) < 400 {
				s.Log = append(s.Log, l)
			}
		})
	}
	err = cmd.Wait()
	tick.Stop()
	close(done)
	code := 0
	var ee *exec.ExitError
	if errors.As(err, &ee) {
		code = ee.ExitCode()
	} else if err != nil {
		return fmt.Errorf("the check stopped: %v", err)
	}
	after, _ := exfatState(target)
	msg, rerr := fsckResult(code, mode)
	sdSet(func(s *sdState) {
		s.DirtyAfter = after
		s.Message = msg
		switch {
		case mode != "check" && after == "dirty" && rerr == nil:
			s.Message += " The card is still marked dirty."
		case mode != "check" && before == "dirty" && after == "clean" && code == 0:
			s.Message = "Nothing was broken: the card was only marked dirty, and the mark is cleared."
		case mode == "check" && after == "dirty" && code == 0:
			s.Message = "Nothing is broken, but the card is marked dirty (Windows offers to scan it): Repair clears the mark."
		}
	})
	return rerr
}
