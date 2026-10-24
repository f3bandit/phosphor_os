//go:build windows

package main

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"time"
)

func hidden(c *exec.Cmd) *exec.Cmd {
	c.SysProcAttr = &syscall.SysProcAttr{HideWindow: true, CreationFlags: 0x08000000} // CREATE_NO_WINDOW
	return c
}

func powershell(script string) (string, error) {
	out, err := hidden(exec.Command("powershell", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script)).CombinedOutput()
	return strings.TrimSpace(string(out)), err
}

// the disks that can be SD cards, never the boot or system disk: connected as SD, USB or MMC, or with
// a volume Windows calls Removable, or named / listed as removable media (as SS1 Tool)
const cardDiskPS = `$ErrorActionPreference='SilentlyContinue'
$sys = @(Get-Disk | Where-Object { $_.IsBoot -or $_.IsSystem } | ForEach-Object { $_.Number })
$remVol = @(Get-Partition | Where-Object { $_.DriveLetter } | Where-Object { (Get-Volume -Partition $_).DriveType -eq 'Removable' } | ForEach-Object { $_.DiskNumber })
$remDrv = @(Get-CimInstance Win32_DiskDrive | Where-Object { $_.MediaType -like '*Removable*' -or $_.Model -match '(?i)\bSD\b|SDXC|SDHC|MMC|card' } | ForEach-Object { [int]$_.Index })
`

func listRemovable() ([]disk, error) {
	out, err := powershell(cardDiskPS + `@(Get-Disk | Where-Object { ($sys -notcontains $_.Number) -and ((@('SD','USB','MMC') -contains [string]$_.BusType) -or ($remVol -contains $_.Number) -or ($remDrv -contains $_.Number)) } | Select-Object Number,FriendlyName,Size,@{n='BusType';e={[string]$_.BusType}},LogicalSectorSize,@{n='Letters';e={(@(Get-Partition -DiskNumber $_.Number | Where-Object { $_.DriveLetter } | ForEach-Object { [string]$_.DriveLetter + ':' }) -join ' ')}}) | ConvertTo-Json -Compress`)
	if err != nil {
		return nil, fmt.Errorf("couldn't list the disks: %s", out)
	}
	out = strings.TrimSpace(out)
	if out == "" || out == "null" {
		return []disk{}, nil
	}
	if strings.HasPrefix(out, "{") {
		out = "[" + out + "]"
	}
	var d []disk
	if err := json.Unmarshal([]byte(out), &d); err != nil {
		return nil, fmt.Errorf("couldn't read the disk list: %v", err)
	}
	res := []disk{}
	for _, x := range d {
		if x.Size > 0 && x.Size <= 2_100_000_000_000 { // no SD card is bigger: never an external hard drive
			res = append(res, x)
		}
	}
	return res, nil
}

func diskReport() ([]diskInfo, error) {
	out, err := powershell(cardDiskPS + `$drv=@{}; Get-CimInstance Win32_DiskDrive | ForEach-Object { $drv[[int]$_.Index] = [string]$_.MediaType }
@(Get-Disk | ForEach-Object { $n=$_.Number
 $l=(@(Get-Partition -DiskNumber $n | Where-Object { $_.DriveLetter } | ForEach-Object { [string]$_.DriveLetter + ':' }) -join ' ')
 [ordered]@{number=$n; name=[string]$_.FriendlyName; size=[int64]$_.Size; bus=[string]$_.BusType; media=[string]$drv[$n]; letters=$l;
  boot=[bool]($sys -contains $n); status=[string]$_.OperationalStatus; health=[string]$_.HealthStatus;
  candidate=[bool]((@('SD','USB','MMC') -contains [string]$_.BusType) -or ($remVol -contains $n) -or ($remDrv -contains $n))} }) | ConvertTo-Json -Compress`)
	if err != nil {
		return nil, fmt.Errorf("couldn't list the disks: %s", out)
	}
	out = strings.TrimSpace(out)
	if strings.HasPrefix(out, "{") {
		out = "[" + out + "]"
	}
	var raw []struct {
		diskInfo
		Candidate bool `json:"candidate"`
	}
	if out != "" && out != "null" {
		if err := json.Unmarshal([]byte(out), &raw); err != nil {
			return nil, fmt.Errorf("couldn't read the disk list: %v", err)
		}
	}
	res := []diskInfo{}
	for _, r := range raw {
		d := r.diskInfo
		switch {
		case d.Boot:
			d.Reason = "Windows runs from this disk"
		case d.Size == 0:
			d.Reason = "No card in this reader (or Windows can't read it)"
		case d.Size > 2_100_000_000_000:
			d.Reason = "Too big to be an SD card"
		case !r.Candidate:
			d.Reason = "Doesn't look like a card reader (connected as " + d.BusType + ", not removable)"
		default:
			d.Offered, d.Reason = true, "Offered"
		}
		res = append(res, d)
	}
	return res, nil
}

func pickImageFile() (string, error) {
	ps := `Add-Type -AssemblyName System.Windows.Forms
$d = New-Object System.Windows.Forms.OpenFileDialog
$d.Title = 'Choose an SD card image'
$d.Filter = 'SD card images (*.img;*.zip)|*.img;*.zip|All files (*.*)|*.*'
$o = New-Object System.Windows.Forms.Form -Property @{TopMost=$true; ShowInTaskbar=$false; Opacity=0; FormBorderStyle='None'; StartPosition='CenterScreen'; Width=1; Height=1}
$o.Show(); $o.Activate()
if ($d.ShowDialog($o) -eq [System.Windows.Forms.DialogResult]::OK) { [Console]::Out.Write($d.FileName) }
$o.Close(); $o.Dispose()`
	out, err := hidden(exec.Command("powershell", "-NoProfile", "-STA", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", ps)).Output()
	if err != nil {
		return "", fmt.Errorf("the file window didn't open: %v", err)
	}
	return strings.TrimSpace(string(out)), nil
}

// startHelper: this program again, as administrator (Windows asks the user), for one job
func startHelper(args []string) error {
	exe, err := os.Executable()
	if err != nil {
		return err
	}
	q := make([]string, len(args))
	for i, a := range args {
		q[i] = "'" + strings.ReplaceAll(`"`+a+`"`, "'", "''") + "'"
	}
	_, err = powershell(`Start-Process -WindowStyle Hidden -FilePath '` + strings.ReplaceAll(exe, "'", "''") + `' -Verb RunAs -ArgumentList @(` + strings.Join(q, ",") + `)`)
	if err != nil {
		return fmt.Errorf("Windows' prompt was declined, so nothing was done")
	}
	return nil
}

func flash(d disk, image string) error {
	// 1. clear the partition table so Windows lets go of every volume on the card
	script := fmt.Sprintf("select disk %d\r\nattributes disk clear readonly noerr\r\nclean\r\nrescan\r\nexit\r\n", d.Number)
	sf := filepath.Join(os.TempDir(), "phosphor_diskpart.txt")
	if err := os.WriteFile(sf, []byte(script), 0o644); err != nil {
		return err
	}
	defer os.Remove(sf)
	sdSet(func(s *sdState) {
		s.Phase, s.Message = "prepare", fmt.Sprintf("Clearing disk %d (%s)", d.Number, d.FriendlyName)
	})
	if out, err := hidden(exec.Command("diskpart", "/s", sf)).CombinedOutput(); err != nil {
		return fmt.Errorf("Windows couldn't clear the card: %s", strings.TrimSpace(string(out)))
	}
	time.Sleep(2 * time.Second)
	dev := fmt.Sprintf(`\\.\PHYSICALDRIVE%d`, d.Number)
	f, err := os.OpenFile(dev, os.O_RDWR, 0)
	if err != nil {
		return fmt.Errorf("couldn't open the card: %v", err)
	}
	defer f.Close()
	if err := writeImage(f, image, d.LogicalSectorSize); err != nil {
		return err
	}
	f.Close()
	_, _ = hidden(exec.Command("powershell", "-NoProfile", "-Command", "Update-HostStorageCache")).CombinedOutput()
	return nil
}

func readTest(d disk) error {
	dev := fmt.Sprintf(`\\.\PHYSICALDRIVE%d`, d.Number)
	f, err := os.Open(dev)
	if err != nil {
		return fmt.Errorf("couldn't open the card: %v", err)
	}
	defer f.Close()
	return readTestDev(f, d.Size)
}
