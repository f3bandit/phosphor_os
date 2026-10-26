// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// The box-art logins this PC uses (Settings > Box art). The SS1 no longer sends passwords and keys
// (remote access is plain HTTP), so the app keeps its own copy: user names and IDs in config.json,
// passwords and keys protected like the pairing key (secret_windows.go). Saving one here also
// sends it to the SS1, for its own lookups.

import (
	"net/http"
	"strings"
)

// the account keys, as phosphord's ACCT list; true for the ones that are secret
var acctKeys = map[string]map[string]bool{
	"screenscraper": {"user": false, "password": true, "devid": false, "devpassword": true},
	"thegamesdb":    {"apikey": true},
	"igdb":          {"client_id": false, "client_secret": true},
}

var acctNames = map[string]string{"screenscraper": "ScreenScraper", "thegamesdb": "TheGamesDB", "igdb": "IGDB"}

func acctSecret(sec, key string) bool { return acctKeys[sec][key] }

// setPCAccount keeps one login on this PC ("" removes it)
func setPCAccount(sec, key, val string) error {
	if _, ok := acctKeys[sec][key]; !ok {
		return errBadAccount
	}
	val = strings.TrimSpace(val)
	enc := val
	if val != "" && acctSecret(sec, key) {
		var err error
		if enc, err = protect(val); err != nil {
			return err
		}
	}
	cfgMu.Lock()
	m := &cfg.Accounts
	if acctSecret(sec, key) {
		m = &cfg.AccountsEnc
	}
	if *m == nil {
		*m = map[string]map[string]string{}
	}
	if (*m)[sec] == nil {
		(*m)[sec] = map[string]string{}
	}
	if enc == "" {
		delete((*m)[sec], key)
	} else {
		(*m)[sec][key] = enc
	}
	cfgMu.Unlock()
	return saveConfig()
}

// pcAccounts: this PC's logins, readable (a fresh copy)
func pcAccounts() map[string]map[string]string {
	cfgMu.Lock()
	plain, enc := cfg.Accounts, cfg.AccountsEnc
	out := map[string]map[string]string{}
	for sec, keys := range acctKeys {
		out[sec] = map[string]string{}
		for key, secret := range keys {
			if !secret {
				if v := plain[sec][key]; v != "" {
					out[sec][key] = v
				}
				continue
			}
			if e := enc[sec][key]; e != "" {
				if v, err := unprotect(e); err == nil && v != "" {
					out[sec][key] = v
				}
			}
		}
	}
	cfgMu.Unlock()
	return out
}

// ss1Only: when the SS1 has a source's login but this PC hasn't (enough of) it, what to tell the
// person; "" otherwise. ready says whether this PC's copy is enough.
func ss1Only(ss1Set map[string]map[string]bool, sec string, ready bool) string {
	if ready {
		return ""
	}
	has := false
	for key := range ss1Set[sec] {
		// a developer login alone isn't the person's account (Phosphor's own may be built in)
		if key != "devid" && key != "devpassword" {
			has = true
		}
	}
	if !has {
		return ""
	}
	return acctNames[sec] + " is set up on the SS1: enter it in Phosphor for Windows too to use it from this PC"
}

// ss1SetFrom: which logins the SS1 has, from scrape_todo's "set" (or, from an SS1 that still sends
// the values, from those)
func ss1SetFrom(set map[string]map[string]bool, acc map[string]map[string]string) map[string]map[string]bool {
	out := map[string]map[string]bool{}
	add := func(sec, key string) {
		if out[sec] == nil {
			out[sec] = map[string]bool{}
		}
		out[sec][key] = true
	}
	for sec, keys := range set {
		for key, on := range keys {
			if on {
				add(sec, key)
			}
		}
	}
	for sec, keys := range acc {
		for key, v := range keys {
			if v != "" {
				add(sec, key)
			}
		}
	}
	return out
}

type acctErr string

func (e acctErr) Error() string { return string(e) }

const errBadAccount = acctErr("not an account setting")

// pcAccountView: what Settings shows: user names and IDs as they are, secrets as set / not set
func pcAccountView() []map[string]any {
	acc := pcAccounts()
	var out []map[string]any
	for sec, keys := range acctKeys {
		for key, secret := range keys {
			v := acc[sec][key]
			a := map[string]any{"sec": sec, "key": key, "set": v != ""}
			if !secret {
				a["value"] = v
			}
			out = append(out, a)
		}
	}
	return out
}

// /api/accounts: GET this PC's logins (no secrets); POST {sec, key, value} keeps one on this PC and
// sends it to the SS1 too. The answer has both: "pc" and the SS1's "accounts".
func apiAccounts(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeJSON(w, map[string]any{"ok": true, "pc": pcAccountView()})
		return
	}
	var req struct{ Sec, Key, Value string }
	if readJSON(r, &req) != nil {
		fail(w, "bad request")
		return
	}
	if err := setPCAccount(req.Sec, req.Key, req.Value); err != nil {
		fail(w, "Couldn't keep it on this PC: "+err.Error())
		return
	}
	var j struct {
		Accounts any `json:"accounts"`
	}
	if err := ss1Call(map[string]any{"cmd": "scrape_accounts", "set": map[string]string{"sec": req.Sec, "key": req.Key, "value": req.Value}}, &j); err != nil {
		writeJSON(w, map[string]any{"ok": false, "error": "Kept on this PC, but the SS1 didn't take it: " + err.Error(), "pc": pcAccountView()})
		return
	}
	writeJSON(w, map[string]any{"ok": true, "pc": pcAccountView(), "accounts": j.Accounts})
}
