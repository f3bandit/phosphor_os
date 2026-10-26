package main

// Steam's own record of every app it knows (appcache/appinfo.vdf, binary KeyValues), read only for the
// soundtracks Phosphor sends: the English title (a soundtrack's plain "name" can be in its original
// language), the album's artist, and which picture in Steam's library cache is the album cover.
//
// Layout, versions 28 and 29 (Steam's since 2024): a header (magic, universe; v29 adds the offset of a
// string table that holds every key), then one entry per app until app id 0: app id, size, info state,
// last updated, access token (8), SHA-1 (20), change number, binary SHA-1 (20), then the app's
// KeyValues: type byte (0 a section, 1 a string, 2 an int32, 7 a uint64, 8 the end of a section), the
// key (v29: an int32 index into the string table; v28: a NUL-terminated string), the value.

import (
	"encoding/binary"
	"errors"
	"os"
	"path/filepath"
	"sync"
	"time"
)

type albumInfo struct {
	Title    string // English when Steam has it
	Artist   string
	Composer string
	Label    string
	CoverID  string // the cover's file name (without .jpg) in librarycache/<appid>/
}

const (
	appinfoV28 = 0x07564428
	appinfoV29 = 0x07564429
)

type kvReader struct {
	d    []byte
	o    int
	strs []string // v29's string table
	v29  bool
}

var errAppinfo = errors.New("appinfo.vdf: not a format this reads")

func (r *kvReader) cstr() (string, error) {
	for i := r.o; i < len(r.d); i++ {
		if r.d[i] == 0 {
			s := string(r.d[r.o:i])
			r.o = i + 1
			return s, nil
		}
	}
	return "", errAppinfo
}

func (r *kvReader) key() (string, error) {
	if !r.v29 {
		return r.cstr()
	}
	if r.o+4 > len(r.d) {
		return "", errAppinfo
	}
	i := int(int32(binary.LittleEndian.Uint32(r.d[r.o:])))
	r.o += 4
	if i < 0 || i >= len(r.strs) {
		return "", errAppinfo
	}
	return r.strs[i], nil
}

// a section, as nested maps (only strings and sections kept: that's all a soundtrack's info needs)
func (r *kvReader) section(depth int) (map[string]any, error) {
	if depth > 32 {
		return nil, errAppinfo
	}
	m := map[string]any{}
	for {
		if r.o >= len(r.d) {
			return nil, errAppinfo
		}
		t := r.d[r.o]
		r.o++
		if t == 8 {
			return m, nil
		}
		k, err := r.key()
		if err != nil {
			return nil, err
		}
		switch t {
		case 0:
			v, err := r.section(depth + 1)
			if err != nil {
				return nil, err
			}
			m[k] = v
		case 1:
			v, err := r.cstr()
			if err != nil {
				return nil, err
			}
			m[k] = v
		case 2:
			r.o += 4
		case 7:
			r.o += 8
		default:
			return nil, errAppinfo
		}
		if r.o > len(r.d) {
			return nil, errAppinfo
		}
	}
}

func kvGet(m map[string]any, path ...string) any {
	var v any = m
	for _, p := range path {
		mm, ok := v.(map[string]any)
		if !ok {
			return nil
		}
		v = mm[p]
	}
	return v
}

func kvStr(m map[string]any, path ...string) string {
	s, _ := kvGet(m, path...).(string)
	return s
}

// parseAppinfo: the album info of the wanted apps (all apps of type Music when want is nil)
func parseAppinfo(d []byte, want map[string]bool) (map[string]albumInfo, error) {
	if len(d) < 8 {
		return nil, errAppinfo
	}
	magic := binary.LittleEndian.Uint32(d)
	r := &kvReader{d: d, o: 8}
	switch magic {
	case appinfoV28:
	case appinfoV29:
		if len(d) < 16 {
			return nil, errAppinfo
		}
		st := int(binary.LittleEndian.Uint64(d[8:]))
		r.o = 16
		if st <= 16 || st+4 > len(d) {
			return nil, errAppinfo
		}
		n := int(binary.LittleEndian.Uint32(d[st:]))
		sr := &kvReader{d: d, o: st + 4}
		for i := 0; i < n; i++ {
			s, err := sr.cstr()
			if err != nil {
				return nil, err
			}
			r.strs = append(r.strs, s)
		}
		r.v29 = true
	default:
		return nil, errAppinfo
	}
	out := map[string]albumInfo{}
	for r.o+8 <= len(d) {
		appid := binary.LittleEndian.Uint32(d[r.o:])
		if appid == 0 {
			break
		}
		size := int(binary.LittleEndian.Uint32(d[r.o+4:]))
		next := r.o + 8 + size
		if size < 60 || next > len(d) {
			return out, errAppinfo
		}
		id := itoa(appid)
		if want == nil || want[id] {
			sub := &kvReader{d: d[:next], o: r.o + 8 + 60, strs: r.strs, v29: r.v29}
			if m, err := sub.section(0); err == nil {
				c, _ := kvGet(m, "appinfo", "common").(map[string]any)
				if c == nil {
					c, _ = kvGet(m, "common").(map[string]any)
				}
				am, _ := kvGet(m, "appinfo", "albummetadata").(map[string]any)
				if c != nil && (want != nil || kvStr(c, "type") == "Music") {
					a := albumInfo{Title: kvStr(c, "name_localized", "english")}
					if a.Title == "" {
						a.Title = kvStr(c, "name")
					}
					if am != nil {
						a.Artist = kvStr(am, "metadata", "artist", "english")
						a.Composer = kvStr(am, "metadata", "composer", "english")
						a.Label = kvStr(am, "metadata", "Label", "english")
						a.CoverID = kvStr(am, "cdn_assets", "album_cover")
					}
					out[id] = a
				}
			}
		}
		r.o = next
	}
	return out, nil
}

func itoa(n uint32) string {
	if n == 0 {
		return "0"
	}
	var b [10]byte
	i := len(b)
	for n > 0 {
		i--
		b[i] = byte('0' + n%10)
		n /= 10
	}
	return string(b[i:])
}

// Steam's record, read again only when it changed (the list is asked for every second or two while sending)
var aiMu sync.Mutex
var aiCache struct {
	mod  time.Time
	size int64
	info map[string]albumInfo
}

func steamAlbumInfo(appid string) (albumInfo, bool) {
	sp := steamPath()
	if sp == "" {
		return albumInfo{}, false
	}
	p := filepath.Join(sp, "appcache", "appinfo.vdf")
	fi, err := os.Stat(p)
	if err != nil {
		return albumInfo{}, false
	}
	aiMu.Lock()
	defer aiMu.Unlock()
	if aiCache.info == nil || !fi.ModTime().Equal(aiCache.mod) || fi.Size() != aiCache.size {
		d, err := os.ReadFile(p)
		if err != nil {
			return albumInfo{}, false
		}
		info, _ := parseAppinfo(d, nil) // what it read before a problem is still good
		aiCache.mod, aiCache.size, aiCache.info = fi.ModTime(), fi.Size(), info
	}
	a, ok := aiCache.info[appid]
	return a, ok
}
