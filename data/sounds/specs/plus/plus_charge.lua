-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "plus_charge",
    category = "plus",
    description = "Small rising pulse with a restrained electrical texture.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.13,
    seed = 401,
    outputGainDb = -9,
    peakLimit = 0.95,
    layers = {
        {
            type = "oscillator",
            waveform = "triangle",
            frequency = { startHz = 280, endHz = 720, curve = "exponential" },
            gain = 0.4,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0.001, decay = 0.075, sustain = 0, release = 0.03 }
        },
        {
            type = "noise",
            color = "pink",
            gain = 0.16,
            start = 0,
            duration = 0.07,
            filter = { type = "bandpass", cutoffHz = 1700, q = 0.707 },
            envelope = { attack = 0.001, decay = 0.04, sustain = 0, release = 0.02 }
        }
    }
}
