#include "midi_synth.h"
#include "synth_process.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <memory>
#include <set>
#ifdef GPP_HAVE_FLUIDSYNTH
#include <fluidsynth.h>
#endif
namespace ChartStudio {
bool synthAvailable() {
#ifdef GPP_HAVE_FLUIDSYNTH
    return true;
#else
    return false;
#endif
}
namespace {
void word(std::ostream &out, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        out.put(char((value >> (8 * i)) & 255));
    }
}
struct WorkDirectory {
    fs::path path;
    explicit WorkDirectory(const fs::path &parent) {
        for (int i = 0; i < 100; ++i) {
            path = parent / (".synth-" +
                             std::to_string(std::chrono::steady_clock::now()
                                                .time_since_epoch()
                                                .count()) +
                             "-" + std::to_string(i));
            if (fs::create_directory(path)) {
                return;
            }
        }
        throw std::runtime_error("Cannot reserve stem temporary directory");
    }
    ~WorkDirectory() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};
#ifdef GPP_HAVE_FLUIDSYNTH
void render(const SynthPlan &p, const SynthConfig &c, const fs::path &path,
            const std::function<void()> &check,
            const std::function<void(double)> &progress) {
    std::unique_ptr<fluid_settings_t, decltype(&delete_fluid_settings)>
        settings(new_fluid_settings(), delete_fluid_settings);
    if (!settings) {
        throw std::runtime_error("Cannot create FluidSynth settings");
    }
    fluid_settings_setnum(settings.get(), "synth.sample-rate", c.sampleRate);
    fluid_settings_setnum(settings.get(), "synth.gain", c.gain);
    fluid_settings_setint(settings.get(), "synth.midi-channels",
                          int((p.drums.size() + 15) / 16 * 16));
    fluid_settings_setint(settings.get(), "synth.cpu-cores", 1);
    fluid_settings_setint(settings.get(), "synth.threadsafe-api", 0);
    std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)> synth(
        new_fluid_synth(settings.get()), delete_fluid_synth);
    if (!synth || fluid_synth_sfload(synth.get(), c.soundfont.c_str(), 1) < 0) {
        throw std::runtime_error("Cannot load SoundFont .sf2");
    }
    for (size_t i = 0; i < p.drums.size(); ++i) {
        fluid_synth_set_channel_type(synth.get(), int(i),
                                     p.drums[i] ? CHANNEL_TYPE_DRUM
                                                : CHANNEL_TYPE_MELODIC);
        fluid_synth_program_change(synth.get(), int(i), 0);
    }
    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4);
    word(out, uint32_t(36 + p.frames * 4), 4);
    out.write("WAVEfmt ", 8);
    word(out, 16, 4);
    word(out, 1, 2);
    word(out, 2, 2);
    word(out, c.sampleRate, 4);
    word(out, c.sampleRate * 4, 4);
    word(out, 4, 2);
    word(out, 16, 2);
    out.write("data", 4);
    word(out, uint32_t(p.frames * 4), 4);
    std::array<int16_t, 2048> pcm{};
    int64_t cursor =
        p.events.empty() ? 0 : std::min(int64_t(0), p.events.front().sample);
    auto advance = [&](int64_t end) {
        while (cursor < end) {
            if (check) {
                check();
            }
            int count = int(std::min(int64_t(1024), end - cursor));
            if (cursor < 0) {
                count = int(std::min(int64_t(count), -cursor));
            }
            if (fluid_synth_write_s16(synth.get(), count, pcm.data(), 0, 2,
                                      pcm.data(), 1, 2) != FLUID_OK) {
                throw std::runtime_error("FluidSynth rendering failed");
            }
            if (cursor >= 0) {
                for (int i = 0; i < count * 2; ++i) {
                    word(out, uint16_t(pcm[i]), 2);
                }
            }
            cursor += count;
            if (progress) {
                progress(double(std::max(int64_t(0), cursor)) /
                         double(std::max(int64_t(1), p.frames)));
            }
            if (!out) {
                throw std::runtime_error("Cannot write stem WAV");
            }
        }
    };
    for (const auto &e : p.events) {
        advance(e.sample);
        switch (e.status) {
        case 0x80:
            fluid_synth_noteoff(synth.get(), e.channel, e.a);
            break;
        case 0x90:
            if (fluid_synth_noteon(synth.get(), e.channel, e.a, e.b) !=
                    FLUID_OK &&
                e.b != 0) {
                throw std::runtime_error(
                    "FluidSynth could not start a note; check the SoundFont "
                    "presets and polyphony");
            }
            break;
        case 0xa0:
            fluid_synth_key_pressure(synth.get(), e.channel, e.a, e.b);
            break;
        case 0xb0:
            fluid_synth_cc(synth.get(), e.channel, e.a, e.b);
            break;
        case 0xc0:
            fluid_synth_program_change(synth.get(), e.channel, e.a);
            break;
        case 0xd0:
            fluid_synth_channel_pressure(synth.get(), e.channel, e.a);
            break;
        case 0xe0:
            fluid_synth_pitch_bend(synth.get(), e.channel, e.a + (e.b << 7));
            break;
        default:
            break;
        }
    }
    advance(std::max(int64_t(0),
                     p.frames - int64_t(std::llround(c.tail * c.sampleRate))));
    for (size_t i = 0; i < p.drums.size(); ++i) {
        fluid_synth_cc(synth.get(), int(i), 64, 0);
        fluid_synth_all_notes_off(synth.get(), int(i));
    }
    advance(p.frames);
    out.close();
    if (!out) {
        throw std::runtime_error("Cannot finish WAV");
    }
}
#endif
} // namespace
void renderStems(const State &s, const SynthConfig &c,
                 const std::function<void()> &check,
                 const std::function<void(double)> &progress) {
#ifndef GPP_HAVE_FLUIDSYNTH
    (void)s;
    (void)c;
    (void)check;
    (void)progress;
    throw std::runtime_error("MIDI synthesis unavailable: rebuild Chart Studio "
                             "with FluidSynth 2 and GPP_ENABLE_MIDI_SYNTH=ON");
#else
    if (c.output.empty() || c.soundfont.empty()) {
        throw std::runtime_error("Select output directory and SoundFont");
    }
    if (c.format != "wav" && c.format != "ogg") {
        throw std::runtime_error("Stem format must be wav or ogg");
    }
    if (!std::isfinite(c.gain) || c.gain < 0 || c.gain > 2 || c.quality < 0 ||
        c.quality > 10) {
        throw std::runtime_error("Gain must be 0..2; Vorbis quality 0..10");
    }
    if (c.mode != "chart" && c.mode != "all" && c.mode != "custom") {
        throw std::runtime_error("Invalid synthesis selection mode");
    }
    auto stems = c.stems.empty() ? synthSelection(s, c.mode) : c.stems;
    std::set<std::string> names;
    std::vector<SynthStem> enabled;
    auto output = fs::absolute(fs::u8path(c.output));
    fs::create_directories(output);
    for (const auto &stem : stems) {
        if (!stem.enabled) {
            continue;
        }
        if (stem.name.empty() || stem.name == "." || stem.name == ".." ||
            stem.name.find_first_of("/\\:") != std::string::npos) {
            throw std::runtime_error(
                "Stem name must be a simple filename without extension");
        }
        auto destination = output / fs::u8path(stem.name + "." + c.format);
        auto lower = stem.name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) { return char(std::tolower(ch)); });
        if (lower == "song" || !names.insert(lower).second ||
            fs::exists(destination)) {
            throw std::runtime_error(
                "Stem destination exists, duplicates another stem, or is "
                "reserved song audio: " +
                destination.u8string());
        }
        enabled.push_back(stem);
    }
    if (enabled.empty()) {
        throw std::runtime_error("No enabled stems");
    }
    WorkDirectory temp(output);
    for (size_t i = 0; i < enabled.size(); ++i) {
        if (check) {
            check();
        }
        const auto plan = synthPlan(s, enabled[i], c);
        auto wav = temp.path / (std::to_string(i) + ".wav");
        render(plan, c, wav, check, [&](double fraction) {
            if (progress) {
                progress((double(i) + fraction) / double(enabled.size()));
            }
        });
        if (c.format == "ogg") {
            synthProcess(
                {"ffmpeg", "-nostdin", "-v", "error", "-n", "-i",
                 wav.u8string(), "-c:a", "libvorbis", "-q:a",
                 std::to_string(c.quality),
                 (temp.path / (std::to_string(i) + ".ogg")).u8string()},
                check);
        }
    }
    if (check) {
        check();
    }
    // Publish only after every stem succeeds. Never overwrite originals.
    std::vector<fs::path> published;
    try {
        for (size_t i = 0; i < enabled.size(); ++i) {
            auto destination =
                output / fs::u8path(enabled[i].name + "." + c.format);
            fs::create_hard_link(
                temp.path / (std::to_string(i) + "." + c.format), destination);
            published.push_back(destination);
        }
    } catch (...) {
        for (const auto &p : published) {
            std::error_code ec;
            fs::remove(p, ec);
        }
        throw;
    }
#endif
}
} // namespace ChartStudio
