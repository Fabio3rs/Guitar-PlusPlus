#include "core.h"
#include "plus_generator.h"
#include <algorithm>
#include <gtest/gtest.h>

using namespace ChartStudio;
namespace {
State musicalParts() {
    State state;
    state.score.tracks.resize(3);
    state.score.tempos = {{0, 500000}, {1920, 400000}};
    state.score.signatures = {{0, 3, 4}, {5760, 5, 4}};
    uint64_t id = 1;
    for (int track = 0; track < 3; ++track) {
        for (int i = 0; i < 80; ++i) {
            ScoreNote note;
            note.id = id++;
            note.pitch = 40 + track * 12 + i % 5;
            note.velocity = 100;
            note.channel = i % 2; // Both tracks also reuse these channels.
            note.program = track == 1 ? 34 : 30;
            note.start = int64_t(i) * 240;
            note.end = note.start + 360;
            state.score.tracks[track].notes.push_back(note);
        }
    }
    state.settings.track = 0;
    return state;
}
Session twoParts() {
    Session session;
    session.state = musicalParts();
    session.addPart(parsePart("ExpertDoubleBass:1"));
    return session;
}
} // namespace

TEST(ChartStudioParts, SectionNamesAndStrictAssociations) {
    for (const auto *section :
         {"ExpertSingle", "HardDoubleBass", "MediumDoubleRhythm",
          "EasyDoubleGuitar", "ExpertKeyboard"}) {
        const auto settings = parsePart(std::string(section) + ":1:0:30");
        EXPECT_EQ(partSection(settings), "[" + std::string(section) + "]");
        EXPECT_EQ(settings.track, 1);
        EXPECT_EQ(settings.channel, 0);
        EXPECT_EQ(settings.program, 30);
    }
    for (const auto *bad :
         {"ExpertDrums:1", "ExpertSingle", "ExpertSingle:-1",
          "ExpertSingle:1:", "ExpertSingle:1:16", "ExpertSingle:1:0:128",
          "ExpertSingle:1x", "ExpertSingle:1:0:1:2"}) {
        EXPECT_THROW(parsePart(bad), std::exception) << bad;
    }
}

TEST(ChartStudioParts, DeterministicBatchIsolationAndUndo) {
    auto session = twoParts();
    const auto before = encode(session.state);
    session.runAll();
    EXPECT_EQ(session.state.events.size(), 80u);
    EXPECT_EQ(session.state.otherParts.front().events.size(), 80u);
    const auto generated = chartText(session.state);
    EXPECT_TRUE(session.undo());
    EXPECT_EQ(encode(session.state), before);
    EXPECT_TRUE(session.redo());
    EXPECT_EQ(chartText(session.state), generated);
    const auto bass = session.state.partId;
    session.edit(session.state.events.front().note.id, 17, 0, 360, false);
    session.pattern(session.state.events.front().note.id, 2, false);
    session.selectPart(findPart(session.state, "ExpertSingle"));
    const auto guitar = partState(session.state, session.state.partId);
    EXPECT_NE(guitar.events.front().note.flags, 17);
    session.runAll();
    EXPECT_EQ(partState(session.state, bass).events.front().note.flags, 17);
    const auto stable = chartText(session.state);
    session.selectPart(bass);
    session.runAll();
    EXPECT_EQ(chartText(session.state), stable);
    const auto active = partState(session.state, bass);
    EXPECT_EQ(active.events.front().sources.front().id, 81u);
    EXPECT_EQ(guitar.events.front().sources.front().id, 1u);
}

TEST(ChartStudioParts, MusicalMemoryIsScopedAndFiltersPreserveSourceIdentity) {
    auto session = twoParts();
    auto settings = parsePart("ExpertKeyboard:0:1:30");
    session.addPart(settings);
    session.runAll();
    EXPECT_EQ(session.state.events.size(), 40u);
    for (const auto &event : session.state.events) {
        EXPECT_EQ(event.sources.front().channel, 1);
    }
    const auto guitar =
        partState(session.state, findPart(session.state, "ExpertSingle"));
    EXPECT_EQ(guitar.events.size(), 80u);
    auto same = guitar.settings;
    const auto lead = canonical(guitar.events, 0, 5, guitar.score, same, false);
    EXPECT_EQ(lead.find("signature-v1/guitar/"), 0u);
    same.instrument = "bass";
    EXPECT_NE(canonical(guitar.events, 0, 5, guitar.score, same, false), lead);
    EXPECT_EQ(session.state.events.front().exact.find("signature-v1/keys/"),
              0u);
}

TEST(ChartStudioParts, FailureCancellationAndStaleResultsDoNotApply) {
    auto session = twoParts();
    auto invalid = parsePart("ExpertKeyboard:99");
    session.addPart(invalid);
    const auto before = encode(session.state);
    EXPECT_THROW(session.runAll(), std::exception);
    EXPECT_EQ(encode(session.state), before);
    session.removePart(session.state.partId);
    const auto source = hash(encode(session.state));
    EXPECT_THROW(generateParts(session.state, {}, {},
                               [](size_t done, size_t, const std::string &) {
                                   if (done == 1) {
                                       throw std::runtime_error("cancel");
                                   }
                               }),
                 std::exception);
    EXPECT_EQ(hash(encode(session.state)), source);
    auto generated = generateParts(session.state);
    session.insert(0, 1, 0);
    EXPECT_THROW(session.applyGenerated(std::move(generated), source),
                 std::exception);
    EXPECT_EQ(session.state.events.size(), 1u);
}

TEST(ChartStudioParts, SharedTimingAndPersistenceRetainEveryPart) {
    auto session = twoParts();
    session.runAll();
    session.setAlignment(1.01, -.2, "test");
    const auto restored = decode(encode(session.state));
    EXPECT_EQ(chartText(restored), chartText(session.state));
    EXPECT_EQ(restored.partId, session.state.partId);
    for (const auto *part : parts(restored)) {
        EXPECT_EQ(part->settings.offset, -.2);
        for (const auto &event : part->events) {
            EXPECT_DOUBLE_EQ(event.note.time,
                             1.01 * restored.score.seconds(event.tick) - .2);
        }
    }
    session.selectPart(session.state.otherParts.front().partId);
    EXPECT_EQ(chartText(session.state), chartText(restored));
}

TEST(ChartStudioParts, LegacyV3PayloadPreservesEditsPlusAndAlignment) {
    auto state = musicalParts();
    generate(state);
    state.events.front().manual = true;
    state.events.front().status = 2;
    state.events.front().note.flags = 17;
    state.plus.settings = plusProfile("sparse");
    PlusPhrase phrase;
    phrase.id = state.plus.nextId++;
    phrase.begin = 0;
    phrase.end = 600;
    phrase.status = 2;
    state.plus.phrases.push_back(phrase);
    state.alignment.scale = 1.01;
    state.settings.offset = .23;
    synchronize(state);
    std::ostringstream legacy;
    {
        cereal::JSONOutputArchive archive(legacy);
        archive(state);
        archive(cereal::make_nvp("alignment", state.alignment),
                cereal::make_nvp("plus", state.plus),
                cereal::make_nvp("musical_markers", state.score.markers));
    }
    const auto restored = decode(legacy.str());
    EXPECT_TRUE(restored.otherParts.empty());
    EXPECT_EQ(partSection(restored.settings), "[ExpertSingle]");
    EXPECT_EQ(encode(restored), encode(state));
}

TEST(ChartStudioParts, OptionalProjectionsBelongToTheirOwnPart) {
    auto session = twoParts();
    session.runAll();
    std::vector<uint64_t> ids;
    for (const auto *part : parts(session.state)) {
        ids.push_back(part->partId);
        Projection projection;
        projection.identity = std::to_string(part->partId);
        projection.difficulty = part->settings.difficulty;
        projection.track = part->settings.track;
        projection.channel = part->settings.channel;
        projection.program = part->settings.program;
        for (const auto &event : part->events) {
            const auto count =
                std::count_if(part->events.begin(), part->events.end(),
                              [&](const Event &other) {
                                  return other.phrase == event.phrase;
                              });
            projection.positions[event.structural] =
                std::vector<int>(size_t(count), 24);
        }
        session.projections[part->partId] = projection;
    }
    for (const auto id : ids) {
        session.selectPart(id);
        session.state.settings.algorithm = "umap:" + std::to_string(id);
    }
    ASSERT_NO_THROW(session.runAll());
    const auto before = encode(session.state);
    auto wrong = session.partProjections();
    wrong[ids[0]] = wrong[ids[1]];
    EXPECT_THROW(generateParts(session.state, {}, wrong), std::exception);
    EXPECT_EQ(encode(session.state), before);
}

TEST(ChartStudioParts, ReconfigureAndRemoveAreExplicitAndUndoable) {
    auto session = twoParts();
    session.runAll();
    const auto before = encode(session.state);
    EXPECT_THROW(session.addPart(parsePart("ExpertDoubleBass:2")),
                 std::exception);
    auto settings = session.state.settings;
    settings.track = 2;
    EXPECT_THROW(session.configurePart(settings), std::exception);
    EXPECT_EQ(encode(session.state), before);
    session.configurePart(settings, true);
    EXPECT_TRUE(session.state.events.empty());
    EXPECT_FALSE(session.state.otherParts.front().events.empty());
    session.undo();
    EXPECT_EQ(encode(session.state), before);
    session.removePart(session.state.partId);
    EXPECT_TRUE(session.state.otherParts.empty());
    session.undo();
    EXPECT_EQ(encode(session.state), before);
}

TEST(ChartStudioParts, PlusIsIndependentAndLockedPhrasesSurviveBatch) {
    auto session = twoParts();
    session.state.plus.settings = plusProfile("strategic");
    session.runAll();
    ASSERT_FALSE(session.state.plus.phrases.empty());
    EXPECT_TRUE(session.state.otherParts.front().plus.phrases.empty());
    session.state.plus.phrases.front().status = 2;
    const auto locked = session.state.plus.phrases.front();
    session.runAll();
    const auto found = std::find_if(
        session.state.plus.phrases.begin(), session.state.plus.phrases.end(),
        [&](const auto &p) { return p.id == locked.id; });
    ASSERT_NE(found, session.state.plus.phrases.end());
    EXPECT_EQ(found->begin, locked.begin);
    EXPECT_EQ(found->end, locked.end);
    EXPECT_EQ(found->status, 2);
    EXPECT_TRUE(session.state.otherParts.front().plus.phrases.empty());
}
