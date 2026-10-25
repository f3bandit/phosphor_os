// Phosphor for Windows: ScreenScraper's login, the way Console Mode does it (#137). Every request needs
// a developer login (devid / devpassword: Phosphor's own, built in, or the person's own) and the
// person's screenscraper.fr login (ssid / sspassword). The person's login is never sent as the
// developer login: ScreenScraper refuses that.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
package main

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"strings"
)

// Phosphor's own developer login, set at build time (build-exe.sh, from PHOSPHOR_SS_DEVID and
// PHOSPHOR_SS_DEVPASSWORD: a build secret, never in the repo). Empty until ScreenScraper gives one.
var ssBuiltinID, ssBuiltinPw string

// ssDevLogin: Phosphor's developer login, or the person's own (Sources › ScreenScraper)
func ssDevLogin(acct func(sec, key string) string) (id, pw string) {
	if ssBuiltinID != "" && ssBuiltinPw != "" {
		return ssBuiltinID, ssBuiltinPw
	}
	return acct("screenscraper", "devid"), acct("screenscraper", "devpassword")
}

// ssMissing: what's missing for ScreenScraper, in words ("" when all four are there)
func ssMissing(acct func(sec, key string) string) string {
	var m []string
	if acct("screenscraper", "user") == "" {
		m = append(m, "your user name")
	}
	if acct("screenscraper", "password") == "" {
		m = append(m, "your password")
	}
	if id, pw := ssDevLogin(acct); id == "" || pw == "" {
		m = append(m, "a developer ID and password (Phosphor's own is on its way from ScreenScraper; until then, your own if you have one)")
	}
	if len(m) == 0 {
		return ""
	}
	return "ScreenScraper needs " + strings.Join(m, ", ")
}

// ssAnswer: ScreenScraper's reply in words, and whether it means stopping (for the rest of a run).
// "" for an answer that's fine (found, or not found).
func ssAnswer(rc, http int, body []byte) (msg string, stop bool) {
	b := strings.ToLower(string(body))
	switch {
	case strings.Contains(b, "erreur de login") && strings.Contains(b, "développeur"),
		strings.Contains(b, "erreur de login") && strings.Contains(b, "developpeur"):
		return "ScreenScraper refused the developer ID", true
	case strings.Contains(b, "erreur de login"):
		return "ScreenScraper refused your login: check the user name and password", true
	case http == 401:
		return "ScreenScraper's API is closed to non-members right now: try later", true
	case http == 423:
		return "ScreenScraper is down for maintenance: try later", true
	case http == 426:
		return "ScreenScraper blocked this version of Phosphor: update it", true
	case http == 430:
		return "Today's ScreenScraper lookups are used up: it goes on tomorrow", true
	case http == 431:
		return "Too many not-found lookups at ScreenScraper today: it goes on tomorrow", true
	case http == 403:
		return fmt.Sprintf("ScreenScraper refused the request (HTTP 403)"), true
	case rc < 0:
		return "Can't reach screenscraper.fr", false
	}
	return "", false
}

// ssGate: ScreenScraper during one run: off for the rest of it after a refused login, a used-up day,
// or 3 failed requests in a row; the reason is noted once
type ssGate struct {
	off   string
	fails int
}

// ssCheck: Check login (Sources › ScreenScraper): asks ScreenScraper for the account's details.
func ssCheck(ctx context.Context, acct func(sec, key string) string) (string, bool) {
	if m := ssMissing(acct); m != "" {
		return m, false
	}
	id, pw := ssDevLogin(acct)
	q := url.Values{"devid": {id}, "devpassword": {pw}, "softname": {"Phosphor"}, "output": {"json"},
		"ssid": {acct("screenscraper", "user")}, "sspassword": {acct("screenscraper", "password")}}
	b, rc, http := netRequest(ctx, "GET", "https://api.screenscraper.fr/api2/ssuserInfos.php?"+q.Encode(), nil, nil, 1<<20)
	if msg, _ := ssAnswer(rc, http, b); msg != "" {
		return msg, false
	}
	if rc != 0 {
		return fmt.Sprintf("ScreenScraper didn't answer (HTTP %d)", http), false
	}
	var j struct {
		Response struct {
			User struct {
				Today   string `json:"requeststoday"`
				Max     string `json:"maxrequestsperday"`
				Threads string `json:"maxthreads"`
			} `json:"ssuser"`
		} `json:"response"`
	}
	if json.Unmarshal(b, &j) != nil || j.Response.User.Max == "" {
		return "ScreenScraper's answer couldn't be read", false
	}
	u := j.Response.User
	return fmt.Sprintf("Login works: %s of %s lookups used today", u.Today, u.Max), true
}

// /api/sscheck: Check login, with the login saved on the SS1 (Sources › ScreenScraper)
func apiSSCheck(w http.ResponseWriter, r *http.Request) {
	var raw struct {
		Miss   bool `json:"miss"`
		Config struct {
			Accounts map[string]map[string]string `json:"accounts"`
		} `json:"config"`
	}
	if err := ss1Call(map[string]any{"cmd": "scrape_todo", "miss": true, "limit": 1}, &raw); err != nil {
		fail(w, "The SS1 didn't answer: "+err.Error())
		return
	}
	if !raw.Miss {
		fail(w, "Update Phosphor on the SS1 (0.27.32 or later) to check the login from here")
		return
	}
	acc := raw.Config.Accounts
	msg, ok := ssCheck(r.Context(), func(sec, key string) string { return acc[sec][key] })
	writeJSON(w, map[string]any{"ok": true, "works": ok, "message": msg})
}
