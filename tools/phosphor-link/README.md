# Phosphor Link

A PC program for reaching a MiSTer through the **SuperStation One dock's USB-C debug port**, or
over SSH. People can use it from a command prompt, and the **Claude desktop app** can use it
directly (as an MCP server) to run tests on the MiSTer, copy files and read the boot log.

## What the debug port is

The dock's debug port is a USB-to-serial bridge to the DE10-Nano's console UART (115200 8N1).
Everything the MiSTer prints to its console comes out there: U-Boot, the Linux kernel, MiSTer
Main's own messages, and a root login prompt. It works with no network, while the MiSTer boots,
and when the network is broken, which is why Phosphor Link prefers it.

Phosphor Link is careful with it:

- **It never types while the MiSTer is booting.** A key press in U-Boot's first seconds stops the
  boot. It waits until the console is quiet, or for the login prompt. If it finds U-Boot already
  stopped at its `=>` prompt, it sends `boot` so the MiSTer carries on.
- **Main's messages can't damage results.** Main prints to the same console at any moment, even
  in the middle of a line. Command output is saved to a file on the MiSTer and fetched as
  numbered, length-checked lines; damaged lines are fetched again, and the whole file is checked
  with md5. Files are sent the same way, in checked pieces.
- **It leaves the console as it found it.** Kernel messages are muted while connected and put
  back afterwards, and it logs out when done.

## Install

1. Copy `phosphor-link.exe` to a folder of its own, e.g. `C:\phosphor-link\`.
2. Connect the dock's USB-C debug port to the PC. Windows installs the USB serial driver itself.
3. Open a command prompt in that folder and check it: `phosphor-link status`
4. Optional: copy `phosphor-link.ini.example` to `phosphor-link.ini` to set the COM port, the
   login or an SSH address.

## Using it with the Claude desktop app

```
phosphor-link install-claude
```

This adds Phosphor Link to the Claude desktop app's settings (`claude_desktop_config.json`,
backed up first). Quit Claude completely (from the tray too) and open it again. Claude can then:

| Tool | What it does |
|---|---|
| `status` | Connects and reports MiSTer Linux version, kernel, uptime, running core, free space |
| `list_ports` | Serial ports on the PC |
| `run` | Runs a command or script on the MiSTer as root; returns output and exit code |
| `push_file` / `push_text` | Copies a file, or writes text, to the MiSTer |
| `pull_file` / `read_file` | Copies a file from the MiSTer, or returns a text file |
| `serial_log` | Listens to the console (Main and kernel messages) without typing |
| `recent_output` | The last part of everything the console printed |
| `reboot` | Reboots and returns the whole boot log, then logs in again |
| `disconnect` | Closes the connection |

`phosphor-link uninstall-claude` removes it again (also with a backup).

## Commands

```
phosphor-link ports                      list serial ports
phosphor-link status                     connect and show the MiSTer's state
phosphor-link run "<command>"            run a command (its exit code is passed back)
phosphor-link push <local> <remote> [mode]
phosphor-link pull <remote> <local>
phosphor-link watch [seconds]            show what the console prints, without typing
phosphor-link reboot                     reboot and show the whole boot log
phosphor-link term                       interactive console (Ctrl-] quits)
phosphor-link selftest                   check the link end to end
```

Everything the console prints is also kept in `phosphor-link.log` next to the program.

## Speed

The port runs at 115200 baud, about 11 KB/s raw. Files travel as base64, so expect roughly
8 KB/s: a script is instant, a 1 MB file takes about two minutes. Use SSH for big files.

## Building

`./build.sh` (Go 1.24+) builds `dist/phosphor-link.exe` and `dist/phosphor-link-linux`. No cgo.

`test/fake_mister.py` is a pretend MiSTer console on a pseudo-terminal: boot log, login, a real
bash, Main messages dropped in at random (including mid-line), U-Boot stops and reboots. It
reports any key sent during boot. `test/mcptest.py` drives the MCP server through every tool.

## Status

0.1.2. Runs on a real SuperStation One (selftest passes over the dock port, about 6.5 KB/s). Also tested against
the pretend console with heavy mid-line noise, a stopped U-Boot and reboots.
