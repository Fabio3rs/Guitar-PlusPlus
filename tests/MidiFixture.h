#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Independently encoded SMF bytes: tests do not use the parser to generate its
// own input, and never depend on third-party song files.
namespace midi_fixture {
inline void integer(std::string &out, uint32_t value, int count) {
    for (int i = count - 1; i >= 0; --i) {
        out.push_back(static_cast<char>((value >> (8 * i)) & 255));
    }
}

inline void vlq(std::string &out, uint32_t value) {
    unsigned char bytes[4];
    int count = 0;
    bytes[count++] = static_cast<unsigned char>(value & 127);
    while ((value >>= 7) != 0) {
        bytes[count++] = static_cast<unsigned char>((value & 127) | 128);
    }
    while (count > 0) {
        out.push_back(static_cast<char>(bytes[--count]));
    }
}

inline std::string data(std::initializer_list<unsigned> values) {
    std::string result;
    for (auto value : values) {
        result.push_back(static_cast<char>(value));
    }
    return result;
}

struct Track {
    struct Event {
        uint32_t tick;
        std::string bytes;
    };
    std::vector<Event> events;

    explicit Track(const std::string &name = "PART GUITAR") {
        meta(0, 3, name);
    }

    void meta(uint32_t tick, unsigned type, const std::string &value) {
        std::string bytes = data({255, type});
        vlq(bytes, static_cast<uint32_t>(value.size()));
        events.push_back({tick, bytes + value});
    }

    void tempo(uint32_t tick, uint32_t microseconds) {
        std::string value;
        integer(value, microseconds, 3);
        meta(tick, 0x51, value);
    }

    void note(uint32_t tick, uint32_t length, unsigned key,
              bool zeroOff = false, unsigned channel = 0) {
        events.push_back({tick, data({0x90 | channel, key, 100})});
        events.push_back({tick + length,
                          data({(zeroOff ? 0x90u : 0x80u) | channel, key, 0})});
    }

    void sysex(uint32_t tick, unsigned difficulty, unsigned phrase,
               bool start) {
        events.push_back({tick, data({0xf0, 8, 0x50, 0x53, 0, 0, difficulty,
                                      phrase, start ? 1u : 0u, 0xf7})});
    }

    std::string encode() const {
        auto sorted = events;
        std::stable_sort(
            sorted.begin(), sorted.end(),
            [](const auto &a, const auto &b) { return a.tick < b.tick; });
        std::string bytes;
        uint32_t last = 0;
        for (const auto &event : sorted) {
            vlq(bytes, event.tick - last);
            bytes += event.bytes;
            last = event.tick;
        }
        bytes += data({0, 255, 47, 0});
        std::string result = "MTrk";
        integer(result, static_cast<uint32_t>(bytes.size()), 4);
        return result + bytes;
    }
};

inline std::string smf(std::vector<Track> tracks, unsigned ppq = 480,
                       unsigned type = 1) {
    std::string result = "MThd";
    integer(result, 6, 4);
    integer(result, type, 2);
    integer(result, static_cast<uint32_t>(tracks.size()), 2);
    integer(result, ppq, 2);
    for (const auto &track : tracks) {
        result += track.encode();
    }
    return result;
}

inline void write(const std::filesystem::path &path, const std::string &data) {
    std::ofstream output(path, std::ios::binary);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
}
} // namespace midi_fixture
