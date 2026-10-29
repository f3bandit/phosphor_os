// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// Watching the SS1 for things worth knowing, while Phosphor runs (window open or not): a power supply
// problem, settings an update reset, a storage check that failed, a Gen 4 NVMe drive in the dock, box
// art finishing, the SS1 going away. Each becomes a notification in the app (Notifications, the bell)
// and, unless turned off, a Windows notification from the tray icon. The tray icon has a dot while a
// warning hasn't been seen. Kept in %APPDATA%\Phosphor\notifications.json (the last 60).

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

type note struct {
	ID    string `json:"id"`    // one per event: the same event is never told twice
	At    int64  `json:"at"`    // unix seconds
	Level string `json:"level"` // warn | info
	Title string `json:"title"`
	Body  string `json:"body"`
	Seen  bool   `json:"seen"`
	Go    string `json:"go,omitempty"` // the app screen that deals with it
}

var notes struct {
	sync.Mutex
	list []note
}

func notesPath() string { return filepath.Join(configDir(), "notifications.json") }

func loadNotes() {
	notes.Lock()
	defer notes.Unlock()
	if b, err := os.ReadFile(notesPath()); err == nil {
		_ = json.Unmarshal(b, &notes.list)
	}
}

func saveNotesLocked() {
	b, _ := json.MarshalIndent(notes.list, "", " ")
	_ = os.MkdirAll(configDir(), 0o700)
	_ = os.WriteFile(notesPath()+".tmp", b, 0o600)
	_ = os.Rename(notesPath()+".tmp", notesPath())
}

func unseenWarnings() (warn, all int) {
	notes.Lock()
	defer notes.Unlock()
	for _, n := range notes.list {
		if !n.Seen {
			all++
			if n.Level == "warn" {
				warn++
			}
		}
	}
	return
}

// addNote: a new notification (nothing if that event was already told)
func addNote(id, level, title, body, goTo string) {
	notes.Lock()
	for _, n := range notes.list {
		if n.ID == id {
			notes.Unlock()
			return
		}
	}
	notes.list = append([]note{{ID: id, At: time.Now().Unix(), Level: level, Title: title, Body: body, Go: goTo}}, notes.list...)
	if len(notes.list) > 60 {
		notes.list = notes.list[:60]
	}
	saveNotesLocked()
	notes.Unlock()
	cfgMu.Lock()
	quiet := cfg.QuietPC
	cfgMu.Unlock()
	if !quiet {
		trayNotify(title, body, level == "warn")
	}
	w, _ := unseenWarnings()
	trayAlert(w > 0)
}

func markSeen() {
	notes.Lock()
	for i := range notes.list {
		notes.list[i].Seen = true
	}
	saveNotesLocked()
	notes.Unlock()
	trayAlert(false)
}

func clearNotes() {
	notes.Lock()
	notes.list = nil
	saveNotesLocked()
	notes.Unlock()
	trayAlert(false)
}

// ---------- the watch ----------

func ask(cmd string) map[string]any {
	b, code, err := call(json.RawMessage(cmd))
	if err != nil || code != 200 {
		return nil
	}
	var m map[string]any
	if json.Unmarshal(b, &m) != nil || m["ok"] != true {
		return nil
	}
	return m
}

func num(v any) float64 { f, _ := v.(float64); return f }
func str(v any) string  { s, _ := v.(string); return s }

func watchSS1() {
	loadNotes()
	w, _ := unseenWarnings()
	trayAlert(w > 0)
	var (
		misses       int
		lostTold     bool
		scraping     bool
		lastStorage  float64
		firstStorage = true
	)
	for {
		cfgMu.Lock()
		paired := cfg.Address != "" && cfg.TokenEnc != ""
		name := cfg.Name
		cfgMu.Unlock()
		if !paired {
			time.Sleep(10 * time.Second)
			continue
		}
		if name == "" || name == "Phosphor" { // the SS1 calls itself Phosphor unless it was renamed
			name = "Your SS1"
		}
		// answering is any reply at all: an SS1 from before 0.27.16 refuses "gen" from another device
		// (HTTP 403), which isn't the same as not being there
		if _, _, err := call(json.RawMessage(`{"cmd":"gen"}`)); err != nil {
			misses++
			// maybe it moved (the router gave it a new address): look for it, every minute or so
			if misses%4 == 1 && autoReconnect() {
				if a, ok := relocate(); ok {
					addNote(fmt.Sprintf("moved-%d", time.Now().Unix()), "info", "Reconnected to "+name,
						"It's at "+a+" now; nothing to do.", "")
					misses = 0
					continue
				}
			}
			if misses == 12 && !lostTold { // three minutes without an answer
				lostTold = true
				addNote(fmt.Sprintf("lost-%d", time.Now().Unix()/3600), "info", name+" isn't answering",
					"It may be off, asleep, or off the network. Phosphor keeps checking and reconnects by itself.", "")
			}
			time.Sleep(15 * time.Second) // while it's away, often: back within seconds of a restart
			continue
		}
		if lostTold {
			addNote(fmt.Sprintf("back-%d", time.Now().Unix()), "info", name+" is back", "Connected again.", "")
		}
		misses, lostTold = 0, false
		sbAuto() // MiSTer & Linux backups: anything that changed, now and then

		// the power check (health-watch): heavy work cut short with no trace, several times
		if p := ask(`{"cmd":"power"}`); p != nil && p["installed"] == true && p["warn"] == true {
			addNote(fmt.Sprintf("power-%v", num(p["count"])), "warn", "Power supply problem?",
				fmt.Sprintf("%s lost power %v times in the middle of something. A weak or shared power adapter is the usual cause.", name, num(p["count"])), "settings/diagnostics")
		}
		// an update put MiSTer settings back to their defaults (update-guard)
		if g := ask(`{"cmd":"guard"}`); g != nil && g["awaiting"] == true {
			n := 0
			if fs, ok := g["files"].([]any); ok {
				for _, f := range fs {
					if m, ok := f.(map[string]any); ok && m["reset"] == true {
						n++
					}
				}
			}
			if n > 0 {
				addNote(fmt.Sprintf("guard-%s-%d", str(g["updated_at"]), n), "warn", fmt.Sprintf("An update reset %d setting%s", n, map[bool]string{true: "", false: "s"}[n == 1]),
					"Your own settings can be put back from the restore point taken before it.", "settings/power/2")
			}
		}
		// storage: Gen 4 drives in the dock, and a check that found a problem
		if s := ask(`{"cmd":"storage"}`); s != nil && s["installed"] == true {
			if ds, ok := s["drives"].([]any); ok {
				for _, d := range ds {
					m, _ := d.(map[string]any)
					if m != nil && str(m["gen"]) == "gen4" {
						addNote("gen4-"+str(m["model"]), "warn", "Gen 4 NVMe drive in the dock",
							str(m["model"])+": Gen 4 drives cause boot and file system trouble in the SS1 dock. A Gen 3 drive is the safe choice.", "settings/storage/1")
					}
				}
			}
			if l, ok := s["last"].(map[string]any); ok && l["busy"] != true {
				at := num(l["at"])
				if at != lastStorage && !firstStorage && strings.Contains(str(l["output"]), "RESULT: FAIL") {
					addNote(fmt.Sprintf("storage-%v", at), "warn", "A storage check found a problem",
						fmt.Sprintf("%s on %s: see Settings > Storage for what it found.", str(l["job"]), str(l["target"])), "settings/storage")
				}
				lastStorage, firstStorage = at, false
			}
			// the offline SD card repair: one that just ran, and another drive to use when the repair drive is gone
			if r, ok := s["sdrescue"].(map[string]any); ok {
				if r["fresh"] == true && str(r["last_result"]) != "" {
					res := map[string]string{"clean": "no errors found, the card is marked clean", "repaired": "the card was repaired",
						"errors_left": "errors are left: repair it in this app (SD card flash & health > Card health)"}[str(r["last_result"])]
					if res == "" {
						res = "the repair didn't run: see Settings > Storage"
					}
					addNote("sdrescue-"+str(r["last_at"]), "info", "SD card repaired offline", "The SS1 restarted to repair its SD card: "+res+".", "settings/storage")
				}
				if nd, ok := r["new_drive"].(map[string]any); ok && r["drive_present"] != true {
					addNote("sdrescue-new-"+str(nd["partuuid"]), "info", "A new drive for repairing the SD card",
						"The SD card's repair drive isn't plugged in, but "+str(nd["model"])+" is. Make it the repair drive in Settings > Storage > SD card.", "settings/storage")
				}
			}
		}
		// a newer Phosphor for the SS1 on GitHub (the SS1 looks every 30 minutes; Phosphor 0.27.42 on)
		if u := ask(`{"cmd":"update"}`); u != nil {
			if on, ok := u["online"].(map[string]any); ok && on["newer"] == true && str(on["latest"]) != "" {
				addNote("ss1upd-"+str(on["latest"]), "info", "Phosphor "+str(on["latest"])+" is out for "+name,
					"It has "+str(u["version"])+". Settings > Power & updates > Update Phosphor downloads and installs it.", "settings/power/1")
			}
		}
		// box art finishing (only a run this app saw going)
		if sp := ask(`{"cmd":"scrape_progress"}`); sp != nil {
			run := sp["running"] == true
			if scraping && !run {
				addNote(fmt.Sprintf("art-%d", time.Now().Unix()), "info", "Box art is done",
					fmt.Sprintf("%v found, %v not found (Match to base games has those).", num(sp["found"]), num(sp["not_found"])), "match")
			}
			scraping = run
		}
		time.Sleep(60 * time.Second)
	}
}
