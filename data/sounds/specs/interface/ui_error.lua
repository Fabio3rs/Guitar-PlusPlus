-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "ui_error",
    category = "interface",
    description = "Muted minor-second strings; distinct from the open confirmation.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.22,
    seed = 105,
    outputGainDb = -8,
    peakLimit = 0.95,
    layers = {
        {
            type = "pluck",
            frequency = { startHz = 146.83, endHz = 146.83 },
            gain = 0.6,
            start = 0,
            duration = 0,
            pan = -0.15,
            damping = 0.96,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "pluck",
            frequency = { startHz = 155.56, endHz = 155.56 },
            gain = 0.5,
            start = 0,
            duration = 0,
            pan = 0.15,
            damping = 0.95,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 293.66, endHz = 196, curve = "exponential" },
            gain = 0.12,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0.006, decay = 0.11, sustain = 0, release = 0.04 }
        }
    }
}
