// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// The SD card write (partition table last, then everything read back and compared) and the read
// test, on any device path, so they can be tested on a plain file. sdcard_windows.go opens the card.

import (
	"archive/zip"
	"crypto/sha256"
	"errors"
	"fmt"
	"io"
	"os"
	"strings"
	"time"
)

// imageSize / openImage: a .img, or the biggest file in a .zip
func openImage(p string) (io.ReadCloser, int64, func(), error) {
	if strings.HasSuffix(strings.ToLower(p), ".zip") {
		zr, err := zip.OpenReader(p)
		if err != nil {
			return nil, 0, nil, err
		}
		var best *zip.File
		for _, f := range zr.File {
			if !f.FileInfo().IsDir() && (best == nil || f.UncompressedSize64 > best.UncompressedSize64) {
				best = f
			}
		}
		if best == nil {
			zr.Close()
			return nil, 0, nil, errors.New("the zip is empty")
		}
		rc, err := best.Open()
		if err != nil {
			zr.Close()
			return nil, 0, nil, err
		}
		return rc, int64(best.UncompressedSize64), func() { zr.Close() }, nil
	}
	f, err := os.Open(p)
	if err != nil {
		return nil, 0, nil, err
	}
	st, _ := f.Stat()
	if st.Size() < 1<<20 {
		f.Close()
		return nil, 0, nil, errors.New("too small to be an SD card image")
	}
	return f, st.Size(), func() {}, nil
}

func imageSize(p string) (io.ReadCloser, int64, func(), error) {
	rc, n, closer, err := openImage(p)
	if err != nil {
		return nil, 0, nil, err
	}
	rc.Close()
	return nil, n, closer, nil
}

type rawDev interface {
	io.ReaderAt
	io.WriterAt
	Sync() error
}

// writeImage: the image onto dev, the first 4 MB (the partition table) last so Windows doesn't try to
// open half-written partitions, then the whole of it read back and compared
func writeImage(f rawDev, image string, sector int) error {
	if sector <= 0 {
		sector = 512
	}
	rc, size, closer, err := openImage(image)
	if err != nil {
		return err
	}
	defer closer()
	defer rc.Close()
	const chunk = 4 << 20
	buf := make([]byte, chunk)
	var first []byte
	var off, got int64 // got: the image's bytes read (off adds padding to whole sectors)
	hasher := sha256.New()
	sdSet(func(s *sdState) {
		s.Phase, s.Done, s.Total, s.Message, s.Image = "write", 0, size, "Writing the image", image
	})
	start := time.Now()
	for {
		n, rerr := io.ReadFull(rc, buf)
		got += int64(n)
		if n > 0 {
			data := buf[:n]
			if rem := n % sector; rem != 0 {
				data = append(data, make([]byte, sector-rem)...)
			}
			hasher.Write(data)
			if off == 0 {
				first = append([]byte(nil), data...)
			} else if _, err := f.WriteAt(data, off); err != nil {
				return fmt.Errorf("writing failed at %d MB: %v (the card may be faulty or locked)", off>>20, err)
			}
			off += int64(len(data))
			o := off
			sdSet(func(s *sdState) { s.Done = o; s.MBps = mbps(o, start) })
		}
		if rerr == io.EOF || rerr == io.ErrUnexpectedEOF { // the last piece, short: the end, if it's all there
			if got < size {
				return fmt.Errorf("the image ended early (%d of %d MB): it may be damaged or still downloading", got>>20, size>>20)
			}
			break
		}
		if rerr != nil {
			return fmt.Errorf("reading the image failed: %v", rerr)
		}
	}
	if first != nil {
		if _, err := f.WriteAt(first, 0); err != nil {
			return fmt.Errorf("writing the partition table failed: %v", err)
		}
	}
	if err := f.Sync(); err != nil {
		return fmt.Errorf("finishing the write failed: %v", err)
	}
	want := hasher.Sum(nil)
	sdSet(func(s *sdState) {
		s.Phase, s.Done, s.Total, s.Message, s.MBps = "verify", 0, off, "Reading it back to check", 0
	})
	vh := sha256.New()
	start = time.Now()
	for pos := int64(0); pos < off; {
		n := int64(chunk)
		if off-pos < n {
			n = off - pos
		}
		if _, err := f.ReadAt(buf[:n], pos); err != nil && err != io.EOF {
			return fmt.Errorf("reading it back failed at %d MB: %v", pos>>20, err)
		}
		vh.Write(buf[:n])
		pos += n
		p := pos
		sdSet(func(s *sdState) { s.Done = p; s.MBps = mbps(p, start) })
	}
	if string(vh.Sum(nil)) != string(want) {
		return errors.New("The check failed: what was read back from the card isn't what was written. The card may be faulty (or fake): try another card")
	}
	return nil
}

// readTestDev: every part of the card read once (nothing written): speed, and the parts that can't be read
func readTestDev(f io.ReaderAt, size int64) error {
	const chunk = 4 << 20
	buf := make([]byte, chunk)
	var bad []int64
	sdSet(func(s *sdState) { s.Phase, s.Done, s.Total, s.Message = "read", 0, size, "Reading the whole card" })
	start := time.Now()
	slowest := 0.0
	for pos := int64(0); pos < size; {
		n := int64(chunk)
		if size-pos < n {
			n = size - pos
		}
		t := time.Now()
		if _, err := f.ReadAt(buf[:n], pos); err != nil && err != io.EOF {
			bad = append(bad, pos>>20)
			if len(bad) >= 64 {
				b := bad
				sdSet(func(s *sdState) { s.Bad = b })
				return fmt.Errorf("Stopped: more than 60 parts of the card can't be read. The card is failing: copy anything you need off it and replace it")
			}
		} else if d := time.Since(t).Seconds(); d > 0 {
			if v := float64(n) / d / 1e6; slowest == 0 || v < slowest {
				slowest = v
			}
		}
		pos += n
		p, b := pos, bad
		sdSet(func(s *sdState) { s.Done = p; s.MBps = mbps(p, start); s.Bad = b })
	}
	avg := mbps(size, start)
	msg := fmt.Sprintf("Every part read: %.0f MB/s on average.", avg)
	if len(bad) > 0 {
		msg = fmt.Sprintf("%d part(s) of the card couldn't be read: it's failing. Copy anything you need off it and replace it.", len(bad))
	} else if avg < 10 {
		msg += " That's slow for an SD card: games load slowly from it, and it may be worn or fake."
	} else {
		msg += " The card reads fine."
	}
	sdSet(func(s *sdState) { s.Message, s.MBps = msg, avg })
	if len(bad) > 0 {
		return errors.New(msg)
	}
	return nil
}

func mbps(n int64, start time.Time) float64 {
	d := time.Since(start).Seconds()
	if d <= 0 {
		return 0
	}
	return float64(n) / d / 1e6
}
