#pragma once

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <string>
#include <utility>

// Preferences only: charts, player profiles and campaigns have their own saves.
class CGameSettings {
  public:
    struct Window {
        int h = 0, w = 0, AA = 0, colorBits = 24, VSyncMode = 3;
        int fullscreen = 1;
        std::string name;

        template <class Archive> void serialize(Archive &archive) {
            archive(cereal::make_nvp("width", w),
                    cereal::make_nvp("height", h),
                    cereal::make_nvp("antialiasing", AA),
                    cereal::make_nvp("colorBits", colorBits),
                    cereal::make_nvp("vsync", VSyncMode),
                    cereal::make_nvp("fullscreen", fullscreen),
                    cereal::make_nvp("title", name));
        }

        bool valid() const;
    };

    enum class LoadResult { Missing, Loaded, Error };

    explicit CGameSettings(std::string path = "data/saves/settings.json")
        : path(std::move(path)) {}

    // Failed reads leave the caller's defaults and the original file intact.
    LoadResult load(Window &window, std::string &language);
    bool save(const Window &window, const std::string &language);
    const std::string &error() const { return lastError; }

  private:
    std::string path;
    std::string lastError;
    bool writable = false;
};
