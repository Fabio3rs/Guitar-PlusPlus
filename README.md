# Guitar-PlusPlus
A brazilian indie game

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

https://www.youtube.com/watch?v=p2zfNnjcJ2M

# Requirements
Bass, Bass_FX http://www.un4seen.com/

GLEW, OpenGL
GLFW (included as submodule)

# Compiling on linux
(instructions for Debian based, need search equivalent packages in other distros):
```
sudo apt update
sudo apt install xorg-dev
sudo apt install extra-cmake-modules cmake build-essential gcc g++ libgl-dev libglew-dev libsqlite3-dev

git clone  --recurse-submodules https://github.com/Fabio3rs/Guitar-PlusPlus/
cd Guitar-PlusPlus/
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE
make
```

Binary output directory: Guitar-PlusPlus/build/bin
Place the binary with the assets (not updated yet...) https://brmodstudio.forumeiros.com/t1590-guitar-topico-oficial

Put your song assets in the data/songs directory

## Quick Play library index

Quick Play recursively discovers songs under `data/songs` and caches metadata in
`data/cache/songlibrary.db`. It displays cached entries before scanning. **F5**
checks additions, modifications and removals; **Ctrl+F5** hashes and parses charts
again and measures main audio durations again. Use the latter if file contents
changed without changing their size or modification timestamp. Directory symlinks
are not followed during recursive discovery.

The index stores `.chart`/`.gpp`/MIDI metadata, extra `song.ini` fields, playable
parts, stems, artwork paths and BASS audio durations. Embedded explicit fields
precede INI fields; a MIDI sequence name is only a title fallback after the INI.
Audio stems and declared difficulty ratings do not imply playable chart parts.
Unavailable songs remain visible. Duration falls back to declared metadata or a
chart estimate (`~`) when audio inspection fails. Songs, settings and saves are
never rewritten by indexing. The cache can be rebuilt; it holds no personal data.

SQLite is a system dependency (`libsqlite3-dev` on Debian/Ubuntu). On Windows,
provide matching headers/libraries through `SQLite3_INCLUDE_DIR` and
`SQLite3_LIBRARY`, or a package-manager CMake toolchain. The adapted SQLite RAII
wrapper retains its MIT notice in `src/sqlite3/LICENSE`; xxHash 0.8.3 retains its
BSD notice in `vendor/xxhash/LICENSE`.

An optional inspection command is built with `-DBUILD_CHART_TOOLS=ON`:

```sh
build/bin/gpp_scan_library build/bin/data/songs /tmp/songlibrary.db > /tmp/songs.json
build/bin/gpp_scan_library build/bin/data/songs /tmp/songlibrary.db --full > /tmp/songs.json
```

It exports the indexed records to stdout and scan counters/timing to stderr.
The current menu remains a simple list; the full grouped Quick Play design is a
separate UI step.

## Configuration and preferences

[`src/Config.lua`](src/Config.lua) documents the existing window, keyboard,
gameplay, scene and legacy multiplayer options. Copy it to the game's working
directory as `Config.lua` when preparing a new installation; do not overwrite
an existing user's script during builds or updates. Paths still resolve from
the working directory, so launch from the directory containing `data/`.

Asset and script directory listings use C++17 `std::filesystem`. They are
non-recursive and retain the filesystem's unspecified enumeration order. Links
to regular files or directories are supported; dangling links and special files
are ignored. Directory access errors are logged instead of treated as successful
empty Lua loads. Scripts are selected by their final `.lua` extension, including
names such as `menu.extra.lua`.

Window preferences are loaded in this order: C++ defaults, then
`data/saves/settings.json`, then the values explicitly supplied by `Config.lua`.
The new settings file uses cereal JSON, version 1, with individual window fields
and the language identifier. It stores string contents rather than C++ object
memory, so `gameWindow` no longer depends on the layout of `std::string` or the
compiler's struct padding. Other Lua options remain in `Config.lua`; they are
not automatically persisted. The existing VSync overrides in menus/gameplay
remain in effect after window creation.

Preferences are saved on normal application exit. The complete archive is
written to a sibling `.tmp` file, closed and then replaces the previous file.
An invalid or unsupported settings file leaves defaults in memory and is not
overwritten; the log explains the error. Correct or remove that file to resume
saving. The former `data/saves/mains` raw-memory format is not read or migrated.
Campaign and chart formats are unchanged.


Song discovery now checks chart headers before adding entries to the song menus.
Text `.chart` files are checked through the end of their `[Song]` section;
invalid timing fields and incomplete headers are rejected and logged. Missing
optional metadata retains the existing defaults. Binary `.gpp` files have no
independent header, so inspecting their metadata requires deserializing the
archive. Recognizable legacy text `.gpp` files remain supported. This preflight
is not a substitute for validating the selected instrument and notes at load time.

Session loading rejects missing/empty instruments, invalid BPM/timing and unreadable
main audio. Optional instrument stems remain optional. Feedback charts without
BPM events retain the 120 BPM fallback. Failed chart loads clear partial/previous
note data; a valid chart with missing audio remains available to the editor, but
cannot start a gameplay session. The editor can still load audio to create an
empty chart. Load failures are logged and gameplay returns to its caller rather
than advancing past the loading phase. There is no campaign result contract yet.

Quick Play and the legacy marathon runners join their loading/update threads
before resetting player data, including failure and window-close exits. The new
failure flag is atomic and owned by the loader; the main thread consumes it.
The existing scope of `playersMutex` is unchanged. Remaining campaign-related
debt: natural completion/result delivery, sustain score/XP timing, Plus phrase
validation, marathon handoff/ownership and existing update/render/input races.
These changes do not make the entire gameplay subsystem thread-safe or validate
its latency. Runtime checks still needed with assets: loading failure in menus,
close during loading, repeated play/pause/exit, and marathon song transitions.

Missing `Config.lua` uses saved preferences/defaults. Syntax or execution errors
in an existing configuration abort startup with a diagnostic in `Guitar++.log`
and a nonzero exit status. Closing/cancelling loading does not enter partially
initialized menus or save preferences. Optional data-script errors are logged.

`CLog` serializes individual log records from loading threads, uses thread-safe
timestamp conversion and falls back to stderr when its file cannot be opened or
written. It remains a synchronous logger; no gameplay/render lock or logging
worker was added. On Windows, the existing crash tracer writes a separate
`Guitar++.crash.log` on a best-effort basis, avoiding the normal logger's mutex.

## CPU-specific builds

The default build and CI do not use `-march=native` or `-mtune=native`. For a
local-only optimized build, configure with `-DGPP_NATIVE_CPU=ON`. Keep it `OFF`
for distribution; this avoids requiring the build machine's CPU instruction
set, but does not bundle runtime libraries or guarantee compatibility with every
Linux distribution. Public asset packages exclude the contents of `data/songs/`;
the other game assets belong to the game team.

CI uses Clang 20 on Ubuntu 24.04, installed from the official `noble-updates`
`universe` packages (`clang-20`, `clang-tidy-20`, `clang-format-20`).

# Garage lighting and OBJ assets

The garage uses a shared OpenGL 2.0 / GLSL 1.10 renderer in menus and gameplay.
These optional settings belong before `setConfigs()` in `Config.lua`:

```lua
-- 0: original; 1: lighting; 2: 512px shadows; 3: 1024px shadows
setGameVar("backgroundQuality", 2)
setGameVar("backgroundAO", true)
setGameVar("backgroundShadowBias", 0.001)
```

`main()` loads `./Config.lua` relative to the working directory at startup.
Plain Lua globals do not update these registered game variables: use
`setGameVar` as above. The same calls from a running script take effect on the
next rendered frame, without recreating the window. Editing the file itself
is applied at the next launch; it is not watched for changes.

Disabling shaders also selects the original path. Shadows require
`EXT_framebuffer_object` and a complete depth framebuffer; otherwise the renderer
keeps per-pixel lighting. The garage light is fixed near its ceiling fixture.
Static shadow maps are reused across camera changes and invalidated on model
reload or shadow-resolution changes. The garage material overrides are confined
to its renderer; notes and other OBJ users retain their existing drawing path.

The CPU-only OBJ parser preserves object/group/material names, UVs (including
repeated coordinates), supplied normals, and MTL Ka/Kd/Ks/Ke/Ns/d/Tr/Ni/illum.
It handles relative indices and triangulates simple planar concave polygons.
Missing normals are generated with OBJ smoothing groups; missing UVs disable
texturing for the affected triangle instead of reusing the previous texture.
Degenerate/untriangulatable faces are logged and skipped; invalid indices reject
the new model without replacing a previously loaded one. Line/point primitives
are not rendered. MTL `map_Kd` supports paths with spaces, scale/offset and clamp;
without it, an existing `material-name.tga` retains the legacy convention.

Geometry remains in typed vectors. VBO owners cannot be copied; uploads use
separate typed arrays with checked byte sizes. Loading and replacement on worker
threads do not create or delete GL objects. Old buffers are retired on the next
render, and GL state is restored before drawing the highway or UI.

## Optional AO bake and scene preview

```sh
cmake -S . -B build -DBUILD_SCENE_TOOLS=ON
cmake --build build --parallel
build/bin/gpp_bake_ao build/bin/test/garage_gpp.obj
```

The offline baker writes `garage_gpp.obj.ao` (version 1, geometry/source signature,
atlas dimensions and independent UVs) and `garage_gpp.obj.ao.pgm` (8-bit P5 atlas).
It does not modify OBJ, MTL or diffuse textures. The loader rejects stale or
malformed sidecars and continues without AO. Regenerate both files after editing
the model. Missing sidecars are supported, including in existing installations.
Generated assets in `build/` are local and are not added to the repository.

The initial baker uses 32 cosine-weighted rays per texel, a 0.65 world-unit
radius, a BVH, triangle islands with padding, and bounded atlas dimensions. It
bakes geometric occlusion, not lighting or texture-alpha occlusion; inspect thin
or alpha-cutout surfaces before distributing the result. Garage oil decals and
the emissive fixture are excluded as occluders. AO affects ambient light only;
dynamic shadows use diffuse alpha cutouts. No bake work runs during gameplay.

`gpp_preview_background MODEL.obj OUTPUT.ppm QUALITY [FRAMES]` renders a hidden
960x540 window and reports synchronized frame times and shadow-map updates.
Use absolute asset/output paths and a scratch working directory: the existing
game texture manager initializes save/configuration services there. On headless
Linux run through `xvfb-run -a`; Mesa can exercise the baseline using
`LIBGL_ALWAYS_SOFTWARE=1 MESA_GL_VERSION_OVERRIDE=2.0 MESA_GLSL_VERSION_OVERRIDE=110`.
These timings describe the standalone garage, not full gameplay or older GPUs.

Tests are included in `ctest --test-dir build --output-on-failure`: CPU OBJ/AO
fixtures plus OpenGL shader, upload/ownership, state-restoration and shadow-cache
checks. Visual validation of gameplay and multiplayer is still required when
changing scene settings or assets.


## Scene definitions

The default remains the garage. `data/scenes/garage.lua` describes the existing
`test/garage_gpp.obj` and `.mtl`; if that definition file is absent, the same
paths/settings are built in for existing asset installations. All paths start
from the game's working directory. When running from `build/bin`, install custom
definitions in `build/bin/data/scenes` (the build does not overwrite user assets).

Select a scene from `Config.lua` or a campaign/menu Lua script:

```lua
setScene("garage") -- true for a valid identifier; files are checked at loading
-- Alternatively: setGameVar("backgroundScene", "garage")
-- getScene() returns the currently loaded identifier, or "" before loading.
```

Selection persists until changed and is applied at startup or the next Quick
Play/marathon loading boundary, never midway through a song. The existing model
continues to serve menus after a session. A failed replacement is logged, keeps
the previous scene, and prevents starting the requested gameplay session.
Invalid garage definitions are reported rather than silently ignored.

A scene file returns a plain Lua table; fields and hooks are read directly,
without invoking `__index` metamethods. Paths are relative to that file, so shared
models and scene-specific materials can live separately:

```lua
-- data/scenes/show.lua
return {
    model = "../models/stage.obj",
    materials = "show/stage.mtl", -- optional; overrides OBJ mtllib declarations
    lightPosition = {3.5, 2.88, -6.05}, -- optional; shader lighting position
    garageMaterials = false, -- default; true only for the garage's material fixes
    onLoaded = function(context) end,
    onSessionStarted = function(context) end,
    onSessionEnded = function(context) end,
    onUnloaded = function(context) end,
}
```

Texture map paths remain relative to their MTL. OBJ-adjacent AO sidecars continue
to work. Models currently use the existing world/camera coordinates; shader
shadows retain the downward-facing 150-degree, 0.1–20-unit light frustum. Scene
selection does not override the player's `backgroundQuality`, `backgroundAO`,
`backgroundShadowBias` or shader preference. Quality 0 retains the legacy fixed
lighting path. The garage's emission/specular/decal overrides do not affect
other scenes unless explicitly enabled.

Hooks receive `{sceneId, songId, songSeconds}` in a scene-owned Lua state with
standard Lua libraries. They run on the main/render thread, once per transition:
loaded after upload, session started at the countdown, session ended after the
session's workers have joined and before player reset, and unloaded before
replacement or normal shutdown. Pause/resume does not repeat start/end hooks.
A marathon is one session; its start supplies the first song and its end supplies
the last player's song clock, not accumulated show time. These are lifecycle
hooks. Per-song notifications are described below; campaign rewards remain a
controller responsibility. Hook errors are
logged without aborting gameplay. Each loaded scene keeps its Lua locals until
unloaded; no scene state or bytecode is saved in campaign progress.

There are no animations, Lua per-frame callbacks or new update/render queues.
Future animation inputs should extend the render-owned scene instance and use
the established song time, including pauses, without reading mutable player
state from Lua or driving judgment. CPU model decoding is asynchronous only
during loading; buffers, textures and retired model resources are handled with
the GL context current. Shadow caches are invalidated on scene replacement even
when the model address/revision is reused. Shared textures still use the existing
global cache; eviction across many different scenes remains future work.
## Loading thread audit and remaining work

Scene loading has a strict handoff: a decoder owns a temporary CPU model, a
`future` joins before the render owner consumes it, and GL upload/replacement
happens only on that owner. Cancellation also joins before destruction. Scene
Lua runs on the same owner at loading/session boundaries. It currently uses
standard Lua libraries and is **trusted local game code**, not a sandbox for
untrusted downloads; isolated Lua states do not provide a security boundary.

Texture loading now lives in `CTextureManager` / `CTextureResource`, behind
`GPPGame`'s compatibility facade in `GPPTexture.cpp`. The core has no GL, GLFW,
BASS, Lua or game-singleton dependency. Production decoding/upload/deletion are
injected by the engine adapter; tests use controlled CPU/GPU substitutes.

The call's completion contract is preserved, not inferred from its name:

| API | Completion contract |
| --- | --- |
| `loadTexture()` on the main/GL owner | Ready or failed before returning, including an already pending decoder. No frame/event pumping or unrelated callback dispatch. |
| `loadTextureBatchAsync()` on that owner | Blocking batch, including callbacks, despite the legacy name. Off-owner calls are rejected. |
| `loadTextureSingleAsync()` | A completed cache hit calls back immediately on the owner; pending loads and off-owner callbacks are dispatched by the owner. |
| `loadTexture()` off-owner | Requests CPU work and returns a stable view; it does not promise a GPU-ready result. |

`Queued`, `Decoding`, `ReadyForUpload`, `Ready`, `Failed` and `Cancelled` describe
the resource separately from its GPU ID. Decoder futures return owned pixels;
workers never capture movable resources or write GL IDs. Short locks protect
cache/queue operations and coherent resource snapshots. Upload, future waits
and callbacks run outside those locks. Synchronous use joins the same decode
and preserves queued callbacks. Each accepted callback runs once during normal
operation; a throwing callback is reported without abandoning the others.
The facade alone retains legacy `0` / `~0u` failure conventions: synchronous
failures can be retried, while asynchronous failures remain cached.

The cache key remains `path + "/" + texture`. There is no eviction yet. Fonts
request `keepPixels` per texture instead of changing a global setting; an atlas
already uploaded without retained pixels uses the existing CPU reread fallback,
without another GPU upload. Pixel views own their data and are read-only. Legacy
callback batch pointers are valid only during the callback; `void*` user data
and Lua associations remain confined to the facade.

The queue still accepts at most 200 outstanding asynchronous requests and pumps
50 completions per normal call. Saturation rejects new requests explicitly;
synchronous loading is not subject to that queue limit. `std::async` remains
encapsulated for this step: these limits are **not a bounded worker executor**.
Worker-side draining waits on a predicate under the notification mutex; the GL
owner must continue pumping. Shutdown wakes waiters, joins CPU decoders,
discards callbacks and deletes textures while the context is still current.
`main` uses a scope guard covering normal exit, cancelled startup and exceptions.
The core destructor only joins CPU work: owners must call `shutdown()` before
losing their GL context. `initializeTextures()` starts a new cache lifetime only
at a lifecycle boundary, without concurrent callers or surviving legacy refs.

Remaining work is deliberately separate:

- `CMultiThreadPool` is no longer used by either asset loader. Its old occupancy
  and lifetime limitations have not been repaired for hypothetical new users.
- A bounded executor, upload time/byte budgets, priorities, texture-request
  cancellation and eviction remain future work. Gameplay update/render sharing
  is unchanged; no whole-game race-freedom or FPS improvement is claimed.

### Model loading

`gpp_models` is a CPU-only library containing OBJ/MTL/AO loading and
`CModelManager`. Workers own their request and return immutable `CModelData`;
there are no game, Lua or GL calls in the decoder. Optional missing MTL files and
invalid AO sidecars retain the existing fallback behavior. AO baking remains
an offline tool operation.

Requests with identical OBJ path, object filter and explicit MTL strings share
in-flight CPU work. Paths retain their filesystem meaning, including `..` after
symlinks; differently spelled aliases are not automatically deduplicated. Each request has its own publication and state:
`Decoding`, `ReadyForUpload`, `WaitingForTextures`, `Ready`, `Failed`, or
`Cancelled`. CPU-only completion stops at `ReadyForUpload`. There is no persistent
GPU model cache: each `GPPOBJ` still owns its buffers/atlas exclusively.

| API | Completion contract |
| --- | --- |
| `GPPOBJ::load()` / `loadInternalObj()` | Synchronous CPU load, preserving deferred GL upload. Failure preserves existing CPU data. |
| `GPPGame::loadModelSync()` | Owner-only; returns true only after GPU publication. Joins a matching pending decoder without pumping unrelated callbacks or frames. |
| `requestModel()` | Owner-only target registration; returns a handle, or null on rejection. Poll its snapshot for actual success/failure. |
| `loadSingleModelAsync()` / `loadModelBatchAsync()` | Return request acceptance, not loading success. A batch exposes a handle per entry; rejection can be partial. |
| `CGuitar::load()` | Remains synchronous, with CPU preparation before the Lua load hook, then GPU preparation before return. |
| `CScene::load()` | Still returns only after transactional replacement or failure/cancellation, optionally drawing loading frames while waiting. |

`CModelManager::requestAsync()` itself supports concurrent CPU requests. Its
`finish`, `pump`, `cancel`, target publication and `shutdown` belong to its owner.
A publisher returns false for an abandoned target and throws on upload failure.
Publications run outside manager/resource locks; a synchronous completion does
not invoke other requests' publishers. Managers and targets must not be destroyed
or reinitialized concurrently with their callers. Owner callbacks may enqueue
other requests; they must not destroy the manager currently executing them.

The game adapter requests material textures through `CTextureManager`; async
publication waits for terminal dependencies without blocking, while sync use
completes the needed textures. Failed textures use the existing neutral fallback,
never the legacy `~0u` GL sentinel. GPU upload builds a temporary model; the old
model survives decode/upload failures. Weak targets prevent publication into a
destroyed object. Moves transfer the target identity; unload/new publication
invalidates older generations. Target registration, moves, unload and GPU use
must remain on the owner thread.

Startup now waits for the five basic gameplay models to be GPU-ready, including
the default guitar fallback. A required model/initial-scene failure aborts startup
with a diagnostic rather than silently drawing missing geometry. Guitar failures
at gameplay entry are logged and retain the renderer's existing fallback.
Scene swaps preserve Lua hook order and the active scene on failure. The main
scope guard joins model work and retires basic/scene GPU resources before texture
shutdown; instruments are also unloaded while the context exists.

Cancellation abandons a request, not its shared decoder. Running CPU work is
retained until it finishes or shutdown joins it. The current implementation still
uses `std::async`, with up to 200 pending requests/jobs and two model completions
per normal pump; these are not a fixed worker count or a hard frame-time budget.
`GPPGame::renderFrame()` pumps both loaders. Direct `CEngine::renderFrame()` calls
do not; no consumer should rely on those loops to complete a new model request.

`gpp_model_tests` links only the CPU library, threads and GoogleTest. Run it with
`ctest --test-dir build -R '^model_core\.' --output-on-failure`.
Model facade/GL regressions are in `Scenes.Model*` and `SceneGL.Model*`; existing
scene/guitar lifecycle and shader cases exercise the synchronous consumers.
For ASan/UBSan include `model_core`, `ObjLoader`, `Scenes`, `SceneGL`, and `ShaderGL`;
for TSan use `model_core` and `Scenes.Model` without a GL context. Use NativeLab
and the same compiler/Mesa setup documented below for texture validation.

### Texture loader validation

`gpp_texture_tests` links only the isolated core, GoogleTest and threads. Its
controlled decoders cover deduplication, synchronous takeover, callbacks,
saturation, retained pixels and shutdown. Existing game-linked tests cover the
facade, real TGA files, fonts, scenes and GL object retirement without game startup.

```sh
cmake --build build --target gpp_texture_tests tests GuitarPP --parallel
ctest --test-dir build --output-on-failure --no-tests=error --timeout 30
```

For sanitizers use separate Debug build directories, Clang and
`GPP_NATIVE_CPU=OFF`. Apply `-fsanitize=address,undefined
-fno-omit-frame-pointer -O1` to both C/C++ compilation and the sanitizer flags to
executable linking; use `-fsanitize=thread` in a separate build for TSan. Execute
configuration, compilation (including test discovery) and tests through
NativeLab MCP when available. The CPU/facade selection is
`^(texture_core\.|Scenes\.(Texture|EmptyTexture)|FontCPU\.)`; GL/font/shader cases
run through the existing Xvfb launcher with OpenGL 2.0/GLSL 1.10 for scene/shader
cases. Keep leak detection enabled for CPU tests and distinguish external
Mesa/GLX allocations from loader diagnostics rather than suppressing loader errors.
On the current NativeLab host, `/opt/amdgpu` GLX reports a repeatable 108124-byte
leak even in existing shader tests. Selecting system Mesa with
`LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu` allows the GL tests to run with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`; no sanitizer suppression is needed.

## Song boundaries, timing and results

`onSongStarted(context)` and `onSongEnded(context)` complement the scene/session
hooks. A marathon starts/ends its session once, but each song has its own events.
A start is the start of that song's countdown, not audio onset. Pause/resume does
not emit another start. End receives `songId`, `songSeconds`, `reason`
(`Completed`, `Aborted`, `LoadFailed`) and a 1-based `players` array containing
`score`, `maxCombo`, `correctNotes`, `totalNotes`. A load failure may have no
preceding start and no player statistics. Cancelling initial loading records an
aborted result without a song-start/end hook pair. Script errors remain logged;
Lua receives copies, not access to the gameplay worker or authoritative results.

C++ controllers can copy `GPPGame::getSongResults()` after the mode returns.
Results are collected before reset/replacement and cleared for the next mode
attempt. Scores are per-song deltas; marathon still retains its cumulative score,
combo and Plus state. `maxCombo` is the largest combo reached on a hit during the
song, so it may include a streak carried from the preceding song. Completion
uses three seconds after the maximum `time + lTime` across the loaded notes,
not merely the note with the latest start. This boundary is cached during chart
validation and moves with `NotesData`; checking completion does not scan notes
in the update loop. It is not the audio-file duration or a narrative success
objective.
Quick Play now returns naturally at that boundary; finishing the last marathon
song exits instead of requesting a nonexistent next song.

Marathon decoding now uses individually owned temporary players. The worker
publishes prepared data with `processing=false`; only the main thread installs
notes, lyrics, metadata and transferred audio handles between songs, before
requesting the next preload. `NotesData` explicitly moves its contiguous buffers;
`CPlayer` itself is not moved/copied in this loader. Existing multiplayer
exclusion via `playersMutex` is retained for the mode, with the main thread now
holding it instead of the loader. This does not fix the separate legacy Quick
Play update/render sharing or multiplayer rendering risks.

Gameplay delta is measured on its updating thread. Resetting its active clock at
loading/resume/song boundaries makes the first sample zero, excluding idle time.
Quick Play acknowledges each pause before the main thread adjusts the shared song
timeline, and restarts its update clock on resume. The Quick Play pause handshake
and audio pause run before another clear/swap; resume restarts audio and releases
the worker before rendering again. Timeline/countdown compensation uses one
monotonic duration. Completion is checked inside the existing update player lock.
Marathon resets its own clock
on resume and song installation. Both shift the song timeline across a pause.
Sustains use the update's active overlap with their judged duration, retaining
the existing 130 ms release tolerance, and award 50 points times multiplier per
second, once for simultaneous held lanes. There is no render delta in that award.

XP is provisional and currently has little practical use. Its progression,
balance and persistence architecture are **not decided**. Sustain XP temporarily
uses 0.5 times multiplier per active second solely to remove tick-rate dependence;
this is not a finalized campaign reward policy. Single-note XP remains unchanged.

Regression tests cover buffer transfer/copy support, audio ownership, pause-clock
reset, sustain rates at 1/2/5/33 ms with Plus on/off, stable result values after
reset, multiple song callbacks within one scene session, and real worker
publication/handoff with two players, repeated songs, missing audio/chart and an
empty playlist. Full interactive
validation is still needed for rapid pause/resume, two-player marathon transitions,
input edge cases, real audio synchronization and low/high FPS; these tests do not
establish race freedom for the whole gameplay subsystem.

### Guitar resource lifecycle

`CGuitars` remains a shared installed catalogue, separate from the chart's
`CPlayer::instrument`. Discovery and Lua configuration run before gameplay.
`addGuitar` returns a pointer, or `nullptr` for a missing/invalid script, failed
`addGuitar` callback or empty model name. A temporary map node lets legacy
`changeGuitarData(id, ...)` configure the candidate without publishing it; insertion
preserves the Lua script object's address. During registration, changes to other
guitars are rejected. Registering an existing ID preserves its object and cache.
`getGuitar` throws for an unknown ID; `getGuitarIfExists` returns null. Neither
lookup inserts an entry.

`CGuitar::load()` returns success only after OBJ decoding, the `loadGuitar` hook
and GL preparation. Calls require the GL owner during loading. Failures leave the
guitar unavailable and retryable; repeated successful loads reuse the resource.
Rendering uses the standard bar and fretboard when the selected guitar is not
loaded. Legacy numeric texture handles remain supported; invalid nonzero handles
are replaced with zero during loading. Textures belong to the existing cache or
scripts, not to the guitar model, so they are not blindly deleted by this class.

Unload invokes `unloadingGuitar` once per successful load, including catalogue
shutdown, then releases the model. Stop all consumers before clearing the
catalogue and retain the current GL context through cleanup. Player pointers stay
valid between songs; do not clear the catalogue or mutate Lua guitar settings
concurrently with gameplay. This is an ownership contract, not a thread-safe
hot-reload API. Lua filesystem/global side effects cannot be rolled back when a
script fails. Missing models can remain discoverable and be retried later.

Scene tests cover invalid scripts/callbacks, non-inserting lookup, OBJ failure and
retry, repeated load/unload, retained cache and manager shutdown events. Real
asset appearance and gameplay frame times still need interactive validation.
Future work: structured asset-path definitions, read-only menu catalogue and
campaign inventory using stable IDs; preserve `GuitarScript.lua` compatibility.

Review regression tests also cover a sustain that ends after the last-starting
note, preservation of that boundary through marathon transfer, all-player
completion, and repeated pause compensation of timeline/countdown clocks. These
clock tests do not simulate a real blocked swap or verify audible pause/resume;
interactive validation of those transitions is still required.

### Bitmap fonts and menus

The UI remains C++17 and OpenGL 2.0. The default `FONT.tga` is a 32-column,
8-row fixed-cell atlas (2048×512, or 1024×256 for the low-resolution variant).
The built-in registration contains the full character map, including accents,
and does not require the example metrics script. The versioned default theme
is in `data/themes/gppdefaulttheme/Theme.lua`; installed themes can still override
it. Cells already contain descenders: default glyph height/offset remain `1, 0`.
The declared default baseline is 14/64 above the cell bottom, with ascent 50/64
and descent 14/64. This is a font convention, not an alpha-derived baseline.

Lua registration supports both signatures:

```lua
addTextureToFont(name, path, texture, characters) -- legacy row description
addTextureToFont(name, path, texture, characters, columns, rows)
setFontCharMetrics(name, characters, height, verticalOffset)
setFontCharSpacing(name, characters, advance, horizontalBearing)
setFontMetrics(name, baseline, ascent, descent)
```

All metric values are in nominal text-size units. Positive vertical offset raises
an entire cell; it does not crop or invent a descender. `setFontCharSpacing`
sets pen advance and the cell's horizontal position relative to that pen.
The old four-argument registration retains empty rows, including a final empty
row, for existing themes. It also counts the final nonempty row's columns.
Use an explicit grid for partial/unused regions and rectangular cells; a final
newline then has no effect on physical geometry. Invalid registrations return
false to Lua (an empty path in C++) without publishing a partial font. Repeating
an unchanged registration reuses alpha analysis, restores glyph overrides and
avoids invalidating layouts when nothing changed. Duplicate characters use the
last registered cell. Replacing an atlas removes its old mappings, but preserves
characters subsequently supplied by another atlas. RGB atlases are opaque;
RGBA atlases use their alpha channel. Unsupported/malformed image data fails.

`layoutText` exposes glyph positions, logical advance, visual bounds and caret
positions without OpenGL. Measurements, centering, direct/buffered/3D text and
menu hit testing share that calculation. Unknown fonts fall back to `default`;
missing glyphs use `?`, or a blank advance if unavailable. Invalid display bytes
use the same fallback. Queries never insert registry entries. Text layout is
single-line: CR/LF have zero advance, tabs default to four spaces. No shaping,
normalization, bidirectional layout or grapheme-aware cursor movement is supplied.

UTF-8 editing indices count Unicode scalar values, while `preTextMaxSize` remains
a **byte budget** for compatibility. A multibyte insertion must fit entirely.
Invalid size/insertion returns `std::string::npos`; invalid removals leave the
string unchanged. Empty strings and out-of-range removals are harmless. Edit
fields retain their layout until the text/font changes and scroll horizontally
to keep the caret visible. Composed and decomposed accents remain different
sequences; combining characters can still be edited independently.

Menus have separate keyboard focus, hover and mouse capture. A click activates
on release over its original target; Enter activates on press, without key-repeat
activation. Arrows/Tab navigate, Home/End/Delete/Backspace edit text, and Escape
leaves editing or activates a back option. Static/disabled options are skipped.
The legacy status bits (`1` selected, `2` activation pulse, `4` editing) remain an
adapter for callbacks. Option Lua callbacks receive `(menuName, optionName,
optValue, action)` once per update, including idle/selection notifications;
activation is a one-update pulse. Pre/post menu update hooks are restored.

`newMenu(name)` now honors a nonempty name; empty names are generated. Repeated
creation of an existing name preserves the object and options. `findMenu` returns
null for a missing menu; `getMenuByName` throws instead of creating a phantom
menu. Lua queries/assignments reject missing menus and invalid indices. Navigation
resolves actual menus separately from game callbacks. Structural navigation and
interface closure happen outside callback traversal; callbacks can append options
without invalidating the active iteration. Newly appended options update on the
next pass. External C++ callbacks must not destroy the menu currently executing.

`CMenu::Style` shares spacing and selection/control colors; optional selection
animation defaults off. `setFlow` supports vertical/horizontal rows alongside
manual placement. Lua-created options keep manual positions. List viewports clip
rendering and interaction; selection reveals offscreen rows. Cached menu text is
batched in consecutive texture runs. Deferred text preserves submission order
and captures each call's color rather than sorting all glyphs by texture ID.
UI rendering restores matrices, scissor, blend, depth and color state.

Profile/rename dialogs are modal. Other interfaces can set `modal=false` and
receive focus by clicking their exposed region. Only one menu receives an event.
Interface copies reset live children, focus/capture and external edit bindings;
known bindings are installed explicitly after opening. Their referenced objects
must outlive the interface. Closing a parent closes its owned interfaces.
Named menus stay in the existing map and overlays retain unique ownership.
Lua callback references remain script-owned; replacement releases the previous
registry reference. This is not a general lifetime manager for arbitrary C++
lambda captures.

GLFW UI callbacks queue key/text/mouse/focus/scroll events on the main thread.
The bounded queue cancels capture on overflow. Device polling and all GL/UI
resource changes also remain on that thread. Focus loss/disconnection releases
menu state; a held gamepad confirmation must be released across menu sessions.
Standard gamepads use D-pad/left stick, A to confirm and B to go back. Axis
thresholds use hysteresis (0.55 on, 0.35 off); navigation repeats after 350 ms,
then every 75 ms without catch-up bursts. Raw guitar/joystick bindings are
optional Lua configuration, independent of gameplay mappings:

```lua
-- GLFW device GUID; indices are zero-based. Hat direction uses GLFW hat bits.
setMenuBinding("DEVICE_GUID", "button", 0, 1, "confirm")
setMenuBinding("DEVICE_GUID", "button", 1, 1, "back")
setMenuBinding("DEVICE_GUID", "hat", 0, 1, "up")
setMenuBinding("DEVICE_GUID", "hat", 0, 4, "down")
setMenuBinding("DEVICE_GUID", "axis", 1, -1, "up")
```

Bindings replace the same device/action entry. Actions are `up`, `down`, `left`,
`right`, `confirm`, `back`; sources are `button`, `hat`, `axis`. Axis direction
is -1/+1. A controls-configuration screen and persistent bindings in settings
are deliberately not part of this change. Gameplay clocks, judgment, input
worker and the known update/render sharing risks are unchanged. Faster UI event
handling cannot eliminate delivery delays caused by a blocked swap.

Configure `-DBUILD_UI_TOOLS=ON` for `gpp_benchmark_ui`; run it from `build/bin`,
optionally passing width and height. It reports median/p95/p99 measurement and
menu submission times. It warms caches, and synchronizes only at benchmark
boundaries (not in production UI rendering). These are submission measurements,
not whole-game frame times or monitor/VSync latency.

A local before/after comparison on Ryzen 9 5900X with Mesa software rendering,
RelWithDebInfo and portable CPU flags produced these microsecond results:

| Resolution / workload | Before median / p95 / p99 | After median / p95 / p99 |
|---|---:|---:|
| 800×600, 5-option menu | 58 / 98 / 154 | 49 / 88 / 127 |
| 800×600, 1,000-option list | 4,591 / 5,471 / 5,942 | 143 / 179 / 192 |
| 1920×1080, 5-option menu | 69 / 95 / 105 | 59 / 88 / 100 |
| 1920×1080, 1,000-option list | 4,903 / 5,831 / 6,224 | 169 / 206 / 219 |
| 800×600, 1,000 text measurements | 347 / 360 / 462 | 216 / 275 / 364 |

The long-list comparison includes the new viewport: the old renderer submitted
all rows even outside the screen. Initial font setup was roughly 8–9 ms in this
single-run sample, including loading/upload; this is not a startup percentile
claim. Hardware gamepad/guitar reconnects, actual modest GPUs, full-scene frame
distributions, monitor VSync and audible gameplay timing still require interactive
hardware validation. Software rendering on a fast CPU does not establish those
results. Test rendering remains compatible with GL 2.0 / GLSL 1.10.

CPU tests can run under ASan/UBSan without a display, including through NativeLab.
For graphical sanitizer runs, this machine's Mesa GLX retains allocations after
context teardown. Use `LSAN_OPTIONS=suppressions=/absolute/path/to/tests/lsan.supp`
with `ctest -L gpu --output-on-failure` in the
sanitized build. The suppression matches only `libGLX_mesa.so`; leak detection
remains enabled. CPU sanitizer tests should run without this suppression.

Validation for this change: all 12 CTest groups passed; UI CPU tests passed with
ASan/UBSan/LeakSanitizer in NativeLab, and font/shader/scene GL tests passed with
the Mesa-only suppression. The real `build/bin` assets were used to inspect the
main menu, play submenu, profile modal and resized window. Pause styling uses
the same flow rules, but an audible in-song pause/resume playtest is still pending.
Continuous typing, dynamic HUD updates, theme/language transitions and repeated
modal lifetimes have regression coverage or implementation safeguards; their
allocation peaks and p95/p99 CPU/frame costs have not yet been benchmarked.

### Chart loader migration evidence

The runtime chart loaders and `NotesData` now live in `CChart`. Each `CPlayer`
still owns its mutable note buffers and playback cursors through the compatibility
alias `CPlayer::NotesData = CChart::NotesData`. Existing `Note_t`, `NoteInt_t`,
`plusNote_t` and `BPM_t` names remain available. The chart discovery method on
`CPlayer` forwards to `CChart`; audio loading and gameplay remain in `CPlayer`.

This first step is a relocation, not a change to parsing rules. The older,
separate parser used by `CChart::open()`/`openFromMemory()` is still present and
has no calls from game runtime. It must not replace the runtime loader yet:
its HOPO threshold, Plus boundaries, supported sections, validation and reload
behavior differ. The moved runtime loader still accepts a `CPlayer` for metadata;
decoupling that interface and unifying the two parsers are subsequent steps.
Existing binary field types, flag values and cereal field order are unchanged.

To capture a baseline **before editing the loader**, build the optional CPU-only
dump tool, then capture complete local songs:

```sh
cmake -S . -B build -DBUILD_CHART_TOOLS=ON
cmake --build build --target gpp_dump_chart
python3 tools/chart_evidence.py capture build/bin/gpp_dump_chart build/chart-baseline build/bin/data/songs
```

After rebuilding the modified loader, compare against that same directory:

```sh
python3 tools/chart_evidence.py compare build/bin/gpp_dump_chart build/chart-baseline
```

Capture refuses to overwrite an existing evidence directory. The manifest hashes
inputs, the executable and dumps; comparison checks that inputs and baseline have
not changed. Feedback files are exercised for each discovered Easy/Medium/Hard/
Expert section; `.gpp` inputs use ExpertSingle. Additional sections can be tested
with `gpp_dump_chart feedback|compiled INPUT INSTRUMENT OUTPUT`.

The dump records the load result, song metadata, instrument and every logical
field of `NotesData`, including every note/BPM/Plus entry, playback cursors,
per-fret times, resolution and end caches. It also exercises the end-time query,
reload and unload. Doubles are recorded as exact 64-bit representations rather
than rounded decimal values. Container capacity, addresses and struct padding
are intentionally excluded. This verifies behavior for the captured corpus;
it does not prove correctness for every possible chart or validate live gameplay.

The initial local migration evidence is in `build/chart-loader-evidence/`:
14 complete song files across 54 instrument/difficulty cases, seven synthetic
Feedback cases, one legacy text `.gpp` and one binary `.gpp` generated before
migration. All 63 before/after dumps matched exactly (61 successful loads and two
expected rejected cases). Synthetic cases include HOPO boundaries, BPM changes,
sustains, offsets, chords, taps, open notes, Plus boundaries, defaults and invalid
bodies. The original executable and SHA-256 manifest are retained there. Dumps
and compiled real-song fixtures stay outside version control because they contain
third-party chart data. They are local evidence, not bundled CI fixtures.

`ctest --test-dir build -R '^ChartRuntime\.' --output-on-failure` runs focused
CPU-only runtime-loader contract tests using small synthetic sections. Expected
values come from the chart's resolution, tempo and offset, independently of the
historical dumps. Coverage includes UTF-8 metadata, quarter-note timing at several
resolutions/tempos, signed offsets, tempo transitions, sustains crossing changes,
instrument/player isolation, and Plus notes strictly inside a single phrase.

Still open: the automatic HOPO threshold, forced-marker ordering, chord sustain
merging, open-note sustains, Plus boundary tolerance and transitions between
consecutive/empty phrases, and offset handling for the initial BPM entry. These
are not asserted as correct merely because the old loader produced them. The
before/after dumps continue to detect changes to those behaviors without defining
their intended semantics. The focused suite exercises the runtime loader, not the
older parser behind `CChart::open()`.

GoogleTest cases share the `tests` executable and are individually registered by
`gtest_discover_tests` (CMake 3.16 or newer, matching CI). New test sources are
automatically picked up on the next build. Use `ctest --test-dir build -N` to list
cases, `-L cpu` for tests without a graphics context, `-L gpu` for graphics tests,
or `-R '^SongLoading\.'` to select a suite. On headless Linux, GL cases use Xvfb
when available. They retain their Mesa environment and run serially with respect
to other graphics cases because some fixtures share temporary assets. CPU cases
can run in parallel. `startup_errors` remains a CMake integration test that
launches the actual game, rather than a GoogleTest case.
