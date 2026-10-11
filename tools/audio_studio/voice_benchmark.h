#pragma once
#include "session.h"

namespace AudioStudio {
void validate(const VoiceBenchmarkOptions &options);
Json compareVoices(const audio::PcmBuffer &pcm,
                   const VoiceBenchmarkOptions &options,
                   audio::DeviceSettings device, float bufferSeconds,
                   const fs::path &outputDirectory,
                   const audio::Checkpoint &checkpoint);
Json loadVoiceReport(const fs::path &path);
int voiceProbe(const fs::path &config, const fs::path &pcm,
               const fs::path &report);
} // namespace AudioStudio
