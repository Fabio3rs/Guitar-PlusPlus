#include "merge.h"
#include <algorithm>
#include <gtest/gtest.h>
using namespace ChartStudio;
namespace {
State fixture() {
    State s;
    s.settings.track = 0;
    s.settings.minimumSustain = 0;
    s.score.tracks.resize(2);
    s.score.tracks[0].notes = {{1, 60, 100, 0, 30, 0, 960},
                               {2, 64, 100, 0, 30, 3840, 4320}};
    s.score.tracks[1].notes = {{3, 65, 100, 1, 30, 480, 1440},
                               {4, 67, 100, 1, 30, 1920, 4080},
                               {5, 69, 100, 1, 30, 3840, 4080}};
    s.score.ppq = 480;
    generate(s);
    return s;
}
} // namespace
TEST(ChartStudioMerge, SilenceSustainPriorityAndProvenance) {
    const auto s = fixture();
    MergeConfig c;
    c.secondaryTrack = 1;
    auto p = proposeMerge(s, c);
    const auto &doc = p.state.merges.at(1);
    ASSERT_EQ(p.state.events.size(), 3u);
    EXPECT_EQ(p.state.events[0].tick, 0);
    EXPECT_EQ(p.state.events[0].duration, 960);
    EXPECT_EQ(p.state.events[1].tick, 1920);
    EXPECT_EQ(p.state.events[1].duration, 1919);
    EXPECT_EQ(p.state.events[2].tick, 3840);
    EXPECT_EQ(doc.primary.events.size(), 2u);
    EXPECT_EQ(doc.secondary.events.size(), 3u);
    EXPECT_FALSE(p.state.events[1].sources.empty());
    EXPECT_TRUE(p.state.events[1].exact.empty());
    EXPECT_EQ(encode(s), encode(fixture()));
    EXPECT_EQ(encode(p.state), encode(proposeMerge(s, c).state));
    EXPECT_EQ(encode(p.state), encode(decode(encode(p.state))));
}
TEST(ChartStudioMerge, ThresholdsManualCutsAndNoResumedTail) {
    const auto s = fixture();
    MergeConfig c;
    c.secondaryTrack = 1;
    c.minimum = "2";
    EXPECT_EQ(proposeMerge(s, c).state.events.size(), 2u);
    c.minimum = "1/2";
    c.maximum = "1";
    EXPECT_EQ(proposeMerge(s, c).state.events.size(), 2u);
    c.maximum = "off";
    c.regions = {{480, 3000, 1}, {3000, 3840, 2}};
    auto p = proposeMerge(s, c);
    ASSERT_EQ(p.state.events.size(), 4u);
    EXPECT_EQ(p.state.events[0].duration, 479);
    EXPECT_EQ(p.state.events[1].tick, 480);
    EXPECT_EQ(p.state.events[2].duration, 1079);
    c.regions.push_back({2000, 4000, 0});
    EXPECT_THROW(proposeMerge(s, c), std::runtime_error);
}
TEST(ChartStudioMerge, SessionIdentityUndoAndProtectedConflicts) {
    Session session;
    session.state = fixture();
    MergeConfig c;
    c.secondaryTrack = 1;
    const auto original = encode(session.state);
    session.applyMerge(proposeMerge(session.state, c));
    const auto merged = encode(session.state);
    EXPECT_TRUE(session.undo());
    EXPECT_EQ(original, encode(session.state));
    EXPECT_TRUE(session.redo());
    EXPECT_EQ(merged, encode(session.state));
    auto proposal = proposeMerge(session.state, c);
    EXPECT_EQ(merged, encode(proposal.state));
    const auto e = session.state.events[1];
    session.edit(e.note.id, 1, e.tick, e.duration, false);
    EXPECT_THROW(session.applyMerge(proposal), std::runtime_error);
    auto retained = proposeMerge(session.state, c);
    EXPECT_EQ(retained.state.events[1].note.flags, 1);
    c.regions = {{960, 3840, 0}};
    EXPECT_THROW(proposeMerge(session.state, c), std::runtime_error);
    session.detachMerge();
    EXPECT_EQ(session.state.events.size(), 2u);
    EXPECT_TRUE(session.state.merges.empty());
    EXPECT_TRUE(session.undo());
    EXPECT_FALSE(session.state.merges.empty());
}
TEST(ChartStudioMerge, RationalGridAndRecipe) {
    EXPECT_EQ(mergeSnap(160, 480, "1/12"), 160);
    EXPECT_EQ(mergeSnap(80, 480, "1/12"), 0);
    EXPECT_EQ(mergeSnap(81, 480, "1/12"), 160);
    EXPECT_EQ(mergeSnap(34, 100, "1/12"), 33);
    EXPECT_THROW(mergeSnap(0, 480, "1/0"), std::runtime_error);
    auto s = fixture();
    MergeConfig c;
    c.secondaryTrack = 1;
    s = proposeMerge(s, c).state;
    auto other = fixture();
    readMergeRecipe(other, mergeRecipe(s));
    EXPECT_EQ(other.merges.at(1).config.secondaryTrack, 1);
    auto q = proposeMerge(other, other.merges.at(1).config);
    EXPECT_EQ(s.events.size(), q.state.events.size());
}

TEST(ChartStudioMerge, TempoMeterAndOffsetDoNotChangeSelection) {
    auto s = fixture();
    MergeConfig c;
    c.secondaryTrack = 1;
    const auto a = proposeMerge(s, c).state;
    s.score.tempos = {{0, 500000}, {1500, 800000}};
    s.score.signatures = {{0, 3, 4}, {2000, 5, 8}};
    s.settings.offset = 2.3;
    s.alignment.scale = .996;
    synchronize(s);
    const auto b = proposeMerge(s, c).state;
    ASSERT_EQ(a.events.size(), b.events.size());
    for (size_t i = 0; i < a.events.size(); ++i) {
        EXPECT_EQ(a.events[i].tick, b.events[i].tick);
        EXPECT_EQ(a.events[i].duration, b.events[i].duration);
        EXPECT_EQ(a.events[i].note.flags, b.events[i].note.flags);
    }
}
TEST(ChartStudioMerge, EmptySourcesLimitsAndRegeneration) {
    auto s = fixture();
    MergeConfig c;
    c.secondaryTrack = 1;
    s.score.tracks[1].notes.clear();
    EXPECT_EQ(proposeMerge(s, c).state.events.size(), 2u);
    s = fixture();
    s.score.tracks[0].notes.clear();
    s.events.clear();
    s.original.clear();
    s.patterns.clear();
    s.nextId = 1;
    const auto proposal = proposeMerge(s, c);
    EXPECT_EQ(proposal.state.events.size(), 3u);
    auto repeated = proposal.state;
    generateMerged(repeated, {});
    ASSERT_EQ(repeated.events.size(), proposal.state.events.size());
    for (size_t i = 0; i < repeated.events.size(); ++i) {
        EXPECT_EQ(repeated.events[i].note.id, proposal.state.events[i].note.id);
    }
    c.maximum = "1/4";
    EXPECT_THROW(proposeMerge(s, c), std::runtime_error);
    c.maximum = "off";
    c.clearance = "9999999999999999999999999";
    EXPECT_THROW(proposeMerge(s, c), std::exception);
}
TEST(ChartStudioMerge, ExactThresholdAndClearance) {
    auto s = fixture();
    MergeConfig c;
    c.secondaryTrack = 1;
    // Primary sustain ends at 960; next attack at 3840: 2880 ticks = 3/2 whole
    // notes.
    c.minimum = "3/2";
    c.maximum = "3/2";
    auto p = proposeMerge(s, c);
    EXPECT_EQ(p.state.events.size(), 3u);
    c.minimum = "7/4";
    c.maximum = "off";
    EXPECT_EQ(proposeMerge(s, c).state.events.size(), 2u);
    c.minimum = "1/2";
    c.clearance = "1";
    EXPECT_EQ(proposeMerge(s, c).state.events.size(), 2u);
}

TEST(ChartStudioMerge, ManualTimingAndProtectedPlus) {
    Session session;
    session.state = fixture();
    MergeConfig c;
    c.secondaryTrack = 1;
    session.applyMerge(proposeMerge(session.state, c));
    const auto id = session.state.events[1].note.id;
    session.edit(id, 4, 2040, 1000, false);
    const auto updated = proposeMerge(session.state, c).state;
    EXPECT_EQ(updated.events[1].note.id, id);
    EXPECT_EQ(updated.events[1].tick, 2040);
    EXPECT_EQ(updated.events[1].duration, 1000);
    auto state = fixture();
    PlusPhrase phrase;
    phrase.id = 1;
    phrase.begin = 0;
    phrase.end = 4321;
    phrase.manual = true;
    state.plus.phrases = {phrase};
    state.plus.nextId = 2;
    c.regions = {{0, 1200, 2}};
    EXPECT_THROW(proposeMerge(state, c), std::runtime_error);
}

TEST(ChartStudioMerge, IndependentInsertAndDeletionSurviveRegeneration) {
    Session session;
    session.state = fixture();
    MergeConfig c;
    c.secondaryTrack = 1;
    session.applyMerge(proposeMerge(session.state, c));
    session.insert(3500, 16, 0);
    const auto inserted = session.state.events[2].note.id;
    auto proposal = proposeMerge(session.state, c);
    ASSERT_EQ(proposal.state.events.size(), 4u);
    session.applyMerge(proposal);
    session.selection = {inserted};
    session.removeSelected();
    proposal = proposeMerge(session.state, c);
    const auto found = std::find_if(
        proposal.state.events.begin(), proposal.state.events.end(),
        [&](const auto &event) { return event.note.id == inserted; });
    ASSERT_NE(found, proposal.state.events.end());
    EXPECT_TRUE(found->removed);
}

TEST(ChartStudioMerge, DetachKeepsSharedAlignment) {
    Session session;
    session.state = fixture();
    MergeConfig config;
    config.secondaryTrack = 1;
    session.applyMerge(proposeMerge(session.state, config));
    session.setAlignment(.997, 2.4, "new recording");
    session.detachMerge();
    EXPECT_DOUBLE_EQ(session.state.settings.offset, 2.4);
    EXPECT_DOUBLE_EQ(session.state.alignment.scale, .997);
    EXPECT_EQ(session.state.alignment.evidence, "new recording");
    EXPECT_NEAR(session.state.events.front().note.time, 2.4, 1e-12);
}
