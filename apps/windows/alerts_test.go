package main

import "testing"

// a notification cleared from the list isn't told again while the SS1 still reports the same event
// ("Drives repaired offline" came back after every Clear until the SS1 restarted), also after a restart of the app
func TestNoteNotToldTwice(t *testing.T) {
	t.Setenv("XDG_CONFIG_HOME", t.TempDir())
	notes.Lock()
	notes.list, notes.told = nil, nil
	notes.Unlock()
	cfgMu.Lock()
	cfg.QuietPC = true
	cfgMu.Unlock()
	addNote("sdrescue-2026-11-06 04:31:44", "info", "Drives repaired offline", "all fine", "settings/storage")
	clearNotes()
	addNote("sdrescue-2026-11-06 04:31:44", "info", "Drives repaired offline", "all fine", "settings/storage")
	if len(notes.list) != 0 {
		t.Fatalf("told again after Clear: %+v", notes.list)
	}
	notes.Lock()
	notes.list, notes.told = nil, nil
	notes.Unlock()
	loadNotes() // the app started again
	addNote("sdrescue-2026-11-06 04:31:44", "info", "Drives repaired offline", "all fine", "settings/storage")
	if len(notes.list) != 0 {
		t.Fatalf("told again after a restart: %+v", notes.list)
	}
	addNote("sdrescue-2026-11-07 10:00:00", "info", "Drives repaired offline", "all fine", "settings/storage")
	if len(notes.list) != 1 {
		t.Fatalf("a new repair wasn't told: %+v", notes.list)
	}
}
