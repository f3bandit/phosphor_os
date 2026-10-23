//go:build windows

package main

// The tray icon: closing the window leaves Phosphor running (box art on the PC keeps going); the
// tray's menu opens it again or exits completely, window and all.

import (
	_ "embed"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"

	"fyne.io/systray"
)

//go:embed assets/phosphor.ico
var trayIcon []byte

func quitApp() { closeWindows(); systray.Quit() }

func runTray(url string) {
	systray.Run(func() {
		systray.SetIcon(trayIcon)
		systray.SetTooltip("Phosphor")
		open := systray.AddMenuItem("Open Phosphor", "Show the Phosphor window")
		systray.AddSeparator()
		quit := systray.AddMenuItem("Exit Phosphor", "Close the window and stop Phosphor")
		go func() {
			for {
				select {
				case <-open.ClickedCh:
					openWindow(url)
				case <-quit.ClickedCh:
					closeWindows()
					systray.Quit()
					return
				}
			}
		}()
	}, func() {})
}

// closeWindows: the Edge windows on Phosphor's own window profile (never the user's own Edge)
func closeWindows() {
	prof := strings.ToLower(filepath.Join(configDir(), "window"))
	ps := `Get-CimInstance Win32_Process -Filter "Name='msedge.exe'" | Where-Object { $_.CommandLine -and $_.CommandLine.ToLower().Contains('` +
		strings.ReplaceAll(prof, "'", "''") + `') } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }`
	c := exec.Command("powershell", "-NoProfile", "-NonInteractive", "-Command", ps)
	c.SysProcAttr = &syscall.SysProcAttr{HideWindow: true, CreationFlags: 0x08000000}
	_ = c.Run()
}

func openFolder(p string) { _ = exec.Command("explorer.exe", p).Start() }
