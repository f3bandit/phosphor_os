// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

//go:build !windows

package main

// Not Windows (tests): PHOS_TEST_DISK=<file> stands in for one card reader; the jobs run in-process.

import (
	"errors"
	"os"
)

func testDisk() (disk, bool) {
	p := os.Getenv("PHOS_TEST_DISK")
	st, err := os.Stat(p)
	if p == "" || err != nil {
		return disk{}, false
	}
	return disk{Number: 7, FriendlyName: "Test card reader", Size: st.Size(), BusType: "USB", LogicalSectorSize: 512, Letters: "E:"}, true
}

func listRemovable() ([]disk, error) {
	if d, ok := testDisk(); ok {
		return []disk{d}, nil
	}
	return []disk{}, nil
}

func diskReport() ([]diskInfo, error) {
	r := []diskInfo{{Number: 0, Name: "PC's own drive", Size: 512 << 30, BusType: "NVMe", Boot: true, Status: "Online", Health: "Healthy", Reason: "Windows runs from this disk"}}
	if d, ok := testDisk(); ok {
		r = append(r, diskInfo{Number: d.Number, Name: d.FriendlyName, Size: d.Size, BusType: d.BusType, Letters: d.Letters, Status: "Online", Health: "Healthy", Offered: true, Reason: "Offered"})
	}
	return r, nil
}

func pickImageFile() (string, error) { return os.Getenv("PHOS_TEST_IMAGE"), nil }

// the job in a goroutine, writing the same status file the helper would
func startHelper(args []string) error {
	go func() {
		defer func() { _ = recover() }()
		opt := map[string]string{}
		for i := 0; i+1 < len(args); i += 2 {
			opt[args[i][2:]] = args[i+1]
		}
		d, _ := testDisk()
		sdMu.Lock()
		sdStatus = opt["status"]
		sdMu.Unlock()
		sdHelperMode = true
		var err error
		if opt["sdjob"] == "flash" {
			err = flash(d, opt["image"])
		} else if opt["sdjob"] == "fsck" {
			err = fsckCard(d, opt["mode"])
		} else {
			err = readTest(d)
		}
		sdSet(func(s *sdState) {
			s.Running = false
			if err != nil {
				s.Phase, s.Error = "failed", err.Error()
			} else {
				s.Phase = "done"
			}
		})
		writeStatus(sd)
		sdHelperMode = false
	}()
	return nil
}

func flash(d disk, image string) error {
	f, err := os.OpenFile(os.Getenv("PHOS_TEST_DISK"), os.O_RDWR, 0)
	if err != nil {
		return errors.New("no test disk")
	}
	defer f.Close()
	return writeImage(f, image, 512)
}

func readTest(d disk) error {
	f, err := os.Open(os.Getenv("PHOS_TEST_DISK"))
	if err != nil {
		return errors.New("no test disk")
	}
	defer f.Close()
	return readTestDev(f, d.Size)
}
