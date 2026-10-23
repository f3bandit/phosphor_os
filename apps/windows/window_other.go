//go:build !windows

package main

import (
	"fmt"
	"os"
)

func openWindow(url string) { fmt.Println("open", url) }
func otherInstance() string { return "" }

func runTray(url string) { select {} }
func closeWindows()      {}
func quitApp()           { os.Exit(0) }
