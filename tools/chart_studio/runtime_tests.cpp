#include "MidiFixture.h"
#include "runtime.h"
#include <gtest/gtest.h>
using namespace ChartStudio;
TEST(ChartStudioRuntime, RoundTripTempoOffsetChordsAndReferenceProtection) {
    const auto root =
        fs::temp_directory_path() /
        ("chart-roundtrip-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(root);
    struct Cleanup {
        fs::path path;
        ~Cleanup() { fs::remove_all(path); }
    } cleanup{root};
    midi_fixture::Track t;
    t.tempo(0, 500001);
    t.tempo(480, 1000000);
    t.note(240, 720, 64);
    t.note(240, 360, 67);
    t.note(1200, 0, 72);
    midi_fixture::write(root / "music.mid", midi_fixture::smf({t}));
    Session s;
    s.openMidi(root / "music.mid");
    s.state.settings.track = 0;
    s.state.settings.offset = -.25;
    s.run();
    ASSERT_NO_THROW(exportValidated(s.state, root / "out.chart", false));
    const auto reference = readReference(root / "out.chart");
    ASSERT_EQ(reference.notes.size(), 2u);
    EXPECT_NEAR(reference.notes[0].time, 0, 1e-5);
    EXPECT_NEAR(reference.notes[0].duration, .5000005, 1e-5);
    s.setAlignment(1.023, .7, "linear regression test");
    ASSERT_NO_THROW(exportValidated(s.state, root / "aligned.chart", false));
    const auto aligned = readReference(root / "aligned.chart");
    EXPECT_NEAR(aligned.notes[0].time, .7 + .2500005 * 1.023, 1e-5);
    EXPECT_NEAR(aligned.notes[0].duration, .5000005 * 1.023, 1e-5);
    EXPECT_EQ(s.state.score.tempos[1].microseconds, 1000000);
    s.state.reference = (root / "out.chart").string();
    const auto before = fileHash(root / "out.chart");
    EXPECT_THROW(exportValidated(s.state, root / "out.chart", true),
                 std::exception);
    EXPECT_EQ(fileHash(root / "out.chart"), before);
    EXPECT_NE(compare(asReference(s.state), reference).find("windows"),
              std::string::npos);
}

TEST(ChartStudioRuntime, SustainCutoffPreservesScoreAndUsesMusicalTicks) {
    State state;
    state.score.ppq = 480;
    state.score.tempos = {{0, 500000}, {240, 1000000}};
    state.settings.track = 0;
    const auto add = [&](int64_t tick, int64_t duration, int mask) {
        Event event;
        event.note.id = state.nextId++;
        event.note.flags = mask;
        event.tick = tick;
        event.duration = duration;
        state.events.push_back(event);
    };
    add(0, 239, 1);
    add(240, 240, 2);
    add(480, 241, 4);
    add(720, 960, 3); // A legitimate long chord overlapping later attacks.
    add(960, 120, 8);
    synchronize(state);
    EXPECT_EQ(exportDuration(state, state.events[0]), 0);
    EXPECT_EQ(exportDuration(state, state.events[1]), 0);
    EXPECT_EQ(exportDuration(state, state.events[2]), 241);
    EXPECT_EQ(exportDuration(state, state.events[3]), 960);
    EXPECT_EQ(state.events[1].duration, 240); // Editing/source data unchanged.
    EXPECT_DOUBLE_EQ(asReference(state).notes[1].duration, 0);
    EXPECT_DOUBLE_EQ(asReference(state).notes[3].duration, 2);
    const auto path =
        fs::temp_directory_path() /
        ("sustain-review-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".chart");
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code error;
            fs::remove(path, error);
        }
    } cleanup{path};
    exportValidated(state, path, false);
    const auto loaded = readReference(path);
    ASSERT_EQ(loaded.notes.size(), 5u);
    EXPECT_EQ(loaded.notes[0].duration, 0);
    EXPECT_EQ(loaded.notes[1].duration, 0);
    EXPECT_NEAR(loaded.notes[2].duration, 241.0 / 480, 1e-6);
    EXPECT_NEAR(loaded.notes[3].duration, 2, 1e-6);
    state.settings.minimumSustain = 0;
    EXPECT_EQ(exportDuration(state, state.events[1]), 240);
    state.settings.minimumSustain = .6;
    EXPECT_EQ(exportDuration(state, state.events[2]), 0);
    EXPECT_EQ(exportDuration(state, state.events[3]), 960);
    state.settings.minimumSustain = -.5;
    state.settings.minimumSustain = -2;
    EXPECT_EQ(exportDuration(state, state.events[3]), 0); // Exactly two beats.
    state.settings.minimumSustain = -1.5;
    EXPECT_EQ(exportDuration(state, state.events[3]), 960);
    state.settings.minimumSustain = -.5;
    state.score.ppq = 481; // Half a beat need not be an integer tick.
    EXPECT_EQ(exportDuration(state, state.events[1]), 0);
    EXPECT_EQ(exportDuration(state, state.events[2]), 241);
}

#include "CChart.h"
#include "CPlayer.h"
#include "plus_simulation.h"
#include "sqlite3/sqlite3raii.hpp"

TEST(ChartStudioPlus, LoaderKeepsAttacksAtMarkerTicksAndIndices) {
    State state;
    state.score.ppq = 192;
    for (int i = 0; i < 20; ++i) {
        Event e;
        e.note.id = state.nextId++;
        e.note.flags = 1 << (i % 5);
        e.tick = i * 96;
        state.events.push_back(e);
    }
    synchronize(state);
    PlusPhrase first, second;
    first.id = 1;
    first.begin = 0;
    first.end = 384;
    second.id = 2;
    second.begin = 576;
    second.end = 960;
    state.plus.phrases = {first, second};
    state.plus.nextId = 3;
    auto path =
        fs::temp_directory_path() /
        ("plus-roundtrip-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".chart");
    struct Cleanup {
        fs::path path;
        ~Cleanup() { fs::remove(path); }
    } cleanup{path};
    exportValidated(state, path, false);
    CChart::NotesData loaded;
    CChart::Metadata metadata;
    ASSERT_TRUE(loaded.loadFeedbackChartCPU(path.u8string(), "[ExpertSingle]",
                                            metadata));
    ASSERT_EQ(loaded.gNotes.size(), 20u);
    ASSERT_EQ(loaded.gPlus.size(), 2u);
    EXPECT_EQ(loaded.gPlus[0].firstNote, 0u);
    EXPECT_EQ(loaded.gPlus[0].lastNote, 3u);
    EXPECT_EQ(loaded.gPlus[1].firstNote, 6u);
    EXPECT_EQ(loaded.gPlus[1].lastNote, 9u);
    state.score.tempos.push_back({700, 750000});
    state.alignment.scale = 1.023;
    state.settings.offset = -.2;
    synchronize(state);
    EXPECT_NO_THROW(exportValidated(state, path, true));
    state.plus.phrases[1].begin = 384;
    EXPECT_THROW(exportValidated(state, path, true), std::exception);
}
TEST(ChartStudioPlus, LegacyProjectMigrationAndAtomicProtection) {
    const auto root =
        fs::temp_directory_path() /
        ("plus-project-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(root);
    struct Cleanup {
        fs::path path;
        ~Cleanup() { fs::remove_all(path); }
    } cleanup{root};
    for (int version : {1, 2}) {
        State state;
        const auto path = root / (std::to_string(version) + ".gppchartproj");
        saveProject(path, state, false);
        std::ostringstream legacy;
        {
            cereal::JSONOutputArchive archive(legacy);
            archive(state);
        }
        {
            SqliteDb db(path.u8string());
            db.exec("PRAGMA user_version=" + std::to_string(version));
            auto update =
                db.prepare("UPDATE metadata SET value=? WHERE key='document'",
                           "legacy fixture");
            (void)update.bind(1, legacy.str()).step();
        }
        const auto before = fileHash(path);
        auto loaded = loadProject(path);
        EXPECT_TRUE(loaded.plus.phrases.empty());
        EXPECT_FALSE(loaded.plus.settings.enabled);
        EXPECT_EQ(before, fileHash(path));
        loaded.plus.settings = plusProfile("sparse");
        EXPECT_THROW(saveProject(path, loaded, false), std::exception);
        EXPECT_EQ(before, fileHash(path));
        saveProject(path, loaded, true);
        EXPECT_EQ(loadProject(path).plus.settings.profile, "sparse");
        SqliteDb db(path.u8string());
        auto query = db.prepare("PRAGMA user_version", "version");
        (void)query.step();
        EXPECT_EQ(sqlite3_column_int(query.get(), 0), 3);
    }
}
TEST(ChartStudioPlus, RuntimeEconomyConstantsAndAttackScore) {
    CPlayer player("Plus economy");
    EXPECT_DOUBLE_EQ(player.maxPlusPower, gpp::plus::maximumEnergy);
    EXPECT_DOUBLE_EQ(player.maxPlusPower / gpp::plus::phrasesPerBar, .2);
    EXPECT_DOUBLE_EQ(.4 / gpp::plus::drainPerSecond, 8);
    player.Notes.gNotes.resize(2);
    player.Notes.gNotes[0].type = nf_green;
    player.Notes.gNotes[1].type = nf_green | nf_red | nf_blue;
    const auto before = player.getPoints();
    player.doNote(0);
    player.doNote(1);
    EXPECT_EQ(player.getPoints() - before, 100);
}

TEST(ChartStudioPlus, MidiReferenceUsesGameplayParserAndOriginalTempo) {
    midi_fixture::Track track;
    track.tempo(0, 500000);
    track.note(0, 900, 116);
    track.note(0, 0, 96);
    track.note(480, 0, 97);
    track.note(960, 0, 98);
    const auto path =
        fs::temp_directory_path() /
        ("plus-midi-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".mid");
    struct Cleanup {
        fs::path path;
        ~Cleanup() { fs::remove(path); }
    } cleanup{path};
    midi_fixture::write(path, midi_fixture::smf({track}));
    const auto reference = readPlusReference(path);
    EXPECT_EQ(reference.first.ppq, 480);
    ASSERT_EQ(reference.second.phrases.size(), 1u);
    EXPECT_EQ(reference.second.phrases.front().runtimeFirst, 0);
    EXPECT_EQ(reference.second.phrases.front().runtimeLast, 1);
    EXPECT_DOUBLE_EQ(
        simulatePlus(reference.first, reference.second.phrases, {}).completed,
        1);
}

TEST(ChartStudioRuntime, ExportDoesNotRequireStrategicMeasureAnalysis) {
    const auto path =
        fs::temp_directory_path() /
        ("plus-long-export-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".chart");
    struct Cleanup {
        fs::path path;
        ~Cleanup() { fs::remove(path); }
    } cleanup{path};
    State state;
    Event first;
    first.note.id = state.nextId++;
    first.note.flags = 1;
    state.events.push_back(first);
    Event last = first;
    last.note.id = state.nextId++;
    last.tick = int64_t(state.score.ppq) * 4 * 100001;
    state.events.push_back(last);
    synchronize(state);
    // The optional planner has a measure limit, ordinary export does not.
    ASSERT_NO_THROW(exportValidated(state, path, false));
    PlusPhrase phrase;
    phrase.id = state.plus.nextId++;
    phrase.begin = 0;
    phrase.end = state.score.ppq;
    state.plus.phrases.push_back(phrase);
    ASSERT_NO_THROW(exportValidated(state, path, true));
}
