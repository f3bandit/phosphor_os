package main

import (
	"image"
	"image/jpeg"
	"os"
	"path/filepath"
	"testing"
)

func writeJPG(t *testing.T, p string, w, h int) {
	os.MkdirAll(filepath.Dir(p), 0o755)
	f, err := os.Create(p)
	if err != nil {
		t.Fatal(err)
	}
	jpeg.Encode(f, image.NewRGBA(image.Rect(0, 0, w, h)), nil)
	f.Close()
}

// Steam's librarycache/<appid>: a hashed 1000 x 1000 album cover, header.jpg (460 x 215), a 32 x 32 icon
func TestSquareArt(t *testing.T) {
	d := filepath.Join(t.TempDir(), "1101300")
	writeJPG(t, filepath.Join(d, "6ef242d5.jpg"), 32, 32)
	writeJPG(t, filepath.Join(d, "90a87604.jpg"), 1000, 1000)
	writeJPG(t, filepath.Join(d, "header.jpg"), 460, 215)
	writeJPG(t, filepath.Join(d, "x", "library_600x900.jpg"), 600, 900)
	if got := squareArt(d); filepath.Base(got) != "90a87604.jpg" {
		t.Fatalf("got %q", got)
	}
	if got := squareArt(filepath.Join(d, "x")); got != "" {
		t.Fatalf("a poster isn't square: got %q", got)
	}
}

func TestPickCover(t *testing.T) {
	got := pickCover([]img{{"a/booklet01.jpg", 900000}, {"a/Cover.png", 200000}, {"a/x.jpg", 950000}})
	if got != "a/Cover.png" {
		t.Fatalf("got %q", got)
	}
	if got := pickCover([]img{{"a/p1.jpg", 100}, {"a/p2.jpg", 300}}); got != "a/p2.jpg" {
		t.Fatalf("got %q", got)
	}
}
