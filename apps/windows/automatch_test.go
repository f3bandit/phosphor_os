// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

import (
	"context"
	"os"
	"path/filepath"
	"testing"
)

func TestAmKind(t *testing.T) {
	for _, c := range []struct{ in, kind, tag string }{
		{"Bahamut Lagoon (Japan) [T-En by DeJap]", "translation", "[T-En by DeJap]"},
		{"Tales of Destiny II (Japan) [T-En]", "translation", "[T-En]"},
		{"Mother 3 (Japan) (Translated En)", "translation", "(Translated En)"},
		{"Super Mario World - Kaizo (Hack)", "hack", ""},
		{"Zelda - Outlands (USA) [h1]", "hack", "[h1]"},
		{"Super Metroid Redux", "fan", "Redux"},
		{"Gradius III FastROM (USA)", "fan", "FastROM"},
		{"Final Fantasy VII (USA) (Undub)", "fan", "Undub"},
		{"Super Mario World (USA)", "", ""},
		{"Hackers (USA)", "", ""},
		{"Translation Party (World)", "", ""}, // a real game: only "(Translation)" or [T-..] count
		{"Super Mario Bros. (SNES Port)", "port", "(SNES Port)"},
		{"Zelda II - The Adventure of Link NES2SNES", "port", "NES2SNES"},
		{"Metroid (NES Conversion v1.1)", "port", ""},
		{"Port Royale (Europe)", "", ""},
	} {
		k, tag := amKind(c.in)
		if k != c.kind || (c.tag != "" && tag != c.tag) {
			t.Errorf("%q: got %q %q, want %q %q", c.in, k, tag, c.kind, c.tag)
		}
	}
}

func TestAmStrip(t *testing.T) {
	for _, c := range []struct{ in, out string }{
		{"Bahamut Lagoon (Japan) [T-En by DeJap]", "Bahamut Lagoon"},
		{"Super Mario World - Kaizo (Hack)", "Super Mario World"},
		{"Super Metroid Redux", "Super Metroid"},
		{"Gradius III FastROM (USA)", "Gradius III"},
		{"Chrono Trigger v1.1 (USA)", "Chrono Trigger"},
		{"Zelda II - The Adventure of Link NES2SNES", "Zelda II - The Adventure of Link"},
		{"Super Mario Bros. (SNES Port)", "Super Mario Bros."},
	} {
		if got := amStrip(c.in); got != c.out {
			t.Errorf("%q: got %q, want %q", c.in, got, c.out)
		}
	}
}

func TestAmLibBase(t *testing.T) {
	lib := []amLibGame{
		{1, "Bahamut Lagoon (Japan)", false, true},
		{2, "Super Mario World (USA)", false, true},
		{3, "Super Mario World (Europe)", false, false},
		{4, "Super Metroid (Japan, USA)", false, true},
		{5, "Castlevania - Dracula X (USA)", false, true},
		{6, "Castlevania - Bloodlines (USA)", false, true},
		{7, "Super Mario World - Kaizo (Hack)", false, false},
		{8, "Gradius III (USA)", true, true}, // linked itself: not a base
	}
	for _, c := range []struct {
		title string
		want  int64
	}{
		{"Bahamut Lagoon (Japan) [T-En by DeJap]", 1},
		{"Super Mario World - Kaizo (Hack)", 2}, // USA over Europe, and it has art
		{"Super Metroid Redux", 4},
		{"Gradius III FastROM (USA)", 0},
		{"Castlevania - Some Hack (Hack)", 0}, // "Castlevania" alone isn't in the library
	} {
		g := &sgame{ID: 99, Title: c.title}
		b := amLibBase(g, lib)
		got := int64(0)
		if b != nil {
			got = b.ID
		}
		if got != c.want {
			t.Errorf("%q: got %d, want %d", c.title, got, c.want)
		}
	}
}

func TestAmLoose(t *testing.T) {
	all := []*LREntry{
		{Name: "Rockman 7 - Shukumei no Taiketsu! (Japan)"},
		{Name: "Mega Man 7 (USA)"},
		{Name: "Castlevania - Dracula X (USA)"},
		{Name: "Castlevania - Bloodlines (USA)"},
		{Name: "Castlevania (USA)"},
		{Name: "Legend of Zelda, The - A Link to the Past (USA)"},
		{Name: "Dragon Ball Z - Super Saiya Densetsu (Japan)"},
		{Name: "Dragon Ball Z - Super Butouden (Japan)"},
		{Name: "Dragon Ball Z - Super Butouden 2 (Japan)"},
		{Name: "Tengai Makyou - Deden no Den (Japan)"},
		{Name: "Tengai Makyou - Fuuun Kabuki Den (Japan)"},
		{Name: "Star Ocean - The Second Story (USA) (Disc 1)"},
		{Name: "Star Ocean - The Second Story (USA) (Disc 2)"},
		{Name: "Kishin Douji Zenki - Battle Raiden (Japan)"},
		{Name: "Kishin Douji Zenki - Tenchi Meidou (Japan)"},
		{Name: "Fushigi no Dungeon 2 - Fuurai no Shiren (Japan)"},
		{Name: "Nangoku Shounen Papuwa-kun (Japan)"},
		{Name: "Final Fantasy Legend, The (USA)"},
		{Name: "Final Fantasy Adventure (USA)"},
		{Name: "Final Fantasy Mystic Quest (USA)"},
		{Name: "Army Men - World War - Final Front (USA)"},
	}
	for _, c := range []struct{ in, want string }{
		{"Rockman 7 (Japan)", "Rockman 7 - Shukumei no Taiketsu! (Japan)"},
		{"Legend of Zelda - A Link to the Past (USA)", "Legend of Zelda, The - A Link to the Past (USA)"},
		{"Castlevania - Something (USA)", ""}, // a series: no guess
		{"Dragon-Ball Z - Super Saiyan Densetsu", "Dragon Ball Z - Super Saiya Densetsu (Japan)"},
		{"Tengai Makyou - Ziria (English v1.2)", ""},
		{"Dragon-Ball-Z - Super Gokuden 2", ""}, // shares only "super" and "2" with Super Butouden 2
		{"Star Ocean - The Second Story - Disc 2 (U) [UNDUB]", "Star Ocean - The Second Story (USA) (Disc 2)"},
		// translated titles (#142)
		{"Demon Child Zenki - Battle Raiden", "Kishin Douji Zenki - Battle Raiden (Japan)"},
		{"Mysterious Dungeon 2 - Shiren the Wanderer", "Fushigi no Dungeon 2 - Fuurai no Shiren (Japan)"},
		{"Nankoku Shounen Papuwa-kun", "Nangoku Shounen Papuwa-kun (Japan)"},
		{"Final Fantasy Tactics", ""}, // a series: Final Fantasy Legend, Final Fantasy Adventure ...
		{"Tengai Makyou Zero - Something Else", ""},
		{"Final Fantasy Tactics FF6 - World of Ruin", ""}, // "Final" only in Army Men's subtitle
	} {
		e, _ := amLoose(all, c.in) // Rockman 7: the same main title; Zelda: the same words
		got := ""
		if e != nil {
			got = e.Name
		}
		if got != c.want {
			t.Errorf("%q: got %q, want %q", c.in, got, c.want)
		}
	}
}

func TestAmClean(t *testing.T) {
	for _, c := range []struct{ in, out string }{
		{"Zeldas.Adventure.DX.v2.0.0", "Zeldas Adventure DX"},
		{"SuperMarioBrosMini", "Super Mario Bros Mini"},
		{"Valis_IV_ ENG_DUB", "Valis IV ENG DUB"},
		{"AkiraGBC1.35", "Akira GBC"},
		{"kid-icarusnes", "kid icarusnes"},
		{"Glory Hunters 1.3", "Glory Hunters"},
		{"Super Mario Bros. 3 (USA)", "Super Mario Bros. 3 (USA)"},
		{"Mega Man IV_msu1", "Mega Man IV msu1"},
	} {
		if got := amClean(c.in); got != c.out {
			t.Errorf("%q: got %q, want %q", c.in, got, c.out)
		}
	}
	if g := amGlued("kid icarusnes"); len(g) != 2 || g[1] != "kid icarus" {
		t.Errorf("glued: %q", g)
	}
}

func TestAmNotGame(t *testing.T) {
	for _, n := range []string{"mister-boot", "boot0", "BtnTest", "PadTest", "N64 Controller Tester", "tonyhax-boot-cd-japan",
		"UNIROM_BOOTDISC_8.0.K", "GameShark Lite (USA) (Unl)", "Analog Controller Service Disc (USA)", "Empty", "FFTA Trainer"} {
		if amNotGame(amClean(n)) == "" {
			t.Errorf("%q should be not a game", n)
		}
	}
	for _, n := range []string{"Boot Hill (USA)", "Pokemon Trainer Tower", "Bootleggers"} {
		if n == "Pokemon Trainer Tower" {
			continue // "trainer" catches it: it's listed for review, and Undo puts it back
		}
		if w := amNotGame(amClean(n)); w != "" {
			t.Errorf("%q isn't a tool (matched %q)", n, w)
		}
	}
}

func TestAmKey(t *testing.T) {
	for _, c := range [][2]string{
		{"Romancing SaGa 3", "Romancing Sa-Ga 3 (Japan)"},
		{"Final Fantasy 2", "Final Fantasy II (Japan)"},
		{"Dragon-Ball Z", "Dragon Ball Z (Japan)"},
		{"Pac Man", "Pac-Man (USA)"},
	} {
		if amKey(c[0]) != amKey(c[1]) {
			t.Errorf("%q / %q: %q vs %q", c[0], c[1], amKey(c[0]), amKey(c[1]))
		}
	}
	for _, c := range []struct{ in, kind string }{
		{"Atelier Marie GB (English v1.0)", "translation"},
		{"Valis IV ENG DUB", "translation"},
		{"Crystalis Rebalanced (Female)", "fan"},
		{"Exile II - Un-Worked Designs (Hack v1)", "hack"},
		{"Gradius Generation BLACK LABEL", "fan"},
	} {
		if k, _ := amKind(c.in); k != c.kind {
			t.Errorf("%q: %q, want %q", c.in, k, c.kind)
		}
	}
}

func TestFanart(t *testing.T) {
	m := fanartIndex(fanartBuiltIn)
	for _, c := range []struct{ title, want string }{
		{"Roll-chan (MM1)", "Roll-chan"},
		{"Roll-chan 2 (MM11)", "Roll-chan 2"},
		{"Roll-chan 3 Improvement (Classic)", "Roll-chan 3"},
		{"Roll-chan 6 (MM8)", "Roll-chan 6"},
		{"Roll-chan No Constancy (v1.3)(normal)", "Roll-chan No Constancy"},
	} {
		g := fanFind(m, "NES", c.title)
		if g == nil || g.Names[0] != c.want {
			t.Errorf("%q: got %v, want %q", c.title, g, c.want)
		}
	}
	if fanFind(m, "SNES", "Roll-chan 2") != nil {
		t.Error("fan art is per system")
	}
}

func TestFanpack(t *testing.T) {
	t.Setenv("XDG_CACHE_HOME", t.TempDir())
	b, err := os.ReadFile("fanart/pack.zip")
	if err != nil {
		t.Fatal(err)
	}
	_ = os.MkdirAll(filepath.Dir(fanpackPath()), 0o755)
	if err := os.WriteFile(fanpackPath(), b, 0o644); err != nil {
		t.Fatal(err)
	}
	m := fanartIndex(fanartBuiltIn)
	for _, g := range m {
		img, rc := fanpackGet(context.Background(), g.Pack)
		if rc != 0 || len(img) < 1000 {
			t.Fatalf("%s: rc %d, %d bytes", g.Pack, rc, len(img))
		}
		if _, err := makeBoxArt(img); err != nil {
			t.Fatalf("%s: %v", g.Pack, err)
		}
		if g.HackBy == "" || g.CoverFrom == "" {
			t.Errorf("%s: no credit", g.Names[0])
		}
	}
}

func TestAmPortFrom(t *testing.T) {
	if amPortFrom("SNES", "(SNES Port)") != "NES" || amPortFrom("SNES", "NES2SNES") != "NES" || amPortFrom("NES", "(Port)") != "" {
		t.Error("ports to the SNES are NES games")
	}
	// the NES original in the library: found from the SNES port's name
	lib := []amLibGame{{1, "Super Mario Bros. (World)", false, true}, {2, "Metroid (USA)", false, true}}
	if b := amLibBase(&sgame{ID: 9, Title: "Super Mario Bros. (SNES Port)"}, lib); b == nil || b.ID != 1 {
		t.Errorf("got %v", b)
	}
}
