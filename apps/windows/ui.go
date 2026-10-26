// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// Showing the window: the open one to the front (with a screen to switch to, picked up by the page),
// or a new one.

import "sync"

var view struct {
	sync.Mutex
	url, next string
}

// showUI: bring Phosphor's window up, on screen v ("" for wherever it is, "notifications")
func showUI(v string) {
	view.Lock()
	view.next = v
	u := view.url
	view.Unlock()
	if focusWindow() {
		return
	}
	if v != "" {
		u += "#" + v
	}
	openWindow(u)
}

// takeView: the screen the page should switch to, once
func takeView() string {
	view.Lock()
	defer view.Unlock()
	v := view.next
	view.next = ""
	return v
}
