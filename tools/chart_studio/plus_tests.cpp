#include "plus_simulation.h"
#include <algorithm>
#include <gtest/gtest.h>

using namespace ChartStudio;
namespace {
State fixture(int bars = 48) {
    State s;
    s.score.ppq = 480;
    for (int bar = 0; bar < bars; ++bar) {
        const int count = bar >= bars * 2 / 3 ? 16 : 4;
        for (int n = 0; n < count; ++n) {
            Event e;
            e.note.id = s.nextId++;
            e.note.flags = 1 << (n % 5);
            e.tick = bar * 1920 + n * 1920 / count;
            s.events.push_back(e);
        }
    }
    synchronize(s);
    return s;
}
} // namespace
TEST(PlusGenerator, DeterministicValidAndPreservesNotes) {
    auto s = fixture();
    const auto before = encode(s);
    const auto p = proposePlus(s, plusProfile("strategic"));
    EXPECT_EQ(encode(s), before);
    Session session;
    session.state = s;
    session.applyPlus(p);
    EXPECT_FALSE(session.state.plus.phrases.empty());
    EXPECT_TRUE(plusProblems(session.state).empty());
    auto again = proposePlus(s, plusProfile("strategic"));
    ASSERT_EQ(p.document.phrases.size(), again.document.phrases.size());
    for (size_t i = 0; i < p.document.phrases.size(); ++i) {
        EXPECT_EQ(p.document.phrases[i].begin, again.document.phrases[i].begin);
        EXPECT_EQ(p.document.phrases[i].end, again.document.phrases[i].end);
    }
    EXPECT_TRUE(session.undo());
    EXPECT_EQ(encode(session.state), before);
    EXPECT_TRUE(session.redo());
    EXPECT_EQ(session.state.plus.phrases.size(), p.document.phrases.size());
}
TEST(PlusGenerator, StaleProposalAndLockedPreservation) {
    Session session;
    session.state = fixture();
    const auto p = proposePlus(session.state, plusProfile("balanced"));
    session.state.settings.offset = 2;
    EXPECT_THROW(session.applyPlus(p), std::exception);
    session.applyPlus(proposePlus(session.state, plusProfile("balanced")));
    auto phrase = session.state.plus.phrases.front();
    phrase.status = 2;
    session.editPlus(phrase);
    auto next = proposePlus(session.state, plusProfile("sparse"));
    session.applyPlus(next);
    EXPECT_TRUE(std::any_of(session.state.plus.phrases.begin(),
                            session.state.plus.phrases.end(), [&](auto q) {
                                return q.id == phrase.id &&
                                       q.begin == phrase.begin &&
                                       q.end == phrase.end && q.status == 2;
                            }));
    phrase.begin += 10;
    EXPECT_THROW(session.editPlus(phrase), std::exception);
}
TEST(PlusGenerator, EmptyAndShortChartsDoNotInventNotes) {
    State empty;
    EXPECT_TRUE(
        proposePlus(empty, plusProfile("strategic")).document.phrases.empty());
    auto s = fixture(1);
    auto p = proposePlus(s, plusProfile("strategic"));
    EXPECT_LE(p.document.phrases.size(), 1u);
}
TEST(PlusGenerator, OffsetDoesNotChangeMusicalDecisions) {
    auto s = fixture();
    auto p = proposePlus(s, plusProfile("strategic"));
    s.settings.offset = 3.75;
    synchronize(s);
    auto q = proposePlus(s, plusProfile("strategic"));
    ASSERT_EQ(p.document.phrases.size(), q.document.phrases.size());
    for (size_t i = 0; i < p.document.phrases.size(); ++i) {
        EXPECT_EQ(p.document.phrases[i].begin, q.document.phrases[i].begin);
    }
}
TEST(PlusGenerator, MetersAndTempoChangesRoundTripState) {
    auto s = fixture();
    s.score.signatures = {{0, 3, 4}, {5760, 5, 4}, {15360, 6, 8}};
    s.score.tempos = {{0, 500000}, {10000, 750000}};
    s.alignment.scale = 1.02;
    synchronize(s);
    Session session;
    session.state = s;
    session.applyPlus(proposePlus(s, plusProfile("strategic")));
    const auto saved = encode(session.state);
    EXPECT_EQ(encode(decode(saved)), saved);
    EXPECT_TRUE(plusProblems(session.state).empty());
}
TEST(PlusSimulation, EconomyAndNoRetroactiveBonus) {
    PlusAnalysis a;
    a.attacks = {{0, 0, 0, 0, 1}, {480, 480, 1, 1, 2}, {4800, 4800, 10, 10, 4}};
    PlusPhrase p;
    p.begin = 0;
    p.end = 960;
    PlusSimulationSettings settings;
    settings.strategy = "immediate";
    const auto r = simulatePlus(a, {p}, settings);
    EXPECT_DOUBLE_EQ(r.completed, 1);
    EXPECT_NEAR(r.activeSeconds, 4, 1e-12);
    EXPECT_DOUBLE_EQ(r.benefit, 0);
    ASSERT_EQ(r.activations.size(), 1u);
    EXPECT_DOUBLE_EQ(r.activations[0].begin, 1);
    EXPECT_DOUBLE_EQ(r.activations[0].end, 5);
}
TEST(PlusSimulation, CompletionRequiresEveryAttack) {
    auto s = fixture(4);
    auto a = analyzePlus(s);
    PlusPhrase p;
    p.begin = 0;
    p.end = 1920;
    PlusSimulationSettings settings;
    settings.strategy = "never";
    settings.success = .5;
    settings.trials = 4096;
    const auto full = simulatePlus(a, {p}, settings);
    EXPECT_NEAR(full.completed, .0625, .025);
    EXPECT_LT(full.completed, .1);
    EXPECT_DOUBLE_EQ(full.completed, simulatePlus(a, {p}, settings).completed);
}
TEST(PlusSimulation, SaturationAndInitialEnergy) {
    auto a = analyzePlus(fixture());
    PlusPhrase p;
    p.begin = 0;
    p.end = 1920;
    PlusSimulationSettings settings;
    settings.strategy = "never";
    settings.initialEnergy = 1;
    const auto r = simulatePlus(a, {p}, settings);
    EXPECT_NEAR(r.wasted, .2, 1e-12);
    EXPECT_DOUBLE_EQ(r.finalEnergy, 1);
    EXPECT_DOUBLE_EQ(r.activeSeconds, 0);
}

TEST(PlusGenerator, PreparationBeforeLateClimaxAndNoPhrasesAcrossSilence) {
    auto state = fixture(64);
    state.events.erase(std::remove_if(state.events.begin(), state.events.end(),
                                      [](auto e) {
                                          return e.tick >= 1920 * 16 &&
                                                 e.tick < 1920 * 32;
                                      }),
                       state.events.end());
    const auto proposal = proposePlus(state, plusProfile("strategic"));
    const auto simulation =
        simulatePlus(proposal.analysis, proposal.document.phrases, {});
    size_t lateTargets = 0;
    for (size_t i = 0; i < proposal.analysis.regions.size(); ++i) {
        if (proposal.analysis.regions[i].begin >= 1920 * 44) {
            ++lateTargets;
            EXPECT_GE(simulation.regionEnergy[i] + 1e-9, .4);
        }
    }
    EXPECT_GT(lateTargets, 0u);
    for (const auto &phrase : proposal.document.phrases) {
        EXPECT_FALSE(phrase.begin < 1920 * 16 && phrase.end > 1920 * 32);
        EXPECT_FALSE(phrase.begin >= 1920 * 16 && phrase.end <= 1920 * 32);
    }
}
TEST(PlusGenerator, RegionalRegenerationPreservesOutsideAndManualPhrases) {
    Session session;
    session.state = fixture();
    session.applyPlus(proposePlus(session.state, plusProfile("strategic")));
    const auto before = session.state.plus.phrases;
    const int64_t begin = 1920 * 24, end = 1920 * 40;
    session.applyPlus(
        proposePlus(session.state, plusProfile("sparse"), {{begin, end}}));
    for (const auto &p : before) {
        if (p.begin < begin || p.end > end) {
            EXPECT_TRUE(std::any_of(
                session.state.plus.phrases.begin(),
                session.state.plus.phrases.end(), [&](auto q) {
                    return q.id == p.id && q.begin == p.begin && q.end == p.end;
                }));
        }
    }
}
TEST(PlusGenerator, ExhaustiveTwoOpportunityOracleOnSmallFixture) {
    auto state = fixture(12);
    const auto proposal = proposePlus(state, plusProfile("strategic"));
    bool feasible = false;
    const auto &analysis = proposal.analysis;
    // Enumerate every disjoint one-measure pair with a separator measure.
    for (size_t i = 0; i + 1 < analysis.bars.size(); ++i) {
        for (size_t j = i + 2; j + 1 < analysis.bars.size(); ++j) {
            PlusPhrase first, second;
            first.begin = analysis.bars[i];
            first.end = analysis.bars[i + 1];
            second.begin = analysis.bars[j];
            second.end = analysis.bars[j + 1];
            auto sim = simulatePlus(analysis, {first, second}, {});
            for (double energy : sim.regionEnergy) {
                feasible |= energy + 1e-9 >= .4;
            }
        }
    }
    EXPECT_TRUE(feasible);
    const auto generated =
        simulatePlus(analysis, proposal.document.phrases, {});
    EXPECT_TRUE(std::any_of(generated.regionEnergy.begin(),
                            generated.regionEnergy.end(),
                            [](double energy) { return energy + 1e-9 >= .4; }));
}
TEST(PlusSimulation, TwoRewardsAndReplenishmentWhileActive) {
    PlusAnalysis a;
    a.attacks = {
        {0, 0, 0, 0, 1}, {1000, 1000, 2, 2, 2}, {10000, 10000, 20, 20, 4}};
    PlusPhrase first, second;
    first.begin = 0;
    first.end = 500;
    second.begin = 1000;
    second.end = 1500;
    PlusSimulationSettings settings;
    settings.strategy = "immediate";
    const auto r = simulatePlus(a, {first, second}, settings);
    EXPECT_NEAR(r.activeSeconds, 8, 1e-12);
    EXPECT_DOUBLE_EQ(r.completed, 2);
    EXPECT_DOUBLE_EQ(r.benefit, 50);
    EXPECT_EQ(r.activations.size(), 1u);
}
TEST(PlusGenerator, CancellationDoesNotMutateDocument) {
    auto state = fixture();
    const auto before = encode(state);
    EXPECT_THROW(proposePlus(state, plusProfile("strategic"), {},
                             [] { throw std::runtime_error("cancel"); }),
                 std::exception);
    EXPECT_EQ(encode(state), before);
}

TEST(PlusEditing, MoveResizeDeleteUndoAndStableIds) {
    Session session;
    session.state = fixture();
    PlusPhrase p;
    p.begin = 0;
    p.end = 1920;
    session.editPlus(p);
    ASSERT_EQ(session.state.plus.phrases.size(), 1u);
    p = session.state.plus.phrases.front();
    const auto id = p.id;
    p.begin = 480;
    p.end = 1440;
    session.editPlus(p);
    EXPECT_EQ(session.state.plus.phrases.front().id, id);
    EXPECT_EQ(session.state.plus.phrases.front().end, 1440);
    EXPECT_TRUE(session.undo());
    EXPECT_EQ(session.state.plus.phrases.front().begin, 0);
    EXPECT_TRUE(session.redo());
    session.editPlus(p, true);
    EXPECT_TRUE(session.state.plus.phrases.empty());
    EXPECT_THROW(session.editPlus(p), std::exception);
    EXPECT_TRUE(session.undo());
    EXPECT_EQ(session.state.plus.phrases.front().id, id);
    EXPECT_TRUE(session.state.plus.phrases.front().manual);
}

TEST(PlusSimulation, ReferenceReportUsesActualLoaderMembership) {
    auto analysis = analyzePlus(fixture(2));
    PlusDocument document;
    PlusPhrase phrase;
    phrase.id = document.nextId++;
    phrase.begin = 0;
    phrase.end = 1920;
    // The loader may consume the first interval attack while advancing.
    phrase.runtimeFirst = 1;
    phrase.runtimeLast = 3;
    document.phrases.push_back(phrase);
    const auto json = plusReport(analysis, document, 1, 42);
    EXPECT_NE(json.find("\"attacks\":3,"), std::string::npos);
    EXPECT_NE(
        json.find("\"maximum\":1,\"reward\":0.2,\"drain_per_second\":0.05"),
        std::string::npos);
}
