-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "garage_amp_switch",
    category = "garage",
    description = "Heavy amp rocker, plastic contact and short electrical crackle.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.16,
    seed = 203,
    outputGainDb = -8,
    peakLimit = 0.95,
    layers = {
        {
            type = "noise",
            color = "white",
            gain = 0.6,
            start = 0,
            duration = 0.025,
            filter = { type = "bandpass", cutoffHz = 900, q = 0.9 },
            envelope = { attack = 0.0004, decay = 0.013, sustain = 0, release = 0.008 }
        },
        {
            type = "noise",
            color = "white",
            gain = 0.26,
            start = 0.022,
            duration = 0.035,
            filter = { type = "lowpass", cutoffHz = 4000, q = 0.707 },
            envelope = { attack = 0.0005, decay = 0.021, sustain = 0, release = 0.008 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 680, endHz = 510, curve = "exponential" },
            gain = 0.18,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0.001, decay = 0.06, sustain = 0, release = 0.02 }
        }
    }
}
