// Phosphor Link as an MCP server, so the Claude desktop app can use the MiSTer directly.
// Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
//
// MCP over stdio: JSON-RPC 2.0, one message per line. stdout carries only protocol messages;
// everything else goes to stderr.
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"
	"unicode/utf8"
)

const maxText = 200_000

type rpcReq struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id,omitempty"`
	Method  string          `json:"method"`
	Params  json.RawMessage `json:"params,omitempty"`
}

type rpcErr struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
}

type rpcResp struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id"`
	Result  any             `json:"result,omitempty"`
	Error   *rpcErr         `json:"error,omitempty"`
}

type tool struct {
	Name        string         `json:"name"`
	Description string         `json:"description"`
	InputSchema map[string]any `json:"inputSchema"`
}

func obj(props map[string]any, req ...string) map[string]any {
	m := map[string]any{"type": "object", "properties": props}
	if len(req) > 0 {
		m["required"] = req
	}
	return m
}
func str(d string) map[string]any { return map[string]any{"type": "string", "description": d} }
func num(d string) map[string]any { return map[string]any{"type": "number", "description": d} }

var tools = []tool{
	{"status", "Connect to the MiSTer (SuperStation One dock debug port, or SSH) and report how it's reached, MiSTer Linux version, kernel, uptime, the running core and free space.", obj(map[string]any{})},
	{"list_ports", "List the serial ports on this PC (the dock's USB-C debug port shows up as a USB serial port).", obj(map[string]any{})},
	{"run", "Run a bash command or script on the MiSTer as root and return its output and exit code. Multi-line scripts are fine. Output over 200 KB is cut.", obj(map[string]any{
		"command":   str("Shell command or script"),
		"timeout_s": num("Seconds to wait (default 120)"),
	}, "command")},
	{"push_file", "Copy a file from this PC to the MiSTer (checked with md5).", obj(map[string]any{
		"local_path":  str("Path on this PC"),
		"remote_path": str("Full path on the MiSTer"),
		"mode":        str("chmod mode, default 644 (755 for scripts)"),
	}, "local_path", "remote_path")},
	{"push_text", "Write text to a file on the MiSTer (scripts, settings), creating folders as needed.", obj(map[string]any{
		"text":        str("File contents"),
		"remote_path": str("Full path on the MiSTer"),
		"mode":        str("chmod mode, default 644"),
	}, "text", "remote_path")},
	{"pull_file", "Copy a file from the MiSTer to this PC (checked with md5).", obj(map[string]any{
		"remote_path": str("Full path on the MiSTer"),
		"local_path":  str("Where to save it on this PC"),
	}, "remote_path", "local_path")},
	{"read_file", "Read a text file on the MiSTer and return it.", obj(map[string]any{
		"remote_path": str("Full path on the MiSTer"),
		"max_bytes":   num("Return at most this many bytes (default 200000)"),
	}, "remote_path")},
	{"serial_log", "Listen to the serial console without typing (MiSTer Main's messages, kernel messages) for some seconds, or until a piece of text appears. Serial only.", obj(map[string]any{
		"seconds": num("How long to listen (default 10, max 600)"),
		"until":   str("Stop early when this text appears"),
	})},
	{"recent_output", "Return the last part of everything the serial console printed since connecting, including Main's output. Serial only.", obj(map[string]any{
		"max_bytes": num("Default 20000"),
	})},
	{"reboot", "Reboot the MiSTer and return the full boot log (U-Boot, kernel, Main start), then log in again. Serial only. Nothing is typed until the login prompt, so the boot isn't interrupted.", obj(map[string]any{
		"timeout_s": num("Seconds to wait for the login prompt (default 180)"),
	})},
	{"press", "Press buttons in Phosphor's shell from here, as if on a controller: names in order, comma or space separated (up, down, left, right, a, b, x, y, l, r, select, start). Returns what the shell got (its input watch log for those presses). Needs the Phosphor shell running.", obj(map[string]any{
		"buttons": str("e.g. \"down, down, a\""),
		"gap_ms":  num("Pause between presses in ms (default 300)"),
	}, "buttons")},
	{"watch_input", "Watch the input the Phosphor shell receives for some seconds (MiSTer Main's controller events, raw controller and keyboard events, front ports, remote presses, and the keys the shell acted on), e.g. while someone presses buttons on the controller. seconds 0 returns the last lines instead.", obj(map[string]any{
		"seconds": num("How long to watch (default 10, max 50)"),
		"lines":   num("With seconds 0: how many of the last lines (default 60)"),
	})},
	{"disconnect", "Close the connection (kernel messages and the terminal are put back the way they were). The next call reconnects.", obj(map[string]any{})},
}

type mcpServer struct {
	sess *session
	out  *json.Encoder
	wmu  sync.Mutex
}

func runMCP(cfg Config) error {
	log.SetOutput(os.Stderr)
	log.SetPrefix("phosphor-link: ")
	srv := &mcpServer{sess: &session{cfg: cfg}, out: json.NewEncoder(os.Stdout)}
	defer srv.sess.close()
	rd := bufio.NewReaderSize(os.Stdin, 1<<20)
	var wg sync.WaitGroup
	for {
		line, err := rd.ReadBytes('\n')
		if len(strings.TrimSpace(string(line))) > 0 {
			var req rpcReq
			if jerr := json.Unmarshal(line, &req); jerr != nil {
				srv.send(rpcResp{JSONRPC: "2.0", ID: json.RawMessage("null"), Error: &rpcErr{-32700, "parse error"}})
			} else {
				wg.Add(1)
				go func() { defer wg.Done(); srv.handle(req) }()
			}
		}
		if err == io.EOF {
			wg.Wait()
			return nil
		}
		if err != nil {
			return err
		}
	}
}

func (m *mcpServer) send(r rpcResp) {
	m.wmu.Lock()
	defer m.wmu.Unlock()
	_ = m.out.Encode(r)
}

func (m *mcpServer) handle(req rpcReq) {
	if len(req.ID) == 0 {
		return // a notification (initialized, cancelled): nothing to answer
	}
	reply := func(res any) { m.send(rpcResp{JSONRPC: "2.0", ID: req.ID, Result: res}) }
	fail := func(code int, msg string) {
		m.send(rpcResp{JSONRPC: "2.0", ID: req.ID, Error: &rpcErr{code, msg}})
	}
	switch req.Method {
	case "initialize":
		var p struct {
			ProtocolVersion string `json:"protocolVersion"`
		}
		_ = json.Unmarshal(req.Params, &p)
		if p.ProtocolVersion == "" {
			p.ProtocolVersion = "2025-06-18"
		}
		reply(map[string]any{
			"protocolVersion": p.ProtocolVersion,
			"capabilities":    map[string]any{"tools": map[string]any{}},
			"serverInfo":      map[string]any{"name": "phosphor-link", "version": version},
			"instructions": "Tools for a MiSTer FPGA (DE10-Nano, SuperStation One) reached through the SuperStation One dock's " +
				"USB-C debug port (serial console) or SSH. Commands run as root. The SD card is /media/fat. " +
				"Phosphor OS lives in /media/fat/phosphor; its diagnostics: bash /media/fat/phosphor/modules/diag/diag.sh" +
				libraryInstructions,
		})
	case "ping":
		reply(map[string]any{})
	case "tools/list":
		reply(map[string]any{"tools": append(append([]tool{}, tools...), libraryTools...)})
	case "tools/call":
		var p struct {
			Name      string         `json:"name"`
			Arguments map[string]any `json:"arguments"`
		}
		if err := json.Unmarshal(req.Params, &p); err != nil {
			fail(-32602, "bad parameters")
			return
		}
		if strings.HasPrefix(p.Name, "library_") {
			if p.Arguments == nil {
				p.Arguments = map[string]any{}
			}
			blocks, isErr := libTool(p.Name, p.Arguments)
			reply(map[string]any{"content": blocks, "isError": isErr})
			return
		}
		text, isErr := m.call(p.Name, p.Arguments)
		reply(map[string]any{"content": []map[string]any{{"type": "text", "text": text}}, "isError": isErr})
	default:
		fail(-32601, "unknown method "+req.Method)
	}
}

func argS(a map[string]any, k, def string) string {
	if v, ok := a[k].(string); ok && v != "" {
		return v
	}
	return def
}

func argN(a map[string]any, k string, def float64) float64 {
	if v, ok := a[k].(float64); ok && v > 0 {
		return v
	}
	return def
}

func secs(f float64) time.Duration { return time.Duration(f * float64(time.Second)) }

func (m *mcpServer) call(name string, a map[string]any) (string, bool) {
	if a == nil {
		a = map[string]any{}
	}
	if name == "list_ports" {
		ps, err := listPorts()
		if err != nil {
			return err.Error(), true
		}
		return formatPorts(ps), false
	}
	if name == "disconnect" {
		m.sess.drop()
		return "disconnected", false
	}
	l, err := m.sess.get()
	if err != nil {
		return "Can't reach the MiSTer: " + err.Error(), true
	}
	text, isErr, broken := m.callOn(l, name, a)
	if broken {
		m.sess.drop()
	}
	return text, isErr
}

func (m *mcpServer) callOn(l Link, name string, a map[string]any) (text string, isErr, broken bool) {
	errOut := func(err error) (string, bool, bool) {
		// a timeout leaves the console usable; anything else (port gone, ssh dropped) reconnects next time
		b := !strings.Contains(err.Error(), "timed out")
		return err.Error(), true, b
	}
	switch name {
	case "status":
		out, _, err := l.Run(statusScript, 30*time.Second)
		if err != nil {
			return errOut(err)
		}
		return "connected by " + l.Name() + "\n" + out, false, false
	case "run":
		cmd := argS(a, "command", "")
		if cmd == "" {
			return "command is empty", true, false
		}
		out, rc, err := l.Run(cmd, secs(argN(a, "timeout_s", 120)))
		if err != nil {
			t, _, b := errOut(err)
			return clip(out, maxText) + "\n[" + t + "]", true, b
		}
		return fmt.Sprintf("%s\n[exit %d]", clip(out, maxText), rc), rc != 0, false
	case "push_file":
		lp := argS(a, "local_path", "")
		data, err := os.ReadFile(lp)
		if err != nil {
			return err.Error(), true, false
		}
		rp := argS(a, "remote_path", "")
		if err := l.Push(data, rp, argS(a, "mode", "644")); err != nil {
			return errOut(err)
		}
		return fmt.Sprintf("copied %s to %s (%d bytes, md5 checked)", lp, rp, len(data)), false, false
	case "push_text":
		rp := argS(a, "remote_path", "")
		t, _ := a["text"].(string)
		if err := l.Push([]byte(t), rp, argS(a, "mode", "644")); err != nil {
			return errOut(err)
		}
		return fmt.Sprintf("wrote %s (%d bytes)", rp, len(t)), false, false
	case "pull_file":
		rp, lp := argS(a, "remote_path", ""), argS(a, "local_path", "")
		data, err := l.Pull(rp)
		if err != nil {
			return errOut(err)
		}
		if dir := filepath.Dir(lp); dir != "" {
			_ = os.MkdirAll(dir, 0755)
		}
		if err := os.WriteFile(lp, data, 0644); err != nil {
			return err.Error(), true, false
		}
		return fmt.Sprintf("saved %s to %s (%d bytes, md5 checked)", rp, lp, len(data)), false, false
	case "read_file":
		data, err := l.Pull(argS(a, "remote_path", ""))
		if err != nil {
			return errOut(err)
		}
		mx := int(argN(a, "max_bytes", maxText))
		if !utf8.Valid(data) {
			return fmt.Sprintf("%d bytes of binary data; use pull_file to save it", len(data)), false, false
		}
		return clip(string(data), mx), false, false
	case "serial_log":
		li, ok := l.(Listener)
		if !ok {
			return errNotSerial.Error(), true, false
		}
		d := argN(a, "seconds", 10)
		if d > 600 {
			d = 600
		}
		out, err := li.Listen(secs(d), argS(a, "until", ""))
		if err != nil {
			return errOut(err)
		}
		if strings.TrimSpace(out) == "" {
			return "(nothing printed)", false, false
		}
		return clip(out, maxText), false, false
	case "recent_output":
		li, ok := l.(Listener)
		if !ok {
			return errNotSerial.Error(), true, false
		}
		return li.Recent(int(argN(a, "max_bytes", 20000))), false, false
	case "press":
		var names []string
		for _, f := range strings.FieldsFunc(strings.ToLower(argS(a, "buttons", "")), func(r rune) bool { return r == ',' || r == ' ' || r == '\n' || r == '\t' }) {
			if !validButton[f] {
				return "unknown button " + f + " (up, down, left, right, a, b, x, y, l, r, select, start)", true, false
			}
			names = append(names, f)
		}
		if len(names) == 0 || len(names) > 60 {
			return "give 1 to 60 buttons", true, false
		}
		gap := argN(a, "gap_ms", 300)
		if gap < 50 {
			gap = 50
		}
		if gap > 3000 {
			gap = 3000
		}
		script := fmt.Sprintf(`[ -p %[1]s ] || { echo "the Phosphor shell isn't running (no %[1]s)"; exit 1; }
m=$( (wc -l < %[2]s) 2>/dev/null || echo 0)
for k in %[3]s; do echo "$k" > %[1]s; sleep %.3[4]f; done
sleep 0.4; n=$( (wc -l < %[2]s) 2>/dev/null || echo 0); [ "$n" -lt "$m" ] && m=0
tail -n +$((m+1)) %[2]s`, keysFifo, watchLog, strings.Join(names, " "), gap/1000)
		out, rc, err := l.Run(script, secs(30+float64(len(names))*gap/1000))
		if err != nil {
			return errOut(err)
		}
		return clip(out, maxText), rc != 0, false
	case "watch_input":
		d := argN(a, "seconds", 10)
		if v, ok := a["seconds"].(float64); ok {
			d = v // 0 is meant: the last lines
		}
		if d > 50 {
			d = 50
		}
		var script string
		if d <= 0 {
			script = fmt.Sprintf("tail -n %d %s 2>/dev/null || echo '(no input watch log: is the Phosphor shell running?)'", int(argN(a, "lines", 60)), watchLog)
		} else {
			script = fmt.Sprintf(`[ -f %[1]s ] || echo '(no input watch log yet: is the Phosphor shell running?)'
m=$( (wc -l < %[1]s) 2>/dev/null || echo 0); sleep %[2]d
n=$( (wc -l < %[1]s) 2>/dev/null || echo 0); [ "$n" -lt "$m" ] && m=0
out=$(tail -n +$((m+1)) %[1]s 2>/dev/null); [ -n "$out" ] && echo "$out" || echo "(no input in %[2]d s)"`, watchLog, int(d))
		}
		out, _, err := l.Run(script, secs(d+20))
		if err != nil {
			return errOut(err)
		}
		return clip(out, maxText), false, false
	case "reboot":
		rb, ok := l.(Rebooter)
		if !ok {
			return errNotSerial.Error(), true, false
		}
		out, err := rb.Reboot(secs(argN(a, "timeout_s", 180)))
		if err != nil {
			return clip(out, maxText) + "\n[" + err.Error() + "]", true, true
		}
		return clip(out, maxText), false, false
	}
	return "unknown tool " + name, true, false
}

// the Phosphor shell's remote keys and input watch log (frontend/src/input.c)
const keysFifo = "/tmp/phosphor-keys"
const watchLog = "/tmp/phosphor-input.log"

var validButton = map[string]bool{"up": true, "down": true, "left": true, "right": true, "a": true, "b": true, "x": true,
	"y": true, "l": true, "r": true, "select": true, "start": true}

const statusScript = `echo "MiSTer Linux: $(cat /MiSTer.version 2>/dev/null || echo unknown)"
echo "kernel: $(uname -r)"
echo "uptime: $(uptime)"
echo "core: $(cat /tmp/CORENAME 2>/dev/null || echo none)"
m=""; for e in /proc/[0-9]*/exe; do t=$(readlink "$e" 2>/dev/null); case "$t" in /media/fat/MiSTer*|/media/fat/*/MiSTer*) m="$t"; break;; esac; done
echo "Main: ${m:-not running}"
df -h /media/fat 2>/dev/null | tail -n 1 | awk '{ print "SD card: " $4 " free of " $2 }'
ip -4 -o addr show 2>/dev/null | awk '$2 != "lo" { print "address: " $2 " " $4 }'
[ -d /media/fat/phosphor ] && echo "Phosphor: installed" || echo "Phosphor: not installed"
`

func formatPorts(ps []PortInfo) string {
	if len(ps) == 0 {
		return "no serial ports found"
	}
	var b strings.Builder
	for _, p := range ps {
		fmt.Fprintf(&b, "%s", p.Name)
		if p.USB {
			fmt.Fprintf(&b, "  USB %s:%s", p.VID, p.PID)
			if p.Product != "" {
				fmt.Fprintf(&b, "  %s", p.Product)
			}
			if p.Serial != "" {
				fmt.Fprintf(&b, "  serial %s", p.Serial)
			}
		}
		b.WriteString("\n")
	}
	return b.String()
}
