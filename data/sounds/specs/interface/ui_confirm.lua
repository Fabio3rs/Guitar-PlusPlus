-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "ui_confirm",
    category = "interface",
    description = "Two bright open fifth strings, gently staggered.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.33,
    seed = 102,
    outputGainDb = -6,
    peakLimit = 0.95,
    layers = {
        {
            type = "pluck",
            frequency = { startHz = 329.63, endHz = 329.63 },
            gain = 0.7,
            start = 0,
            duration = 0,
            pan = -0.3,
            damping = 0.997,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "pluck",
            frequency = { startHz = 493.88, endHz = 493.88 },
            gain = 0.55,
            start = 0.025,
            duration = 0,
            pan = 0.3,
            damping = 0.997,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "noise",
            color = "white",
            gain = 0.04,
            start = 0,
            duration = 0.012,
            filter = { type = "lowpass", cutoffHz = 2800, q = 0.707 },
            envelope = { attack = 0.001, decay = 0.007, sustain = 0, release = 0.003 }
        }
    }
}
