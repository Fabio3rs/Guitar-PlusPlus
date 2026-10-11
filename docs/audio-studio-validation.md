# Audio Studio validation — 2026-10-10

Workspace: `/mnt/projects/Projects/Guitar-PlusPlus`. Execution used the connected
Native Lab MCP with structured argv. No Native Lab permissions, audio-device
access, system libraries or submodules were changed.

## Results

| Check | Result |
|---|---|
| Normal build, Audio/Scene both OFF | Passed |
| Normal build, Scene only | Passed |
| Normal build, Audio only | Passed |
| Normal build, both Studios ON | Passed |
| Full CTest suite after final implementation changes | **125/125 passed** |
| Audio core aggregate | **8/8 cases passed** |
| Headless CLI, persistent RPC, presets and WAV exports | Passed |
| Xvfb/xdotool waveform seek and fret selection | Passed |
| GUI screenshot, inspected visually | Passed |
| Mesa OpenGL 2.0 / GLSL 1.10 override, real-song GUI capture | Passed |
| ASan + UBSan + LSan, audio core/CLI/UI | **3/3 CTest entries passed**, using system Mesa |
| GDB/ptrace, complete audio core suite | **8/8 passed**, inferior exited normally |
| strace, real OGG analysis/synthesis/export at 83.250 s | Exited 0; exported WAV |
| TSan | **Not clean**: installed BASS lifecycle diagnostics; see below |
| Audible output / physical loopback latency | Not available in the Native Lab session |
| GitHub-hosted AudioStudio workflow | Added; execution awaits a push/PR |

The regular configuration uses the existing local Clang toolchain; sanitizer
configurations use `/usr/bin/clang++-20`, Debug, and `GPP_NATIVE_CPU=OFF`.
The compiler emitted no new warnings from the Audio Studio sources. Existing
Lua `tmpnam` linker warnings remain in game/scene targets.

The dedicated CI workflow builds both Studios and uses generated test signals.
It downloads BASS and BASS_FX following the existing x86-64 setup. BASSmix is not
linked, so its archive is neither extracted nor added as a CI dependency.

## Reproduction

Build/tests normally:

```sh
cmake -S . -B build -DBUILD_AUDIO_STUDIO=ON -DBUILD_SCENE_STUDIO=ON \
  -DBUILD_TESTING=ON -DGPP_NATIVE_CPU=OFF
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --parallel 2
```

The sanitizer build used:

```sh
cmake -S . -B build/audio-studio/asan -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/clang-20 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-20 \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_AUDIO_STUDIO=ON -DBUILD_SCENE_STUDIO=OFF \
  -DBUILD_TESTING=ON -DGPP_NATIVE_CPU=OFF -DGLFW_BUILD_DOCS=OFF \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build build/audio-studio/asan --target gpp_audio_tests gpp_audio_studio --parallel 4

env LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu LIBGL_ALWAYS_SOFTWARE=1 \
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/audio-studio/asan -R '^audio_' --output-on-failure
```

The explicit library path is specific to this machine. Its default GL loader
selected `/opt/amdgpu/lib/x86_64-linux-gnu/libGLX_mesa.so.0`, which produced an
LSan report of **108124 bytes in 481 allocations** on GUI teardown. Repeating
with `/usr/lib/x86_64-linux-gnu` and software Mesa passed with leak detection still
enabled. No suppression or blanket disabling of leak detection was used.

GDB and strace ran separately from the sanitizer processes. The real source was
an existing local `build/bin/data/songs/Slow Ride/guitar.ogg`; no song was added to
the repository. Timing recorded while tracing is not a performance baseline.

## BASS compatibility and TSan finding

Installed versions observed: BASS **2.4.14.1** (`0x02040e01`) and BASS_FX
**2.4.12.1** (`0x02040c01`). The BASS header is newer than the binary. The first
loop test exposed `BASS_ERROR_NOTAVAIL` for `BASS_POS_END`; loop playback now uses
mixtime position/end sync callbacks supported by the installed binary.

An unsuppressed TSan build (`-fsanitize=thread -fno-omit-frame-pointer`) reported
mutex destruction against a condition-variable waiter inside `libbass.so` during
`BASS_Free`. A standalone executable containing only a loop of
`BASS_Init(0, 44100, 0, nullptr, nullptr)` and `BASS_Free()` reproduced the same
report and exited **66**, without linking any Audio Studio code or BASS_FX.

A second diagnostic run allowed TSan to continue reporting, while retaining exit
code 66, to exercise the complete session cancellation/publication/device-change
case. Its functional assertions passed; TSan additionally reported mutex and
condition-variable reinitialization accesses inside BASS. These reports are
preserved, not suppressed. This is not a clean TSan result, and it does not prove
that the closed-source library or the whole audio subsystem is race-free.
BASS/BASS_FX themselves are precompiled and are not sanitizer-instrumented.

The Studio's worker is joined before device reinitialization/free, and published
results are protected by its own mutex/atomics. No gameplay synchronization was
changed to work around the vendor-library finding.

## Local artifacts

These are ignored build outputs:

- `build/audio-studio/build-matrix.log`: four build combinations and full CTest.
- `build/audio-studio/tests/report.json`: CLI/RPC/export results and generated WAVs.
- `build/audio-studio/ui-tests/studio.png`: GUI driven with real mouse events.
- `build/audio-studio/real-song-gl20/studio.png`: inspected real-song GUI with GL 2.0 override.
- `build/audio-studio/real-song/fail.wav`: local real-song synthesis result.
- `build/audio-studio/strace-summary.txt`: syscall summary of that export.
- `build/audio-studio/asan/final-validation.log`: ASan/UBSan/LSan final run.
- `build/audio-studio/tsan/validation.log`: initial unsuppressed TSan run.
- `build/audio-studio/tsan/bass_lifecycle.cpp` and `bass-lifecycle.log`: minimal vendor reproduction.
- `build/audio-studio/tsan/session-diagnostic.log`: continued TSan session diagnostic.

## Remaining limits

- Native Lab exposes neither `/dev/snd` nor PipeWire/PulseAudio sockets. No Sound
  exercises decoding, FX and BASS playback state, but not audible output quality,
  dropouts, actual device switching or command-to-speaker latency.
- Linux/X11 and software Mesa were exercised. Windows, macOS, physical legacy GPUs
  and real audio-device disconnect/reconnect remain untested. The GL override is
  a compatibility check, not a benchmark of old hardware.
- Waveforms use a bounded min/max overview; zoom does not decode additional
  sample-level detail. Synthesis, FFT and FX results are available through RPC;
  custom five-track gating and gameplay integration are explicitly deferred.
- Live previews do not apply the offline whole-buffer normalization/padding.
  Loop exports retain the prototype's baked-loop behavior and stop immediately.
- No gameplay latency or runtime performance improvement is claimed.

## Review fixes — 2026-10-10

Corrected preset temporary-file collisions, rejected live-FX updates committing
settings, lost loops after device reinitialization, selection/loop divergence,
and the worker shutdown condition-variable notification window. Presets reserve
an exclusive sibling directory; live FX builds a replacement chain while the
channel is locked and retains the previous chain on failure. Selection edits in
the GUI are committed with Enter and share validation with RPC.

Native Lab validation after these fixes:

- Audio core: 12 cases passed, including source preservation during preset save,
  failed-save cleanup, rejected live FX preserving a pending job, actual decoding
  loop bounds after selection/device changes, and repeated idle-worker shutdown.
- Audio CTest: core, CLI/RPC and Xvfb UI passed (3/3).
- ASan, UBSan and LSan: the same 3/3 entries passed using system Mesa.
- GDB/ptrace: all 12 core cases passed; inferior exited normally.

The shutdown stress test does not prove all thread interleavings; the fix uses
one mutex for predicate publication and waiting to close the lost-wakeup window.
TSan was not rerun for these fixes; the existing vendor BASS finding above remains
unresolved. No physical audio output or audible-latency measurement was available.
