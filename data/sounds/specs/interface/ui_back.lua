-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "ui_back",
    category = "interface",
    description = "Soft descending slide with the bass removed.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.14,
    seed = 103,
    outputGainDb = -10,
    peakLimit = 0.95,
    layers = {
        {
            type = "oscillator",
            waveform = "triangle",
            frequency = { startHz = 720, endHz = 310, curve = "exponential" },
            gain = 0.38,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0.003, decay = 0.08, sustain = 0, release = 0.03 },
            filter = { type = "highpass", cutoffHz = 240, q = 0.707 }
        },
        {
            type = "pluck",
            frequency = { startHz = 246.94, endHz = 246.94 },
            gain = 0.16,
            start = 0,
            duration = 0,
            pan = 0,
            damping = 0.92,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        }
    }
}
