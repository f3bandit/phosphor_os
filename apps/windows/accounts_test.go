// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

import (
	"encoding/json"
	"os"
	"strings"
	"testing"
)

// the box-art logins kept on this PC: user names as they are, secrets only protected
func TestPCAccountsStoreLoad(t *testing.T) {
	t.Setenv("XDG_CONFIG_HOME", t.TempDir())
	t.Setenv("HOME", t.TempDir())
	cfgMu.Lock()
	old := cfg
	cfg = config{Accent: "amber"}
	cfgMu.Unlock()
	defer func() { cfgMu.Lock(); cfg = old; cfgMu.Unlock() }()

	set := [][3]string{
		{"screenscraper", "user", "fake-user"}, {"screenscraper", "password", "fake-pass"},
		{"thegamesdb", "apikey", "fake-key"}, {"igdb", "client_id", "fake-id"}, {"igdb", "client_secret", "fake-secret"},
	}
	for _, s := range set {
		if err := setPCAccount(s[0], s[1], s[2]); err != nil {
			t.Fatal(err)
		}
	}
	if err := setPCAccount("screenscraper", "nope", "x"); err == nil {
		t.Error("an unknown key was kept")
	}

	// read back from the file, as a fresh start would
	cfgMu.Lock()
	cfg = config{}
	cfgMu.Unlock()
	loadConfig()
	acc := pcAccounts()
	for _, s := range set {
		if acc[s[0]][s[1]] != s[2] {
			t.Errorf("%s.%s = %q, want %q", s[0], s[1], acc[s[0]][s[1]], s[2])
		}
	}

	// secrets go through protect(), never into the plain map
	b, err := os.ReadFile(configPath())
	if err != nil {
		t.Fatal(err)
	}
	var raw struct {
		Accounts    map[string]map[string]string `json:"accounts"`
		AccountsEnc map[string]map[string]string `json:"accounts_protected"`
	}
	if err := json.Unmarshal(b, &raw); err != nil {
		t.Fatal(err)
	}
	for sec, keys := range raw.Accounts {
		for key := range keys {
			if acctSecret(sec, key) {
				t.Errorf("%s.%s kept as plain text", sec, key)
			}
		}
	}
	if raw.AccountsEnc["screenscraper"]["password"] == "fake-pass" || raw.AccountsEnc["screenscraper"]["password"] == "" {
		t.Error("the password isn't kept protected")
	}
	if raw.Accounts["screenscraper"]["user"] != "fake-user" {
		t.Error("the user name wasn't kept")
	}

	// what Settings sees: no secret values
	for _, a := range pcAccountView() {
		if acctSecret(a["sec"].(string), a["key"].(string)) {
			if _, has := a["value"]; has {
				t.Errorf("view shows %v.%v", a["sec"], a["key"])
			}
		}
	}

	// "" removes it
	if err := setPCAccount("thegamesdb", "apikey", ""); err != nil {
		t.Fatal(err)
	}
	if pcAccounts()["thegamesdb"]["apikey"] != "" {
		t.Error("the key wasn't removed")
	}
}

// set on the SS1, not on this PC: skipped, with one plain note
func TestSS1OnlySkips(t *testing.T) {
	s := &scrapeRun{
		accounts: map[string]map[string]string{"igdb": {"client_id": "fake-id", "client_secret": "fake-secret"}},
		// an older SS1 still sends values: they count as set, but aren't used
		ss1Set: ss1SetFrom(map[string]map[string]bool{"thegamesdb": {"apikey": true}, "screenscraper": {"user": true, "password": true}},
			map[string]map[string]string{"igdb": {"client_id": "fake-id"}}),
	}
	ready := map[string]bool{}
	for i := range scrapeSources {
		ready[scrapeSources[i].id] = scrapeSources[i].ready(s)
		ready[scrapeSources[i].id] = scrapeSources[i].ready(s) // twice: still one note each
	}
	if ready["thegamesdb"] || ready["screenscraper"] || !ready["igdb"] {
		t.Errorf("ready = %v", ready)
	}
	want := []string{
		"TheGamesDB is set up on the SS1: enter it in Phosphor for Windows too to use it from this PC",
		"ScreenScraper is set up on the SS1: enter it in Phosphor for Windows too to use it from this PC",
	}
	if strings.Join(s.st.Notes, "\n") != strings.Join(want, "\n") {
		t.Errorf("notes = %q", s.st.Notes)
	}

	// nothing on the SS1 either: ScreenScraper says what's missing, as before
	s = &scrapeRun{}
	for i := range scrapeSources {
		if scrapeSources[i].id == "screenscraper" {
			scrapeSources[i].ready(s)
		}
	}
	if len(s.st.Notes) != 1 || !strings.HasPrefix(s.st.Notes[0], "ScreenScraper needs") {
		t.Errorf("notes = %q", s.st.Notes)
	}
	// a developer login alone on the SS1 isn't the person's account
	if m := ss1Only(map[string]map[string]bool{"screenscraper": {"devid": true, "devpassword": true}}, "screenscraper", false); m != "" {
		t.Errorf("devid only: %q", m)
	}
}
