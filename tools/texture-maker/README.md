# Phosphor Texture Maker

A developer tool for making Phosphor's **sprite background textures** from your own pixel-art
sprites. Not part of Phosphor and not for users: what it makes is added to Phosphor's source and
ships as a built-in texture.

`PhosphorTextureMaker.exe` opens in its own window (Edge app mode, or the default browser). It
closes by itself a few minutes after its window is closed.

## Making a texture
1. **Sprites:** drop PNGs on the window or press **+** (up to 128×128 each). ◐ leaves one out
   for now, ✕ removes it.
2. **Colours to shades:** every texture is drawn in the accent colour, so a sprite's colours become
   shades of it (16 levels: 0 = empty, 15 = full).
   - **Background:** *Auto* uses the PNG's transparency, or, for a picture with none, its corner
     colour. *Transparency* uses only transparency, *None* keeps every pixel.
   - **Shade by:** *Each colour* gives every colour its own shade, darkest to brightest, so an
     outline and a fill of the same brightness still differ. *Brightness* follows how light each
     colour looks (CIE L\*).
   - **Shades** (how many steps), **Darkest shade** (so dark colours stay visible), and
     **Brightest is** *Strongest* or *Faintest*.
3. **Layout:**
   - The whole screen (1280×720 base pixels, no repeat) or a tile that repeats. **Auto** tile size
     makes the tile big enough for every sprite to land about twice at the density picked, so the
     repeat is a scatter, not a grid of one sprite.
   - Pixel scale (base pixels per sprite pixel), density and minimum gap.
   - Angle: random 90° turns, or **Set** any angle (0–359.5°, typed) for every sprite; flip left to right (as it shows on screen) at random, every other sprite, or off.
   - Grid snap and a seed with **Re-roll**.
4. **Look:** *Sprites* (accent on the sprites) or *Inverted* (accent everywhere else), and
   strength. Phosphor's own textures use 0.13.
5. The preview shows the real strength (or a brighter view to check the layout) in any accent,
   with all five accents underneath.

**Save project** / **Open project** keep the sprites and every setting in one `.ptm` file. The last
project opens again at the next start.

## Export and Add to Phosphor
**Export** writes `<exports>/<id>/`:

| File | What it is |
|---|---|
| `tex_<id>.h` | The texture for the TV (`frontend/src/`) |
| `tex_<id>.js` | The same for the Windows app (`apps/windows/web/`) |
| `HOW_TO_ADD.txt` | The table lines, for adding it by hand |
| `preview.png` | All five accents and the bright view |
| `texture.json` | Its name, size and counts |
| `<id>.ptm` | The project |

**Add to Phosphor** (Phosphor's source set in **Folders…**) does it all:
1. Copies the two data files.
2. Adds the texture's line to `texture.c` and `texture.js` with the next free number (the same in
   both), and to the order Settings shows them in.
3. Adds the `<script>` line to the Windows app's `index.html`.

Adding a texture with an id that's already there replaces its data and name and keeps its number, so a texture picked on a console stays the same one.

## How Phosphor draws them
`frontend/src/texsprite.c` (`sprite_alpha`) on the TV and `SpriteTex.alpha` in
`apps/windows/web/texture.js` and `web/spritetex.js` here are the same drawing, rounding included.
They give the same pixels at every scale (tested at 0.5, 0.75, 1 and 1.5, tiles and whole screens,
every turn and mirror, inverted). If one changes, change all three.

Texture format (`texsprite.h`):
- Sprites are shades 0..15, two pixels a byte (the left one in the high half), with each row
  padded to a byte.
- Placements are `x, y, sprite, orientation`: bits 0–1 are turns of 90° clockwise, and bit 2
  mirrors left to right first.
- A sprite pixel covers `px` base pixels.
- A free angle (`cq`, `sq`: cosine and sine × 65536, whole numbers) turns every sprite about its
  middle after its 90° turns; bit 3 of the orientation turns it the other way (a flipped sprite).
  The TV builds `texsprite.c` with `-ffp-contract=off` so its sums round like JavaScript's (checked
  on x86 and on ARM).

## Building
`./build.sh` writes `dist/PhosphorTextureMaker.exe` (and a Linux build). Go 1.22+, no other
dependencies.
