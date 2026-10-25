package main

import (
	"encoding/json"
	"os"
	"testing"
)

// every name in the fan art list finds its own entry, and no two entries claim the same game
func TestFanartNames(t *testing.T) {
	m := fanartIndex(fanartBuiltIn)
	var f struct {
		Games []*fanGame `json:"games"`
	}
	b, _ := os.ReadFile("fanart/fanart.json")
	if err := json.Unmarshal(b, &f); err != nil {
		t.Fatal(err)
	}
	for _, g := range f.Games {
		if g.Art == "" || g.Pack == "" || g.CoverFrom == "" {
			t.Errorf("%v: art, pack and cover_from are needed", g.Names)
		}
		for _, n := range g.Names {
			if x := fanFind(m, g.Sys, n); x == nil || x.Pack != g.Pack {
				t.Errorf("%q (%s) doesn't find its own cover", n, g.Sys)
			}
		}
	}
}
