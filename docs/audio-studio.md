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
`--benchmark N` excludes file loading and reports mean FFT, synthesis and FX
processing durations after an initial warmup. `--json` reports state, settings,
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
