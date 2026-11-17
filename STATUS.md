# Status: Pokemon Fulldex — Explorers of Sky support

Written for: the project owner, to pick up the next step.

Updated 18 November 2026, branch `feature/pmd-explorers-of-sky`. Pushed to origin up to `68d995a`. Commits after that (`b0f2498`, `7b46120`, `c5ace24`, `ba71803`) are local only unless noted below.

This file is untracked by git. It was lost once (overwritten on disk) and rebuilt on 18 November from the session record. Sections are kept in order, oldest first.

## Session 1: setup (before 15 November)

- Build environment: WSL1 Ubuntu 24.04 on `D:\WSL\Ubuntu-24.04`, Android SDK in `/root/android-sdk`, repo at `d:\AI\APPS DEV\pokemon-fulldex`.
- Reference APK checked, repo cloned, SDL2 source verified, ROM checked (both `(USA)` and `(E)` at 128 MiB; `(E)` header says US, not used).
- Constraints: drive C off limits for project files; ROMs kept in `rom\` and never committed; `build_all.sh` needs the SoulSilver ROM for its address tables.

## Session 2: Explorers of Sky support (15 to 16 November)

- Game code `C2SE` (US, `pmdsky-debug` NA addresses). Registered in KnownGames. Save file `pmd-explorers-of-sky.sav`.
- Function table `tools/pmd/C2SE.txt` (8 overlay functions). Scene detection via FuncWatch: ground, dungeon, dungeon menu, personality test. Confirmed on device (ground mode at frame 284).
- Generic composer (`generic_scene.h`): the screen the game is played on fills the view; the other screen's content goes beside it. Seamless bars, edge continuation.
- Commits: `17e37de`, `3943f31`, `e76e1ca`, `d104175`, `3cc5a20`, `06f79df` (pushed to origin 16 November).

## Session 3: refinement prompt, F1 to F4 (17 November)

Owner's problems: F1 sides (plain strips, mirrored art, streaks); F2 transition blocks; F3 second screen too big; F4 second screen covers menus.

| # | Item | State |
|---|---|---|
| F1 | Sides in towns, cutscenes, intro, title | Fixed on PC. Ground maps kept whole in RAM as 3x3-tile chunks; `pmdground.h` reads them (`GROUND_STATE_MAP` at `0x02324CC0` → `ground_bg` → renderer state at +0x210 / +0x264). Used only where the entries match 98% of the window the game drew. Mirroring removed. Where art truly ends, a soft fill into its own colours. |
| F2 | Transition blocks | Margin checker `tools/pmd/check_margins.py` over the intro: only the Chunsoft logo (frames 340–460) flagged. Composer slide, not the margins. |
| F3 | Panel too big | 60% default (0.3 of DS screen). SECOND SCREEN SIZE 30–100% in display settings. |
| F4 | Panel covers menus | Fades out while a game window reaches it (`WINDOW_LIST` `0x022A88DC`, 20 × 0xE0, byte 0xB6 = shown). Back 10 frames after the last closes. At 20:9 it sits wholly in the margin. |

- Dungeon floor beyond the screen: 12 extra columns each side, by changing 11 instruction words in overlay 29 in memory (`pmdwide.h`, checked before change). Map buffer `0x021B6EE0`; writers traced with `SSR_VRAMW`.
- Margin renderer timing (PC, 20:9, 86 columns a side): dungeon mean 0.67 ms, cutscenes 0.9 ms.
- Commits: `b5036c8`, `b6527f1`, `a03b69b`, `45fc300`, `3bd0dc1`, `68d995a` (pushed).
- Device: APK from `68d995a` installed on SM-G998B (`adb install -r`, `Success`, 17 Nov 00:55). Log showed `[pmdwide] frame 18283`, dungeon at frame 18294, menus detected, no crash. Owner has not yet reported on the sides or panel.

## Session 4: immersive widescreen prompt (18 November)

| Item | State |
|---|---|
| Paintings with stretched-colour sides (coastal panorama, treasure scene, dark forest, night fountain, green clearing, title cave) | Fixed headless. `frontend/widefill.h` continues each picture with its own 8x8 cells; mirror only as last resort. Drawn each frame with the game's palette and animation. |
| Treasure scene: items copied as debris | Fixed: see-through object layer over another left clear beside the screen. |
| Dialga's sky: banded, wrong sides | Fixed: map in RAM checked against the screen each frame and offset to match; if nothing matches, sides keep to what the game drew (logged as `safety`). |
| Margin checker | Also finds flat stretches, mirrors, seams. Intro sweep: 64 frames flagged before, 22 after. |
| Tier B (scroll memory), 3D layer beside the screen, scene profiles and lab, procedural extras | Not done (see `docs/EOS-NOTES.md`, end of widefill section). |

Commits: `b0f2498` (fill, check against screen, test), `7b46120` (checker, notes).

### Frame-by-frame pass (18 November)

Whole intro composed every frame and checked (`tools/pmd/check_temporal.py`, `check_margins.py`, `SSR_WIDE2D_TRACE=1`). Sides jumping or flickering on a still screen: 137 frames, now 30 (24 are the game's logo sprite flying in, correct; 6 the fountain-to-island white burst, one frame of which, 3437, still shows blocks at the right). Fixed: Chunsoft logo (white beside it), copyright screens (letter pieces), fades not reaching the sides, island light-ray bands, treasure scene's black strip and one-frame pops, waterfall flicker, forest losing its fill, soft-colour streaks in transitions (now a faded mirror).

Commits `c5ace24`, `ba71803` (local). APK from `ba71803` installed on SM-G998B (`Success`).

## Device status

- Last install: `ba71803`, `Success`. v2 signature verified, no game files in APK.
- Owner has not yet seen this build on the phone.

## Next steps

1. Owner: play the intro (let it run past the title once), New Game, the beach. Say scene by scene whether the sides look right. Pull `game.log` for `[wide2d]` (timings, `safety`) and `[widefill]` (build times).
2. Chunsoft logo slide (F2 leftover).
3. Dungeon darkness past room walls (needs the 3D layer drawn wide).
4. Scene profiles / contact-sheet lab for owner's picks, if generic fill is not enough somewhere.
5. README / NOTICE / credits (pmdsky-debug, MIT).
6. Push `c5ace24`, `ba71803`, `b0f2498`, `7b46120` to origin once owner confirms the phone build.
