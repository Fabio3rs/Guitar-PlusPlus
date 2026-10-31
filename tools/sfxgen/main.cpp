#include "audio/sfx.h"
#include <cereal/external/rapidjson/stringbuffer.h>
#include <cereal/external/rapidjson/writer.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>

namespace {
namespace sfx = gpp::audio::sfx;
namespace audio = gpp::audio;
namespace fs = std::filesystem;
struct Options {
    fs::path spec, all, output, directory, compareDirectory;
    bool check = false, float32 = false, json = false, force = false;
    bool requireFilenameId = false;
};

Options options(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() {
            if (++i >= argc) {
                throw audio::Error("Missing value for " + arg);
            }

            return fs::u8path(argv[i]);
        };
        if (arg == "--spec") {
            o.spec = value();
        } else if (arg == "--all") {
            o.all = value();
        } else if (arg == "--output") {
            o.output = value();
        } else if (arg == "--output-dir") {
            o.directory = value();
        } else if (arg == "--compare-dir") {
            o.compareDirectory = value();
        } else if (arg == "--check") {
            o.check = true;
        } else if (arg == "--float32") {
            o.float32 = true;
        } else if (arg == "--json") {
            o.json = true;
        } else if (arg == "--force") {
            o.force = true;
        } else if (arg == "--require-filename-id") {
            o.requireFilenameId = true;
        } else if (arg == "--help") {
            std::cout
                << "Guitar++ offline SFX generator v" << sfx::generatorVersion
                << "\n"
                   "  --spec RECIPE.lua --output SOUND.wav\n"
                   "  --all RECIPE_DIRECTORY --output-dir DIRECTORY\n"
                   "  --spec FILE | --all DIRECTORY: --check (validate only)\n"
                   "  --compare-dir DIRECTORY (render and compare WAV content; "
                   "no writes)\n"
                   "  --float32 --json --force (explicitly replace generated "
                   "WAVs)\n"
                   "  --require-filename-id (validate the packaged library "
                   "convention)\n"
                   "No device or BASS initialization. Default export: PCM16.\n";
            std::exit(0);
        } else {
            throw audio::Error("Unknown argument: " + arg);
        }
    }

    if (o.spec.empty() == o.all.empty()) {
        throw audio::Error("Choose exactly one of --spec or --all");
    }

    if ((o.check && (!o.output.empty() || !o.directory.empty() ||
                     !o.compareDirectory.empty())) ||
        (!o.compareDirectory.empty() &&
         (!o.output.empty() || !o.directory.empty() || o.force)) ||
        (!o.all.empty() && !o.output.empty()) ||
        (!o.spec.empty() && !o.directory.empty())) {
        throw audio::Error("Incompatible output/check/compare options");
    }

    if (!o.check && o.compareDirectory.empty() &&
        ((!o.spec.empty() && o.output.empty()) ||
         (!o.all.empty() && o.directory.empty()))) {
        throw audio::Error("Generation requires --output or --output-dir");
    }

    return o;
}

std::string portableId(std::string id) {
    for (auto &c : id) {
        if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';
        }
    }

    return id;
}
} // namespace

int main(int argc, char **argv) {
    try {
        const auto o = options(argc, argv);
        const auto files = o.spec.empty() ? sfx::recipes(o.all)
                                          : std::vector<fs::path>{o.spec};
        if (files.empty()) {
            throw audio::Error("No .lua recipes found");
        }

        // Validate the complete batch before writing. Colliding IDs must not
        // overwrite each other, even with --force on case-insensitive volumes.
        std::vector<std::optional<sfx::SoundDefinition>> definitions(
            files.size());
        std::vector<std::string> errors(files.size());
        std::map<std::string, size_t> ids;
        for (size_t i = 0; i < files.size(); ++i) {
            try {
                definitions[i] = sfx::load(files[i]);
                const auto &id = definitions[i]->id;
                const auto entry = ids.emplace(portableId(id), i);
                if (!entry.second) {
                    const auto first = entry.first->second;
                    errors[first] = errors[i] =
                        "Case-insensitive recipe ID collision: " +
                        files[first].string() + " and " + files[i].string();
                }

                if (o.requireFilenameId && files[i].stem().u8string() != id) {
                    throw audio::Error("Recipe id must equal its filename stem "
                                       "for packaging: " +
                                       id);
                }
            } catch (const std::exception &e) {
                errors[i] = e.what();
            }
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> json(buffer);
        json.StartObject();
        json.Key("generatorVersion");
        json.Uint(sfx::generatorVersion);
        json.Key("recipes");
        json.StartArray();
        unsigned failures = 0;
        for (size_t i = 0; i < files.size(); ++i) {
            const auto &file = files[i];
            json.StartObject();
            json.Key("file");
            json.String(file.u8string().c_str());
            try {
                const auto start = std::chrono::steady_clock::now();
                if (!errors[i].empty()) {
                    throw audio::Error(errors[i]);
                }

                const auto &sound = *definitions[i];
                json.Key("id");
                json.String(sound.id.c_str());
                if (!o.check) {
                    const auto pcm = sfx::render(sound);
                    const auto measured = audio::levels(pcm);
                    fs::path output = o.output.empty()
                                          ? o.directory / (sound.id + ".wav")
                                          : o.output;
                    if (!o.compareDirectory.empty()) {
                        output = o.compareDirectory / (sound.id + ".wav");
                        const auto expected = audio::wavBytes(pcm, o.float32);
                        if (!fs::is_regular_file(output) ||
                            fs::file_size(output) != expected.size()) {
                            throw audio::Error(
                                "Comparison size/missing file: " +
                                output.string());
                        }

                        std::ifstream in(output, std::ios::binary);
                        std::vector<uint8_t> actual(expected.size());
                        in.read(reinterpret_cast<char *>(actual.data()),
                                std::streamsize(actual.size()));
                        if (!in || actual != expected) {
                            throw audio::Error("Comparison differs: " +
                                               output.string());
                        }
                    } else {
                        sfx::exportWav(output, pcm, o.float32, o.force);
                    }

                    json.Key("output");
                    json.String(output.u8string().c_str());
                    json.Key("frames");
                    json.Uint64(pcm.frames());
                    json.Key("sampleRate");
                    json.Uint(pcm.rate);
                    json.Key("channels");
                    json.Uint(pcm.channels);
                    json.Key("peak");
                    json.Double(measured.peak);
                    json.Key("rms");
                    json.Double(measured.rms);
                    json.Key("fingerprint");
                    json.String(sfx::fingerprint(pcm).c_str());
                    json.Key("loopBegin");
                    json.Uint64(pcm.loop.begin);
                    json.Key("loopEnd");
                    json.Uint64(pcm.loop.end);
                }

                json.Key("milliseconds");
                json.Double(std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - start)
                                .count());
                json.Key("ok");
                json.Bool(true);
                if (!o.json) {
                    std::cout << "OK " << file.string() << '\n';
                }
            } catch (const std::exception &e) {
                ++failures;
                json.Key("ok");
                json.Bool(false);
                json.Key("error");
                json.String(e.what());
                std::cerr << file.string() << ": " << e.what() << '\n';
            }

            json.EndObject();
        }

        json.EndArray();
        json.Key("failures");
        json.Uint(failures);
        json.EndObject();
        if (o.json) {
            std::cout << buffer.GetString() << '\n';
        }

        return failures ? 1 : 0;
    } catch (const std::exception &e) {
        std::cerr << "gpp_sfxgen: " << e.what() << '\n';
        return 1;
    }
}
