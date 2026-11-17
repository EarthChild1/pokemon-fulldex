# Pokémon Mystery Dungeon: Explorers of Sky — notes

Findings behind Explorers of Sky's support, with the command or log line each rests on. Unconfirmed
points are marked as such.

## The ROM

- Game code **`C2SE`** (North America), read from bytes 0x0C-0x0F of the owner's ROM (`[launcher] ...
  (game code C2SE)` in the app's log). Not `C2AE` as sometimes assumed.
- Symbols: [pmdsky-debug](https://github.com/UsernameFodder/pmdsky-debug) at commit `80f28e0`, version `NA`
  (its `symbols/` folder is MIT, © 2022 UsernameFodder).

## The headless harness

Built in WSL (Ubuntu 24.04, WSL1) into `build-native-pmd/` (git-excluded), with `ninja -C build-native-pmd ssrecomp`.
A run:

```
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build-native-pmd/ssrecomp <rom> --headless --no-save --jit \
  --funcs tools/pmd/C2SE.txt --state-load <state> --relative --frames N --input <script> \
  --shot-dir <dir> --shot-at a,b --shot-raw --shot-layers --compose 1600x720
```

Scripts: `<first> <last> KEY...` or `<first> <last> touch x y` (bottom screen coordinates). Reaching the
first dungeon from a new game: A every 16 frames; at the personality test **hold** the bow
(`0 900 touch 128 83`); at the naming screens `UP, A, START, A` every 64 frames (the Yes/No question
defaults to No). The first dungeon (Beach Cave) starts about 24800 frames after power-on.

Diagnostics added for this work (environment variables, off by default):

- `SSR_BGMAP_DUMP=1`: with each screenshot, `bgmap_<frame>_<unit><bg>.png` (every tiled layer's whole map
  as video memory holds it, transparent pixels magenta) and a log line with DISPCNT, BGCNT, scroll, the
  windows, BLDCNT and master brightness.
- `SSR_VRAMW=0xLO-0xHI`: logs the ARM9 pc, lr, a stack scan for return addresses, and (for a DMA) the
  source address of writes to that range of video memory or main RAM. Use with `--interp`.
- `SSR_PMD_WIDE=0`: leaves the game's drawing alone (pmdwide.h).
- `SSR_WIDE2D_DEBUG=1`: prints, for line 100, each layer wide2d.h considers.

## How the game draws (settled)

| Scene | Played on | World | Other screen |
|---|---|---|---|
| Dungeon | bottom (engine A) | BG3, 512x256 map, 16 colours, scrolled with the camera; BG2 512x256 the tile grid (only while shown); sprites (OBJ) | the player's choice: controls, team, log, map; BG0/BG1 256-colour text layers |
| Ground (towns, cutscenes) | bottom | BG3 + BG2, 512-wide maps; one tall scene over both screens | the upper half of the same scene; windows when a menu is up |
| Personality test | top (questions) | backdrops | the bow (touch) |

Evidence: `--shot-layers` (`layers_046040.png`: the floor yellow = BG3), `[bgmap] frame 46040 unit A dispcnt
C0211F18 bgcnt 0001 1B80 5C12 5E1B`. BG0 is the 3D layer; in dungeons BLDCNT is `3E41`, EVA = EVB = 8: the
3D layer blended at half over the others. The dark outside the lit room is translucent black 3D polygons
over the floor (not a window: both windows are off).

UI (text windows, the HUD line, the message log) is on BG0/BG1 with 256x256 maps and extended palettes;
world layers are 512 wide. wide2d.h uses that difference.

## The dungeon floor beyond the screen

The BG3 map holds only what the screen shows plus a few columns (dump at 46040: columns 72-368 filled,
scroll 104). The map is copied each change by DMA from `0x021B6EE0` (`SSR_VRAMW` on `0x0600F000`), and that
buffer is written by an unnamed helper at `0x0200B458`/`0x0200B470`, called through `0x02051D8C` from three
unnamed routines in overlay 29 (stack scan: `UpdateCamera+0x320` → `0x233711C` → `0x2051D8C`):

- `0x02336F4C` draws the whole view: 33 columns from the camera's x >> 3, 27 rows (many callers: a new
  floor, a jump of the camera). The pmdsky-debug name at this address range is `UpdateTrapsVisibility`,
  which looks like a misattribution; the code is the full redraw.
- `0x0233711C(dx, dy)` draws one column at camera x + dx, called from `UpdateCamera` with dx 255 (moving
  right) or 0 (left).
- `0x023372A4(dx, dy)` draws one row of 33 columns, called from `UpdateCamera` with dx 0.

Tables at `0x02352B9C` (column mod 3) and `0x02352D58` (column div 3, the dungeon tile), indexed by column
+ 30, run to column 191: with 12 extra columns each side they reach exactly the floor's last tile for any
camera on a 56-tile floor, and `GetTile` (`0x023360FC`) returns the default tile off the floor.
The map is 64 columns: 33 + 2 × 12 = 57 fit.

`frontend/pmdwide.h` changes 11 instruction words (listed there with their original values) so all three
draw 12 more columns on each side. Each word is checked before it is changed; the change is made again
each time the overlay loads (`[pmdwide] frame 24770: dungeon floor drawn 12 columns wider on each side`).
Measured: the map then holds 456 of 512 columns (57) on every sample in the first floor.

Only the map's off-screen columns change: the DS screen is identical, and no game data is read
differently. The first dungeon was reached at the same frame (24803) with and without the change.
(Not yet done: a `--hash-every` comparison of main RAM outside the map buffer over a scripted walk.)

## Drawing the wide view (wide2d.h)

Beside the screen: every 512-wide layer that shows at that edge of the screen on that row (so the grid,
left stale beside the screen when hidden, is not continued), sprites past the edges (9-bit X), the
line's scroll and brightness, the frame's palettes. A translucent cover over the world at the screen's edge
(measured on the 4 outermost world pixels against the bare world: a black or white cover of some weight)
is carried on beside it: this continues the dungeon's darkness. In ground mode the layers come from the
game's maps in RAM (below).

Known limits:

- The darkness is continued per row from the screen's edge: past a room's wall the floor beside the
  screen is lit where the game would darken it. The exact shape needs the 3D layer drawn wide (the game's
  darkness polygons; `GPU3D_Wide`), not done.
- Sprites beside the screen: the game only shows monsters within 6 tiles of the camera
  (`IsPositionVisibleAndOnScreen`), 144 px, against 171 px at 16:9. Not changed yet.
- Rotating, bitmap and 256-colour layers, mosaic and alpha blending between layers are not drawn beside
  the screen.

## Towns, cutscenes, the intro and the title (pmdground.h)

The refinement prompt's hypotheses, checked against this branch:

- **C1 (the game fills only near the camera): confirmed, and the cause of the seams.** Ground mode keeps
  each map whole in main RAM as 3x3-tile chunks and its renderer (overlay 11 `0x022EED98`) copies a
  window of 12 x 9 chunks (36 x 27 tiles) into the hardware map from column 2; the rest is never written
  (`SSR_VRAMW` on the map buffer `0x02143920`, DMA'd to `0x0600F000`: writes only from that function and
  the tile animator). The plain strips (16 to 32 px) were two layers scrolled differently (BG2 20, BG3 32
  in the meadow) ending at different columns of that window.
- **C2 (mirroring): confirmed**, it was this branch's own fill (wide2d.h, `b6527f1`): every row reflected
  about the last column with anything drawn. Removed; a cell-by-cell mirror is now widefill.h's last resort.
- **C3 (per-line effects only in the centre): not the cause here.** wide2d.h already uses each line's scroll
  and brightness (`LineScroll`, `LineBlend`); the streaks were the fills. No scene in the intro set uses
  per-line scroll changes on the world layers (checked with `SSR_BGMAP_DUMP`).
- **C4 (frame mismatch): not found.** The margins are drawn synchronously from the frame just emulated
  (Android does not use the pipeline); the detector below finds no margin/centre brightness jumps outside
  the Chunsoft logo (a composer slide, see known limits).
- **C5 (fixed inset): confirmed**, fixed below.

Reading the game's maps: `GROUND_STATE_MAP` (`0x02324CC0`) and `GROUND_STATE_WEATHER` (`0x02324CC4`) point to
`struct ground_bg`; the renderer state is at +0x210 and +0x264 (stride 0x54, longer than pmdsky-debug's
28-byte `map_render`). Measured with `SSR_PEEK` in the meadow (frames 2340-2700) and the stone room:

| Offset | Meaning | Meadow | Stone room |
|---|---|---|---|
| +6 (byte) | map repeats across (`wrap_around`) | 1 | 0 |
| +8, +C | width, height in chunks | 16, 16 | 60, 40 |
| +10, +14 | width, height in pixels | 384, 384 | 1184, 768 |
| +1C, +20 | chunks' tiles, layers 0 and 1 (9 u16 entries a chunk) | | |
| +24, +28 | chunk maps, layers 0 and 1 (u16, 64 a row) | | |
| +34, +38 | camera pixel | 83..233, 184 | |
| +44, +48 | window's first chunk | 3..9, 7 | |
| +4C, +50 | hardware scroll | | |

A layer is used for a hardware background only while its entries match 98% of the 36 x 27 window the
game drew (every frame). Map pixel for hardware map position (sx, sy): (cx*24 - 16 + sx, cy*24 + sy).
Where the picture really ends (a map that does not repeat, or blank tiles to the end of the view), it is
carried on with its own art (widefill.h, below). The soft colour of its last 8 columns (7 rows around next
to it, 49 rows around 24 px out) is now only the fallback: before a fill is ready, and where the map in RAM
cannot be trusted this frame.

## Carrying a picture on past its end (widefill.h)

The owner's second batch of screenshots: every frame whose sides read as stretched colour was a fixed
painting the game drew no wider than the DS shows (coastal panorama, treasure scene, dark forest, night
fountain, green clearing, title cave). All of them are ground mode with a source in RAM (`SSR_BGMAP_DUMP`
at frames 900, 1700, 2120, 5100, 5500: maps 45x72, 66x24, 39x72, 33x72, 39x42 tiles, none repeating),
so the fix is in one place.

- **The fill is a map of cells, not colours.** For each 8x8 cell beside the picture, which cell of the
  picture is shown there (`Wide2D::Fill`). Each frame wide2d.h draws that cell through the layer as
  usual: the frame's palette, tile animation, scroll, brightness and fades apply exactly as on the
  screen, and pixels stay on the 8-pixel grid, never blended.
- **How cells are chosen** (texture continuation, after Ashikhmin's coherent synthesis): outwards one
  column at a time, top to bottom, each new cell is a picture cell whose own inward neighbour has the same
  facing column as the cell placed inward of it, and whose own upper neighbour has the same bottom row as
  the cell placed above (sum of |dR|+|dG|+|dB| over those 16 pixels). The cell that continues the same
  stretch of art matches exactly, so whole stretches carry on together. Only rows within 6 cells
  (`kRowReach`), so sky stays sky. The same cell beside or under itself costs extra (`kRepeatCost`): it
  read as stamped lines of stars. Ties: a hash of the map, so a map always gets the same fill.
- **Mirror (tier D)** is in the same search at a fixed cost per pixel (`kMirrorCost` 54): it wins only
  where no cell of the picture fits. Over the intro, 0 to 15% of cells (most maps under 2%).
- **Pictures stacked in one map.** A tall map can hold several pictures one above the other (the
  night fountain, 312x600; the island, 312x720), separated by blank rows; each run of rows with art has its
  own columns, and cells come only from the picture being continued.
- **Objects over a picture** (the treasure scene's items, BG2 over BG3 with its own map): a layer drawing
  in under 55% of its picture's cells with another source layer behind it is left clear beside the
  screen (`kClear`), so the picture behind shows; copies of the items read as debris.
- **When it is built.** Once per map, on a worker thread, from a copy of main RAM, the layers' tile
  graphics and the palette (2 to 40 ms on the PC; under 10 ms for most maps). Key: a hash of the map's
  chunk tables, plus a sample of the tiles' pictures in video memory: full-screen paintings number their
  tiles the same way, so two scenes can share a map and differ only in video memory. Two samples count as
  the same scene unless more than a quarter differs, so animated tiles (the waterfall) do not rebuild it.
  The last 6 maps are kept. A palette still black at build time (under 3 colours) is retried 15 frames on.
- **Checked against the screen every frame, per layer** (wide2d.h). On up to 576 points of the screen
  where the layer is in front, its source must give the screen's colour (within 12 per channel). Below
  90%, the game's window moved on by a chunk is tried: offsets 0 or ±24 across and down, the best used
  (measured over the whole intro: every real mismatch was a whole chunk, and the two layers of a scene
  can be a chunk apart on different frames; the earlier free search found wrong offsets on the waves).
  A layer under 70% at all of them is not used from RAM that frame (`[wide2d] safety: ...`, at most once
  a second) and nothing of it is drawn beside the screen; a layer that matched the frame before and
  still matches 45% keeps last frame's offset once (the waterfall's tiles are changed in video memory
  after the frame is drawn, every 9 frames: it dropped out for a frame each time).
- **Pictures side by side.** The Nintendo, Pokémon Company, copyright and Chunsoft screens are one
  792x384 map, a 3x2 grid of 264x192 screens; at the Chunsoft logo the left side showed the white screen
  next to it. A boundary between pictures is a cell column where all but one of the 8 pixel rows change
  sharply and the 32 px either side differ on average by 120 (of 765), on a run of 8 cell rows or more,
  leaving pieces at least 28 cells wide. Rows with the same boundaries form pictures; the one under the
  screen's middle (`ChunkSource::viewTx`) is the one continued.
- **Plain pictures.** A picture 85% or more calm cells of its most common colour (the logo and copyright
  screens) is continued with such cells only: anything else beside it was pieces of its letters. A
  smooth gradient (Dialga's sky) is not "plain" and keeps its clouds. (Penalising busy cells everywhere
  was tried and dropped: it kept the trees out of the island's water too.)
- **Transparency** counts in the matching (64 per pixel where one side has art and the other only the
  backdrop): the treasure scene's dark painting no longer runs into a black strip.
- **Groups and sticky fills.** Layers of one size and placement are one picture (the forest's foliage
  and its dark come from two renderers); layers grouped the frame before stay grouped while their
  windows are a chunk apart. A layer keeps its last fill for 30 frames while its own map and tiles are
  the same, so a frame where a layer is missed or the grouping changes does not drop the fill.
- **The cover** (a black or white overlay carried on from the screen's edge, made for the dungeon's
  darkness) is, in towns and cutscenes, one value for the whole side: the median of the rows. The game
  fades those scenes with an overlay, the same on every row, so the sides fade with the screen; the
  island's light rays, different row by row, painted bands when carried on row by row.
- **Past what can be drawn** (no fill yet, a distrusted layer): the picture mirrored about its last
  column, fading toward its edge's soft colour (to 60% at 96 px). The soft colour alone read as streaks.

Frame by frame (18 November): the whole intro (9000 frames, every frame composed, `--shot-every 1`,
`SSR_WIDE2D_TRACE=1` for each frame's fill and check state) through `tools/pmd/check_temporal.py` (sides
that jump on a still screen, one-frame flickers) and `check_margins.py`. Temporal flags, first run to
last: 137, 74, 34, 30. Of the 30 left, 24 are the "Pokémon Mystery Dungeon" logo, a game sprite, flying
in from beyond the screen (correct), and 6 the fountain-to-island white burst (frames 3435-3441), of which
one frame (3437, the flash's peak) still shows blocks at the right. Safety activations: 23, then 10, then
1 (3437). `check_margins.py` flags left: dark art carried on as dark (treasure scene's right, the forest),
the fountain's dark frames, the main menu's fade.

Diagnostics: `SSR_WIDE_FILL=0` (off), `SSR_WIDE_FILL_DUMP=<dir>` (each map built, with its fill on both
sides, `fill_<w>x<h>_<seed>.ppm`; mirrored cells tinted red, rows of no picture blue),
`SSR_WIDE_FILL_DEBUG=1` (key and tile changes every 10 frames), `SSR_WIDE2D_TRACE=1` (per frame: each
layer with a fill F or a map without one S, and the check: matched samples, offset or SAFETY),
`SSR_WIDE2D_VERIFY=1` (how many on-screen
world pixels the drawing disagrees with), `SSR_BGMAP_DUMP=1` now also writes `srcmap_<frame>_<unit><bg>.png`
(a layer's map in RAM drawn with the tiles in video memory). Test: `tests/test_widefill.py` (synthetic
maps: a periodic texture continued exactly in step, the same fill every time, objects left clear, stacked
pictures kept apart, two screens side by side cut apart with the one in view continued, wide2d.h drawing
through the fill).

Not done from the immersive-widescreen prompt, and why:

- Tier B (scroll memory) adds nothing in ground mode (the whole map is in RAM, tier A) or in the dungeon
  (pmdwide.h has the game draw the columns). Not built.
- The 3D layer is still not drawn beside the screen (the dungeon's darkness past room walls; Dialga).
- Scene profiles and the contact-sheet lab: the dump above is the start of the lab; no per-scene settings
  yet. Procedural extras (stars, fog): not started.

## The panel (generic_scene.h, pmd.h)

`WINDOW_LIST` (`0x022A88DC`, 20 x 0xE0): bytes 4-7 x, y, width, height in tiles, byte 8 the engine (0 = A),
byte 0xB6 non-zero while shown (`valid`). Confirmed with `SSR_PMD_WINDOWS=1` in the dungeon: idle, only the
top screen's help (`1:16,16-240,176`); with the menu open, `0:16,16-64,112` (the menu), `0:16,128-240,176`
(the team box) and `0:88,32-240,48` (the floor name), exactly as drawn; the hidden message box has 0xB6 = 0.
SECOND SCREEN SIZE (30-100%, default 60%: 0.3 of the DS screen, 290 x 215 px at 1600 x 720). The panel fades
out in 7 frames when a window on the large screen, 8 px larger, reaches it, and back in 11 frames after none
has for 10. At 20:9 the panel lies wholly in the margin (x 261-338 of the 426-px view), so the dungeon's
windows never reach it; at 16:9 it overlaps x 219-256 and steps aside for the menu (checked).

No pixel-based fallback: the window list is read from the one supported version only, confirmed at run
time; a pixel detector would mistake the HUD line for a window.

## Checks and timings

- `python3 tools/pmd/check_margins.py <dir>`: over composed 1600 x 720 frames, flags black blocks next to
  the screen, brightness jumps between the margin and the screen's edge, flat stretches (`flat`: the
  margin one colour across while the screen beside it has detail, over a quarter of the rows), visible
  mirroring and straight vertical seams. Intro, 9000 frames every 20 (450 frames), with `--bench-compose`
  (the fill needs consecutive frames): before widefill.h 64 frames flagged (flat on the forest, the
  clearing, the treasure scene ...); after, 22: the Chunsoft logo (340-460, the composer's slide), the
  treasure scene's and forest's right side (the art is dark there: dark carried on, correct), one frame of
  the fountain (dark), and the main menu's fade (7680-7700).
- `[wide2d]` in the log every 300 frames: drawing the margins. PC (WSL1), 20:9 (86 columns a side): dungeon
  mean 0.67 ms, worst 1.13 ms; cutscenes mean 0.9 ms, worst 1.9 ms. With the fill and the check against the
  screen: cutscenes mean 1.0 to 1.5 ms, worst 5.6 ms (frames searching an offset). Building a fill (worker
  thread, not in the frame): 2 to 40 ms. Phone: pending.

## The picture (generic_scene.h, pmd.h)

`PmdFuncs` (pmd.h) names the mode from functions in `tools/pmd/C2SE.txt`. Dungeon, its menus and ground mode
make the bottom screen large (the earlier build had the dungeon's top screen large, which put the controls
page in the middle: the owner's symptom S1); the personality test the top. In a dungeon (with the change
made) and in ground mode the world fills the view (`Scene::wide`, a placed `WideImage`), and the top screen
is a framed half-size panel in the top right corner while it has text or windows on it.
