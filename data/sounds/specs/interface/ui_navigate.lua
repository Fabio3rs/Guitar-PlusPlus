-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "ui_navigate",
    category = "interface",
    description = "Quiet muted pick contact for repeated menu navigation.",
    sampleRate = 44100,
    channels = 2,
    duration = 0.052,
    seed = 101,
    outputGainDb = -12,
    peakLimit = 0.95,
    layers = {
        {
            type = "pluck",
            frequency = { startHz = 196, endHz = 196 },
            gain = 0.4,
            start = 0,
            duration = 0,
            pan = 0,
            damping = 0.85,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "noise",
            color = "white",
            gain = 0.12,
            start = 0,
            duration = 0.016,
            filter = { type = "bandpass", cutoffHz = 2300, q = 0.707 },
            envelope = { attack = 0.0003, decay = 0.009, sustain = 0, release = 0.004 }
        }
    }
}
