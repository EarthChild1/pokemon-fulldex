# Status: Pokemon Fulldex — Explorers of Sky support

Written for: the project owner, to pick up the next step.

Updated 21 November 2026. Branch `feature/pmd-explorers-of-sky`. Pushed to origin.

**Core rule (owner's):** commit and push after every change.

## Pre-flight decisions (21 November 2026)

Done before any phase work, per the commence prompt (section 3). Evidence is the command and its output.

- **PF-0 workspace.** Branch `feature/pmd-explorers-of-sky`, head `2ea978a` (2026-11-18 01:16 +1100, "STATUS.md: full
  history from the session transcripts..."). Working tree clean except the untracked reference APK folder
  `Pokemon fulldex apk/` (owner's, never committed).
- **PF-1 upstream.** `git remote add upstream https://github.com/mamesj/pokemon-fulldex`;
  `git remote set-url --push upstream DISABLED`; `git fetch upstream`. **Authoritative developer head:
  `e0de264` (2026-11-20 09:00 UTC, "CHANGELOG: Vulkan for the 3DS games on Linux and Android, and what Auto
  picks").** The older prompts named `928f47b` and `fbf6cf8`; both are superseded. Since `4607874`: upstream 189
  commits, 256 files (+48714 -1195); this branch 18 commits, 32 files (+3878 -34). Phases 1 and 2 do not rebase onto
  it; phase 3 ports onto it (re-fetched at 3.1 and 3.6).
- **PF-2 reference file.** `build_environment_setup_prompt.md` searched in `D:\AI\APPS DEV`, `D:\WSL\scratch` and
  the scratchpad: **not found.** Reference file not provided; the commence prompt restates the rules that apply.
  Not blocking.
- **PF-3 commands and facts, checked at `upstream/main` (`git show upstream/main:<path>`).**
  1. `--wide-gpu` / `--no-wide-gpu`: `frontend/main.cpp` usage line 775, parser lines 858 and 873. Present.
  2. `--set k=v`: parser line 853, applied by `cfg.Apply(kv)` at line 2484; `aspect=` is a config key (read
     line 1818, written line 1926: `auto`, `16:9`, `16:10`, `3:2`, `4:3`). So `--set aspect=16:9` works.
  3. Default window: lines 2728-2729, `1280` x `720` in the PC launcher build unless a saved `window=WxH`
     (read line 1872, written line 1947, updated on resize line 9799) is at least 320 x 240.
  4. `--scale`: default 3 (line 734, parser 835), multiplies `LayoutNativeSize` (line 2721); the PC launcher build
     overrides it with the 1280 x 720 rule above.
  5. `g_mergedAspect = aspectValue[clamp(cfg.aspect,0,4)]` line 11330; ASPECT RATIO picker line 6600 with
     AUTO, 16:9, 16:10, 3:2, 4:3.
  6. `tools/pc/build_all.sh`: function tables into `$DATA/funcs` (line 110 on), copied to each package's
     `data/funcs` (line 159); a content check `check_contents` (line 175) with an allow-list regex of table names
     (line 179) applied to the zip and tarballs. Packages: `pokemon-ds-wide-pc-windows-x86_64.zip`,
     `pokemon-ds-wide-pc-linux-x86_64.tar.gz`, `pokemon-ds-wide-pc-linux-aarch64.tar.gz` and `.AppImage`.
  7. `tools/pc/README.txt` line 6, "It plays your own copies of:" then one line per game family.
  8. `tools/make_appimage.sh`, `tools/updater/write_checksums.py`, `tools/pc/mingw-w64.cmake`,
     `tools/pc/sdl2-mingw`: all present.
  9. Tests: `.github/workflows/native.yml` runs `python -m unittest discover -s tests -v` then
     `python tests/native_smoke.py --build-dir <dir>` (Linux, macOS and Windows/MSYS2 jobs).
  10. MinGW: was missing; installed in WSL with `apt-get install -y g++-mingw-w64-x86-64`.
      `x86_64-w64-mingw32-g++-posix` = GCC 13-posix (the cmake file wants the posix variant).
- **PF-4 environment.** WSL1 Ubuntu 24.04 (kernel 4.4.0-19041-Microsoft), 16 cores. OpenJDK 17.0.21. Android SDK
  `/root/android-sdk`: build-tools 34.0.0, android-34, NDK 27.2.12479018. cmake, ninja, g++, Pillow 10.2.0 present;
  clang absent. `build-native-pmd/ssrecomp` present (built 18 Nov). ROMs in `rom\` (names only): the Explorers of
  Sky (US) `.nds` and `.7z`, SoulSilver (USA) `.nds` and `.zip`. `adb devices` (Windows adb): **the phone is not
  attached; an Android emulator is** (`emulator-5554`, `sdk_gphone64_x86_64`, Android 17, ABIs `x86_64,arm64-v8a`,
  1080 x 2400 = 20:9 like the phone). The APK already ships `x86_64`, so the emulator is used for Android runs while
  the phone is away. **Emulator results are not phone results**; every phone gate stays open for the owner.
- **PF-5 push rule.** Commit and push every change, to `origin` only. Older "do not push" guidance is void.
- **PF-6 commit trailers.** The commence prompt says no model name in pushed text and to use a `Claude-Session`
  trailer. That session link is not this session's (this is a local VS Code session with no claude.ai/code link),
  so writing it would attribute the work to the wrong session. **From here on commits carry no trailer** until the
  owner decides. Earlier commits (up to `2ea978a`) carry `Co-Authored-By` with a model name; they are pushed and
  are not rewritten.
- **Open owner decisions.**
  1. Should a model name (a `Co-Authored-By` line) appear in commit trailers? And should a session trailer be
     used at all? Until answered: none.
  2. Phone gates (1.1 to 1.5 and phase 1 done): wait for the phone. The commence prompt says phase 2 starts only after
     every phase 1 gate, including the phone; the owner's 21 Nov instruction is to work around the phone. **Reading
     taken:** finish every phase 1 task that does not need the phone (headless harness and emulator), collect the
     phone checks into one batch, and keep going in order; the phone-confirmation lines stay "waiting for owner".

**Sources used for this file:** git history of the branch (`git log main..feature/pmd-explorers-of-sky`), the
four session transcripts in `C:\Users\Home\.claude\projects\d--AI-APPS-DEV-pokemon-fulldex\` (15 to 18 November),
the device logs pulled in `D:\WSL\scratch\phone-s5` to `phone-s7`, and `docs/EOS-NOTES.md`. Commit hashes and
numbers come from those sources. Where a result was not checked on the phone, it says so.

---

## Phase 1 (Android), from 21 November 2026

### How Android is tested while the phone is away

- An Android emulator of my own: AVD `eos_api37` (Android 17, x86_64, 1080 x 2400 = 20:9 like the phone), in
  `D:\AI\DevTools\android-user\avd`, run without a window on port 5556. (`emulator-5554` belongs to another
  project's session; my install and ROM copy there were removed.)
- Two emulator-only limits, both in the developer's code and not changed on this branch:
  1. The launcher divides by zero when a mouse event arrives before its first draw (`Launcher::Layout`,
     `frontend/launcher.h`, from `PickGame`; the emulator's virtual mouse sends one). Same code upstream. The
     emulator builds carry a one-line guard that is **never committed** (`D:\WSL\scratch\p1\emu_test.sh`).
  2. WORLD ON GPU needs OpenGL ES 3.1; the emulator's has 3.0, and the app crashes (SIGSEGV) after
     `EGL_BAD_CONFIG: no ES 3.1 support` instead of falling back. Emulator runs use `wide_gpu=0`, `ds3d_gpu=0`.
     The phone (Mali-G78, ES 3.2) is not affected, but the phone's graphics path is not tested by the emulator.
- Every emulator result below is an emulator result. Phone gates stay open for the owner.

### Task 1.1: the fresh New Game black view

- **Cause found, fixed, pushed:** `d2df393`, `3f1e848`, notes `c44521e` (docs/EOS-NOTES.md, "A new game's first
  screens"). The personality test writes its opening lines and asks its questions on the **touch** screen,
  with the top screen black or only a backdrop; the top screen was the large one, so the view was black and
  then had no question. The game was running and waiting for A. Now the screen with the game's windows is
  large (the top one for the result), and the touch screen while the top one is blank.
- **Not a regression:** black at `68d995a` too (emulator). The phone run of `68d995a` got through by pressing
  on blind. No first bad commit; the rule dates from `06f79df`.
- **Tests:** `tests/test_generic_game.cpp` (ROM-free), `tests/test_eos_new_game.py` (ROM-gated: failed before,
  passes after, headless, WSL).
- **Checked on the emulator** (build `3f1e848` + the test guard): New Game, "Welcome!", the questions with
  their answers, the Bow (a touch on the panel reaches the game: `[touch] 2160,540: ... touch screen 127,96`),
  the result, the naming, then the first ground scene (`[pmd] frame 19055: ground`).
- **Waiting for the owner (phone):** see the checklist below, items 1 to 4.

Found on the way (for tasks 1.3 and 1.4):
- P1. Opening lines and text boxes are **mirrored into the sides** ("erofeB" beside "Before you depart...";
  the Bow's "Don't move your finger" box mirrored left and right).
- P2. Personality-test backdrop: the sides have a hard seam at the view's 4:3 edge and a different phase and
  brightness from the middle; in the Bow step, flat horizontal stripes.

### Tasks 1.3 to 1.6: progress (22 November)

Done and pushed (headless and emulator checks; **not yet seen on the phone**):
- `a18ad14` Audit of the side fill against the owner's design (docs/EOS-NOTES.md, "The side fill against the
  owner's design"): tier A partly, B not built, C and D partly, selector, profiles, lab and extras not built;
  measures 1 to 5 partly.
- `f5ecbcb` Measure 5: alpha blending (BLDCNT effect 1) beside the screen, and the RAM-map check compares
  blended. Personality-test backdrop: no seam, check 486/486 (was safety 244/486). P2 fixed. Plain screens
  (text on black) carried on plain, not mirrored: P1 fixed for the opening lines. Palette retry logged once.
- `94eba13` Fill taken from the picture's edge quarter only: the night fountain's teal blocks and jet
  fragments gone (P4 mostly; small remnants at the far right).
- `f9ec739` Task 1.5: panel steps aside on a second signal (a window's body in the picture), unit tests for
  its place at 20:9, 16:9, 16:10 and the overlap; checked headless at 16:9 with the dungeon menu.
- `a55e72e` Task 1.6 item 5: pmdsky-debug in NOTICE and the Credits page; README row for the game.

Checks (intro, 9000 frames from power-on): temporal checker 31 frames (24 sprite fly-ins, the 3437 burst);
SAFETY 1 frame (3437); margin checker every 20th frame 30 (the island's water read as flat; the treasure
scene's dark side; the main menu's fade; 3820/3440 one side).

Also fixed since: `58782ca` message boxes on black screens no longer mirrored into the sides; `9b18f74` the
3437 burst no longer drops to safety (its big pop gone); `e0e86f9` P3, the sunset sea's black strips and block
(gaps in the art closed beside the screen). Intro now: safety 0 frames, temporal checker 29.

Open, found on the way:
- C1 "Chapter 1: A Storm at Sea" card (new game, ~5950-6100): a navy block and a piece of another painting at
  the right.
- C2 the storm at sea (~7500-8000): dark patches in the clouds at the right; at 7800-8000 BG2's map fails its
  check (safety) and the sides break into pieces of rock and wave.
- L1 Chunsoft and copyright screens: a piece of the neighbouring screen (a "(c)", a blue "N") at the outer
  edge, and the Nintendo screen's notice box in the right margin. A split of the map's pictures by empty
  columns was tried and reverted (it cut the screens into slivers).
- 3436-3438 the fountain-to-island burst: pale blocks and island pieces at the right for 3 frames.
- Not started: tier B scroll memory, the selector, profiles and the contact-sheet lab (needs the owner's
  picks), procedural extras, measure 2 cross-fade, the 3D layer wide (1.6 item 2), sprites beyond 6 tiles
  (1.6 item 3), towns beyond the beach (1.6 item 4), the timing table on the phone.
- A headless script now plays a new game from power-on into the first dungeon (`[pmd] frame 21791: dungeon`):
  `D:\WSL\scratch\p1\newgame.sh`.

### Android devices beyond the owner's phone (22 November)

The owner asked that the build play on most Android devices, not only theirs. What was checked (emulator results
are emulator results; the owner's phone is the only real device):
- **Android versions.** The app's minimum is Android 7.0 (API 24, about 97% of devices in use), unchanged.
  New emulators `eos_api24` (Android 7.0) and `eos_api29` (Android 10, the first with scoped storage), plus
  `eos_api37`: on each, install, the storage permission and the folder picker, the game list, and the game's
  intro all work. The owner's phone (Android 15) runs the same APK.
- **Graphics chips without OpenGL ES 3.1** (older and budget phones): WORLD ON GPU crashed the app at the first
  start (SIGSEGV after `EGL_BAD_CONFIG`). Fixed in `69c9c9b`: the processor draws the world instead, and
  game.log says why. Checked on the three emulators (their graphics have ES 3.0 only).
- **Mouse, Chromebook, Samsung DeX:** a mouse event before the launcher's first drawing divided by zero.
  Fixed in `8ed720b`. The emulator builds no longer need their uncommitted guard.
- **Screen shapes** (emulator, game in landscape): 4:3 and 16:10 tablets, 16:9, 20:9 fill the screen; 21:9
  shows black bars at the sides (the developer's aspect limit, left as it is).
- **16 KB memory pages** (Android 15 and later devices that use them): both libraries in both ABIs aligned
  (`tools/android/check_native_alignment.py`).
- **Processor types:** arm64-v8a (all phones and tablets from about 2017 on) and x86_64 (Chromebooks, PC
  emulators). 32-bit-only ARM phones (armeabi-v7a) are not built: melonDS's JIT is for 64-bit only, and this
  game runs on the JIT, so such a phone would be far too slow. Owner's call whether that matters.
- **Found, not fixed (upstream launcher):** a tablet held upright (4:3 or 16:10 portrait): the game tile is
  drawn over the music bar and under the Continue banner. Sideways it is right.
- APK with both fixes: `D:\WSL\scratch\p1\apk\eos-compat1.apk` (code at `8ed720b` with STATUS only after it;
  SHA-256 `614f330d...07a1`). **Installed on the phone on 22 November** in place of `eos-e0e8.apk` (the game
  code is the same; the save file unchanged, same size and date). The checklist below is run on it.

### The naming screen's flicker (fix_nickname_flicker prompt, 22 November)

The owner's report and material (on `eos-compat1`): two videos (12:15:39, 62 s, the beach story to "What is your
name?"; 12:17:41, 61 s, the intro, the personality test, the partner, the naming) and five screenshots of the
partner's nickname screen. Copies in `D:\WSL\scratch\naming\` (never committed). They also show a fresh New Game
from the opening lines through the questions and the naming to the beach, on the phone (checklist items 2 and 6,
as far as the videos go; the owner still says whether they count).

**Gate 3, cause (headless, WSL; full write-up in docs/EOS-NOTES.md, "The naming screen's flicker"):**
- Reproduced: new game to the nickname keyboard (frame 5363), then no input. The layout alternated 14 times in
  320 frames (2.5 a second; the videos show about two bursts a second), the same two states as the screenshots.
- None of H1 to H4. Window list, cover, large screen and mode never changed (`SSR_PMD_PANEL=1`, `SSR_PMD_WINDOWS=1`).
  The world beside the screen came and went (`PmdGround::Find`: 2, 1, 0 layers), and the composer slid the top
  screen left without it. Cause: the game moves its map window's chunk (+0x44, 4 to 5 at frame 5383) about 22
  frames before it copies the new window into the hardware map (360 of 972 entries matched until 5405, then 972).
- Second finding: on the nickname screen the controls' help was the large screen. The keyboard was a panel too
  small to read at 20:9, and at 16:9 it was never shown (the help's window lies under the panel's place).

**Gate 4, fixes (headless, WSL; pushed):**
- `046e388` the window a chunk to each side is tried too (same strict check). Idle: 0 changes in 320 frames.
  New game to the first dungeon: the world goes away 49 times instead of 59. Intro: 9000 frames byte for byte the same.
- `8f6c0e6` `SSR_PMD_PANEL=1`, each frame's layout to the log (off unless asked for).
- `cb32e46` the keyboard a mode of its own (`GetKeyboardStatus`, pmdsky-debug arm9, NA `0x02036CD4`, now in the
  table), the touch screen large. Seen only on the two keyboards of a new game (5363-5452, 18659-18764).
  Nickname screen: the keyboard large throughout; the help a panel at 20:9, out of the way at 16:9.
- `414902e` notes. Tests: `tests/test_pmd_ground.cpp` (synthetic), `tests/test_pmd_panel.cpp` (a cover flipping
  every frame keeps the panel out, no climb; the keyboard's mode; the table), `tests/test_eos_naming.py` (ROM,
  headless, 20:9 and 16:9: 0 changes of the large screen, the world or the slide in 420 idle frames, no fade out
  and back). The stale-window fix alone does not change that test's result once the keyboard is large (its map
  has no lag); its own evidence is the idle run with the top screen large and the new-game count above.
- Not verified yet: the phone. Not done yet: the minor-glitch sweep of the videos (section 6 of the prompt) and the
  known open items (section 5: 3437, the Chunsoft slide, copyright letters, light-ray bands, waterfall flicker).

**Section 6, the videos frame by frame** (every frame extracted with ffmpeg into `D:\WSL\scratch\naming\frA`,
`frB`; both checkers at 1280 x 576: A temporal 8, margins 1; B temporal 53, margins 233). Video times are the
recording's own; "frame" is the video frame (about 30 a second).

| Video, s (frame) | Scene | Side | What | Verdict |
|---|---|---|---|---|
| A 3.5, 5.2, 6.4 (111, 164, 204) | the fall, storm | all | one-frame white flash | correct: lightning, the centre flashes too |
| A 6.7 (213-215) | "Waaaaah!" over a grey fade | L, R | the message box mirrored into the left margin ("deeeaaW"), a box edge at the right | **fixed** headless (`c4de389`, reproduced at frames 6840-6920), phone to confirm |
| B 3.9-6.8 (113-203) | the logo flying in | R | small changes at the right | correct: the logo's flight, inside the screen |
| B 6.9 (189-191) | Grovyle and the Sableye | all | one dark frame | correct: the game's flash |
| B 7.8 (210) | the white burst to the island | L, R | pale blocks in both margins | **defect**, known item "3437", open |
| B 34.8 (971), 48.9 (1388) | end of a question | L, R | the sides go black a frame before the centre fades | **G3**: on the old build; `046e388` and `34ab2eb` at its causes, phone to confirm |
| B 35.4 (992) | the aura's bow | R | the top screen's panel appears at once | to check: whether it faded in (the video has 1 frame in 2) |
| B 39.7 (1120) | the bow fading | R | the bow's sparkle | correct: inside the screen |
| B 46.8 (1329), 49.1 (1394), 58.3 (1667) | partner choice, naming, "Is the name OK?" | L | a vertical band in the left margin for a frame | **G3** (the same short drops): as above, phone to confirm |
| B 46-60 (1388-1640) | the nickname screen | L, R | the layout jumping (seams, pops every 24 frames) | **fixed** (`046e388`, `cb32e46`), phone to confirm |
| B 33-35 (flat L) | the questions | L | margin read as flat | not a fill defect: the D-pad overlay covers it (touch buttons out of scope) |

G3, cause (headless, with a temporary per-layer log, not committed): when the backdrop changes (a new question's
colour), the renderer names the new map for 1 or 2 frames before the hardware has it (window chunk 4,1; 567 or
864 entries, 88 or 37 matching); no window near it matches, so the world is off for those frames (frames 1265-1266,
1721, 5781, 5876-5877 of the headless new game). `34ab2eb` keeps last frame's map while the hardware still holds
it exactly: 1721 goes; the four left are scene changes under black (the hardware itself between maps), not seen.
Also open: C1, the "Chapter 1" card (headless 5990-6120): the map in RAM holds the night card, a column of navy
tiles and the storm painting side by side, and the right margin shows the column and the next painting
(`SSR_WIDE_FILL_DUMP`: 576 x 216, 3 pictures). Splitting pictures was tried before and reverted; not fixed.

Test suite (WSL, `python3 -m unittest discover -s tests`): 124 tests, 14 skipped (ROM or tools absent), 1 error:
`test_ui_customisation.test_touch_editor` (the D-pad size after an edit). It fails the same at `68d995a` (before
this session's work): pre-existing, touch-button code, out of scope. The new tests pass.

APK for the phone: `D:\WSL\scratch\p1\apk\eos-nick2.apk` (code at `f1c1a0e`: the naming fixes, the kept map and
the fade; 16 KB alignment ok; no `.nds` or `.sav`; `assets/funcs/C2SE.txt` has `GetKeyboardStatus`; SHA-256
`c77bc205...1ad4`). The five EoS tests pass on it (headless, WSL). Not installed yet: the phone was not connected.

**On the phone (26 November, by Claude over adb, not the owner's confirmation):** `eos-nick2.apk` installed with
`-r` (a copy of the app's files first, `D:\WSL\scratch\p1\phone_backup_20261126_1625`). The app said the save is
blank: it is, and was already on 22 and 17 November (all 131072 bytes 00/FF, the same checksum in the save, its
prelaunch copy and the 17 November backup), so the in-game save was never written; the progress is in the state
slots (slot 0 18 November 01:31, slots 1 and 2), which exit does not write and which are unchanged. With Play
Anyway, a New Game: the opening lines shown, the questions, the Bow held, the aura, the partner, the nickname
screen with the keyboard large and the help small at the top right. 20 s recorded idle on it (`ph_naming.mp4`,
60 fps, 1188 frames): no layout change. Left: once every 48 frames (0.8 s) the left margin loses its waves for one
frame (temporal checker: popL 30-80, against 150-450 in the owner's video). **G4, open**: headless the side check
matches about half (113 of 209 samples, offset search) on the two frames after each chunk step of the backdrop
(5384, 5386, 5432, 5434, ...); one of its two blended wave layers is dropped there. Not yet fixed.

**On the phone, second pass (26 November, Claude over adb):** the first pass's game.log showed every typed letter
dropping the keyboard's mode for about 20 frames (the top screen large in between). `d5896fb` holds the mode 90
frames; `eos-nick3.apk` installed (slots and save checked unchanged before and after), New Game again to the
nickname, 30 s recorded while typing (A and B in turn every 1.5 s): the keyboard's mode held through all of it.
One question ("Is the name OK?") came and went by a press, and as it closed the top screen was large for 5 frames
(7592-7597); headless the same as the keyboard opens (5359-5363). `05ac196`: a large screen the mode asks for
replaces the shown one only after 8 frames in a row (not yet on the phone: `eos-nick4.apk`). G4 (a band in the left
margin for one frame every 0.8 s) still seen in this recording. The pass stopped at the name question when a call
came in on the phone.

**Owner, please (section 7 of the prompt):**
1. Install the new APK (below) and play the naming again: does the picture hold one layout the whole time
   (keyboard large; on your phone the controls' help small at the top right)?
2. For each minor glitch you saw "here and there": the video name and the second (or say what and where).
3. Later, confirm each fixed glitch on the phone.

### Phone checklist (one batch, when the phone is plugged in)

APK: `D:\WSL\scratch\p1\apk\eos-e0e8.apk` (code at `e0e86f9`; arm64-v8a and x86_64; signature ok; 16 KB alignment
ok; `assets/funcs/C2SE.txt` present; no `.nds` or `.sav`; SHA-256 `4b545b75...3adc`). Install with
`adb install -r` (keeps the save). **Installed on the phone on 22 November** (save files kept; a copy of the
app's files taken first, in `D:\WSL\scratch\p1\phone_backup_*`). Then, scene by scene, say for each: sides convincing (yes/no); any seam,
mirror, stretch, black block or banding (where, when); second screen size and hiding (right/wrong); anything else.

1. A loaded save still plays (task 1.1 gate).
2. A fresh New Game from a clean start (Manage Saves: back up the save first, then start new): "Welcome!" and
   the lines are shown, the questions show their answers, the Bow can be pressed, the naming, the first scene.
3. Boot and Chunsoft logo; copyright screens; title cave (let the title run once).
4. The intro: storm, waves, beach, the fountain and island scenes.
5. New Game menu over the Grovyle artwork; the black text screens.
6. Personality test; naming; the beach dialogue.
7. First dungeon B1F and B2F: the Stairs menu and its information window; the Controls panel top right; a menu
   open.
8. Fast forward on and off.
Then I pull `game.log` and logcat.

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
