package main

import "testing"

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
	}
	for _, c := range []struct{ in, want string }{
		{"Rockman 7 (Japan)", "Rockman 7 - Shukumei no Taiketsu! (Japan)"},
		{"Legend of Zelda - A Link to the Past (USA)", "Legend of Zelda, The - A Link to the Past (USA)"},
		{"Castlevania - Something (USA)", ""}, // a series: no guess
	} {
		e, _ := amLoose(all, c.in) // Rockman 7: the same main title (exact false); Zelda: the same words
		got := ""
		if e != nil {
			got = e.Name
		}
		if got != c.want {
			t.Errorf("%q: got %q, want %q", c.in, got, c.want)
		}
	}
}
