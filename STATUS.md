# Status: Pokemon Fulldex — Explorers of Sky support

Written for: the project owner, to pick up the next step.

Updated 18 November 2026. Branch `feature/pmd-explorers-of-sky`. Pushed to origin up to `dd535cd`.

**Core rule (owner's):** commit and push after every change.

**Sources used for this file:** git history of the branch (`git log main..feature/pmd-explorers-of-sky`), the
four session transcripts in `C:\Users\Home\.claude\projects\d--AI-APPS-DEV-pokemon-fulldex\` (15 to 18 November),
the device logs pulled in `D:\WSL\scratch\phone-s5` to `phone-s7`, and `docs/EOS-NOTES.md`. Commit hashes and
numbers come from those sources. Where a result was not checked on the phone, it says so.

---

## 1. Setup (15 November)

- Repo `d:\AI\APPS DEV\pokemon-fulldex`, branch `main`. Build in WSL1 Ubuntu 24.04 on `D:\WSL\Ubuntu-24.04`
  (WSL1 only; drive C off limits for project files). Android SDK in `/root/android-sdk` (build-tools 34.0.0,
  platform 34, NDK 27.2.12479018). JDK 17, CMake, Ninja, Pillow.
- Reference APK (`Pokemon fulldex apk\`, DS only, from the owner) compared with the new build. Named the new one
  `pokemon-fulldex-ds-only-android.apk` (`SHA256SUMS.txt` updated).
- ROMs in `rom\`, never committed. `SoulSilver (USA)` is the build-test ROM for the address tables. The `(E)` one
  is not used (header says US).
- Owner's rule: `rom\` never committed.
- Phone: Samsung Galaxy S21 Ultra (`SM-G998B`, serial `R5CR823DRHN`). USB debugging on. Owner plugs in when asked.
- First install: reference APK ran, widescreen shown. Explorers of Sky ROM tested on the phone (15 November).

## 2. Explorers of Sky support (15 to 16 November)

Goal (owner): Explorers of Sky (US) plays on the phone with the same widescreen presentation as SoulSilver: one
16:9 picture, the game as the main focus, the other screen beside it, nothing cut off, smooth.

- **Game code `C2SE`** (US, from ROM header). Registered in `KnownGames` (`launcher.h`), save file
  `pmd-explorers-of-sky.sav`. Plays from the list and from a direct ROM open.
- **Function table `tools/pmd/C2SE.txt`** (8 overlay functions, from `pmdsky-debug` commit `80f28e0`, MIT).
  FuncWatch detects the mode: ground, dungeon, dungeon menu, personality test. Confirmed on the phone (ground at
  frame 284, matching the PC run at 279).
- **Generic composer** (`generic_scene.h`, `merged.h`): the screen the game is played on fills the view; the other
  screen's content stands beside it, with seamless bars and edge continuation.
- Commits (pushed 16 November): `17e37de` plain emulator, `3943f31` save size check, `e76e1ca` large screen follows
  the action, `d104175` merged view fills the window, `3cc5a20` edges continued to the sides, `06f79df` game
  state decides the large screen.

Owner's reports during this stage (summarised from the transcript):
- Screen was the wrong way round: small screen should be the big one (15 Nov, screenshots).
- Wanted the official SoulSilver presentation as reference (15 Nov).
- Asked for a plan to make the whole game work on the phone (15 Nov).

## 3. Refinement F1 to F4 (16 to 17 November)

Owner's problems (from screenshots of the S21 Ultra, 1600 x 720 = 20:9):

| # | Problem | Result |
|---|---|---|
| F1 | Sides: plain strips, mirrored art, streaks | Fixed on PC. Ground-mode maps are kept whole in RAM. `pmdground.h` reads them (`GROUND_STATE_MAP` `0x02324CC0` → `ground_bg` → renderer state at +0x210 / +0x264). Used only where entries match 98% of the window the game drew. Mirroring removed. Where art truly ends, a soft fill into its own colours. |
| F2 | Transition blocks | Margin checker `tools/pmd/check_margins.py` over the intro: only the Chunsoft logo (frames 340–460) flagged; that is the composer's slide, not the margins. Later fixed in section 4. |
| F3 | Second screen too big | 60% default (0.3 of the DS screen). `SECOND SCREEN SIZE` 30–100% in the display settings. |
| F4 | Second screen covers menus | Fades out while a game window reaches it. `WINDOW_LIST` at `0x022A88DC` (20 × 0xE0; byte 0xB6 non-zero while shown). Back 10 frames after the last window closes. At 20:9 it sits wholly in the side margin, so dungeon menus never reach it. |

Other work in this stage:
- **Dungeon floor beyond the screen**: 12 extra columns each side, by changing 11 instruction words in overlay 29 in
  memory (`pmdwide.h`, each word checked before change, re-applied on overlay load). Map buffer `0x021B6EE0`.
  Confirmed on the phone: `[pmdwide] frame 18283: dungeon floor drawn 12 columns wider on each side` (install
  of `68d995a`, 17 November).
- **Margin renderer** (`wide2d.h`), timed on PC at 20:9, 86 columns a side: dungeon mean 0.67 ms, cutscenes 0.9 ms.
- **WIDE MAP LOADING** now applies and un-applies straight away for this game.
- Commits (pushed): `45fc300` real side art in towns and cutscenes + smaller panel, `3bd0dc1` margin timing log,
  `68d995a` ground maps notes and margin checker. Earlier: `b5036c8` dungeon floor wide, `b6527f1` towns and
  cutscenes wide, `a03b69b` notes and composer tests, `dd535cd` this file (tracked from now on).
- Device: APK from `68d995a` installed on the SM-G998B (`adb install -r`, `Success`, 17 Nov 00:55). Log: dungeon
  reached at frame 18294; menus detected; no crash.

## 4. Immersive widescreen pass (17 to 18 November)

Owner (17 Nov 12:00, after testing the phone): *"still having minor glitches here and there in the intro but so far
mostly looking good. can you test it out and check frame by frame."*

| Item | State |
|---|---|
| Paintings with stretched-colour sides (coastal panorama, treasure scene, dark forest, night fountain, green clearing, title cave) | Fixed on PC. `frontend/widefill.h` continues each picture with its own 8×8 cells; mirror only as last resort. Drawn each frame with the game's palette and animation. |
| Treasure scene: items copied as debris | Fixed: see-through object layer over another left clear beside the screen. |
| Dialga's sky: banded, wrong sides | Fixed: map in RAM checked against the screen each frame and offset to match; if nothing matches, the sides keep what the game drew (logged as `safety`). |
| Margin checker | Also finds flat stretches, mirrors and seams. Intro sweep: 64 frames flagged before, 22 after. |
| Frame-by-frame intro pass | Whole intro composed every frame, checked with `tools/pmd/check_temporal.py`, `check_margins.py`, and `SSR_WIDE2D_TRACE=1`. Sides jumping or flickering on a still screen: 137 frames before, 30 after (24 are the logo sprite flying in, correct; 6 the fountain-to-island white burst, one of which, frame 3437, still shows blocks at the right). |
| Fixed in the pass | Chunsoft logo (white beside it), copyright screens (letter pieces), fades not reaching the sides, island light-ray bands, treasure scene black strip and one-frame pops, waterfall flicker, forest losing its fill, soft-colour streaks in transitions (now a faded mirror). |
| Not done | Tier B (scroll memory), 3D layer beside the screen, scene profiles and contact-sheet lab, procedural extras (see `docs/EOS-NOTES.md`, widefill section). |

Commits:
- `b0f2498` paintings carried on with their own art beside the screen
- `7b46120` margin checker finds stretches, mirrors, seams; notes
- `c5ace24` the intro's sides checked frame by frame, glitches fixed
- `ba71803` frame-by-frame margin checker; notes on the intro pass

All four pushed (origin at `dd535cd`).

## 5. Device status

- Last install recorded in the session: `ba71803`, `Success` (18 Nov). APK v2 signature verified; no game files in
  the APK; `assets/funcs/C2SE.txt` present; 16 KB ELF alignment OK.
- The phone's log from the install of `68d995a` (17 Nov) showed the patch, the dungeon, and menus with no crash.
  Logs from `ba71803` are not yet pulled.
- **Owner has not yet reported on the sides or the panel from `ba71803`.** Please play the intro (let it run past the
  title once), New Game, the beach and the first dungeon, and say scene by scene what looks right or wrong.

## 6. Commit list (feature branch, from main)

`17e37de` `3943f31` `e76e1ca` `d104175` `3cc5a20` `06f79df` `b5036c8` `b6527f1` `a03b69b` `45fc300` `3bd0dc1`
`68d995a` `b0f2498` `7b46120` `c5ace24` `ba71803` `dd535cd`

## 7. Next steps

1. Owner: play the intro, New Game and the beach on the phone; say what is still wrong, scene by scene. Then pull
   `game.log` for `[wide2d]` (timings, `safety`) and `[widefill]` (build times).
2. Chunsoft logo slide (old F2 leftover; the logo still slides in the composer).
3. Dungeon darkness past room walls: needs the 3D layer drawn wide (`GPU3D_Wide`).
4. Scene profiles / contact-sheet lab for owner's picks, if the generic fill is not enough somewhere.
5. Sprites beyond 6 tiles from the camera (`IsPositionVisibleAndOnScreen`): shared with game logic, so left alone.
6. README, NOTICE and credits: `pmdsky-debug` (MIT, © 2022 UsernameFodder) for the function table.
7. Treasure Town and other large towns: checked only on the beach and first dungeon so far.
