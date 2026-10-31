-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "plus_end",
    category = "plus",
    description = "Energy winds down with a soft falling buzz and a tiny residual contact.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.28,
    seed = 404,
    outputGainDb = -10,
    peakLimit = 0.95,
    layers = {
        {
            type = "oscillator",
            waveform = "triangle",
            frequency = { startHz = 640, endHz = 130, curve = "exponential" },
            gain = 0.32,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0.005, decay = 0.17, sustain = 0, release = 0.07 }
        },
        {
            type = "noise",
            color = "brown",
            gain = 0.3,
            start = 0,
            duration = 0,
            filter = { type = "lowpass", cutoffHz = 1800, q = 0.707 },
            envelope = { attack = 0.004, decay = 0.15, sustain = 0, release = 0.05 }
        },
        {
            type = "noise",
            color = "white",
            gain = 0.08,
            start = 0.205,
            duration = 0.025,
            filter = { type = "bandpass", cutoffHz = 2400, q = 0.707 },
            envelope = { attack = 0.001, decay = 0.013, sustain = 0, release = 0.008 }
        }
    }
}
