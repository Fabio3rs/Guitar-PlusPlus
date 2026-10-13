#include "CMidiChart.h"
#include "MidiFixture.h"
#include <gtest/gtest.h>

namespace {
using namespace midi_fixture;

class MidiChartTest : public ::testing::Test {
  protected:
    std::filesystem::path directory;
    MidiChart chart;
    std::string error;

    void SetUp() override {
        directory =
            std::filesystem::temp_directory_path() /
            ("gpp-midi-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
    }

    void TearDown() override { std::filesystem::remove_all(directory); }

    bool load(const std::string &bytes) {
        write(directory / "notes.mid", bytes);
        return readMidiChart(directory / "notes.mid", chart, error);
    }

    const MidiChart::Instrument &expert() {
        return chart.instruments.at("[ExpertSingle]");
    }
};

TEST_F(MidiChartTest, TypeOneFindsAllDifficultiesAndBassWithoutAudio) {
    Track tempo("TEMPO"), guitar, bass("PART BASS"), drums("PART DRUMS");
    tempo.tempo(0, 500000);
    for (int d = 0; d < 4; ++d) {
        guitar.note(480, 0, 60 + 12 * d);
        bass.note(960, 240, 64 + 12 * d);
    }
    drums.note(0, 10, 96);
    ASSERT_TRUE(load(smf({tempo, guitar, bass, drums}))) << error;
    ASSERT_EQ(chart.instruments.size(), 8u);
    EXPECT_EQ(chart.trackNames.size(), 4u);
    EXPECT_DOUBLE_EQ(expert().notes[0].time, 0.5);
    EXPECT_EQ(expert().notes[0].type & 31, nf_green);
    EXPECT_EQ(chart.source.ppq, 480);
}

TEST_F(MidiChartTest,
       TempoSegmentsAndSustainEndpointsPreserveMusicalPositions) {
    for (const unsigned ppq : {96, 192, 480, 960}) {
        Track tempo("TEMPO"), guitar;
        tempo.tempo(0, 500000);
        tempo.tempo(ppq, 1000000);
        tempo.tempo(2 * ppq, 250000);
        guitar.note(ppq / 2, 3 * ppq, 96);
        guitar.note(ppq, 0, 97);
        guitar.note(2 * ppq, 0, 98);
        ASSERT_TRUE(load(smf({tempo, guitar}, ppq))) << error;
        const auto &part = expert();
        ASSERT_EQ(part.notes.size(), 3u);
        EXPECT_NEAR(part.notes[0].time, 0.25, 1e-12);
        EXPECT_NEAR(part.notes[0].lTime, 1.625, 1e-12);
        EXPECT_NEAR(part.notes[1].time, 0.5, 1e-12);
        EXPECT_NEAR(part.notes[2].time, 1.5, 1e-12);
        EXPECT_EQ(part.source.gems[0].endTick, ppq * 7 / 2);
        ASSERT_EQ(part.source.tempos.size(), 3u);
        EXPECT_EQ(part.source.tempos[1].tick, ppq);
    }
}

TEST_F(MidiChartTest, MissingTempoUses120AndCoincidentTemposUseFileOrder) {
    Track guitar;
    guitar.note(480, 0, 96);
    ASSERT_TRUE(load(smf({guitar}, 480, 0))) << error;
    EXPECT_DOUBLE_EQ(expert().notes[0].time, 0.5);
    EXPECT_DOUBLE_EQ(chart.bpm[0].lTime, 120);
    Track tempo("TEMPO");
    tempo.tempo(0, 500000);
    tempo.tempo(0, 1000000);
    ASSERT_TRUE(load(smf({tempo, guitar}))) << error;
    EXPECT_DOUBLE_EQ(expert().notes[0].time, 1.0);
    EXPECT_DOUBLE_EQ(chart.bpm[0].lTime, 60);
    EXPECT_EQ(chart.source.tempos.size(), 2u);
}

TEST_F(MidiChartTest, ZeroVelocityZeroLengthAndCoincidentReattacks) {
    Track guitar;
    guitar.note(0, 0, 96, true);
    guitar.note(480, 480, 97, true);
    guitar.note(960, 240, 97);
    ASSERT_TRUE(load(smf({guitar}))) << error;
    ASSERT_EQ(expert().notes.size(), 3u);
    EXPECT_DOUBLE_EQ(expert().notes[0].lTime, 0);
    EXPECT_DOUBLE_EQ(expert().notes[1].lTime, 0.5);
    EXPECT_DOUBLE_EQ(expert().notes[2].lTime, 0.25);
}

TEST_F(MidiChartTest, RunningStatusAndIndependentChannelsPairCorrectly) {
    Track guitar;
    guitar.events.push_back({0, data({0x90, 96, 100})});
    guitar.events.push_back({0, data({97, 100})});
    guitar.events.push_back({200, data({96, 0})});
    guitar.events.push_back({300, data({97, 0})});
    guitar.note(480, 180, 98, false, 1);
    guitar.note(480, 240, 99, false, 2);
    ASSERT_TRUE(load(smf({guitar}))) << error;
    ASSERT_EQ(expert().notes.size(), 2u);
    EXPECT_EQ(expert().notes[0].type & 31, nf_green | nf_red);
    EXPECT_NEAR(expert().notes[0].lTime, 300.0 / 960.0, 1e-12);
    EXPECT_DOUBLE_EQ(expert().notes[1].lTime, 0.25);
    EXPECT_EQ(expert().source.gems.size(), 4u);
}

TEST_F(MidiChartTest, ChordsUseMaximumSustainButRetainOriginalEnds) {
    Track guitar;
    guitar.note(0, 480, 96);
    guitar.note(0, 960, 97);
    guitar.note(480, 159, 98);
    guitar.note(960, 160, 99);
    ASSERT_TRUE(load(smf({guitar}))) << error;
    ASSERT_EQ(expert().notes.size(), 3u);
    EXPECT_DOUBLE_EQ(expert().notes[0].lTime, 1);
    EXPECT_DOUBLE_EQ(expert().notes[1].lTime, 0);
    EXPECT_NEAR(expert().notes[2].lTime, 1.0 / 6, 1e-12);
    EXPECT_EQ(expert().source.gems[0].endTick, 480);
    EXPECT_EQ(expert().source.gems[1].endTick, 960);
    EXPECT_FALSE(expert().source.warnings.empty());
}

TEST_F(MidiChartTest, NaturalHopoUsesInclusiveTickThresholdAndChordSubsetRule) {
    Track guitar;
    guitar.note(0, 0, 96);
    guitar.note(161, 0, 97);
    guitar.note(323, 0, 98);
    guitar.note(484, 0, 96);
    guitar.note(484, 0, 97);
    guitar.note(645, 0, 96);
    guitar.note(806, 0, 98);
    guitar.note(900, 0, 98);
    ASSERT_TRUE(load(smf({guitar}))) << error;
    const bool strum[] = {true, false, true, true, true, false, true};
    ASSERT_EQ(expert().notes.size(), 7u);
    for (size_t i = 0; i < 7; ++i) {
        EXPECT_EQ((expert().notes[i].type & nf_not_hopo) != 0, strum[i]) << i;
    }
    Track tempo("TEMPO");
    tempo.tempo(100, 2000000);
    tempo.tempo(500, 100000);
    ASSERT_TRUE(load(smf({tempo, guitar}))) << error;
    for (size_t i = 0; i < 7; ++i) {
        EXPECT_EQ((expert().notes[i].type & nf_not_hopo) != 0, strum[i]) << i;
    }
}

TEST_F(MidiChartTest, ExplicitModifiersOverrideNaturalRuleOnHalfOpenIntervals) {
    Track guitar;
    guitar.note(0, 0, 96);
    guitar.note(100, 0, 97);
    guitar.note(200, 0, 98);
    guitar.note(500, 0, 98);
    guitar.note(600, 0, 99);
    guitar.note(100, 100, 102); // Force strum ends before tick 200.
    guitar.note(500, 1, 101);   // Repeated note forced HOPO.
    guitar.note(600, 1, 104);
    ASSERT_TRUE(load(smf({guitar}))) << error;
    EXPECT_NE(expert().notes[1].type & nf_not_hopo, 0);
    EXPECT_EQ(expert().notes[2].type & nf_not_hopo, 0);
    EXPECT_EQ(expert().notes[3].type & nf_not_hopo, 0);
    EXPECT_NE(expert().notes[4].type & noteTap, 0);
}

TEST_F(MidiChartTest, SongIniOverridesAndDelayAreImportOnly) {
    write(directory / "song.ini",
          "[song]\nname = Ação\nartist = Test\ncharter = Author\n"
          "delay = -250\nhopo_frequency = 170\neighthnote_hopo = "
          "true\nsustain_cutoff_threshold = 0\n");
    Track guitar;
    guitar.note(0, 1, 96);
    guitar.note(170, 0, 97);
    guitar.note(341, 0, 98);
    ASSERT_TRUE(load(smf({guitar}))) << error;
    EXPECT_EQ(chart.name, "Ação");
    EXPECT_EQ(chart.artist, "Test");
    EXPECT_EQ(chart.charter, "Author");
    EXPECT_DOUBLE_EQ(expert().notes[0].time, -0.25);
    EXPECT_GT(expert().notes[0].lTime, 0);
    EXPECT_EQ(expert().source.hopoThresholdTicks, 170);
    EXPECT_EQ(expert().notes[1].type & nf_not_hopo, 0);
    EXPECT_NE(expert().notes[2].type & nf_not_hopo, 0);
}

TEST_F(MidiChartTest,
       PlusIncludesStartExcludesEndAndHandlesConsecutivePhrases) {
    Track guitar;
    for (int tick : {0, 100, 200, 300, 400}) {
        guitar.note(tick, 0, 96);
    }
    guitar.note(0, 200, 116);
    guitar.note(200, 200, 116);
    ASSERT_TRUE(load(smf({guitar}))) << error;
    ASSERT_EQ(expert().plus.size(), 2u);
    EXPECT_EQ(expert().plus[0].firstNote, 0u);
    EXPECT_EQ(expert().plus[0].lastNote, 1u);
    EXPECT_EQ(expert().plus[1].firstNote, 2u);
    EXPECT_EQ(expert().plus[1].lastNote, 3u);
}

TEST_F(MidiChartTest, LegacyPlus103AndExplicit116DistinguishSolo) {
    Track guitar;
    guitar.note(0, 0, 96);
    guitar.note(0, 480, 103);
    ASSERT_TRUE(load(smf({guitar}))) << error;
    ASSERT_EQ(expert().plus.size(), 1u);
    write(directory / "song.ini", "[song]\nstar_power_note = 116\n");
    ASSERT_TRUE(load(smf({guitar}))) << error;
    EXPECT_TRUE(expert().plus.empty());
}

TEST_F(MidiChartTest, EnhancedOpensAndSysexPreserveOpenStrumHopoAndTaps) {
    Track guitar;
    guitar.meta(0, 1, "[ENHANCED_OPENS]");
    guitar.note(0, 0, 96);
    guitar.note(120, 0, 95);
    guitar.note(240, 0, 95);
    guitar.note(360, 0, 97);
    guitar.sysex(360, 3, 1, true);
    guitar.sysex(361, 3, 1, false);
    guitar.sysex(360, 0xff, 4, true);
    guitar.sysex(361, 0xff, 4, false);
    ASSERT_TRUE(load(smf({guitar}))) << error;
    EXPECT_EQ(expert().notes[1].type & (nf_open | nf_not_hopo), nf_open);
    EXPECT_EQ(expert().notes[2].type & (nf_open | nf_not_hopo),
              nf_open | nf_not_hopo);
    EXPECT_EQ(expert().notes[3].type & (nf_open | noteTap), nf_open | noteTap);
    Track animation;
    animation.note(0, 100, 59);
    animation.note(480, 0, 96);
    ASSERT_TRUE(load(smf({animation}))) << error;
    EXPECT_EQ(chart.instruments.size(), 1u);
    EXPECT_EQ(expert().notes.size(), 1u);
}

TEST_F(MidiChartTest, UnsupportedTracksAndAbsentDifficultiesAreNotInvented) {
    Track bass("PART BASS"), drums("PART DRUMS"), events("EVENTS");
    bass.note(0, 0, 96);
    drums.note(0, 0, 96);
    events.meta(0, 1, "[section Intro]");
    ASSERT_TRUE(load(smf({bass, drums, events}))) << error;
    EXPECT_EQ(chart.instruments.size(), 1u);
    EXPECT_EQ(chart.instruments.count("[ExpertDoubleBass]"), 1u);
    EXPECT_EQ(chart.instruments.count("[ExpertSingle]"), 0u);
    ASSERT_TRUE(load(smf({drums, events}))) << error;
    EXPECT_TRUE(chart.instruments.empty());
    EXPECT_FALSE(chart.source.events.empty());
}

TEST_F(MidiChartTest, TruncatedAndMalformedFilesFailAndClearPreviousOutput) {
    Track guitar;
    guitar.note(0, 480, 96);
    const auto valid = smf({guitar});
    ASSERT_TRUE(load(valid));
    for (size_t size = 0; size < valid.size(); ++size) {
        EXPECT_FALSE(load(valid.substr(0, size))) << size;
        EXPECT_TRUE(chart.instruments.empty());
        EXPECT_FALSE(error.empty());
    }
    EXPECT_FALSE(load(smf({guitar}, 0)));
    EXPECT_FALSE(load(smf({guitar}, 0xe728)));
    EXPECT_FALSE(load(smf({guitar}, 480, 2)));
    EXPECT_FALSE(load(smf({})));
    auto corrupt = valid;
    corrupt[18] = 0x7f;
    EXPECT_FALSE(load(corrupt));
    Track unclosed;
    unclosed.events.push_back({0, data({0x90, 96, 100})});
    EXPECT_FALSE(load(smf({unclosed})));
    Track zeroTempo;
    zeroTempo.tempo(0, 0);
    EXPECT_FALSE(load(smf({zeroTempo})));
}
} // namespace
