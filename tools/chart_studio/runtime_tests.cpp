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
