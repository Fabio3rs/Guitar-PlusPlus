# Guitar++ Scene Studio

Independent development tool using `GuitarPP_lib`, the game's OBJ/MTL/AO loader,
`CBackgroundRenderer` and GLSL 1.10 shaders. The default game build does not
compile/link ImGui, PNG capture, EGL backend or RPC code. No gameplay loop,
audio, game menu, personal settings or global game Lua initialization is started.

## Build and desktop

Requires the existing game dependencies, CMake >= 3.25, and libpng development
files. On Linux, EGL development files enable the optional EGL backend. Tests
also use Python 3, GoogleTest and Xvfb; mouse tests run when xdotool is present.
Nothing is downloaded by the Studio build. Use the already pinned
`vendor/imgui` submodule (commit `778992face67ecefaa5c2932db4b2101545ea4df`).
The third-party ImGui target is included through `add_subdirectory(... SYSTEM)`.

Run from the workspace root:

```sh
cmake -S . -B build -G Ninja -DBUILD_SCENE_STUDIO=ON -DBUILD_TESTING=ON -DGPP_NATIVE_CPU=OFF
cmake --build build --parallel
./build/bin/gpp_scene_studio
```

The default source searches the asset root and the known `build/bin` runtime
for the garage. If absent, the original synthetic fixture is used. An explicitly
requested invalid source fails rather than silently substituting a scene.
`--scene synthetic` always selects the fixture. `--asset-root build/bin --scene
garage` selects the local garage explicitly. Garage assets are not redistributed.
The default garage camera looks inside; Frame object fits its complete bounds.

Mouse gestures begin over the viewport: left drag orbits, middle drag pans and
wheel zooms. Panels handle their own input. Frame object and Reset camera are
available above the viewport. Scene Explorer accepts an ID, Lua file or OBJ;
Inspector shows read-only parts/materials. Lighting and Shadows/AO use the same
controller operations as CLI/RPC. Docking uses one native window; multi-viewport
windows are disabled. The layout is saved to the output directory's `layout.ini`.

The repository's pre-release GLFW identifies as 3.4 but lacks `glfwGetPlatform`.
A tool-local translation unit selects ImGui's supported GLFW 3.3 feature set;
neither submodule is patched or replaced. The OpenGL3 backend uses `#version
110` and its own isolated loader; shared renderer code continues to use GLEW.

## CLI and captures

```sh
./build/bin/gpp_scene_studio --help
./build/bin/gpp_scene_studio --mode headless --scene synthetic \
  --light -2 4 1 --quality 3 --ao on --shadow-bias 0.001 \
  --eye 5 4 6 --target 0 0.5 0 --up 0 1 0 --fov 45 --time 0 \
  --width 960 --height 540 --capture example.png --json
```

Without a display, use the NativeLab commands below. `--scene ID`,
`--scene-file FILE` and `--model FILE` are mutually exclusive. Definition paths
are relative to the Lua file; MTL/texture paths retain the game conventions.
OBJ material parameters/transforms are inspection-only. Quality 0 preserves
original drawing, 1 enables lighting, 2 uses 512 shadows, and 3 uses 1024 shadows.
Bias accepts 0..0.02, FOV 5..120 degrees; coordinates must be finite and within
+/-1,000,000. Eye/target/up must form a valid view. Dimensions are bounded by
hardware, 8192 per axis and 32M total pixels. The current renderer is static;
`--time` is recorded for reproducibility and does not invent animation hooks.

`--output-dir` defaults to `build/scene-studio`. Artifact names are relative to
that directory; absolute paths must resolve inside it. PNG capture also writes
`NAME.png.json`, or the name supplied with `--metadata`. Metadata includes source,
camera, lighting, requested/effective effects, AO status, dimensions, backend,
vendor/renderer/version and hardware/software/unknown classification. A requested
but unavailable effect is identified by `ao_reason`/`shadow_reason`; it is not
reported as active. Inspect shader/asset messages on stderr as well.

Captures are clean viewport images and can differ in resolution from the GUI.
`--mode gui --frames 30 --ui-capture editor.png --json` captures the complete UI
and exits. Its metadata identifies `image_kind=full_ui`. Normal rendering does
not read back pixels or wait for GPU completion. Capture readback synchronizes
only when requested. PNG/metadata failures return nonzero status; capture output
uses temporary files before rename (the two-file pair is not a filesystem transaction).

`--json` reserves stdout for a JSON result, including structured startup errors;
diagnostics go to stderr. Without it, stdout reports completion. The CLI exits
0 for success, 1 for failure. `--require-hardware` rejects software and unknown
renderers; it does not force a driver or fake acceleration. Classification uses
the actual GL renderer string and retains it in the report.

```sh
./build/bin/gpp_scene_studio --mode headless --scene synthetic \
  --benchmark 100 --warmup 10 --json
```

Benchmark records median/p95 CPU submission plus GPU completion, excluding swap
and readback. It is a diagnostic mode using `glFinish`, not the normal frame loop.
Run benchmarks without competing GPU work. Use capture and benchmark as separate
CLI operations if both result sets are needed, or separate RPC commands.

## NativeLab: software and AMD EGL

Always enter NativeLab from `/mnt/projects/Projects/Guitar-PlusPlus`, not a build
subdirectory. Use the existing trusted GPU authorization; the tool does not read
or modify NativeLab policy, mount host displays, or fall back outside the sandbox.

```sh
native-lab doctor gpu
native-lab run true
native-lab doctor gpu --session
native-lab run env EGL_PLATFORM=surfaceless eglinfo -B -p surfaceless
native-lab run xvfb-run -a build/bin/gpp_scene_studio \
  --mode headless --scene synthetic --capture software.png --json
native-lab run build/bin/gpp_studio_egl_probe build/scene-studio/egl-probe.png
native-lab run build/bin/gpp_scene_studio --mode headless --backend egl \
  --require-hardware --scene synthetic --capture hardware.png --json
```

Xvfb must run **inside** NativeLab. On the tested workspace it reports llvmpipe;
this validates functionality, not Radeon performance. `direct rendering: Yes`
does not establish hardware acceleration. Note that this machine's `xvfb-run`
wrapper itself merges child stderr into stdout. For machine parsing, start Xvfb
around a Python/shell harness that captures the executable's streams separately,
or start Xvfb separately in the same session. The executable itself keeps streams
separate; the CLI tests verify this.

EGL requests desktop OpenGL compatibility on a surfaceless display, with no
window-system surface. It explicitly establishes legacy matrix state and keeps
a tiny valid FBO bound between draws. The installed GLEW returns
`GLEW_ERROR_NO_GLX_DISPLAY` after loading GL entry points under EGL; only that
specific result is accepted, only under EGL, followed by capability validation.
The standalone probe draws a triangle, verifies pixels and writes a PNG before
using scene assets. The scene renderer requires FBO/NPOT support and emits a clear
error if unavailable; shadows/AO retain the shared renderer's fallback behavior.

The captured workspace files are accessible from the host-side Codex client.
Use the returned absolute `capture` path with Codex's local `view_image` tool.
The generic NativeLab MCP returns process text, **not** automatic MCP image content.
An actual Studio PNG was inspected this way during validation.

## Persistent automation, then MCP

`--rpc-stdio` keeps the instance alive and uses newline-delimited JSON-RPC 2.0.
It is an application protocol, not a new MCP server. Requests require a string
or integer `id`; notifications/batches are not supported. EOF, `shutdown`, SIGINT
and SIGTERM request clean shutdown. No TCP port or Unix socket is opened.

```json
{"jsonrpc":"2.0","id":1,"method":"state"}
{"jsonrpc":"2.0","id":2,"method":"set","params":{"light":[-2,4,1],"quality":3,"ao":true,"shadow_bias":0.002}}
{"jsonrpc":"2.0","id":3,"method":"capture","params":{"path":"iteration.png","width":960,"height":540}}
{"jsonrpc":"2.0","id":4,"method":"shutdown"}
```

Methods:

| Method | Parameters/result |
| --- | --- |
| `state`, `capabilities`, `inspect` | Current state, capabilities, parts and material descriptions |
| `load` | `kind`: `scene`, `file`, `model`; `source`: ID/path |
| `set` | Any of `eye`, `target`, `up`, `fov`, `light`, `quality`, `ao`, `shadow_bias`, `time` |
| `frame`, `reset`, `reload` | Fit camera, restore scene defaults, or reload files preserving current adjustments |
| `render` | Optional `width`, `height`; return updated state |
| `capture` | `path`, optional `width`, `height`, `metadata`; return artifact metadata |
| `benchmark` | Optional `frames` (100), `warmup` (10), dimensions, `timeout` (30 seconds, max 300) |
| `shutdown` | Acknowledge then exit |

Input buffering is limited to 64 KiB and one command is consumed per loop; send
one request and await its response before the next. Oversized input is rejected
and discarded through newline. Parse/parameter/method errors use JSON-RPC codes;
operational errors use -32000 with a semantic `data` string. GL runs only on the
context owner, never on an input thread. Scene replacement is transactional through
decode/upload; failed loads preserve the previous scene.

Non-benchmark commands report when execution exceeded 30 seconds; this is an
elapsed-time check, not preemption of filesystem/driver calls. Lua execution has
an instruction and memory budget, benchmarks check deadlines between frames. A
blocked driver or unusually slow OBJ decode must be stopped using NativeLab's
process timeout/kill facilities. A timeout after a completed mutation may leave
the new valid state; inspect before retrying. No shell/code-evaluation RPC exists.
Scene definitions use text-only Lua with no standard libraries or lifecycle hooks,
limited to 8 MiB and 100,000 instructions; game Lua behavior is unchanged.

Use the existing NativeLab MCP **after** the program builds and captures correctly:

1. Merge [the local configuration example](examples/scene-studio-codex.toml) into
   this project's `.codex/config.toml` on the client/host. Do not install globally.
   The directory is intentionally masked inside NativeLab; Studio never reads it.
   If `.codex` does not exist yet, install this configuration before starting the
   NativeLab session. An active session may have created an empty synthetic
   `.codex` placeholder: adding files to it makes NativeLab reject new commands.
   Finish running processes and stop that workspace's session from the workspace
   root before installing the configuration. If a file was already added to the
   placeholder, move that new file aside first, leaving the placeholder empty;
   run `native-lab stop`, then restore the file before `native-lab run true`.
   Preserve any existing client configuration and never edit NativeLab's policy
   or synthetic-directory registry to bypass this check.
2. Restart the MCP connection. Use `/mcp` in Codex or `codex mcp list` to check the
   connection. Call `diagnostics`; `cwd` must be the canonical Guitar++ workspace,
   and `native_lab_binary` must match the installed CLI. `NATIVE_LAB_BIN` prevents
   the checkout launcher and installed daemon from disagreeing about session identity.
3. Call `run` with structured argv, e.g. `["build/bin/gpp_scene_studio", "--mode",
   "headless", "--backend", "egl", "--scene", "synthetic", "--rpc-stdio"]`.
4. Await `RPC ready` on stderr with `expect`. Send a newline-terminated request via
   `write(process_id, data=...)`; use `expect`/`head` to read its response. For a
   one-shot capture simply `run` the CLI, then `wait` for exit and read the JSON.
5. Open the returned PNG path with `view_image`. End persistent sessions with
   `shutdown` then `wait`; use `kill` if unresponsive. NativeLab owns the process
   registry and lifecycle. Do not build a second process manager or bridge the
   host's localhost to the sandbox's private localhost.

See the [NativeLab README](https://github.com/Fabio3rs/native-lab#servidor-mcp-genérico)
and [official Codex MCP documentation](https://learn.chatgpt.com/docs/extend/mcp?surface=cli)
for client configuration and connection details.

## Tests and ownership

```sh
native-lab run ctest --test-dir build --output-on-failure
native-lab run python3 tools/scene_studio/test_cli.py \
  build/bin/gpp_scene_studio build/scene-studio/gpu-tests egl
native-lab run env GPP_STUDIO_TEST_BACKEND=egl build/bin/gpp_studio_tests
```

Studio tests are opt-in with its build. They check GL restoration, framebuffer
reuse, shadow invalidation, transactional errors, restricted Lua, texture/resource
ownership, PNG CRC/dimensions, observable/coherent visual differences, isolation
of personal settings and actual mouse navigation/panel separation. Pixel equality
between drivers is deliberately not required. The synthetic OBJ/MTL/Lua and AO
sidecars are original project fixtures; regenerate AO explicitly with
`gpp_bake_ao tools/scene_studio/fixtures/synthetic.obj` when its geometry changes.
Runtime never bakes or overwrites sidecars.

Context ownership is single-threaded in the tool. Models/textures, targets,
renderer programs and ImGui are destroyed while the context is still current;
then GLFW/EGL is closed. Geometry inspection is copied only on load, and tool-only
GL guards/diagnostics do not enter the game's update/render loops. Shared texture
injection runs only during upload; an unset provider retains the game's loader.
No saves, chart formats, gameplay threads or Plus behavior are changed.

Material editing, scene export, animation, shader editing, remote control, a
specialized MCP and host-to-NativeLab bridges remain outside this version.
