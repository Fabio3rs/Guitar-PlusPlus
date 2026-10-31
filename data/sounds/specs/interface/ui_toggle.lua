-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "ui_toggle",
    category = "interface",
    description = "Pedal latch with two tiny impacts and a metal ring.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.095,
    seed = 104,
    outputGainDb = -9,
    peakLimit = 0.95,
    layers = {
        {
            type = "noise",
            color = "white",
            gain = 0.5,
            start = 0,
            duration = 0.013,
            filter = { type = "bandpass", cutoffHz = 1800, q = 1.2 },
            envelope = { attack = 0.0003, decay = 0.007, sustain = 0, release = 0.004 }
        },
        {
            type = "noise",
            color = "white",
            gain = 0.24,
            start = 0.016,
            duration = 0.012,
            filter = { type = "lowpass", cutoffHz = 3400, q = 0.707 },
            envelope = { attack = 0.0003, decay = 0.007, sustain = 0, release = 0.003 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 2387, endHz = 2387, curve = "exponential" },
            gain = 0.15,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0.001, decay = 0.055, sustain = 0, release = 0.02 }
        }
    }
}
