#pragma once
#include "CChart.h"
#include <atomic>
#include <cereal/types/map.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace songs {
enum class ScanMode { Incremental, Full };

struct SongRecord {
    std::string path, chartPath, format, audioPath, coverPath;
    std::string title, artist, charter, album, genre, year, origin;
    std::optional<double> duration, previewStart, previewEnd;
    std::string durationSource;
    double chartEnd = 0, bpmInitial = 0, bpmMin = 0, bpmMax = 0;
    std::vector<std::string> instruments, stems, trackNames, identification;
    std::map<std::string, std::string> chartFields, iniFields, sources;
    bool available = false;
    std::string error;

    template <class Archive> void serialize(Archive &a) {
        a(CEREAL_NVP(path), CEREAL_NVP(chartPath), CEREAL_NVP(format),
          CEREAL_NVP(audioPath), CEREAL_NVP(coverPath), CEREAL_NVP(title),
          CEREAL_NVP(artist), CEREAL_NVP(charter), CEREAL_NVP(album),
          CEREAL_NVP(genre), CEREAL_NVP(year), CEREAL_NVP(origin),
          CEREAL_NVP(duration), CEREAL_NVP(previewStart),
          CEREAL_NVP(previewEnd), CEREAL_NVP(durationSource),
          CEREAL_NVP(chartEnd), CEREAL_NVP(bpmInitial), CEREAL_NVP(bpmMin),
          CEREAL_NVP(bpmMax), CEREAL_NVP(instruments), CEREAL_NVP(stems),
          CEREAL_NVP(trackNames), CEREAL_NVP(identification),
          CEREAL_NVP(chartFields), CEREAL_NVP(iniFields), CEREAL_NVP(sources),
          CEREAL_NVP(available), CEREAL_NVP(error));
    }
};

struct ScanProgress {
    bool running = false, complete = false, cancelled = false;
    ScanMode mode = ScanMode::Incremental;
    uint64_t examined = 0, added = 0, modified = 0, removed = 0, reused = 0;
    uint64_t hashes = 0, chartParses = 0, audioProbes = 0;
    std::string warning;
};

struct LibrarySnapshot {
    uint64_t revision = 0, contentRevision = 0;
    std::vector<SongRecord> songs;
    ScanProgress progress;
};

struct AudioInfo {
    std::optional<double> duration;
    std::string error;
};
AudioInfo probeAudio(const std::filesystem::path &file);

// UI owns the service; the worker exclusively owns SQLite, filesystem reads,
// and mutable records. Publication never copies concurrently mutable fields.
class SongLibrary {
  public:
    using Inspector =
        std::function<CChart::Inspection(const std::filesystem::path &)>;
    using AudioProbe = std::function<AudioInfo(const std::filesystem::path &)>;
    SongLibrary(std::filesystem::path root = "data/songs",
                std::filesystem::path database = "data/cache/songlibrary.db",
                Inspector inspector = {}, AudioProbe audio = {});
    ~SongLibrary();
    SongLibrary(const SongLibrary &) = delete;
    SongLibrary &operator=(const SongLibrary &) = delete;
    void refresh(ScanMode mode = ScanMode::Incremental);
    void cancel() noexcept;
    bool running() const noexcept;
    void wait(); // Lifecycle/tests only. The menu polls before joining.
    std::shared_ptr<const LibrarySnapshot> snapshot() const;

  private:
    struct State;
    std::unique_ptr<State> state_;
    void run(ScanMode mode) noexcept;
    std::filesystem::path root_, database_;
    Inspector inspector_;
    AudioProbe audio_;
    std::thread worker_;
    mutable std::mutex requestMutex_;
    bool active_ = false;
    std::optional<ScanMode> pending_;
    std::atomic<bool> cancel_{false};
    std::shared_ptr<const LibrarySnapshot> published_;
};
} // namespace songs
