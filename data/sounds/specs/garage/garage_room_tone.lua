-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "garage_room_tone",
    category = "garage",
    description = "Subtle enclosed-room equipment bed; no crowd or human recordings.",
    sampleRate = 44100,
    channels = 2,
    duration = 7,
    seed = 202,
    outputGainDb = -26,
    peakLimit = 0.95,
    loop = {
        enabled = true,
        begin = 0,
        ["end"] = 7,
        crossfade = 0.25
    },
    layers = {
        {
            type = "noise",
            color = "brown",
            gain = 0.34,
            start = 0,
            duration = 0,
            filter = { type = "lowpass", cutoffHz = 420, q = 0.707 },
            envelope = { attack = 0, decay = 0, sustain = 1, release = 0 },
            pan = -0.2,
            modulation = { rateHz = 0.13, amplitude = 0.25 }
        },
        {
            type = "noise",
            color = "pink",
            gain = 0.18,
            start = 0,
            duration = 0,
            filter = { type = "bandpass", cutoffHz = 850, q = 0.5 },
            envelope = { attack = 0, decay = 0, sustain = 1, release = 0 },
            pan = 0.25,
            modulation = { rateHz = 0.19, amplitude = 0.2 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 92, endHz = 92, curve = "exponential" },
            gain = 0.015,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0, decay = 0, sustain = 1, release = 0 }
        }
    }
}
