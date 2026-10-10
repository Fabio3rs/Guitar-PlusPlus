#include "CChart.h"
#include "ActiveDelta.h"
#include "GPPGame.h"
#include <type_traits>
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

TEST_F(SongLoading, NotesMoveTransfersBuffersAndPreservesCopySupport) {
    static_assert(std::is_nothrow_move_constructible_v<CPlayer::NotesData>);
    static_assert(std::is_nothrow_move_assignable_v<CPlayer::NotesData>);
    CPlayer::NotesData source;
    source.gNotes.resize(5); source.BPM.resize(2); source.gPlus.resize(1);
    source.notePos = 2;
    const auto *notes = source.gNotes.data();
    const auto *bpm = source.BPM.data();
    const auto *plus = source.gPlus.data();
    CPlayer::NotesData moved(std::move(source));
    CPlayer::NotesData destination;
    destination = std::move(moved);
    source.unloadChart(); moved.unloadChart();
    EXPECT_EQ(destination.gNotes.data(), notes);
    EXPECT_EQ(destination.BPM.data(), bpm);
    EXPECT_EQ(destination.gPlus.data(), plus);
    EXPECT_EQ(destination.notePos, 2u);
    auto copy = destination;
    EXPECT_NE(copy.gNotes.data(), notes);
    EXPECT_EQ(copy.gNotes.size(), 5u);
}
TEST_F(SongLoading, UpdateDeltaExcludesIdleTimeAndHasNoCatchup) {
    ActiveDelta delta;
    EXPECT_DOUBLE_EQ(delta.sample(100.0), 0.0);
    EXPECT_NEAR(delta.sample(100.002), 0.002, 1e-12);
    delta.reset();
    EXPECT_DOUBLE_EQ(delta.sample(150.0), 0.0);
    EXPECT_NEAR(delta.sample(150.005), 0.005, 1e-12);
    EXPECT_DOUBLE_EQ(delta.sample(149.0), 0.0);
    EXPECT_DOUBLE_EQ(delta.sample(std::numeric_limits<double>::quiet_NaN()), 0.0);
    EXPECT_DOUBLE_EQ(delta.sample(160.0), 0.0);
}
TEST_F(SongLoading, SustainsAndProvisionalXPAgreeAcrossCadences) {
    for (const bool plus : {false, true}) {
        for (const double step : {0.001, 0.002, 0.005, 0.033}) {
            CPlayer player("sustain");
            player.plusEnabled = plus;
            double previous = 0.0;
            for (double now = step; previous < 2.0; now += step) {
                player.addPointsByDoingLongNote(sustainActiveSeconds(0.1, 1.13, now, now - previous));
                previous = now;
            }
            EXPECT_NEAR(player.getPoints(), plus ? 100 : 50, 1);
            EXPECT_NEAR(player.getExperience(), plus ? 101.0 : 100.5, 1e-9);
            const auto points = player.getPoints();
            player.addPointsByDoingLongNote(-1.0);
            player.addPointsByDoingLongNote(std::numeric_limits<double>::infinity());
            EXPECT_EQ(player.getPoints(), points);
        }
    }
}
TEST_F(SongLoading, SongResultIsIndependentOfResetAndMarathonTotals) {
    CPlayer player("results");
    player.Notes.gNotes.resize(3);
    player.beginSongResult();
    player.doNote(0); player.doNote(1);
    player.breakCombo();
    auto first = player.songResult();
    EXPECT_EQ(first.score, 100);
    EXPECT_EQ(first.maxCombo, 2);
    EXPECT_EQ(first.correctNotes, 2u);
    EXPECT_EQ(first.totalNotes, 3u);
    player.beginSongResult();
    player.doNote(2);
    auto second = player.songResult();
    EXPECT_EQ(second.score, 50);
    EXPECT_EQ(second.correctNotes, 1u);
    player.resetSongThings();
    EXPECT_EQ(first.score, 100);
    EXPECT_EQ(first.totalNotes, 3u);
}
TEST_F(SongLoading, PreparedSongTransferPreservesAudioOwnership) {
    struct SilentAudio {
        bool initialized = BASS_Init(0, 44100, 0, nullptr, nullptr) != 0;
        ~SilentAudio() { if (initialized) { BASS_Free(); } }
    } audio;
    ASSERT_TRUE(audio.initialized);
    CPlayer target("target");
    const auto oldHandle = BASS_StreamCreate(44100, 2, 0, STREAMPROC_PUSH, nullptr);
    ASSERT_NE(oldHandle, 0u);
    target.songAudioID = oldHandle;
    unsigned int transferred = 0;
    {
        CPlayer prepared("prepared");
        prepared.Notes.gNotes.resize(2);
        const auto *notes = prepared.Notes.gNotes.data();
        transferred = BASS_StreamCreate(44100, 2, 0, STREAMPROC_PUSH, nullptr);
        ASSERT_NE(transferred, 0u);
        prepared.songAudioID = transferred;
        prepared.songName = "second";
        target.takeLoadedSong(prepared);
        EXPECT_EQ(prepared.songAudioID, -1);
        EXPECT_EQ(target.Notes.gNotes.data(), notes);
        EXPECT_EQ(target.songName, "second");
    }
    BASS_CHANNELINFO info{};
    EXPECT_FALSE(BASS_ChannelGetInfo(oldHandle, &info));
    EXPECT_TRUE(BASS_ChannelGetInfo(transferred, &info));
    target.releaseSong();
    EXPECT_FALSE(BASS_ChannelGetInfo(transferred, &info));
}


// Exercise the actual publication/install protocol without exposing a runtime
// loading API or requiring a window, GPU, licensed music or input simulation.
struct GPPGameLoadingTestAccess {
    static void exercise() {
        GPPGame::loadThreadData load;
        load.instruments = {"[ExpertSingle]", "[ExpertSingle]"};
        load.songsList = {"test", "test", "missing"};
        CGamePlay module;
        module.players.push_back(std::make_shared<CPlayer>("one"));
        module.players.push_back(std::make_shared<CPlayer>("two"));
        load.continueThread = true;
        load.processing = true;
        load.loadSong = true;
        std::thread worker(GPPGame::loadMarathonThread, std::ref(load));
        struct Join {
            GPPGame::loadThreadData &state;
            std::thread &thread;
            ~Join() { state.continueThread = false; thread.join(); }
        } join{load, worker};
        auto wait = [&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (load.processing && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return !load.processing;
        };
        ASSERT_TRUE(wait());
        ASSERT_FALSE(load.loadFailed);
        ASSERT_EQ(load.prepared.size(), 2u);
        EXPECT_TRUE(module.getPlayer(0).Notes.gNotes.empty());
        EXPECT_TRUE(module.getPlayer(1).Notes.gNotes.empty());
        ASSERT_TRUE(GPPGame::installMarathonSong(module, load));
        const auto oldStream = module.getPlayer(0).songAudioID;
        const auto *oldNotes = module.getPlayer(0).Notes.gNotes.data();
        load.songID = 1;
        load.processing = true;
        load.loadSong = true;
        ASSERT_TRUE(wait());
        ASSERT_FALSE(load.loadFailed);
        EXPECT_EQ(module.getPlayer(0).Notes.gNotes.data(), oldNotes);
        BASS_CHANNELINFO info{};
        EXPECT_TRUE(BASS_ChannelGetInfo(oldStream, &info));
        ASSERT_EQ(load.prepared.size(), 2u);
        const auto *nextNotes = load.prepared[0]->Notes.gNotes.data();
        ASSERT_TRUE(GPPGame::installMarathonSong(module, load));
        EXPECT_EQ(module.getPlayer(0).Notes.gNotes.data(), nextNotes);
        EXPECT_FALSE(BASS_ChannelGetInfo(oldStream, &info));
        load.songID = 2;
        load.processing = true;
        load.loadSong = true;
        ASSERT_TRUE(wait());
        EXPECT_TRUE(load.loadFailed);
        EXPECT_FALSE(GPPGame::installMarathonSong(module, load));
        EXPECT_EQ(module.getPlayer(0).Notes.gNotes.data(), nextNotes);
    }
    static void emptyList() {
        GPPGame::loadThreadData load;
        load.continueThread = true;
        load.processing = true;
        load.loadSong = true;
        std::thread worker(GPPGame::loadMarathonThread, std::ref(load));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (load.processing && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        load.continueThread = false;
        worker.join();
        EXPECT_FALSE(load.processing);
        EXPECT_TRUE(load.listEnd);
        EXPECT_FALSE(load.loadFailed);
    }
};
TEST_F(SongLoading, MarathonPublishesWithoutChangingLivePlayers) {
    struct SilentAudio {
        bool initialized = BASS_Init(0, 44100, 0, nullptr, nullptr) != 0;
        ~SilentAudio() { if (initialized) { BASS_Free(); } }
    } audio;
    ASSERT_TRUE(audio.initialized);
    writeChart(chart());
    const unsigned char wave[] = {
        'R','I','F','F',40,0,0,0,'W','A','V','E',
        'f','m','t',' ',16,0,0,0,1,0,1,0,
        0x44,0xac,0,0,0x88,0x58,1,0,2,0,16,0,
        'd','a','t','a',4,0,0,0,0,0,0,0
    };
    std::ofstream output(directory / "data/songs/test/song.ogg", std::ios::binary);
    output.write(reinterpret_cast<const char *>(wave), sizeof(wave));
    output.close();
    GPPGameLoadingTestAccess::exercise();
    GPPGameLoadingTestAccess::emptyList();
}

TEST_F(SongLoading, CompletionWaitsForEarlierSustainAndSurvivesTransfer) {
    // 120 BPM / 192 ticks: the first note ends at 10 s, the last starts at 0.5 s.
    writeChart(chart("Resolution = 192\nOffset = 0\n", "0 = B 120000\n",
                     "0 = N 0 3840\n192 = N 1 0\n"));
    CPlayer loaded("loaded");
    ASSERT_TRUE(loaded.loadSongOnlyChart("test"));
    EXPECT_DOUBLE_EQ(loaded.Notes.lastNoteEnd, 10.0);
    loaded.musicRunningTime = 4.1;
    EXPECT_FALSE(loaded.isSongChartFinished());
    loaded.musicRunningTime = 13.0;
    EXPECT_FALSE(loaded.isSongChartFinished());
    loaded.musicRunningTime = 13.001;
    EXPECT_TRUE(loaded.isSongChartFinished());
    CPlayer target("target");
    target.takeLoadedSong(loaded);
    target.musicRunningTime = 4.1;
    EXPECT_FALSE(target.isSongChartFinished());
    EXPECT_DOUBLE_EQ(target.Notes.lastNoteEnd, 10.0);
    target.Notes.unloadChart();
    EXPECT_DOUBLE_EQ(target.Notes.lastNoteEnd, 0.0);
    EXPECT_TRUE(target.isSongChartFinished());
}
TEST_F(SongLoading, UpdateReportsCompletionForAllPlayers) {
    writeChart(chart());
    CGamePlay module;
    for (const auto *name : {"one", "two"}) {
        auto player = std::make_shared<CPlayer>(name);
        ASSERT_TRUE(player->loadSongOnlyChart("test"));
        player->bUpdateP = false; // No GLFW input required by this test.
        player->musicRunningTime = player->Notes.lastNoteEnd + 4.0;
        module.players.push_back(std::move(player));
    }
    EXPECT_TRUE(module.update());
    module.players.back()->musicRunningTime = 0.0;
    EXPECT_FALSE(module.update());
}
TEST_F(SongLoading, PauseCompensationKeepsTimelineAndCountdownAligned) {
    CGamePlay module;
    module.startSongTime = std::chrono::steady_clock::now();
    const auto originalTimeline = module.startSongTime;
    for (const auto *name : {"one", "two"}) {
        auto player = std::make_shared<CPlayer>(name);
        player->startTime = 100.0;
        player->musicRunningTime = -2.0;
        module.players.push_back(std::move(player));
    }
    // Several pauses, including a long delay inside the menu/rendering.
    for (const auto milliseconds : {100, 2300, 15}) {
        module.compensatePause(std::chrono::milliseconds(milliseconds));
    }
    EXPECT_EQ(module.startSongTime - originalTimeline, std::chrono::milliseconds(2415));
    for (const auto &player : module.players) {
        EXPECT_NEAR(player->startTime, 102.415, 1e-10);
        EXPECT_DOUBLE_EQ(player->musicRunningTime, -2.0);
    }
}
