#include "CMenu.h"
#include "MidiFixture.h"
#include "SongFiles.h"
#include "SongLibrary.h"
#include "sqlite3/sqlite3raii.hpp"
#include "ui/CMenuSystem.h"
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <future>
#include <gtest/gtest.h>
#include <type_traits>

namespace {
namespace fs = std::filesystem;
using songs::ScanMode;
using songs::SongLibrary;

class SongLibraryTest : public ::testing::Test {
  protected:
    fs::path directory, root, db;
    void SetUp() override {
        directory =
            fs::temp_directory_path() /
            ("gpp-library-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        root = directory / "songs";
        db = directory / "cache/library.db";
        fs::create_directories(root);
    }
    void TearDown() override { fs::remove_all(directory); }
    static void write(const fs::path &file, const std::string &text) {
        fs::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary);
        out << text;
    }
    static std::string chart(const std::string &name = "Canção A") {
        return "[Song]\n{\n Name = \"" + name +
               "\"\n Artist = \"Artista\"\n"
               " Charter = \"Autor\"\n Genre = \"Metal\"\n PreviewStart = "
               "12.5\n"
               " Album = \"Ao vivo\"\n Year = 2004\n ExtraField = "
               "\"preservado\"\n"
               " Resolution = 192\n}\n[SyncTrack]\n{\n0 = B 120000\n192 = B "
               "180000\n}\n"
               "[ExpertSingle]\n{\n0 = N 0 0\n192 = N 1 192\n}\n"
               "[EasySingle]\n{\n0 = N 0 0\n}\n";
    }
    fs::path song(const std::string &name = "pack/música",
                  const std::string &title = "Canção A") {
        auto path = root / fs::u8path(name);
        write(path / "notes.chart", chart(title));
        write(path / "song.ogg", "fixture audio");
        return path;
    }
    static songs::AudioInfo audio(const fs::path &) { return {123.25, {}}; }
    static auto scan(SongLibrary &library,
                     ScanMode mode = ScanMode::Incremental) {
        library.refresh(mode);
        library.wait();
        return library.snapshot();
    }
};
} // namespace

TEST(SQLiteRaii, Cxx17OwnershipParametersAndTransactions) {
    static_assert(!std::is_copy_constructible<SqliteTxn>::value);
    static_assert(!std::is_move_constructible<SqliteTxn>::value);
    SqliteDb db(":memory:");
    db.exec("CREATE TABLE entries(value TEXT)");
    {
        SqliteTxn transaction(db);
        auto insert = db.prepare("INSERT INTO entries VALUES(?)", "insert");
        EXPECT_EQ(insert.bind(1, std::string_view{}).step(), SQLITE_DONE);
        transaction.commit();
    }
    {
        SqliteTxn transaction(db);
        auto insert = db.prepare("INSERT INTO entries VALUES(?)", "insert");
        EXPECT_EQ(insert.bind(1, std::optional<std::string_view>{}).step(),
                  SQLITE_DONE);
    }
    auto query =
        db.prepare("SELECT value, typeof(value) FROM entries", "select");
    ASSERT_EQ(query.step(), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_bytes(query.get(), 0), 0);
    EXPECT_EQ(sqlite3_column_type(query.get(), 0), SQLITE_TEXT);
    EXPECT_EQ(query.step(), SQLITE_DONE);
    try {
        db.exec("INVALID SQL");
        FAIL();
    } catch (const SqliteError &error) {
        EXPECT_EQ(error.code & 255, SQLITE_ERROR);
    }
}

TEST_F(SongLibraryTest, InvalidDatabaseOpenReportsCode) {
    try {
        SqliteDb database((directory / "missing/file.db").u8string());
        FAIL();
    } catch (const SqliteError &error) {
        EXPECT_EQ(error.code & 255, SQLITE_CANTOPEN);
    }
}

TEST_F(SongLibraryTest, MetadataRoundTripAndUnchangedRefreshDoNoContentWork) {
    const auto path = song();
    write(path / "song.ini",
          "[song]\nname = INI title\nartist = INI artist\nalbum = INI album\n"
          "charter = INI author\nsong_length = 500000\ndiff_drums = "
          "5\nloading_phrase = hello\n");
    write(path / "drums.ogg", "stem");
    write(path / "album.png", "cover");
    {
        SongLibrary library(root, db, {}, audio);
        const auto first = scan(library);
        ASSERT_TRUE(first->progress.complete) << first->progress.warning;
        ASSERT_EQ(first->songs.size(), 1u);
        const auto &record = first->songs.front();
        EXPECT_EQ(record.title, "Canção A");
        EXPECT_EQ(record.artist, "Artista");
        EXPECT_EQ(record.charter, "Autor");
        EXPECT_EQ(record.album, "Ao vivo");
        EXPECT_EQ(record.genre, "Metal");
        EXPECT_EQ(record.year, "2004");
        EXPECT_EQ(record.iniFields.at("loading_phrase"), "hello");
        EXPECT_EQ(record.chartFields.at("ExtraField"), "preservado");
        EXPECT_EQ(record.sources.at("name"), "chart");
        EXPECT_EQ(record.duration, 123.25);
        EXPECT_EQ(record.durationSource, "audio");
        EXPECT_EQ(record.previewStart, 12.5);
        EXPECT_EQ(record.bpmMin, 120);
        EXPECT_EQ(record.bpmMax, 180);
        EXPECT_EQ(record.instruments.size(), 2u);
        EXPECT_EQ(record.stems, std::vector<std::string>{"drums"});
        EXPECT_TRUE(record.available);
        EXPECT_EQ(first->progress.chartParses, 1u);
        EXPECT_EQ(first->progress.audioProbes, 1u);
        EXPECT_EQ(first->progress.hashes, 2u);
        const auto next = scan(library);
        EXPECT_EQ(next->progress.hashes, 0u);
        EXPECT_EQ(next->progress.chartParses, 0u);
        EXPECT_EQ(next->progress.audioProbes, 0u);
        EXPECT_EQ(next->progress.reused, 1u);
        EXPECT_EQ(first->progress.hashes, 2u); // old snapshots are immutable
    }
    SongLibrary reopened(root, db, {}, audio);
    const auto next = scan(reopened);
    ASSERT_EQ(next->songs.size(), 1u);
    EXPECT_EQ(next->songs.front().chartFields.at("ExtraField"), "preservado");
    EXPECT_EQ(next->progress.hashes, 0u);
    EXPECT_EQ(next->progress.chartParses, 0u);
    EXPECT_EQ(next->progress.audioProbes, 0u);
}

TEST_F(SongLibraryTest, HashAvoidsParsingWhenOnlyTimestampChanges) {
    const auto path = song() / "notes.chart";
    SongLibrary library(root, db, {}, audio);
    scan(library);
    fs::last_write_time(path,
                        fs::last_write_time(path) + std::chrono::seconds(3));
    auto next = scan(library);
    EXPECT_EQ(next->progress.hashes, 1u);
    EXPECT_EQ(next->progress.chartParses, 0u);
    EXPECT_EQ(next->progress.audioProbes, 0u);
    next = scan(library);
    EXPECT_EQ(next->progress.hashes, 0u);
}

TEST_F(SongLibraryTest, FullScanDetectsSameSizeAndTimestampAndRechecksAudio) {
    const auto path = song() / "notes.chart";
    SongLibrary library(root, db, {}, audio);
    const auto original = scan(library);
    const auto timestamp = fs::last_write_time(path);
    const auto size = fs::file_size(path);
    write(path, chart("Canção B"));
    ASSERT_EQ(size, fs::file_size(path));
    fs::last_write_time(path, timestamp);
    auto next = scan(library);
    EXPECT_EQ(next->songs.front().title, "Canção A");
    next = scan(library, ScanMode::Full);
    EXPECT_EQ(next->songs.front().title, "Canção B");
    EXPECT_EQ(next->progress.chartParses, 1u);
    EXPECT_EQ(next->progress.audioProbes, 1u);
    EXPECT_EQ(original->songs.front().title, "Canção A");
}

TEST_F(SongLibraryTest, IniAndAudioAndArtworkInvalidateIndependently) {
    const auto path = song();
    SongLibrary library(root, db, {}, audio);
    auto initial = scan(library);
    ASSERT_TRUE(initial->progress.complete) << initial->progress.warning;
    ASSERT_EQ(initial->songs.size(), 1u) << initial->progress.warning;
    write(path / "song.ini", "[song]\nicon = rb4\ndiff_band = 4\n");
    auto next = scan(library);
    ASSERT_TRUE(next->progress.complete) << next->progress.warning;
    ASSERT_EQ(next->songs.size(), 1u) << next->progress.warning;
    EXPECT_EQ(next->songs.front().origin, "rb4");
    EXPECT_EQ(next->progress.chartParses, 1u);
    EXPECT_EQ(next->progress.audioProbes, 0u);
    write(path / "song.ogg", "different audio bytes");
    next = scan(library);
    ASSERT_TRUE(next->progress.complete) << next->progress.warning;
    ASSERT_EQ(next->songs.size(), 1u) << next->progress.warning;
    EXPECT_EQ(next->progress.chartParses, 0u);
    EXPECT_EQ(next->progress.audioProbes, 1u);
    EXPECT_EQ(next->progress.hashes, 0u);
    write(path / "album.jpg", "cover");
    next = scan(library);
    ASSERT_TRUE(next->progress.complete) << next->progress.warning;
    ASSERT_EQ(next->songs.size(), 1u) << next->progress.warning;
    EXPECT_FALSE(next->songs.front().coverPath.empty());
    EXPECT_EQ(next->progress.chartParses, 0u);
    EXPECT_EQ(next->progress.audioProbes, 0u);
}

TEST_F(SongLibraryTest, RemovedAndRenamedSongsAreReconciledOnlyOnCompleteScan) {
    const auto path = song();
    SongLibrary library(root, db, {}, audio);
    scan(library);
    fs::rename(path, path.parent_path() / "renamed");
    auto next = scan(library);
    ASSERT_EQ(next->songs.size(), 1u);
    EXPECT_EQ(next->songs.front().path, "pack/renamed");
    EXPECT_EQ(next->progress.added, 1u);
    EXPECT_EQ(next->progress.removed, 1u);
    fs::rename(root, directory / "offline");
    next = scan(library);
    EXPECT_FALSE(next->progress.complete);
    EXPECT_EQ(next->songs.size(), 1u);
    fs::create_directories(root);
    next = scan(library);
    EXPECT_TRUE(next->songs.empty());
    EXPECT_EQ(next->progress.removed, 1u);
}

TEST_F(SongLibraryTest, InvalidPreferredChartFallsBackAndFailuresAreCached) {
    const auto path = song();
    write(path / fs::u8path("música.gpp"), "broken compiled file");
    SongLibrary library(root, db, {}, audio);
    auto next = scan(library);
    ASSERT_EQ(next->songs.size(), 1u);
    EXPECT_EQ(next->songs.front().format, ".chart");
    EXPECT_EQ(next->progress.chartParses, 2u);
    next = scan(library);
    EXPECT_EQ(next->progress.chartParses, 0u);
    fs::remove(path / "notes.chart");
    next = scan(library);
    EXPECT_FALSE(next->songs.front().available);
    EXPECT_EQ(next->progress.chartParses, 0u);
    write(path / fs::u8path("música.gpp"), "N 6 0 120\nN 0 1 0\n");
    next = scan(library);
    EXPECT_TRUE(next->songs.front().available);
    EXPECT_EQ(next->songs.front().format, ".gpp");
}

TEST_F(SongLibraryTest, MidiTitleFallsBackAfterIniAndRetainsIdentification) {
    const auto path = song();
    fs::remove(path / "notes.chart");
    midi_fixture::Track tempo("Internal Title"), guitar;
    tempo.meta(0, 2, "Copyright Example");
    tempo.meta(240, 1, "charter = Internal Author");
    guitar.note(0, 480, 96);
    write(path / "NOTES.MID", midi_fixture::smf({tempo, guitar}));
    write(
        path / "song.ini",
        "[song]\nname = Readable Title\nartist = Band\ncharter = INI Author\n");
    SongLibrary library(root, db, {}, audio);
    auto next = scan(library);
    ASSERT_EQ(next->songs.size(), 1u);
    EXPECT_EQ(next->songs.front().title, "Readable Title");
    EXPECT_EQ(next->songs.front().artist, "Band");
    EXPECT_EQ(next->songs.front().charter, "Internal Author");
    ASSERT_FALSE(next->songs.front().trackNames.empty());
    EXPECT_EQ(next->songs.front().trackNames.front(), "Internal Title");
    ASSERT_FALSE(next->songs.front().identification.empty());
    EXPECT_EQ(next->songs.front().identification.front(), "Copyright Example");
    fs::remove(path / "song.ini");
    next = scan(library);
    EXPECT_EQ(next->songs.front().title, "Internal Title");
    midi_fixture::Track generic("notes");
    write(path / "NOTES.MID", midi_fixture::smf({generic, guitar}));
    next = scan(library);
    EXPECT_EQ(next->songs.front().title, "música");
}

TEST_F(SongLibraryTest, DurationFallbackAndStemsDoNotInventPlayableParts) {
    const auto path = song();
    write(path / "song.ini", "[song]\nsong_length = 45234\ndiff_drums = 5\n");
    write(path / "drums.ogg", "drums");
    SongLibrary library(root, db, {}, [](const fs::path &) {
        return songs::AudioInfo{{}, "Invalid audio"};
    });
    auto next = scan(library);
    EXPECT_EQ(next->songs.front().duration, 45.234);
    EXPECT_EQ(next->songs.front().durationSource, "metadata");
    EXPECT_FALSE(next->songs.front().available);
    EXPECT_EQ(next->songs.front().instruments.size(), 2u);
    fs::remove(path / "song.ini");
    next = scan(library);
    EXPECT_EQ(next->songs.front().durationSource, "chart-estimate");
    EXPECT_GT(*next->songs.front().duration, 0);
    EXPECT_EQ(next->progress.audioProbes, 0u); // failed probes also cached
}

TEST_F(SongLibraryTest, CorruptAndIncompatibleDatabasesAreRebuilt) {
    song();
    write(db, "this is not sqlite");
    {
        SongLibrary library(root, db, {}, audio);
        auto next = scan(library);
        ASSERT_EQ(next->songs.size(), 1u);
        EXPECT_TRUE(next->progress.complete);
    }
    {
        SqliteDb database(db.u8string());
        database.exec("PRAGMA user_version=900");
    }
    SongLibrary library(root, db, {}, audio);
    auto next = scan(library);
    EXPECT_EQ(next->songs.size(), 1u);
    EXPECT_EQ(next->progress.chartParses, 1u);
}

TEST_F(SongLibraryTest, UnwritableCacheKeepsWorkingInMemoryAcrossRefreshes) {
    song();
    write(directory / "not-a-directory", "file");
    SongLibrary library(root, directory / "not-a-directory/index.db", {},
                        audio);
    auto next = scan(library);
    ASSERT_EQ(next->songs.size(), 1u);
    EXPECT_FALSE(next->progress.warning.empty());
    next = scan(library);
    EXPECT_EQ(next->progress.chartParses, 0u);
    EXPECT_EQ(next->progress.audioProbes, 0u);
}

TEST_F(SongLibraryTest,
       CachedSnapshotIsPublishedBeforeSlowScanAndCancellationPreservesRows) {
    const auto path = song();
    song("second");
    {
        SongLibrary library(root, db, {}, audio);
        scan(library);
    }
    fs::remove_all(root / "second");
    write(path / "notes.chart", chart("Changed title"));
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    SongLibrary library(
        root, db,
        [&](const fs::path &file) {
            entered.set_value();
            gate.wait_for(std::chrono::seconds(5));
            return CChart::inspect(file.u8string());
        },
        audio);
    library.refresh();
    const auto ready = entered.get_future().wait_for(std::chrono::seconds(5));
    const auto cached = library.snapshot();
    library.cancel();
    release.set_value();
    library.wait();
    ASSERT_EQ(ready, std::future_status::ready);
    EXPECT_EQ(cached->songs.size(), 2u);
    EXPECT_EQ(library.snapshot()->songs.size(), 2u);
    EXPECT_TRUE(library.snapshot()->progress.cancelled);
    EXPECT_EQ(library.snapshot()->progress.removed, 0u);
}

TEST_F(SongLibraryTest, FilesModifiedDuringInspectionAreNotCommitted) {
    const auto path = song();
    SongLibrary library(
        root, db,
        [&](const fs::path &file) {
            const auto result = CChart::inspect(file.u8string());
            write(path / "notes.chart", chart("new longer title"));
            return result;
        },
        audio);
    const auto result = scan(library);
    EXPECT_FALSE(result->progress.complete);
    EXPECT_TRUE(result->songs.empty());
    EXPECT_FALSE(result->progress.warning.empty());
}

TEST_F(SongLibraryTest,
       NestedBasenameResolutionAndUnrelatedMidiRemainCompatible) {
    const auto path = root / "pack/Named Song";
    write(path / "Named Song.chart", chart());
    write(path / "Named Song.mp3", "audio");
    write(path / "original.mid", "unrelated");
    const auto files = songfiles::list(path);
    ASSERT_EQ(songfiles::charts(path, files).size(), 1u);
    EXPECT_EQ(songfiles::audio(path, files).filename(), "Named Song.mp3");
    SongLibrary library(root, db, {}, audio);
    const auto result = scan(library);
    ASSERT_EQ(result->songs.size(), 1u);
    EXPECT_EQ(result->songs.front().path, "pack/Named Song");
    EXPECT_TRUE(result->songs.front().available);
}

TEST_F(SongLibraryTest, ThousandSongsWarmScanOnlyChecksSignatures) {
    for (unsigned i = 0; i < 1000; ++i) {
        song("pack/" + std::to_string(i));
    }
    SongLibrary library(root, db, {}, audio);
    auto result = scan(library);
    ASSERT_EQ(result->songs.size(), 1000u);
    result = scan(library);
    EXPECT_EQ(result->progress.reused, 1000u);
    EXPECT_EQ(result->progress.chartParses, 0u);
    EXPECT_EQ(result->progress.audioProbes, 0u);
    EXPECT_EQ(result->progress.hashes, 0u);
}

TEST(SongLibraryMenu, RefreshShortcutsDistinguishModifiersAndIgnoreRepeats) {
    CMenu menu;
    CMenu::menuOpt option;
    option.type = CMenu::textbtn;
    option.optionName = "incremental";
    option.shortcutKey = GLFW_KEY_F5;
    option.shortcutMods = 0;
    menu.addOpt(option);
    option.optionName = "full";
    option.shortcutMods = GLFW_MOD_CONTROL;
    menu.addOpt(option);
    CControls::MenuEvent event;
    event.key = GLFW_KEY_F5;
    event.action = GLFW_PRESS;
    event.mods = GLFW_MOD_CONTROL;
    EXPECT_TRUE(menu.processEvent(event));
    EXPECT_EQ(menu.options[0].status & 3, 0);
    EXPECT_EQ(menu.options[1].status & 3, 3);
    menu.resetBtns();
    event.mods = 0;
    EXPECT_TRUE(menu.processEvent(event));
    EXPECT_EQ(menu.options[0].status & 3, 3);
    EXPECT_EQ(menu.options[1].status & 2, 0);
    menu.resetBtns();
    event.action = GLFW_REPEAT;
    menu.processEvent(event);
    EXPECT_EQ(menu.options[0].status & 2, 0);
    EXPECT_EQ(menu.options[1].status & 2, 0);
}

TEST_F(SongLibraryTest, FullRequestsCoalesceDuringAnIncrementalScan) {
    song();
    std::promise<void> entered, release;
    const auto gate = release.get_future().share();
    std::atomic<int> calls{0};
    SongLibrary library(
        root, db,
        [&](const fs::path &file) {
            if (++calls == 1) {
                entered.set_value();
                gate.wait_for(std::chrono::seconds(5));
            }
            return CChart::inspect(file.u8string());
        },
        audio);
    library.refresh();
    const auto ready = entered.get_future().wait_for(std::chrono::seconds(5));
    for (int i = 0; i < 10; ++i) {
        library.refresh(ScanMode::Full);
        library.refresh(ScanMode::Incremental);
    }
    release.set_value();
    library.wait();
    ASSERT_EQ(ready, std::future_status::ready);
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(library.snapshot()->progress.mode, ScanMode::Full);
    EXPECT_TRUE(library.snapshot()->progress.complete);
}

TEST_F(SongLibraryTest, CompiledChartsKeepHistoricalArchiveLayout) {
    const auto path = song();
    CChart compiled;
    ASSERT_TRUE(compiled.open((path / "notes.chart").u8string()));
    ASSERT_TRUE(compiled.compileGppChart((path / "Chart.gpp").u8string()));
    SongLibrary library(root, db, {}, audio);
    const auto result = scan(library);
    ASSERT_EQ(result->songs.size(), 1u);
    EXPECT_EQ(result->songs.front().format, ".gpp");
    EXPECT_EQ(result->songs.front().title, "Canção A");
    EXPECT_EQ(result->songs.front().artist, "Artista");
    EXPECT_TRUE(result->songs.front().available);
}

TEST_F(SongLibraryTest, AudioProbeReadsDurationWithoutAnOutputDevice) {
    const auto path = directory / "one-second.wav";
    {
        std::ofstream out(path, std::ios::binary);
        const auto little = [&](uint32_t number, int bytes) {
            for (int i = 0; i < bytes; ++i) {
                out.put(static_cast<char>((number >> (8 * i)) & 255));
            }
        };
        out.write("RIFF", 4);
        little(88236, 4);
        out.write("WAVEfmt ", 8);
        little(16, 4);
        little(1, 2);
        little(1, 2);
        little(44100, 4);
        little(88200, 4);
        little(2, 2);
        little(16, 2);
        out.write("data", 4);
        little(88200, 4);
        const std::string silence(88200, '\0');
        out.write(silence.data(), static_cast<std::streamsize>(silence.size()));
    }
    const auto result = songs::probeAudio(path);
    ASSERT_TRUE(result.duration.has_value()) << result.error;
    EXPECT_DOUBLE_EQ(*result.duration, 1.0);
    const auto missing = songs::probeAudio(directory / "missing.ogg");
    EXPECT_FALSE(missing.duration.has_value());
    EXPECT_FALSE(missing.error.empty());
}

struct SongListTestAccess {
    static bool update(CMenu &menu, const std::map<int, std::string> &rows,
                       const songs::LibrarySnapshot &snapshot) {
        return CMenuSystem::updateSongRows(menu, rows, snapshot);
    }
};

TEST(SongLibraryMenu, MetadataRefreshKeepsOptionsFocusScrollAndTextCaches) {
    CFonts fonts;
    CMenu::Context context;
    context.fonts = &fonts;
    CMenu menu("library", &context);
    CMenu::menuOpt option;
    option.optionName = "song:pack/song";
    option.type = CMenu::table_row;
    option.cells = {"old"};
    menu.tableColumns = {1};
    menu.addOpt(option);
    menu.select(0);
    menu.scrollOffset = .4;
    menu.options[0].cellLayouts.resize(1);
    menu.options[0].cellLayouts[0].text = "cached";
    auto *original = &menu.options[0];
    songs::LibrarySnapshot snapshot;
    songs::SongRecord song;
    song.path = "pack/song";
    song.title = "new";
    song.available = true;
    song.format = ".mid";
    snapshot.songs.push_back(song);
    const std::map<int, std::string> rows{{0, song.path}};
    ASSERT_TRUE(SongListTestAccess::update(menu, rows, snapshot));
    EXPECT_EQ(&menu.options[0], original);
    EXPECT_EQ(menu.options[0].cells[0], "new");
    EXPECT_EQ(menu.options[0].cellLayouts[0].text, "cached");
    EXPECT_EQ(menu.focusedOption(), 0);
    EXPECT_DOUBLE_EQ(menu.scrollOffset, .4);
    snapshot.songs[0].path = "other/song";
    EXPECT_FALSE(SongListTestAccess::update(menu, rows, snapshot));
    EXPECT_EQ(menu.options[0].optionName, "library.song:pack/song");
}
