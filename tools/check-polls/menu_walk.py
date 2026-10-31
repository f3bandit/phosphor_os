#!/usr/bin/env python3
# Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
"""Menu lag test, run on the SS1 (python3 there): walks Phosphor's menus with the shell's own remote keys
(/tmp/phosphor-keys) and reports every stall the shell logged meanwhile (frontend.log "slow:" and "poll:").
The source check (check_polls.py) finds synchronous polls; this finds what the menu actually does on the
SuperStation: a request that took seconds, a screen that repaints all the time, a list that stutters.

  python3 menu_walk.py [--cold] [--quick]
    --cold   restart phosphord first, so its caches are empty (the worst case: just after start-up)
    --quick  the Settings list only

It starts from Home (B a few times), so leave a game first. Exit status 1 when the menu stood still for
250 ms or more while being walked (the opening of a screen after A is reported, not counted: that's a press).
"""
import os
import re
import subprocess
import sys
import time
import functools
print = functools.partial(print, flush=True)   # (the report as it goes, also into a file)

FIFO = "/tmp/phosphor-keys"
LOG = "/tmp/phosphor/frontend.log"
GAP = 0.6
SCREENS = {0: "Home", 1: "Systems", 2: "Games", 3: "Settings"}


def keys(seq, gap=GAP):
    for k in seq.split():
        with open(FIFO, "w") as f:
            f.write(k + "\n")
        time.sleep(gap)


def screen():
    # the shell's screen (screen_t) and Settings row (SET_ id), from <tmp>/screen
    try:
        a, b = open("/tmp/phosphor/screen").read().split()
        return int(a), int(b)
    except (OSError, ValueError):
        return -1, -1


def go_home():
    for _ in range(10):
        if screen()[0] == 0:
            return True
        keys("b", 0.5)
    return screen()[0] == 0


def to_row(sid):
    # into Settings, then down to that row (set_id), however the list is (wallpaper rows, a moved list)
    if not go_home():
        return False
    keys("start", 0.8)
    for _ in range(45):
        c, r = screen()
        if c != 3:
            return False
        if r == sid:
            return True
        keys("down", 0.3)
    return False


def log_size():
    try:
        return os.path.getsize(LOG)
    except OSError:
        return 0


def stalls_since(pos):
    with open(LOG, "rb") as f:
        f.seek(pos)
        text = f.read().decode("utf-8", "replace")
    return [l for l in text.splitlines() if " slow:" in l or " poll:" in l]


def phase(name, seq, gap=GAP, opening=False):
    pos = log_size()
    keys(seq, gap)
    time.sleep(1.5)
    found = stalls_since(pos)
    bad = []
    for l in found:
        m = re.search(r"slow: (\d+) ms \(keys (\d+), timers (\d+), drawing (\d+)", l)
        if m:
            total, k, t, d = map(int, m.groups())
            if opening:
                continue                     # (opening a screen after A, its first paint included: the press)
            if total >= 250:
                bad.append(l)
        elif " poll:" in l or ("phosphord took" in l and not opening):
            bad.append(l)                    # (a request the opening of a screen waited for: the press)
    print(f"{name}: {'OK' if not bad else str(len(bad)) + ' stall(s)'}")
    for l in bad:
        print("   " + l.split("phosphor: ", 1)[-1])
    return len(bad)


def main():
    if "--cold" in sys.argv:
        pid = subprocess.run(["pidof", "phosphord"], capture_output=True, text=True).stdout.split()
        for p in pid:
            os.kill(int(p), 15)
        time.sleep(1)
        subprocess.Popen(["/media/fat/phosphor/phosphor.sh", "service"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(3)
        print("phosphord restarted (its caches empty)")
    if not os.path.exists(FIFO):
        print("the shell isn't running (no " + FIFO + ")")
        return 2
    if not go_home():
        print("couldn't get back to Home (leave a game first)")
        return 2
    n = 0
    n += phase("Settings list, down and up", "start " + "down " * 35 + "up " * 35)
    go_home()
    if "--quick" not in sys.argv:
        # the Settings rows that open a screen of their own, never one that does something on A (Look for
        # games, Update, MiSTer menu, Restart, Shut down, toggles): their order in ui.c's SET_ list, with
        # Wallpaper's picker row only there while wallpapers are on
        names = ["WIFI", "BT", "CTRL", "RETROSPY", "REMOTE", "SAMBA", "CLOUD", "BACKUP", "PORTS", "STORAGE", "DISCS", "RA",
                 "SHOTS", "VIDEO", "ACCENT", "TEXTURE", "WALLPAPER", "WALL_PICK", "CLOCK", "SAVER", "RESCAN", "SCRAPE",
                 "SCRAPE_INFO", "SOURCES", "CORES", "UPDATE_ALL", "PHOSUPD", "GUARD", "SCRIPTS", "UPDLOG", "DIAG",
                 "MISTER", "RESTART", "SHUTDOWN", "ABOUT"]
        safe = {"WIFI", "BT", "CTRL", "RETROSPY", "REMOTE", "CLOUD", "BACKUP", "PORTS", "STORAGE", "DISCS", "RA", "VIDEO",
                "SCRAPE", "SOURCES", "UPDATE_ALL", "PHOSUPD", "GUARD", "SCRIPTS", "UPDLOG", "DIAG", "ABOUT"}
        for sid, nm in enumerate(names):          # (sid: the SET_ id, as <tmp>/screen gives it)
            if nm not in safe:
                continue
            if not to_row(sid):
                print(f"{nm}: not found in Settings (skipped)")
                continue
            n += phase(f"{nm}: open", "a", 0.35, opening=True)
            if screen()[0] == 3:
                print(f"{nm}: didn't open a screen (skipped)")
                continue
            n += phase(f"{nm}: on the screen", "down down up up", GAP)
            go_home()
        go_home()
        n += phase("Home cards", "right right right right left left left left")
        n += phase("A library list", "a down down down down down down up up up up up up", GAP)
        go_home()
    print(f"menu_walk: {n} stall(s)")
    return 1 if n else 0


if __name__ == "__main__":
    sys.exit(main())
