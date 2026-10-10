#include "CChart.h"
#include <gtest/gtest.h>
#include <bass.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>

namespace {
class SongLoading : public ::testing::Test {
  protected:
    std::filesystem::path directory;
    std::filesystem::path previousDirectory;

    void SetUp() override {
        previousDirectory = std::filesystem::current_path();
        directory = std::filesystem::temp_directory_path() /
            ("gpp-song-test-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory / "data/songs/test");
        std::filesystem::current_path(directory);
    }
    void TearDown() override {
        std::filesystem::current_path(previousDirectory);
        std::filesystem::remove_all(directory);
    }
    std::string writeChart(const std::string &contents,
                           const std::string &name = "notes.chart") {
        const auto path = directory / "data/songs/test" / name;
        std::ofstream output(path, std::ios::binary);
        output << contents;
        return path.string();
    }
    std::string chart(const std::string &header = "Resolution = 192\nOffset = 0\n",
                      const std::string &bpm = "0 = B 120000\n",
                      const std::string &notes = "0 = N 0 0\n192 = N 1 192\n") {
        return "[Song]\n{\n" + header + "}\n[SyncTrack]\n{\n" + bpm +
            "}\n[ExpertSingle]\n{\n" + notes + "}\n";
    }
    std::string compiled(CChart &source) {
        const auto path = (directory / "data/songs/test/notes.gpp").string();
        EXPECT_TRUE(source.compileGppChart(path));
        return path;
    }
    void fillCompiled(CChart &source) {
        CPlayer::NotesData::Note note;
        note.time = 1.0;
        note.type = nf_green;
        source.getInstrument("[ExpertSingle]").gNotes.push_back(note);
        note.time = 0;
        note.lTime = 120;
        source.getBPMContainer().push_back(note);
    }
};
}

TEST_F(SongLoading, HeaderRejectsMalformedTimingAndTruncation) {
    for (const auto &header : {"Resolution = 0\n", "Resolution = -192\n",
                              "Resolution = nan\n", "Resolution = 192oops\n",
                              "Offset = inf\n", "Offset = invalid\n"}) {
        const auto path = writeChart(chart(header));
        EXPECT_FALSE(CChart::validateChartHeader(path)) << header;
        CPlayer player("test");
        EXPECT_FALSE(player.Notes.loadFeedbackChart(path, player));
    }
    EXPECT_FALSE(CChart::validateChartHeader(writeChart("[Song]\n{\nName = song\n")));
    EXPECT_FALSE(CChart::validateChartHeader(writeChart("not a chart")));
    EXPECT_FALSE(CChart::validateChartHeader((directory / "missing.chart").string()));
}

TEST_F(SongLoading, MissingOptionalMetadataAndBPMUseExistingDefaults) {
    const auto path = writeChart(chart("", ""));
    EXPECT_TRUE(CChart::validateChartHeader(path));
    CPlayer player("test");
    ASSERT_TRUE(player.Notes.loadFeedbackChart(path, player));
    ASSERT_EQ(player.Notes.BPM.size(), 1u);
    EXPECT_DOUBLE_EQ(player.Notes.BPM[0].lTime, 120.0);
    ASSERT_EQ(player.Notes.gNotes.size(), 2u);
    EXPECT_DOUBLE_EQ(player.Notes.gNotes[1].time, 0.5);
    EXPECT_DOUBLE_EQ(player.Notes.gNotes[1].lTime, 0.5);
}

TEST_F(SongLoading, FailedReloadClearsPreviousNotesAndTimingCache) {
    CPlayer player("test");
    ASSERT_TRUE(player.Notes.loadFeedbackChart(writeChart(chart()), player));
    player.Notes.getChartEnd();
    EXPECT_FALSE(player.Notes.loadFeedbackChart((directory / "missing.chart").string(), player));
    EXPECT_TRUE(player.Notes.gNotes.empty());
    EXPECT_TRUE(player.Notes.BPM.empty());
    EXPECT_EQ(player.Notes.chartEnd, 0.0);
}

TEST_F(SongLoading, EmptyInstrumentAndBadBPMOrNoteDoNotStart) {
    CPlayer player("test");
    player.instrument = "[ExpertDoubleBass]";
    EXPECT_FALSE(player.Notes.loadFeedbackChart(writeChart(chart()), player));
    player.instrument = "[ExpertSingle]";
    EXPECT_FALSE(player.Notes.loadFeedbackChart(writeChart(chart("", "0 = B 0\n")), player));
    EXPECT_FALSE(player.Notes.loadFeedbackChart(writeChart(chart("", "", "bad = N 0 0\n")), player));
    EXPECT_FALSE(player.Notes.loadFeedbackChart(writeChart(chart("", "", "")), player));
    EXPECT_TRUE(player.Notes.gNotes.empty());
    EXPECT_TRUE(player.Notes.BPM.empty());
}

TEST_F(SongLoading, CompiledChartPreservesInstrumentFailureAndRejectsBadTiming) {
    CChart source;
    fillCompiled(source);
    const auto path = compiled(source);
    EXPECT_TRUE(CChart::validateChartHeader(path));
    CPlayer player("test");
    ASSERT_TRUE(player.Notes.loadChart(path, player));
    player.instrument = "[ExpertDoubleBass]";
    EXPECT_FALSE(player.Notes.loadChart(path, player));
    EXPECT_TRUE(player.Notes.gNotes.empty());
    player.instrument = "[ExpertSingle]";
    source.getBPMContainer().clear();
    EXPECT_FALSE(player.Notes.loadChart(compiled(source), player));
    EXPECT_FALSE(CChart::validateChartHeader(path));
    source.getBPMContainer().push_back(CPlayer::NotesData::Note());
    source.getBPMContainer()[0].lTime = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(player.Notes.loadChart(compiled(source), player));
}

TEST_F(SongLoading, CorruptBinaryDoesNotFallThroughToTextParser) {
    CPlayer player("test");
    // Arbitrary text is neither a binary archive nor a legacy N-record chart.
    const auto path = writeChart("0 = 0 0\n0 = 6 120\n", "notes.gpp");
    EXPECT_FALSE(CChart::validateChartHeader(path));
    EXPECT_FALSE(player.Notes.loadChart(path, player));
    EXPECT_TRUE(player.Notes.gNotes.empty());
}

TEST_F(SongLoading, SongLoadingRequiresAudioButChartOnlyDoesNot) {
    writeChart(chart(), "NOTES.CHART");
    CPlayer player("test");
    ASSERT_TRUE(player.loadSongOnlyChart("test"));
    EXPECT_FALSE(player.loadSong("test"));
    // Valid chart data remains available for the editor even without audio.
    EXPECT_EQ(player.Notes.gNotes.size(), 2u);
    EXPECT_TRUE(player.songFullPath.empty());
    EXPECT_FALSE(player.loadSongOnlyChart("missing"));
    EXPECT_TRUE(player.Notes.BPM.empty());
}

TEST_F(SongLoading, TruncatedBodyDoesNotLoadPartialSong) {
    CPlayer player("test");
    auto text = chart();
    text.erase(text.find_last_of('}'));
    const auto path = writeChart(text);
    // Discovery only checks the header. Loading must still validate the body.
    EXPECT_TRUE(CChart::validateChartHeader(path));
    EXPECT_FALSE(player.Notes.loadFeedbackChart(path, player));
    EXPECT_TRUE(player.Notes.gNotes.empty());
}

TEST_F(SongLoading, RepeatedLoadReplacesNotesInsteadOfAppending) {
    CPlayer player("test");
    const auto path = writeChart(chart());
    ASSERT_TRUE(player.Notes.loadFeedbackChart(path, player));
    ASSERT_TRUE(player.Notes.loadFeedbackChart(path, player));
    EXPECT_EQ(player.Notes.gNotes.size(), 2u);
    EXPECT_EQ(player.Notes.BPM.size(), 1u);
}

TEST_F(SongLoading, LegacyTextGppStillLoadsAfterHeaderValidation) {
    const auto path = writeChart("N 6 0 120\nN 0 1 0\nN 1 2 0.5\n", "notes.gpp");
    EXPECT_TRUE(CChart::validateChartHeader(path));
    CPlayer player("test");
    ASSERT_TRUE(player.Notes.loadChart(path, player));
    EXPECT_EQ(player.Notes.gNotes.size(), 2u);
    ASSERT_EQ(player.Notes.BPM.size(), 1u);
    EXPECT_DOUBLE_EQ(player.Notes.BPM[0].lTime, 120.0);
}

TEST_F(SongLoading, ValidAudioLoadsWithoutOptionalStemAndReleasesOnReloadFailure) {
    // BASS's no-sound device needs no hardware or audio server.
    struct SilentAudio {
        bool initialized = BASS_Init(0, 44100, 0, nullptr, nullptr) != 0;
        ~SilentAudio() { if (initialized) { BASS_Free(); } }
    } audio;
    ASSERT_TRUE(audio.initialized);
    writeChart(chart());
    // BASS detects the format from contents. Use a tiny generated PCM WAVE
    // under the song filename recognized by discovery; no licensed fixture.
    const unsigned char wave[] = {
        'R','I','F','F', 40,0,0,0, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
        0x44,0xac,0,0, 0x88,0x58,1,0, 2,0, 16,0,
        'd','a','t','a', 4,0,0,0, 0,0,0,0
    };
    {
        std::ofstream output(directory / "data/songs/test/song.ogg", std::ios::binary);
        output.write(reinterpret_cast<const char *>(wave), sizeof(wave));
    }
    CPlayer player("test");
    ASSERT_TRUE(player.loadSong("test"));
    EXPECT_NE(player.songAudioID, 0);
    EXPECT_NE(player.songAudioID, -1);
    EXPECT_EQ(player.instrumentSound, 0);
    const auto previousHandle = player.songAudioID;
    EXPECT_FALSE(player.loadSong("missing"));
    BASS_CHANNELINFO info{};
    EXPECT_FALSE(BASS_ChannelGetInfo(previousHandle, &info));
    EXPECT_TRUE(player.Notes.gNotes.empty());
}
