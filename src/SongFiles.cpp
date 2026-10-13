#include "SongFiles.h"
#include <algorithm>
#include <cctype>

namespace songfiles {
std::string lower(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Paths list(const std::filesystem::path &directory) {
    Paths result;
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            result.push_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::filesystem::path named(const Paths &files, const std::string &name) {
    for (const auto &file : files) {
        if (lower(file.filename().u8string()) == lower(name)) {
            return file;
        }
    }
    return {};
}

Paths charts(const std::filesystem::path &directory, const Paths &files) {
    const auto base = directory.filename().u8string();
    Paths result;
    for (const auto &name :
         {base + ".gpp", base + ".chart", std::string("Chart.gpp"),
          std::string("Notes.gpp"), std::string("Chart.chart"),
          std::string("notes.chart"), std::string("notes.mid")}) {
        const auto wanted = lower(name);
        for (const auto &file : files) {
            if (lower(file.filename().u8string()) == wanted &&
                std::find(result.begin(), result.end(), file) == result.end()) {
                result.push_back(file);
            }
        }
    }
    return result;
}

std::filesystem::path audio(const std::filesystem::path &directory,
                            const Paths &files) {
    const auto base = directory.filename().u8string();
    for (const auto &name :
         {base + ".ogg", base + ".mp3", std::string("song.ogg"),
          std::string("song.mp3")}) {
        const auto path = named(files, name);
        if (!path.empty()) {
            return path;
        }
    }
    return {};
}
} // namespace songfiles
