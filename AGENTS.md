# Guitar++ — Agent instructions

## Project identity and priorities

Guitar++ (Guitar-PlusPlus) is a Brazilian indie rhythm game written primarily in C++17. Understand the existing gameplay and rendering pipeline before changing it.

- **Plus** is the game's named special ability, conceptually similar to the special power in Guitar Hero: correctly playing a marked sequence of special notes earns Plus energy, which the player can activate for a temporary scoring boost. Use **Plus** in project terminology, code and UI. Do not casually rename it to a term borrowed from another game.
- The 3D note models have a distinctive shape that loosely resembles a **`+`**. Preserve that visual identity and note readability when altering models, rendering, or UI.
- The game is intended to run on **low-end/older computers** while still making the most of available hardware: 3D components, lighting, particles and other visual effects are intentional features. Optimize and scale their cost rather than removing visual richness by default.
- **Input responsiveness, musical timing and correct note judgment take precedence** over visual polish. A prettier effect is not a reason to introduce input latency, audio desynchronization or missed frames.
- Favor focused, maintainable improvements. Legacy names and patterns are not, on their own, a reason for a large unrelated rewrite.

## Risk tolerance and engineering pragmatism

Guitar++ is a **game**, not safety-critical infrastructure or a financial transaction system. Keep engineering effort proportional to the consequences of failure: responsive gameplay, enjoyable visuals, hardware compatibility and continued development matter more than eliminating every hypothetical risk at any cost.

- A rare, recoverable rendering glitch or game crash is usually an inconvenience; it does not justify sacrificing gameplay latency or undertaking a major architectural rewrite without evidence of a real problem. **Do** take loss or corruption of saves/charts, security issues in networking or scripting, and repeatable gameplay failures more seriously.
- Some legacy concurrency risks, including unsynchronized update/render state sharing, are **known technical debt**. Do not assume they are correct under the C++ memory model or introduce new races casually. Equally, do not replace the independent gameplay loop with a global lock or single-threaded redesign just to make a theoretical race disappear.
- Prefer **small, measurable, non-blocking improvements** when touching risky code. Explain what a change fixes, what remains risky, and its practical impact. If the user explicitly chooses to retain a bounded risk, respect that decision and document the tradeoff rather than repeatedly escalating the solution.
- Avoid overengineering: do not add heavyweight frameworks, elaborate abstractions, mandatory instrumentation or exhaustive test machinery unless they solve a concrete problem. Tests and safeguards should be commensurate with the feature and its likely failure modes.
- This pragmatic tolerance is not permission to conceal crashes, claim undefined behavior is safe, or knowingly corrupt user data. Be transparent about observable failures and meaningful risks.

## Code map: where to start looking

- `src/main.cpp`: startup, Lua configuration, menu wiring and top-level execution.
- `src/GPPGame.{h,cpp}`: overall game coordination, menus, themes, resource loading, gameplay modes and asynchronous texture/model loading.
- `src/CEngine.{h,cpp}`: window, OpenGL rendering, drawing buffers, timing and BASS audio integration.
- `src/CGamePlay.{h,cpp}`: gameplay update/render pipeline, note highway, timing, effects and Plus progression/activation.
- `src/CPlayer.{h,cpp}`: per-player state, chart notes, input state, note judgment, combo, score and Plus energy.
- `src/CChart.{h,cpp}`: Feedback-style chart parsing, BPM/offset conversion, instrument note data, Plus phrases and compiled chart handling.
- `src/CCharter.{h,cpp}`: chart editing; `src/CControls.{h,cpp}`: keyboard/joystick callbacks and input helpers (not yet a fully isolated input pipeline).
- `src/CGuitars.{h,cpp}` and `src/GPPOBJ.{h,cpp}`: instrument configuration and OBJ-based 3D models.
- `src/CShader.{h,cpp}`, `src/FireShader.h`, `src/PlusLightningShader.h`, `src/CPlusLightning.{h,cpp}` and `src/CParticle.{h,cpp}`: shaders, fire, lightning and particle effects.
- `src/CMenu.{h,cpp}`, `src/CFonts.{h,cpp}`, `src/CLuaH.*`, `src/CLuaFunctions.*`, `src/CLanguageManager.*`: menus, text, Lua scripting and translations.
- `src/CGameSettings.*`, `src/CCampaing.*`, `src/CMultiplayer.*`: preferences, campaign and networking. Preferences use cereal JSON in `data/saves/settings.json`; the obsolete raw-memory `CSaveSystem` was removed. Some existing identifiers have historical spelling; do not rename them as an incidental cleanup.
- `tests/`: GoogleTest-based tests. `CMakeLists.txt` and `.github/workflows/BuildGuitarPP.yml` are the build/CI references.

`glfw`, `glm` and `cereal` are Git submodules; `src/Lua/` and `src/utf8/` contain bundled third-party code. Avoid editing these when implementing game features unless explicitly necessary.

## Gameplay and chart invariants

- Preserve the relationship between **audio playback time**, chart offset/BPM conversion, note positions and judgment windows. Avoid logic that assumes a fixed frame rate; use the project's established clocks and delta-time mechanisms appropriately.
- Preserve distinct behaviors for chords, HOPOs, taps, open notes and sustained notes. In particular, `notesFlags` contains **bit flags**, including state bits; do not treat its values as a mutually exclusive enum or discard unrelated bits during updates.
- **Plus phrase data and the player's Plus state are different things.** `CChart`/`CPlayer::NotesData` store phrases (`gPlus`, `firstNote`, `lastNote`), while gameplay tracks progression and handles energy (`plusPower`), activation (`plusEnabled`) and reward effects (`bPlusStrike`, `CPlusLightning`). Understand all related paths before changing Plus logic.
- Changes to Plus detection must preserve the intended condition that the special sequence be played correctly, including its boundaries and misses. Test full sequences, interrupted sequences and transitions between consecutive phrases.
- Keep judgment, score, combo and Plus behavior consistent across player instances. Avoid leaking one player's state, camera, input or effects into another's.
- Chart parsing and serialized save/chart data are compatibility-sensitive. Do not change formats or saved field layouts without explicitly handling existing content and explaining any migration.

## Input, gameplay update and rendering threads (critical)

**Preserve the reason for the split:** gameplay/input processing should continue at a high, render-independent frequency even on slow GPUs. This separation was added to an older codebase and has known synchronization gaps; treat the design intent and the current implementation separately. Never assume a data race is safe merely because the game appears to run correctly.

### Current implementation: understand it before changing it

- `GPPGame::startModule()` starts `updateGamePLay()` in a worker thread. That loop runs `CGamePlay::update()` independently from the main thread's `CGamePlay::render()`. Its `sleep_for(200us)` is a scheduling hint, **not** a guaranteed fixed update rate.
- Gameplay input is currently read inside `CPlayer::update()` (called by the gameplay update worker), through `CEngine::getKey()` / `glfwGetKey()`. The existing `CControls` callbacks and joystick-query wrappers do not yet form a fully separated, thread-safe input pipeline.
- `CEngine::renderFrame()` currently calls `glfwSwapBuffers()` and then `glfwPollEvents()` on the main/render thread. Slow rendering or a blocked swap can therefore delay **event delivery**, even though the gameplay thread keeps updating. Moving `glfwPollEvents()` earlier within the same frame would not eliminate this coupling.
- **`GPPGame::playersMutex` has multiple existing roles.** `CGamePlay::update()` acquires it; `GPPGame::loadThread()` uses it around player reset/song loading; `GPPGame::loadMarathonThread()` uses it during marathon loading and handoff; legacy `_WIN32`-guarded `CMultiplayer::callbackFun()` paths acquire it when adding players or updating player readiness. It is **not** merely an attempted update-versus-render lock. Before modifying, shrinking, replacing or removing it, inspect **all** users, their lock scopes, player-container mutations and relevant game modes/platforms. Notably, `loadMarathonThread()` currently holds the mutex across its long-running loop; evaluate the existing lifecycle and interactions before changing that scope.
- **That mutex does not provide complete update/render synchronization.** `CGamePlay::render()` does not acquire it, while the update and render paths read and/or modify shared player/note state (e.g., note flags, Plus state, combo and timing). Some rendering functions also modify gameplay-related state. `pausegplay`/`continuegplay` are atomic, but they do not synchronize player data. Do not infer that `playersMutex` is useless because rendering does not lock it, or that shared gameplay state is safe merely because the mutex exists. Treat the remaining races as known risks without casually expanding the mutex across the render loop.

### Thread roles and intended direction

- **GLFW event owner (currently the main thread):** initialize/terminate GLFW; create/destroy windows; call `glfwPollEvents()`/`glfwWaitEvents*()`; handle keyboard/mouse callbacks, joystick hotplug callbacks and joystick/gamepad polling. The GLFW key, mouse-button, joystick and gamepad state query APIs used here are main-thread-only. Do **not** move those calls into the gameplay worker to reduce latency. Keep event callbacks short and non-blocking.
- **Gameplay/update worker:** consume an application-owned representation of input, evaluate timing and note judgment, and own authoritative mutable gameplay state (notes, combo, score, sustains and Plus progression). Its cadence and correctness should not depend on how often rendering finishes.
- **OpenGL/render owner (currently the main thread):** hold the current GL context, render models, notes, HUD and effects, and use an explicitly safe view of gameplay state. Prefer keeping render-only transient animation/buffer state local to the renderer; avoid changing authoritative note/score/Plus state in rendering routines.
- OpenGL rendering *can* be moved to another thread, but that is a separate, explicit architectural migration: detach the context from the original thread before making it current elsewhere, and audit GLEW initialization, asset uploads, GL object lifetimes, menus, callbacks and shutdown. GLFW event processing must remain on the main thread. Do not initiate this migration as an incidental input fix.

### Non-blocking communication and correctness

- Prefer **clear ownership and lightweight publication** over a broad mutex around `update()` and `render()`. Avoid blocking input/judgment on GPU work and avoid blocking an entire rendered frame on gameplay. Local, short synchronization is acceptable when it is actually required; correctness must be demonstrated, not inferred from low contention.
- For current pressed/released **levels**, a compact atomic bitmask or another properly synchronized snapshot may suffice. For edge-sensitive actions such as strumming, fast taps and HOPO transitions, preserve **ordered press/release events** (ideally timestamped) so two transitions between rendered frames are not silently lost. Consider a bounded single-producer/single-consumer queue if its ownership model fits; never assume SPSC when there are multiple writers/readers.
- Keep keyboard, joystick buttons, hats, analog axes and gamepad mappings distinguishable. Capture GLFW device state on its permitted thread and transfer **owned copies** to the gameplay thread; do not retain borrowed GLFW array pointers across callbacks, disconnects or threads. Account for focus loss, synthetic key releases, joystick disconnection/reconnection and overflowing event queues without leaving virtual frets stuck down.
- Use an explicitly shared monotonic time base for input timestamps and chart/judgment calculations. A callback timestamp measures when GLFW **delivered** an event, which may be later than physical actuation if event polling stalled. Do not claim sub-frame input accuracy solely from increasing the gameplay update rate.
- Plain concurrent reads/writes of ordinary C++ fields constitute data races and can cause undefined behavior; CPU-native atomic loads are not a C++ memory-model guarantee. If several fields need a consistent view (e.g., note status and Plus state), publish a **coherent** snapshot or use another correct synchronization protocol. Making one status flag atomic does not automatically make adjacent mutable data safe. Never implement a purported lock-free snapshot by concurrently copying non-atomic mutable objects.
- When modifying either thread, identify which thread **writes**, **reads** and **owns** every newly shared field. **Do not weaken existing synchronization with song/chart loading or multiplayer while addressing update/render races.** Preserve the independent gameplay loop unless the requested task explicitly changes this architecture; do not hide concurrency problems by moving all judgment into `render()` or by placing a large lock around both loops.

### Validation for changes in this area

- Exercise very low and high rendering FPS, delayed swaps, rapid press-release pairs within a rendered frame, chords, HOPOs, taps, sustains, missed notes, Plus phrases/activation, pause/resume, focus changes and joystick/gamepad unplug/replug where applicable.
- Measure event-delivery delay, event-to-judgment delay, update cadence, queue saturation and frame time separately. In particular, a continuously running update thread cannot consume **new GLFW input** until the main thread has processed the corresponding events.
- If a task is confined to visuals or unrelated gameplay, avoid broad threading redesign. If a task touches shared state, describe remaining known races and which ones were actually addressed; do not claim the whole subsystem is thread-safe based on a passing build or a single playtest.

## Rendering and performance

- **Maintain the existing OpenGL compatibility baseline unless a task explicitly changes it.** The engine requests an OpenGL **2.0** context and built-in effects use **GLSL 1.10**. Do not introduce core-profile-only APIs, recent GLSL syntax or mandatory newer GPU features without a compatible path.
- Effects should remain visually worthwhile on capable hardware while having **bounded, predictable cost** on weaker hardware. Favor batching, buffer reuse, caching, limited overdraw, short effect lifetimes and bounded emission over blanket removal of effects.
- Follow the existing particle-budget pattern: `CParticle::emitFire()` currently limits fire to roughly **30 emissions/second and eight live particles per lane**, without catch-up bursts. Do not accidentally reintroduce unbounded spawn loops or frame-rate-dependent particle counts.
- Keep notes, sustains, hit targets and HUD **legible** through lighting, fire, lightning and translucent geometry. Respect draw ordering, per-player cameras, depth testing and transparency.
- OpenGL is stateful. When adding effects, restore the shader program and any modified blend, depth, lighting, color or other relevant state. Prefer the approach used by `CParticle`/`CPlusLightning`; do not allow an effect to alter subsequent fretboard or HUD rendering.
- Keep OpenGL object creation/upload/destruction on a thread with the appropriate **current GL context**. CPU-side loading/decoding may be asynchronous, but do not turn worker threads into uncoordinated GL callers. Observe existing resource ownership and shutdown ordering.
- If changing a hot path, inspect the per-frame allocations, draw calls and complexity, and compare behavior at both high and low frame rates. Do not claim a performance improvement without measurements or a clearly explained reduction in work.

## Audio, scripts and assets

- Audio is integrated through **BASS/BASS_FX** and `CEngine`. Gameplay already uses more than one notion of time: a `std::chrono::steady_clock`-based song timeline (`startSongTime` / `getRunningMusicTime()`), mutable per-player `musicRunningTime`, and BASS playback positions that may be adjusted. Trace their relationship before changing synchronization; do not silently replace one clock or add an unsynchronized one.
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
- `GPP_NATIVE_CPU` enables `-march=native` / `-mtune=native` on GNU/Clang for local builds. It defaults to OFF and is explicitly OFF in CI. Keep it OFF for **portable distribution builds**; a binary built for the local CPU may not run on older machines.

## Working conventions

- Prefer the existing C++17 style, local interfaces and ownership patterns. Improve code you touch, but avoid unrelated bulk formatting, wholesale renames or third-party-code churn. Do not omit {}, keep a line break between functions and logical separations inside functions.
- Inspect callers, data flow and lifecycle before editing shared systems, especially `GPPGame`, `CEngine`, `CPlayer`, `CGamePlay`, rendering resources and Lua registrations.
- Make changes in small, reviewable units; preserve public and script-facing behavior unless the task explicitly requests a change. Add a regression test where a behavior is delicate or previously broken.
- Distinguish **observed behavior**, **intended game design** and **unverified assumptions** in your explanation. Ask when a design decision truly cannot be inferred; otherwise favor a compatible, documented implementation.
- Summarize what changed, which build/tests ran, what could not be validated, and any effects on older hardware or asset compatibility. Do not push, release or make unrelated repository changes unless requested.
- native_lab MCP (when available) to test sanitizers (ptrace) or sockets/loopback localhost

