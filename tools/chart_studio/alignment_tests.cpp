#include "alignment.h"
#include <algorithm>
#include <gtest/gtest.h>
using namespace ChartStudio;
namespace {
Score fixture() {
    Score score;
    score.tracks.resize(2);
    uint32_t rng = 197;
    for (int i = 0; i < 140; ++i) {
        rng = rng * 1664525u + 1013904223u;
        ScoreNote n;
        n.id = uint64_t(i + 1);
        n.pitch = 48 + int((rng >> 8) % 24);
        n.velocity = 100;
        n.program = 27;
        n.start = 480 + i * 480;
        n.end = n.start + 360;
        score.tracks[0].notes.push_back(n);
        n.id += 1000;
        n.channel = 9;
        n.pitch = 35;
        score.tracks[1].notes.push_back(n);
    }
    score.tempos = {{0, 500000}, {24000, 550000}};
    return score;
}
ChromaSequence warped(const ChromaSequence &m, double scale, double offset) {
    ChromaSequence a;
    for (double time = 0;
         time < m.back().time * scale + std::max(0., offset) + 1; time += .05) {
        ChromaFrame frame;
        frame.time = time;
        const auto index = std::llround((time - offset) / scale / .05);
        if (index >= 0 && size_t(index) < m.size()) {
            frame.chroma = m[size_t(index)].chroma;
        }
        a.push_back(frame);
    }
    return a;
}
TEST(Alignment, OffsetAndLinearHaveDeterministicCandidatesAndLocalEvidence) {
    auto m = scoreChroma(fixture());
    AlignmentSettings settings;
    settings.minimumOffset = -3;
    settings.maximumOffset = 4;
    settings.mode = "offset";
    auto r = alignChroma(m, warped(m, 1, 2.3), settings);
    ASSERT_FALSE(r.candidates.empty());
    EXPECT_NEAR(r.candidates[0].offset, 2.3, .055);
    EXPECT_EQ(r.candidates[0].scale, 1);
    EXPECT_TRUE(r.reliable);
    EXPECT_EQ(r.windows.size(), 6u);
    settings.mode = "auto";
    auto a = warped(m, 1.018, -1.2);
    r = alignChroma(m, a, settings);
    EXPECT_NEAR(r.candidates[0].scale, 1.018, .0015);
    EXPECT_NEAR(r.candidates[0].offset, -1.2, .06);
    EXPECT_TRUE(r.reliable);
    EXPECT_GT(r.suggestedSilence, .6);
    EXPECT_EQ(alignmentJson(r), alignmentJson(alignChroma(m, a, settings)));
}
TEST(Alignment, SilenceConstantHarmonyAndInvalidInputCannotAutoApply) {
    auto m = scoreChroma(fixture());
    auto silent = warped(m, 1, 0);
    for (auto &f : silent) {
        f.chroma = {};
    }
    AlignmentSettings s;
    s.mode = "offset";
    s.minimumOffset = -2;
    s.maximumOffset = 2;
    EXPECT_FALSE(alignChroma(m, silent, s).reliable);
    for (auto &f : m) {
        f.chroma = {};
        f.chroma[0] = 1;
    }
    EXPECT_FALSE(alignChroma(m, warped(m, 1, 0), s).reliable);
    s.mode = "dtw";
    EXPECT_THROW(alignChroma(m, silent, s), std::exception);
    EXPECT_THROW(scoreChroma(fixture(), 0), std::exception);
    s.mode = "offset";
    silent[1].time = silent[0].time;
    EXPECT_THROW(alignChroma(m, silent, s), std::exception);
}
TEST(Alignment, TimingIsUndoablePersistentAndIndependentFromMapping) {
    Session session;
    session.state.score = fixture();
    session.state.settings.track = 0;
    session.run();
    const auto original = session.state;
    const auto id = original.events.front().note.id;
    session.pattern(id, 2, true);
    const auto locked = session.state;
    session.setAlignment(1.02, 2.5, "test evidence");
    EXPECT_EQ(session.state.score.tempos[1].microseconds, 550000);
    EXPECT_NEAR(session.state.events.front().note.time,
                original.events.front().note.time * 1.02 + 2.5, 1e-9);
    EXPECT_NEAR(session.state.events.front().note.duration,
                original.events.front().note.duration * 1.02, 1e-9);
    const auto reopened = decode(encode(session.state));
    EXPECT_EQ(reopened.alignment.scale, 1.02);
    EXPECT_EQ(reopened.alignment.evidence, "test evidence");
    EXPECT_EQ(audioTick(reopened, audioSeconds(reopened, 25000)), 25000);
    session.run();
    for (size_t i = 0; i < locked.events.size(); ++i) {
        EXPECT_EQ(session.state.events[i].note.flags,
                  locked.events[i].note.flags);
    }
    session.undo();
    session.undo();
    EXPECT_EQ(session.state.alignment.scale, 1);
    session.redo();
    EXPECT_EQ(session.state.alignment.scale, 1.02);
    EXPECT_THROW(session.setAlignment(0, 0, ""), std::exception);
    // Legacy project payloads have no top-level alignment member.
    std::ostringstream old;
    {
        cereal::JSONOutputArchive archive(old);
        archive(original);
    }
    EXPECT_EQ(decode(old.str()).alignment.scale, 1);
    auto drums = fixture();
    drums.tracks.erase(drums.tracks.begin());
    EXPECT_THROW(scoreChroma(drums), std::exception);
}
} // namespace
