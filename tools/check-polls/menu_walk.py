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
250 ms or more while being walked (the opening of a screen after A is reported, not counted: that's a press),
when a screen keeps repainting with nobody touching it, or when the shell crashed.

Besides the stalls (docs/menu-rules.md has the rules these come from):
  idle      on every screen it opens, 3 s with no presses: what the shell repainted (<tmp>/draw). More than
            IDLE_MAX % of the screen a second fails (the controller test repainted the whole controller 10-16
            times a second with nobody touching it, Oct 31 2026)
  crash     the shell's pid before and after every step; a new one is a crash (its CRASH line is printed)
  quick     in and out faster than a person: Left / Right on Home then Start at once (the cards' slide outlived
            its cards: a crash, 0.27.65), and every screen opened and left again at once
  ctest     the controller test, when a controller is connected (Settings > Controllers, Y): idle and Select+Start
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
IDLE_MAX = 30          # % of the screen a second a screen may repaint with nobody touching it (a clock, a spinner)
CRASHLOG = "/media/fat/phosphor/logs/phosphor.log"
SHELL_PID = [None]


def shell_pid():
    return subprocess.run(["pidof", "phosphor"], capture_output=True, text=True).stdout.strip()


def crashed(name):
    # a new shell pid (or none): the shell died in this step
    now = shell_pid()
    if now == SHELL_PID[0]:
        return 0
    last = ""
    try:
        last = [l for l in open(CRASHLOG, errors="replace") if "CRASH" in l][-1].strip()
    except (OSError, IndexError):
        pass
    print(f"{name}: CRASH (the shell was {SHELL_PID[0]}, now {now or 'not running'}) {last}")
    SHELL_PID[0] = now
    time.sleep(6)                            # (started again by phosphor.sh, or not)
    SHELL_PID[0] = shell_pid()
    return 1


def drawn():
    try:
        return int(open("/tmp/phosphor/draw").read().split()[1])
    except (OSError, ValueError, IndexError):
        return None


def idle(name, secs=3.0, settle=1.2):
    # nobody touching it: a screen should repaint (almost) nothing
    time.sleep(settle)                       # (what the press itself started: an opening, a fade)
    a = drawn()
    time.sleep(secs)
    time.sleep(1.1)                          # (<tmp>/draw is written once a second)
    b = drawn()
    if a is None or b is None:
        print(f"{name}: idle: no <tmp>/draw (a shell older than 0.27.66)")
        return 0
    rate = (b - a) / (secs + 1.1)
    if rate > IDLE_MAX:
        print(f"{name}: idle: repaints {rate:.0f}% of the screen a second with nobody touching it (max {IDLE_MAX})")
        return 1
    return 0


def keys(seq, gap=GAP):
    for k in seq.split():
        with open(FIFO, "w") as f:
            f.write(k + "\n")
        time.sleep(gap)


def screen():
    # the shell's screen (screen_t), Settings row (SET_ id) and that row's title, from <tmp>/screen
    try:
        parts = open("/tmp/phosphor/screen").read().strip().split(" ", 2)
        return int(parts[0]), int(parts[1]), (parts[2] if len(parts) > 2 else "")
    except (OSError, ValueError, IndexError):
        return -1, -1, ""


def go_home():
    for _ in range(10):
        if screen()[0] == 0:
            return True
        keys("b", 0.5)
    return screen()[0] == 0


def to_row(title):
    # into Settings, then down to the row with that title, however the list is (rows added, wallpaper rows):
    # by title, never by number (a number off by one pressed A on toggles, Cores and nearly Shut down, Nov 4 2026)
    if not go_home():
        return False
    keys("start", 0.8)
    for _ in range(45):
        c, r, t = screen()
        if c != 3:
            return False
        if t == title:
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
    return len(bad) + crashed(name)


def quick_home():
    # Left / Right then Start at once, both ways: the cards' slide (170 ms) must not outlive the cards
    n = 0
    for i in range(12):
        if not go_home():
            break
        keys("right" if i % 2 == 0 else "left", 0.03)
        keys("start", 0.8)
        n += crashed(f"quick: Home, {'Right' if i % 2 == 0 else 'Left'} then Start")
        if n:
            break
    print(f"quick: Home's cards: {'OK' if not n else 'CRASH'}")
    return n


def controllers():
    try:
        out = subprocess.run(["/media/fat/phosphor/bin/phos", '{"cmd":"controllers"}'], capture_output=True, text=True, timeout=10).stdout
        import json
        return len(json.loads(out).get("controllers") or [])
    except Exception:
        return 0


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
    SHELL_PID[0] = shell_pid()
    n = 0
    n += phase("Settings list, down and up", "start " + "down " * 35 + "up " * 35)
    n += idle("Settings")
    go_home()
    if "--quick" not in sys.argv:
        # the Settings rows that open a screen of their own, by their title on screen: never one that does
        # something on A (Look for games, Get box art, Cores, Update, MiSTer menu, Restart, Shut down, toggles)
        safe = [("WIFI", "WiFi"), ("BT", "Bluetooth"), ("CTRL", "Controllers"), ("RETROSPY", "RetroSpy"),
                ("REMOTE", "Remote access"), ("CLOUD", "Cloud saves"), ("BACKUP", "Backups"),
                ("PORTS", "Front ports & memory cards"), ("STORAGE", "Storage"), ("DISCS", "Discs"),
                ("RA", "RetroAchievements"), ("VIDEO", "Video"), ("SOURCES", "Box art sources"),
                ("UPDATE_ALL", "Update All"), ("PHOSUPD", "Update Phosphor"), ("GUARD", "Updates & restore points"),
                ("SCRIPTS", "Scripts"), ("UPDLOG", "Last update's log"), ("DIAG", "Diagnostics"),
                ("ABOUT", "About this system")]
        # screens that do something when they open: their idle check after it's done (Diagnostics runs its checks
        # on the first visit)
        settle = {"DIAG": 10.0}
        # (an older shell gives no titles: nothing matches, so nothing is opened)
        for nm, title in safe:
            if not to_row(title):
                print(f"{nm}: '{title}' not found in Settings (skipped)")
                continue
            if screen()[2] != title:          # (checked again right before A)
                print(f"{nm}: not on '{title}' (skipped)")
                continue
            n += phase(f"{nm}: open", "a", 0.35, opening=True)
            if screen()[0] == 3:
                print(f"{nm}: didn't open a screen (skipped)")
                continue
            n += phase(f"{nm}: on the screen", "down down up up", GAP)
            n += idle(nm, settle=settle.get(nm, 1.2))
            if nm == "CTRL" and controllers():
                n += phase("CTRL: the controller test (Y)", "y", 0.35, opening=True)
                if screen()[0] == 25:
                    n += idle("the controller test")
                    keys("power", 1.0)       # (Select + Start closes it)
                else:
                    print("the controller test didn't open (skipped)")
            go_home()
            # out again at once, before it has finished opening
            if to_row(title) and screen()[2] == title:
                keys("a", 0.05)
                keys("b", 0.8)
                n += crashed(f"quick: {nm} opened and left at once")
            go_home()
        go_home()
        n += phase("Home cards", "right right right right left left left left")
        n += idle("Home")
        n += quick_home()
        go_home()
        n += phase("A library list", "a down down down down down down up up up up up up", GAP)
        go_home()
    print(f"menu_walk: {n} stall(s)")
    return 1 if n else 0


if __name__ == "__main__":
    sys.exit(main())
