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
- **C2 (mirroring): confirmed**, it was this branch's own fill (wide2d.h, `b6527f1`). Removed.
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
Where the picture really ends (a map that does not repeat, or blank tiles to the end of the view), it goes
on in the soft colour of its last 8 columns (7 rows around next to it, 49 rows around 24 px out): no mirrored
or repeated copies. Scenes with art exactly the window's width (the waterfall, the title cave, the world
map) show that fill beyond the art; the game has no picture there.

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
  the screen and brightness jumps between the margin and the screen's edge. Intro, 9000 frames every 20:
  only frames 340-460 (the Chunsoft logo).
- `[wide2d]` in the log every 300 frames: drawing the margins. PC (WSL1), 20:9 (86 columns a side): dungeon
  mean 0.67 ms, worst 1.13 ms; cutscenes mean 0.9 ms, worst 1.9 ms. Phone: pending.

## The picture (generic_scene.h, pmd.h)

`PmdFuncs` (pmd.h) names the mode from functions in `tools/pmd/C2SE.txt`. Dungeon, its menus and ground mode
make the bottom screen large (the earlier build had the dungeon's top screen large, which put the controls
page in the middle: the owner's symptom S1); the personality test the top. In a dungeon (with the change
made) and in ground mode the world fills the view (`Scene::wide`, a placed `WideImage`), and the top screen
is a framed half-size panel in the top right corner while it has text or windows on it.
