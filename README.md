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
sudo apt install extra-cmake-modules cmake build-essential gcc g++ libgl-dev libglew-dev

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
hooks, not success/failure or campaign reward notifications. Hook errors are
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

The legacy loaders are **not fully race-free**. This change removes a texture
callback holding a pointer to an expired stack variable, protects texture queue
inspection/publication with the consumer's mutex, retains each queued callback,
and waits for texture decoders before moving/destroying their objects. Decoders
no longer write the GL texture ID; failure is consumed through their future.
Missing optional completion callbacks no longer print `targetfun zero`.
Synchronous texture use consumes an existing decoder without replacing its
cache entry, so queued completion callbacks still run through the streamer.

Follow-up work should keep strict synchronization at loading boundaries without
adding a render/update-wide lock:

- Marathon still installs notes, audio handles and lyrics from its loader while
  the main loop can render player data. The existing `playersMutex` is not taken
  by that render path. Its atomic `processing`/`sendToModulePlayers` flags
  publish readiness, but rendering continues while the handoff is pending;
  checking readiness only before advancing does not protect those reads.
  Replace this with a prepared-song result consumed by the
  main thread between songs; preserve preload of the following song.
- Texture cache access is not uniformly synchronized (`gTextures`, `lasync`,
  image metadata and returned references). Some cached completion callbacks can
  execute on the requesting thread and under loader locks. Establish one owner
  for cache mutation/upload and dispatch callbacks outside queue locks on that
  owner. The batch API and font/menu direct cache access need the same audit.
- `forceTexturesToLoad` needs a predicate and notification protocol under its
  condition-variable mutex to prevent lost wakeups, including cancellation.
- The generic `CMultiThreadPool` is not independently safe: occupancy and object
  lifetime require an external lock spanning lookup/use/removal. Texture queue
  callers now use their common lock; model queue publication/teardown still
  needs an owner-thread contract or the same protection.

These are loading/lifetime debts, separate from the already known gameplay
update/render sharing. Passing build/GL tests does not establish race freedom;
concurrent loading/cancellation stress and sanitizer runs remain necessary for
a broader loader redesign. No FPS improvement is claimed by these changes.
