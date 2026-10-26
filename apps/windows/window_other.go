//go:build !windows

package main

import (
	"fmt"
	"os"
)

func openWindow(url string) <-chan struct{} {
	fmt.Println("open", url)
	c := make(chan struct{})
	close(c)
	return c
}
func otherInstance() string          { return "" }
func replaceOlder(url string) string { return url }

func runTray(url string)                       { select {} }
func closeWindows()                            {}
func quitApp()                                 { os.Exit(0) }
func openFolder(p string)                      { fmt.Println("explore", p) }
func trayNotify(title, body string, warn bool) { fmt.Println("notify:", title, "-", body) }
func trayAlert(on bool)                        {}
func focusWindow() bool                        { return false }
