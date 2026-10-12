# Audio Studio validation

## Adaptive implementation review — 2026-10-12

Reviewed the changed Audio Workbench/core paths and their ownership, cancellation,
cache persistence, CLI option precedence and preview lifetimes. Gameplay/shader
changes already present in the workspace were excluded from edits.

Two reproducible CLI issues were corrected: loading a cache discarded explicit
`--set` overrides (requesting `fx.drive=2` still generated with 3.4), and requesting
a report after rescanning could export the old, stale bank. Artifact loading now
precedes explicit preset/parameter overrides and processing; reports select a bank
only when current. CLI regression tests cover bank-only regeneration and rescanning
with a changed hop. The cache magic comparison was also made explicit (`!= 0`).

All 17 changed C++/header files pass `clang-format --dry-run --Werror` using the
repository configuration. All 12 changed translation units were checked with
`clang-tidy -p build`, the repository's enabled checks and a header filter for
`src/audio` and `tools/audio_studio`. No compiler errors were reported; this is
**not a warning-free tidy result**. Most findings concern magic numbers, public
data structures, casts and style. Bounded frame/channel/voice counts were checked
when triaging numeric conversions.

The optional-access warning in the worker is guarded by the condition-variable
predicate and the same mutex, followed by the shutdown check. The proposed Session
field reorder would compromise its documented resource destruction order and was
not applied. RPC `std::endl` deliberately flushes each response. RapidJSON reports
an alignment issue in its missing-member fallback (`static char buffer` plus
placement new); this is a third-party diagnostic, not silently suppressed or
fixed by editing the submodule. Imported cache field access is checked before that
fallback; no new reachable failure from this warning was demonstrated.

Logs and the file list are in `build/audio-studio/review/` (`*.tidy.log`,
`files.json`, `summary.json`, `tests.log`, `asan-tests.log`). After the fixes,
the build and all five audio CTest entries passed normally and with
ASan/UBSan/LSan. No bulk tidy fixes or dependency configuration changes were applied.

## Adaptive cache and sustained restart — 2026-10-12

The new reusable `src/audio` scanner/cache/prepared-bank components are exercised
through the Workbench GUI, CLI and RPC, without gameplay integration. Local external
libraries were updated from the user-provided ZIPs to BASS **2.4.18.3**
(`0x02041203`) and BASS_FX **2.4.12.6** (`0x02040c06`), including the runtime
copies under `build/bin`. Previous binaries/headers are preserved under
`build/audio-studio/adaptive-debug/original-bass`. No system library was replaced.

Validation in Native Lab:

- Full normal build passed. Full CTest passed **181/181** after the dependency
  update. Subsequent focused checks passed all five audio CTest entries after
  the final detector/test refinements.
- Seven adaptive core cases cover deterministic scan, opposing stereo channels,
  volume changes, transient/persistent harmonic changes, silence/fallback, short
  input, boundaries, deduplication, invalidation, roundtrip PCM, malformed analysis,
  processing versions, cancellation during generation, atomic-write failure,
  voice exhaustion and Session/device lifetime.
- The UI test now clicks the actual Whole Song Analysis tab, scans, generates,
  prepares and selects the representative on the region strip. Xvfb uses a
  1600x1000 screen so the 1440-wide window's tabs are reachable.
- CLI/RPC exercises the full chain, bank-only FX regeneration, exports, comparison,
  sustained release, stale/failed loads, simulation and seven isolated benchmark
  subprocesses. Existing voice/CLI/UI tests pass.
- **ASan + UBSan + LSan: 5/5 audio entries passed**, including the expanded UI and
  malformed-file tests. System Mesa/No Sound were used, with leak checks enabled.
- GDB ran the six adaptive cases present at that point three times; the inferior
  exited normally. The seventh malformed-file case was subsequently checked in
  the normal and sanitizer suites.

### Fade-state correction and dependency isolation

Sustained press now explicitly replaces a pending volume slide with a zero-duration
slide; stop cancels it as well. Pan/buffer configuration occurs during preparation.
This avoids relying on newer SetAttribute slide-cancellation behavior. Tests check
re-press after completed fade, re-press during fade, surviving the old fade deadline,
restored gain, loop bounds and cancelling slides on stop. Existing mixtime callbacks
remain unchanged.

The test matrix used the same Native Lab/glibc 2.39 and explicit slide cancellation:

| BASS | BASS_FX | Repeated sustained test |
|---|---|---|
| 2.4.14.1 | 2.4.12.1 | First passed; second hung, timeout 30 s |
| 2.4.14.1 | 2.4.12.6 | 26 passed; iteration 27 hung, timeout 55 s |
| 2.4.18.3 | 2.4.12.1 | 100/100 passed with expanded fade assertions |
| 2.4.18.3 | 2.4.12.6 | 100/100 passed with expanded fade assertions |

Before the update, an attached GDB captured the second press blocked in
`BASS_ChannelPlay`, through `pthread_cond_signal` and
`__condvar_quiesce_and_switch_g1`. The improvement follows the BASS binary in this
matrix. It does **not** establish the internal root cause, rule out a glibc
interaction, or identify the installed glibc patch level (`dpkg-query` had no libc6
package record). No claim that glibc BZ #25847 caused this hang is made. The default
`BASS_CONFIG_UPDATETHREADS` was already 1; no global update-thread workaround was
introduced. Historical TSan findings below have not been revalidated on the new
binary; passing ASan does not replace that check.

Runtime capability probing on 2.4.18.3 confirmed that `BASS_SAMCHAN_STREAM` returns
`BASS_CTYPE_STREAM_SAMPLE` and that setting loop/end positions succeeds. The
production sustained experiment retains the tested portable mixtime path; merely
accepting position setters is not an audible-loop-quality test. The new FX binary
produces GNU linker warnings about local `.dynsym` entries; linking and all audio
checks nevertheless completed. The vendor binary was not rewritten to silence them.

### Real-song measurements

RelWithDebInfo, `GPP_NATIVE_CPU=OFF`, No Sound, no sanitizers/debugger. Local guitar
stems were decoded sequentially; no third-party audio was committed. Each bank was
loaded/prepared in a separate probe process, with 100 muted trigger calls and one
warmup. The bounded bursts intentionally also measure rejection: 68/100 calls were
rejected with the default 32-voice reject policy. Reported accepted-call timings
must not be confused with the faster rejection path or physical audible latency.

| Stem / profile | Regions | Unique five-sample sets | Bank bytes | Prepare ms |
|---|---:|---:|---:|---:|
| Slow Ride / aggressive, dedup | 5 | 2 | 483495 | 0.462 |
| Slow Ride / balanced, no dedup | 11 | 9 | 2141238 | 2.050 |
| Slow Ride / balanced, dedup | 11 | 6 | 1437534 | 1.136 |
| Slow Ride / conservative, dedup | 9 | 4 | 963162 | 0.880 |
| Slow Ride / fixed baseline | 1 | 1 | 241255 | 0.199 |
| Raining Blood / aggressive, dedup | 4 | 3 | 715557 | 0.755 |
| Raining Blood / balanced, dedup | 7 | 6 | 1427260 | 1.197 |
| Raining Blood / conservative, dedup | 7 | 6 | 1427248 | 1.577 |
| Raining Blood / fixed baseline | 1 | 1 | 241324 | 0.208 |

Slow Ride: 251.147 s, 2512 observations, 700.8 ms scan, 116.5 ms synthesis,
6.84 ms FX. The balanced deduplicated bank contains 30 samples and 1,386,720 PCM
bytes; prepared-process RSS was 8,802,304 bytes (whole process, not bank-only RAM).
Its accepted trigger-call p95 was 0.00524 ms. Raining Blood: 259.320 s, 708.6 ms
scan; balanced accepted-call p95 0.00488 ms. These are individual local runs,
not hardware-independent performance guarantees.

Sensitivity profiles are not monotonically ordered by final region count: early
boundaries, reference adaptation and subsequent merging interact. Silent regions
always share the fallback even when harmonic deduplication is disabled. The
aggressive results above illustrate why the timeline and listening comparisons
are required. **No perceptual quality conclusion or minimum sufficient set count
has been established by these automated runs.** Physical audio/loopback latency
and audibility of loop seams remain unmeasured.

Artifacts (ignored build outputs):

- `build/audio-studio/adaptive-debug/native-attach-gdb.log`: original attached stack.
- `build/audio-studio/adaptive-debug/slide-old.log`, `slide-new-regression.log`,
  `new-bass-only.log`, `new-fx-only.log`: dependency matrix.
- `build/audio-studio/adaptive-debug/gdb-new.log`: successful post-update GDB run.
- `build/audio-studio/adaptive-debug/ctest.log`, `audio-final.log`, `asan.log`:
  full and final focused validation.
- `build/audio-studio/adaptive-debug/runtime-capabilities.json`: loaded versions/API probe.
- `build/audio-studio/adaptive-real-new/` and `adaptive-raining/`: banks, reports,
  seven benchmark cases and per-case banks for auditioning.

## Original workbench validation — 2026-10-10

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

## Preparation/API benchmark — 2026-10-10

Added asynchronous GUI/RPC benchmarking and CLI JSON report export, without new
libraries. Normal and ASan/UBSan/LSan audio CTest suites passed (3/3 each),
including report percentile/raw-sample consistency, RPC completion and invalid
count/path checks. Sanitizer timings are not used as benchmark results.

Native Lab has pw-record/parec and PipeWire 1.0.5/PulseAudio 16.1 development
packages, but no /dev/snd. `pactl info` failed with connection refused and
`pw-cli info 0` reported host down. System capture and physical output remain
pending; final-mix marker instrumentation is also explicitly not implemented.

Two sequential, unsanitized RelWithDebInfo runs used the existing Slow Ride
`guitar.ogg` at 83.25 seconds, 100 measured repetitions plus one separate first
trial, NoSound, default buffers and default FX. Source loading warmed caches.
Reports (ignored build artifacts):

- `build/audio-studio/benchmarks/slow-ride-nosound.json`: default 850 ms synthesis.
- `build/audio-studio/benchmarks/slow-ride-51ms-nosound.json`: 51 ms synthesis;
  processed length about 131 ms because the configured 80 ms FX tail is retained.

For the 51 ms case, p50/p95 in milliseconds: analysis 3.103/3.550, synthesis
4.432/4.993, offline FX 0.226/0.317, PCM preparation 0.065/0.079, reused PCM
play API 0.0043/0.0074. The default-duration synthesis p50 was 73.061 ms.
These are local processing/API observations, not latency-to-output measurements,
and do not justify a player calibration offset or a definitive playback strategy.

## Voice/memory comparison and architecture review — 2026-10-10

The Linux workbench now compares HSAMPLE/HCHANNEL against independently playable
streams sharing one float WAV. Each strategy/count runs in a fresh exec process.
Both strategies consume identical PCM (verified by a reported fingerprint).
Reject/oldest-replacement policies, saturation, scheduling lateness, warmup/raw
API timings, RSS/PSS/HWM snapshots, release snapshots and marginal memory estimates
are exported together. Burst completion is checked through channel state before
starting the next burst, including device-buffering grace time.

Validation: normal and ASan/UBSan/LSan audio suites passed (4/4 entries: core,
CLI, voices and Xvfb UI). The voice test covers distinct process IDs, matching
PCM hashes, memory availability, saturation/replacement accounting, preserved
reports on invalid import, cancellation/reaping and temporary-directory cleanup.
The isolated HSAMPLE probe also exited normally under GDB/ptrace. Performance
and memory comparisons must use the regular build, not sanitizer/debugger runs.
TSan's previously documented vendor BASS issue remains unresolved.

Architecture review:

- `src/audio` remains independent of the UI, benchmark subprocess management and
  gameplay state. Production DSP/transport logic is reused without changing the
  game's update/render threads or OpenGL compatibility baseline.
- Session/UI ownership remains on the control thread; immutable job inputs and
  completions cross the worker boundary under the existing mutex/revision model.
  The worker launches probes with `posix_spawn`, avoiding C++/BASS work after a
  raw fork of the multithreaded application. Cancellation reaps the active child.
- The helper executable has no ImGui/Scene Studio dependency. Keep it beside the
  Studio binary. Benchmark code is tool-specific; the experimental voice pool
  has deliberately not become the game's runtime sound-bank API yet.
- Memory instrumentation is sampled outside API timing; event buffers are
  allocated before baseline and JSON serialization follows the snapshots.
  RSS/PSS changes include allocator/page effects and are not exact BASS heap
  allocations. Acquisition includes sample voice creation/recycling versus
  scanning the prepared stream pool, which is stated in the report.
- Reports have a versioned shape and imported GUI fields are validated before
  publication. Failed imports preserve the last report. Device initialization
  failures are explicit; the probe never silently substitutes NoSound.
- C++ code under `tools/audio_studio` and `src/audio` was formatted and checked
  with `clang-format --dry-run --Werror`, using the existing project style from
  the root `.clang-format`, without audio-specific overrides.

No blocking architectural issue was found in this increment after fixing the
burst-drain boundary. Remaining work is final-mix/loopback/physical measurement,
audible overlap/glitch validation, continuous gates and sustained background CPU
profiling. The GUI/session currently share a parameter catalogue and report JSON;
that is sufficient at this size. If more benchmark families are added, typed
report models and a dedicated benchmark UI panel would reduce schema/UI coupling.
This is a maintainability direction, not a prerequisite for the present tests.

Reference run: `build/audio-studio/benchmarks/voices-32-nosound.json`, regular
RelWithDebInfo build, Slow Ride at 83.25 s, 51 ms synthesis plus retained 80 ms
FX tail, 10 measured bursts, zero inter-trigger interval and reject-at-capacity.
All ten cases used distinct processes and identical PCM fingerprints. At 32
voices, both strategies observed 32 active channels, zero API failures and 320
expected capacity rejections (32 excess requests in each of ten bursts).
Observed RSS growth from loaded PCM to the post-burst snapshot was 64 KiB for
HSAMPLE and 1904 KiB for shared-WAV streams. This is a local NoSound process-memory
comparison; allocator reuse/shared pages prevent interpreting it as exact BASS
allocation or as proof of output latency. The reference run was made after builds,
debugger and sanitizer tests finished.
