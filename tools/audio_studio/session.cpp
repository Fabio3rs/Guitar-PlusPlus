#include "session.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <utility>

namespace AudioStudio {
namespace {
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start)
        .count();
}
} // namespace

Session::Session(audio::DeviceSettings device, fs::path output)
    : deviceSettings(device), outputRoot(fs::absolute(output)) {
    fs::create_directories(outputRoot);
    musicFx.enabled = false;
    musicFx.normalize = false;
    const auto setupStart = Clock::now();
    try {
        bass_ = std::make_unique<audio::BassSystem>(deviceSettings);
    } catch (const audio::Error &e) {
        if (deviceSettings.device != -1) {
            throw;
        }
        notice = std::string(e.what()) +
                 "; using No Sound. Audible playback is unavailable.";
        deviceSettings.device = 0;
        bass_ = std::make_unique<audio::BassSystem>(deviceSettings);
    }
    streamBuffer = float(deviceSettings.bufferMs) / 1000;
    deviceSetupMs = elapsed(setupStart);
    worker_ = std::thread(&Session::worker, this, bass_->device());
}

Session::~Session() {
    joinWorker();
    preview.close();
    music.close();
}

void Session::joinWorker() {
    {
        // Share the waiter's mutex so shutdown cannot miss its transition
        // from checking the predicate to sleeping.
        std::lock_guard<std::mutex> lock(mutex_);
        quitting_.store(true);
        ++revision_;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

uint64_t Session::enqueue(Job job) {
    job.id = ++revision_;
    jobKind = job.kind;
    busy_ = true;
    progress_ = 0;
    error.clear();
    const auto id = job.id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_ = std::move(job);
        complete_.reset();
    }
    cv_.notify_one();
    return id;
}

void Session::worker(int device) {
    // BASS device selection is thread-local. No GLFW/ImGui calls on this
    // thread.
    const bool deviceReady = BASS_SetDevice(DWORD(device)) != FALSE;
    while (!quitting_.load()) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock,
                     [&] { return quitting_.load() || pending_.has_value(); });
            if (quitting_.load()) {
                return;
            }
            job = std::move(*pending_);
            pending_.reset();
        }
        Completion done;
        done.job = job;
        auto checkpoint = [&](double progress) {
            if (quitting_.load() || revision_.load() != job.id) {
                throw audio::Error("Cancelled");
            }
            progress_.store(progress);
        };
        try {
            audio::check(deviceReady, "Select worker BASS device");
            checkpoint(0);
            if (job.kind == "benchmark") {
                done.report = std::make_shared<Json>(runBenchmark(
                    job.source, job.time, job.settings, job.iterations,
                    job.bufferSeconds, checkpoint));
                done.report->AddMember("device_setup_ms", deviceSetupMs,
                                       done.report->GetAllocator());
                checkpoint(1);
                saveJson(job.output, *done.report);
            } else if (job.kind == "load") {
                done.wave = std::make_shared<audio::Waveform>(
                    audio::waveform(job.source, checkpoint));
            } else if (job.kind == "analyze" || job.kind == "generate") {
                auto result = std::make_shared<Product>();
                result->settings = job.settings;
                result->source = job.source;
                result->time = job.time;
                result->analysis =
                    audio::analyze(job.source, job.time, job.settings.analysis,
                                   [&](double p) { checkpoint(p * .3); });
                if (job.kind == "generate") {
                    auto start = Clock::now();
                    result->raw = audio::synthesize(
                        result->analysis, job.settings.synth,
                        [&](double p) { checkpoint(.3 + p * .3); });
                    result->synthMs = elapsed(start);
                    start = Clock::now();
                    result->processed = audio::processFx(
                        result->raw, job.settings.fx,
                        [&](double p) { checkpoint(.6 + p * .4); });
                    result->fxMs = elapsed(start);
                    audio::bakeLoop(result->raw, job.settings.synth.crossfade);
                    audio::bakeLoop(result->processed,
                                    job.settings.synth.crossfade);
                    result->rawLevels = audio::levels(result->raw);
                    result->processedLevels = audio::levels(result->processed);
                }
                done.product = std::move(result);
            } else if (job.kind == "export_segment") {
                auto raw =
                    audio::readSegment(job.source, job.time, job.length,
                                       [&](double p) { checkpoint(p * .5); });
                auto pcm =
                    job.raw
                        ? std::move(raw)
                        : audio::processFx(raw, job.settings.fx, [&](double p) {
                              checkpoint(.5 + p * .5);
                          });
                checkpoint(.99);
                audio::writeWav(job.output, pcm, job.float32);
            } else if (job.kind == "export") {
                checkpoint(.1);
                audio::writeWav(job.output,
                                job.raw ? job.product->raw
                                        : job.product->processed,
                                job.float32);
            }
            checkpoint(1);
        } catch (const std::exception &e) {
            done.error = e.what();
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (revision_.load() == job.id) {
                complete_ = std::move(done);
            }
        }
        cv_.notify_all();
    }
}

void Session::tick() {
    std::optional<Completion> done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (complete_) {
            done = std::move(complete_);
            complete_.reset();
        }
    }
    if (!done || done->job.id != revision_) {
        return;
    }
    busy_ = false;
    if (!done->error.empty()) {
        error = done->error;
        return;
    }
    try {
        if (done->wave) {
            // Open validates a replacement before freeing the previous channel.
            music.open(done->job.source, &musicFx);
            source = done->job.source;
            wave = std::move(done->wave);
            product.reset();
            preview.close();
            cursor = selectionStart = 0;
            selectionEnd = std::min(1., wave->info.duration);
            musicLoop = false;
            music.controls(musicVolume, musicPan, streamBuffer);
        }
        if (done->product) {
            product = std::move(done->product);
        }
        if (done->report) {
            benchmarkReport = std::move(done->report);
        }
        if (!done->job.output.empty()) {
            notice = "Exported " + done->job.output.string();
        }
    } catch (const std::exception &e) {
        error = e.what();
    }
}

void Session::wait() {
    while (busy_) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(20),
                         [&] { return complete_.has_value(); });
        }
        tick();
    }
    if (!error.empty()) {
        throw audio::Error(error);
    }
}

void Session::cancel() {
    ++revision_;
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.reset();
    complete_.reset();
    busy_ = false;
    jobKind = "cancelled";
}

uint64_t Session::load(const fs::path &path) {
    Job job;
    job.kind = "load";
    job.source = fs::absolute(path);
    return enqueue(std::move(job));
}

uint64_t Session::generate(bool synthesis) {
    if (source.empty()) {
        throw audio::Error("Load an audio file first");
    }
    audio::validate(settings);
    Job job;
    job.kind = synthesis ? "generate" : "analyze";
    job.source = source;
    job.time = cursor;
    job.settings = settings;
    return enqueue(std::move(job));
}

uint64_t Session::benchmark(int iterations, const fs::path &report) {
    if (source.empty() || iterations < 1 || iterations > 1000) {
        throw audio::Error("Load audio and choose 1 to 1000 benchmark iterations");
    }
    Job job;
    job.kind = "benchmark";
    job.source = source;
    job.time = cursor;
    job.settings = settings;
    job.iterations = iterations;
    job.bufferSeconds = streamBuffer;
    job.output = artifact(report);
    return enqueue(std::move(job));
}

fs::path Session::artifact(const fs::path &name) const {
    if (name.empty()) {
        throw audio::Error("An output filename is required");
    }
    const auto root = fs::weakly_canonical(outputRoot);
    const auto target =
        fs::weakly_canonical(name.is_absolute() ? name : root / name);
    const auto relative = target.lexically_relative(root);
    if (relative.empty() || *relative.begin() == ".." || target == root) {
        throw audio::Error("Output must be inside the Studio output directory");
    }
    if (!source.empty() && target == fs::weakly_canonical(source)) {
        throw audio::Error("Cannot overwrite the loaded audio source");
    }
    fs::create_directories(target.parent_path());
    return target;
}

uint64_t Session::exportAudio(const fs::path &path, bool segment, bool raw,
                              bool float32) {
    if (segment && (source.empty() || !std::isfinite(selectionStart) ||
                    !std::isfinite(selectionEnd) || selectionStart < 0 ||
                    selectionEnd <= selectionStart ||
                    selectionEnd > wave->info.duration)) {
        throw audio::Error("Select a valid source segment");
    }
    if (!segment && (!product || product->processed.samples.empty())) {
        throw audio::Error("Generate a preview before exporting");
    }
    Job job;
    job.kind = segment ? "export_segment" : "export";
    job.output = artifact(path);
    job.product = product;
    job.source = source;
    job.raw = raw;
    job.float32 = float32;
    job.settings = settings;
    if (segment) {
        job.settings.fx = musicFx;
    }
    job.time = selectionStart;
    job.length = selectionEnd - selectionStart;
    return enqueue(std::move(job));
}

void Session::updateSettings(const audio::Settings &next) {
    audio::validate(next);
    if (preview.handle() && previewMode == "live") {
        preview.effects(next.fx);
    }
    cancel();
    settings = next;
}

void Session::applyDevice(const audio::DeviceSettings &next) {
    audio::validate(next);
    const double position = music.position();
    joinWorker();
    preview.close();
    music.close();
    bass_.reset();
    const auto setupStart = Clock::now();
    std::string failure;
    try {
        bass_ = std::make_unique<audio::BassSystem>(next);
        deviceSettings = next;
    } catch (const std::exception &e) {
        failure = e.what();
        try {
            bass_ = std::make_unique<audio::BassSystem>(deviceSettings);
        } catch (...) {
            deviceSettings.device = 0;
            bass_ = std::make_unique<audio::BassSystem>(deviceSettings);
        }
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.reset();
        complete_.reset();
    }
    quitting_ = false;
    busy_ = false;
    deviceSetupMs = elapsed(setupStart);
    worker_ = std::thread(&Session::worker, this, bass_->device());
    streamBuffer = float(deviceSettings.bufferMs) / 1000;
    if (!source.empty()) {
        music.open(source);
        music.position(position);
        music.controls(musicVolume, musicPan, streamBuffer);
        music.effects(musicFx);
        if (musicLoop) {
            music.loop(selectionStart, selectionEnd, true);
        }
    }
    notice = "Device applied; playback is paused.";
    if (!failure.empty()) {
        throw audio::Error("Device change failed; restored a usable device: " +
                           failure);
    }
}

void Session::seek(double time) {
    if (!wave || !std::isfinite(time) || time < 0 ||
        time >= wave->info.duration) {
        throw audio::Error("Cursor must be inside the source duration");
    }
    music.position(time);
    cursor = time;
}

void Session::setSelection(double start, double end) {
    if (!wave || !std::isfinite(start) || !std::isfinite(end) || start < 0 ||
        start >= end || end > wave->info.duration) {
        throw audio::Error("Invalid selection");
    }
    if (musicLoop) {
        music.loop(start, end, true);
    }
    selectionStart = start;
    selectionEnd = end;
}

void Session::setMusicLoop(double start, double end, bool enabled) {
    music.loop(start, end, enabled);
    selectionStart = start;
    selectionEnd = end;
    musicLoop = enabled;
}

void Session::playPreview(const std::string &mode) {
    const auto start = Clock::now();
    if (mode == "original") {
        if (source.empty()) {
            throw audio::Error("Load a source first");
        }
        preview.open(source);
        preview.position(cursor);
        preview.loop(
            cursor,
            std::min(wave->info.duration, cursor + settings.analysis.window),
            true);
    } else {
        if (!product || product->processed.samples.empty()) {
            throw audio::Error("Generate a preview first");
        }
        if (mode != "raw" && mode != "live" && mode != "processed") {
            throw audio::Error("Unknown preview mode");
        }
        preview.open(mode == "processed" ? product->processed : product->raw);
        if (mode == "live") {
            preview.effects(settings.fx);
        }
    }
    previewMode = mode;
    preview.controls(previewVolume, previewPan, streamBuffer);
    previewPrepareMs = elapsed(start);
    preview.play();
}

void Session::applyMusicFx(const audio::FxSettings &fx) {
    auto next = settings;
    next.fx = fx;
    audio::validate(next);
    if (music.handle()) {
        music.effects(fx);
    }
    musicFx = fx;
}

void Session::storeSlot(int slot) {
    if (slot < 0 || slot > 1 || !product ||
        product->processed.samples.empty()) {
        throw audio::Error("Generate a result before storing A/B");
    }
    slots[size_t(slot)] = product;
}

void Session::recallSlot(int slot) {
    if (slot < 0 || slot > 1 || !slots[size_t(slot)]) {
        throw audio::Error("A/B slot is empty");
    }
    const auto &saved = slots[size_t(slot)];
    if (saved->source != source) {
        throw audio::Error("Reload the slot's source before recalling A/B");
    }
    updateSettings(saved->settings);
    product = saved;
    seek(saved->time);
}
} // namespace AudioStudio
