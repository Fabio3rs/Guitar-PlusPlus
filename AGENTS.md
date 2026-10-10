# Guitar++ — Agent instructions

## Project identity and priorities

Guitar++ (Guitar-PlusPlus) is a Brazilian indie rhythm game written primarily in C++17. Understand the existing gameplay and rendering pipeline before changing it.

- **Plus** is the game's named special ability, conceptually similar to the special power in Guitar Hero: correctly playing a marked sequence of special notes earns Plus energy, which the player can activate for a temporary scoring boost. Use **Plus** in project terminology, code and UI. Do not casually rename it to a term borrowed from another game.
- The 3D note models have a distinctive shape that loosely resembles a **`+`**. Preserve that visual identity and note readability when altering models, rendering, or UI.
- The game is intended to run on **low-end/older computers** while still making the most of available hardware: 3D components, lighting, particles and other visual effects are intentional features. Optimize and scale their cost rather than removing visual richness by default.
- **Input responsiveness, musical timing and correct note judgment take precedence** over visual polish. A prettier effect is not a reason to introduce input latency, audio desynchronization or missed frames.
- Favor focused, maintainable improvements. Legacy names and patterns are not, on their own, a reason for a large unrelated rewrite.

## Code map: where to start looking

- `src/main.cpp`: startup, Lua configuration, menu wiring and top-level execution.
- `src/GPPGame.{h,cpp}`: overall game coordination, menus, themes, resource loading, gameplay modes and asynchronous texture/model loading.
- `src/CEngine.{h,cpp}`: window, OpenGL rendering, drawing buffers, timing and BASS audio integration.
- `src/CGamePlay.{h,cpp}`: gameplay update/render pipeline, note highway, timing, effects and Plus progression/activation.
- `src/CPlayer.{h,cpp}`: per-player state, chart notes, input state, note judgment, combo, score and Plus energy.
- `src/CChart.{h,cpp}`: Feedback-style chart parsing, BPM/offset conversion, instrument note data, Plus phrases and compiled chart handling.
- `src/CCharter.{h,cpp}`: chart editing; `src/CControls.{h,cpp}`: keyboard/controller input.
- `src/CGuitars.{h,cpp}` and `src/GPPOBJ.{h,cpp}`: instrument configuration and OBJ-based 3D models.
- `src/CShader.{h,cpp}`, `src/FireShader.h`, `src/PlusLightningShader.h`, `src/CPlusLightning.{h,cpp}` and `src/CParticle.{h,cpp}`: shaders, fire, lightning and particle effects.
- `src/CMenu.{h,cpp}`, `src/CFonts.{h,cpp}`, `src/CLuaH.*`, `src/CLuaFunctions.*`, `src/CLanguageManager.*`: menus, text, Lua scripting and translations.
- `src/CSaveSystem.*`, `src/CCampaing.*`, `src/CMultiplayer.*`: persistence, campaign and networking. Some existing identifiers have historical spelling; do not rename them as an incidental cleanup.
- `tests/`: GoogleTest-based tests. `CMakeLists.txt` and `.github/workflows/BuildGuitarPP.yml` are the build/CI references.

`glfw`, `glm` and `cereal` are Git submodules; `src/Lua/` and `src/utf8/` contain bundled third-party code. Avoid editing these when implementing game features unless explicitly necessary.

## Gameplay and chart invariants

- Preserve the relationship between **audio playback time**, chart offset/BPM conversion, note positions and judgment windows. Avoid logic that assumes a fixed frame rate; use the project's established clocks and delta-time mechanisms appropriately.
- Preserve distinct behaviors for chords, HOPOs, taps, open notes and sustained notes. In particular, `notesFlags` contains **bit flags**, including state bits; do not treat its values as a mutually exclusive enum or discard unrelated bits during updates.
- **Plus phrase data and the player's Plus state are different things.** `CChart`/`CPlayer::NotesData` store phrases (`gPlus`, `firstNote`, `lastNote`), while gameplay tracks progression and handles energy (`plusPower`), activation (`plusEnabled`) and reward effects (`bPlusStrike`, `CPlusLightning`). Understand all related paths before changing Plus logic.
- Changes to Plus detection must preserve the intended condition that the special sequence be played correctly, including its boundaries and misses. Test full sequences, interrupted sequences and transitions between consecutive phrases.
- Keep judgment, score, combo and Plus behavior consistent across player instances. Avoid leaking one player's state, camera, input or effects into another's.
- Chart parsing and serialized save/chart data are compatibility-sensitive. Do not change formats or saved field layouts without explicitly handling existing content and explaining any migration.

## Rendering and performance

- **Maintain the existing OpenGL compatibility baseline unless a task explicitly changes it.** The engine requests an OpenGL **2.0** context and built-in effects use **GLSL 1.10**. Do not introduce core-profile-only APIs, recent GLSL syntax or mandatory newer GPU features without a compatible path.
- Effects should remain visually worthwhile on capable hardware while having **bounded, predictable cost** on weaker hardware. Favor batching, buffer reuse, caching, limited overdraw, short effect lifetimes and bounded emission over blanket removal of effects.
- Follow the existing particle-budget pattern: `CParticle::emitFire()` currently limits fire to roughly **30 emissions/second and eight live particles per lane**, without catch-up bursts. Do not accidentally reintroduce unbounded spawn loops or frame-rate-dependent particle counts.
- Keep notes, sustains, hit targets and HUD **legible** through lighting, fire, lightning and translucent geometry. Respect draw ordering, per-player cameras, depth testing and transparency.
- OpenGL is stateful. When adding effects, restore the shader program and any modified blend, depth, lighting, color or other relevant state. Prefer the approach used by `CParticle`/`CPlusLightning`; do not allow an effect to alter subsequent fretboard or HUD rendering.
- Keep OpenGL object creation/upload/destruction on a thread with the appropriate **current GL context**. CPU-side loading/decoding may be asynchronous, but do not turn worker threads into uncoordinated GL callers. Observe existing resource ownership and shutdown ordering.
- If changing a hot path, inspect the per-frame allocations, draw calls and complexity, and compare behavior at both high and low frame rates. Do not claim a performance improvement without measurements or a clearly explained reduction in work.

## Audio, scripts and assets

- Audio is integrated through **BASS/BASS_FX** and `CEngine`; gameplay synchronization depends on it. Avoid replacing audio-based timing with an unrelated wall clock.
- Lua is part of the game's configuration, events, menus, themes and extensibility. Preserve existing Lua interfaces and event hooks when changing the corresponding C++ code.
- Game assets are **not fully included in this repository**. Runtime paths refer to `data/models`, `data/sprites`, `data/songs`, themes and other assets; startup also looks for `Config.lua` in the working directory (a sample exists at `src/Config.lua`). Do not mistake missing runtime assets for an unrelated code failure.
- Preserve established asset file names and directory conventions unless a change deliberately updates all consumers. Avoid committing third-party music, models or other assets without appropriate redistribution rights.

## Build and validation

- The project uses **CMake and C++17**. Read `CMakeLists.txt` before adding source files: project `.cpp` files are listed explicitly, so new compilation units need a corresponding CMake update.
- Initialize dependencies with `git submodule update --init --recursive`.
- Linux builds require the OpenGL/GLFW/GLEW system dependencies and the external **BASS and BASS_FX** headers/libraries in `bass/` (see `bass/README.md` and the CI workflow for the current x86-64 download/setup procedure). GoogleTest is required when `BUILD_TESTING=ON`.
- Typical development commands, after dependencies are available:

  ```sh
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
  cmake --build build --parallel
  (cd build && ctest --output-on-failure)
  ```

- On headless Linux, graphical tests can use **Xvfb**; `tests/CMakeLists.txt` already wraps the GL/font tests with `xvfb-run` when available. CI runs on Ubuntu with Clang, Ninja, Mesa and Xvfb.
- The existing test suite covers shader ownership/lifetimes, GL state and effects, Plus lightning, particle limits and font rendering. Add focused tests for new behaviors. Rendering changes should be checked with the shader tests **and**, when assets/hardware are available, visually in-game.
- If a dependency or asset prevents a test, report exactly what could and could not be checked. Do not treat a successful build as proof that gameplay and performance were validated.
- The current CMake compiler flags include `-march=native` on GNU/Clang. Consider its impact when working on **portable distribution builds**; a binary built for the local CPU may not run on older machines.

## Working conventions

- Prefer the existing C++17 style, local interfaces and ownership patterns. Improve code you touch, but avoid unrelated bulk formatting, wholesale renames or third-party-code churn.
- Inspect callers, data flow and lifecycle before editing shared systems, especially `GPPGame`, `CEngine`, `CPlayer`, `CGamePlay`, rendering resources and Lua registrations.
- Make changes in small, reviewable units; preserve public and script-facing behavior unless the task explicitly requests a change. Add a regression test where a behavior is delicate or previously broken.
- Distinguish **observed behavior**, **intended game design** and **unverified assumptions** in your explanation. Ask when a design decision truly cannot be inferred; otherwise favor a compatible, documented implementation.
- Summarize what changed, which build/tests ran, what could not be validated, and any effects on older hardware or asset compatibility. Do not push, release or make unrelated repository changes unless requested.
