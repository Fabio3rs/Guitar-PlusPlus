-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "gameplay_countdown_tick",
    category = "gameplay",
    description = "Precise dry stick-like click with a short woody resonance.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.07,
    seed = 301,
    outputGainDb = -6,
    peakLimit = 0.95,
    layers = {
        {
            type = "noise",
            color = "white",
            gain = 0.55,
            start = 0,
            duration = 0.022,
            filter = { type = "bandpass", cutoffHz = 2400, q = 1.3 },
            envelope = { attack = 0.0002, decay = 0.014, sustain = 0, release = 0.006 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 1240, endHz = 980, curve = "exponential" },
            gain = 0.35,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0.0004, decay = 0.035, sustain = 0, release = 0.02 }
        }
    }
}
