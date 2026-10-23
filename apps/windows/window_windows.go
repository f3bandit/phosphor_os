//go:build windows

package main

// The window: Edge in app mode (no address bar or tabs), with its own profile so it opens fast and
// remembers its size. Without Edge, the default browser.

import (
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"time"
)

func edgePath() string {
	for _, env := range []string{"ProgramFiles(x86)", "ProgramFiles", "LocalAppData"} {
		if base := os.Getenv(env); base != "" {
			p := filepath.Join(base, "Microsoft", "Edge", "Application", "msedge.exe")
			if _, err := os.Stat(p); err == nil {
				return p
			}
		}
	}
	return ""
}

func openWindow(url string) {
	if edge := edgePath(); edge != "" {
		c := exec.Command(edge, "--app="+url, "--window-size=1440,900", "--user-data-dir="+filepath.Join(configDir(), "window"),
			"--no-first-run", "--disable-features=Translate")
		c.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
		if c.Start() == nil {
			return
		}
	}
	_ = exec.Command("rundll32", "url.dll,FileProtocolHandler", url).Start()
}

// otherInstance: the address of a copy that's already running (it answers), or ""
func otherInstance() string {
	b, err := os.ReadFile(instancePath())
	if err != nil {
		return ""
	}
	url := strings.TrimSpace(string(b))
	c := http.Client{Timeout: 700 * time.Millisecond}
	if r, err := c.Get(strings.Replace(url, "/?k=", "/api/hello?k=", 1)); err == nil {
		r.Body.Close()
		if r.StatusCode == 200 {
			return url
		}
	}
	return ""
}
