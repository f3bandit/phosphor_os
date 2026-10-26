//go:build windows

package main

// The window: Edge in app mode (no address bar or tabs), with its own profile so it opens fast and
// remembers its size. Without Edge, the default browser.
//
// The profile is a different one when Phosphor runs as administrator: an Edge started as administrator
// on a profile keeps it (Edge stays running in the background), and an Edge started normally then can't
// hand the window over to it, so it opened nothing until Phosphor was run as administrator again.

import (
	"encoding/json"
	"unsafe"

	"golang.org/x/sys/windows"

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

// windowProfile: Edge's profile for Phosphor's window, one for each of normal and administrator
func windowProfile() string {
	name := "window"
	if windows.GetCurrentProcessToken().IsElevated() {
		name = "window-admin"
	}
	return filepath.Join(configDir(), name)
}

func openBrowser(url string) {
	_ = exec.Command("rundll32", "url.dll,FileProtocolHandler", url).Start()
}

func openWindow(url string) {
	if edge := edgePath(); edge != "" {
		c := exec.Command(edge, "--app="+url, "--window-size=1440,900", "--user-data-dir="+windowProfile(),
			"--no-first-run", "--disable-features=Translate")
		c.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
		if c.Start() == nil {
			return
		}
	}
	openBrowser(url)
}

// otherInstance: the address of a copy that's already running (it answers), or ""
// an older copy still running (in the tray, say) when a new version is started: it's asked to quit,
// so the new one takes over instead of just showing the old window. "" when it went.
func replaceOlder(url string) string {
	c := http.Client{Timeout: 2 * time.Second}
	r, err := c.Get(strings.Replace(url, "/?k=", "/api/state?k=", 1))
	if err != nil {
		return url
	}
	var st struct {
		V string `json:"app_version"`
	}
	_ = json.NewDecoder(r.Body).Decode(&st)
	r.Body.Close()
	if st.V == appVersion {
		return url
	}
	if r, err := c.Post(strings.Replace(url, "/?k=", "/api/quit?k=", 1), "application/json", strings.NewReader("{}")); err == nil {
		r.Body.Close()
	}
	for i := 0; i < 50; i++ {
		time.Sleep(100 * time.Millisecond)
		if otherInstance() == "" {
			return ""
		}
	}
	return url
}

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

var (
	pFindWindow       = windows.NewLazySystemDLL("user32.dll").NewProc("FindWindowW")
	pShowWindow       = windows.NewLazySystemDLL("user32.dll").NewProc("ShowWindow")
	pIsIconic         = windows.NewLazySystemDLL("user32.dll").NewProc("IsIconic")
	pSetForegroundWnd = windows.NewLazySystemDLL("user32.dll").NewProc("SetForegroundWindow")
)

// focusWindow: Phosphor's window (Edge app window titled "Phosphor") to the front; false if none is open
func focusWindow() bool {
	cls, _ := windows.UTF16PtrFromString("Chrome_WidgetWin_1")
	t, _ := windows.UTF16PtrFromString("Phosphor")
	h, _, _ := pFindWindow.Call(uintptr(unsafe.Pointer(cls)), uintptr(unsafe.Pointer(t)))
	if h == 0 {
		return false
	}
	if ic, _, _ := pIsIconic.Call(h); ic != 0 {
		pShowWindow.Call(h, 9) // SW_RESTORE
	}
	pSetForegroundWnd.Call(h)
	return true
}
