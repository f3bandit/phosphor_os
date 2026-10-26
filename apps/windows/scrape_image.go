// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// Box art on the PC: the picture made here, the way the SS1 stores it (longest side 512 px, averaged
// down, transparent edges on the shell's dark panel colour, JPEG), so the SS1 only has to save it.

import (
	"bytes"
	"errors"
	"image"
	_ "image/gif"
	"image/jpeg"
	_ "image/png"

	_ "golang.org/x/image/bmp"
	_ "golang.org/x/image/webp"
)

const artBoxMax = 512

func makeBoxArt(data []byte) ([]byte, error) {
	cfg, _, err := image.DecodeConfig(bytes.NewReader(data))
	if err != nil {
		return nil, err
	}
	if cfg.Width < 1 || cfg.Height < 1 || int64(cfg.Width)*int64(cfg.Height) > 5000*5000 {
		return nil, errors.New("too big for box art")
	}
	src, _, err := image.Decode(bytes.NewReader(data))
	if err != nil {
		return nil, err
	}
	b := src.Bounds()
	w, h := b.Dx(), b.Dy()
	nw, nh := w, h
	if w > artBoxMax || h > artBoxMax {
		if w >= h {
			nw, nh = artBoxMax, h*artBoxMax/w
		} else {
			nh, nw = artBoxMax, w*artBoxMax/h
		}
		if nw < 1 {
			nw = 1
		}
		if nh < 1 {
			nh = 1
		}
	}
	// the source as 8-bit RGBA, not premultiplied
	px := make([]uint8, w*h*4)
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			r, g, bl, a := src.At(b.Min.X+x, b.Min.Y+y).RGBA()
			o := (y*w + x) * 4
			if a == 0 {
				px[o], px[o+1], px[o+2], px[o+3] = 0, 0, 0, 0
				continue
			}
			px[o] = uint8((r * 0xffff / a) >> 8)
			px[o+1] = uint8((g * 0xffff / a) >> 8)
			px[o+2] = uint8((bl * 0xffff / a) >> 8)
			px[o+3] = uint8(a >> 8)
		}
	}
	out := image.NewRGBA(image.Rect(0, 0, nw, nh))
	bg := [3]int{24, 24, 28}
	for y := 0; y < nh; y++ {
		y0, y1 := y*h/nh, (y+1)*h/nh
		if y1 <= y0 {
			y1 = y0 + 1
		}
		for x := 0; x < nw; x++ {
			x0, x1 := x*w/nw, (x+1)*w/nw
			if x1 <= x0 {
				x1 = x0 + 1
			}
			var s [4]int
			n := 0
			for yy := y0; yy < y1; yy++ {
				o := (yy*w + x0) * 4
				for xx := x0; xx < x1; xx++ {
					s[0] += int(px[o])
					s[1] += int(px[o+1])
					s[2] += int(px[o+2])
					s[3] += int(px[o+3])
					o += 4
					n++
				}
			}
			a := s[3] / n
			d := out.Pix[(y*nw+x)*4:]
			for c := 0; c < 3; c++ {
				v := s[c] / n
				d[c] = uint8((v*a + bg[c]*(255-a)) / 255)
			}
			d[3] = 255
		}
	}
	var buf bytes.Buffer
	if err := jpeg.Encode(&buf, out, &jpeg.Options{Quality: 92}); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}
