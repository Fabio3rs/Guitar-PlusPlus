# Scene Studio validation — 2026-10-10

Workspace: `/mnt/projects/Projects/Guitar-PlusPlus`. No NativeLab policy or GPU
permissions were changed. The ImGui gitlink and checkout remained at
`778992face67ecefaa5c2932db4b2101545ea4df`; submodule connectivity validation and
worktree checks passed. Existing staged submodule changes were preserved.

## Executed checks

| Check | Result |
| --- | --- |
| `BUILD_SCENE_STUDIO=OFF`, build all | Passed; GuitarPP has no ImGui/PNG/EGL tool dependencies |
| `BUILD_SCENE_STUDIO=ON`, build all | Passed with the existing ImGui submodule |
| Full CTest suite inside NativeLab | **114/114 passed** (includes three aggregate Studio tests) |
| Studio ownership/state tests under EGL | **4/4 passed**, including textured reload and texture deletion |
| CLI/RPC/PNG/visual suite under EGL | Passed |
| Desktop GUI, 45 frames and full UI capture | Passed on Radeon; captured image inspected |
| Xvfb GUI and real mouse automation | Orbit, pan, zoom and panel input isolation passed |
| OpenGL 2.0 / GLSL 1.10 Mesa override | GUI rendered and captured successfully in software |
| EGL probe | Triangle pixels checked and PNG saved on Radeon |
| Real garage OBJ/MTL/textures/AO | Loaded/rendered/captured inside NativeLab; source assets not redistributed |
| Direct NativeLab MCP workflow | `run`, `expect`, `write` (set/capture/shutdown), `wait`; exit 0 |
| Agent image inspection | Captured PNG opened through Codex `view_image` using the shared absolute workspace path |
| Local Codex configuration | Installed only in project `.codex/config.toml`; no global configuration edited |

The existing game still emits legacy compilation/link warnings (including Lua's
`tmpnam` reference); they were not hidden through global warning suppression.
Tests protect sentinel personal settings/Config.lua, check that no other files
appear in the temporary working directory, reject malformed/oversized requests,
and verify that failed scene loads preserve the previous scene.

Installing the local client configuration initially populated an active NativeLab
synthetic `.codex` placeholder, so policy validation correctly rejected subsequent
commands. After all registered processes had exited, the new file was temporarily
moved aside, this workspace's session stopped, and the file restored before
starting a new session. No policy or authorization was changed. A subsequent
MCP-launched EGL garage capture with `--require-hardware` exited successfully;
the resulting interior image was inspected.

## Observed renderers

- Desktop GLFW: **AMD / Radeon RX 7900 XTX (radeonsi, navi31, LLVM 19.1.5,
  DRM 3.63, 6.14.0-1020-oem)**, OpenGL **4.6 Compatibility Profile**, Mesa
  **25.0.0-devel**; classified hardware.
- NativeLab EGL surfaceless: the same Radeon renderer and compatibility profile;
  classified hardware. The API requested is desktop OpenGL, not OpenGL ES.
- NativeLab Xvfb/GLX: **Mesa / llvmpipe (LLVM 19.1.5, 256 bits)**,
  OpenGL **3.3 Compatibility Profile**, Mesa **25.0.0-devel**; classified software.
- The GL 2.0 test deliberately restricted Mesa's advertised GL/GLSL versions;
  it remained llvmpipe and is not evidence from a physical OpenGL 2.0-era GPU.

The first EGL integration exposed an initial matrix-mode state of zero on this
stack. Explicit initialization of the projection/modelview state fixed restoration;
no render errors were discarded to make the tests pass. GLEW's specific missing-GLX
result is accepted only with a validated EGL context and working required GL APIs.

## Local evidence

Artifacts are intentionally ignored build outputs:

- `build/scene-studio/desktop.png` and `.png.json`: complete desktop editor.
- `build/scene-studio/studio-ui.png`: Xvfb editor.
- `build/scene-studio/studio-gl20.png`: compatibility smoke.
- `build/scene-studio/egl-probe.png`: standalone GPU probe.
- `build/scene-studio/garage-gpu.png` and `.png.json`: local garage capture.
- `build/scene-studio/agent-iteration.png` and `.png.json`: capture requested
  directly through NativeLab MCP after changing light and shadow quality.
- `build/scene-studio/tests/report.json`: software visual/CLI results.
- `build/scene-studio/gpu-tests/report.json`: hardware visual/CLI results.
- `build/scene-studio/mouse-tests/`: actual mouse-navigation capture and logs.
- `build/Testing/Temporary/LastTest.log`: complete suite output.
- `build/scene-studio-off-build.log` and `build/scene-studio-build.log`: build logs.

Visual tests compare alternate renders on the same backend. They require a
nontrivial pixel difference; removing AO/shadows must increase total brightness.
For example, the Radeon test observed mean absolute channel differences of about
0.084 for AO, 0.185 for shadows/bias, 5.23 for moved lighting and 14.95 for the
alternate camera (8-bit channels at 320x240). These are regression checks, not
performance claims or cross-driver golden images.

## Remaining limits

### Review follow-up — 2026-10-10

The reported late-load failure was reproduced through RPC with a triangle near
X = 2,000,000: `load` returned an error but replaced the active scene. Loading now
validates the candidate's initial settings before committing the scene or changing
the renderer. Regression tests cover scene/settings preservation, reset defaults,
shadow-cache preservation and the persistent RPC response after this failure.

After the fix, the full NativeLab software suite passed **114/114 CTest entries**
(the Studio GL entry now contains five cases), and the EGL Studio tests passed
**5/5**. The new separate `SceneStudio.yml` workflow enables Studio, runs the full
software suite and uploads diagnostic artifacts. YAML parsing and shell syntax
were checked locally; its first GitHub-hosted execution remains pending push.

The operational `.codex/config.toml` was removed from the Git index and ignored,
with the local file preserved. The configuration example remains versioned.
CMake's minimum remains 3.25 to support `add_subdirectory(... SYSTEM)`.

### Platform and operational limits

- Linux desktop/X11, Xvfb and AMD EGL were exercised. Windows, macOS, Wayland,
  other GPU vendors and physical legacy GPUs remain untested.
- No performance comparison against an earlier game build was claimed; the
  shared game draw path received no new per-frame inspection or instrumentation.
- RPC timeouts cannot preempt a blocked driver/filesystem call. Use NativeLab's
  process lifecycle tools for those failures; inspect state after a timeout.
- The local config is ready for the client's next reload. The already-connected
  NativeLab MCP was used for this run; a newly restarted client was not exercised.
- Material/scene editing and export, animation and a specialized MCP are deferred
  by design. TCP/Unix socket servers are not included; persistent control uses stdio.

## Model viewer implementation — 2026-10-12

Implemented cursor-anchored orbit zoom, aspect-correct framing, orthographic and
axis views, captured free navigation, per-part geometric selection, focus,
hide/isolate/show-all, neutral/wireframe display modes, bounded helpers and shared
GUI/CLI/RPC operations. Inspection includes per-part bounds/triangles and texture
paths, loaded status and dimensions. A small selected detail within a large model
has a regression test for continued zoom and close clipping.

### Checks executed

- Full configured build with Scene/Audio/GUI Studios enabled: passed (Clang,
  RelWithDebInfo, `GPP_NATIVE_CPU=OFF`). Separate Debug game build with all three
  Studios disabled: passed (GCC, `GPP_NATIVE_CPU=OFF`).
- All **180 CTest entries** passed, run as 176 existing non-Scene-Studio entries
  plus four Studio entries. Studio includes **7 CPU camera cases** and **12 GL
  cases**, CLI/RPC/PNG integration, and actual xdotool keyboard/mouse automation.
- Mouse automation covers orbit, pan, cursor zoom, panel isolation, click
  selection, double-click pivot, focus shortcut, captured free movement, Escape,
  focus loss and text entry without camera movement.
- Radeon EGL: **12/12 Studio GL cases passed**; extended CLI/RPC/visual suite also
  passed. Real garage and isolated OBJ captures were generated and inspected.
- Mesa OpenGL **2.0** / GLSL **1.10** override: **12/12 GL cases** and extended
  GUI mouse/keyboard automation passed. This is software compatibility coverage,
  not a test on a physical legacy GPU.
- `git diff --check`: passed. Existing bundled Lua `tmpnam` and other legacy
  build warnings remain; no new warnings in the modified shared renderer/model
  implementation were left in the final Clang build.

### Measurements

Hardware: Radeon RX 7900 XTX, NativeLab EGL. Render measurements use 960x540,
quality 2, AO, the same explicit camera before/after, 10 warmup + 100 measured
frames. They measure CPU submission plus GPU completion, excluding swap/readback.
The final after measurement ran without concurrent test/build work. An earlier
sample overlapped Xvfb testing and was discarded from this comparison.

| Scene | Before median / p95 (ms) | After median / p95 (ms) |
| --- | --- | --- |
| Synthetic | 0.09468 / 0.12498 | 0.102291 / 0.132860 |
| Garage | 0.363541 / 0.619482 | 0.333001 / 0.587661 |

These short samples are a sanity comparison, not evidence of a speedup or a
performance guarantee for older hardware. Picking was measured separately around
`Session::pick`, excluding RPC/JSON: 500 viewport queries after 20 warmups gave
median/p95 **0.000090/0.000140 ms** for the synthetic fixture and
**0.001620/0.002560 ms** for the garage. The sample rays hit geometry 86/500 and
500/500 times, respectively; cost remains dependent on intersected triangle count.

Normal rendering performs no picking, CPU geometry copies or readback. The
optional shared visibility mask adds a per-part check; helper geometry has bounded
line counts and existing VBOs are reused for wireframe/highlights. Default game
calls supply no mask, and neither gameplay threading nor gameplay state was changed.

Local ignored artifacts: `build/scene-studio/viewer-baseline.json`,
`build/scene-studio/viewer/render-comparison.json`,
`build/scene-studio/viewer/picking-benchmark.jsonl`, garage/isolated-OBJ PNGs under
`build/scene-studio/viewer/`, EGL visual reports under `viewer-egl/`, and GL 2.0
mouse artifacts under `viewer-gl20-ui/`. The temporary picking benchmark source
and executable are build artifacts, not a new runtime dependency.

### Deliberate limits

- Static OBJ/MTL inspection; no material/transform editing, export or animation.
- Picking is two-sided geometry-based and does not sample texture alpha holes.
- Hidden geometry stops casting dynamic shadows but its baked AO remains in
  other surfaces. Game shadow volume/lighting remains fixed, with Unlit available
  for neutral inspection.
- Part IDs belong to a loaded revision; successful reload clears selection and
  visibility, while retaining camera/display/light settings.
- Linux X11, software Mesa and AMD EGL were exercised. Windows/macOS/Wayland,
  other GPU vendors, physical old GPUs and a manual gameplay session were not
  validated in this change.

### Follow-up pitfall review (2026-10-12)

- Reproduced invalid projection matrices for tiny bounds viewed from 1,000,000
  units away; depth-plane margins now account for floating-point precision.
- Reproduced failure to load a 0.00001-unit triangle because automatic framing
  produced a degenerate camera. Framing and projection conversion now respect
  the camera's minimum distance/span.
- Fixed Unlit ignoring MTL UV scale, offset and clamp. A three-part TGA fixture
  checks rendered colors, repeated/clamped sampling of a shared texture and
  preservation of the caller's texture matrix. Texture wrap parameters are
  explicitly restored because the GL attribute stack does not restore them.
- Code inspection found that orbit release depended on seeing a single release
  event inside an active viewport. It now checks the button level and cancels
  gestures when the viewport is inactive. Existing physical mouse, keyboard,
  Escape and focus-loss tests pass; hiding the viewport mid-drag was not
  separately automated.
- Applied repository clang-format to all 14 changed/new C++ files. Full build
  passed; all 180 CTest entries passed. Studio now contains eight camera cases
  and 14 GL cases, plus CLI/RPC and physical UI integration scripts.
- All four Studio CTest entries also passed with Mesa forced to OpenGL 2.0 /
  GLSL 1.10. All 14 GL cases passed via hardware EGL on the Radeon RX 7900 XTX.
- Documented the inherited TGA-only decoder. PNG/JPEG loading was not added.
  No new GPU feature requirement or gameplay synchronization change was made.
  Physical old GPUs and manual gameplay remain untested.
