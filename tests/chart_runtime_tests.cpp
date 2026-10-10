#include "CChart.h"
#include "CPlayer.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

namespace {
// These tests assert format/math contracts, not historical loader output.
// HOPO thresholds, chord sustain merging, open-note sustain handling and Plus
// boundary tolerances remain characterized by the separate evidence dumps.
class ChartRuntime : public ::testing::Test {
  protected:
    std::filesystem::path directory;

    void SetUp() override {
        directory =
            std::filesystem::temp_directory_path() /
            ("gpp-chart-contracts-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
    }

    void TearDown() override { std::filesystem::remove_all(directory); }

    std::string write(const std::string &song, const std::string &sync,
                      const std::string &instruments) {
        const auto path = directory / "notes.chart";
        std::ofstream output(path, std::ios::binary);
        output << "[Song]\n{\n"
               << song << "}\n[SyncTrack]\n{\n"
               << sync << "}\n"
               << instruments;
        output.close();
        EXPECT_TRUE(output);
        return path.string();
    }

    std::string guitar(const std::string &notes) {
        return "[ExpertSingle]\n{\n" + notes + "}\n";
    }
};

TEST_F(ChartRuntime, SongMetadataPreservesUtf8AndSpacesInsideQuotes) {
    const auto path = write("Name = \"  Canção ao vivo  \"\n"
                            "Artist = \"João e André\"\n"
                            "Charter = \"Autoria çã\"\n"
                            "Resolution = 192\nOffset = 0\n",
                            "0 = B 120000\n", guitar("0 = N 0 0\n"));
    CPlayer player("metadata");

    ASSERT_TRUE(player.Notes.loadFeedbackChart(path, player));
    EXPECT_EQ(player.songName, "  Canção ao vivo  ");
    EXPECT_EQ(player.songArtist, "João e André");
    EXPECT_EQ(player.songCharter, "Autoria çã");
}

TEST_F(ChartRuntime, OneQuarterNoteUsesResolutionAndBpm) {
    for (const int resolution : {192, 480, 960}) {
        for (const int bpm : {60, 120, 150}) {
            SCOPED_TRACE(::testing::Message()
                         << "resolution=" << resolution << " bpm=" << bpm);
            const auto path = write(
                "Resolution = " + std::to_string(resolution) + "\nOffset = 0\n",
                "0 = B " + std::to_string(bpm * 1000) + "\n",
                guitar(std::to_string(resolution) + " = N 0 " +
                       std::to_string(resolution) + "\n"));
            CPlayer player("quarter note");

            ASSERT_TRUE(player.Notes.loadFeedbackChart(path, player));
            ASSERT_EQ(player.Notes.gNotes.size(), 1u);
            EXPECT_NEAR(player.Notes.gNotes[0].time, 60.0 / bpm, 1e-12);
            EXPECT_NEAR(player.Notes.gNotes[0].lTime, 60.0 / bpm, 1e-12);
        }
    }
}

TEST_F(ChartRuntime, OffsetTranslatesNoteStartsWithoutChangingDurations) {
    for (const auto &offset : {"-0.25", "0", "0.75"}) {
        SCOPED_TRACE(offset);
        const auto path =
            write(std::string("Resolution = 192\nOffset = ") + offset + "\n",
                  "0 = B 120000\n", guitar("0 = N 0 192\n192 = N 1 192\n"));
        CPlayer player("offset");

        ASSERT_TRUE(player.Notes.loadFeedbackChart(path, player));
        ASSERT_EQ(player.Notes.gNotes.size(), 2u);
        EXPECT_DOUBLE_EQ(player.Notes.gNotes[0].time, std::stod(offset));
        EXPECT_DOUBLE_EQ(player.Notes.gNotes[1].time, 0.5 + std::stod(offset));
        EXPECT_DOUBLE_EQ(player.Notes.gNotes[0].lTime, 0.5);
        EXPECT_DOUBLE_EQ(player.Notes.gNotes[1].lTime, 0.5);
    }
}

TEST_F(ChartRuntime, SyncTrackIntegratesTempoSegmentsAtAndAfterChanges) {
    // Each section spans one quarter note: 0.5 s, 1 s, then 0.25 s.
    const auto path =
        write("Resolution = 192\nOffset = 0\n",
              "0 = B 120000\n192 = B 60000\n384 = B 240000\n",
              guitar("0 = N 0 0\n192 = N 1 0\n384 = N 2 0\n576 = N 3 0\n"));
    CPlayer player("tempo changes");

    ASSERT_TRUE(player.Notes.loadFeedbackChart(path, player));
    ASSERT_EQ(player.Notes.gNotes.size(), 4u);
    const double times[] = {0.0, 0.5, 1.5, 1.75};

    for (size_t i = 0; i < 4; ++i) {
        EXPECT_DOUBLE_EQ(player.Notes.gNotes[i].time, times[i]);
    }

    ASSERT_EQ(player.Notes.BPM.size(), 3u);
    const double bpms[] = {120.0, 60.0, 240.0};

    for (size_t i = 0; i < 3; ++i) {
        EXPECT_DOUBLE_EQ(player.Notes.BPM[i].time, times[i]);
        EXPECT_DOUBLE_EQ(player.Notes.BPM[i].lTime, bpms[i]);
    }
}

TEST_F(ChartRuntime, SustainIntegratesAcrossTwoTempoChanges) {
    // Tick 96 starts at 0.25 s; tick 672 ends at 1.875 s.
    const auto path = write("Resolution = 192\nOffset = 0\n",
                            "0 = B 120000\n192 = B 60000\n384 = B 240000\n",
                            guitar("96 = N 2 576\n"));
    CPlayer player("sustain");

    ASSERT_TRUE(player.Notes.loadFeedbackChart(path, player));
    ASSERT_EQ(player.Notes.gNotes.size(), 1u);
    const auto &note = player.Notes.gNotes.front();
    EXPECT_DOUBLE_EQ(note.time, 0.25);
    EXPECT_DOUBLE_EQ(note.lTime, 1.625);
    EXPECT_NE(note.type & nf_slide, 0);
    EXPECT_DOUBLE_EQ(player.Notes.lastNoteEnd, 1.875);
}

TEST_F(ChartRuntime, InstrumentSectionsAndPlayerBuffersStayIndependent) {
    const auto path = write("Resolution = 192\nOffset = 0\n", "0 = B 120000\n",
                            guitar("0 = N 0 0\n") +
                                "[MediumSingle]\n{\n192 = N 1 0\n}\n"
                                "[ExpertDoubleBass]\n{\n384 = N 4 0\n}\n");
    CPlayer guitarPlayer("guitar"), mediumPlayer("medium"), bassPlayer("bass");
    mediumPlayer.instrument = "[MediumSingle]";
    bassPlayer.instrument = "[ExpertDoubleBass]";

    ASSERT_TRUE(guitarPlayer.Notes.loadFeedbackChart(path, guitarPlayer));
    ASSERT_TRUE(mediumPlayer.Notes.loadFeedbackChart(path, mediumPlayer));
    ASSERT_TRUE(bassPlayer.Notes.loadFeedbackChart(path, bassPlayer));
    ASSERT_EQ(guitarPlayer.Notes.gNotes.size(), 1u);
    ASSERT_EQ(mediumPlayer.Notes.gNotes.size(), 1u);
    ASSERT_EQ(bassPlayer.Notes.gNotes.size(), 1u);
    EXPECT_DOUBLE_EQ(guitarPlayer.Notes.gNotes[0].time, 0.0);
    EXPECT_DOUBLE_EQ(mediumPlayer.Notes.gNotes[0].time, 0.5);
    EXPECT_DOUBLE_EQ(bassPlayer.Notes.gNotes[0].time, 1.0);
    EXPECT_EQ(guitarPlayer.Notes.gNotes[0].type & CChart::notesEnum, nf_green);
    EXPECT_EQ(mediumPlayer.Notes.gNotes[0].type & CChart::notesEnum, nf_red);
    EXPECT_EQ(bassPlayer.Notes.gNotes[0].type & CChart::notesEnum, nf_orange);

    guitarPlayer.Notes.gNotes[0].type |= nf_picked;
    guitarPlayer.Notes.unloadChart();
    EXPECT_EQ(mediumPlayer.Notes.gNotes[0].type & nf_picked, 0);
    EXPECT_EQ(bassPlayer.Notes.gNotes[0].type & nf_picked, 0);
}

TEST_F(ChartRuntime, PlusPhraseStoresTimingAndStrictlyInteriorNotes) {
    // All notes are strictly inside one phrase; boundary policy is left open.
    const auto path = write("Resolution = 192\nOffset = 0\n", "0 = B 120000\n",
                            guitar("96 = S 2 384\n192 = N 0 0\n384 = N 1 0\n"));
    CPlayer player("Plus");

    ASSERT_TRUE(player.Notes.loadFeedbackChart(path, player));
    ASSERT_EQ(player.Notes.gNotes.size(), 2u);
    ASSERT_EQ(player.Notes.gPlus.size(), 1u);
    const auto &phrase = player.Notes.gPlus.front();
    EXPECT_DOUBLE_EQ(phrase.time, 0.25);
    EXPECT_DOUBLE_EQ(phrase.lTime, 1.0);
    EXPECT_EQ(phrase.firstNote, 0u);
    EXPECT_EQ(phrase.lastNote, 1u);
}
} // namespace
