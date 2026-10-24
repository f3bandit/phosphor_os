package main

// Adding a texture to Phosphor's source: its data files next to texture.c and texture.js, its line
// in both texture tables (the next free number, the same in both) and in the order Settings shows
// them, and the Windows app's <script> line. Adding one that's already there replaces its data and
// keeps its number, so a texture picked on a console stays the same one.

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

const (
	markInc   = "/* sprite textures: includes (tools/texture-maker) */"
	markEntry = "/* sprite textures: entries (tools/texture-maker) */"
)

type phInfo struct {
	Next     int      `json:"next"`    // the number a new texture gets
	Sprites  []string `json:"sprites"` // ids of the sprite textures already in
	Ready    bool     `json:"ready"`   // texture.c and texture.js can draw sprite textures
	TexC     string   `json:"texture_c"`
	TexJS    string   `json:"texture_js"`
	IndexHTM string   `json:"index_html"`
}

func phPaths(root string) (c, js, html, src, web string) {
	src = filepath.Join(root, "frontend", "src")
	web = filepath.Join(root, "apps", "windows", "web")
	return filepath.Join(src, "texture.c"), filepath.Join(web, "texture.js"), filepath.Join(web, "index.html"), src, web
}

var (
	cTableRe  = regexp.MustCompile(`(?s)\} T\[\] = \{\n(.*?)\n\};`)
	jsTableRe = regexp.MustCompile(`(?s)const T = \[\n(.*?)\n\t\];`)
	cEntryRe  = regexp.MustCompile(`(?m)^\t\{ `)
	jsEntryRe = regexp.MustCompile(`(?m)^\t\t(\[|null)`)
	cOrderRe  = regexp.MustCompile(`static const int ORDER\[\] = \{([^}]*)\};`)
	jsOrderRe = regexp.MustCompile(`const ORDER = \[([^\]]*)\];`)
	cIncRe    = regexp.MustCompile(`#include "tex_([a-z0-9_]+)\.h"`)
)

func phosphorInfo(root string) (phInfo, error) {
	var in phInfo
	cp, jp, hp, _, _ := phPaths(root)
	in.TexC, in.TexJS, in.IndexHTM = cp, jp, hp
	cb, err := os.ReadFile(cp)
	if err != nil {
		return in, err
	}
	jb, err := os.ReadFile(jp)
	if err != nil {
		return in, err
	}
	c, js := string(cb), string(jb)
	ct, jt := cTableRe.FindStringSubmatch(c), jsTableRe.FindStringSubmatch(js)
	if ct == nil || jt == nil {
		return in, fmt.Errorf("couldn't find the texture tables in texture.c and texture.js")
	}
	nc, nj := len(cEntryRe.FindAllString(ct[1], -1)), len(jsEntryRe.FindAllString(jt[1], -1))
	if nc != nj {
		return in, fmt.Errorf("texture.c has %d textures and texture.js %d: they have to match before one is added", nc, nj)
	}
	in.Next = nc
	for _, m := range cIncRe.FindAllStringSubmatch(c, -1) {
		in.Sprites = append(in.Sprites, m[1])
	}
	in.Ready = strings.Contains(c, markInc) && strings.Contains(c, markEntry) && strings.Contains(js, markEntry) && strings.Contains(c, "SH_SPRITES")
	return in, nil
}

// the number a texture already has in the C table (-1 when it isn't in)
func entryNumber(c, ref string) int {
	ct := cTableRe.FindStringSubmatch(c)
	if ct == nil {
		return -1
	}
	for i, l := range strings.Split(ct[1], "\n") {
		_ = i
		if strings.Contains(l, ref+" }") {
			// count the entries before this line
			pre := ct[1][:strings.Index(ct[1], l)]
			return len(cEntryRe.FindAllString(pre, -1))
		}
	}
	return -1
}

func appendOrder(s string, re *regexp.Regexp, n int) string {
	return re.ReplaceAllStringFunc(s, func(m string) string {
		sub := re.FindStringSubmatch(m)
		list := strings.TrimRight(sub[1], " ")
		for _, f := range strings.Split(list, ",") {
			if strings.TrimSpace(f) == strconv.Itoa(n) {
				return m
			}
		}
		return strings.Replace(m, sub[1], list+", "+strconv.Itoa(n)+sub[1][len(list):], 1) /* the space before the brace stays as it was */
	})
}

func apiInstall(w http.ResponseWriter, r *http.Request) {
	var e exportReq
	if err := json.NewDecoder(r.Body).Decode(&e); err != nil {
		fail(w, 400, "bad request")
		return
	}
	if !idRe.MatchString(e.ID) || e.Name == "" || strings.ContainsAny(e.Name, "\"\\\n") {
		fail(w, 400, "bad texture id or name")
		return
	}
	cfgMu.Lock()
	root := cfg.Phosphor
	cfgMu.Unlock()
	if root == "" || !isPhosphor(root) {
		fail(w, 400, "Pick Phosphor's source folder first (Settings).")
		return
	}
	info, err := phosphorInfo(root)
	if err != nil {
		fail(w, 400, err.Error())
		return
	}
	if !info.Ready {
		fail(w, 400, "This copy of Phosphor can't draw sprite textures yet (texture.c and texture.js need the sprite texture support).")
		return
	}
	hName, jName := "tex_"+e.ID+".h", "tex_"+e.ID+".js"
	hData, err := e.data(hName)
	if err != nil {
		fail(w, 400, err.Error())
		return
	}
	jData, err := e.data(jName)
	if err != nil {
		fail(w, 400, err.Error())
		return
	}
	cp, jp, hp, src, web := phPaths(root)
	cb, _ := os.ReadFile(cp)
	jb, _ := os.ReadFile(jp)
	hb, err := os.ReadFile(hp)
	if err != nil {
		fail(w, 500, err.Error())
		return
	}
	c, js, html := string(cb), string(jb), string(hb)
	var entry struct {
		W, H int
	}
	_ = json.Unmarshal([]byte(e.Files["texture.json"]), &entry)
	if entry.W <= 0 || entry.H <= 0 {
		fail(w, 400, "texture.json is missing")
		return
	}
	ID := strings.ToUpper(e.ID)
	ref := "&TX_" + ID
	cLine := fmt.Sprintf("\t{ \"%s\", SH_SPRITES, %d, %d, 1, M_LINE, %s },", e.Name, entry.W, entry.H, ref)
	jLine := fmt.Sprintf("\t\t[\"%s\", SH.SPRITES, %d, %d, 1, LINE, TEX_%s],", e.Name, entry.W, entry.H, ID)
	num := entryNumber(c, ref)
	replaced := num >= 0
	if replaced {
		// the same number: only its line (the name may have changed) and its data
		tag := "   /* " + strconv.Itoa(num) + " */"
		c = regexp.MustCompile(`(?m)^\t\{ "[^"]*", SH_SPRITES, [^\n]*`+regexp.QuoteMeta(ref)+` \},.*$`).ReplaceAllLiteralString(c, cLine+tag)
		js = regexp.MustCompile(`(?m)^\t\t\["[^"]*", SH\.SPRITES, [^\n]*TEX_`+ID+`\],.*$`).ReplaceAllLiteralString(js, jLine+tag)
	} else {
		num = info.Next
		c = strings.Replace(c, markInc, "#include \""+hName+"\"\n"+markInc, 1)
		c = strings.Replace(c, "\t"+markEntry, cLine+"   /* "+strconv.Itoa(num)+" */\n\t"+markEntry, 1)
		js = strings.Replace(js, "\t\t"+markEntry, jLine+"   /* "+strconv.Itoa(num)+" */\n\t\t"+markEntry, 1)
		c = appendOrder(c, cOrderRe, num)
		js = appendOrder(js, jsOrderRe, num)
	}
	if !strings.Contains(html, `src="`+jName+`"`) {
		html = strings.Replace(html, `<script src="texture.js"></script>`, `<script src="`+jName+`"></script><script src="texture.js"></script>`, 1)
	}
	for _, f := range []struct {
		p string
		b []byte
	}{{filepath.Join(src, hName), hData}, {filepath.Join(web, jName), jData}, {cp, []byte(c)}, {jp, []byte(js)}, {hp, []byte(html)}} {
		if err := os.WriteFile(f.p, f.b, 0o644); err != nil {
			fail(w, 500, err.Error())
			return
		}
	}
	msg := fmt.Sprintf("\"%s\" added to Phosphor as texture %d.", e.Name, num)
	if replaced {
		msg = fmt.Sprintf("\"%s\" updated in Phosphor (texture %d, the same number as before).", e.Name, num)
	}
	writeJSON(w, map[string]any{"ok": true, "number": num, "message": msg,
		"files": []string{filepath.Join(src, hName), filepath.Join(web, jName), cp, jp, hp}})
}
