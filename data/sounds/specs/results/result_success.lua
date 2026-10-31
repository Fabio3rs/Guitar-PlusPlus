-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "result_success",
    category = "results",
    description = "Brief E major guitar voicing; staggered plucks close the performance.",
    sampleRate = 44100,
    channels = 2,
    duration = 1.25,
    seed = 501,
    outputGainDb = -5,
    peakLimit = 0.95,
    layers = {
        {
            type = "pluck",
            frequency = { startHz = 164.81, endHz = 164.81 },
            gain = 0.65,
            start = 0,
            duration = 0,
            pan = -0.35,
            damping = 0.9985,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "pluck",
            frequency = { startHz = 246.94, endHz = 246.94 },
            gain = 0.55,
            start = 0.025,
            duration = 0,
            pan = -0.1,
            damping = 0.9985,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "pluck",
            frequency = { startHz = 329.63, endHz = 329.63 },
            gain = 0.5,
            start = 0.05,
            duration = 0,
            pan = 0.1,
            damping = 0.9985,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "pluck",
            frequency = { startHz = 415.3, endHz = 415.3 },
            gain = 0.4,
            start = 0.075,
            duration = 0,
            pan = 0.35,
            damping = 0.9985,
            envelope = { attack = 0.0005, decay = 0, sustain = 1, release = 0.04 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 659.25, endHz = 659.25, curve = "exponential" },
            gain = 0.09,
            start = 0.09,
            duration = 0,
            pan = 0,
            envelope = { attack = 0.01, decay = 0.7, sustain = 0, release = 0.2 }
        }
    }
}
