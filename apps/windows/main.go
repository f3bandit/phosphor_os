// Phosphor for Windows: the PC companion for Phosphor on the SuperStation One.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
//
// One exe, no install. It serves its screens on 127.0.0.1 (a random port, with a key in the URL so
// nothing else on the PC can use it) and shows them in an Edge app window. It talks to the SS1 over
// Phosphor's remote access (docs/remote-api.md), paired with the code the TV shows.
package main

import (
	"crypto/rand"
	"embed"
	"encoding/hex"
	"flag"
	"fmt"
	"io/fs"
	"log"
	"net"
	"net/http"
	"os"
)

const appVersion = "0.2.37"

//go:embed web
var webFS embed.FS

var sessionKey string

func main() {
	if runSDJob(os.Args[1:]) { // the SD card helper (as administrator): one job, then it exits
		return
	}
	noWindow := flag.Bool("no-window", false, "don't open a window (print the address)")
	port := flag.Int("port", 0, "port on 127.0.0.1 (0: any free one)")
	flag.Parse()

	url := otherInstance()
	if url != "" {
		url = replaceOlder(url) // an older version: it quits, this one takes over
	}
	if url != "" && !*noWindow { // already running: show that window
		if !focusWindow() {
			openWindow(url)
		}
		return
	}
	b := make([]byte, 16)
	if _, err := rand.Read(b); err != nil {
		log.Fatal(err)
	}
	sessionKey = hex.EncodeToString(b)
	loadConfig()

	ln, err := net.Listen("tcp", fmt.Sprintf("127.0.0.1:%d", *port))
	if err != nil {
		log.Fatal(err)
	}
	url = fmt.Sprintf("http://%s/?k=%s", ln.Addr(), sessionKey)
	mux := http.NewServeMux()
	web, _ := fs.Sub(webFS, "web")
	mux.Handle("/", http.FileServer(http.FS(web)))
	routes(mux)
	saveInstance(url)
	view.url = url
	go watchSS1() // warnings from the SS1 -> notifications (in the window and from the tray)
	if *noWindow {
		fmt.Println(url)
	} else {
		openWindow(url)
	}
	srv := &http.Server{Handler: mux}
	go func() {
		if err := srv.Serve(ln); err != nil && err != http.ErrServerClosed {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
	}()
	runTray(url) // the tray icon (Windows): Open Phosphor, Exit. Returns when it's time to quit.
	os.Remove(instancePath())
}
