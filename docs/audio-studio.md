# Guitar++ Audio Studio

Audio Studio is a development workbench for BASS/BASS_FX and the fail-sound
synthesizer. It is a separate executable, not a gameplay mode. Its controls expose
processing details deliberately: the Studio's UI is not a proposal for player-facing
settings. It does not modify game preferences, charts, songs, or the gameplay clocks.

## Build and open

```sh
cmake -S . -B build -G Ninja -DBUILD_AUDIO_STUDIO=ON -DBUILD_TESTING=ON -DGPP_NATIVE_CPU=OFF
cmake --build build --target gpp_audio_studio gpp_audio_tests --parallel
./build/bin/gpp_audio_studio
```

`BUILD_AUDIO_STUDIO` defaults to OFF. It works with `BUILD_SCENE_STUDIO` both ON
and OFF. The two tools share `gpp_studio_imgui`, including its compatibility shim
for the pinned GLFW. Audio Studio does not link the scene core or `GuitarPP_lib`.
The GUI uses OpenGL 2.0 / GLSL 1.10. A system DejaVu font is optional; the bundled
ImGui font is the fallback. BASSmix is **not used by this version**.

The file browser starts at the workspace's `build/bin/data/songs`. Choose another
folder, enter a file path, or drop an audio file into the window. Open one main
track or stem at a time; simultaneous stem mixing is not implemented. WAV, OGG,
MP3, MP2 and AIFF are listed, using the installed BASS decoders.

```sh
./build/bin/gpp_audio_studio --file 'build/bin/data/songs/Slow Ride/guitar.ogg' --time 83.25
```

The default output directory is `build/audio-studio`. Use `--output-dir` to choose
a different directory. Layout, presets, exported WAVs and captures are separate
from game configuration. Artifact filenames resolve inside this directory, and
export cannot overwrite the currently loaded source. Original music is never
copied into version control.

## Workbench workflow

1. Select a point on the waveform. White marks the analysis cursor; blue marks
   the BASS playback position. Drag to move the cursor; release requests analysis.
   Zoom and view offset navigate long tracks. Start/end fields select an export
   region or music playback loop.
2. Inspect the spectrum and MIDI candidates. Expand **Analysis parameters** to
   adjust FFT size, window/hop, note range, harmonic weighting and candidate pruning.
   Harmonic candidates are a heuristic, not chart transcription or ground truth.
3. Select a fret mask with G/R/Y/B/O, then **Generate**. No frets or no detected
   notes uses the prototype's muted E2/B2 fallback. Change voicing, envelopes,
   harmonics, pick transient, detune and normalization in **Fail Synth**.
4. In **BASS FX**, choose Music/stem or Synth preview. All parameters of the
   pre-EQ, BASS_FX distortion and post-EQ are editable, with individual/global
   bypass. EQ bandwidth takes precedence over Q; set bandwidth to zero to use Q.
   Frequency must be below the source's Nyquist frequency.
5. **Trigger** auditions the original analysis region, raw synthesized PCM,
   synthesized PCM with live FX, or the rendered result. Preview and music have
   separate volume/pan. Live FX changes update the active live preview; rendered
   PCM changes only after Generate. Normalization and padding are offline stages.
6. Store generated results in A/B. Hear A/B restores each result's parameters and
   source cursor and restarts its processed preview. Both slots must refer to the
   currently loaded file; the saved result includes its analysis time.
7. Export a generated result or the source selection, raw or processed, in PCM16
   or float32 WAV. Source selection processing uses the Music/stem FX settings.
   Presets serialize analysis, synthesis and synthesis-FX parameters (schema 1).

Numeric controls accept Ctrl+click entry in addition to dragging/sliding. Every
parameter's hover tooltip gives its automation name and range. Device controls
are in their own panel: buffer capacity, live channel buffering, update periods,
output device and requested sample rate. **Apply device settings** joins the
worker, recreates channels, preserves the song position and leaves playback paused.
Unavailable default output falls back to **No Sound** with an explicit notice;
an explicitly requested unavailable device reports an error.

### Durations and loop semantics

Defaults reproduce the prototype's starting point: FFT 8192, 200 ms analysis,
44.1 kHz stereo synthesis, 850 ms one-shot, 40 ms pre-roll, 80 ms FX tail. The
51/100/850 ms buttons change **synthesized content**, not the FX tail. A 51 ms
one-shot therefore produces about 131 ms of processed PCM with the default tail.
The pre-roll is removed; the tail remains. Envelopes adapt to short durations.

Loop synthesis uses an attack, sustained body and release. After FX, the
prototype's tail-to-head crossfade is baked and the overlapped tail/release is
excluded from the loop WAV. The loop is `[begin, end)` in frames internally;
RIFF `smpl` stores an inclusive end. Playback runs the intro once and repeats the
body. Stop ends immediately; release-on-stop is not implemented.

Loop playback uses `BASS_SYNC_MIXTIME` position/end callbacks, including with
older BASS 2.4.14 binaries that do not implement `BASS_POS_LOOP`/`BASS_POS_END`.
Callbacks only seek, read an atomic loop target and publish an atomic error code.
A short BASS channel lock protects sync replacement. No rendering-frame polling
is used to wrap the loop.

## Headless and automation

Headless initializes no graphical context and defaults to the No Sound device.
It still performs real BASS decoding and real BASS_FX processing.

```sh
./build/bin/gpp_audio_studio --mode headless --file song.ogg --time 83.25 \
  --set synth.fretMask=5 --set synth.duration=0.1 --export fail.wav --json

./build/bin/gpp_audio_studio --mode headless --file song.ogg --time 83.25 \
  --set synth.loop=true --set synth.loopBody=0.4 --float32 --export fail.loop.wav

./build/bin/gpp_audio_studio --mode headless --file song.ogg --time 10 \
  --length 2 --segment --export source-region.wav

./build/bin/gpp_audio_studio --mode headless --file song.ogg --benchmark 10

./build/bin/gpp_audio_studio --device 0 --file song.wav \
  --frames 30 --ui-capture studio.png
```

`--preset FILE` loads a schema-1 preset. Repeated `--set key=JSON_VALUE` applies
validated overrides; booleans use `true`/`false`. `--analysis-only` skips synthesis.
`--benchmark N` runs preparation/API trials and exports `benchmark.json` under
`--output-dir`; `--benchmark-report NAME.json` changes the filename. `--json` reports state, settings,
candidates, spectrum, timings, PCM levels and buffer configuration.

`--rpc-stdio` works with either GUI or headless. Send one JSON-RPC 2.0 object per
line, with a string/integer `id`. stdout carries only replies; diagnostics use
stderr. Requests are bounded to 1 MiB. Example:

```json
{"jsonrpc":"2.0","id":1,"method":"load","params":{"path":"/absolute/song.ogg"}}
{"jsonrpc":"2.0","id":2,"method":"state"}
{"jsonrpc":"2.0","id":3,"method":"seek","params":{"time":83.25}}
{"jsonrpc":"2.0","id":4,"method":"set","params":{"synth.fretMask":5,"fx.drive":3.8}}
{"jsonrpc":"2.0","id":5,"method":"generate"}
```

Load, analyze, generate and export return a `job_id`. Poll `state` until `busy`
is false and inspect `error`; the current `job_id` identifies the request.
A new job or parameter update supersedes the previous worker request. An export
already writing its final file may complete before cancellation is observed.
Never send Generate until Load has completed. The previous committed source and
result survive a failed load or generation.

| Method | Parameters |
|---|---|
| `state`, `device.list`, `cancel`, `shutdown` | None |
| `load` | `path` |
| `set`, `music.fx` | Flat parameter object; names from `state.settings` |
| `seek` | `time`, seconds |
| `selection` | `start`, `end`, seconds |
| `analyze`, `generate` | Use current cursor/settings |
| `benchmark` | `iterations` (1–1000, default 30), `path` (default `benchmark.json`) |
| `preview` | `mode`: `original`, `raw`, `live`, `processed` |
| `transport` | `target`: `music`/`preview`; `action`: `play`/`pause`/`stop`/`controls`/`loop` |
| `transport`, controls | `volume`, `pan`, `buffer_seconds` |
| `transport`, loop | `start`, `end`, `enabled` |
| `device.apply` | `device`, `rate`, `buffer_ms`, `update_ms`, `device_buffer_ms`, `device_period_ms` |
| `export` | `path`, `segment`, `raw`, `float32` |
| `preset.save`, `preset.load` | `path` |
| `slot.store`, `slot.recall` | `slot`: 0 or 1 |

All configuration changes use the same catalogue and validation as the GUI.
Presets do not silently alter device configuration. State exposes callback errors,
source/preview playback states and whether the device is No Sound.

## Production library and ownership

`src/audio` contains `gpp_audio`: typed settings, FFT/harmonic analysis, synthesis,
PCM/WAV handling, FX chains and BASS transport. It has no ImGui, OpenGL, scene,
Lua or gameplay dependency. The port replaces `std::numbers` with a C++17
constant, validates inputs, suppresses silence-only MIDI candidates, limits
oscillator harmonics to Nyquist and avoids intermediate PCM16 quantization.
Per-fret gain controls now affect synthesis explicitly.

The standalone Studio owns BASS initialization. Processing functions expect an
initialized context; reusable functions do not call BASS_Init/BASS_Free globally.
Each thread selects its BASS device explicitly. The control thread owns playback
handles, UI state and slot references. One worker owns its decoding/FX handles
and builds results privately. A mutex transfers jobs/completions; atomic revision,
progress and shutdown fields handle cancellation. Completed PCM/results are
immutable shared objects. Shutdown joins the worker before freeing playback and
BASS. Full-song waveform decoding stores a bounded min/max envelope rather than
all decoded PCM. DSP work and BASS automatic playback updating are independent
of rendering cadence.

The game does not use this library yet. Five continuous fret tracks, BASSmix,
gates, a persistent audio cache, tempo/pitch controls, gameplay event simulation
and sample-versus-stream latency comparisons are future experiments.

## Measurement and tests

Diagnostics distinguish analysis/synthesis/FX times, preview preparation (memory
stream setup), seek/play API durations and BASS buffer settings. **Audible latency
is unmeasured**, represented as JSON null. No Sound and offline processing do not
validate physical output timing. Live and rendered previews can differ in level
because only the offline path performs full-buffer RMS normalization and drains
the configured tail.

```sh
ctest --test-dir build -R '^audio_' --output-on-failure
```

The core suite uses generated tones/silence and exercises rates/channels, MIDI
candidates, fret masks, short envelopes, loops, FX parity, presets, cancellation,
failed replacement and teardown. CLI tests cover persistent RPC and exports.
Xvfb/xdotool tests use actual mouse input for waveform seeking and fret selection.
No commercial song is a test fixture.

The dedicated AudioStudio CI workflow builds both Studios and runs the full
suite. See [validation results](audio-studio-validation.md) for Native Lab runs,
sanitisers, debugger/tracer evidence and limits of the installed vendor binaries.


## Preparation and BASS API benchmark

In **Diagnostics → Benchmark / JSON report**, choose a count and filename, then
Run benchmark and export. The worker runs asynchronously; progress and Cancel
remain available. Existing music/preview channels and generated results are
preserved. Pause playback before comparisons to avoid competing audio work.
The panel shows the last completed report (which may use older settings).

```sh
./build/bin/gpp_audio_studio --mode headless \
  --file "build/bin/data/songs/Slow Ride/guitar.ogg" --time 83.25 \
  --benchmark 100 --benchmark-report slow-ride.json \
  --output-dir build/audio-studio/benchmarks
```

Headless defaults to NoSound. Select `--device ID` to measure API/preparation costs
with an accessible real output. Benchmark channels are muted in both cases.
The JSON includes source/cursor, synthesis/FX settings, device/library versions,
compiler/build configuration, configured buffers, PCM size and individual timing
samples. The first trial is separate from N warm trials. Each metric reports
mean, min, maximum and nearest-rank p50/p95/p99; small counts cannot characterize
tails reliably. Filesystem/device caches are not reset, and source loading before
the benchmark can warm caches. Run performance trials without sanitizers/debugger.

Measured stages: analysis, synthesis, offline FX, loop baking, source opening,
source seek/play/stop, PCM stream preparation, live FX attachment, explicit
initial buffer filling and reuse of an already-created PCM stream. Prefill uses
`BASS_ChannelUpdate(channel, 0)` and measures filling the configured buffer; it
is not a fixed-frame DSP throughput comparison. Source and generated PCM have
different content. PCM preparation includes WAV serialization and stream opening;
these are memory streams, not BASS sample voices. Reused PCM play has no explicit
prefill step. API timings are wall-clock durations around the transport calls.
`device_setup_ms` is one sample of device configuration/initialization including
fallback, not a statistical measurement of `BASS_Init` alone.

The report records **pending** for command-to-final-mix marker detection,
timestamped Linux system loopback, physical output, sample voices, continuous
gates, overlap and glitch tests. `audible_latency_ms` remains null. Reported BASS
latency is labelled separately and is null with NoSound. NoSound costs cannot be
used to calibrate a player's audible latency. No capture backend or extra library
is required or installed by this benchmark.

RPC `benchmark` returns a job ID; wait for `state.busy == false`, check `error`,
and read `state.benchmark` or the exported file. As with other exports, cancelling
after the final write has started may still produce the report.

## HSAMPLE versus shared PCM streams (Linux)

Generate a one-shot effect, then use **Diagnostics → HSAMPLE vs shared PCM
streams**. Choose a maximum voice count, burst count, trigger interval and an
exhaustion policy. The default rejects a trigger when all voices are occupied;
"Replace oldest" uses BASS's sample override policy and an oldest-started policy
for streams. Each burst attempts twice the voice limit, followed by a wait until
the voices finish. The standard sizes are 1, 5, 10, 20, 32 up to the requested
limit; a nonstandard requested maximum is included too.

The last generated processed PCM is used, including its recorded synthesis/FX
settings. "Trim FX tail" cuts it to the raw PCM duration; it does not regenerate
the effect and may cut off a release. Without trimming, 51 ms synthesis plus an
80 ms tail produces approximately 131 ms PCM. Both strategies receive exactly
the same float PCM; each child reports a PCM FNV-1a fingerprint for verification.
The streams share one immutable memory WAV rather than keeping a copy per voice.

```sh
./build/bin/gpp_audio_studio --mode headless --device -1 \
  --file "build/bin/data/songs/Slow Ride/guitar.ogg" --time 83.25 \
  --set synth.duration=0.051 \
  --compare-voices 32 --voice-bursts 10 --voice-interval-ms 0 \
  --voice-report voices-device.json --output-dir build/audio-studio/benchmarks
```

Add `--voice-steal-oldest` to test replacement, or `--voice-trim-tail` for a
shortened effect. Device 0 is the offline/NoSound alternative.
These measurements are muted. API-active voices and detected capacity
replacement do not prove audible overlap, correct output or absence of glitches.
The actual trigger times and scheduling lateness are recorded; zero interval
means sequential API calls, not literally simultaneous execution.

`gpp_audio_voice_probe` must be next to `gpp_audio_studio`. Each strategy/count
starts a fresh process with `posix_spawn`, without running C++/BASS code in a
forked copy of the threaded GUI. Cancellation kills and reaps the current child;
private temporary files are removed. The worker and UI keep their existing
ownership model. Effects are limited to 2 seconds, voices to 32, bursts to 100
and interval to 100 ms, with a 150-second estimated limit and 180-second timeout
per case. No dependencies are installed. The child uses the requested device
without silently falling back; an exclusive device already owned by the parent
may reject a second initialization. Use a shared device or NoSound in that case.

The JSON contains:

- Acquisition, restart and play timings, raw events, warmup burst separately,
  p50/p95/p99/maximum/mean, rejected triggers, API failures, replacements and
  maximum observed active voices. Sample acquisition includes native voice
  creation/recycling; streams are created up front and acquired from a pool.
- RSS/PSS and peak RSS from `/proc`, after BASS initialization, PCM load,
  preparation, bursts, resource release and PCM release. Missing metrics are
  null. Event instrumentation is preallocated before the baseline; snapshots and
  state queries are outside API timers. JSON serialization follows snapshots.
- PCM size/capacity, shared WAV capacity, RSS/PSS strategy growth relative to
  loaded PCM, and an approximate per-extra-voice RSS difference versus the
  one-voice case. Page granularity and allocator retention make these process
  measurements noisy; a negative difference is not "negative allocation".
- Device, versions, build configuration, source time/settings and explicit
  pending output-latency/glitch/gate measurements.

The UI displays the last completed/imported report. Use **Open previous report**
to reopen a JSON report, with relative paths resolved under the output directory.
RPC methods are `voices.compare` (`voices`, `bursts`, `interval_ms`,
`steal_oldest`, `trim_tail`, `path`) and `voices.load` (`path`). Comparison is an
asynchronous job; wait for `state.busy` to become false and inspect `error`.
Results are in `state.voice_comparison` and the output file. Failed imports leave
the previous report intact.
