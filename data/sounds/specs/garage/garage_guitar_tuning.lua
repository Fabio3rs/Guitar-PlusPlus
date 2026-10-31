-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "garage_guitar_tuning",
    category = "garage",
    description = "Low E string gradually settles into tune with a slight pitch wobble.",
    sampleRate = 44100,
    channels = 2,
    duration = 1.8,
    seed = 205,
    outputGainDb = -5,
    peakLimit = 0.95,
    layers = {
        {
            type = "pluck",
            frequency = { startHz = 79.8, endHz = 82.407 },
            gain = 0.85,
            start = 0,
            duration = 0,
            pan = 0,
            damping = 0.999,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 },
            modulation = { rateHz = 3.8, pitchCents = 7 }
        },
        {
            type = "noise",
            color = "white",
            gain = 0.09,
            start = 0,
            duration = 0.027,
            filter = { type = "lowpass", cutoffHz = 2000, q = 0.707 },
            envelope = { attack = 0.0003, decay = 0.018, sustain = 0, release = 0.005 }
        }
    }
}
