# Third-party code in phosphord

Each keeps its own licence (the licence file next to it, or at the end of the file); the full
notices are in THIRD_PARTY_NOTICES.md at the top of the repository, which ships with every release.

| Component | Version | From | Licence |
|---|---|---|---|
| cJSON | 1.7.19 | https://github.com/DaveGamble/cJSON | MIT (`cjson/LICENSE`) |
| SQLite | 3.50.4 (amalgamation) | https://sqlite.org | Public domain (`sqlite/README`) |
| rcheevos | 12.5.0 | https://github.com/RetroAchievements/rcheevos | MIT (`rcheevos/LICENSE`) |
| libchdr | see `libchdr/VERSION` | https://github.com/rtissera/libchdr | BSD 3-Clause (`libchdr/LICENSE.txt`) |
| LZMA SDK (in libchdr) | 26.02 | https://www.7-zip.org/sdk.html | Public domain (`libchdr/deps/lzma-26.02/LICENSE`) |
| miniz (in libchdr) | 3.1.2 | https://github.com/richgel999/miniz | MIT (top of `miniz.c`) |
| Zstandard (in libchdr) | 1.5.7 | https://github.com/facebook/zstd | BSD (`libchdr/deps/zstd-1.5.7/LICENSE`) |
| stb_image, stb_image_write | see `stb/VERSION` | https://github.com/nothings/stb | MIT or public domain (end of each file) |
| stb_vorbis | 1.22 | https://github.com/nothings/stb | MIT or public domain (end of the file) |
| dr_flac, dr_mp3, dr_wav | 0.13.4, 0.7.4, 0.14.6 | https://github.com/mackron/dr_libs | Public domain or MIT-0 (end of each file) |

The TV interface's third-party code (LVGL, with LodePNG) is in `frontend/third_party/`.
