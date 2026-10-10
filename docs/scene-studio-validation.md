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
