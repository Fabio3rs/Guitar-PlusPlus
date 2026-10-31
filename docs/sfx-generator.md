# Procedural SFX generator

Guitar++ has an offline C++17 sound renderer, a restricted Lua recipe loader,
`gpp_sfxgen`, and a dockable **SFX Designer** in Audio Studio. They generate audio
without loading a song, FFT analysis or commercial samples. The adaptive Fail
Synth, its presets, and the game's existing sounds remain independent.

## Build and run

```sh
cmake -S . -B build -G Ninja -DBUILD_SFXGEN=ON -DBUILD_AUDIO_STUDIO=ON \
  -DBUILD_TESTING=ON -DGPP_NATIVE_CPU=OFF
cmake --build build --target gpp_sfxgen gpp_sfx_tests gpp_audio_studio gpp_audio_tests --parallel
ctest --test-dir build -R '^(sfx_|audio_)' --output-on-failure
```

Both tool options default to OFF. Set `BUILD_AUDIO_STUDIO=OFF` to build the CLI
without ImGui or the graphical editor. The repository's normal CMake system
requirements still apply. The audio library links BASS/BASS_FX; the CLI never
initializes BASS, opens a device, initializes GLFW, or creates a GL context.
Keep `GPP_NATIVE_CPU=OFF` for distribution. No new external dependencies are used;
Lua comes from `src/Lua`, and JSON reporting uses existing RapidJSON headers.

```sh
build/bin/gpp_sfxgen --spec data/sounds/specs/plus/plus_activate.lua \
  --output build/sfx/plus_activate.wav
build/bin/gpp_sfxgen --all data/sounds/specs --output-dir build/sfx --json \
  > build/sfx-manifest.json
build/bin/gpp_sfxgen --all data/sounds/specs --check
build/bin/gpp_sfxgen --all data/sounds/specs --compare-dir build/sfx --json
```

`--check` loads and validates, without rendering. `--float32` selects IEEE float32;
default WAV encoding is PCM16. `--compare-dir` renders and compares complete WAV
bytes, without writing. Select the same encoding used by the reference export.
`--force` explicitly replaces existing generated WAVs; ordinary export refuses
existing files and symlinks. Batch output is flat: `<id>.wav`. Duplicate IDs are
errors; independent recipes continue after a failure. Any failure returns exit
code 1. JSON contains one result/error per recipe, generator version, render/export
time, PCM frames/format, levels, half-open loop bounds and a content fingerprint.
Top-level option/directory failures print diagnostics to stderr and return 1.

The fingerprint is FNV-1a over the generator version (uint32 little-endian) followed
by the canonical float32 WAV bytes. It is an accidental-change detector, not a
cryptographic signature. Comparisons use bytes, not only hashes. Run builds with
matching generator/compiler/math environment for reproducibility comparisons.

## Audio Studio workflow

```sh
build/bin/gpp_audio_studio
# Or open a recipe and generate its first preview immediately:
build/bin/gpp_audio_studio --sfx data/sounds/specs/plus/plus_activate.lua
# Fully offline Studio session, sharing exactly the same worker:
build/bin/gpp_audio_studio --mode headless --device 0 \
  --sfx data/sounds/specs/garage/garage_amp_idle_hum.lua \
  --export amp.wav --float32 --output-dir build/audio-studio/demo --json
```

1. Click **Sources → SFX Designer**, or launch with `--sfx`. The window can dock
   anywhere. Existing dock layouts are preserved; its first opening is a floating
   window with room for the editor. Dock it by dragging its title bar. Dropping a `.lua` file opens it in the designer.
2. In **Recipes**, filter the installed relative filename/category paths, select
   a recipe, or enter an **Open path**. Change **Recipe folder** to browse another
   library. **New from template** creates a plucked string; **Duplicate** creates
   an unsaved copy with a new ID. **Reload disk** reloads the opened recipe.
3. In **Sound**, edit ID/category/description, duration, sample rate, channel count,
   seed, master gain, peak ceiling and loop settings. In **Layers**, add/remove or
   bypass layers and edit generator, gain/pan, frequency sweep, envelope, filters
   and modulation. Numerical edits do not start rendering. Hover fields for bounds.
4. **Generate / Regenerate** captures the draft and queues the worker. Progress
   and **Cancel** remain available. Edits during generation are marked as newer
   than the generated snapshot. A failed load/render or cancellation keeps the
   last valid recipe/result. Opening a successful new recipe retains the previous
   PCM until generation; the generated ID and stale label make this visible.
5. **Play**, **Stop**, **Loop Preview** audition the generated snapshot through
   its own `AudioTransport`. Loop Preview uses stored bounds, or the whole buffer
   for one-shots. Preview volume is in **Export** and does not change exported PCM.
   Generation and export work with **No Sound**; an explicit notice explains that
   preview is inaudible. Device changes recreate transports and retain generated PCM.
6. **Save Lua** writes back to the opened recipe (or the Save As path for a new
   draft). **Save As** refuses to overwrite another existing recipe. Relative save
   paths resolve inside the Studio output folder; an absolute path can save into
   the repository. Saving writes canonical declarative Lua: comments, arithmetic
   expressions and original formatting are not retained. Unknown fields are rejected,
   so there are no silently discarded schema extensions. Reload/new/open (including
   file drops) ask **Save and continue / Discard / Cancel** before replacing an
   edited draft. Saving failures keep the dialog and draft available. Generating
   audio does not clear the unsaved marker. Duplicate retains current edits in its
   new draft. There is no undo stack or application-exit save prompt.
7. **Export → Export WAV** writes the last generated snapshot, which may differ
   from current unsynthesized edits. Select PCM16 or float32. Files must be within
   the Studio output folder and cannot replace its loaded source. Existing WAVs
   are refused; choose another filename. Save the Lua recipe separately.

Waveform min/max bins are built on the worker, bounded to 2048. The window reports
PCM duration, sample count (interleaved values), sample rate/channels, peak, RMS,
loop frames and generation time. A labeled display gain makes quiet ambient
waveforms visible without changing audio or exported levels. It does not scan all PCM every UI frame.

Optional stdio RPC methods use the existing Studio JSON-RPC connection:

| Method | Parameters |
|---|---|
| `sfx.load` | `path`, asynchronous load; rejects unsaved edits unless `discard: true` |
| `sfx.set` | `recipe`: complete Lua text, validated before replacing the draft |
| `sfx.generate` | asynchronous generation, no source required |
| `sfx.save` | `path`, same save rules as GUI |
| `sfx.export` | `path`, `float32` (default false), asynchronous export |
| `sfx.preview` | `loop` (default false) |
| `sfx.stop` | none |
| `state`, `cancel` | existing shared job status/cancellation; `state.sfx` exposes result metrics and `dirty` (unsaved recipe edits, independent of PCM `stale`) |

## Schema 1

Recipes return a table. Required fields are `schemaVersion = 1`, `id`, `duration`
and a nonempty dense `layers` array. Each layer requires `type`. All other fields
have defaults. Unknown fields, numeric strings, invalid enums, holes in arrays,
non-finite numbers and inconsistent bounds are errors with filename/field paths.

```lua
return {
    schemaVersion = 1,
    id = "pick_demo",
    category = "interface",
    description = "A short plucked fifth",
    sampleRate = 44100,
    channels = 2,
    duration = 0.3,
    seed = 42,
    outputGainDb = -6,
    peakLimit = 0.95,
    layers = {
        {
            type = "pluck",
            frequency = { startHz = 329.63, endHz = 329.63 },
            damping = 0.997,
            gain = 0.7,
            pan = -0.2,
            envelope = { attack = 0.001, decay = 0, sustain = 1, release = 0.04 },
        },
        {
            type = "noise",
            duration = 0.02,
            color = "pink",
            gain = 0.1,
            filter = { type = "bandpass", cutoffHz = 1800, q = 0.7 },
            envelope = { attack = 0.001, decay = 0.012, sustain = 0, release = 0.005 },
        },
    },
}
```

### Sound fields and limits

| Field | Default | Bounds / units |
|---|---|---|
| `schemaVersion` | required | exactly 1 |
| `id` | required | 1–64 ASCII letters/digits, `_` or `-`; filename-safe |
| `category` | `interface` | same identifier rules |
| `description` | empty | at most 4096 bytes; no NUL |
| `sampleRate` | 44100 | integer 8000–96000 Hz |
| `channels` | 2 | 1 or 2 |
| `duration` | required | 0.001–30 seconds |
| `seed` | 12031 | integer 0–4294967295 |
| `outputGainDb` | -6 | -80 to +12 dB |
| `peakLimit` | 0.95 | 0.01–0.99 linear amplitude |
| `layers` | required | 1–32 entries, at most 32 million active layer frames including loop continuation |
| `loop` | disabled | table described below |

PCM allocation including loop continuation is capped at 64 MiB. Output frame count
is `round(duration * sampleRate)`; layers use the same rounding and are clipped to
that exact frame count. No implicit FX tail is appended. Lengthen the definition
and layer envelope explicitly for a longer tail. The C++ defaults are a valid
300 ms sine draft; the GUI's New template replaces it with a pluck.

### Layers

| Field | Default | Meaning / bounds |
|---|---|---|
| `type` | required | `oscillator`, `noise`, `pluck` |
| `enabled` | true | bypass without changing other layers' random streams |
| `start` | 0 | seconds; must leave at least one frame |
| `duration` | 0 | zero = remainder; otherwise ≥ one frame, contained within the sound |
| `gain` | 0.5 | 0–4 linear amplitude |
| `pan` | 0 | -1 left to +1 right; equal-power stereo; ignored in mono |
| `waveform` | `sine` | `sine`, `triangle`, `sawtooth`, `square`, `pulse` |
| `phase` | 0 | initial oscillator phase, 0–1 cycles |
| `duty` | 0.5 | pulse only, 0.05–0.95 |
| `color` | `white` | noise: `white`, `pink`, `brown` |
| `damping` | 0.996 | pluck feedback, 0.8–0.9999; higher means longer resonance |

`frequency = { startHz=440, endHz=440, curve="exponential" }`: absent `endHz`
uses `startHz`. Curves are `linear` or `exponential`. Both endpoints including pitch
modulation must stay between 20 Hz and 0.45 × sample rate; pluck's upper bound is
0.125 × sample rate. Phase is integrated through sweeps. Saw/square/pulse use PolyBLEP;
triangle sums up to 16 odd harmonics below Nyquist. This reduces aliasing but is
not a general oversampled synthesizer; extreme pulse widths/pitches can sound thin.

`envelope = { attack=.001, decay=.08, sustain=0, release=.05, exponential=true }`:
times are 0–30 seconds, sustain is 0–1. Decay moves from peak to sustain; release
occupies the final portion of the layer. If attack + decay + release exceeds its
duration, all three times shrink proportionally. Exponential decay uses a normalized
`exp(-5*x)` curve and a squared release; otherwise both are linear. Zero attack/decay
starts at sustain. Use `decay=0, sustain=1` for a string's natural damping. Global
one-shots receive an additional ≤1 ms onset/ending ramp; use envelopes on offset
layers to avoid abrupt internal edges.

`filter = { type="none", cutoffHz=2000, q=.707 }`: `none`, `lowpass`, `highpass`,
`bandpass`; cutoff 10 Hz–0.45 × sample rate; Q 0.1–10. RBJ-style biquads use double
state and bounded coefficients. A higher-Q bandpass on a transient supplies a simple
metal/wood resonance. Disabling a filter retains its validated parameters.

`modulation = { rateHz=5, pitchCents=0, amplitude=0 }`: sinusoidal rate 0–20 Hz,
pitch depth 0–1200 cents, amplitude depth 0–1. AM varies between `1-depth` and 1.
Pitch modulation affects oscillator/pluck; AM applies to every generator. Parameter
validation also checks currently unused/bypassed settings to catch latent errors.

Pluck uses noise excitation, a fractional delay line, a two-point averaging loss
filter and feedback damping (Karplus–Strong). Sweeping the delay approximates tuning;
it is not a physical model of a guitar body, pick, fretboard or amplifier.

### Ambient loops

```lua
loop = { enabled = true, begin = 0, ["end"] = 6, crossfade = 0.16 }
-- For each continuous layer:
envelope = { attack = 0, decay = 0, sustain = 1, release = 0 }
```

`end` is a Lua keyword, so use `["end"]`, not `end =`. If a loop table is present,
`enabled` defaults to true. `begin` defaults to 0 and `end=0` means the complete
duration. Bounds must describe at least two frames. Crossfade defaults to 20 ms;
choose zero or at least two frames, at most one second and half the loop length.

Internal bounds are `[begin,end)` in frames; existing WAV serialization writes the
RIFF `smpl` inclusive end as `end-1`. SFX does **not** call the Fail Synth's
`bakeLoop()`, which trims the buffer. Instead it continues the generators beyond
the output end, and blends that continuation into the loop head with a raised-cosine
crossfade. The seam therefore joins consecutive generator samples, including for
noise. For partial loops, the intro also crossfades into the corresponding pre-end
region. Neither output duration nor loop bounds are shortened. The dedicated
post-roll is discarded after crossfading. Final finite layers are not restarted;
a loop over a silent/released layer will remain silent. Use sustained layers for
beds. Crossfade can introduce a small level dip for uncorrelated noise. Zero
crossfade intentionally performs no seam repair. Stop is immediate; release-on-stop
and a separate playback fade are not implemented.

## Numerical behavior, ownership and determinism

`src/audio/sfx.h` contains independent typed definitions. `sfx.cpp` validates and
renders to the existing `PcmBuffer`, reusing `levels()` and `normalize()` for peak
protection. `sfx_lua.cpp` loads/saves/discovers recipes and stages exports around
`writeWav()`. No heavy processing is added to gameplay or scene hooks.

Noise uses xorshift32 with shifts 13, 17, 5. Each layer starts with
`seed XOR (0x9e3779b9 * (index+1))`, uint32 wraparound; zero state maps to
`0x6d2b79f5`. Its upper 24 bits map to `bits/8388608 - 1`. Reordering layers changes
random streams; bypassing a layer does not change the others. Pink uses a fixed
seven-state colored-noise filter; brown uses a leaky integrator. A 10 Hz DC blocker
runs on the final channels before loop processing. Final gain is applied globally.
If the measured peak exceeds the ceiling, the whole buffer is attenuated uniformly.
Quiet sounds are never boosted. Relative dynamics between layers are preserved.

The same recipe/seed/generator in the same runtime environment reproduces PCM.
There is no `random_device`. Binary equality across platforms, compiler flags,
libm implementations or changed generator versions is **not guaranteed**. Mathematical
functions and floating-point rounding differ. Do not use fast-math to claim parity.

Each load has its own Lua state, no standard libraries, no gameplay API, text-only
loading, 256 KiB input, 8 MiB Lua allocations and a 100,000-instruction limit. Tables
are read with raw lookups; no metamethods run during validation. Loading returns a
new validated object; caller state is changed only on success. This is a bounded
recipe interpreter, not an extension/plugin runtime.

Audio Studio transfers copied drafts through its existing job mutex/revision
protocol. The worker owns intermediate buffers, waveform calculation, preview WAV
serialization and export;
only completed immutable `SfxProduct` pointers reach the UI. Input controls and
SFX playback handles belong to the control thread. The transport shares the
prepared immutable WAV bytes; Play does not rescan or copy PCM. Keeping the preview
ready costs one additional float WAV buffer per generated result, retained across
device changes. Cancellation is checked every
4096 rendered frames and between stages. Existing fail-synth and SFX jobs share
one worker; a new job supersedes the previous one. WAV serialization/file writing
is an indivisible bounded stage, with a cancellation check before final publication.
There is a very small commit window after that check in which a complete export
can still finish. Temporary files are removed on cancellation/failure; earlier
completed results remain usable.

Recipe saves and WAV exports stage a sibling temporary file. POSIX no-replace
publication uses a hard link (a filesystem without hard-link support fails safely);
explicit replacement uses rename. Windows uses `MoveFileExW`, with replacement
only when authorized. This avoids Windows' `std::filesystem::rename` replacement
limitation and does not require NTFS hard links. The synth adds no BASS FX of its
own: layer filters are portable CPU DSP. A future optional master FX pass may reuse
`processFx()` with explicit BASS initialization and tail/loop rules.

## Authored library and packaging

All 17 recipes are in `data/sounds/specs/`; filenames equal their IDs.
The CLI checks the complete batch for case-insensitive ID collisions before writing
any output. Every colliding recipe fails, even with `--force`; unrelated valid
recipes can still export. This applies on Linux too, so libraries remain portable
to filesystems that do not distinguish filename case.

| Category | IDs / character |
|---|---|
| interface | `ui_navigate` muted pick, `ui_confirm` open fifth, `ui_back` descending slide, `ui_toggle` pedal latch, `ui_error` muted dissonance |
| garage | `garage_amp_idle_hum` 6 s / 60 Hz loop, `garage_room_tone` 7 s filtered room bed, `garage_amp_switch` rocker, `garage_cable_connect` jack/pop, `garage_guitar_tuning` settling low E |
| gameplay | `gameplay_countdown_tick` dry stick, `combo_milestone` high string accent |
| plus | `plus_charge` small pulse, `plus_ready` open chord, `plus_activate` falling discharge/thump, `plus_end` fading buzz |
| results | `result_success` staggered E major closure |

These are authored synthetic approximations; numerical tests do not establish
realism or pleasantness. Audition the WAVs at matched volume and in music before
shipping. Environmental presets are deliberately quiet. Raise monitoring volume
carefully rather than normalizing every file to the same peak.

```sh
# Demonstration files, untracked and reproducible:
build/bin/gpp_sfxgen --all data/sounds/specs --output-dir build/sfx --force --json \
  > build/sfx-manifest.json
# Populate the runtime pack's separate generated directory:
cmake --build build --target gpp_sfx_library
# Stage authored specs and only their generated WAV allowlist:
cmake --install build --prefix build/sfx-package --component SfxAssets
```

`gpp_sfx_library` is opt-in, never part of player startup or the default game build.
It emits `build/bin/data/sounds/generated/<id>.wav`. With both `BUILD_SFXGEN=ON`
and `GPP_PACKAGE_RUNTIME=ON`, these generated WAVs also join the existing Runtime
component. Run the library target **before** packaging; absent WAVs are installation
errors. The target and installer both validate `id == filename stem`, using the
CLI's `--require-filename-id` option. After changing an ID, rename the recipe and
regenerate the library. The installer fails on mismatched IDs even if old WAVs are
still present. Normal CLI use permits recipes with arbitrary filenames unless
that option is supplied. Recipes are versioned; WAVs are rebuildable development artifacts.
No legacy filename or `GPPGame::loadBasicSounds()` reference is changed, and no
external recording is downloaded or copied.

## Future gameplay and soundscapes (not implemented)

Pre-render assets during development/packaging and prepare bounded BASS samples or
streams on a loading path. Runtime triggers should only select/play prepared audio;
no Lua recipe parsing, synthesis, allocations proportional to a sound, or FFT on
the judgment thread. Keep per-player gameplay state and musical clocks unchanged.
The existing `PreparedSample` pattern is suitable for short one-shots; loop beds
need a stream/transport that honors the WAV region. Centralize category volume in
Music, SFX, Ambience and Crowd buses, with per-category voice limits.

`CScene` already owns a Lua lifecycle table on the main/render thread and calls
`onLoaded`, `onSessionStarted`, `onSessionEnded`, `onUnloaded`, `onSongStarted`,
`onSongEnded` at transitions. Context includes scene/song IDs and song seconds;
song-end includes reason and player results. The current `garage.lua` declares
visual assets and optional lifecycle hooks. None of those hooks is changed here.

A future small soundscape controller could:

- Prepare/fade in its bed at `onLoaded`, crossfade old/new scenes, then release
  handles at `onUnloaded` after fades finish.
- Enable bounded occasional events while idle, with monotonic scheduling, minimum
  intervals, probabilities, limited pitch/volume variation and per-event caps.
  For example, tuning every 25–70 seconds at probability 0.6 and a switch every
  40–110 seconds at probability 0.3. Skip missed events; do not catch up in bursts.
- Lower Ambience/Crowd at `onSongStarted`, restore after `onSongEnded`, and use
  session transitions for setup/cleanup across multiple songs. Handle aborts and
  unloads, not only successful results. Avoid adding a per-frame Lua callback for
  every emitter; the controller can maintain its own bounded scheduling state.
- Map existing crowd approval to indifferent/attentive/animated/excited textures
  with hysteresis and fades. Use sparse cheers/applause at results, never per-note.
  Human crowd realism likely needs clearly licensed recordings combined with
  synthetic textures and event variation; white noise alone is not a convincing
  audience. No crowd recordings or crowd synthesis are included in this change.

Current limits: no OGG encoder, no realtime synth, no soundscape runtime, no
automatic gameplay wiring, no master BASS FX editor for SFX, no undo, and no claim
of physically realistic guitars/applause. The initial library needs human listening
and in-game mix evaluation; audible hardware latency is not measured by No Sound.


## Implementation validation (Linux, 2026-10-31)

The complete configured build passed with `GPP_NATIVE_CPU=OFF`. NativeLab ran
`ctest --test-dir build --output-on-failure --parallel 4`: 322/323 tests passed,
including all nine audio/SFX tests and the shader/font suites with Xvfb/Mesa.
`chart_studio_ui` did not create its expected project after Ctrl+S in that run;
an isolated repeat passed without code changes. The initial full log is retained
as `build/sfx-full-ctest.log`. The dedicated
UI test uses actual ImGui mouse/keyboard input to edit seed, regenerate, play/stop,
add/remove layers, duplicate/save Lua, filter/open an ambient recipe, and export
float32 WAV with loop metadata. It also exercises save/discard/cancel on an unsaved
draft and recovery from a refused save. Captures are in
`build/audio-studio/sfx-ui-tests/`; they were also visually inspected.

A separate Debug build with `-fsanitize=address,undefined -fno-omit-frame-pointer`
ran `sfx_core`, `sfx_cli`, `sfx_package` and `audio_core` through NativeLab, with all four passing.
The vendor BASS binaries themselves are not instrumented. The CLI was separately
built with `BUILD_AUDIO_STUDIO=OFF`, validated all recipes, generated the runtime
library and staged 17 recipes plus 17 WAVs with the `SfxAssets` install component.
Demonstration WAVs and level/fingerprint reports are in `build/sfx/` and
`build/sfx-manifest.json`. CI now regenerates and uploads these demonstrations.

A headless No Sound RPC measurement using 30 s / 96 kHz / stereo PCM recorded
4.99–5.13 ms for three first Play calls after regeneration, versus 42–46 ms before
moving WAV serialization to the worker. The RPC round trip includes the headless
loop's 5 ms polling interval; it is not an audible-latency measurement. The current
samples are in `build/sfx-preview-timing.json`.

This validation covers numeric output, device-free synthesis/export and No Sound
transport behavior. It does not constitute listening evaluation, physical output
latency measurement, a Windows build/run, or a low-end hardware benchmark. Audible
preview and musical mix quality require human evaluation on an output device.
