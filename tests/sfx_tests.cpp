#include "audio/sfx.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>

namespace sfx = gpp::audio::sfx;
namespace audio = gpp::audio;
namespace fs = std::filesystem;
namespace {
const std::string valid =
    R"(return {schemaVersion=1, id="test", duration=.1, layers={{type="noise"}}})";

sfx::SoundDefinition steady() {
    sfx::SoundDefinition s;
    s.channels = 1;
    s.duration = .5;
    s.outputGainDb = 0;
    s.layers[0].envelope = {0, 0, 1, 0, false};
    return s;
}

double rms(const audio::PcmBuffer &pcm, size_t begin, size_t end) {
    double power = 0;
    for (size_t i = begin; i < end; ++i) {
        power += double(pcm.samples[i]) * pcm.samples[i];
    }

    return std::sqrt(power / (end - begin));
}

class SfxFiles : public testing::Test {
  protected:
    fs::path root;
    void SetUp() override {
        root =
            fs::temp_directory_path() /
            ("gpp-sfx-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
    }

    void TearDown() override { fs::remove_all(root); }
};

uint32_t u32(const std::vector<uint8_t> &b, size_t at) {
    return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 |
           uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}
} // namespace

TEST(Sfx, ValidLuaAndRoundtrip) {
    auto s = sfx::parse(valid);
    EXPECT_EQ(s.id, "test");
    EXPECT_EQ(s.layers[0].type, sfx::Generator::Noise);
    s.description = "Line one\nLine two \\\"á";
    const auto text = sfx::serialize(s);
    const auto reloaded = sfx::parse(text);
    EXPECT_EQ(reloaded.description, s.description);
    EXPECT_EQ(sfx::serialize(reloaded), text);
    EXPECT_EQ(sfx::render(s).samples, sfx::render(reloaded).samples);
}

TEST(Sfx, RejectsBadTypesUnknownFieldsAndSparseArrays) {
    for (const auto *text :
         {"return nil", "return {schemaVersion=2}",
          "return "
          "{schemaVersion=1,id='x',duration='1',layers={{type='noise'}}}",
          "return {schemaVersion=1,id='x',duration=.1,layers={{type='wrong'}}}",
          "return "
          "{schemaVersion=1,id='../x',duration=.1,layers={{type='noise'}}}",
          "return "
          "{schemaVersion=1,id='x',duration=.1,typo=2,layers={{type='noise'}}}",
          "return "
          "{schemaVersion=1,id='x',duration=.1,layers={[1]={type='noise'},[3]={"
          "type='noise'}}}",
          "return "
          "{schemaVersion=1,id='x',duration=.1,layers={{type='noise',enabled=1}"
          "}}",
          "return "
          "{schemaVersion=1,id='x',duration=.1,layers={{type='noise',filter={q="
          "'2'}}}}"}) {
        EXPECT_THROW(sfx::parse(text), audio::Error) << text;
    }

    try {
        sfx::parse("return "
                   "{schemaVersion=1,id='x',duration=.1,layers={{type='noise',"
                   "pan=2}}}",
                   "broken.lua");
        FAIL();
    } catch (const audio::Error &e) {
        EXPECT_NE(std::string(e.what()).find("broken.lua: layers[1].pan"),
                  std::string::npos);
    }
}

TEST(Sfx, RestrictedLuaAndFiniteNumbers) {
    for (const auto *text :
         {"while true do end", "return os.execute('anything')",
          "return io.open('anything')", "return require('math')",
          "local a={} for i=1,99999 do a[i]={} end return a",
          "local s='x'; for i=1,24 do s=s..s end return s",
          "return "
          "{schemaVersion=1,id='x',duration=0/0,layers={{type='noise'}}}",
          "return "
          "{schemaVersion=1,id='x',duration=.1,layers={{type='noise',gain=1/"
          "0}}}"}) {
        EXPECT_THROW(sfx::parse(text), audio::Error);
    }

    EXPECT_THROW(sfx::parse(std::string(256 * 1024 + 1, ' ')), audio::Error);
    auto s = steady();
    s.layers[0].envelope.attack = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(sfx::render(s), audio::Error);
    s = steady();
    s.loop.crossfade = std::numeric_limits<double>::infinity();
    EXPECT_THROW(sfx::validate(s), audio::Error);
}

TEST(Sfx, BoundsAndNyquist) {
    auto s = steady();
    s.duration = 31;
    EXPECT_THROW(sfx::render(s), audio::Error);
    s = steady();
    s.layers.resize(33);
    EXPECT_THROW(sfx::render(s), audio::Error);
    s = steady();
    s.layers[0].frequency.startHz = s.sampleRate * .5;
    EXPECT_THROW(sfx::render(s), audio::Error);
    s = steady();
    s.layers[0].frequency.startHz = s.sampleRate * .4;
    s.layers[0].modulation.pitchCents = 1200;
    EXPECT_THROW(sfx::validate(s), audio::Error);
    s = steady();
    s.layers[0].start = s.duration;
    EXPECT_THROW(sfx::validate(s), audio::Error);
}

TEST(Sfx, RateDurationChannelsPanAndShortEnvelopes) {
    for (unsigned rate : {8000u, 22050u, 44100u, 48000u, 96000u}) {
        for (unsigned channels : {1u, 2u}) {
            auto s = steady();
            s.sampleRate = rate;
            s.channels = channels;
            s.duration = .001;
            s.layers[0].pan = -1;
            s.layers[0].envelope = {5, 2, .3, 3, true};
            const auto pcm = sfx::render(s);
            EXPECT_EQ(pcm.frames(), size_t(std::llround(s.duration * rate)));
            EXPECT_EQ(pcm.rate, rate);
            EXPECT_EQ(pcm.channels, channels);
            EXPECT_EQ(pcm.samples.front(), 0);
            EXPECT_EQ(pcm.samples.back(), 0);
            EXPECT_GT(audio::levels(pcm).rms, 0);
            if (channels == 2) {
                for (size_t i = 1; i < pcm.samples.size(); i += 2) {
                    EXPECT_EQ(pcm.samples[i], 0);
                }
            }
        }
    }
}

TEST(Sfx, SeedReproductionNoiseAndPluck) {
    for (auto generator : {sfx::Generator::Noise, sfx::Generator::Pluck}) {
        for (auto color :
             {sfx::Color::White, sfx::Color::Pink, sfx::Color::Brown}) {
            auto s = steady();
            s.layers[0].type = generator;
            s.layers[0].color = color;
            auto first = sfx::render(s);
            EXPECT_EQ(first.samples, sfx::render(s).samples);
            EXPECT_EQ(sfx::fingerprint(first),
                      sfx::fingerprint(sfx::render(s)));
            ++s.seed;
            EXPECT_NE(first.samples, sfx::render(s).samples);
            EXPECT_TRUE(std::all_of(first.samples.begin(), first.samples.end(),
                                    [](float x) { return std::isfinite(x); }));
            EXPECT_GT(audio::levels(first).rms, .0001);
            double dc = 0;
            for (float x : first.samples) {
                dc += x;
            }

            EXPECT_LT(std::abs(dc / first.samples.size()), .01);
        }
    }
}

TEST(Sfx, OscillatorPitchAndWaveforms) {
    auto s = steady();
    auto sine = sfx::render(s);
    int crossings = 0;
    for (size_t i = 1001; i < sine.frames() - 1000; ++i) {
        crossings += sine.samples[i - 1] <= 0 && sine.samples[i] > 0;
    }

    EXPECT_NEAR(double(crossings) /
                    (double(sine.frames() - 2000) / s.sampleRate),
                440, 3);
    for (auto w : {sfx::Wave::Triangle, sfx::Wave::Sawtooth, sfx::Wave::Square,
                   sfx::Wave::Pulse}) {
        s.layers[0].waveform = w;
        s.layers[0].duty = .2;
        auto pcm = sfx::render(s);
        EXPECT_NE(pcm.samples, sine.samples);
        EXPECT_GT(audio::levels(pcm).rms, .05);
        EXPECT_LE(audio::levels(pcm).peak, .950001);
    }
}

TEST(Sfx, EnvelopePluckDecayAndDisabledLayers) {
    auto s = steady();
    s.layers[0].envelope = {.002, .2, 0, .1, true};
    auto pcm = sfx::render(s);
    EXPECT_GT(rms(pcm, 100, 1000), 10 * rms(pcm, 15000, 20000));
    s = steady();
    s.layers[0].type = sfx::Generator::Pluck;
    s.layers[0].damping = .95;
    pcm = sfx::render(s);
    EXPECT_GT(rms(pcm, 100, 1000), 3 * rms(pcm, 15000, 20000));
    s.layers[0].enabled = false;
    EXPECT_EQ(audio::levels(sfx::render(s)).peak, 0);
}

TEST(Sfx, FiltersAttenuateAndValidate) {
    auto s = steady();
    const auto base = audio::levels(sfx::render(s)).rms;
    s.layers[0].filter = {sfx::FilterType::Lowpass, 60, .707};
    EXPECT_LT(audio::levels(sfx::render(s)).rms, base * .1);
    s.layers[0].filter = {sfx::FilterType::Highpass, 6000, .707};
    EXPECT_LT(audio::levels(sfx::render(s)).rms, base * .1);
    s.layers[0].filter = {sfx::FilterType::Bandpass, 440, 2};
    EXPECT_GT(audio::levels(sfx::render(s)).rms, base * .8);
    s.layers[0].filter.q = 0;
    EXPECT_THROW(sfx::render(s), audio::Error);
    s.layers[0].filter.q = 1;
    s.layers[0].filter.cutoffHz = s.sampleRate;
    EXPECT_THROW(sfx::render(s), audio::Error);
}

TEST(Sfx, FinalPeakLimitDoesNotBoostQuietMixes) {
    auto s = steady();
    s.layers[0].gain = .01;
    EXPECT_LT(audio::levels(sfx::render(s)).peak, .02);
    s.layers[0].gain = 4;
    s.outputGainDb = 12;
    s.layers.resize(8, s.layers[0]);
    s.peakLimit = .6;
    EXPECT_NEAR(audio::levels(sfx::render(s)).peak, .6, 1e-6);
}

TEST(Sfx, AmbientLoopsPreserveDurationAndContinuity) {
    for (double begin : {0., .1}) {
        auto s = steady();
        s.layers[0].frequency = {61.3, 61.3, sfx::Curve::Linear};
        s.layers[0].type = sfx::Generator::Noise;
        s.layers[0].color = sfx::Color::Brown;
        s.layers[0].filter = {sfx::FilterType::Lowpass, 300, .707};
        s.loop = {true, begin, .45, .05};
        auto pcm = sfx::render(s);
        EXPECT_EQ(pcm.frames(), 22050u);
        EXPECT_EQ(pcm.loop.begin, size_t(std::llround(begin * s.sampleRate)));
        EXPECT_EQ(pcm.loop.end, size_t(std::llround(.45 * s.sampleRate)));
        EXPECT_LT(std::abs(pcm.samples[pcm.loop.begin] -
                           pcm.samples[pcm.loop.end - 1]),
                  .01);
        s.loop.end = 0;
        pcm = sfx::render(s);
        EXPECT_EQ(pcm.loop.end, pcm.frames());
        EXPECT_LT(std::abs(pcm.samples[pcm.loop.begin] - pcm.samples.back()),
                  .01);
    }

    auto s = steady();
    s.loop = {true, .2, .1, .01};
    EXPECT_THROW(sfx::render(s), audio::Error);
}

TEST(Sfx, CancellationAndMonotonicProgress) {
    auto s = steady();
    s.layers.resize(4, s.layers[0]);
    double last = -1;
    EXPECT_THROW(sfx::render(s,
                             [&](double p) {
                                 EXPECT_GE(p, last);
                                 last = p;
                                 if (p > .2) {
                                     throw audio::Error("Cancelled");
                                 }
                             }),
                 audio::Error);
    EXPECT_GT(last, .2);
    last = -1;
    sfx::render(s, [&](double p) {
        EXPECT_GE(p, last);
        last = p;
    });
    EXPECT_EQ(last, 1);
}

TEST_F(SfxFiles, FileRoundtripWavFormatsLoopAndSafeReplacement) {
    auto s = steady();
    s.loop = {true, .05, .4, .01};
    sfx::save(root / "recipe.lua", s);
    EXPECT_EQ(sfx::serialize(sfx::load(root / "recipe.lua")),
              sfx::serialize(s));
    EXPECT_THROW(sfx::save(root / "recipe.lua", s), audio::Error);
    EXPECT_NO_THROW(sfx::save(root / "recipe.lua", s, true));
    auto pcm = sfx::render(s);
    for (bool floats : {false, true}) {
        auto path = root / (floats ? "float.wav" : "pcm16.wav");
        sfx::exportWav(path, pcm, floats);
        auto bytes = audio::wavBytes(pcm, floats);
        EXPECT_EQ(fs::file_size(path), bytes.size());
        EXPECT_EQ(bytes[20], floats ? 3 : 1);
        EXPECT_EQ(bytes[34], floats ? 32 : 16);
        EXPECT_EQ(u32(bytes, 24), s.sampleRate);
        const size_t smpl = 44 + pcm.samples.size() * (floats ? 4 : 2);
        EXPECT_EQ(std::string(bytes.begin() + smpl, bytes.begin() + smpl + 4),
                  "smpl");
        EXPECT_EQ(u32(bytes, smpl + 52), pcm.loop.begin);
        EXPECT_EQ(u32(bytes, smpl + 56), pcm.loop.end - 1);
        EXPECT_THROW(sfx::exportWav(path, pcm, floats), audio::Error);
        auto replacement = pcm;
        for (auto &sample : replacement.samples) {
            sample *= .5f;
        }
        EXPECT_THROW(sfx::exportWav(path, replacement, floats, true,
                                    [](double p) {
                                        if (p == 1) {
                                            throw audio::Error("Cancelled");
                                        }
                                    }),
                     audio::Error);
        std::ifstream in(path, std::ios::binary);
        const std::vector<uint8_t> kept((std::istreambuf_iterator<char>(in)),
                                        {});
        EXPECT_EQ(kept, bytes);

        EXPECT_THROW(sfx::exportWav(root / "cancelled.wav", pcm, floats, false,
                                    [](double p) {
                                        if (p == 1) {
                                            throw audio::Error("Cancelled");
                                        }
                                    }),
                     audio::Error);
        EXPECT_FALSE(fs::exists(root / "cancelled.wav"));
    }

    EXPECT_EQ(
        std::distance(fs::directory_iterator(root), fs::directory_iterator{}),
        3);
}

TEST(Sfx, EntireLibraryRenders) {
    const auto files = sfx::recipes(GPP_SFX_SPECS);
    ASSERT_EQ(files.size(), 17u);
    for (const auto &file : files) {
        SCOPED_TRACE(file.string());
        const auto sound = sfx::load(file);
        const auto pcm = sfx::render(sound);
        EXPECT_EQ(pcm.frames(),
                  size_t(std::llround(sound.duration * sound.sampleRate)));
        const auto measured = audio::levels(pcm);
        EXPECT_GT(measured.rms, .000001);
        EXPECT_LE(measured.peak, sound.peakLimit + 1e-6);
        EXPECT_EQ(measured.clipped, 0u);
    }
}
