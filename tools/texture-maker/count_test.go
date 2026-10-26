// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package main

import (
	"os"
	"testing"
)

// the texture tables count the same in texture.c and texture.js, removed numbers included
func TestTablesMatch(t *testing.T) {
	c, _ := os.ReadFile("../../frontend/src/texture.c")
	js, _ := os.ReadFile("../../apps/windows/web/texture.js")
	ct, jt := cTableRe.FindStringSubmatch(string(c)), jsTableRe.FindStringSubmatch(string(js))
	if ct == nil || jt == nil {
		t.Fatal("tables not found")
	}
	nc, nj := len(cEntryRe.FindAllString(ct[1], -1)), len(jsEntryRe.FindAllString(jt[1], -1))
	if nc != nj || nc != 28 {
		t.Fatalf("texture.c %d, texture.js %d (want 28 each)", nc, nj)
	}
}
