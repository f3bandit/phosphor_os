//go:build windows

package main

import (
	"os"
	"os/exec"
	"path/filepath"
	"syscall"
)

// the window: Edge in app mode (no address bar or tabs) with its own profile; without Edge, the
// default browser
func openWindow(url string) {
	for _, env := range []string{"ProgramFiles(x86)", "ProgramFiles", "LocalAppData"} {
		base := os.Getenv(env)
		if base == "" {
			continue
		}
		edge := filepath.Join(base, "Microsoft", "Edge", "Application", "msedge.exe")
		if _, err := os.Stat(edge); err != nil {
			continue
		}
		c := exec.Command(edge, "--app="+url, "--window-size=1500,920", "--user-data-dir="+filepath.Join(os.Getenv("LocalAppData"), "PhosphorTextureMaker", "window"),
			"--no-first-run", "--disable-features=Translate")
		c.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
		if c.Start() == nil {
			return
		}
	}
	_ = exec.Command("rundll32", "url.dll,FileProtocolHandler", url).Start()
}
