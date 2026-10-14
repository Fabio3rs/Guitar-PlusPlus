#include "MidiFixture.h"
#include "core.h"
#include "sqlite3/sqlite3raii.hpp"
#include <algorithm>
#include <fstream>
#include <gtest/gtest.h>

using namespace ChartStudio;
namespace {
class ScoreTest : public testing::Test {
  protected:
    fs::path root;
    void SetUp() override {
        root =
            fs::temp_directory_path() /
            ("gpp-score-test-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(root);
    }
    void TearDown() override { fs::remove_all(root); }
    fs::path midi(const std::vector<int> &pitches = {64, 66, 67, 71, 69},
                  int repeats = 2, int transpose = 0) {
        midi_fixture::Track track("Guitar");
        track.events.push_back({0, midi_fixture::data({0xc0, 30})});
        track.tempo(0, 500000);
        track.meta(0, 0x58, midi_fixture::data({4, 2, 24, 8}));
        for (int repeat = 0; repeat < repeats; ++repeat) {
            for (size_t i = 0; i < pitches.size(); ++i) {
                track.note(uint32_t(size_t(repeat) * 3840 + i * 240), 120,
                           unsigned(pitches[i] + repeat * transpose));
            }
        }
        const auto path = root / "musical.mid";
        midi_fixture::write(path, midi_fixture::smf({track}));
        return path;
    }
    Session mapped() {
        Session s;
        s.openMidi(midi());
        s.state.settings.track = 0;
        s.run();
        return s;
    }
};
std::vector<int> masks(const State &s) {
    std::vector<int> result;
    result.reserve(s.events.size());
    for (const auto &e : s.events) {
        result.push_back(e.note.flags);
    }
    return result;
}

TEST_F(ScoreTest, MusicalTypeOnePreservesPitchesRhythmAndPrograms) {
    const auto score = importMidi(midi());
    ASSERT_EQ(score.tracks.size(), 1u);
    EXPECT_EQ(score.ppq, 480);
    EXPECT_EQ(score.tracks[0].notes[0].pitch, 64);
    EXPECT_EQ(score.tracks[0].notes[0].program, 30);
    EXPECT_EQ(score.tracks[0].notes[0].end, 120);
    EXPECT_EQ(score.signatures[0].numerator, 4);
}

TEST_F(ScoreTest, TypeZeroOverlapsChannelsVelocityZeroAndProgramChanges) {
    midi_fixture::Track track("Type 0");
    track.events.push_back({0, midi_fixture::data({0xc0, 30})});
    track.events.push_back({0, midi_fixture::data({0xc1, 24})});
    track.note(0, 480, 64, true, 0);
    track.note(120, 480, 64, false, 0);
    track.note(0, 240, 64, false, 1);
    track.note(0, 60, 36, false, 9);
    track.events.push_back({700, midi_fixture::data({0xc0, 31})});
    track.note(720, 120, 60);
    const auto path = root / "type0.mid";
    midi_fixture::write(path, midi_fixture::smf({track}, 480, 0));
    const auto score = importMidi(path);
    ASSERT_EQ(score.tracks[0].notes.size(), 5u);
    EXPECT_EQ(score.tracks[0].notes[0].end, 480);
    EXPECT_EQ(score.tracks[0].notes[3].end, 600);
    EXPECT_EQ(score.tracks[0].notes.back().program, 31);
    EXPECT_NE(inspect(score).find("PERCUSSION"), std::string::npos);
}

TEST_F(ScoreTest, IndependentTracksDoNotMergeSamePitch) {
    midi_fixture::Track a("A"), b("B");
    a.note(0, 480, 64);
    b.note(0, 120, 64);
    const auto path = root / "tracks.mid";
    midi_fixture::write(path, midi_fixture::smf({a, b}));
    const auto score = importMidi(path);
    EXPECT_NE(score.tracks[0].notes[0].id, score.tracks[1].notes[0].id);
    EXPECT_NE(score.tracks[0].notes[0].end, score.tracks[1].notes[0].end);
}

TEST_F(ScoreTest, TempoAndSignatureChangesIntegrateSustain) {
    midi_fixture::Track t;
    t.tempo(0, 500000);
    t.tempo(480, 1000000);
    t.meta(480, 0x58, midi_fixture::data({3, 3, 24, 8}));
    t.note(240, 720, 64);
    const auto path = root / "tempo.mid";
    midi_fixture::write(path, midi_fixture::smf({t}));
    const auto score = importMidi(path);
    EXPECT_DOUBLE_EQ(score.seconds(240), .25);
    EXPECT_DOUBLE_EQ(score.seconds(960), 1.5);
    EXPECT_EQ(score.ticks(1.5), 960);
    EXPECT_EQ(score.signatures.back().denominator, 8);
}

TEST_F(ScoreTest, InvalidAndUnclosedMidiAreRejected) {
    auto path = midi();
    midi_fixture::write(path, "invalid");
    EXPECT_THROW(importMidi(path), std::exception);
    midi_fixture::Track t;
    t.events.push_back({0, midi_fixture::data({0x90, 64, 100})});
    midi_fixture::write(path, midi_fixture::smf({t}));
    EXPECT_THROW(importMidi(path), std::exception);
    t.events.push_back({1, midi_fixture::data({0x80, 64, 0})});
    midi_fixture::write(path, midi_fixture::smf({t}, 0));
    EXPECT_THROW(importMidi(path), std::exception);
}

TEST_F(ScoreTest, ExplicitTrackRequiredAndPercussionNotImplicitlySelected) {
    Session s;
    s.openMidi(midi());
    EXPECT_THROW(s.run(), std::exception);
    for (auto &n : s.state.score.tracks[0].notes) {
        n.channel = 9;
    }
    s.state.settings.track = 0;
    EXPECT_THROW(s.run(), std::exception);
    s.state.settings.channel = 9;
    EXPECT_NO_THROW(s.run());
}

TEST_F(ScoreTest, AscendingDescendingAndDeterministicStrategies) {
    for (const std::string algorithm : {"beam", "heuristic"}) {
        for (const auto &pitches : {std::vector<int>{60, 62, 64, 65, 67},
                                    std::vector<int>{67, 65, 64, 62, 60}}) {
            Session s;
            s.openMidi(midi(pitches, 1));
            s.state.settings.track = 0;
            s.state.settings.algorithm = algorithm;
            s.run();
            auto before = masks(s.state);
            EXPECT_EQ(before.size(), 5u);
            if (pitches.front() < pitches.back()) {
                EXPECT_TRUE(std::is_sorted(before.begin(), before.end()));
            } else {
                EXPECT_TRUE(std::is_sorted(before.rbegin(), before.rend()));
            }
            s.run();
            EXPECT_EQ(masks(s.state), before);
        }
    }
}

TEST_F(ScoreTest, RepeatedPhraseSharesLanesDuringFirstGeneration) {
    auto s = mapped();
    ASSERT_EQ(s.state.events.size(), 10u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(s.state.events[i].note.flags,
                  s.state.events[i + 5].note.flags);
    }
    EXPECT_EQ(s.state.events[5].origin, "project-repeat");
}

TEST_F(ScoreTest, PersistentProjectReuseAndCanonicalStability) {
    auto s = mapped();
    const auto before = chartText(s.state);
    const auto key = s.state.events[0].exact;
    s.save(root / "project.gppchartproj");
    Session reopened;
    reopened.openProject(root / "project.gppchartproj");
    reopened.run();
    EXPECT_EQ(chartText(reopened.state), before);
    EXPECT_EQ(reopened.state.events[0].exact, key);
    EXPECT_EQ(reopened.state.events[0].origin, "project-cache");
    EXPECT_EQ(hash(key), hash(reopened.state.events[0].exact));
}

TEST_F(ScoreTest, StrictTranspositionReusesConfirmedLibrary) {
    auto s = mapped();
    s.pattern(s.state.events[0].note.id, 1, true);
    promote(root / "patterns.sqlite", s.state.patterns.begin()->second);
    Session second;
    second.openMidi(midi({69, 71, 72, 76, 74}, 1));
    second.state.settings.track = 0;
    second.run(loadLibrary(root / "patterns.sqlite"));
    EXPECT_EQ(second.state.events[0].origin, "library-cache");
    EXPECT_EQ(second.state.events[0].reason, "strict transposition");
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(second.state.events[i].note.flags,
                  s.state.events[i].note.flags);
    }
}

TEST_F(ScoreTest,
       SimilarPhraseIsNotSilentlyEquivalentAndStructuralCanBeDisabled) {
    auto s = mapped();
    s.pattern(s.state.events[0].note.id, 1, true);
    promote(root / "library", s.state.patterns.begin()->second);
    Session other;
    other.openMidi(midi({69, 71, 73, 76, 74}, 1));
    other.state.settings.track = 0;
    other.run(loadLibrary(root / "library"));
    EXPECT_EQ(other.state.events[0].origin, "algorithm");
    other.openMidi(midi({69, 71, 72, 76, 74}, 1));
    other.state.settings.track = 0;
    other.state.settings.structural = false;
    other.run(loadLibrary(root / "library"));
    EXPECT_EQ(other.state.events[0].origin, "algorithm");
}

TEST_F(ScoreTest, ManualLockedEditsSurviveWeightsAndExperimentalCleanup) {
    auto s = mapped();
    const auto event = s.state.events[0];
    s.edit(event.note.id, 16, event.tick, event.duration, true);
    s.pattern(event.note.id, 2, true);
    s.state.settings.weights[0] = 0;
    s.state.settings.weights[1] = 90;
    s.clearExperimental();
    s.run();
    EXPECT_EQ(s.state.events[0].note.flags, 16);
    EXPECT_EQ(s.state.events[5].note.flags, 16);
    EXPECT_EQ(s.state.patterns.at(event.exact).status, 2);
}

TEST_F(ScoreTest, ConfirmedPatternsSurviveParameterChanges) {
    auto s = mapped();
    s.pattern(s.state.events[0].note.id, 1, true);
    const auto before = masks(s.state);
    s.state.settings.weights.fill(0);
    s.run();
    EXPECT_EQ(masks(s.state), before);
}

TEST_F(ScoreTest, IncompatibleProtectedPatternReportsConflictAndKeepsNotes) {
    auto s = mapped();
    const auto e = s.state.events[0];
    s.edit(e.note.id, 31, e.tick, e.duration, true);
    s.pattern(e.note.id, 2, true);
    s.run();
    EXPECT_FALSE(s.state.conflicts.empty());
    EXPECT_EQ(s.state.events[0].note.flags, 31);
}

TEST_F(ScoreTest, InputOrderDoesNotChangeMusicalDecision) {
    auto s = mapped();
    const auto before = masks(s.state);
    s.state.events.clear();
    s.state.patterns.clear();
    std::reverse(s.state.score.tracks[0].notes.begin(),
                 s.state.score.tracks[0].notes.end());
    s.run();
    EXPECT_EQ(masks(s.state), before);
}

TEST_F(ScoreTest, MissingCorruptAndFutureDatabasesAreSafe) {
    EXPECT_TRUE(loadLibrary(root / "absent").empty());
    safeWrite(root / "corrupt", "not sqlite", false);
    const auto before = fileHash(root / "corrupt");
    EXPECT_THROW(loadLibrary(root / "corrupt"), std::exception);
    EXPECT_EQ(fileHash(root / "corrupt"), before);
    auto s = mapped();
    s.save(root / "future");
    {
        SqliteDb db((root / "future").string());
        db.exec("PRAGMA user_version=999");
    }
    const auto future = fileHash(root / "future");
    EXPECT_THROW(loadProject(root / "future"), std::exception);
    EXPECT_EQ(fileHash(root / "future"), future);
}

TEST_F(ScoreTest, FailedPromotionRollsBackAndDoesNotLoseConfirmedDecision) {
    auto s = mapped();
    s.pattern(s.state.events[0].note.id, 1, true);
    auto p = s.state.patterns.begin()->second;
    promote(root / "library", p);
    const auto old = loadLibrary(root / "library");
    p.masks[0] = p.masks[0] == 1 ? 2 : 1;
    EXPECT_THROW(promote(root / "library", p), std::exception);
    EXPECT_EQ(loadLibrary(root / "library")[0].masks, old[0].masks);
}

TEST_F(ScoreTest, EditorIdsAndUndoSurviveInsertReorderAndDelete) {
    gpp::charter::Editor editor;
    uint64_t first = 0, second = 0;
    editor.edit([&](auto &d) {
        first = d.insert({0, 2, 1, 1});
        second = d.insert({0, 1, 0, 2});
    });
    editor.edit([&](auto &d) {
        d.move(first, 0);
        d.lanes(second, 7);
        d.sustain(second, 3);
    });
    EXPECT_EQ(editor.document().notes.front().id, first);
    EXPECT_EQ(editor.document().find(second)->duration, 3);
    editor.edit([&](auto &d) { d.remove(first); });
    EXPECT_EQ(editor.document().find(first), nullptr);
    ASSERT_TRUE(editor.undo());
    EXPECT_NE(editor.document().find(first), nullptr);
    ASSERT_TRUE(editor.redo());
    EXPECT_EQ(editor.document().find(first), nullptr);
}

TEST_F(ScoreTest, ManualMoveCopyPasteDeleteAndUndoArePersistent) {
    auto s = mapped();
    const auto e = s.state.events.front();
    s.edit(e.note.id, 3, 3000, 900, false);
    EXPECT_TRUE(s.undo());
    EXPECT_TRUE(s.redo());
    s.selection = {e.note.id};
    s.copy();
    const auto size = s.state.events.size();
    s.paste(12000);
    EXPECT_EQ(s.state.events.size(), size + 1);
    EXPECT_NE(*s.selection.begin(), e.note.id);
    s.removeSelected();
    s.run();
    EXPECT_TRUE(s.state.events.back().removed);
}

TEST_F(ScoreTest, ChordsRetainIndividualDurationsUntilExportPolicy) {
    midi_fixture::Track t;
    t.note(0, 480, 60);
    t.note(0, 240, 64);
    t.note(0, 960, 67);
    t.note(0, 360, 71);
    auto path = root / "chord.mid";
    midi_fixture::write(path, midi_fixture::smf({t}));
    Session s;
    s.openMidi(path);
    s.state.settings.track = 0;
    s.run();
    ASSERT_EQ(s.state.events.size(), 1u);
    EXPECT_EQ(s.state.events[0].sources.size(), 4u);
    EXPECT_EQ(s.state.events[0].duration, 240);
    EXPECT_EQ(exportDuration(s.state, s.state.events[0]), 0);
    s.state.settings.minimumSustain =
        0; // Explicitly preserve short source sustains.
    EXPECT_NE(chartText(s.state).find(" 240\n"), std::string::npos);
}

TEST_F(ScoreTest, ProjectMissingMidiStillOpensAndExportsWithoutAudio) {
    auto s = mapped();
    s.save(root / "project");
    fs::remove(fs::u8path(s.state.input));
    Session reopened;
    reopened.openProject(root / "project");
    EXPECT_FALSE(chartText(reopened.state).empty());
}

TEST_F(ScoreTest, ExternalProjectChangeAndFailedSavePreserveOriginal) {
    auto s = mapped();
    s.save(root / "project");
    const auto original = fileHash(root / "project");
    EXPECT_THROW(saveProject(root / "project", s.state, false), std::exception);
    EXPECT_EQ(fileHash(root / "project"), original);
    {
        SqliteDb db((root / "project").string());
        db.exec("INSERT INTO metadata VALUES('external','edit')");
    }
    const auto modified = fileHash(root / "project");
    EXPECT_THROW(s.save(root / "project"), std::exception);
    EXPECT_EQ(fileHash(root / "project"), modified);
}
TEST_F(ScoreTest, BeatNavigationCrossesTempoChangesInBothDirections) {
    const std::vector<gpp::charter::TempoPoint> tempos{
        {0, 120}, {1, 60}, {3, 240}};
    EXPECT_DOUBLE_EQ(gpp::charter::advanceBeats(tempos, .5, 2), 2);
    EXPECT_DOUBLE_EQ(gpp::charter::advanceBeats(tempos, 2, -2), .5);
    EXPECT_DOUBLE_EQ(gpp::charter::advanceBeats(tempos, 3, -1), 2);
    EXPECT_DOUBLE_EQ(gpp::charter::advanceBeats(tempos, 3, 1), 3.25);
}

TEST_F(ScoreTest, SqliteOperationFailureDoesNotLoseOtherPatterns) {
    auto s = mapped();
    s.pattern(s.state.events[0].note.id, 1, true);
    auto p = s.state.patterns.begin()->second;
    promote(root / "library", p);
    {
        SqliteDb db((root / "library").string());
        db.exec("CREATE TRIGGER fail_insert BEFORE INSERT ON patterns BEGIN "
                "SELECT RAISE(ABORT,'test failure'); END;");
    }
    const auto before = loadLibrary(root / "library");
    p.exact += "different";
    EXPECT_THROW(promote(root / "library", p), std::exception);
    EXPECT_EQ(loadLibrary(root / "library").size(), before.size());
    EXPECT_EQ(loadLibrary(root / "library")[0].masks, before[0].masks);
}

TEST_F(ScoreTest, ExactLibraryReuseAndOpposingConfirmedPatternsAreReported) {
    auto s = mapped();
    s.pattern(s.state.events[0].note.id, 1, true);
    auto p = s.state.patterns.begin()->second;
    promote(root / "library", p);
    Session other;
    other.openMidi(midi());
    other.state.settings.track = 0;
    other.run(loadLibrary(root / "library"));
    EXPECT_EQ(other.state.events[0].origin, "library-cache");
    EXPECT_EQ(other.state.events[0].reason, "exact");
    auto conflicting = p;
    conflicting.masks[0] = p.masks[0] == 1 ? 2 : 1;
    Session third;
    third.openMidi(midi());
    third.state.settings.track = 0;
    third.run({p, conflicting});
    EXPECT_FALSE(third.state.conflicts.empty());
}

TEST_F(ScoreTest, RegionGenerationPreservesOutsideAndUnconfirmedManualEdits) {
    auto s = mapped();
    const auto e = s.state.events[0];
    s.edit(e.note.id, 16, e.tick + 10, e.duration + 100, false);
    const auto edited = s.state.events[0];
    s.state.settings.weights.fill(0);
    s.run({}, {{3840, 6000}});
    EXPECT_EQ(s.state.events[0].tick, edited.tick);
    EXPECT_EQ(s.state.events[0].note.flags, 16);
    s.run();
    EXPECT_EQ(s.state.events[0].note.flags, 16);
}

TEST_F(ScoreTest, PersistedCacheCannotChangePhraseBoundaries) {
    std::vector<int> pitches;
    pitches.reserve(100);
    for (int i = 0; i < 100; ++i) {
        pitches.push_back(48 + ((i * 19 + i / 7) % 27));
    }
    Session first;
    first.openMidi(midi(pitches, 1));
    first.state.settings.track = 0;
    first.run();
    const auto before = masks(first.state);
    std::vector<std::string> phrases;
    phrases.reserve(first.state.events.size());
    for (const auto &event : first.state.events) {
        phrases.push_back(event.exact);
    }
    first.save(root / "stable.gppchartproj");
    Session second;
    second.openProject(root / "stable.gppchartproj");
    second.run();
    EXPECT_EQ(before, masks(second.state));
    for (size_t i = 0; i < phrases.size(); ++i) {
        EXPECT_EQ(phrases[i], second.state.events[i].exact);
        EXPECT_EQ(second.state.events[i].origin, "project-cache");
    }
}

TEST_F(ScoreTest, UmapsAreOptionalAndCannotOverrideLocksOrMismatchedInput) {
    auto s = mapped();
    Projection p;
    p.identity = "test";
    p.inputHash = s.state.inputHash;
    p.difficulty = "expert";
    p.track = 0;
    std::map<std::string, std::vector<int>> rows;
    for (const auto &e : s.state.events) {
        rows[e.phrase].push_back(48);
    }
    for (const auto &e : s.state.events) {
        p.positions[e.structural] = rows[e.phrase];
    }
    s.state.settings.algorithm = "umap:test";
    EXPECT_THROW(s.run(), std::exception);
    s.projection = p;
    ASSERT_NO_THROW(s.run());
    const auto calculated = masks(s.state);
    s.run();
    EXPECT_EQ(calculated, masks(s.state));
    EXPECT_EQ(s.state.events.front().origin, "project-cache");
    s.edit(s.state.events.front().note.id, 1, s.state.events.front().tick,
           s.state.events.front().duration, true);
    s.pattern(s.state.events.front().note.id, 2, true);
    const auto locked = masks(s.state);
    s.projection->identity = "another-seed";
    for (auto &row : s.projection->positions) {
        std::fill(row.second.begin(), row.second.end(), 0);
    }
    s.state.settings.algorithm = "umap:another-seed";
    s.run();
    EXPECT_EQ(locked, masks(s.state));
    s.save(root / "umap.gppchartproj");
    Session opened;
    opened.openProject(root / "umap.gppchartproj");
    EXPECT_THROW(opened.run(), std::exception);
    opened.projection = s.projection;
    opened.run();
    EXPECT_EQ(locked, masks(opened.state));
    opened.projection->inputHash = "wrong source";
    EXPECT_THROW(opened.run(), std::exception);
    EXPECT_EQ(locked, masks(opened.state));
    s.projection->positions.begin()->second.clear();
    EXPECT_THROW(s.run(), std::exception);
}

} // namespace
