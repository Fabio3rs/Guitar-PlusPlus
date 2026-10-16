#include "midi_synth.h"
#include "MidiInput.h"
#include <MidiFile.h>
#include <algorithm>
#include <fstream>
#include <set>
#include <tuple>

namespace ChartStudio {
std::vector<SynthStem> synthSelection(const State &s, const std::string &mode) {
    std::vector<SynthStem> result;
    std::set<std::tuple<int, int, int>> seen;
    auto add = [&](const std::string &name, SynthSource source) {
        if (!seen.emplace(source.track, source.channel, source.program)
                 .second) {
            return;
        }
        auto it =
            std::find_if(result.begin(), result.end(),
                         [&](const SynthStem &v) { return v.name == name; });
        if (it == result.end()) {
            result.push_back({name, true, {source}});
        } else {
            it->sources.push_back(source);
        }
    };
    if (mode == "chart") {
        for (const auto *p : parts(s)) {
            if (s.merges.count(p->partId)) {
                const auto &merge = s.merges.at(p->partId);
                for (const auto *source : {&merge.primary, &merge.secondary}) {
                    add(p->settings.instrument,
                        {source->settings.track, source->settings.channel,
                         source->settings.program});
                }
                continue;
            }
            add(p->settings.instrument,
                {p->settings.track, p->settings.channel, p->settings.program});
        }
    } else if (mode == "all") {
        for (size_t t = 0; t < s.score.tracks.size(); ++t) {
            std::set<int> channels;
            for (const auto &n : s.score.tracks[t].notes) {
                channels.insert(n.channel);
            }
            for (int ch : channels) {
                add("track" + std::to_string(t) + "_ch" + std::to_string(ch),
                    {int(t), ch, -1});
            }
        }
    } else if (mode != "custom") {
        throw std::runtime_error("Instruments must be chart, all or custom");
    }
    return result;
}

SynthPlan synthPlan(const State &s, const SynthStem &stem,
                    const SynthConfig &c) {
    if (c.sampleRate < 8000 || c.sampleRate > 192000 ||
        !std::isfinite(c.tail) || c.tail < 0 || c.tail > 30) {
        throw std::runtime_error(
            "Sample rate must be 8000..192000; tail 0..30 seconds");
    }
    if (fileHash(fs::u8path(s.input)) != s.inputHash) {
        throw std::runtime_error(
            "Original MIDI has changed; reopen before synthesizing");
    }
    std::ifstream in(fs::u8path(s.input), std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(in)), {});
    gpp::midi::validateSmf(
        std::vector<unsigned char>(bytes.begin(), bytes.end()));
    std::istringstream stream(bytes, std::ios::binary);
    smf::MidiFile midi;
    if (!midi.read(stream)) {
        throw std::runtime_error("Cannot read original MIDI");
    }
    midi.linkNotePairs();
    struct Raw {
        int track, order;
        int64_t tick;
        const smf::MidiEvent *event;
        std::vector<unsigned char> bytes;
    };
    std::vector<Raw> raw;
    int64_t last = 0;
    std::set<std::pair<int, int>> noteChannels;
    for (int t = 0; t < midi.getTrackCount(); ++t) {
        for (int i = 0; i < midi[t].size(); ++i) {
            const auto &e = midi[t][i];
            last = std::max(last, int64_t(e.tick));
            if (!e.empty() && e[0] >= 0x80 && e[0] < 0xf0) {
                raw.push_back({t, i, e.tick, &e, {e.begin(), e.end()}});
                if ((e[0] & 0xf0) == 0x90 && e.size() > 2 && e[2]) {
                    noteChannels.emplace(t, e[0] & 15);
                }
            }
        }
    }
    std::stable_sort(raw.begin(), raw.end(), [](const Raw &a, const Raw &b) {
        return std::tie(a.tick, a.track, a.order) <
               std::tie(b.tick, b.track, b.order);
    });
    std::vector<bool> matched(stem.sources.size(), false);
    for (const auto &source : stem.sources) {
        if (source.track < 0 || source.track >= midi.getTrackCount() ||
            source.channel < -1 || source.channel > 15 || source.program < -1 ||
            source.program > 127) {
            throw std::runtime_error("Invalid MIDI source selection");
        }
    }
    SynthPlan plan;
    const double end = std::max(0.0, audioSeconds(s, last)) + c.tail;
    if (!std::isfinite(end) || end * c.sampleRate > 500000000) {
        throw std::runtime_error("Stem exceeds supported WAV length");
    }
    plan.frames = std::llround(end * c.sampleRate);
    // One virtual channel per source track/channel: shared channel numbers
    // cannot steal voices.
    for (auto pair : noteChannels) {
        std::vector<SynthSource> selected;
        for (auto source : stem.sources) {
            if (source.track == pair.first &&
                (source.channel < 0 || source.channel == pair.second)) {
                selected.push_back(source);
            }
        }
        if (selected.empty()) {
            continue;
        }
        const int channel = int(plan.drums.size());
        plan.drums.push_back(pair.second == 9);
        int program = 0;
        std::set<const smf::MidiEvent *> selectedOffs;
        size_t ordinal = 0;
        for (const auto &r : raw) {
            const auto order = ordinal++;
            int status = r.bytes[0] & 0xf0, ch = r.bytes[0] & 15;
            if (ch != pair.second) {
                continue;
            }
            // Controller-only conductor tracks supply shared channel state.
            if (r.track != pair.first && noteChannels.count({r.track, ch})) {
                continue;
            }
            int a = r.bytes.size() > 1 ? r.bytes[1] : 0,
                b = r.bytes.size() > 2 ? r.bytes[2] : 0;
            if (status == 0xc0) {
                program = a;
            }
            if (status == 0x90 && b) {
                if (r.track != pair.first) {
                    continue;
                }
                bool allowed = std::any_of(
                    selected.begin(), selected.end(), [&](SynthSource v) {
                        return v.program < 0 || v.program == program;
                    });
                if (!allowed) {
                    continue;
                }
                for (size_t index = 0; index < stem.sources.size(); ++index) {
                    const auto &source = stem.sources[index];
                    if (source.track == pair.first &&
                        (source.channel < 0 || source.channel == ch) &&
                        (source.program < 0 || source.program == program)) {
                        matched[index] = true;
                    }
                }
                // Keep the importer's FIFO association even when intervening
                // Note Ons of the same pitch are excluded by program filters.
                if (const auto *off = r.event->getLinkedEvent()) {
                    selectedOffs.insert(off);
                }
            } else if (status == 0x80 || status == 0x90) {
                if (r.track != pair.first || !selectedOffs.erase(r.event)) {
                    continue;
                }
            }
            plan.events.push_back(
                {std::llround(audioSeconds(s, r.tick) * c.sampleRate), channel,
                 status, a, b, order});
        }
    }
    for (size_t index = 0; index < matched.size(); ++index) {
        if (!matched[index]) {
            const auto &source = stem.sources[index];
            throw std::runtime_error(
                "Stem '" + stem.name +
                "': no Note On matches track=" + std::to_string(source.track) +
                " channel=" + std::to_string(source.channel) +
                " program=" + std::to_string(source.program));
        }
    }
    if (plan.drums.empty()) {
        throw std::runtime_error("Stem has no musical source");
    }
    if (plan.drums.size() > 256) {
        throw std::runtime_error("At most 256 track/channel sources per stem");
    }
    std::stable_sort(plan.events.begin(), plan.events.end(),
                     [](const SynthEvent &a, const SynthEvent &b) {
                         return std::tie(a.sample, a.order, a.channel) <
                                std::tie(b.sample, b.order, b.channel);
                     });
    return plan;
}
} // namespace ChartStudio
