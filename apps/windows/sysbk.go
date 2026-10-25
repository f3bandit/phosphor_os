package main

// MiSTer & Linux backups (SS1 Tool's): every version of the SS1's system files (Main, the menu core,
// Linux) and settings files (MiSTer.ini and its profiles, Phosphor's settings, the network address,
// name and SSH settings), kept on this PC in %APPDATA%\Phosphor\backups\sysbk\<id>\<sha256>\. Checked
// when the SS1 is connected and every half hour after; a file is only fetched when it changed.
// A settings file that changed when Linux did looks reset by the update: it's flagged, with the
// version from before to put back. Putting one back sends it in pieces; phosphord checks it and keeps
// the file as it was on the SS1 first (phosphor/backups/sysbk). Passwords, logins, pairings and keys
// never come here: phosphord doesn't offer them (system/phosphord/sysbk.c).

import (
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"time"
)

type sbItem struct {
	ID     string  `json:"id"`
	Name   string  `json:"name"`
	Kind   string  `json:"kind"`
	Exists bool    `json:"exists"`
	Size   float64 `json:"size"`
	Hash   *string `json:"hash"`
}

type sbList struct {
	OK    bool     `json:"ok"`
	Error string   `json:"error"`
	Linux string   `json:"linux"`
	Busy  bool     `json:"busy"`
	Items []sbItem `json:"items"`
}

type sbInfo struct {
	Hash  string `json:"hash"`
	Size  int64  `json:"size"`
	Linux string `json:"linux"` // MiSTer Linux when this version was first seen
	Seen  int64  `json:"seen"`
}

var sb struct {
	sync.Mutex
	busy    bool
	job     string // "save" | "restore"
	file    string
	done    int64
	total   int64
	err     string
	last    int64 // the last full check (unix)
	lastOK  bool
	restart bool // a system file was put back: the SS1 needs a restart
}

func sbRoot() string { return filepath.Join(configDir(), "backups", "sysbk") }

func sbCall(v any, out any) error {
	b, _ := json.Marshal(v)
	r, code, err := call(b)
	if err != nil {
		return err
	}
	if code != 200 {
		return fmt.Errorf("the SS1 answered %d", code)
	}
	var e struct {
		OK    bool   `json:"ok"`
		Error string `json:"error"`
	}
	_ = json.Unmarshal(r, &e)
	if !e.OK {
		if e.Error == "unknown cmd" {
			return errors.New("update Phosphor on the SS1 (0.27.28 or later) for backups")
		}
		return errors.New(e.Error)
	}
	return json.Unmarshal(r, out)
}

// the versions kept of one file, newest first
func sbVersions(id string) []sbInfo {
	out := []sbInfo{}
	ds, _ := os.ReadDir(filepath.Join(sbRoot(), id))
	for _, d := range ds {
		b, err := os.ReadFile(filepath.Join(sbRoot(), id, d.Name(), "info.json"))
		if err != nil {
			continue
		}
		var in sbInfo
		if json.Unmarshal(b, &in) == nil && in.Hash == d.Name() {
			out = append(out, in)
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Seen > out[j].Seen })
	return out
}

// fetch one file's current version (in pieces), unless it's kept already
func sbFetch(it sbItem, linux string) error {
	if it.Hash == nil || !it.Exists {
		return nil
	}
	dir := filepath.Join(sbRoot(), it.ID, *it.Hash)
	if _, err := os.Stat(filepath.Join(dir, "info.json")); err == nil {
		return nil
	}
	tmp := dir + ".incomplete"
	_ = os.RemoveAll(tmp)
	if err := os.MkdirAll(tmp, 0o700); err != nil {
		return err
	}
	f, err := os.Create(filepath.Join(tmp, "content"))
	if err != nil {
		return err
	}
	var off int64
	for {
		var p struct {
			Size   int64   `json:"size"`
			Length int64   `json:"length"`
			Data   string  `json:"data"`
			Hash   *string `json:"hash"`
		}
		if err := sbCall(map[string]any{"cmd": "sysbk", "get": it.ID, "offset": off}, &p); err != nil {
			f.Close()
			_ = os.RemoveAll(tmp)
			return err
		}
		d, err := base64.StdEncoding.DecodeString(p.Data)
		if err != nil {
			f.Close()
			_ = os.RemoveAll(tmp)
			return err
		}
		if _, err := f.Write(d); err != nil {
			f.Close()
			_ = os.RemoveAll(tmp)
			return err
		}
		off += int64(len(d))
		sb.Lock()
		sb.done, sb.total = off, p.Size
		sb.Unlock()
		if off >= p.Size || len(d) == 0 {
			if p.Hash == nil || *p.Hash != *it.Hash {
				f.Close()
				_ = os.RemoveAll(tmp)
				return fmt.Errorf("%s changed while it was being copied: next time", it.Name)
			}
			break
		}
	}
	if err := f.Close(); err != nil {
		_ = os.RemoveAll(tmp)
		return err
	}
	if got, err := fileSHA256(filepath.Join(tmp, "content")); err != nil || got != *it.Hash {
		_ = os.RemoveAll(tmp)
		return fmt.Errorf("%s didn't arrive whole: next time", it.Name)
	}
	b, _ := json.MarshalIndent(sbInfo{Hash: *it.Hash, Size: off, Linux: linux, Seen: time.Now().Unix()}, "", "  ")
	if err := os.WriteFile(filepath.Join(tmp, "info.json"), b, 0o600); err != nil {
		_ = os.RemoveAll(tmp)
		return err
	}
	return os.Rename(tmp, dir)
}

// sbSave: everything that changed since last time; hashing a big file on the SS1 can take a minute
func sbSave() {
	sb.Lock()
	if sb.busy {
		sb.Unlock()
		return
	}
	sb.busy, sb.job, sb.err, sb.file = true, "save", "", ""
	sb.Unlock()
	err := func() error {
		var l sbList
		for tries := 0; ; tries++ {
			if err := sbCall(map[string]any{"cmd": "sysbk"}, &l); err != nil {
				return err
			}
			if !l.Busy || tries > 90 {
				break
			}
			time.Sleep(2 * time.Second) // the SS1 is still reading Linux (390 MB) for its hash
		}
		for _, it := range l.Items {
			sb.Lock()
			sb.file, sb.done, sb.total = it.Name, 0, int64(it.Size)
			sb.Unlock()
			if err := sbFetch(it, l.Linux); err != nil {
				return err
			}
			// a settings file the last Linux update put back to stock: say so (once per version)
			if it.Kind == "settings" && it.Hash != nil {
				if st, before := sbStatus(*it.Hash, sbVersions(it.ID)); st == "reset" && before != nil {
					addNote("sysbk-reset-"+it.ID+"-"+(*it.Hash)[:12], "warn", it.Name+" looks reset by an update",
						"It changed when MiSTer Linux did. The version from before is kept on this PC: Settings > Power & updates > MiSTer & Linux backups.", "settings/power/4")
				}
			}
		}
		return nil
	}()
	sb.Lock()
	sb.busy, sb.file = false, ""
	sb.last, sb.lastOK = time.Now().Unix(), err == nil
	if err != nil {
		sb.err = err.Error()
	}
	sb.Unlock()
}

// the status of one file: unchanged, changed, reset (it changed when Linux did), new (one version)
func sbStatus(cur string, vs []sbInfo) (string, *sbInfo) {
	idx := -1
	for i, v := range vs {
		if v.Hash == cur {
			idx = i
		}
	}
	if idx < 0 {
		return "not kept yet", nil
	}
	if idx+1 >= len(vs) {
		return "kept", nil
	}
	prev := vs[idx+1]
	if vs[idx].Linux != "" && prev.Linux != "" && vs[idx].Linux != prev.Linux {
		return "reset", &prev
	}
	return "changed", nil
}

func apiSysbk(w http.ResponseWriter, r *http.Request) {
	if r.Method == "POST" {
		var req struct {
			Do   string `json:"do"` // save | restore
			ID   string `json:"id"`
			Hash string `json:"hash"`
		}
		if readJSON(r, &req) != nil {
			fail(w, "bad request")
			return
		}
		switch req.Do {
		case "save":
			go sbSave()
		case "restore":
			if strings.ContainsAny(req.ID, `/\`) || strings.Contains(req.ID, "..") || len(req.Hash) != 64 || strings.ContainsAny(req.Hash, `/\.`) {
				fail(w, "bad request")
				return
			}
			go sbRestore(req.ID, req.Hash)
		default:
			fail(w, "do: save or restore")
			return
		}
		time.Sleep(150 * time.Millisecond)
	}
	var l sbList
	err := sbCall(map[string]any{"cmd": "sysbk"}, &l)
	sb.Lock()
	st := map[string]any{"ok": true, "busy": sb.busy, "job": sb.job, "file": sb.file, "done": sb.done, "total": sb.total,
		"error": sb.err, "last": sb.last, "last_ok": sb.lastOK, "restart": sb.restart, "folder": sbRoot()}
	sb.Unlock()
	if err != nil {
		st["offline"] = err.Error()
	}
	st["linux"] = l.Linux
	items := []map[string]any{}
	for _, it := range l.Items {
		vs := sbVersions(it.ID)
		cur := ""
		if it.Hash != nil {
			cur = *it.Hash
		}
		status, before := sbStatus(cur, vs)
		if !it.Exists {
			status = "not on the SS1"
		}
		items = append(items, map[string]any{"id": it.ID, "name": it.Name, "kind": it.Kind, "exists": it.Exists, "size": it.Size,
			"hash": cur, "status": status, "before": before, "versions": vs})
	}
	st["items"] = items
	writeJSON(w, st)
}

func sbRestore(id, hash string) {
	sb.Lock()
	if sb.busy {
		sb.Unlock()
		return
	}
	sb.busy, sb.job, sb.err, sb.file = true, "restore", "", id
	sb.Unlock()
	err := func() error {
		data, err := os.ReadFile(filepath.Join(sbRoot(), id, hash, "content"))
		if err != nil {
			return errors.New("that version isn't on this PC")
		}
		const piece = 512 * 1024
		for off := 0; off < len(data) || (off == 0 && len(data) == 0); off += piece {
			end := off + piece
			if end > len(data) {
				end = len(data)
			}
			var res struct {
				Done    bool `json:"done"`
				Restart bool `json:"restart"`
			}
			if err := sbCall(map[string]any{"cmd": "sysbk", "put": id, "offset": off, "size": len(data), "hash": hash,
				"data": base64.StdEncoding.EncodeToString(data[off:end])}, &res); err != nil {
				return err
			}
			sb.Lock()
			sb.done, sb.total = int64(end), int64(len(data))
			if res.Restart {
				sb.restart = true
			}
			sb.Unlock()
			if res.Done {
				break
			}
		}
		return nil
	}()
	sb.Lock()
	sb.busy, sb.file = false, ""
	if err != nil {
		sb.err = err.Error()
	}
	sb.Unlock()
	if err == nil {
		go sbSave() // the file as it is now is a version too
	}
}

// sbAuto: from the watcher, while the SS1 answers: a check now and then (only changed files are fetched)
func sbAuto() {
	sb.Lock()
	due := !sb.busy && time.Now().Unix()-sb.last > 30*60
	sb.Unlock()
	if due {
		go sbSave()
	}
}

func fileSHA256(path string) (string, error) {
	f, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer f.Close()
	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return "", err
	}
	return hex.EncodeToString(h.Sum(nil)), nil
}
