#pragma once
#include "audio/audio.h"
#include <atomic>
#include <cereal/external/rapidjson/document.h>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace AudioStudio {
namespace audio = gpp::audio;
namespace fs = std::filesystem;
using Json = rapidjson::Document;
using Value = rapidjson::Value;

struct Product {
    audio::AnalysisResult analysis;
    audio::PcmBuffer raw, processed;
    audio::Settings settings;
    fs::path source;
    double time = 0, synthMs = 0, fxMs = 0;
    audio::Levels rawLevels, processedLevels;
};

class Session {
    struct Job {
        uint64_t id = 0;
        std::string kind;
        fs::path source, output;
        audio::Settings settings;
        double time = 0, length = 0;
        bool float32 = false, raw = false;
        std::shared_ptr<const Product> product;
    };
    struct Completion {
        Job job;
        std::shared_ptr<audio::Waveform> wave;
        std::shared_ptr<Product> product;
        std::string error;
    };
    // BASS must outlive transports and the joined worker.
    std::unique_ptr<audio::BassSystem> bass_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Job> pending_;
    std::optional<Completion> complete_;
    std::atomic<uint64_t> revision_{0};
    std::atomic<bool> quitting_{false};
    std::atomic<double> progress_{0};
    bool busy_ = false;
    uint64_t enqueue(Job job);
    void worker(int device);
    void joinWorker();

  public:
    audio::AudioTransport music, preview;
    audio::DeviceSettings deviceSettings;
    audio::Settings settings;
    audio::FxSettings musicFx;
    fs::path outputRoot, source;
    std::shared_ptr<const audio::Waveform> wave;
    std::shared_ptr<const Product> product;
    std::array<std::shared_ptr<const Product>, 2> slots;
    double cursor = 0, selectionStart = 0, selectionEnd = 1;
    double previewPrepareMs = 0;
    float musicVolume = .7f, musicPan = 0, previewVolume = .7f, previewPan = 0;
    float streamBuffer = .5f;
    std::string error, notice, jobKind, previewMode = "processed";
    bool shutdown = false, musicLoop = false;
    Session(audio::DeviceSettings device, fs::path output);
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
    void tick();
    void wait();
    void cancel();
    bool busy() const { return busy_; }
    double progress() const { return progress_.load(); }
    uint64_t revision() const { return revision_.load(); }
    uint64_t load(const fs::path &path);
    uint64_t generate(bool synthesis);
    uint64_t exportAudio(const fs::path &path, bool segment, bool raw,
                         bool float32);
    void updateSettings(const audio::Settings &next);
    void applyDevice(const audio::DeviceSettings &next);
    void seek(double time);
    void setSelection(double start, double end);
    void setMusicLoop(double start, double end, bool enabled);
    void playPreview(const std::string &mode);
    void applyMusicFx(const audio::FxSettings &fx);
    void storeSlot(int slot);
    void recallSlot(int slot);
    fs::path artifact(const fs::path &name) const;
};

Json settingsJson(const audio::Settings &settings);
audio::Settings parseSettings(const Value &json, audio::Settings base = {});
Json stateJson(Session &session);
Json parseJson(const std::string &text);
std::string encode(const Value &value);
std::string dispatch(Session &session, const std::string &line);
void savePreset(const fs::path &file, const audio::Settings &settings);
audio::Settings loadPreset(const fs::path &file);
int runGui(Session &session, bool rpc, int frames, const std::string &capture);
// Shared bounded stdin pump, called only from the control thread.
class StdinPump {
    std::string buffered_;

  public:
    bool ended = false;
    void poll(Session &session);
};
} // namespace AudioStudio
