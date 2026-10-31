-- Authored for Guitar++: procedural sources only, no recorded samples.
return {
    schemaVersion = 1,
    id = "garage_amp_idle_hum",
    category = "garage",
    description = "Quiet 60 Hz amp hum, transformer harmonics and filtered hiss. Change all harmonic frequencies together.",
    sampleRate = 44100,
    channels = 2,
    duration = 6,
    seed = 201,
    outputGainDb = -25,
    peakLimit = 0.95,
    loop = {
        enabled = true,
        begin = 0,
        ["end"] = 6,
        crossfade = 0.16
    },
    layers = {
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 60, endHz = 60, curve = "exponential" },
            gain = 0.35,
            start = 0,
            duration = 0,
            pan = 0,
            envelope = { attack = 0, decay = 0, sustain = 1, release = 0 },
            modulation = { rateHz = 0.37, amplitude = 0.08 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 120, endHz = 120, curve = "exponential" },
            gain = 0.07,
            start = 0,
            duration = 0,
            pan = -0.15,
            envelope = { attack = 0, decay = 0, sustain = 1, release = 0 }
        },
        {
            type = "oscillator",
            waveform = "sine",
            frequency = { startHz = 180, endHz = 180, curve = "exponential" },
            gain = 0.025,
            start = 0,
            duration = 0,
            pan = 0.15,
            envelope = { attack = 0, decay = 0, sustain = 1, release = 0 }
        },
        {
            type = "noise",
            color = "pink",
            gain = 0.1,
            start = 0,
            duration = 0,
            filter = { type = "lowpass", cutoffHz = 1600, q = 0.707 },
            envelope = { attack = 0, decay = 0, sustain = 1, release = 0 },
            modulation = { rateHz = 0.23, amplitude = 0.16 }
        }
    }
}
