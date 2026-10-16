#include "MidiFixture.h"
#include "midi_synth.h"
#include "synth_process.h"
#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
using namespace ChartStudio;
class SynthTest : public testing::Test {
  protected:
    fs::path root;
    Session session;
    SynthConfig config;
    void SetUp() override {
        root =
            fs::temp_directory_path() /
            ("gpp synth ' test-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(root);
        midi_fixture::Track t("Guitar"), bass("Bass");
        t.tempo(0, 500000);
        t.tempo(480, 1000000);
        t.events.push_back({0, midi_fixture::data({0xc0, 30})});
        t.events.push_back({240, midi_fixture::data({0xb0, 64, 127})});
        t.events.push_back({300, midi_fixture::data({0xe0, 0, 80})});
        t.events.push_back({350, midi_fixture::data({0xa0, 60, 40})});
        t.events.push_back({400, midi_fixture::data({0xd0, 50})});
        t.events.push_back({960, midi_fixture::data({0xb0, 64, 0})});
        t.note(480, 480, 60);
        bass.note(0, 960, 48);
        midi_fixture::write(root / "input.mid", midi_fixture::smf({t, bass}));
        session.openMidi(root / "input.mid");
        session.state.settings.track = 0;
        config.output = (root / "output").u8string();
        config.sampleRate = 16000;
        config.tail = .25;
#ifdef GPP_TEST_SOUNDFONT
        config.soundfont = GPP_TEST_SOUNDFONT;
#endif
    }
    void TearDown() override { fs::remove_all(root); }
    SynthStem stem() { return {"guitar", true, {{0, -1, -1}}}; }
};
TEST_F(SynthTest, ScheduleUsesChartTempoScaleAndOffset) {
    session.setAlignment(1.2, .1, "");
    auto p = synthPlan(session.state, stem(), config);
    bool note = false, cc = false, bend = false, pressure = false, poly = false;
    for (auto e : p.events) {
        if (e.status == 0x90) {
            note = true;
            EXPECT_EQ(e.sample, std::llround(audioSeconds(session.state, 480) *
                                             config.sampleRate));
        }
        if (e.status == 0xb0) {
            cc = true;
        }
        if (e.status == 0xe0) {
            bend = true;
        }
        if (e.status == 0xd0) {
            pressure = true;
        }
        if (e.status == 0xa0) {
            poly = true;
        }
    }
    EXPECT_TRUE(note && cc && bend && pressure && poly);
    EXPECT_EQ(p.frames,
              std::llround((audioSeconds(session.state, 960) + config.tail) *
                           config.sampleRate));
}
TEST_F(SynthTest, NegativeOffsetKeepsPrerollEvents) {
    session.setAlignment(1, -1, "");
    auto p = synthPlan(session.state, stem(), config);
    EXPECT_LT(p.events.front().sample, 0);
    EXPECT_EQ(p.frames, 12000);
}
TEST_F(SynthTest, SourcesDeduplicatedAcrossDifficulties) {
    Part part = static_cast<Part>(session.state);
    part.settings.difficulty = "hard";
    part.partId = 2;
    session.state.otherParts.push_back(part);
    auto stems = synthSelection(session.state, "chart");
    ASSERT_EQ(stems.size(), 1u);
    EXPECT_EQ(stems[0].sources.size(), 1u);
    EXPECT_EQ(synthSelection(session.state, "all").size(), 2u);
}
TEST_F(SynthTest, SharedChannelsAreIsolatedAndSelectionFilters) {
    auto st = stem();
    st.sources.push_back({1, 0, -1});
    auto p = synthPlan(session.state, st, config);
    EXPECT_EQ(p.drums.size(), 2u);
    st.sources = {{0, 1, -1}};
    EXPECT_THROW(synthPlan(session.state, st, config), std::runtime_error);
    st.sources = {{0, 0, 30}};
    p = synthPlan(session.state, st, config);
    EXPECT_EQ(p.drums.size(), 1u);
}
TEST_F(SynthTest, ConfigurationPersistsWithoutChangingChartSettings) {
    session.state.synth = config;
    session.state.synth.stems = {stem()};
    session.save(root / "project.gppchartproj");
    auto loaded = loadProject(root / "project.gppchartproj");
    EXPECT_EQ(loaded.synth.sampleRate, 16000);
    EXPECT_EQ(loaded.synth.stems[0].sources[0].track, 0);
    EXPECT_EQ(loaded.settings.track, 0);
}
TEST_F(SynthTest, OriginalMidiMustRemainUnchanged) {
    std::ofstream out(root / "input.mid", std::ios::app);
    out.put('x');
    out.close();
    EXPECT_THROW(synthPlan(session.state, stem(), config), std::runtime_error);
}
TEST_F(SynthTest, MissingEncoderIsAnExplicitError) {
    EXPECT_THROW(synthProcess({"gpp-nonexistent-encoder-65732"}, {}),
                 std::runtime_error);
}
TEST_F(SynthTest, OfflineWaveAndCancellation) {
    if (!synthAvailable()) {
        EXPECT_THROW(renderStems(session.state, config), std::runtime_error);
        return;
    }
    if (config.soundfont.empty()) {
        GTEST_SKIP() << "Set GPP_TEST_SOUNDFONT for offline rendering tests";
    }
    config.stems = {stem(), {"bass", true, {{1, -1, -1}}}};
    renderStems(session.state, config);
    const auto size = 44 + 4 * synthPlan(session.state, stem(), config).frames;
    EXPECT_EQ(fs::file_size(root / "output/guitar.wav"), uint64_t(size));
    EXPECT_EQ(fs::file_size(root / "output/bass.wav"), uint64_t(size));
    std::ifstream audio(root / "output/guitar.wav", std::ios::binary);
    audio.seekg(44);
    int earlyPeak = 0, musicPeak = 0;
    for (int frame = 0; frame < 16000; ++frame) {
        for (int channel = 0; channel < 2; ++channel) {
            unsigned lo = unsigned(audio.get()), hi = unsigned(audio.get());
            int value = int16_t(lo | (hi << 8));
            if (frame < 6400) {
                earlyPeak = std::max(earlyPeak, std::abs(value));
            }
            if (frame > 9600) {
                musicPeak = std::max(musicPeak, std::abs(value));
            }
        }
    }
    EXPECT_LT(earlyPeak, 10);
    EXPECT_GT(musicPeak, 100);
    EXPECT_THROW(renderStems(session.state, config), std::runtime_error);
    config.output = (root / "cancel").u8string();
    int calls = 0;
    EXPECT_THROW(renderStems(session.state, config,
                             [&] {
                                 if (++calls > 4) {
                                     throw std::runtime_error("cancel");
                                 }
                             }),
                 std::runtime_error);
    EXPECT_TRUE(fs::is_empty(root / "cancel"));
    config.output = (root / "negative").u8string();
    session.setAlignment(1, -1, "");
    renderStems(session.state, config);
    EXPECT_EQ(fs::file_size(root / "negative/guitar.wav"), 44u + 4 * 12000);
}
TEST_F(SynthTest, OggOrCleanEncodingFailure) {
    if (!synthAvailable() || config.soundfont.empty()) {
        GTEST_SKIP();
    }
    config.stems = {stem()};
    config.format = "ogg";
    bool encoder = true;
    try {
        synthProcess({"ffmpeg", "-version"}, {});
    } catch (...) {
        encoder = false;
    }
    if (!encoder) {
        EXPECT_THROW(renderStems(session.state, config), std::runtime_error);
        EXPECT_TRUE(fs::is_empty(root / "output"));
        return;
    }
    renderStems(session.state, config);
    EXPECT_GT(fs::file_size(root / "output/guitar.ogg"), 100u);
}

TEST_F(SynthTest, SimultaneousChannelsPreserveOriginalEventOrder) {
    midi_fixture::Track t("Interleaved channels");
    t.tempo(0, 500000);
    t.events.push_back({480, midi_fixture::data({0x91, 67, 90})});
    t.events.push_back({480, midi_fixture::data({0x90, 60, 90})});
    t.events.push_back({960, midi_fixture::data({0x81, 67, 0})});
    t.events.push_back({960, midi_fixture::data({0x80, 60, 0})});
    midi_fixture::write(root / "interleaved.mid", midi_fixture::smf({t}));
    session.openMidi(root / "interleaved.mid");
    auto plan = synthPlan(session.state, stem(), config);
    std::vector<int> pitches;
    for (const auto &e : plan.events) {
        if (e.status == 0x90) {
            pitches.push_back(e.a);
        }
    }
    EXPECT_EQ(pitches, (std::vector<int>{67, 60}));
}

TEST_F(SynthTest, ProgramFilterKeepsPairedOffOfOverlappingPitch) {
    midi_fixture::Track t("Programs sharing a pitch");
    t.tempo(0, 500000);
    t.events.push_back({0, midi_fixture::data({0xc0, 1})});
    t.events.push_back({0, midi_fixture::data({0x90, 60, 100})});
    t.events.push_back({240, midi_fixture::data({0xc0, 0})});
    t.events.push_back({240, midi_fixture::data({0x90, 60, 100})});
    t.events.push_back({480, midi_fixture::data({0x80, 60, 0})});
    t.events.push_back({960, midi_fixture::data({0x90, 60, 0})});
    midi_fixture::write(root / "programs.mid", midi_fixture::smf({t}));
    session.openMidi(root / "programs.mid");
    auto p = synthPlan(session.state, {"selected", true, {{0, 0, 0}}}, config);
    std::vector<int64_t> ons, offs;
    for (const auto &e : p.events) {
        if (e.status == 0x90 && e.b) {
            ons.push_back(e.sample);
        }
        if (e.status == 0x80 || (e.status == 0x90 && !e.b)) {
            offs.push_back(e.sample);
        }
    }
    EXPECT_EQ(ons, (std::vector<int64_t>{4000}));
    EXPECT_EQ(offs, (std::vector<int64_t>{16000}));
}

TEST_F(SynthTest, UnmatchedProgramIsRejectedEvenAlongsideValidSource) {
    EXPECT_THROW(
        synthPlan(session.state, {"missing", true, {{0, 0, 99}}}, config),
        std::runtime_error);
    EXPECT_THROW(synthPlan(session.state,
                           {"mixed", true, {{0, 0, 30}, {0, 0, 99}}}, config),
                 std::runtime_error);
    if (!synthAvailable() || config.soundfont.empty()) {
        return;
    }
    config.stems = {stem(), {"missing", true, {{0, 0, 99}}}};
    EXPECT_THROW(renderStems(session.state, config), std::runtime_error);
    EXPECT_TRUE(fs::is_empty(root / "output"));
}
