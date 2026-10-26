# RetroAchievements

Read this before changing anything about achievements, in phosphor-main (`achievements.cpp`,
`ra_http.cpp`), phosphord (`ra.c`), the TV shell (`ui.c`: Settings › RetroAchievements, the in-game
menu) or the apps.

RetroAchievements support is **odelot's** MiSTer RetroAchievements Main and cores
(<https://github.com/odelot/Main_MiSTer>, GPL-3.0), merged into phosphor-main, built on the
RetroAchievements team's **rcheevos** library (MIT). Achievements, badges and profiles come from
<https://retroachievements.org>. Credit them wherever achievements are mentioned.

## What it does

- Achievements while you play NES, SNES, Game Boy, GBA, N64, PlayStation, Mega Drive, Mega-CD,
  Master System, Game Gear, PC Engine / PC Engine CD and Atari 7800 games, through odelot's
  RetroAchievements-enabled cores.
- Settings › RetroAchievements on the TV: log in, hardcore, pop-up position, unlock sound, get or
  update the cores, match the library, your progress (the viewer).
- In-game menu: the game's achievements. Game pages: "Achievements N / M".
- Phosphor for Windows (Settings › RetroAchievements, a game's Achievements tab) and Android
  (More › RetroAchievements, a game's page): the same, except logging in, which is on the TV only.

## How it fits together

```
phosphor-main  achievements.cpp (odelot's): rcheevos rc_client in the game core's RAM mirror; reads
     │          /media/fat/retroachievements.cfg; API 6: "ra", "ra list", events {"event":"ra",...}
     │          ra_http.cpp: curl with the CA bundle (no -k), the POST body from a temp file
phosphord ra.c  logs in once (dorequest.php r=login2) and keeps only the token in the cfg; the cores
     │          from odelot's GitHub releases into _RA_Cores/Cores; matching (rc_hash + API_GetGameList)
     │          into the ra_games table; the Web API key (config/ra_webkey) for profiles and game lists
     │          play.c: a game whose system has an RA core starts on it (setname RA_<core>)
TV shell + apps: phosphord's ra, ra_set, ra_key, ra_cores, ra_match, ra_profile, ra_game
```

## Privacy

- The password is used once to log in and never stored: only the token is kept.
- `ra_login` and `ra_logout` are not allowed over remote access; the apps can't log in or out.
- The Web API key is typed by the user (on the TV or in Phosphor for Windows) and kept on the SS1.

## Not done yet

- An unlock in a game, tested on hardware; the viewer after a full library match.
