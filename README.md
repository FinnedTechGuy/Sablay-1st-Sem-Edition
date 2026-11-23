# SABLAY — 1st Semester Build (`2C1MILL`)

Hey! This is our progress build for **SABLAY**, a retro UP Tacloban campus platformer written in **C99** using **raylib 5.5**. Right now the playable build covers **Stages 1 to 5** (`NSTP 1`, `SCI 10`, `MATH 18`, `CMSC 56`, and `P.E. 1`), along with the full menu flow, GWA health mechanics, 8-bit sound effects, and save-file persistence.

---

## What's Working Right Now

### Menus & Presentation
- **Main Menu & Artwork**: Loads our custom pixel-art Oblation + SABLAY banner directly from `assets/campus_bg.jpg` (instead of drawing the campus pixel-by-pixel in C). Menu options sit cleanly below the plaque with a thick 8-way black outline + drop shadow so there's no boxy rectangle blocking the art.
- **Audio Settings**: Adjust Master Volume and SFX Volume in 10% increments (saved to `save_profile.txt`).
- **Course Briefing, 64px HUD & Pause**: Shows the stage title, hint, timer, gravity multiplier, live GWA, score, slips (`0/3`), and research papers (`0/3` in this Week 3 build). Pressing `ESC`, `P`, or clicking the top-right gear opens Pause.
- **Game Over & Victory**: Hitting `3` slips, `5.00` GWA, or `0s` brings up the UP Registrar `NOTICE OF ACADEMIC DISMISSAL` (`DROPPED`) screen with `RETAKE COURSE` (restores your stage-entry snapshot) or `MAIN MENU`. Clearing Stage 2 opens the Week 3 Victory screen with dynamic Latin honors (`SUMMA`, `MAGNA`, `CUM LAUDE`, `PASSING`).

### Playable Stages
1. **Stage 1 — `NSTP 1` (`CWTS`)**:
   - Campus orientation stage teaching movement, climbing ladders, and pushing a wooden crate onto the `CRATE` pressure plate to bring the exit terminal online.
   - Includes a patrolling stress hazard (stomp from above for `-0.25` GWA recovery, or take `+0.50` GWA + `1` slip on contact), `1` research paper, and `1` UP coffee mug.
2. **Stage 2 — `SCI 10` (`LOW-GRAV`)**:
   - Switches gravity to `0.55x` so Isko floats farther across gap pits.
   - Features `2` oscillating purple hazard orbs, sequential `STEP 1` -> `STEP 2` pressure plates (stepping on `STEP 2` early warns you to hit `STEP 1` first), `2` research papers, and `1` coffee mug.
3. **Stage 3 — `MATH 18` (`CALCULUS`)**:
   - Displays the chalkboard equation `x^2 - 5x + 6 = 0`. Jump over the false root `x = 1` (`+0.50` GWA and `+1` slip penalty) and step on the true roots `x = 2` and `x = 3` (`+200` pts each), with optional upper platforms for UP Coffee and Paper `4/7`.
4. **Stage 4 — `CMSC 56` (`DISCRETE`)**:
   - Interactive boolean circuit `OUT = (A AND B) OR (NOT C)` starting at `A=0, B=0, C=1`. Climb ladders and step or press `E` on `IN A`, `IN B`, `IN C` to turn the wire trace green (`OUT = 1`), with `2` stress sprites, `2` papers, and UP Coffee.
5. **Stage 5 — `P.E. 1` (`AGILITY`)**:
   - Timed `70s` obstacle course: jump over `2` white/red hurdles, dodge/stomp fatigue sprites, climb to ring the balcony `BELL`, and ride `2` blue moving platforms across the floor pit to the exit.

---

## Devlog: Bugs & Fixes Along the Way

- **Removing the huge hardcoded background code**: Originally `src/main.c` painted the entire Oblation campus pixel-by-pixel in `buildCampusBg()` plus drew stage gradients/stars manually. We replaced all of that with `loadFilteredTex()` loading `assets/campus_bg.jpg`, `assets/stage1_bg.jpg`, and `assets/stage2_bg.jpg` directly.
- **Main Menu readability without a dark box**: After switching to `assets/campus_bg.jpg`, the bright speckled pedestal behind `NEW GAME / CONTINUE / SETTINGS / EXIT` made thin text hard to read, while a translucent black rectangle looked too boxy. Switching `drawOutlinedText()` to an 8-direction outline + drop shadow and giving `drawMenuOptions()` a double black halo kept the text crisp with zero box behind it.
- **`ESC` quitting the whole game**: Raylib closes the window on `ESC` by default, so we added `SetExitKey(KEY_NULL)` in `src/main.c` and handled `ESC` ourselves for Pause and Settings.
- **Corner snags & one-way platforms**: Checking X and Y collisions together made Isko catch on platform corners. Splitting collision into X-first then Y-first—and only landing on thin ledges (`h < 24px`) while falling (`vy >= 0` with previous feet above the ledge)—fixed jumping up through platforms.
- **Instant 3-slip wipeouts**: Touching a hazard every frame drained all 3 slips immediately, so `applyPenalty()` now grants `1.25s` of invulnerability frames (`invuln_timer`).

---

## Controls

- `A / D` or `Left / Right`: Move & push the crate
- `W / Space / Up`: Jump (or climb up ladders)
- `S / Down`: Climb down ladders
- `Enter / Space / E`: Select menu items / confirm briefing / toggle CMSC 56 switches
- `Esc / P` (or click the top-right gear): Pause / Settings

---

## Project Layout & Background Assets

```text
Sablay-1st-Sem-Edition/
├── assets/
│   ├── campus_bg.jpg        # Main Menu + Settings/Briefing/Endings backdrop (1024x576)
│   ├── stage1_bg.jpg        # Stage 1 (NSTP 1) playfield backdrop
│   ├── stage2_bg.jpg        # Stage 2 (Science 10) playfield backdrop
│   ├── icon.png             # Transparent 256x256 window/taskbar icon (cropped PNG)
│   ├── icon.ico             # Windows Explorer .exe icon wrapper
│   ├── app.rc               # Windows resource script embedding icon.ico
│   ├── isko_idle.png        # Player idle sprite (36x36 PNG)
│   ├── isko_run1.png        # Player run frame 1 (36x36 PNG)
│   ├── isko_run2.png        # Player run frame 2 (36x36 PNG)
│   ├── isko_jump.png        # Player jump sprite (36x36 PNG)
│   └── README.txt           # Asset inventory notes
├── include/game.h           # Shared structs, constants, SceneState enum, prototypes
├── src/core.c               # Pure C99 game logic: save/load, GWA engine, physics, puzzles
├── src/main.c               # Raylib window, JPEG/PNG loading, 8-bit SFX, UI screens
├── .gitignore               # Ignores compiled *.exe / *.o / *.res binaries & local saves
└── Makefile                 # Strict C99 build (with windres icon) & --smoke-test script
```

---

## Build & Run (Windows MSYS2 UCRT64 + raylib)

```powershell
make
./sablay_week3.exe
```

Run the `--smoke-test` (hidden window check to verify textures & audio load/unload cleanly):

```powershell
make test
```

---

## References & Docs Used

Everything that went into building this Week 3 milestone, mapped to the actual parts of the codebase:

- **Main Window, Game Loop, Input & File Checks (`src/main.c`)**:
  - [raylib Cheatsheet (`rcore`)](https://www.raylib.com/cheatsheet/cheatsheet.html) and [raylib Examples (`core_basic_window.c`)](https://www.raylib.com/examples.html) — `InitWindow`, `SetWindowIcon`, `SetTargetFPS(60)`, `SetExitKey(KEY_NULL)`, `SetConfigFlags(FLAG_WINDOW_HIDDEN)` (for `--smoke-test`), `GetFrameTime`, `IsKeyPressed`/`IsKeyDown`, `IsMouseButtonPressed`, `GetMousePosition`, and `FileExists`.
- **JPEG Backgrounds & PNG Sprites (`loadFilteredTex`, `buildGrassTile`, `makePixelTex`)**:
  - [raylib Cheatsheet (`rtextures`)](https://www.raylib.com/cheatsheet/cheatsheet.html) and [raylib Examples (`textures_image_loading.c`, `textures_image_drawing.c`)](https://www.raylib.com/examples.html) — `LoadTexture`, `SetTextureFilter` (`TEXTURE_FILTER_BILINEAR` for `assets/*.jpg`, `TEXTURE_FILTER_POINT` for `assets/isko_*.png` and pixel tiles), `DrawTexturePro`, `UnloadTexture`, plus CPU-side `GenImageColor`, `ImageDrawRectangle`, `LoadTextureFromImage`, and `UnloadImage`.
  - Player character sprites (`assets/isko_idle.png`, `isko_run1.png`, `isko_run2.png`, `isko_jump.png`) from [Ozzbit Games — Action Platformer Character Template](https://ozzbit-games.itch.io/action-platformer-character-template).
  - Custom pixel-art Oblation + SABLAY title artwork saved at `assets/campus_bg.jpg` (`1024x576`), plus UP Tacloban corridor backgrounds (`assets/stage1_bg.jpg`, `assets/stage2_bg.jpg`).
- **2D Shapes, Outlined Text & UI Screens (`drawOutlinedText`, `drawMenuOptions`, `drawSablayBox`, HUD & Modals)**:
  - [raylib Cheatsheet (`rshapes` & `rtext`)](https://www.raylib.com/cheatsheet/cheatsheet.html) — `DrawRectangle`, `DrawRectangleRec`, `DrawRectangleLines`, `DrawRectangleLinesEx`, `DrawLine`, `DrawCircle`, `CheckCollisionPointRec`, `DrawText`, `MeasureText`, and `TextFormat`.
  - Classic arcade 8-direction text outline + double-halo technique in `drawOutlinedText()` and `drawMenuOptions()` so menu options stay readable over bright backgrounds without needing a dark box behind them.
- **Procedural 8-Bit Sound Effects (`makeTone`)**:
  - [raylib Cheatsheet (`raudio`)](https://www.raylib.com/cheatsheet/cheatsheet.html) and [raylib Examples (`audio_raw_stream.c`)](https://www.raylib.com/examples.html) — `InitAudioDevice`, `Wave`, `MemAlloc`, `LoadSoundFromWave`, `UnloadWave`, `SetMasterVolume`, `SetSoundVolume`, `PlaySound`, and `UnloadSound` generating `22050 Hz` 16-bit mono square, triangle, and LCG-noise (`1103515245u`) tones with linear decay.
- **2D Platformer Physics, Puzzles & State Machine (`include/game.h`, `src/core.c`)**:
  - **C99 2D Platformer References**: [raylib `core_2d_camera_platformer.c`](https://github.com/raysan5/raylib/blob/master/examples/core/core_2d_camera_platformer.c) (`Player` struct, platform `Rect` blocks, gravity, jumping, and collision checks) and [raysan5/raylib-games (`classics/src/platformer.c`)](https://github.com/raysan5/raylib-games) for the minimal C99 platformer game-loop backbone.
  - **Finite State Machine (`SceneState`)**: 6 scenes (`MENU`, `SETTINGS`, `BRIEFING`, `GAME`, `GAMEOVER`, `VICTORY`), pause overlay, and stage-entry snapshots (`retakeCurrentStage`) so `RETAKE COURSE` cleanly restores GWA/score/papers.
  - **Separate-Axis AABB Collision & One-Way Ledges (`boxesOverlap`, `movePlayerAndCrateX`, `movePlayerY`)**: X-axis resolution + crate pushing first, then Y-axis gravity/jumping, with downward-only ledge landing (`vy >= 0 && prev_y + h <= plat.y + 8`) for thin platforms (`h < 24px`).
  - **Math & Motion (`<math.h>`)**: `0.55x` gravity scaling in Science 10, sinusoidal bobbing (`sinf`) for collectibles/hazard orbs, and `fabsf`/`floorf`/`ceilf`/`roundf`/`fmaxf` for timer/score/collision math.
- **C99 Standard Library & Save-File Persistence (`loadProfile`, `saveProfile`)**:
  - `<stdio.h>` (`fopen`, `fscanf`, `fprintf`, `fclose`, `snprintf`) for reading/writing `save_profile.txt`.
  - `<string.h>`, `<stdbool.h>` (`memset`, `memcpy`, `strcmp`, `bool play_sfx[SFX_COUNT]`) for decoupling pure logic in `core.c` from raylib rendering in `main.c`.
- **Compiler & Build Toolchain (`Makefile`)**:
  - [MSYS2](https://www.msys2.org/) UCRT64 (`mingw-w64-ucrt-x86_64-gcc`, `mingw-w64-ucrt-x86_64-raylib`, GNU `make`) and [raylib (`raysan5/raylib`)](https://github.com/raysan5/raylib), compiled under strict C99 flags (`-std=c99 -Wall -Wextra -Werror -g`) linked with `-lraylib -lopengl32 -lgdi32 -lwinmm -lm`.
