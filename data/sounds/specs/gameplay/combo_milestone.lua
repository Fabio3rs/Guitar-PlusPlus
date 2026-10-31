-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "combo_milestone",
    category = "gameplay",
    description = "Small high string harmonic accent for occasional combo milestones.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.27,
    seed = 302,
    outputGainDb = -10,
    peakLimit = 0.95,
    layers = {
        {
            type = "pluck",
            frequency = { startHz = 659.25, endHz = 659.25 },
            gain = 0.48,
            start = 0,
            duration = 0,
            pan = -0.2,
            damping = 0.997,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "pluck",
            frequency = { startHz = 987.77, endHz = 987.77 },
            gain = 0.3,
            start = 0.012,
            duration = 0,
            pan = 0.2,
            damping = 0.997,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 1318.5, endHz = 1318.5, curve = "exponential" },
            gain = 0.09,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0.001, decay = 0.11, sustain = 0, release = 0.05 }
        }
    }
}
