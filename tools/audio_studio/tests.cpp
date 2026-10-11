#include "session.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>

using namespace AudioStudio;
namespace {
audio::PcmBuffer tone(unsigned channels = 2, unsigned rate = 44100,
                      double seconds = .5) {
    audio::PcmBuffer pcm;
    pcm.rate = rate;
    pcm.channels = channels;
    pcm.samples.resize(size_t(seconds * rate) * channels);
    for (size_t frame = 0; frame < pcm.frames(); ++frame) {
        for (unsigned ch = 0; ch < channels; ++ch) {
            pcm.samples[frame * channels + ch] =
                float(.4 * std::sin(2 * 3.141592653589793 * 440 *
                                    double(frame) / rate));
        }
    }
    return pcm;
}

class AudioTest : public testing::Test {
  protected:
    fs::path directory;
    std::unique_ptr<audio::BassSystem> bass;
    void SetUp() override {
        directory =
            fs::temp_directory_path() /
            ("gpp-audio-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(directory);
        audio::DeviceSettings device;
        device.device = 0;
        bass = std::make_unique<audio::BassSystem>(device);
    }
    void TearDown() override {
        bass.reset();
        fs::remove_all(directory);
    }
};

TEST_F(AudioTest, FloatWavRoundtripAndMonoRates) {
    for (unsigned rate : {22050u, 44100u, 48000u}) {
        for (unsigned channels : {1u, 2u}) {
            auto pcm = tone(channels, rate);
            auto file = directory / "tone.wav";
            audio::writeWav(file, pcm, true);
            auto decoded = audio::readSegment(file, 0, pcm.seconds());
            EXPECT_EQ(pcm.samples, decoded.samples);
            auto wave = audio::waveform(file);
            EXPECT_EQ(wave.info.rate, rate);
            EXPECT_LE(wave.low.size(), 65536u);
            auto analysis = audio::analyze(file, 0, {});
            EXPECT_TRUE(
                std::any_of(analysis.notes.begin(), analysis.notes.end(),
                            [](const auto &n) { return n.midi == 69; }));
            EXPECT_NO_THROW(audio::analyze(file, pcm.seconds() - .001, {}));
        }
    }
}

TEST_F(AudioTest, SilenceHasNoSpuriousMidiAndSynthFallbackIsFinite) {
    auto pcm = tone();
    std::fill(pcm.samples.begin(), pcm.samples.end(), 0);
    audio::writeWav(directory / "silence.wav", pcm, false);
    const auto result = audio::analyze(directory / "silence.wav", 0, {});
    EXPECT_TRUE(result.notes.empty());
    EXPECT_GT(audio::levels(audio::synthesize(result, {})).rms, 0);
}

TEST_F(AudioTest, AllMasksShortEnvelopesAndLoops) {
    audio::AnalysisResult analysis;
    analysis.notes = {{69, 440, 1, "A4"}, {76, 659.25, .8, "E5"}};
    for (int mask = 0; mask <= 31; ++mask) {
        audio::SynthSettings settings;
        settings.fretMask = mask;
        settings.duration = .051;
        const auto pcm = audio::synthesize(analysis, settings);
        EXPECT_EQ(pcm.frames(), size_t(std::ceil(.051 * 44100)));
        EXPECT_LE(audio::levels(pcm).peak, .950001);
        EXPECT_LT(std::abs(pcm.samples.back()), .001);
    }
    audio::SynthSettings settings;
    settings.loop = true;
    auto pcm = audio::synthesize(analysis, settings);
    audio::bakeLoop(pcm, .01);
    EXPECT_GT(pcm.loop.end, pcm.loop.begin);
    EXPECT_EQ(pcm.loop.end, pcm.frames());
    const auto wav = audio::wavBytes(pcm, true);
    const std::string bytes(wav.begin(), wav.end());
    EXPECT_NE(bytes.find("smpl"), std::string::npos);
    audio::AudioTransport preview;
    preview.open(pcm);
    preview.play();
    preview.stop();
    settings.loopBody = .01;
    settings.crossfade = 1;
    pcm = audio::synthesize(analysis, settings);
    EXPECT_NO_THROW(audio::bakeLoop(pcm, 1));
}

TEST_F(AudioTest, FxBypassIdentityPaddingAndActualProcessing) {
    const auto raw = tone();
    audio::FxSettings fx;
    fx.enabled = false;
    fx.normalize = false;
    fx.preRoll = fx.tail = fx.fade = 0;
    auto result = audio::processFx(raw, fx);
    EXPECT_EQ(result.samples, raw.samples);
    fx.preRoll = .04;
    fx.tail = .08;
    result = audio::processFx(raw, fx);
    EXPECT_EQ(result.frames(), raw.frames() + size_t(.08 * raw.rate));
    EXPECT_TRUE(std::equal(raw.samples.begin(), raw.samples.end(),
                           result.samples.begin()));
    fx.enabled = true;
    fx.normalize = true;
    result = audio::processFx(raw, fx);
    EXPECT_NE(result.samples, raw.samples);
    EXPECT_LE(audio::levels(result).peak, .950001);
    fx.pre.frequency = raw.rate;
    EXPECT_THROW(audio::processFx(raw, fx), audio::Error);
}

TEST_F(AudioTest, LiveFxMatchesOfflineUnnormalizedChain) {
    auto raw = tone();
    auto bytes = audio::wavBytes(raw, true);
    audio::Stream stream(
        BASS_StreamCreateFile(TRUE, bytes.data(), 0, bytes.size(),
                              BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT));
    ASSERT_NE(stream.get(), 0u);
    audio::FxSettings fx;
    fx.normalize = false;
    fx.preRoll = fx.tail = fx.fade = 0;
    audio::FxChain chain;
    chain.apply(stream.get(), raw.rate, fx);
    std::vector<float> live(raw.samples.size());
    const auto got = BASS_ChannelGetData(stream.get(), live.data(),
                                         DWORD(live.size() * sizeof(float)));
    ASSERT_EQ(got, live.size() * sizeof(float));
    const auto offline = audio::processFx(raw, fx);
    ASSERT_EQ(live.size(), offline.samples.size());
    for (size_t i = 0; i < live.size(); ++i) {
        ASSERT_NEAR(live[i], offline.samples[i], 1e-5);
    }
    fx.enabled = false;
    EXPECT_NO_THROW(chain.apply(stream.get(), raw.rate, fx));
    fx.enabled = true;
    EXPECT_NO_THROW(chain.apply(stream.get(), raw.rate, fx));
}

TEST_F(AudioTest, InvalidInputAndCancellation) {
    audio::Settings settings;
    settings.analysis.fft = 513;
    EXPECT_THROW(audio::validate(settings), audio::Error);
    settings = {};
    settings.synth.fretMask = 32;
    EXPECT_THROW(audio::validate(settings), audio::Error);
    settings = {};
    settings.fx.drive = NAN;
    EXPECT_THROW(audio::validate(settings), audio::Error);
    auto pcm = tone();
    pcm.loop = {100, pcm.frames() + 1};
    EXPECT_THROW(audio::wavBytes(pcm, true), audio::Error);
    audio::writeWav(directory / "tone.wav", tone(), true);
    EXPECT_THROW(audio::analyze(directory / "tone.wav", -1, {}), audio::Error);
    EXPECT_THROW(
        audio::waveform(directory / "tone.wav",
                        [](double) { throw audio::Error("Cancelled"); }),
        audio::Error);
}

TEST_F(AudioTest, PresetRoundtripAndValidation) {
    audio::Settings settings;
    settings.synth.duration = .1;
    settings.fx.drive = 4;
    settings.synth.shifts[3] = 7;
    settings.fx.post.enabled = false;
    savePreset(directory / "preset.json", settings);
    EXPECT_EQ(encode(settingsJson(settings)),
              encode(settingsJson(loadPreset(directory / "preset.json"))));
    EXPECT_THROW(parseSettings(parseJson("{\"synth.fretMask\":32}")),
                 audio::Error);
    EXPECT_THROW(parseSettings(parseJson("{\"synth.voices\":1.5}")),
                 audio::Error);
    EXPECT_THROW(parseSettings(parseJson("{\"synth.loop\":1}")), audio::Error);
    EXPECT_THROW(parseSettings(parseJson("{\"unknown\":2}")), audio::Error);
}

TEST_F(AudioTest, SessionPublicationCancellationAndPreservation) {
    bass.reset();
    audio::DeviceSettings d;
    d.device = 0;
    Session s(d, directory);
    audio::writeWav(directory / "tone.wav", tone(), true);
    s.load(directory / "tone.wav");
    s.wait();
    s.generate(true);
    s.wait();
    const auto saved = s.product;
    s.storeSlot(0);
    s.load(directory / "missing.wav");
    EXPECT_THROW(s.wait(), audio::Error);
    EXPECT_EQ(s.product, saved);
    EXPECT_EQ(s.source, directory / "tone.wav");
    auto highEq = s.musicFx;
    highEq.enabled = true;
    highEq.pre.frequency = 18000;
    s.applyMusicFx(highEq);
    audio::writeWav(directory / "low-rate.wav", tone(2, 8000), true);
    s.load(directory / "low-rate.wav");
    EXPECT_THROW(s.wait(), audio::Error);
    EXPECT_EQ(s.source, directory / "tone.wav");
    EXPECT_EQ(s.product, saved);
    highEq.enabled = false;
    s.applyMusicFx(highEq);
    s.generate(true);
    s.cancel();
    s.tick();
    EXPECT_FALSE(s.busy());
    for (int i = 0; i < 20; ++i) {
        auto next = s.settings;
        next.synth.duration = .01 + i * .001;
        s.updateSettings(next);
        s.generate(true);
    }
    s.wait();
    EXPECT_DOUBLE_EQ(s.product->settings.synth.duration, .029);
    s.recallSlot(0);
    EXPECT_EQ(s.product, saved);
    s.applyDevice(d);
    EXPECT_EQ(s.product, saved);
    EXPECT_THROW(s.artifact("../escape.wav"), audio::Error);
    EXPECT_THROW(s.exportAudio("tone.wav", false, false, false), audio::Error);
    s.generate(true); // destructor joins/cancels a job still in flight
}

TEST_F(AudioTest, PresetTemporaryNeverOverwritesLoadedSource) {
    bass.reset();
    audio::DeviceSettings d;
    d.device = 0;
    Session s(d, directory);
    const auto source = directory / "preset.json.tmp";
    const auto pcm = tone();
    audio::writeWav(source, pcm, true);
    const auto original = audio::wavBytes(pcm, true);
    s.load(source);
    s.wait();
    savePreset(s.artifact("preset.json"), s.settings);
    std::ifstream in(source, std::ios::binary);
    const std::vector<uint8_t> actual((std::istreambuf_iterator<char>(in)), {});
    EXPECT_EQ(actual, original);
    EXPECT_NO_THROW(loadPreset(directory / "preset.json"));
    // A failed rename must also leave unrelated files and directories intact.
    fs::create_directory(directory / "occupied");
    EXPECT_THROW(savePreset(directory / "occupied", s.settings), fs::filesystem_error);
    EXPECT_TRUE(fs::is_directory(directory / "occupied"));
    EXPECT_TRUE(fs::exists(source));
    for (const auto &entry : fs::directory_iterator(directory)) {
        EXPECT_EQ(entry.path().filename().string().find(".tmp-"), std::string::npos);
    }
}

TEST_F(AudioTest, RejectedLiveFxPreservesSettingsAndPendingJob) {
    bass.reset();
    audio::DeviceSettings d;
    d.device = 0;
    Session s(d, directory);
    audio::writeWav(directory / "tone.wav", tone(), true);
    s.load(directory / "tone.wav");
    s.wait();
    s.generate(true);
    s.wait();
    s.playPreview("live");
    const auto before = encode(settingsJson(s.settings));
    const auto channel = s.preview.handle();
    s.generate(true);
    const auto revision = s.revision();
    auto invalid = s.settings;
    invalid.fx.pre.frequency = 30000;
    EXPECT_THROW(s.updateSettings(invalid), audio::Error);
    EXPECT_EQ(encode(settingsJson(s.settings)), before);
    EXPECT_EQ(s.preview.handle(), channel);
    EXPECT_EQ(s.revision(), revision);
    s.wait();
    EXPECT_FALSE(s.product->processed.samples.empty());
    EXPECT_NO_THROW(s.playPreview("live"));
}

TEST_F(AudioTest, SelectionAndDeviceChangesPreserveActualLoop) {
    bass.reset();
    audio::DeviceSettings d;
    d.device = 0;
    Session s(d, directory);
    audio::writeWav(directory / "tone.wav", tone(), true);
    s.load(directory / "tone.wav");
    s.wait();
    s.setMusicLoop(.05, .1, true);
    s.setSelection(.2, .3);
    EXPECT_THROW(s.setSelection(.4, .2), audio::Error);
    EXPECT_DOUBLE_EQ(s.selectionStart, .2);
    EXPECT_DOUBLE_EQ(s.selectionEnd, .3);
    auto checkLoop = [&] {
        s.music.position(.2);
        ASSERT_TRUE(BASS_ChannelUpdate(s.music.handle(), 400));
        const auto bytes = BASS_ChannelGetPosition(s.music.handle(), BASS_POS_BYTE | BASS_POS_DECODE);
        ASSERT_NE(bytes, QWORD(-1));
        const double decoded = BASS_ChannelBytes2Seconds(s.music.handle(), bytes);
        EXPECT_GE(decoded, .2 - 1. / 44100);
        EXPECT_LE(decoded, .3 + 1. / 44100);
        EXPECT_EQ(s.music.loopError(), 0);
    };
    checkLoop();
    s.applyDevice(d);
    EXPECT_TRUE(s.musicLoop);
    checkLoop();
}

TEST_F(AudioTest, RepeatedIdleWorkerShutdown) {
    bass.reset();
    audio::DeviceSettings d;
    d.device = 0;
    for (int i = 0; i < 30; ++i) {
        Session s(d, directory);
        if (i % 2) {
            std::this_thread::yield();
        }
    }
}
} // namespace
