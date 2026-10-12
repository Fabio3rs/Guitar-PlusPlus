#include "session.h"
#include "voice_benchmark.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>
#include <type_traits>

namespace AudioStudio {
namespace {
using Clock = std::chrono::steady_clock;
double ms(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
std::string songKey(audio::SongSettings s) {
    std::ostringstream out;
    out.precision(17);
    audio::visitSongSettings(s, [&](const std::string &, auto &v, double,
                                    double) { out << v << ','; });
    return out.str();
}
Json readCoreJson(const std::string &text) {
    Json result;
    result.Parse(text.c_str(), text.size());
    if (result.HasParseError()) {
        throw audio::Error("Invalid core report");
    }
    return result;
}
void addTimings(Value &out, const char *name, const std::vector<double> &input,
                Json::AllocatorType &a) {
    auto sorted = input;
    std::sort(sorted.begin(), sorted.end());
    Value metric(rapidjson::kObjectType), samples(rapidjson::kArrayType);
    for (auto v : input) {
        samples.PushBack(v, a);
    }
    for (const auto &q : {std::pair<const char *, double>{"p50_ms", .5},
                          {"p95_ms", .95},
                          {"p99_ms", .99},
                          {"max_ms", 1}}) {
        metric.AddMember(
            Value(q.first, a),
            Value(sorted[size_t(std::ceil(q.second * sorted.size())) - 1]), a);
    }
    metric.AddMember("samples_ms", samples, a);
    out.AddMember(Value(name, a), metric, a);
}
} // namespace

bool Session::cacheCurrent() const { return failCache && !cacheStale; }

void Session::updateAdaptive(const audio::SongSettings &song,
                             const audio::Settings &synth) {
    audio::validate(song);
    audio::validate(synth);
    if (songKey(song) == songKey(songSettings) &&
        encode(settingsJson(synth)) == encode(settingsJson(cacheSettings))) {
        return;
    }
    cacheStale = true;
    cancel();
    stopAdaptive();
    failBank.reset();
    individualBank.reset();
    sustained.reset();
    directPreview.close();
    directProduct.reset();
    songSettings = song;
    cacheSettings = synth;
}

uint64_t Session::adaptiveJob(const std::string &kind, const fs::path &path,
                              int iterations) {
    if (source.empty()) {
        throw audio::Error("Load an audio source first");
    }
    audio::validate(songSettings);
    audio::validate(cacheSettings);
    Job job;
    job.kind = kind;
    job.source = source;
    job.songSettings = songSettings;
    job.settings = cacheSettings;
    job.device = deviceSettings;
    job.device.device = int(BASS_GetDevice());
    job.bankOptions = bankOptions;
    job.song = songAnalysis;
    job.cache = failCache;
    job.time = cursor;
    job.iterations = iterations;
    if (kind == "cache.generate" && !job.song && job.cache) {
        job.song = std::make_shared<audio::SongAnalysis>(job.cache->song);
    }
    if (kind == "cache.compare") {
        job.settings.synth.fretMask = int(cacheMask);
    }
    if (kind == "song.load" || kind == "cache.load") {
        job.output = fs::absolute(path);
    } else if (kind == "cache.generic") {
        job.output = fs::absolute(genericDirectory);
    } else if (!path.empty()) {
        job.output = artifact(path);
    } else if (kind == "cache.benchmark") {
        job.output = artifact("adaptive-benchmark.json");
    }
    if ((kind == "song.segment" || kind == "song.save" ||
         kind == "cache.generate" || kind == "cache.benchmark") &&
        !job.song) {
        throw audio::Error("Scan the song first");
    }
    if ((kind == "cache.prepare" || kind == "cache.save" ||
         kind == "cache.export" || kind == "cache.compare" ||
         kind == "cache.sustain" || kind == "cache.generic") &&
        !cacheCurrent()) {
        throw audio::Error("Generate or load a current cache first");
    }
    if ((kind == "song.save" || kind == "cache.save" ||
         kind == "cache.export" || kind == "song.load" ||
         kind == "cache.load") &&
        path.empty()) {
        throw audio::Error("Artifact path required");
    }
    if (kind == "cache.benchmark" && (iterations < 1 || iterations > 1000)) {
        throw audio::Error("Iterations must be 1..1000");
    }
    return enqueue(std::move(job));
}

void Session::processAdaptive(const Job &job, Completion &done,
                              const audio::Checkpoint &cb) {
    const auto start = Clock::now();
    if (job.kind == "song.scan") {
        done.song = std::make_shared<audio::SongAnalysis>(
            audio::scanSong(job.source, job.songSettings, cb));
    } else if (job.kind == "song.segment") {
        done.song = std::make_shared<audio::SongAnalysis>(
            audio::segmentSong(*job.song, job.songSettings, cb));
    } else if (job.kind == "song.save") {
        audio::saveSongAnalysis(job.output, *job.song, cb);
    } else if (job.kind == "song.load") {
        done.song = std::make_shared<audio::SongAnalysis>(
            audio::loadSongAnalysis(job.output, job.source, cb));
    } else if (job.kind == "cache.generate") {
        auto song = job.song;
        if (!song->observations.empty()) {
            done.song =
                std::make_shared<audio::SongAnalysis>(audio::segmentSong(
                    *song, job.songSettings, [&](double p) { cb(p * .05); }));
            song = done.song;
        } else if (songKey(song->settings) != songKey(job.songSettings)) {
            throw audio::Error(
                "Load analysis.json or scan again to change segmentation");
        }
        done.cache =
            std::make_shared<audio::SongFailCache>(audio::generateFailCache(
                *song, job.settings, [&](double p) { cb(.05 + .95 * p); }));
    } else if (job.kind == "cache.load") {
        done.cache = std::make_shared<audio::SongFailCache>(
            audio::loadFailCache(job.output, job.source, cb));
        done.fileBytes = fs::file_size(job.output);
    } else if (job.kind == "cache.save") {
        audio::saveFailCache(job.output, *job.cache, cb);
        done.fileBytes = fs::file_size(job.output);
    } else if (job.kind == "cache.prepare") {
        if (!audio::sourceMatches(job.cache->song.source, job.source, cb)) {
            throw audio::Error("Source changed; regenerate cache");
        }
        done.bank = std::make_unique<audio::PreparedFailBank>(
            job.cache, job.bankOptions, cb);
    } else if (job.kind == "cache.export") {
        for (size_t i = 0; i < job.cache->samples.size(); ++i) {
            auto target = job.output;
            target += "-" + std::to_string(i) + ".wav";
            if (fs::weakly_canonical(target) ==
                    fs::weakly_canonical(job.source) ||
                (fs::exists(target) && fs::equivalent(target, job.source))) {
                throw audio::Error("Sample export would overwrite source");
            }
        }
        auto reportPath = job.output;
        reportPath += ".json";
        if (fs::weakly_canonical(reportPath) ==
                fs::weakly_canonical(job.source) ||
            (fs::exists(reportPath) &&
             fs::equivalent(reportPath, job.source))) {
            throw audio::Error("Report export would overwrite source");
        }
        for (size_t i = 0; i < job.cache->samples.size(); ++i) {
            cb(double(i) / job.cache->samples.size());
            auto path = job.output;
            path += "-" + std::to_string(i) + ".wav";
            audio::writeWav(path, job.cache->samples[i].pcm, true);
        }
        auto report = readCoreJson(audio::cacheReportJson(*job.cache));
        auto path = job.output;
        path += ".json";
        saveJson(path, report);
    } else if (job.kind == "cache.compare") {
        auto point = job.cache->song;
        point.settings.fixed = true;
        point.observations.clear();
        audio::FailRegion region;
        region.end = point.frames;
        region.representative = uint64_t(job.time * point.info.rate);
        point.regions = {region};
        auto settings = job.settings;
        settings.synth.loop = false;
        auto individual =
            std::make_shared<audio::SongFailCache>(audio::generateFailCache(
                point, settings, [&](double p) { cb(p * .8); }));
        done.bank = std::make_unique<audio::PreparedFailBank>(
            individual, job.bankOptions, [&](double p) { cb(.8 + .05 * p); });
        done.direct = std::make_shared<Product>();
        done.direct->analysis = individual->sets.front().analysis;
        done.direct->settings = settings;
        done.direct->source = job.source;
        done.direct->time = job.time;
        done.direct->raw =
            audio::synthesize(done.direct->analysis, settings.synth,
                              [&](double p) { cb(.85 + .1 * p); });
        done.direct->processed =
            audio::processFx(done.direct->raw, settings.fx,
                             [&](double p) { cb(.95 + .05 * p); });
    } else if (job.kind == "cache.generic") {
        auto generic = std::make_shared<audio::SongFailCache>();
        generic->song = job.cache->song;
        generic->song.observations.clear();
        audio::FailRegion region;
        region.end = generic->song.frames;
        generic->song.regions = {region};
        generic->settings = audio::shortFailSettings();
        generic->fxVersion = BASS_FX_GetVersion();
        audio::FailSet set;
        const char *names[] = {"verde", "vermelho", "amarelo", "azul",
                               "laranja"};
        for (size_t i = 0; i < 5; ++i) {
            cb(double(i) / 5);
            const auto path =
                job.output / (std::string("erro-") + names[i] + ".wav");
            auto file = audio::openFile(path, true);
            const auto info = audio::sourceInfo(file.get());
            if (info.duration > 30) {
                throw audio::Error("Generic effect exceeds 30 seconds");
            }
            generic->samples.push_back(
                {audio::readSegment(path, 0, info.duration, cb),
                 info.duration});
            set.sampleIds[i] = uint32_t(i);
        }
        generic->sets.push_back(set);
        done.bank = std::make_unique<audio::PreparedFailBank>(
            generic, job.bankOptions, cb);
    } else if (job.kind == "cache.sustain") {
        const auto *region = job.cache->regionAt(job.time);
        done.sustained = std::make_unique<audio::SustainedFailPreview>(
            job.cache->sets[region->setId].analysis, job.settings, cb);
    } else if (job.kind == "cache.benchmark") {
#ifndef __linux__
        throw audio::Error("Isolated cache benchmarks currently require Linux");
#else
        fs::path runDirectory;
        for (int attempt = 0; attempt < 100; ++attempt) {
            auto candidate = job.output;
            candidate +=
                ".banks-" +
                std::to_string(Clock::now().time_since_epoch().count()) + "-" +
                std::to_string(attempt);
            if (fs::create_directory(candidate)) {
                runDirectory = candidate;
                break;
            }
        }
        if (runDirectory.empty()) {
            throw audio::Error("Cannot reserve benchmark output directory");
        }
        auto report = std::make_shared<Json>(rapidjson::kObjectType);
        auto &a = report->GetAllocator();
        report->AddMember("schema_version", 1, a);
        report->AddMember("kind", "adaptive_cache_benchmark", a);
        report->AddMember("bass_version", BASS_GetVersion(), a);
        report->AddMember("fx_version", BASS_FX_GetVersion(), a);
        report->AddMember("no_sound", BASS_GetDevice() == 0, a);
        report->AddMember("audible_latency_ms", Value(), a);
        report->AddMember("build_type", Value(GPP_AUDIO_BUILD_TYPE, a), a);
        report->AddMember("compiler", Value(GPP_AUDIO_COMPILER, a), a);
        report->AddMember("method",
                          "Same stored observations; muted prepared bank, one "
                          "warmup trigger, bounded sequential bursts. API "
                          "times are not audible latency.",
                          a);
        Value cases(rapidjson::kArrayType);
        for (unsigned profile = 0; profile < 4; ++profile) {
            for (unsigned dedup = 0; dedup < (profile == 3 ? 1u : 2u);
                 ++dedup) {
                auto options = job.songSettings;
                const double thresholds[] = {.15, .25, .4, .25},
                             persistence[] = {.5, .8, 1, .8},
                             minimum[] = {3, 4, 5, 4};
                options.threshold = thresholds[profile];
                options.hysteresis = options.threshold * .6;
                options.persistence = persistence[profile];
                options.minimum = minimum[profile];
                options.fixed = profile == 3;
                options.deduplicate = dedup != 0;
                const unsigned caseIndex = profile * 2 + dedup;
                auto progress = [&](double p) { cb((caseIndex + p) / 7); };
                auto song = audio::segmentSong(
                    *job.song, options, [&](double p) { progress(.01 * p); });
                auto cache = std::make_shared<audio::SongFailCache>(
                    audio::generateFailCache(song, job.settings, [&](double p) {
                        progress(.01 + .79 * p);
                    }));
                const char *names[] = {"aggressive", "balanced", "conservative",
                                       "fixed"};
                const auto bankPath =
                    runDirectory / (std::string(names[profile]) +
                                    (dedup ? "-dedup.gppfc" : "-unique.gppfc"));
                const auto saveStart = Clock::now();
                audio::saveFailCache(bankPath, *cache,
                                     [&](double p) { progress(.8 + .1 * p); });
                const double saveMs = ms(saveStart);
                const auto configPath = runDirectory / "probe-config.json",
                           resultPath = runDirectory / "probe-result.json";
                struct Cleanup {
                    fs::path config, result;
                    ~Cleanup() {
                        std::error_code ec;
                        fs::remove(config, ec);
                        fs::remove(result, ec);
                    }
                } cleanup{configPath, resultPath};
                Json config(rapidjson::kObjectType);
                auto &ca = config.GetAllocator();
                config.AddMember("source",
                                 Value(job.source.string().c_str(), ca), ca);
                config.AddMember("voices", int(job.bankOptions.voices), ca);
                config.AddMember("steal_oldest", job.bankOptions.stealOldest,
                                 ca);
                config.AddMember("iterations", job.iterations, ca);
                config.AddMember("device", job.device.device, ca);
                config.AddMember("rate", job.device.rate, ca);
                config.AddMember("buffer_ms", job.device.bufferMs, ca);
                config.AddMember("update_ms", job.device.updateMs, ca);
                config.AddMember("device_buffer_ms", job.device.deviceBufferMs,
                                 ca);
                config.AddMember("device_period_ms", job.device.devicePeriodMs,
                                 ca);
                saveJson(configPath, config);
                const auto executable =
                    fs::read_symlink("/proc/self/exe").parent_path() /
                    "gpp_audio_voice_probe";
                runAudioProbe({executable.string(), "--cache",
                               configPath.string(), bankPath.string(),
                               resultPath.string()},
                              [&](double) { progress(.95); });
                std::ifstream resultFile(resultPath);
                auto child = readCoreJson(std::string(
                    (std::istreambuf_iterator<char>(resultFile)), {}));
                std::vector<double> times, acceptedTimes, rejectedTimes;
                for (const auto &event : child["events"].GetArray()) {
                    const double t = event["trigger_ms"].GetDouble();
                    times.push_back(t);
                    (event["accepted"].GetBool() ? acceptedTimes
                                                 : rejectedTimes)
                        .push_back(t);
                }
                Value c(rapidjson::kObjectType);
                c.AddMember("profile", Value(names[profile], a), a);
                c.AddMember("deduplicate", options.deduplicate, a);
                c.AddMember("regions", uint64_t(song.regions.size()), a);
                c.AddMember("sets", uint64_t(cache->sets.size()), a);
                c.AddMember("pcm_bytes", cache->pcmBytes(), a);
                c.AddMember("prepare_ms", child["prepare_ms"].GetDouble(), a);
                c.AddMember("synth_ms", cache->synthMs, a);
                c.AddMember("fx_ms", cache->fxMs, a);
                c.AddMember("save_ms", saveMs, a);
                c.AddMember("bank_path", Value(bankPath.string().c_str(), a),
                            a);
                c.AddMember("file_bytes", uint64_t(fs::file_size(bankPath)), a);
                c.AddMember("rejected", child["rejected"].GetUint64(), a);
                c.AddMember("replaced", child["replaced"].GetUint64(), a);
                c.AddMember("peak_active", child["peak_active"].GetUint(), a);
                addTimings(c, "trigger_api", times, a);
                if (!acceptedTimes.empty()) {
                    addTimings(c, "accepted_trigger_api", acceptedTimes, a);
                }
                if (!rejectedTimes.empty()) {
                    addTimings(c, "rejected_trigger_api", rejectedTimes, a);
                }
                c.AddMember("playback", Value(child, a), a);
                auto details = readCoreJson(audio::cacheReportJson(*cache));
                c.AddMember("cache", Value(details["cache"], a), a);
                cases.PushBack(c, a);
                progress(1);
            }
        }
        report->AddMember("cases", cases, a);
        report->AddMember("total_ms", ms(start), a);
        if (!job.output.empty()) {
            cb(1);
            saveJson(job.output, *report);
        }
        done.report = std::move(report);
#endif
    } else {
        throw audio::Error("Unknown adaptive job: " + job.kind);
    }
    done.ioMs = ms(start);
}

void Session::publishAdaptive(Completion &done) {
    if (done.song) {
        songAnalysis = std::move(done.song);
        cacheStale = true;
        if (done.job.kind == "song.load") {
            songSettings = songAnalysis->settings;
        }
    }
    if (done.cache) {
        stopAdaptive();
        failBank.reset();
        individualBank.reset();
        sustained.reset();
        directProduct.reset();
        directPreview.close();
        failCache = std::move(done.cache);
        cacheStale = false;
        if (done.job.kind == "cache.load" && songAnalysis) {
            if (songAnalysis->source.hash == failCache->song.source.hash &&
                songAnalysis->settings.fft == failCache->song.settings.fft &&
                songAnalysis->settings.hop == failCache->song.settings.hop) {
                auto next =
                    std::make_shared<audio::SongAnalysis>(*songAnalysis);
                next->regions = failCache->song.regions;
                next->settings = failCache->song.settings;
                songAnalysis = std::move(next);
            } else {
                songAnalysis.reset();
            }
        }
        if (done.job.kind == "cache.load") {
            songSettings = failCache->song.settings;
            cacheSettings = failCache->settings;
        }
        cacheFileBytes = done.fileBytes;
    }
    if (done.direct) {
        directPreview.open(done.direct->processed);
        directProduct = std::move(done.direct);
    }
    if (done.bank) {
        if (done.job.kind == "cache.compare") {
            individualBank = std::move(done.bank);
        } else if (done.job.kind == "cache.generic") {
            genericBank = std::move(done.bank);
        } else {
            failBank = std::move(done.bank);
        }
    }
    if (done.sustained) {
        sustained = std::move(done.sustained);
    }
    if (done.job.kind == "cache.save" || done.job.kind == "cache.load") {
        cacheFileBytes = done.fileBytes;
        cacheIoMs = done.ioMs;
    }
}

void Session::adaptivePreview(const std::string &mode, double time,
                              unsigned mask) {
    if (mode == "direct") {
        if (!directProduct ||
            unsigned(directProduct->settings.synth.fretMask) != mask) {
            throw audio::Error("Prepare comparison for this fret mask first");
        }
        directPreview.controls(previewVolume, previewPan, streamBuffer);
        directPreview.position(0);
        directPreview.play();
    } else {
        auto *bank = mode == "cache"        ? failBank.get()
                     : mode == "individual" ? individualBank.get()
                     : mode == "generic"    ? genericBank.get()
                                            : nullptr;
        if (!bank || (mode == "cache" && !cacheCurrent())) {
            throw audio::Error("Prepare this preview bank first");
        }
        bank->trigger(time, mask, previewVolume);
    }
}

void Session::stopAdaptive() {
    simulating = false;
    for (auto *bank :
         {failBank.get(), individualBank.get(), genericBank.get()}) {
        if (bank) {
            bank->stop();
        }
    }
    if (sustained) {
        sustained->stop();
    }
    if (directPreview.handle()) {
        directPreview.stop();
    }
}

void Session::tickSimulation() {
    if (!simulating) {
        return;
    }
    try {
        if (!std::isfinite(simulationInterval) || simulationInterval < .01 ||
            simulationInterval > 60) {
            throw audio::Error("Invalid simulation interval");
        }
        const double t = music.position();
        if (BASS_ChannelIsActive(music.handle()) != BASS_ACTIVE_PLAYING ||
            t < simulationPrevious) {
            stopAdaptive();
            return;
        }
        if (t >= simulationNext) {
            simulationSkipped += uint64_t(std::max(
                0., std::floor((t - simulationNext) / simulationInterval)));
            adaptivePreview("cache", t, cacheMask);
            simulationNext = t + simulationInterval;
        }
        simulationPrevious = t;
    } catch (const std::exception &e) {
        simulating = false;
        error = e.what();
    }
}

Json adaptiveSettings(const Session &s) {
    auto json = settingsJson(s.cacheSettings);
    auto &a = json.GetAllocator();
    auto settings = s.songSettings;
    audio::visitSongSettings(
        settings, [&](const std::string &name, auto &v, double, double) {
            json.AddMember(Value(name.c_str(), a), Value(v), a);
        });
    return json;
}

void parseAdaptiveSettings(Session &s, const Value &json) {
    if (!json.IsObject()) {
        throw audio::Error("Expected adaptive parameters object");
    }
    auto song = s.songSettings;
    auto synth = s.cacheSettings;
    Json other(rapidjson::kObjectType);
    auto &a = other.GetAllocator();
    for (auto it = json.MemberBegin(); it != json.MemberEnd(); ++it) {
        const std::string name(it->name.GetString(),
                               it->name.GetStringLength());
        bool found = false;
        audio::visitSongSettings(song, [&](const std::string &key, auto &field,
                                           double lo, double hi) {
            if (key != name) {
                return;
            }
            found = true;
            using T = std::decay_t<decltype(field)>;
            if constexpr (std::is_same_v<T, bool>) {
                if (!it->value.IsBool()) {
                    throw audio::Error("Expected boolean: " + name);
                }
                field = it->value.GetBool();
            } else {
                if (!it->value.IsNumber()) {
                    throw audio::Error("Expected number: " + name);
                }
                const double v = it->value.GetDouble();
                if (!std::isfinite(v) || v < lo || v > hi ||
                    (std::is_integral_v<T> && v != std::floor(v))) {
                    throw audio::Error("Invalid adaptive parameter: " + name);
                }
                field = T(v);
            }
        });
        if (!found) {
            other.AddMember(Value(it->name, a), Value(it->value, a), a);
        }
    }
    synth = parseSettings(other, synth);
    s.updateAdaptive(song, synth);
}

Json adaptiveState(Session &s, size_t offset, size_t limit) {
    Json out(rapidjson::kObjectType);
    auto &a = out.GetAllocator();
    auto settings = adaptiveSettings(s);
    out.AddMember("settings", Value(settings, a), a);
    out.AddMember("current", s.cacheCurrent(), a);
    out.AddMember("prepared", bool(s.failBank), a);
    out.AddMember("file_bytes", s.cacheFileBytes, a);
    out.AddMember("io_ms", s.cacheIoMs, a);
    out.AddMember("simulation_skipped", s.simulationSkipped, a);
    out.AddMember("simulating", s.simulating, a);
    out.AddMember("audible_latency_ms", Value(), a);
    const auto *song = s.songAnalysis ? s.songAnalysis.get()
                       : s.failCache  ? &s.failCache->song
                                      : nullptr;
    if (song) {
        out.AddMember("duration", song->info.duration, a);
        out.AddMember("observations", uint64_t(song->observations.size()), a);
        out.AddMember("detected_regions", uint64_t(song->detectedRegions), a);
        out.AddMember("regions", uint64_t(song->regions.size()), a);
        out.AddMember("scan_ms", song->scanMs, a);
        out.AddMember("segment_ms", song->segmentMs, a);
        out.AddMember("identify_ms", song->identifyMs, a);
        out.AddMember("open_ms", song->openMs, a);
        out.AddMember("decode_ms", song->decodeMs, a);
        out.AddMember("fft_ms", song->fftMs, a);
        out.AddMember("descriptor_ms", song->descriptorMs, a);
        Value regions(rapidjson::kArrayType);
        for (size_t i = std::min(offset, song->regions.size());
             i < song->regions.size() &&
             i - offset < std::min(limit, size_t(10000));
             ++i) {
            const auto &r = song->regions[i];
            Value v(rapidjson::kObjectType);
            v.AddMember("id", uint64_t(i), a);
            v.AddMember("start", double(r.begin) / song->info.rate, a);
            v.AddMember("end", double(r.end) / song->info.rate, a);
            v.AddMember("representative",
                        double(r.representative) / song->info.rate, a);
            v.AddMember("set_id", r.setId, a);
            v.AddMember("silent", r.silent, a);
            v.AddMember("change", r.change, a);
            regions.PushBack(v, a);
        }
        out.AddMember("region_list", regions, a);
    }
    if (s.failCache) {
        out.AddMember("unique_sets", uint64_t(s.failCache->sets.size()), a);
        out.AddMember("samples", uint64_t(s.failCache->samples.size()), a);
        out.AddMember("pcm_bytes", s.failCache->pcmBytes(), a);
        out.AddMember("synth_ms", s.failCache->synthMs, a);
        out.AddMember("fx_ms", s.failCache->fxMs, a);
        out.AddMember("representative_ms", s.failCache->representativeMs, a);
    }
    if (s.failBank) {
        const auto stats = s.failBank->stats();
        out.AddMember("prepare_ms", stats.prepareMs, a);
        out.AddMember("trigger_api_ms", stats.lastTriggerMs, a);
        out.AddMember("active", stats.active, a);
        out.AddMember("peak_active", stats.peakActive, a);
        out.AddMember("rejected", stats.rejected, a);
        out.AddMember("replaced", stats.replaced, a);
        out.AddMember("failures", stats.failures, a);
        out.AddMember("prepared_pcm_bytes", stats.pcmBytes, a);
    }
    if (s.directProduct) {
        out.AddMember("comparison_time", s.directProduct->time, a);
        out.AddMember("comparison_mask",
                      s.directProduct->settings.synth.fretMask, a);
    }
    if (s.cacheBenchmarkReport) {
        out.AddMember("benchmark", Value(*s.cacheBenchmarkReport, a), a);
    }
    return out;
}
} // namespace AudioStudio
