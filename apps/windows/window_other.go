//go:build !windows

package main

import "fmt"

func openWindow(url string) { fmt.Println("open", url) }
func otherInstance() string { return "" }
