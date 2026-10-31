-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "plus_ready",
    category = "plus",
    description = "Recognizable high open chord and a short charged relay ring.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.48,
    seed = 402,
    outputGainDb = -5,
    peakLimit = 0.95,
    layers = {
        {
            type = "pluck",
            frequency = { startHz = 329.63, endHz = 329.63 },
            gain = 0.65,
            start = 0,
            duration = 0,
            pan = -0.35,
            damping = 0.998,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "pluck",
            frequency = { startHz = 493.88, endHz = 493.88 },
            gain = 0.55,
            start = 0.018,
            duration = 0,
            pan = 0,
            damping = 0.998,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "pluck",
            frequency = { startHz = 659.25, endHz = 659.25 },
            gain = 0.4,
            start = 0.035,
            duration = 0,
            pan = 0.35,
            damping = 0.998,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 1320, endHz = 1760, curve = "exponential" },
            gain = 0.12,
            start = 0.04,
            duration = 0.2,
            pan = 0,
            envelope = { attack = 0.002, decay = 0.12, sustain = 0, release = 0.04 }
        },
        {
            type = "noise",
            color = "white",
            gain = 0.12,
            start = 0,
            duration = 0.035,
            filter = { type = "lowpass", cutoffHz = 3200, q = 0.707 },
            envelope = { attack = 0.001, decay = 0.021, sustain = 0, release = 0.01 }
        }
    }
}
