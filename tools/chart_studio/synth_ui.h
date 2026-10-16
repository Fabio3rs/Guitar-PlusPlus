#pragma once
#include "midi_synth.h"
#include <future>
namespace ChartStudio {
class SynthPanel {
    std::atomic<bool> cancel{false};
    std::atomic<float> progress{0};
    std::future<void> job;
    std::string message;

  public:
    ~SynthPanel();
    void draw(Session &session);
};
} // namespace ChartStudio
