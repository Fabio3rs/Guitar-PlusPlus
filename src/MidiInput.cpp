#include "MidiInput.h"
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace gpp::midi {
using Bytes = std::vector<unsigned char>;
namespace {
void require(bool ok, const char *message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}
} // namespace
// Upstream deliberately trusts chunk lengths, permits missing EOT, allocates
// from untrusted lengths and accumulates ticks in int. Check those boundaries
// before read(); this pass stores no musical events and performs no decoding
// into game data. Limits comfortably exceed normal multi-instrument charts.
int validateSmf(const Bytes &bytes) {
    size_t pos = 0, end = bytes.size(), events = 0;
    auto byte = [&]() -> unsigned {
        require(pos < end, "Truncated MIDI data");
        return bytes[pos++];
    };
    auto integer = [&](int count) {
        uint32_t value = 0;
        for (int i = 0; i < count; ++i) {
            value = (value << 8) | byte();
        }
        return value;
    };
    auto vlq = [&]() {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const auto b = byte();
            value = (value << 7) | (b & 127);
            if (b < 128) {
                return value;
            }
        }
        throw std::runtime_error("MIDI VLQ exceeds four bytes");
    };
    require(integer(4) == 0x4d546864, "Not a Standard MIDI File (MThd)");
    require(integer(4) == 6, "Unsupported MIDI header length");
    const auto type = integer(2), tracks = integer(2), ppq = integer(2);
    require(type <= 1,
            "SMF Type 2 has independent timelines and is unsupported");
    require(tracks > 0 && tracks <= 256 && (type != 0 || tracks == 1),
            "Invalid or excessive MIDI track count");
    require(ppq > 0 && ppq < 0x8000,
            "MIDI requires positive PPQ; SMPTE is unsupported");
    for (unsigned track = 0; track < tracks; ++track) {
        require(integer(4) == 0x4d54726b, "Missing MIDI track chunk (MTrk)");
        const auto length = integer(4);
        require(length <= bytes.size() - pos, "Truncated MIDI track chunk");
        end = pos + length;
        unsigned running = 0;
        uint64_t tick = 0;
        bool ended = false;
        while (pos < end) {
            require(++events <= 2000000, "MIDI event budget exceeded");
            tick += vlq();
            require(tick <=
                        static_cast<uint64_t>(std::numeric_limits<int>::max()),
                    "MIDI tick exceeds midifile integer range");
            unsigned status = byte();
            if (status < 128) {
                require(running >= 0x80 && running < 0xf0,
                        "Invalid MIDI running status");
                --pos;
                status = running;
            }
            if (status < 0xf0) {
                running = status;
                const int size = ((status & 0xe0) == 0xc0) ? 1 : 2;
                for (int i = 0; i < size; ++i) {
                    require(byte() < 128, "Invalid MIDI channel data");
                }
            } else {
                running = 0;
                require(status == 0xff || status == 0xf0 || status == 0xf7,
                        "Unsupported system status in SMF");
                const auto meta = status == 0xff ? byte() : 0;
                const auto size = vlq();
                require(size <= end - pos, "Truncated MIDI event payload");
                if (status == 0xff && meta == 0x51) {
                    require(size == 3, "Invalid tempo payload");
                    require((bytes[pos] | bytes[pos + 1] | bytes[pos + 2]) != 0,
                            "Zero MIDI tempo");
                }
                if (status == 0xff && meta == 0x58) {
                    require(size == 4, "Invalid time signature payload");
                }
                pos += size;
                if (status == 0xff && meta == 0x2f) {
                    require(size == 0 && pos == end,
                            "Invalid end-of-track boundary");
                    ended = true;
                }
            }
        }
        require(ended, "Missing MIDI end-of-track event");
        end = bytes.size();
    }
    require(pos == bytes.size(), "Unexpected data after MIDI tracks");
    return static_cast<int>(ppq);
}

} // namespace gpp::midi
