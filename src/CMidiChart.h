#pragma once

#include "CChart.h"
#include <filesystem>
#include <map>

// A CPU-only input adapter. No MIDI playback, engine calls or shared state.
struct MidiChart {
    struct Instrument {
        std::vector<CChart::Note> notes;
        std::vector<CChart::plusNote> plus;
        ChartSourceInfo source;
    };

    std::string name, artist, charter;
    std::vector<CChart::Note> bpm;
    ChartSourceInfo source;
    std::vector<std::string> trackNames, identification;
    std::map<std::string, Instrument> instruments;
};

bool readMidiChart(const std::filesystem::path &path, MidiChart &result,
                   std::string &error);
