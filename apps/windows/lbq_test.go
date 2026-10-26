// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

import (
	"context"
	"fmt"
	"os"
	"strings"
	"testing"
)

func TestLBQuery(t *testing.T) {
	q := os.Getenv("LBQ")
	if q == "" {
		t.Skip()
	}
	d, rc := lbGet(context.Background(), func(string) {})
	if rc != 0 {
		t.Fatal(rc)
	}
	for _, g := range d.Games {
		if strings.Contains(g.Norm, q) {
			fmt.Println("GAME", g.Platform, "|", g.Name, "|", g.Norm)
		}
	}
	for n, ids := range d.Alt {
		if strings.Contains(n, q) {
			for _, id := range ids {
				if g := d.Games[id]; g != nil {
					fmt.Println("ALT", n, "->", g.Platform, "|", g.Name)
				}
			}
		}
	}
}
