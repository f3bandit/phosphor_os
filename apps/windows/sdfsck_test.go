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

// a repair that fixes something but leaves the card dirty runs again by itself (fsck.exfat stops at a problem
// and only clears the mark after a run that got through): a stand-in fsck.exfat, fixing in its first pass and
// clearing the mark in its second
func TestFsckCardPasses(t *testing.T) {
	dir := t.TempDir()
	img := filepath.Join(dir, "card.img")
	b := make([]byte, 4096)
	copy(b[3:], "EXFAT   ")
	b[106] = 2
	os.WriteFile(img, b, 0o644)
	fake := filepath.Join(dir, "fsck.sh")
	os.WriteFile(fake, []byte(`#!/bin/sh
n=$(cat "`+dir+`/n" 2>/dev/null || echo 0); n=$((n+1)); echo $n > "`+dir+`/n"
for t; do :; done
echo "pass $n"
if [ $n -ge 2 ]; then printf '\000' | dd of="$t" bs=1 seek=106 conv=notrunc 2>/dev/null; exit 0; fi
exit 1
`), 0o755)
	t.Setenv("PHOS_TEST_FSCK", fake)
	t.Setenv("PHOS_TEST_DISK", img)
	sd = sdState{}
	if err := fsckCard(disk{Number: 7, FriendlyName: "Test card reader"}, "repair"); err != nil {
		t.Fatal(err)
	}
	if n, _ := os.ReadFile(filepath.Join(dir, "n")); string(n) != "2\n" {
		t.Fatalf("passes: %q", n)
	}
	if sd.DirtyAfter != "clean" || sd.Message != "Problems found and fixed. (2 passes)" {
		t.Fatalf("%+v", sd)
	}
	// errors left for Deep repair: one pass, and the error says so
	os.WriteFile(filepath.Join(dir, "n"), []byte("0\n"), 0o644)
	os.WriteFile(fake, []byte("#!/bin/sh\necho left\nexit 4\n"), 0o755)
	if err := fsckCard(disk{Number: 7}, "repair"); err == nil {
		t.Fatal("errors left, no error")
	}
}
