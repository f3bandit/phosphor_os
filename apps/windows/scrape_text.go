// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// Box art on the PC: the name handling, the same as phosphord's (ident.c title_norm, scraper.c
// name_base, src_libretro.c serial_norm and friends), so the PC matches exactly what the SS1 would.

import (
	"strconv"
	"strings"
)

var titleNoise = map[string]bool{
	"hack": true, "hacks": true, "hacked": true, "patch": true, "patched": true, "mod": true, "modded": true,
	"translation": true, "translated": true, "trans": true, "english": true, "eng": true, "en": true,
	"fix": true, "fixed": true, "fixes": true, "edition": true, "improvement": true, "improved": true,
	"restoration": true, "redux": true, "beta": true, "proto": true, "prototype": true, "demo": true, "by": true,
	"unl": true, "hb": true, "rev": true, "version": true, "romhack": true, "kaizo": true,
}

func isAlnum(c byte) bool {
	return c >= '0' && c <= '9' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z'
}
func isDigit(c byte) bool { return c >= '0' && c <= '9' }

// titleNorm: lower-case title words, nothing in brackets, no noise words: "Super Mario World (USA)"
// -> "super mario world"
func titleNorm(title string) string {
	var b strings.Builder
	depth := 0
	n := 0
	for i := 0; i < len(title) && n < 511; i++ {
		c := title[i]
		switch c {
		case '(', '[', '{':
			depth++
			continue
		case ')', ']', '}':
			if depth > 0 {
				depth--
			}
			continue
		}
		if depth > 0 {
			continue
		}
		if isAlnum(c) {
			if c >= 'A' && c <= 'Z' {
				c += 'a' - 'A'
			}
			b.WriteByte(c)
		} else {
			b.WriteByte(' ')
		}
		n++
	}
	var out []string
	for _, w := range strings.Fields(b.String()) {
		if titleNoise[w] || (len(w) > 1 && w[0] == 'v' && isDigit(w[1])) {
			continue
		}
		out = append(out, w)
	}
	return strings.Join(out, " ")
}

// nameBase: "Final Fantasy III (USA)" -> "Final Fantasy III"; "Legend of Zelda, The - A Link" ->
// "The Legend of Zelda - A Link"
func nameBase(official string) string {
	s := official
	if i := strings.Index(s, " ("); i >= 0 {
		s = s[:i]
	}
	if i := strings.Index(s, " ["); i >= 0 {
		s = s[:i]
	}
	if i := strings.Index(s, ", The"); i >= 0 && (i+5 == len(s) || s[i+5] == ' ') {
		s = "The " + s[:i] + s[i+5:]
	}
	return s
}

// serialNorm: "GM T-81036 -00" -> "T-81036"
func serialNorm(in string) string {
	p := strings.TrimLeft(in, " ")
	if len(p) >= 3 && strings.EqualFold(p[:3], "GM ") {
		p = p[3:]
	}
	var b strings.Builder
	for i := 0; i < len(p); i++ {
		c := p[i]
		if c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f' {
			continue
		}
		if c >= 'a' && c <= 'z' {
			c -= 'a' - 'A'
		}
		b.WriteByte(c)
	}
	o := b.String()
	if l := len(o); l > 4 && o[l-3] == '-' && isDigit(o[l-2]) && isDigit(o[l-1]) {
		o = o[:l-3]
	}
	return o
}

func nameKey(name string) string {
	base := nameBase(name)
	if len(base) >= 4 && strings.EqualFold(base[:4], "The ") {
		base = base[4:]
	}
	return titleNorm(base)
}

// regionOf: the first brackets' words: "X (USA, Canada)" -> "USA, Canada"
func regionOf(name string) string {
	i := strings.IndexByte(name, '(')
	if i < 0 {
		return ""
	}
	j := strings.IndexByte(name[i:], ')')
	if j < 0 {
		return ""
	}
	return name[i+1 : i+j]
}

func discNo(name string) int {
	i := strings.Index(strings.ToLower(name), "(disc ")
	if i < 0 || i+6 >= len(name) || !isDigit(name[i+6]) {
		return 0
	}
	j := i + 6
	for j < len(name) && isDigit(name[j]) {
		j++
	}
	n, _ := strconv.Atoi(name[i+6 : j])
	return n
}

// libretro thumbnail file names: these characters become _
func thumbName(name string) string {
	var b strings.Builder
	for i := 0; i < len(name); i++ {
		if strings.IndexByte("&*/:`<>?\\|", name[i]) >= 0 {
			b.WriteByte('_')
		} else {
			b.WriteByte(name[i])
		}
	}
	return b.String()
}

// netEscape: like phosphord's: everything but letters, digits and -._~ as %XX
func netEscape(s string) string {
	const hex = "0123456789ABCDEF"
	var b strings.Builder
	for i := 0; i < len(s); i++ {
		c := s[i]
		if isAlnum(c) || c == '-' || c == '.' || c == '_' || c == '~' {
			b.WriteByte(c)
		} else {
			b.WriteByte('%')
			b.WriteByte(hex[c>>4])
			b.WriteByte(hex[c&15])
		}
	}
	return b.String()
}

// trimField: like sres_set: trimmed, no Windows line ends
func trimField(v string) string {
	return strings.ReplaceAll(strings.TrimSpace(v), "\r", "")
}
