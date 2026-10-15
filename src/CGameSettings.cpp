#include "CGameSettings.h"

#include <cereal/archives/json.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

bool CGameSettings::Window::valid() const {
    // Zero dimensions select the monitor's resolution in CEngine.
    return w >= 0 && h >= 0 && AA >= 0 && colorBits > 0 && VSyncMode >= 0 &&
           VSyncMode <= 3 && fullscreen >= 0 && fullscreen <= 3;
}

CGameSettings::LoadResult CGameSettings::load(Window &window,
                                              std::string &language) {
    writable = false;
    lastError.clear();
    try {
        if (!std::filesystem::exists(path)) {
            writable = true;
            return LoadResult::Missing;
        }
        std::ifstream input(path);
        if (!input) {
            throw std::runtime_error("cannot open file");
        }
        Window candidate;
        std::string candidateLanguage;
        int version = 0;
        cereal::JSONInputArchive archive(input);
        archive(cereal::make_nvp("version", version));
        if (version != 1) {
            throw std::runtime_error("unsupported settings version");
        }
        archive(cereal::make_nvp("window", candidate),
                cereal::make_nvp("language", candidateLanguage));
        if (!candidate.valid()) {
            throw std::runtime_error("invalid window settings");
        }
        window = std::move(candidate);
        language = std::move(candidateLanguage);
        writable = true;
        return LoadResult::Loaded;
    } catch (const std::exception &e) {
        lastError = path + ": " + e.what();
        return LoadResult::Error;
    }
}

bool CGameSettings::save(const Window &window, const std::string &language) {
    if (!writable) {
        lastError = path + ": saving disabled until settings load successfully";
        return false;
    }
    lastError.clear();
    try {
        if (!window.valid()) {
            throw std::runtime_error("invalid window settings");
        }
        const std::filesystem::path destination(path);
        if (!destination.parent_path().empty()) {
            std::filesystem::create_directories(destination.parent_path());
        }
        const std::filesystem::path temporary(path + ".tmp");
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            throw std::runtime_error("cannot open temporary file");
        }
        {
            cereal::JSONOutputArchive archive(output);
            archive(cereal::make_nvp("version", 1),
                    cereal::make_nvp("window", window),
                    cereal::make_nvp("language", language));
        }
        output.close();
        if (!output) {
            throw std::runtime_error("cannot finish writing temporary file");
        }
        // Replace only after the complete archive has been written and closed.
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING)) {
            throw std::runtime_error(
                "cannot replace settings file (Windows error " +
                std::to_string(GetLastError()) + ")");
        }
#else
        std::filesystem::rename(temporary, destination);
#endif
        return true;
    } catch (const std::exception &e) {
        lastError = path + ": " + e.what();
        return false;
    }
}
