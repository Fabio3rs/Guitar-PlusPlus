-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "garage_cable_connect",
    category = "garage",
    description = "Jack plug contact followed by a soft electrical pop and grounded ring.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.28,
    seed = 204,
    outputGainDb = -8,
    peakLimit = 0.95,
    layers = {
        {
            type = "noise",
            color = "white",
            gain = 0.4,
            start = 0,
            duration = 0.02,
            filter = { type = "bandpass", cutoffHz = 2200, q = 0.707 },
            envelope = { attack = 0.0005, decay = 0.012, sustain = 0, release = 0.005 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 145, endHz = 62, curve = "exponential" },
            gain = 0.38,
            start = 0.045,
            duration = 0.15,
            pan = 0,
            envelope = { attack = 0.001, decay = 0.08, sustain = 0, release = 0.04 }
        },
        {
            type = "noise",
            color = "pink",
            gain = 0.24,
            start = 0.047,
            duration = 0.1,
            filter = { type = "lowpass", cutoffHz = 3100, q = 0.707 },
            envelope = { attack = 0.001, decay = 0.06, sustain = 0, release = 0.02 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 1550, endHz = 1550, curve = "exponential" },
            gain = 0.07,
            start = 0.008,
            duration = 0.09,
            pan = 0,
            envelope = { attack = 0.001, decay = 0.045, sustain = 0, release = 0.02 }
        }
    }
}
