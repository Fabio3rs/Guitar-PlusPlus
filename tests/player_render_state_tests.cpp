#include "CGamePlay.h"
#include "GPPGame.h"
#include <gtest/gtest.h>

TEST(PlayerRenderState, CachesArePerPlayerAndRetainTailCapacityAcrossReset) {
    CPlayer first("first"), second("second");
    first.renderState.tailsData.resize(96);
    const auto capacity = first.renderState.tailsData.capacity();
    first.renderState.BPMNowBuffer = 7;
    first.BPMNowUpdate = 3;
    first.renderState.plusCircleBuffer = 123;
    EXPECT_EQ(second.renderState.BPMNowBuffer, 0u);
    EXPECT_EQ(second.renderState.plusCircleBuffer, ~0U);
    first.resetSongThings();
    EXPECT_EQ(first.BPMNowUpdate, 0u);
    EXPECT_EQ(first.renderState.BPMNowBuffer, 0u);
    EXPECT_EQ(first.renderState.plusCircleBuffer, ~0U);
    EXPECT_EQ(first.renderState.tailsData.capacity(), capacity);
}

TEST(PlayerRenderState, ChartEndReadDoesNotPopulateAuthoritativeCache) {
    CChart::NotesData notes;
    CChart::Note note;
    note.time = 5;
    note.lTime = 2;
    notes.gNotes.push_back(note);
    const auto &view = notes;
    EXPECT_DOUBLE_EQ(view.getChartEnd(1), 8);
    EXPECT_DOUBLE_EQ(notes.chartEnd, 0);
    EXPECT_DOUBLE_EQ(notes.getChartEnd(1), 8);
    EXPECT_DOUBLE_EQ(notes.chartEnd, 8);
    EXPECT_DOUBLE_EQ(view.getChartEnd(10), 8);
}

TEST(PlayerRenderState,
     UpdateExpiresPlusWithoutRenderingOrChangingVisualBpmCache) {
    CGamePlay module;
    auto player = std::make_shared<CPlayer>("expiry");
    player->enableBot = true;
    player->plusPower = 0;
    CChart::Note bpm;
    bpm.lTime = 120;
    player->Notes.BPM.push_back(bpm);
    module.players.push_back(player);
    module.startSongTime = std::chrono::steady_clock::now();
    player->renderState.BPMNowBuffer = 9;
    player->bPlusStrike = true;
    player->plusThunterStrikeStart = CEngine::engine().getTime() - 3;
    module.update();
    EXPECT_TRUE(player->bPlusStrike);
    player->plusThunterStrikeStart = CEngine::engine().getTime() - 5;
    module.update();
    EXPECT_FALSE(player->bPlusStrike);
    EXPECT_EQ(player->renderState.BPMNowBuffer, 9u);
    EXPECT_EQ(player->BPMNowUpdate, 0u);
    // Pause compensation preserves the wall-clock expiry used by the effect.
    const double strike = player->plusThunterStrikeStart;
    module.compensatePause(std::chrono::seconds(2));
    EXPECT_DOUBLE_EQ(player->plusThunterStrikeStart, strike);
}
