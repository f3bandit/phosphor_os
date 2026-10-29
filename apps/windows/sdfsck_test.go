package main

import (
	"os"
	"os/exec"
	"path/filepath"
	"testing"
)

// needs PHOS_TEST_FSCK (fsck-exfat/fsck.exfat.exe under wine) and mkfs.exfat: skipped otherwise
func TestFsckCard(t *testing.T) {
	if os.Getenv("PHOS_TEST_FSCK") == "" {
		t.Skip("no PHOS_TEST_FSCK")
	}
	mkfs, err := exec.LookPath("mkfs.exfat")
	if err != nil {
		t.Skip("no mkfs.exfat")
	}
	img := filepath.Join(t.TempDir(), "card.img")
	if err := os.WriteFile(img, nil, 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.Truncate(img, 64<<20); err != nil {
		t.Fatal(err)
	}
	if out, err := exec.Command(mkfs, img).CombinedOutput(); err != nil {
		t.Fatalf("mkfs: %v %s", err, out)
	}
	f, _ := os.OpenFile(img, os.O_RDWR, 0)
	f.WriteAt([]byte{2}, 106) // marked dirty
	f.Close()
	t.Setenv("PHOS_TEST_DISK", img)
	d := disk{Number: 7, FriendlyName: "Test card reader"}

	sd = sdState{}
	if err := fsckCard(d, "check"); err != nil {
		t.Fatal(err)
	}
	if sd.Dirty != "dirty" || sd.DirtyAfter != "dirty" {
		t.Fatalf("check changed the mark: %+v", sd)
	}
	sd = sdState{}
	if err := fsckCard(d, "repair"); err != nil {
		t.Fatal(err)
	}
	if sd.Dirty != "dirty" || sd.DirtyAfter != "clean" {
		t.Fatalf("repair didn't clear the mark: %+v", sd)
	}
	if st, _ := exfatState(img); st != "clean" {
		t.Fatalf("card: %s", st)
	}
	// not exFAT: left alone
	other := filepath.Join(t.TempDir(), "fat.img")
	os.WriteFile(other, make([]byte, 1<<20), 0o644)
	t.Setenv("PHOS_TEST_DISK", other)
	if err := fsckCard(d, "repair"); err == nil {
		t.Fatal("repaired something that isn't exFAT")
	}
	if err := fsckCard(d, "format"); err == nil {
		t.Fatal("unknown mode accepted")
	}
}

func TestFsckResult(t *testing.T) {
	cases := []struct {
		code int
		mode string
		bad  bool
	}{{0, "check", false}, {1, "repair", false}, {4, "check", false}, {4, "repair", true}, {8, "check", true}, {5, "deep", true}}
	for _, c := range cases {
		_, err := fsckResult(c.code, c.mode)
		if (err != nil) != c.bad {
			t.Errorf("%d %s: %v", c.code, c.mode, err)
		}
	}
}
