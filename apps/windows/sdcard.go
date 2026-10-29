// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).

package main

// Tools > SD card flash & health: put Phosphor's SD card image on a card in this PC's reader (the same images and the same careful write as SS1 Tool: partition table last, then the
// whole card read back and compared), and a read test of a card (every part read once, nothing
// written). Writing to a disk needs administrator rights: the window never has them. A small helper
// (this same program, started with --sdjob through Windows' own "allow this app" prompt) does the one
// job and writes its progress to a file the window reads.

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"time"
)

// Phosphor's SD card image (MiSTer with Phosphor on it), from Phosphor's releases. Only Phosphor's:
// no other SuperStation image is offered here.
const phosphorRepo = "f3bandit/phosphor_os"

type flashImage struct {
	Kind      string `json:"kind"` // phosphor
	Label     string `json:"label"`
	Name      string `json:"name"`
	Tag       string `json:"tag"`
	Release   string `json:"release"`
	URL       string `json:"url"`
	Size      int64  `json:"size"`
	Published string `json:"published"`
	Have      bool   `json:"have"` // already downloaded on this PC
}

type ghAsset struct {
	Name string `json:"name"`
	Size int64  `json:"size"`
	URL  string `json:"browser_download_url"`
}

type ghRelease struct {
	Tag        string    `json:"tag_name"`
	Name       string    `json:"name"`
	Draft      bool      `json:"draft"`
	Prerelease bool      `json:"prerelease"`
	Published  string    `json:"published_at"`
	Assets     []ghAsset `json:"assets"`
}

// phosphorImage: the SD card image of the newest Phosphor release that has one
// (phosphor-<v>-sdcard-<date>.img.zip; drafts left out, as Phosphor's own updater does)
func phosphorImage(rels []ghRelease) (flashImage, bool) {
	for _, r := range rels {
		if r.Draft {
			continue
		}
		for _, a := range r.Assets {
			n := strings.ToLower(a.Name)
			if strings.HasPrefix(n, "phosphor-") && strings.Contains(n, "-sdcard-") && strings.HasSuffix(n, ".img.zip") {
				name := r.Name
				if name == "" {
					name = "Phosphor " + r.Tag
				}
				return flashImage{Kind: "phosphor", Label: "Phosphor OS", Name: a.Name, Tag: r.Tag, Release: name, URL: a.URL, Size: a.Size, Published: r.Published}, true
			}
		}
	}
	return flashImage{}, false
}

var webClient = &http.Client{Timeout: 30 * time.Second}

func webGet(url string) ([]byte, error) {
	req, _ := http.NewRequest("GET", url, nil)
	req.Header.Set("User-Agent", "Phosphor/"+appVersion)
	resp, err := webClient.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("HTTP %d", resp.StatusCode)
	}
	return io.ReadAll(io.LimitReader(resp.Body, 4<<20))
}

var assetHrefRe = regexp.MustCompile(`href="(/[^"]+/releases/download/([^/"]+)/([^"/]+))"`)

// imagesFromHTML: when GitHub's API says "too many requests", the latest release page
func imagesFromHTML() ([]flashImage, error) {
	resp, err := webClient.Get("https://github.com/" + phosphorRepo + "/releases/latest")
	if err != nil {
		return nil, err
	}
	resp.Body.Close()
	tag := ""
	if p := resp.Request.URL.Path; strings.Contains(p, "/releases/tag/") {
		tag = p[strings.LastIndex(p, "/")+1:]
	}
	if tag == "" {
		return nil, errors.New("couldn't find the latest release")
	}
	b, err := webGet("https://github.com/" + phosphorRepo + "/releases/expanded_assets/" + tag)
	if err != nil {
		return nil, err
	}
	rel := ghRelease{Tag: tag, Name: "Phosphor " + tag}
	for _, m := range assetHrefRe.FindAllStringSubmatch(string(b), -1) {
		rel.Assets = append(rel.Assets, ghAsset{Name: m[3], URL: "https://github.com" + m[1]})
	}
	if p, ok := phosphorImage([]ghRelease{rel}); ok {
		return []flashImage{p}, nil
	}
	return nil, nil
}

var (
	imgMu    sync.Mutex
	imgCache []flashImage
	imgTime  time.Time
)

func installerImages(force bool) ([]flashImage, error) {
	imgMu.Lock()
	defer imgMu.Unlock()
	if !force && imgCache != nil && time.Since(imgTime) < 10*time.Minute {
		return imgCache, nil
	}
	var imgs []flashImage
	b, err := webGet("https://api.github.com/repos/" + phosphorRepo + "/releases?per_page=10")
	if err == nil {
		var rels []ghRelease
		if json.Unmarshal(b, &rels) == nil {
			if p, ok := phosphorImage(rels); ok {
				imgs = append(imgs, p)
			}
		}
	}
	if len(imgs) == 0 {
		h, herr := imagesFromHTML()
		if herr != nil && err != nil {
			return nil, fmt.Errorf("couldn't reach GitHub: %v", err)
		}
		imgs = h
	}
	if len(imgs) == 0 {
		return nil, errors.New("no Phosphor SD card image in the latest release")
	}
	imgCache, imgTime = imgs, time.Now()
	return imgs, nil
}

func imageCacheDir() string { return filepath.Join(configDir(), "images") }

func haveImage(a flashImage) (string, bool) {
	p := filepath.Join(imageCacheDir(), a.Name)
	st, err := os.Stat(p)
	return p, err == nil && (a.Size == 0 || st.Size() == a.Size)
}

// ---------- the job: a download here, or a flash / read test in the helper ----------

type sdState struct {
	Running bool    `json:"running"`
	Job     string  `json:"job"`   // download | flash | readtest
	Phase   string  `json:"phase"` // download, prepare, write, verify, read, done, failed, ready
	Done    int64   `json:"done"`
	Total   int64   `json:"total"`
	Message string  `json:"message"`
	Error   string  `json:"error"`
	Image   string  `json:"image"`
	MBps    float64 `json:"mbps,omitempty"`
	Bad     []int64 `json:"bad,omitempty"` // read test: MB offsets that couldn't be read
	At      int64   `json:"at"`
}

var (
	sdMu      sync.Mutex
	sd        sdState
	sdStatus  string // the helper: where it writes sd; the window: where it reads it from
	sdHelping bool   // a helper is running (started from this window)
)

func sdSet(f func(*sdState)) {
	sdMu.Lock()
	f(&sd)
	sd.At = time.Now().Unix()
	s := sd
	sdMu.Unlock()
	if sdHelperMode && sdStatus != "" {
		writeStatus(s)
	}
}

var lastWrite time.Time

func writeStatus(s sdState) {
	if s.Running && time.Since(lastWrite) < 400*time.Millisecond {
		return
	}
	lastWrite = time.Now()
	b, _ := json.Marshal(s)
	tmp := sdStatus + ".tmp"
	if os.WriteFile(tmp, b, 0o644) == nil {
		_ = os.Rename(tmp, sdStatus)
	}
}

func sdStatusPath() string { return filepath.Join(configDir(), "sdjob.json") }

// the state as the window sees it: the helper's file while it runs (or after)
func sdNow() sdState {
	sdMu.Lock()
	s := sd
	helping := sdHelping
	sdMu.Unlock()
	if s.Job == "download" && s.Running {
		return s
	}
	if b, err := os.ReadFile(sdStatusPath()); err == nil {
		var f sdState
		if json.Unmarshal(b, &f) == nil && f.At >= s.At {
			if f.Running && time.Now().Unix()-f.At > 30 { // the helper went away mid-job
				f.Running, f.Phase, f.Error = false, "failed", "The helper stopped (closed, or Windows' prompt was declined)"
			}
			if !f.Running {
				sdMu.Lock()
				sdHelping = false
				sdMu.Unlock()
			}
			return f
		}
	}
	if helping && time.Now().Unix()-s.At > 60 {
		s.Running, s.Phase, s.Error = false, "failed", "Windows' prompt was declined, so nothing was done"
		sdMu.Lock()
		sdHelping, sd = false, s
		sdMu.Unlock()
	}
	return s
}

type progressWriter struct{}

func (progressWriter) Write(b []byte) (int, error) {
	sdSet(func(s *sdState) { s.Done += int64(len(b)) })
	return len(b), nil
}

type stallReader struct {
	r  io.Reader
	cb func()
}

func (s *stallReader) Read(b []byte) (int, error) {
	n, err := s.r.Read(b)
	if n > 0 {
		s.cb()
	}
	return n, err
}

func downloadTo(url, dst string, size int64) error {
	_ = os.MkdirAll(filepath.Dir(dst), 0o755)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
	if err != nil {
		return err
	}
	req.Header.Set("User-Agent", "Phosphor/"+appVersion)
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return fmt.Errorf("HTTP %d", resp.StatusCode)
	}
	stall := time.AfterFunc(time.Minute, cancel) // nothing for a minute: give up
	defer stall.Stop()
	if resp.ContentLength > 0 && size == 0 {
		size = resp.ContentLength
		sdSet(func(s *sdState) { s.Total = size })
	}
	tmp := dst + ".part"
	f, err := os.Create(tmp)
	if err != nil {
		return err
	}
	n, err := io.Copy(io.MultiWriter(f, progressWriter{}), &stallReader{r: resp.Body, cb: func() { stall.Reset(time.Minute) }})
	if e := f.Close(); err == nil {
		err = e
	}
	if err != nil {
		_ = os.Remove(tmp)
		if ctx.Err() != nil {
			return errors.New("the download stalled (nothing arrived for a minute): check the internet connection and try again")
		}
		return err
	}
	if size > 0 && n != size {
		_ = os.Remove(tmp)
		return fmt.Errorf("the download is incomplete (%d of %d MB)", n>>20, size>>20)
	}
	return os.Rename(tmp, dst)
}

// ---------- the window's side ----------

func sdRoutes(h func(string, http.HandlerFunc)) {
	h("/api/sd/images", func(w http.ResponseWriter, r *http.Request) {
		imgs, err := installerImages(r.URL.Query().Get("refresh") == "1")
		if err != nil {
			fail(w, err.Error())
			return
		}
		out := make([]flashImage, len(imgs))
		for i, a := range imgs {
			out[i] = a
			_, out[i].Have = haveImage(a)
		}
		writeJSON(w, map[string]any{"ok": true, "images": out})
	})
	h("/api/sd/disks", func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Query().Get("all") == "1" {
			rep, err := diskReport()
			if err != nil {
				fail(w, err.Error())
				return
			}
			writeJSON(w, map[string]any{"ok": true, "disks": rep})
			return
		}
		d, err := listRemovable()
		if err != nil {
			fail(w, err.Error())
			return
		}
		writeJSON(w, map[string]any{"ok": true, "disks": d})
	})
	h("/api/sd/state", func(w http.ResponseWriter, r *http.Request) {
		writeJSON(w, map[string]any{"ok": true, "state": sdNow()})
	})
	h("/api/sd/download", func(w http.ResponseWriter, r *http.Request) {
		var req struct{ Kind string }
		_ = readJSON(r, &req)
		if sdNow().Running {
			fail(w, "Something is already running: wait for it to finish")
			return
		}
		imgs, err := installerImages(false)
		if err != nil {
			fail(w, err.Error())
			return
		}
		var a *flashImage
		for i := range imgs {
			if imgs[i].Kind == req.Kind {
				a = &imgs[i]
			}
		}
		if a == nil {
			fail(w, "That image isn't in the latest release")
			return
		}
		if p, ok := haveImage(*a); ok {
			sdSet(func(s *sdState) {
				*s = sdState{Job: "download", Phase: "ready", Image: p, Message: "Already on this PC: " + a.Name}
			})
			writeJSON(w, map[string]any{"ok": true, "image": p})
			return
		}
		dst := filepath.Join(imageCacheDir(), a.Name)
		sdSet(func(s *sdState) {
			*s = sdState{Running: true, Job: "download", Phase: "download", Total: a.Size, Message: "Downloading " + a.Name + " (" + a.Release + ")"}
		})
		go func() {
			err := downloadTo(a.URL, dst, a.Size)
			sdSet(func(s *sdState) {
				s.Running = false
				if err != nil {
					s.Phase, s.Error = "failed", err.Error()
					return
				}
				s.Phase, s.Image, s.Message = "ready", dst, "Downloaded "+a.Name
			})
		}()
		writeJSON(w, map[string]any{"ok": true, "started": true})
	})
	h("/api/sd/pick", func(w http.ResponseWriter, r *http.Request) {
		p, err := pickImageFile()
		if err != nil {
			fail(w, err.Error())
			return
		}
		if p != "" {
			_, size, closer, err := imageSize(p)
			if err != nil {
				fail(w, "That file can't be opened as an SD card image: "+err.Error())
				return
			}
			closer()
			writeJSON(w, map[string]any{"ok": true, "image": p, "size": size})
			return
		}
		writeJSON(w, map[string]any{"ok": true, "image": ""})
	})
	// flash {disk, image, confirm}: confirm is the disk's number typed again
	// readtest {disk}
	h("/api/sd/start", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			Job     string
			Disk    int
			Image   string
			Confirm string
		}
		if err := readJSON(r, &req); err != nil {
			fail(w, "bad request")
			return
		}
		if sdNow().Running {
			fail(w, "Something is already running: wait for it to finish")
			return
		}
		d, err := cardDisk(req.Disk)
		if err != nil {
			fail(w, err.Error())
			return
		}
		// the number, and what the helper checks it's still the same disk by (numbers can move when cards come and go)
		args := []string{"--sdjob", req.Job, "--disk", fmt.Sprint(d.Number), "--size", fmt.Sprint(d.Size)}
		if d.UniqueId != "" {
			args = append(args, "--uid", d.UniqueId)
		}
		switch req.Job {
		case "flash":
			if req.Confirm != fmt.Sprint(req.Disk) {
				fail(w, "Type the disk number to confirm")
				return
			}
			_, size, closer, err := imageSize(req.Image)
			if err != nil {
				fail(w, "Can't open the image: "+err.Error())
				return
			}
			closer()
			if size > d.Size {
				fail(w, fmt.Sprintf("The image (%d MB) is bigger than the card (%d MB)", size>>20, d.Size>>20))
				return
			}
			args = append(args, "--image", req.Image)
		case "readtest":
		default:
			fail(w, "job: flash or readtest")
			return
		}
		_ = os.Remove(sdStatusPath())
		sdSet(func(s *sdState) {
			*s = sdState{Running: true, Job: req.Job, Phase: "prepare", Message: "Waiting for Windows' prompt (allow it to go on)"}
		})
		if err := startHelper(append(args, "--status", sdStatusPath())); err != nil {
			sdSet(func(s *sdState) { s.Running, s.Phase, s.Error = false, "failed", err.Error() })
			fail(w, err.Error())
			return
		}
		sdMu.Lock()
		sdHelping = true
		sdMu.Unlock()
		writeJSON(w, map[string]any{"ok": true})
	})
}

// ---------- the helper's side ----------

var sdHelperMode bool

// runSDJob: started with --sdjob: do that one job (as administrator) and exit
func runSDJob(args []string) bool {
	if len(args) == 0 || args[0] != "--sdjob" {
		return false
	}
	sdHelperMode = true
	opt := map[string]string{}
	for i := 0; i+1 < len(args); i += 2 {
		opt[strings.TrimPrefix(args[i], "--")] = args[i+1]
	}
	sdStatus = opt["status"]
	// (an administrator other than the user has another AppData: the window's file is passed in)
	if filepath.Base(sdStatus) != "sdjob.json" || filepath.Base(filepath.Dir(sdStatus)) != "Phosphor" {
		os.Exit(2)
	}
	var num int
	fmt.Sscan(opt["disk"], &num)
	job := opt["sdjob"]
	sdSet(func(s *sdState) {
		*s = sdState{Running: true, Job: job, Phase: "prepare", Message: "Checking the disk"}
	})
	var err error
	d, derr := cardDisk(num) // checked again here: never the PC's own disks
	if derr == nil && (fmt.Sprint(d.Size) != opt["size"] || d.UniqueId != opt["uid"]) {
		derr = fmt.Errorf("disk %d isn't the card that was chosen any more (a card was put in or taken out?): nothing was done; choose it again", num)
	}
	if derr != nil {
		err = derr
	} else if job == "flash" {
		err = flash(d, opt["image"])
	} else if job == "readtest" {
		err = readTest(d)
	} else {
		err = errors.New("unknown job")
	}
	sdSet(func(s *sdState) {
		s.Running = false
		if err != nil {
			s.Phase, s.Error = "failed", err.Error()
			return
		}
		s.Phase = "done"
		if job == "flash" {
			s.Message = "Done. Eject the card, put it in the SuperStation and let its installer finish."
		}
	})
	writeStatus(sd)
	os.Exit(0)
	return true
}

func cardDisk(num int) (disk, error) {
	disks, err := listRemovable()
	if err != nil {
		return disk{}, err
	}
	for _, d := range disks {
		if d.Number == num {
			return d, nil
		}
	}
	return disk{}, errors.New("That disk isn't an SD card or USB card reader, so it's left alone")
}

type disk struct {
	Number            int    `json:"Number"`
	FriendlyName      string `json:"FriendlyName"`
	Size              int64  `json:"Size"`
	BusType           string `json:"BusType"`
	LogicalSectorSize int    `json:"LogicalSectorSize"`
	Letters           string `json:"Letters"`
	UniqueId          string `json:"UniqueId"` // Windows' id for the disk (serial-based): the same disk after a renumbering
}

type diskInfo struct {
	Number  int    `json:"number"`
	Name    string `json:"name"`
	Size    int64  `json:"size"`
	BusType string `json:"bus"`
	Media   string `json:"media"`
	Letters string `json:"letters"`
	Boot    bool   `json:"boot"`
	Status  string `json:"status"`
	Health  string `json:"health"`
	Offered bool   `json:"offered"`
	Reason  string `json:"reason"`
}
