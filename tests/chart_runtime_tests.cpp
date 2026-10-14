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

TEST_F(ChartRuntime, InspectionEvidenceDoesNotChangeCompiledPlusData) {
    const auto path =
        write("Name = \"Synthetic\"\nResolution = 192\nOffset = 0\n",
              "0 = B 120000\n0 = TS 4\n384 = TS 3 2\n",
              guitar("0 = S 2 384\n96 = N 0 0\n288 = N 1 0\n"));
    CChart::NotesData feedback;
    CChart::Metadata metadata;
    ASSERT_TRUE(
        feedback.loadFeedbackChartCPU(path, "[ExpertSingle]", metadata));
    EXPECT_TRUE(feedback.source.events.empty());
    const auto defaultNotes = feedback.gNotes;
    ASSERT_TRUE(feedback.loadFeedbackChartCPU(path, "[ExpertSingle]", metadata,
                                              nullptr, true));
    ASSERT_EQ(feedback.gNotes.size(), defaultNotes.size());
    for (size_t i = 0; i < defaultNotes.size(); ++i) {
        EXPECT_DOUBLE_EQ(feedback.gNotes[i].time, defaultNotes[i].time);
        EXPECT_DOUBLE_EQ(feedback.gNotes[i].lTime, defaultNotes[i].lTime);
        EXPECT_EQ(feedback.gNotes[i].type, defaultNotes[i].type);
    }
    ASSERT_EQ(feedback.gPlus.size(), 1u);
    ASSERT_EQ(feedback.source.events.size(), 3u);

    CChart compiler;
    ASSERT_TRUE(compiler.open(path));
    const auto binary = (directory / "notes.gpp").string();
    ASSERT_TRUE(compiler.compileGppChart(binary));
    CChart::NotesData compiled;
    ASSERT_TRUE(compiled.loadChartCPU(binary, "[ExpertSingle]", metadata));
    ASSERT_EQ(compiled.gPlus.size(), feedback.gPlus.size());
    EXPECT_DOUBLE_EQ(compiled.gPlus[0].time, feedback.gPlus[0].time);
    EXPECT_DOUBLE_EQ(compiled.gPlus[0].lTime, feedback.gPlus[0].lTime);
    EXPECT_EQ(compiled.gPlus[0].firstNote, feedback.gPlus[0].firstNote);
    EXPECT_EQ(compiled.gPlus[0].lastNote, feedback.gPlus[0].lastNote);
    EXPECT_TRUE(compiled.source.events.empty());
    EXPECT_EQ(compiled.source.ppq, 0);
}

TEST_F(ChartRuntime, MalformedDiagnosticMeterDoesNotRejectPlayableChart) {
    const auto path = write("Resolution = 192\nOffset = 0\n",
                            "0 = B 120000\ninvalid = TS 4\n192 = TS 3 bad\n",
                            guitar("0 = N 0 0\n192 = N 1 0\n"));
    CChart::NotesData notes;
    CChart::Metadata metadata;
    ASSERT_TRUE(notes.loadFeedbackChartCPU(path, "[ExpertSingle]", metadata));
    EXPECT_TRUE(notes.source.warnings.empty());
    ASSERT_TRUE(notes.loadFeedbackChartCPU(path, "[ExpertSingle]", metadata,
                                           nullptr, true));
    ASSERT_EQ(notes.gNotes.size(), 2u);
    EXPECT_DOUBLE_EQ(notes.gNotes[1].time, 0.5);
    EXPECT_TRUE(notes.source.events.empty());
    EXPECT_EQ(notes.source.warnings.size(), 2u);
}
