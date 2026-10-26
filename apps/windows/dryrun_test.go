// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// A dry run of the matching rules against a real library (no SS1, no changes): NFJSON=path go test -run TestDryRun -v

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"testing"
)

func TestDryRun(t *testing.T) {
	p := os.Getenv("NFJSON")
	if p == "" {
		t.Skip("NFJSON not set")
	}
	b, _ := os.ReadFile(p)
	var d struct {
		NF []struct {
			ID    int64  `json:"id"`
			Sys   string `json:"sys"`
			Title string `json:"title"`
		} `json:"nf"`
		Lib map[string][]amLibGame `json:"lib"`
	}
	if err := json.Unmarshal(b, &d); err != nil {
		t.Fatal(err)
	}
	lists := map[string][]*LREntry{}
	ctx := context.Background()
	for _, g := range d.NF {
		clean := amClean(g.Title)
		if w := amNotGame(clean); w != "" {
			fmt.Printf("NOTGAME\t%s\t%s\t(%s)\n", g.Sys, g.Title, w)
			continue
		}
		kind, tag := amKind(clean)
		sg := &sgame{ID: g.ID, Title: g.Title, Sys: g.Sys}
		if kind != "" {
			if lb := amLibBase(sg, d.Lib[g.Sys]); lb != nil {
				fmt.Printf("LIBRARY\t%s\t%s\t-> %s\t[%s %s]\n", g.Sys, g.Title, lb.Title, kind, tag)
				continue
			}
		}
		base := amStrip(clean)
		all, ok := lists[g.Sys]
		if m, has := lrSystems[g.Sys]; has && !ok {
			for _, lr := range m.lr {
				l, rc := libretro.get(ctx, lr, m.disc, func(string) {})
				if rc < 0 {
					t.Fatal("list", lr)
				}
				for k := range l.Entries {
					all = append(all, &l.Entries[k])
				}
			}
			lists[g.Sys] = all
		}
		var hit *LREntry
		exact := ""
		for _, n := range append([]string{clean}, amGlued(base)...) {
			if hit, exact = amLoose(all, n); hit != nil {
				break
			}
		}
		if hit == nil && len(all) > 0 {
			hit = lrByName(all, base) // what the sources would be asked: the base name, exactly
		}
		if hit != nil {
			fmt.Printf("LISTS\t%s\t%s\t-> %s\t[%s %s]\n", g.Sys, g.Title, hit.Name, kind, exact)
			continue
		}
		fmt.Printf("ASK\t%s\t%s\t(ask the sources for %q)\t[%s]\n", g.Sys, g.Title, base, kind)
	}
}
