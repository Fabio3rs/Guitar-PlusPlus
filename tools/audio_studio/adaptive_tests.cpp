#include "session.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>

using namespace AudioStudio;
namespace {
class AdaptiveTest : public testing::Test {
  protected:
    fs::path dir;
    std::unique_ptr<audio::BassSystem> bass;
    void SetUp() override {
        dir =
            fs::temp_directory_path() /
            ("gpp-adaptive-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(dir);
        audio::DeviceSettings settings;
        settings.device = 0;
        bass = std::make_unique<audio::BassSystem>(settings);
    }
    void TearDown() override {
        bass.reset();
        fs::remove_all(dir);
    }
    audio::PcmBuffer signal(double seconds,
                            const std::function<double(double)> &frequency,
                            bool opposite = false) {
        audio::PcmBuffer p;
        p.samples.resize(size_t(seconds * p.rate) * 2);
        double phase = 0;
        for (size_t i = 0; i < p.frames(); ++i) {
            const auto hz = frequency(double(i) / p.rate);
            phase += 6.283185307179586 * hz / p.rate;
            const auto value = hz > 0 ? float(.2 * std::sin(phase)) : 0;
            p.samples[i * 2] = value;
            p.samples[i * 2 + 1] = opposite ? -value : value;
        }
        return p;
    }
    audio::SongAnalysis scan(const audio::PcmBuffer &p,
                             audio::SongSettings settings = {}) {
        audio::writeWav(dir / "song.wav", p, true);
        return audio::scanSong(dir / "song.wav", settings);
    }
};

TEST_F(AdaptiveTest, ConstantVolumeAndOpposedStereoDoNotFragment) {
    auto pcm = signal(12, [](double) { return 220; }, true);
    for (size_t i = pcm.samples.size() / 2; i < pcm.samples.size(); ++i) {
        pcm.samples[i] *= .2f;
    }
    auto result = scan(pcm);
    ASSERT_EQ(result.regions.size(), 1u);
    EXPECT_FALSE(result.regions[0].silent);
    EXPECT_EQ(result.regions[0].begin, 0u);
    EXPECT_EQ(result.regions[0].end, pcm.frames());
    EXPECT_EQ(result.observations.size(), 120u);
    auto again = audio::scanSong(dir / "song.wav", {});
    ASSERT_EQ(result.observations.size(), again.observations.size());
    for (size_t i = 0; i < result.observations.size(); ++i) {
        EXPECT_EQ(result.observations[i].signature.chroma,
                  again.observations[i].signature.chroma);
    }
}

TEST_F(AdaptiveTest, PersistentChangesReturnAndTransient) {
    auto pcm = signal(24, [](double t) {
        return t >= 8 && t < 16 ? 277.1826 : t > 4 && t < 4.08 ? 1300 : 220;
    });
    auto result = scan(pcm);
    ASSERT_EQ(result.regions.size(), 3u);
    EXPECT_EQ(result.regions.front().setId, result.regions.back().setId);
    EXPECT_NE(result.regions[0].setId, result.regions[1].setId);
    EXPECT_NEAR(double(result.regions[1].begin) / result.info.rate, 8, 1);
    auto settings = result.settings;
    settings.deduplicate = false;
    auto unique = audio::segmentSong(result, settings);
    EXPECT_NE(unique.regions.front().setId, unique.regions.back().setId);
    settings.harmonicWeight = .2;
    const auto reweighted = audio::segmentSong(result, settings);
    for (size_t i = 1; i < reweighted.observations.size(); ++i) {
        EXPECT_DOUBLE_EQ(
            reweighted.observations[i].variation,
            audio::distance(reweighted.observations[i - 1].signature,
                            reweighted.observations[i].signature, .2));
    }
    settings.fixed = true;
    EXPECT_EQ(audio::segmentSong(result, settings).regions.size(), 1u);
    settings.fft *= 2;
    EXPECT_THROW(audio::segmentSong(result, settings), audio::Error);
}

TEST_F(AdaptiveTest, SilenceShortFilesAndBoundaries) {
    auto result = scan(signal(.02, [](double) { return 0; }));
    ASSERT_EQ(result.regions.size(), 1u);
    EXPECT_TRUE(result.regions[0].silent);
    const auto cache =
        audio::generateFailCache(result, audio::shortFailSettings());
    ASSERT_EQ(cache.samples.size(), 5u);
    ASSERT_EQ(cache.sets.size(), 1u);
    EXPECT_TRUE(cache.sets[0].fallback);
    EXPECT_TRUE(cache.sets[0].analysis.notes.empty());
    EXPECT_EQ(cache.regionAt(-1), &cache.song.regions[0]);
    EXPECT_EQ(cache.regionAt(100), &cache.song.regions[0]);
    EXPECT_EQ(cache.regionAt(NAN), nullptr);
    EXPECT_EQ(cache.regionAt(INFINITY), nullptr);
    EXPECT_GT(audio::levels(cache.samples[0].pcm).rms, 0);
    EXPECT_NEAR(cache.samples[0].contentSeconds, .051, 1. / 44100);
    EXPECT_NEAR(cache.samples[0].pcm.seconds(), .131, 1. / 44100);
    auto transition =
        scan(signal(20, [](double t) { return t >= 6 && t < 12 ? 0 : 220; }));
    ASSERT_EQ(transition.regions.size(), 3u);
    EXPECT_TRUE(transition.regions[1].silent);
}

TEST_F(AdaptiveTest, CacheRoundtripInvalidationAndAtomicFailure) {
    auto song = scan(signal(1, [](double) { return 220; }));
    auto cache = audio::generateFailCache(song, audio::shortFailSettings());
    audio::saveSongAnalysis(dir / "analysis.json", song);
    auto read =
        audio::loadSongAnalysis(dir / "analysis.json", dir / "song.wav");
    EXPECT_EQ(read.observations.size(), song.observations.size());
    audio::saveFailCache(dir / "bank.gppfc", cache);
    const auto size = fs::file_size(dir / "bank.gppfc");
    auto loaded = audio::loadFailCache(dir / "bank.gppfc", dir / "song.wav");
    ASSERT_EQ(loaded.samples.size(), 5u);
    EXPECT_EQ(loaded.samples[0].pcm.samples, cache.samples[0].pcm.samples);
    auto settings = cache.settings.synth;
    settings.fretMask = 1;
    auto expected = audio::processFx(
        audio::synthesize(cache.sets[0].analysis, settings), cache.settings.fx);
    EXPECT_EQ(expected.samples, loaded.samples[0].pcm.samples);
    EXPECT_THROW(audio::saveFailCache(dir / "bank.gppfc", cache,
                                      [](double p) {
                                          if (p >= .5) {
                                              throw audio::Error("Cancelled");
                                          }
                                      }),
                 audio::Error);
    EXPECT_EQ(fs::file_size(dir / "bank.gppfc"), size);
    EXPECT_NO_THROW(audio::loadFailCache(dir / "bank.gppfc", dir / "song.wav"));
    fs::create_directory(dir / "occupied");
    EXPECT_ANY_THROW(audio::saveFailCache(dir / "occupied", cache));
    EXPECT_THROW(audio::saveFailCache(dir / "song.wav", cache), audio::Error);
    for (const auto &entry : fs::directory_iterator(dir)) {
        EXPECT_EQ(entry.path().string().find(".tmp-"), std::string::npos);
    }
    {
        std::ofstream out(dir / "bank.gppfc", std::ios::binary | std::ios::app);
        out.put('x');
    }
    EXPECT_THROW(audio::loadFailCache(dir / "bank.gppfc", dir / "song.wav"),
                 audio::Error);
    audio::writeWav(dir / "song.wav", signal(1, [](double) { return 330; }),
                    true);
    EXPECT_THROW(
        audio::loadSongAnalysis(dir / "analysis.json", dir / "song.wav"),
        audio::Error);
    EXPECT_THROW(audio::generateFailCache(song, audio::shortFailSettings()),
                 audio::Error);
}

TEST_F(AdaptiveTest, CancellationAndVoicePolicies) {
    auto song = scan(signal(1, [](double) { return 220; }));
    auto cancel = [](double) { throw audio::Error("Cancelled"); };
    EXPECT_THROW(audio::scanSong(dir / "song.wav", {}, cancel), audio::Error);
    EXPECT_THROW(
        audio::generateFailCache(song, audio::shortFailSettings(), cancel),
        audio::Error);
    bool cancelledDuringGeneration = false;
    EXPECT_THROW(audio::generateFailCache(
                     song, audio::shortFailSettings(),
                     [&](double p) {
                         if (p > .2 && p < .9) {
                             cancelledDuringGeneration = true;
                             throw audio::Error("Cancelled during synthesis");
                         }
                     }),
                 audio::Error);
    EXPECT_TRUE(cancelledDuringGeneration);
    auto cache = std::make_shared<audio::SongFailCache>(
        audio::generateFailCache(song, audio::shortFailSettings()));
    EXPECT_THROW(audio::PreparedFailBank(cache, {}, cancel), audio::Error);
    audio::PreparedFailBank reject(cache, {3, false});
    EXPECT_TRUE(reject.trigger(0, 3, 0));
    EXPECT_FALSE(reject.trigger(0, 3, 0));
    EXPECT_EQ(reject.stats().active, 2u);
    EXPECT_EQ(reject.stats().rejected, 1u);
    reject.stop();
    audio::PreparedFailBank replace(cache, {3, true});
    EXPECT_TRUE(replace.trigger(0, 3, 0));
    EXPECT_TRUE(replace.trigger(0, 3, 0));
    EXPECT_EQ(replace.stats().active, 3u);
    EXPECT_EQ(replace.stats().replaced, 1u);
    EXPECT_FALSE(replace.trigger(0, 31, 0));
    EXPECT_THROW(replace.trigger(0, 32), audio::Error);
    audio::SustainedFailPreview sustained(cache->sets[0].analysis,
                                          cache->settings);
    sustained.press(0, .1f);
    ASSERT_TRUE(BASS_ChannelUpdate(sustained.handle(0), 700));
    const auto bytes = BASS_ChannelGetPosition(sustained.handle(0),
                                               BASS_POS_BYTE | BASS_POS_DECODE);
    const auto position = BASS_ChannelBytes2Seconds(sustained.handle(0), bytes);
    EXPECT_GE(position, cache->settings.synth.loopStart - 1. / 44100);
    EXPECT_LT(position,
              cache->settings.synth.loopStart + cache->settings.synth.loopBody);
    sustained.release(0, 20);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(BASS_ChannelIsActive(sustained.handle(0)), BASS_ACTIVE_STOPPED);
    sustained.press(0, .1f);
    EXPECT_EQ(BASS_ChannelIsActive(sustained.handle(0)), BASS_ACTIVE_PLAYING);
    sustained.release(0, 100);
    ASSERT_TRUE(BASS_ChannelIsSliding(sustained.handle(0), BASS_ATTRIB_VOL));
    sustained.press(0, .1f);
    EXPECT_FALSE(BASS_ChannelIsSliding(sustained.handle(0), BASS_ATTRIB_VOL));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(BASS_ChannelIsActive(sustained.handle(0)), BASS_ACTIVE_PLAYING);
    float volume = 0;
    ASSERT_TRUE(BASS_ChannelGetAttribute(sustained.handle(0), BASS_ATTRIB_VOL,
                                         &volume));
    EXPECT_FLOAT_EQ(volume, .1f);
    sustained.release(0, 100);
    sustained.stop();
    EXPECT_FALSE(BASS_ChannelIsSliding(sustained.handle(0), BASS_ATTRIB_VOL));
    EXPECT_EQ(BASS_ChannelIsActive(sustained.handle(0)), BASS_ACTIVE_STOPPED);
    EXPECT_THROW(sustained.press(0, NAN), audio::Error);
}

TEST_F(AdaptiveTest, RejectMalformedAnalysisAndProcessingVersions) {
    const auto song = scan(signal(1, [](double) { return 220; }));
    const auto path = dir / "analysis.json";
    auto reject = [&](const std::function<void(Json &)> &mutate) {
        audio::saveSongAnalysis(path, song);
        std::ifstream input(path);
        auto doc =
            parseJson(std::string(std::istreambuf_iterator<char>(input), {}));
        input.close();
        mutate(doc);
        saveJson(path, doc);
        EXPECT_THROW(audio::loadSongAnalysis(path, dir / "song.wav"),
                     audio::Error);
    };
    reject([](Json &d) { d["cache"]["version"].SetInt(999); });
    reject([](Json &d) { d["cache"]["analyzer"].SetInt(999); });
    reject([](Json &d) { d["cache"]["song"]["bassVersion"].SetInt(0); });
    reject([](Json &d) { d["cache"]["song"]["frames"].SetString("invalid"); });
    reject(
        [](Json &d) { d["cache"]["song"]["regions"][0]["begin"].SetInt(1); });
    reject([](Json &d) { d["cache"]["song"]["regions"][0]["end"].SetInt(1); });
    reject([](Json &d) { d["cache"]["song"]["observations"].SetObject(); });
}

TEST_F(AdaptiveTest, AdaptiveSessionPublicationAndDeviceTeardown) {
    auto song = scan(signal(1, [](double) { return 220; }));
    bass.reset();
    audio::DeviceSettings d;
    d.device = 0;
    Session s(d, dir);
    s.load(dir / "song.wav");
    s.wait();
    s.adaptiveJob("song.scan");
    s.wait();
    s.adaptiveJob("cache.generate");
    s.wait();
    auto cache = s.failCache;
    ASSERT_TRUE(s.cacheCurrent());
    s.adaptiveJob("cache.prepare");
    s.wait();
    ASSERT_TRUE(s.failBank);
    s.adaptivePreview("cache", 0, 3);
    s.adaptiveJob("cache.compare");
    s.wait();
    ASSERT_TRUE(s.individualBank);
    ASSERT_TRUE(s.directProduct);
    s.adaptivePreview("direct", 0, 4);
    auto settings = s.cacheSettings;
    settings.fx.drive = 2;
    s.updateAdaptive(s.songSettings, settings);
    EXPECT_FALSE(s.cacheCurrent());
    EXPECT_FALSE(s.failBank);
    EXPECT_EQ(s.failCache, cache);
    EXPECT_TRUE(s.songAnalysis);
    s.adaptiveJob("cache.generate");
    s.cancel();
    EXPECT_EQ(s.failCache, cache);
    s.applyDevice(d);
    EXPECT_FALSE(s.failBank);
}
} // namespace
