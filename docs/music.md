# Music: the SS1's music player

Read this before changing the music player, in phosphord (`music.c`, `flacenc.c`, `remote.c`), the TV
shell (`ui.c`: Music, Now playing) or the apps (Phosphor for Windows `steam.go` and the Music screen,
Phosphor for Android More › Music).

## What it does

- Plays albums from a `Music` (or `music`) folder on the SD card or a USB drive, a music CD in the
  dock's drive, and soundtracks sent from Steam on a PC.
- Plays on the TV only while no game is running; starting a game stops it.
- Its own tile on the home screen and a Now playing card while it plays.
- Controlled from the TV, the TV remote, Phosphor for Android and Phosphor for Windows.
- Copies a music CD to FLAC (optional: Music › the CD › X on the TV).

## How it fits together

```
files / CD ─ phosphord music.c: the library (album.json, covers), a player thread decoding to 16-bit
             │               stereo (dr_flac, dr_mp3, dr_wav, stb_vorbis; the CD with CDROMREADAUDIO)
             │               → aplay → ALSA default (48 kHz plug) → /dev/MrAudio → the menu core
             ├─ CD names     MusicBrainz disc ID → names and year; Cover Art Archive → the cover
             │               (kept in data/music_cd; a retry 30 s later when the network wasn't up yet)
             ├─ CD copy      a thread: each sector read twice until they agree, FLAC (flacenc.c),
             │               "NN - Title.flac", album.json and cover.jpg into <drive>/Music/<Artist>/<Album>
             └─ remote       music, music_albums, music_album, music_play, music_ctl, music_queue,
                             music_rip; PUT /music/<album>/<file>; GET /music/cover/<id>
TV shell: Music (albums, the CD), Now playing; stops when a core other than the menu is loaded
apps: the same commands; covers through the app (Windows /api/mcover/<id>, Android art/mcover/<id>)
```

## Steam soundtracks

Phosphor for Windows reads Steam's own records on the PC: the Steam folder from the registry
(`HKCU\Software\Valve\Steam\SteamPath`), its libraries (`steamapps/libraryfolders.vdf`) and each
installed app (`appmanifest_*.acf`). Soundtracks are in `steamapps/music/<installdir>`. Phosphor
doesn't sign in to Steam or change anything there. Ticked soundtracks are sent one file at a time with
`PUT /music/<album>/<file>` into `<drive>/Music/Steam/<album>` (FLAC when Steam has it, else MP3).
A soundtrack you own but haven't downloaded shows up after it's installed in Steam.

## Credits

- MusicBrainz (CD names) and the Cover Art Archive (covers): <https://musicbrainz.org>,
  <https://coverartarchive.org>.
- dr_flac, dr_mp3, dr_wav by David Reid (public domain or MIT-0); stb_vorbis by Sean Barrett
  (public domain or MIT): `system/third_party/audio`.

## Not done yet

- Signing in through Steam for soundtracks owned but not downloaded.
- Copying a CD on real hardware end to end, and the Steam import end to end.
