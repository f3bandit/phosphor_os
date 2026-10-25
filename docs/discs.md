# Discs: the dock's CD/DVD drive

Read this before changing anything about physical discs, in phosphor-main (`phosphor_disc.cpp`,
`support/physical_disc/`), phosphord (`disc.c`), the TV shell (`ui.c`, `mainlink.c`) or the apps.

Disc support is **Main_MiSTer_Physical_Disc** by **Anime0t4ku and bonna79** (GPL-3.0), merged into
phosphor-main. Phosphor adds the screens, the names and box art, and decides what a disc does.
Credit them wherever disc support is mentioned (Settings › Discs on the TV and in the apps, the README).

## What it plays

PlayStation, Saturn, Mega-CD / Sega CD, PC Engine CD / TurboGrafx-CD, Neo Geo CD, CD-i, 3DO, and
MSU-1 (SNES) / MD+ (Mega Drive) discs, read raw from the drive while the game runs. Music CDs and DVDs
are recognised but not played yet (Phosphor's own music player comes later, with Steam music too).

## How it fits together

```
drive (/dev/sr0)
  └─ phosphor-main  phosphor_disc.cpp: tray polled once a second (menu core only), a new disc read once
       │              on a thread (system, disc code, label, region); "disc" on /tmp/phosphor-main.sock
       │              (API 5), events {"event":"disc",...}; "disc eject", "disc on|off"
       ├─ phosphord  disc.c: names it (your library's game with the same disc code, else libretro's
       │              lists), box art (the library's, else libretro's, kept in data/disc_art),
       │              playable or why not; disc_play writes /tmp/phosphor/disc.mgl and loads it
       └─ TV shell   mainlink.c: the events; ui.c: the screens below
apps (Windows, Android): phosphord's remote commands disc, disc_play, disc_eject, disc_enable,
                          GET /art/disc (docs/remote-api.md)
```

A disc game starts the way upstream does it: an `.mgl` that loads the stock core under an `A0CD-*`
set name, which tells Main's disc code that the core is for the drive:

```xml
<mistergamedescription><rbf>_Console/PSX</rbf><setname same_dir="1">A0CD-PSX</setname></mistergamedescription>
```

Upstream's own launcher (Auto Disc Discovery, the `[A0CD-*] main=` switching) is **off** in Phosphor
(`cfg.physical_disc_launch = 0`; `PHYSICAL_DISC_MGL=1` in MiSTer.ini turns it back on). Nothing starts
by itself: the user picks the disc.

## What was decided (the mockups, `mockup-discs-v2.html`)

| | |
|---|---|
| Where the disc shows | **Library › Disc**: an "In the drive · <game>" line at the top, only while a disc is in. A Play, Y Eject. |
| Disc button on the home screen | none |
| Status bar | a disc icon while one is in |
| Play last (X) | the last game, unless that was the disc and it's no longer in: then the last game from storage |
| Disc put in at power-on | just shown, not started |
| Music CDs | left out for now |
| Settings › Discs | Discs on/off (off: the drive isn't watched or read at all), the drive, Eject |
| In-game menu | Phosphor's own (B), see below |

## The in-game menu

A **short press** of the menu button in a game opens Phosphor's menu over the running game; a **long
press** (a count of 3 Mississippi, then let go; anything over 0.7 s counts) opens MiSTer's OSD as
before. Not much longer: 8BitDo controllers turn themselves off when the home button is held too long.

- Main (API 5) shows Linux's screen over the core: `ingame on` (`video_chvt(1)`, `video_fb_enable(1)`);
  the game keeps running and playing sound; the screen behind the menu is black.
- The shell then draws the menu once (drawn before Main switches, the switch wipes it; drawn twice, it
  jitters).
- Left: the game's box art, title, year · developer · players · genre, description. Right: Back to the
  game, Change disc (disc games), MiSTer menu, Quit to Phosphor.
- The controller drives the menu only (`phosphor_pad` while `ingame_client == owner`), not the game.

## Not done yet

- Real game disc tests (only a music CD has been in the drive so far).
- Multi-disc games: Change disc ejects; the core's disc code mounts the next disc when the tray closes.
- A ripper (disc to an image in the library).
- Phosphor's own music player.
