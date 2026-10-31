-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "plus_activate",
    category = "plus",
    description = "Signature Plus discharge: falling electrical bite, low thump and a gritty short tail.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.8,
    seed = 12031,
    outputGainDb = -3,
    peakLimit = 0.95,
    layers = {
        {
            type = "oscillator",
            waveform = "sawtooth",
            frequency = { startHz = 1800, endHz = 85, curve = "exponential" },
            gain = 0.5,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0.001, decay = 0.35, sustain = 0, release = 0.15 },
            filter = { type = "lowpass", cutoffHz = 4300, q = 0.8 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 110, endHz = 45, curve = "exponential" },
            gain = 0.8,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0.001, decay = 0.32, sustain = 0, release = 0.16 }
        },
        {
            type = "noise",
            color = "pink",
            gain = 0.65,
            start = 0,
            duration = 0,
            filter = { type = "lowpass", cutoffHz = 2800, q = 0.707 },
            envelope = { attack = 0.0005, decay = 0.27, sustain = 0, release = 0.15 }
        },
        {
            type = "noise",
            color = "white",
            gain = 0.26,
            start = 0,
            duration = 0.055,
            filter = { type = "highpass", cutoffHz = 5200, q = 0.707 },
            envelope = { attack = 0.0003, decay = 0.028, sustain = 0, release = 0.02 }
        },
        {
            type = "pluck",
            frequency = { startHz = 164.81, endHz = 164.81 },
            gain = 0.38,
            start = 0.018,
            duration = 0,
            pan = -0.2,
            damping = 0.988,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "oscillator",
            waveform = "pulse",
            frequency = { startHz = 2740, endHz = 680, curve = "exponential" },
            gain = 0.11,
            start = 0,
            duration = 0,
            pan = 0.25,
            duty = 0.22,
            envelope = { attack = 0.001, decay = 0.18, sustain = 0, release = 0.1 }
        }
    }
}
