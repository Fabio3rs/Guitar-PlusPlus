#include "CGamePlay.h"
#include "CBackgroundRenderer.h"
#include "CEngine.h"
#include "CFonts.h"
#include "GPPGame.h"
#include "GameplayProfile.h"
#include "PlusRules.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <future>
#include <ratio>

static auto gpppowi(int b, int e) noexcept -> int {
    assert(e >= 0);
    return static_cast<int>(std::pow(b, e));
}

constexpr static double NOTE_SIZE = 0.2;
// Trigger.obj has a local top at y=0.043614; leave a small surface clearance.
constexpr static double STRIKE_FIRE_SURFACE = 0.046;

const int CGamePlay::notesFlagsConst[5] = {
    gpppowi(2, 0), gpppowi(2, 1), gpppowi(2, 2), gpppowi(2, 3), gpppowi(2, 4)};

void CGamePlay::drawBPMLine(double position, unsigned int Texture,
                            const CPlayer &Player) {
    CEngine::RenderDoubleStruct TempStruct3D{};
    const double pos = -0.492;
    double rtime = getRunningMusicTime(Player) - position;

    double nCalc = rtime * speedMp;

    nCalc += 0.55;

    const double size = 0.25;

    TempStruct3D.Text = Texture;

    TempStruct3D.x1 = pos;
    TempStruct3D.x2 = pos * -1.0;
    TempStruct3D.x3 = TempStruct3D.x2;
    TempStruct3D.x4 = TempStruct3D.x1;

    TempStruct3D.y1 = -0.4999;
    TempStruct3D.y2 = TempStruct3D.y1;
    TempStruct3D.y3 = -0.4999;
    TempStruct3D.y4 = TempStruct3D.y3;

    TempStruct3D.z1 = nCalc;
    TempStruct3D.z2 = TempStruct3D.z1;
    TempStruct3D.z3 = nCalc + size;
    TempStruct3D.z4 = TempStruct3D.z3;

    TempStruct3D.TextureX1 = 0.0F;
    TempStruct3D.TextureX2 = 1.0F;
    TempStruct3D.TextureY1 = 1.0F;
    TempStruct3D.TextureY2 = 0.0F;

    double alpha = pos2Alpha(-TempStruct3D.z1 / 6.5);

    if (alpha <= 0.0) {
        return;
    }

    CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);
    CEngine::engine().Render3DQuad(TempStruct3D);
}

auto CGamePlay::fretboardPositionCalcByT(double time, double prop,
                                         double *max) const -> double {
    const double size = 2.1 * 1.33333333;

    if (max != nullptr) {
        {
            *max = size * prop;
        }
    }

    double cCalc = -time * 5.0;
    double propSpeeed = 5.0 / speedMp;

    cCalc /= propSpeeed;

    cCalc *= 1000000.0;
    cCalc = static_cast<double>((int64_t)cCalc %
                                (int64_t)((size * 1000000) * prop));
    return cCalc / 1000000.0;
}

auto CGamePlay::getBPMAt(const CPlayer &player, double time) -> double {
    double result = 120.0;

    for (const auto &BPMn : player.Notes.BPM) {
        if (BPMn.time <= time) {
            result = BPMn.lTime;
        } else {
            break;
        }
    }

    return result;
}

auto CGamePlay::getBPMAtStruct(const CPlayer &player, double time)
    -> CPlayer::NotesData::Note {
    CPlayer::NotesData::Note result;

    for (const auto &BPMn : player.Notes.BPM) {
        if (BPMn.time <= time) {
            result = BPMn;
        } else {
            break;
        }
    }

    return result;
}

auto CGamePlay::getBPMAtIt(CPlayer &player, double time)
    -> std::vector<CPlayer::NotesData::Note>::iterator {
    std::vector<CPlayer::NotesData::Note>::iterator result;

    for (auto it = player.Notes.BPM.begin(); it != player.Notes.BPM.end();
         it++) {
        if ((*it).time < time) {
            result = it;
        } else {
            break;
        }
    }

    return result;
}

void CGamePlay::alertLVLUP(const CPlayer &player) {
    CEngine &engine = CEngine::engine();

    double delta = CEngine::getTime() - player.lvlUpAlert.startTime;

    double y = 0.6;

    if (delta <= 0.2) {
        double scale = delta / 0.2;

        CEngine::setScale(scale, scale, 1.0);
    } else if (delta >= 0.2 && delta <= 1.5) {

    } else if (delta >= 1.5 && delta <= 2.5) {
        y += (delta - 1.5) / 1.0;
    }

    std::string lvlUPMsg = "Level UP! ";
    lvlUPMsg += std::to_string(player.getLevel());

    CFonts::fonts().drawTextInScreen(
        lvlUPMsg, CFonts::fonts().getCenterPos(lvlUPMsg, 0.1, 0.0), y, 0.1);
    engine.matrixReset();
}

void CGamePlay::alertNotesStreak(const CPlayer &player) {
    CEngine &engine = CEngine::engine();

    double delta = CEngine::getTime() - player.notesStreakAlert.startTime;

    double y = 0.6;

    if (delta <= 0.2) {
        double scale = delta / 0.2;

        CEngine::setScale(scale, scale, 1.0);
    } else if (delta >= 0.2 && delta <= 1.5) {

    } else if (delta >= 1.5 && delta <= 2.5) {
        y += (delta - 1.5) / 1.0;
    }

    std::string notesStreakMsg = std::to_string(player.notesStreakAlert.status);
    notesStreakMsg += " Notes Streak";

    CFonts::fonts().drawTextInScreen(
        notesStreakMsg, CFonts::fonts().getCenterPos(notesStreakMsg, 0.1, 0.0),
        y, 0.1);
    engine.matrixReset();
}

auto CGamePlay::alertTest(CFonts::textAlert &t) -> bool {
    CEngine &engine = CEngine::engine();

    double delta = CEngine::getTime() - t.startTime;

    double y = 0.7;

    if (delta <= 0.2) {
        double scale = delta / 0.2;

        CEngine::setScale(scale, scale, 1.0);
    } else if (delta >= 0.2 && delta <= 1.5) {

    } else if (delta >= 1.5 && delta <= 2.5) {
        y += (delta - 1.5) / 1.0;
    }

    CFonts::fonts().drawTextInScreen(
        t.msg, CFonts::fonts().getCenterPos(t.msg, 0.1, 0.0), y, 0.1);
    engine.matrixReset();

    return delta <= 2.3;
}

auto CGamePlay::getBPMAtI(const CPlayer &player, double time) -> size_t {
    size_t resultI = 0;

    for (size_t i = 0, size = player.Notes.BPM.size(); i < size; i++) {
        const auto &BPMn = player.Notes.BPM[i];

        if (BPMn.time <= time) {
        } else {
            break;
        }

        resultI = i;
    }

    return resultI;
}

CEngine::dTriangleWithAlpha *
CGamePlay::drawBPMLines(const CPlayer &Player, PlayerRenderState &renderState) {
    static std::vector<CPlayer::NotesData::Note> BPMValuesdata;
    BPMl.clear();
    BPMValuesdata.clear();
    BPMl.useColors = true;
    BPMl.autoEnDisaColors = false;
    BPMl.texture = BPMTextID;
    double mscRunnTime = getRunningMusicTime(Player);
    const double time = mscRunnTime - 0.5;

    auto calcQuad = [&](double position, double runTime,
                        const CPlayer &Player) {
        static CEngine::RenderDoubleStruct TempStruct3D;
        double rtime = runTime - position;

        double nCalc = rtime * speedMp;

        nCalc += 0.75;

        double alpha = pos2Alpha(-nCalc / 5.8);

        if (alpha <= 0.0) {
            return false;
        }

        const double pos = -0.492;

        const double size = 0.05;

        static staticCallFunc rendr([&]() {
            TempStruct3D.x1 = pos;
            TempStruct3D.x2 = pos * -1.0;
            TempStruct3D.x3 = TempStruct3D.x2;
            TempStruct3D.x4 = TempStruct3D.x1;

            TempStruct3D.y1 = -0.4995;
            TempStruct3D.y2 = TempStruct3D.y1;
            TempStruct3D.y3 = -0.4995;
            TempStruct3D.y4 = TempStruct3D.y3;

            TempStruct3D.TextureX1 = 0.0F;
            TempStruct3D.TextureX2 = 1.0F;
            TempStruct3D.TextureY1 = 1.0F;
            TempStruct3D.TextureY2 = 0.0F;
        });

        TempStruct3D.z1 = nCalc;
        TempStruct3D.z2 = TempStruct3D.z1;
        TempStruct3D.z3 = nCalc + size;
        TempStruct3D.z4 = TempStruct3D.z3;

        TempStruct3D.alphaBottom = alpha;
        TempStruct3D.alphaTop = alpha;

        CEngine::pushQuad(BPMl, TempStruct3D);

        return true;
    };

    double BBPM = 0.0;
    double minTime = 0;

    const auto bpmBuffSize = Player.Notes.BPM.size();

    if (bpmBuffSize > 0) {
        const double mtime = (time - 1.0);
        size_t BPMnowbuff = renderState.BPMNowBuffer;

        if (BPMnowbuff >= bpmBuffSize) {
            BPMnowbuff = bpmBuffSize - 1;
        }

        if (mtime >= 0.0) {
            size_t nbuff = BPMnowbuff + 1;
            if (bpmBuffSize > nbuff) {
                if (((mscRunnTime - 1.5) > (Player.Notes.BPM[nbuff].time))) {
                    BPMnowbuff = nbuff;
                    renderState.BPMNowBuffer = BPMnowbuff;
                }
            }
        } else {
            BPMnowbuff = 0;
        }

        double BPS = 60.0 / Player.Notes.BPM[BPMnowbuff].lTime;

        double blinetime = mtime - Player.Notes.BPM[BPMnowbuff].time;

        if (blinetime >= 0.0) {
            blinetime /= BPS;
        } else {
            blinetime = 0.0;
        }

        double tCalc = 0.0;
        double timeToSum = Player.Notes.BPM[BPMnowbuff].time;

        auto bpmMultiplier = static_cast<int64_t>(floor(blinetime));

        double chartEnd = Player.Notes.getChartEnd(1.0);

        for (size_t i = 0; tCalc < 9.0; i++, bpmMultiplier++) {
            blinetime = timeToSum + BPS * bpmMultiplier;

            if (blinetime > chartEnd) {
                break;
            }

            {
                size_t nbuff = BPMnowbuff + 1;
                if (bpmBuffSize > nbuff) {
                    if (blinetime >= Player.Notes.BPM[nbuff].time) {
                        BPMnowbuff = nbuff;

                        bpmMultiplier = 0;
                        const auto &BPMNow = Player.Notes.BPM[BPMnowbuff];

                        blinetime = timeToSum = BPMNow.time;
                        BPS = 60.0 / BPMNow.lTime;
                    }
                }
            }

            if (!calcQuad(blinetime, mscRunnTime, Player)) {
                break;
            }

            tCalc += BPS;
        }
    }

    return &BPMl;
}

void CGamePlay::renderIndivdualFlame(int id, double pos, unsigned int Texture,
                                     int state, double sizeproportion,
                                     const CPlayer &player) {
    CEngine::RenderDoubleStruct TempStruct3D{};

    double nCalc = pos;

    const double size = 0.25;
    const double position = -0.56;

    TempStruct3D.Text = Texture;

    TempStruct3D.TextureX1 = 1.0 / 4.0 * (double)state;
    TempStruct3D.TextureX2 = 1.0 / 4.0 * (double)state + 1.0 / 4.0;
    TempStruct3D.TextureY1 = 1.0;
    TempStruct3D.TextureY2 = 0.0;

    TempStruct3D.x1 = position + (double(id) * size / 1.15) +
                      (size - size * sizeproportion) / 2.0;
    TempStruct3D.x2 = TempStruct3D.x1 + size * sizeproportion;
    TempStruct3D.x3 = TempStruct3D.x1 + size * sizeproportion;
    TempStruct3D.x4 = TempStruct3D.x1;

    TempStruct3D.y1 = -0.5 + size * sizeproportion;
    TempStruct3D.y2 = TempStruct3D.y1;
    TempStruct3D.y3 = -0.5;
    TempStruct3D.y4 = TempStruct3D.y3;

    TempStruct3D.z1 = nCalc + size * 4.0;
    TempStruct3D.z2 = TempStruct3D.z1;
    TempStruct3D.z3 = TempStruct3D.z1;
    TempStruct3D.z4 = TempStruct3D.z3;

    TempStruct3D.alphaBottom = 1.0;
    TempStruct3D.alphaTop = 0.1;

    CEngine::engine().Render3DQuadWithAlpha(TempStruct3D);
}

void CGamePlay::renderIndivdualStrikeButton3D(int id, double pos,
                                              unsigned int Texture,
                                              double state,
                                              const CPlayer &player) const {
    CEngine::RenderDoubleStruct TempStruct3D{};
    double rtime = 0.0;
    static double lp[3] = {0.0, 0.0, 0.0};

    if (rtime > -5.0) {
        const double idd = id;

        const double size = 0.2;
        const double position = -0.51;

        auto form = [size, position](double idd) {
            return position + (idd * size / 48.0) + (idd * size);
        };

        const static double xdata[] = {form(0), form(1), form(2), form(3),
                                       form(4)};

        TempStruct3D.Text = Texture;
        TempStruct3D.TextureX1 = idd * 0.2;
        TempStruct3D.TextureX2 = idd * 0.2 + 0.2;

        double nCalc = rtime * speedMp;

        nCalc += 0.55;

        if (player.plusEnabled) {
            TempStruct3D.TextureY1 = 0.5;
            TempStruct3D.TextureY2 = 0.0;
        } else {
            TempStruct3D.TextureY1 = 1.0;
            TempStruct3D.TextureY2 = 0.5;
        }

        TempStruct3D.x1 = xdata[id];
        TempStruct3D.x2 = TempStruct3D.x1 + size;
        TempStruct3D.x3 = TempStruct3D.x1 + size;
        TempStruct3D.x4 = TempStruct3D.x1;

        TempStruct3D.y1 = -0.5;
        TempStruct3D.y2 = TempStruct3D.y1;
        TempStruct3D.y3 = -0.5;
        TempStruct3D.y4 = TempStruct3D.y3;

        TempStruct3D.z1 = nCalc;
        TempStruct3D.z2 = TempStruct3D.z1;
        TempStruct3D.z3 = nCalc + size * 2.0;
        TempStruct3D.z4 = TempStruct3D.z3;

        {
            lp[0] = TempStruct3D.x1 + 0.1;
            lp[1] = -0.4992;
            lp[2] = TempStruct3D.z1 + size;
            CEngine::engine().renderAt(TempStruct3D.x1 + 0.1, -0.4992,
                                       TempStruct3D.z1 + size);
        }

        auto *texts = GPPGame::GuitarPP().sbaseTexture3D;

        GPPGame::GuitarPP().triggerBASEOBJ.draw(
            player.plusEnabled ? texts[5] : texts[id], false);
    }
}

void CGamePlay::renderIndivdualStrikeButton3DStrike(
    size_t id, double pos, unsigned int Texture, double state,
    const CPlayer &player) const {
    CEngine::RenderDoubleStruct TempStruct3D{};
    double rtime = 0.0;

    if (rtime > -5.0) {
        double size = 0.2;
        double position = -0.51;

        auto form = [size, position](double idd) {
            return position + (idd * size / 48.0) + (idd * size);
        };

        const static double xdata[] = {form(0), form(1), form(2), form(3),
                                       form(4)};

        TempStruct3D.Text = Texture;
        TempStruct3D.TextureX1 = double(id) * 0.2;
        TempStruct3D.TextureX2 = double(id) * 0.2 + 0.2;

        double nCalc = rtime * speedMp;

        nCalc += 0.55;

        if (player.plusEnabled) {
            TempStruct3D.TextureY1 = 0.5;
            TempStruct3D.TextureY2 = 0.0;
        } else {
            TempStruct3D.TextureY1 = 1.0;
            TempStruct3D.TextureY2 = 0.5;
        }

        TempStruct3D.x1 = xdata[id];
        TempStruct3D.x2 = TempStruct3D.x1 + size;
        TempStruct3D.x3 = TempStruct3D.x1 + size;
        TempStruct3D.x4 = TempStruct3D.x1;

        TempStruct3D.y1 = -0.5;
        TempStruct3D.y2 = TempStruct3D.y1;
        TempStruct3D.y3 = -0.5;
        TempStruct3D.y4 = TempStruct3D.y3;

        TempStruct3D.z1 = nCalc;
        TempStruct3D.z2 = TempStruct3D.z1;
        TempStruct3D.z3 = nCalc + size * 2.0;
        TempStruct3D.z4 = TempStruct3D.z3;

        auto *texts = GPPGame::GuitarPP().striggerTexture3D;

        CEngine::engine().renderAt(TempStruct3D.x1 + 0.1, -0.5 + state,
                                   TempStruct3D.z1 + size);
        GPPGame::GuitarPP().triggerOBJ.draw(
            player.plusEnabled ? texts[5] : texts[id], false);
    }
}

auto CGamePlay::getRunningMusicTime(const CPlayer &player) const -> double {

    using namespace std::chrono;
    auto res = duration_cast<duration<double, std::ratio<1, 1>>>(
                   steady_clock::now() - startSongTime)
                   .count();
    return res;
}

auto CGamePlay::getRunningMusicTimeUpd(const CPlayer &player) const -> double {

    using namespace std::chrono;
    auto res = duration_cast<duration<double, std::ratio<1, 1>>>(
                   steady_clock::now() - startSongTime)
                   .count();
    return res;
}

auto CGamePlay::time2Position(double Time, const CPlayer &player) -> double {
    return ((getRunningMusicTime(player) - Time) * -1.0) / 1.5;
}

auto CGamePlay::pos2Alpha(double pos) -> double {
    double result = 0.0;

    const double minPosMinAlpha = 1.6;
    const double maxPosMaxAlpha = 0.6;
    const double dif = minPosMinAlpha - maxPosMaxAlpha;

    if (pos < maxPosMaxAlpha) {
        return 1.0;
    }

    if (pos > minPosMinAlpha) {
        return 0.0;
    }

    result = (1.0 / dif) * (minPosMinAlpha - pos);

    if (result > 1.0) {
        {
            return 1.0;
        }
    }

    return result;
}

void CGamePlay::renderHoposLight() {
    double size = 0.2;

    CEngine::RenderDoubleStruct TempStruct3D{};

    TempStruct3D.Text = hopoLightText;
    TempStruct3D.TextureX1 = 0.0;
    TempStruct3D.TextureX2 = 1.0;
    TempStruct3D.TextureY1 = 1.0;
    TempStruct3D.TextureY2 = 0.0;

    for (auto it = hopostp.rbegin(); it != hopostp.rend(); it++) {
        auto &hp = *it;

        TempStruct3D.x1 = hp.x;
        TempStruct3D.x2 = TempStruct3D.x1 + size;
        TempStruct3D.x3 = TempStruct3D.x1 + size;
        TempStruct3D.x4 = TempStruct3D.x1;

        TempStruct3D.y1 = hp.y;
        TempStruct3D.y2 = TempStruct3D.y1;
        TempStruct3D.y3 = hp.y;
        TempStruct3D.y4 = TempStruct3D.y3;

        TempStruct3D.z1 = hp.z;
        TempStruct3D.z2 = TempStruct3D.z1;
        TempStruct3D.z3 = hp.z + size * 2.0;
        TempStruct3D.z4 = TempStruct3D.z3;

        double alpha = pos2Alpha(-TempStruct3D.z1 / 5.8);

        if (alpha <= 0.0) {
            continue;
        }

        CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);

        CEngine::engine().Render3DQuad(TempStruct3D);
    }

    CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);
    hopostp.clear();
}

void CGamePlay::renderIndivdualNoteShadow(int id, double pos,
                                          unsigned int Texture, bool tail,
                                          const CPlayer &player) {
    CEngine::RenderDoubleStruct TempStruct3D{};
    double rtime = getRunningMusicTimeUpd(player) - pos;

    bool rotated = false;

    if (rtime > -5.0) {
        constexpr double size = 0.2;
        constexpr double position = -0.51;

        auto form = [size, position](double idd) {
            return position + (idd * size / 48.0) + (idd * size);
        };

        static const double xdata[] = {form(0), form(1), form(2), form(3),
                                           form(4)};

        TempStruct3D.TextureX1 = 0.0;
        TempStruct3D.TextureX2 = 1.0;

        double nCalc = rtime * speedMp;

        nCalc += 0.55;

        TempStruct3D.TextureY1 = 1.0;
        TempStruct3D.TextureY2 = 0.0;

        TempStruct3D.x1 = xdata[id];
        TempStruct3D.x2 = TempStruct3D.x1 + size;
        TempStruct3D.x3 = TempStruct3D.x1 + size;
        TempStruct3D.x4 = TempStruct3D.x1;

        TempStruct3D.y1 = -0.4;
        TempStruct3D.y2 = TempStruct3D.y1;
        TempStruct3D.y3 = -0.4;
        TempStruct3D.y4 = TempStruct3D.y3;

        TempStruct3D.z1 = nCalc;
        TempStruct3D.z2 = TempStruct3D.z1;
        TempStruct3D.z3 = nCalc + size * 2.0;
        TempStruct3D.z4 = TempStruct3D.z3;

        CEngine::engine().renderAt(TempStruct3D.x1 + 0.1, -0.5,
                                   TempStruct3D.z1 + size);

        size_t plusPos = player.Notes.plusPos;

        if (plusPos < player.Notes.gPlus.size()) {
            const auto &p = player.Notes.gPlus[plusPos];

            if (p.time <= pos && pos < (p.time + p.lTime)) {
                CEngine::Rotate(player.renderState.rangle, 0.0, 1.0, 0.0);
                rotated = true;
            }
        }

        GPPGame::GuitarPP().noteOBJ.draw(
            GPPGame::GuitarPP().strumsTexture3D[id], false);
    }

    if (rotated) {
        {
            CEngine::engine().matrixReset();
        }
    }
}

auto CGamePlay::renderIndivdualNote(unsigned int texture,
                                    CEngine::RenderDoubleStruct &TempStruct3D,
                                    const CPlayer &player, bool rotated)
    -> bool {
    CEngine::engine().renderAt(TempStruct3D.x1 + 0.1, -0.5,
                               TempStruct3D.z1 + NOTE_SIZE);

    if (rotated) {
        CEngine::Rotate(player.renderState.rangle, 0.0, 1.0, 0.0);
    }

    GPPGame::GuitarPP().noteOBJ.draw(texture, false);

    if (rotated) {
        CEngine::engine().matrixReset();
    }

    return true;
}

auto CGamePlay::renderOpenNote(double pos, unsigned int Texture, int type,
                               const CPlayer &player) -> bool {
    CEngine::RenderDoubleStruct TempStruct3D{};
    double rtime = getRunningMusicTimeUpd(player) - pos;

    bool rotated = false;

    if (rtime > -5.0) {
        constexpr double size = 0.2;
        constexpr double position = -0.51;

        auto form = [=](double idd) {
            return position + (idd * size / 48.0) + (idd * size);
        };

        static const double xdata[] = {form(0), form(1), form(2), form(3),
                                        form(4)};

        TempStruct3D.TextureX1 = 0.0;
        TempStruct3D.TextureX2 = 1.0;

        double nCalc = rtime * speedMp;

        nCalc += 0.55;

        TempStruct3D.TextureY1 = 1.0;
        TempStruct3D.TextureY2 = 0.0;

        TempStruct3D.x1 = xdata[2];
        TempStruct3D.x2 = TempStruct3D.x1 + size;
        TempStruct3D.x3 = TempStruct3D.x1 + size;
        TempStruct3D.x4 = TempStruct3D.x1;

        TempStruct3D.y1 = -0.46;
        TempStruct3D.y2 = TempStruct3D.y1;
        TempStruct3D.y3 = -0.46;
        TempStruct3D.y4 = TempStruct3D.y3;

        TempStruct3D.z1 = nCalc;
        TempStruct3D.z2 = TempStruct3D.z1;
        TempStruct3D.z3 = nCalc + size * 2.0;
        TempStruct3D.z4 = TempStruct3D.z3;

        bool notHOPO = (type & notesFlags::nf_not_hopo) != 0;

        if (!notHOPO) {
            for (double i : xdata) {
                gppVec3f vec3data;
                vec3data.x = static_cast<float>(i);
                vec3data.y = -0.47F;
                vec3data.z = static_cast<float>(TempStruct3D.z1);

                hopostp.push_back(vec3data);
            }
        }

        double alpha = pos2Alpha(-TempStruct3D.z1 / 5.8);

        if (alpha <= 0.0) {
            return false;
        }

        CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);

        CEngine::engine().renderAt(0.0, -0.5, TempStruct3D.z1 + size);

        unsigned int text = 0;

        if (player.plusEnabled) {
            text = notHOPO ? GPPGame::GuitarPP().openNotePTexture3D
                           : GPPGame::GuitarPP().openNoteHOPOPTexture3D;
        } else {
            text = notHOPO ? GPPGame::GuitarPP().openNoteTexture3D
                           : GPPGame::GuitarPP().openNoteHOPOTexture3D;
        }

        GPPGame::GuitarPP().openNoteOBJ.draw(text, false);
    }

    return true;
}

void CGamePlay::renderIndividualLine(int id, double pos1, double pos2,
                                     unsigned int Texture,
                                     const CPlayer &player) {
    CEngine::RenderDoubleStruct TempStruct3D{};

    double runningTime = getRunningMusicTimeUpd(player);
    double rtime = runningTime - pos1;
    double rtime2 = runningTime - (pos1 + pos2);

    const double size = 0.2;
    const double position = -0.51;

    auto form = [size, position](double idd) {
        return position + (idd * size / 48.0) + (idd * size);
    };

    const static double xdata[] = {form(0), form(1), form(2), form(3), form(4)};

    double nCalc = rtime * speedMp;
    double nCalc2 = rtime2 * speedMp;

    nCalc += 0.55 + size;
    nCalc2 += 0.55 + size * 1.5;

    TempStruct3D.Text = Texture;
    TempStruct3D.TextureX1 = 0.0;
    TempStruct3D.TextureX2 = 1.0;
    TempStruct3D.TextureY1 = 1.0;
    TempStruct3D.TextureY2 = 0.0;

    TempStruct3D.x1 = xdata[id];
    TempStruct3D.x2 = TempStruct3D.x1 + size;
    TempStruct3D.x3 = TempStruct3D.x1 + size;
    TempStruct3D.x4 = TempStruct3D.x1;

    TempStruct3D.y1 = -0.4995;
    TempStruct3D.y2 = TempStruct3D.y1;
    TempStruct3D.y3 = -0.4995;
    TempStruct3D.y4 = TempStruct3D.y3;

    TempStruct3D.z1 = nCalc;
    TempStruct3D.z2 = TempStruct3D.z1;
    TempStruct3D.z3 = nCalc2;
    TempStruct3D.z4 = TempStruct3D.z3;

    TempStruct3D.alphaBottom = pos2Alpha(-TempStruct3D.z3 / 5.5);
    TempStruct3D.alphaTop = pos2Alpha(-TempStruct3D.z2 / 5.5);

    CEngine::engine().Render3DQuadWithAlpha(TempStruct3D);
}

void CGamePlay::renderTimeOnNote(double pos, double time,
                                 const CPlayer &player) {
    CEngine::RenderDoubleStruct TempStruct3D{};
    double rtime = getRunningMusicTimeUpd(player) - pos;

    if (rtime > -5.0) {
        double size = 0.2;
        double position = -0.51;

        double nCalc = rtime * speedMp;

        nCalc += 0.55;

        if (player.plusEnabled) {
            TempStruct3D.TextureY1 = 0.5;
            TempStruct3D.TextureY2 = 0.0;
        } else {
            TempStruct3D.TextureY1 = 1.0;
            TempStruct3D.TextureY2 = 0.5;
        }

        TempStruct3D.y1 = -0.5;
        TempStruct3D.y2 = TempStruct3D.y1;
        TempStruct3D.y3 = -0.5;
        TempStruct3D.y4 = TempStruct3D.y3;

        TempStruct3D.z1 = nCalc;
        TempStruct3D.z2 = TempStruct3D.z1;
        TempStruct3D.z3 = nCalc + size * 2.0;
        TempStruct3D.z4 = TempStruct3D.z3;

        double alpha = pos2Alpha(-TempStruct3D.z1 / 5.8);

        if (alpha <= 0.0) {
            return;
        }

        CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);

        std::string timestr = std::to_string(time);

        CFonts::fonts().draw3DTextInScreen(
            timestr, CFonts::fonts().getCenterPos(timestr, 0.2, 0.0), -0.4992,
            TempStruct3D.z1, 0.2, 0.0, -0.2);
    }
}

void CGamePlay::renderTailsBuffer(const CPlayer &player) {
    CEngine::RenderDoubleStruct TempStruct3D{};

    unsigned int text = GPPGame::GuitarPP().lineText;

    constexpr double size = 0.2;
    constexpr double position = -0.51;

    auto form = [&](int i) {
        return position + (static_cast<double>(i) * size / 48.0) +
               (static_cast<double>(i) * size);
    };

    static const double posx[] = {form(0), form(1), form(2), form(3),
                                      form(4)};
    static const double posxpsize[] = {posx[0] + size, posx[1] + size,
                                           posx[2] + size, posx[3] + size,
                                           posx[4] + size};

    TempStruct3D.Text = text;
    TempStruct3D.TextureX1 = 0.0;
    TempStruct3D.TextureX2 = 1.0;
    TempStruct3D.TextureY1 = 1.0;
    TempStruct3D.TextureY2 = 0.0;

    TempStruct3D.y1 = -0.49;
    TempStruct3D.y2 = TempStruct3D.y1;
    TempStruct3D.y3 = -0.49;
    TempStruct3D.y4 = TempStruct3D.y3;

    for (const auto &t : player.renderState.tailsData) {
        for (size_t i = 0; i < 5; i++) {
            if ((t.type & notesFlagsConst[i]) != 0) {
                TempStruct3D.x1 = posx[i];
                TempStruct3D.x2 = posxpsize[i];
                TempStruct3D.x3 = TempStruct3D.x2;
                TempStruct3D.x4 = TempStruct3D.x1;

                TempStruct3D.z1 = t.top;
                TempStruct3D.z2 = TempStruct3D.z1;
                TempStruct3D.z3 = t.bottom;
                TempStruct3D.z4 = TempStruct3D.z3;

                TempStruct3D.alphaBottom = pos2Alpha(-TempStruct3D.z3 / 5.5);
                TempStruct3D.alphaTop = pos2Alpha(-TempStruct3D.z2 / 5.5);

                CEngine::engine().Render3DQuadWithAlpha(TempStruct3D);
            }
        }
    }
}

void CGamePlay::addTailToBuffer(const CPlayer::NotesData::Note &note,
                                double pos1, double pos2, double runningTime,
                                PlayerRenderState &renderState) const {

    double rtime = runningTime - pos1;
    double rtime2 = runningTime - (pos1 + pos2);

    const double size = 0.2;
    const double position = -0.51;

    double nCalc = rtime * speedMp;
    double nCalc2 = rtime2 * speedMp;

    nCalc += 0.55 + size;
    nCalc2 += 0.55 + size * 1.5;

    lineData lndata{};
    lndata.type = note.type;
    lndata.top = nCalc;
    lndata.bottom = nCalc2;

    renderState.tailsData.push_back(lndata);
}

auto CGamePlay::renderNote(const CPlayer::NotesData::Note &note,
                           const CPlayer &player, double ltimet,
                           PlayerRenderState &renderState) -> bool {
    double time = note.time;
    double dif = time - ltimet;
    bool bAddTailToBuffer = false;
    double lt = 0.0;
    double tlng = 0.0;
    bool result = true;

    if ((note.type & notesFlags::nf_open) != 0) {
        if ((((note.type & notesFlags::nf_picked) == 0) || dif > -0.5) &&
            ((note.type & notesFlags::nf_doing_slide) == 0)) {
            result = renderOpenNote(time, 0, note.type, player);
        }

        return result;
    }

    unsigned int texture = 1;

    if (note.lTime > 0.0 && ((note.type & notesFlags::nf_doing_slide) != 0) &&
        ((note.type & notesFlags::losted) == 0)) {
        bAddTailToBuffer = true;
        lt = ltimet;
        tlng = time - ltimet + note.lTime;
    } else if (((note.type & notesFlags::nf_slide) != 0) &&
               ((note.type & notesFlags::nf_slide_picked) == 0)) {

        bAddTailToBuffer = true;
        lt = time;
        tlng = note.lTime;
    }

    if (((note.type & notesFlags::nf_not_hopo) ^ notesFlags::nf_not_hopo) !=
        0) {
        texture = GPPGame::GuitarPP().HOPOSText;
    }

    if ((((note.type & notesFlags::nf_picked) == 0) || dif > -0.5) &&
        ((note.type & notesFlags::nf_doing_slide) == 0)) {
    } else {
        if (bAddTailToBuffer) {
            addTailToBuffer(note, lt, tlng, ltimet, renderState);
        }

        return result;
    }

    CEngine::RenderDoubleStruct TempStruct3D{};
    double rtime = getRunningMusicTimeUpd(player) - time;

    if (rtime < -5.0) {
        if (bAddTailToBuffer) {
            addTailToBuffer(note, lt, tlng, ltimet, renderState);
        }

        return false;
    }

    TempStruct3D.TextureX1 = 0.0;
    TempStruct3D.TextureX2 = 1.0;

    const double nCalc = (rtime * speedMp) + 0.55;

    TempStruct3D.z1 = nCalc;
    TempStruct3D.z2 = TempStruct3D.z1;
    TempStruct3D.z3 = nCalc + NOTE_SIZE * 2.0;
    TempStruct3D.z4 = TempStruct3D.z3;

    const double alpha = pos2Alpha(-TempStruct3D.z1 / 5.8);

    if (alpha <= 0.0) {
        return false;
    }

    constexpr double position = -0.51;

    auto form = [&](double idd) {
        return position + (idd * NOTE_SIZE / 48.0) + (idd * NOTE_SIZE);
    };

    static const double xdata[] = {form(0), form(1), form(2), form(3), form(4)};

    CEngine::engine().setColor(1.0, 1.0, 1.0, alpha);

    bool rotated = false;

    {
        size_t plusPos = player.Notes.plusPos;

        if (plusPos < player.Notes.gPlus.size()) {
            const auto &p = player.Notes.gPlus[plusPos];

            if (p.time <= note.time && note.time < (p.time + p.lTime)) {
                rotated = true;
            }
        }
    }

    auto *texts = (texture == GPPGame::GuitarPP().HOPOSText)
                      ? GPPGame::GuitarPP().hopoTexture3D
                      : GPPGame::GuitarPP().strumsTexture3D;

    if ((note.type & noteTap) != 0) {
        texts = GPPGame::GuitarPP().tapTexture3D;
    }

    for (size_t i = 0; i < 5; i++) {
        if ((note.type & notesFlagsConst[i]) == 0) {
            continue;
        }

        TempStruct3D.x1 = xdata[i];

        if (texture == GPPGame::GuitarPP().HOPOSText) {
            gppVec3f vec3data{static_cast<float>(TempStruct3D.x1), -0.462F,
                              static_cast<float>(TempStruct3D.z1)};

            hopostp.push_back(vec3data);
        }

        result = renderIndivdualNote(player.plusEnabled ? texts[5] : texts[i],
                                     TempStruct3D, player, rotated);
    }

    if (bAddTailToBuffer) {
        addTailToBuffer(note, lt, tlng, ltimet, renderState);
    }

    return result;
}

void CGamePlay::renderNotePlayer(const CPlayer &player,
                                 PlayerRenderState &renderState) {
    const auto &notes = player.Notes;
    const auto &gNotes = player.Notes.gNotes;
    const double musicTime = getRunningMusicTimeUpd(player);

    for (size_t i = notes.notePos, size = gNotes.size(); i < size; i++) {
        const auto &note = gNotes[i];
        double noteTime = note.time - musicTime;

        if (noteTime <= 5.0) {
            double endNoteTime = noteTime + note.lTime;

            bool bLongNPicked =
                (note.type & notesFlags::nf_slide_picked) != 0 &&
                endNoteTime >= 0.03;
            if ((note.type & notesFlags::nf_picked) == 0 ||
                noteTime >= -0.0025 || bLongNPicked) {
                if (!renderNote(note, player, musicTime, renderState)) {
                    break;
                }
            }
        }
    }
}

void CGamePlay::updatePlayer(CPlayer &player, double deltatime) {
    GPP_PROFILE_SCOPE(UpdatePlayer);
    auto &notes = player.Notes;
    auto &gNotes = player.Notes.gNotes;
    auto &engine = CEngine::engine();

    int playerLVL = player.getLevel();

    if (player.plusEnabled) {
        player.plusPower -= deltatime * gpp::plus::drainPerSecond;

        if (player.plusPower <= 0.0) {
            player.plusEnabled = false;
            player.plusPower = 0.0;
        }
    }

    if (player.bPlusStrike &&
        CEngine::getTime() - player.plusThunterStrikeStart > 4.0) {
        player.bPlusStrike = false;
    }

    player.update();

    const double musicTime = getRunningMusicTime(player);

    double minendtime = 0.0;
    auto minendtimei = notes.notePos;

    auto getHighestFlag = [](int flags) {
        int result = 0;
        for (int ji = 4; ji >= 0; ji--) {
            if ((flags & notesFlagsConst[ji]) != 0) {
                result = notesFlagsConst[ji];
                break;
            }
        }

        return result;
    };

    const int fretsPressedFlags = player.getFretsPressedFlags();
    const int lastFretsPressedFlags = player.getLastFretsPressedFlags();
    const int highestFlagInPressedKey = getHighestFlag(fretsPressedFlags);
    const int lastHighestFlagInPressedKey =
        getHighestFlag(lastFretsPressedFlags);

    bool noteDoedThisFrame = false;

    size_t plusSize = player.Notes.gPlus.size();

    if (player.Notes.plusPos < plusSize) {
        auto &plusp = player.Notes.gPlus[player.Notes.plusPos];

        int64_t tplusnotes = (plusp.lastNote - plusp.firstNote) + 1;

        if ((plusp.time + plusp.lTime) > (musicTime + 0.01) &&
            plusp.time <= musicTime && tplusnotes > 0) {
            player.plusLoadF =
                ((double)(1 + notes.lastNotePicked - plusp.firstNote) /
                 (double)tplusnotes);
        } else {
            player.plusLoadF = 0.0;
        }

        if ((plusp.time + plusp.lTime) < (musicTime + 0.01)) {
            ++player.Notes.plusPos;
        }
    } else {
        player.plusLoadF = player.plusLoadB = 0.0;
    }

    if (std::abs(player.plusLoadF - player.plusLoadB.load()) > 1e-4) {
        auto loadB = player.plusLoadB.load();
        player.plusLoadB.store(std::min(
            1.0, loadB + ((player.plusLoadF - loadB) * deltatime * 10.0)));
    }

    bool inslide = false;
    bool inslide2 = false;

    if (gNotes.size() > notes.notePos) {
        for (int ji = 0; ji < 5; ji++) {
            if ((gNotes[notes.notePos].type & notesFlagsConst[ji]) != 0) {
                const size_t npos = ~(static_cast<size_t>(0));

                if (player.notesSlide[ji] != npos) {
                    inslide = true;
                }
            }
        }
    }

    if (!player.plusEnabled && player.plusPower > 0.0 &&
        (CEngine::engine().getKey(' ') != 0)) {
        player.plusEnabled = true;
    }

    auto doPlus = [&](size_t i) {
        if (notes.plusPos < notes.gPlus.size()) {
            auto &plPos = notes.gPlus[notes.plusPos];

            if (player.plusPower < player.maxPlusPower) {
                if (plPos.lastNote == i) {
                    player.plusPower +=
                        player.maxPlusPower / gpp::plus::phrasesPerBar;
                    ++notes.plusPos;
                    player.plusThunterStrikeStart = CEngine::getTime();
                    player.bPlusStrike = true;
                    player.lastNotePlusType = notes.gNotes[i].type;
                }
            }

            player.plusPower = std::min(player.plusPower, player.maxPlusPower);
        }
    };

    auto getLastNotePickedTimeDiff = [&](size_t i) {
        double result = 0;
        if (player.Notes.lastNotePicked == ~(static_cast<size_t>(0))) {
            return player.Notes.gNotes[i].time;
        }

        return player.Notes.gNotes[i].time -
               player.Notes.gNotes[player.Notes.lastNotePicked].time;
    };

    auto tapResulted = [&player](int flags) {
        double ttime = CEngine::getTime();

        for (int ji = 0; ji < 5; ji++) {
            if ((flags & notesFlagsConst[ji]) != 0) {
                player.fretsPressedTime[ji] = ttime - 100.0;
            }
        }
    };

    auto doNoteFunc = [&](CPlayer::NotesData::Note &note, size_t i) {
        if (!noteDoedThisFrame && !bIsACharterGP) {
            tapResulted(note.type);

            if (note.lTime == 0.0) {
                player.doNote(i);
                note.type |= notesFlags::nf_picked;
                doPlus(i);

                noteDoedThisFrame = true;
                player.aError = false;

                {
                    for (int ji = 0; ji < 5; ji++) {
                        if (((note.type & notesFlagsConst[ji]) != 0) ||
                            (note.type & notesFlags::nf_open) != 0) {
                            notes.fretsNotePickedTime[ji] = CEngine::getTime();
                            player.notesSlide[ji] = -1;
                        }
                    }
                }
            } else if ((note.type & notesFlags::nf_doing_slide) == 0) {
                player.doNote(i);
                doPlus(i);

                noteDoedThisFrame = true;
                player.aError = false;

                {
                    for (int ji = 0; ji < 5; ji++) {
                        if ((note.type & notesFlagsConst[ji]) != 0) {
                            notes.fretsNotePickedTime[ji] = CEngine::getTime();
                        }
                    }
                }

                if (note.lTime > 0.0) {
                    note.type |= notesFlags::nf_doing_slide;

                    inslide2 = true;

                    {
                        for (int ji = 0; ji < 5; ji++) {
                            if ((note.type & notesFlagsConst[ji]) != 0) {
                                player.notesSlide[ji] = i;
                            }
                        }
                    }
                }
            }
        }
    };

    size_t firstNoteToDo = 0;
    bool firstNoteToDoSetted = false;
    bool strklinbotsetted = false;

    bool errorThisFrame = false;
    bool hopostrmm = false;
    bool strumdelayed = false;

    // Advance the BPM index based on the current music time.
    if (player.BPMNowUpdate >= player.Notes.BPM.size() &&
        !player.Notes.BPM.empty()) {
        player.BPMNowUpdate = player.Notes.BPM.size() - 1;
    }

    if (musicTime >= 0.0) {
        size_t nbuff = player.BPMNowUpdate + 1;
        if (player.Notes.BPM.size() > nbuff) {
            if (musicTime > (player.Notes.BPM[nbuff].time)) {
                player.BPMNowUpdate = nbuff;
            }
        }
    } else {
        player.BPMNowUpdate = 0;
    }

    const double BPS = 30.0 / player.Notes.BPM[player.BPMNowUpdate].lTime;

    auto checkCorrect = [&](int ntsInac, int ntsT, int noteHF,
                            const CPlayer::NotesData::Note &note) {
        if (!bIsACharterGP) {
            if (ntsInac > 1 && fretsPressedFlags == ntsT) {
                return true;
            }

            if (ntsInac == 1 && highestFlagInPressedKey == noteHF) {
                return true;
            }
            if (ntsInac == 0 && fretsPressedFlags == 0) {
                return true;
            }
        }

        return false;
    };

    auto processError = [&]() {
        if (!bIsACharterGP) {
            player.processError();

            errorThisFrame = true;

            int fretPid = 0;
            for (auto &fretP : player.fretsPressed) {
                if (fretP || fretsPressedFlags == 0) {
                    {
                        CEngine::playSoundStream(
                            GPPGame::GuitarPP().errorsSound[fretPid]);
                    }
                }

                fretPid++;
            }
        }
    };

    auto isTapping = [&player](int flags, double time) {
        bool result = true;

        double ttime = CEngine::getTime();

        for (int ji = 0; ji < 5; ji++) {
            if ((flags & notesFlagsConst[ji]) != 0) {
                if (result) {
                    result = (ttime - player.fretsPressedTime[ji]) < time;
                } else {
                    break;
                }
            }
        }

        return result;
    };

    const double speedCALC = speedMp / gSpeed;

    for (size_t i = notes.notePos, size = gNotes.size(); i < size; i++) {
        auto &note = gNotes[i];
        const double noteTime = note.time - musicTime;
        const double endNoteTime = noteTime + note.lTime;

        const double rtime = (musicTime - note.time) * speedCALC;

        if (!inslide && !inslide2) {
            minendtime = endNoteTime;
            minendtimei = i;

            if (i > 0) {
                minendtimei = i - 1;
            }

            if (i > 2) {
                minendtimei = i - 2;
            }

            if (i > 5) {
                minendtimei = i - 5;
            }
        }

        if (noteTime <= 5.0) {

            bool bLongNPicked =
                (note.type & notesFlags::nf_slide_picked) != 0 &&
                endNoteTime >= 0.03;
            if ((note.type & notesFlags::nf_picked) == 0 ||
                noteTime >= -0.0025 || bLongNPicked) {
            }
        }

        const bool tappable = (note.type & notesFlags::noteTap) != 0;

        if (endNoteTime > -1.5 && noteTime < 5.0) {
            if (!hopostrmm) {
                if (noteTime >= -0.05 && noteTime < 0.1 &&
                    ((note.type & notesFlags::hopontstrmmd) == 0) &&
                    highestFlagInPressedKey ==
                        getHighestFlag(note.type & CPlayer::notesEnum) &&
                    (player.palhetaKey != 0)) {
                    hopostrmm = true;
                    note.type |= notesFlags::hopontstrmmd;
                }
            }

            const bool isStrum = (note.type & notesFlags::nf_not_hopo) != 0;

            int ntsInac = 0;
            const int ntsT = note.type & CPlayer::notesEnum;
            const int noteHF = getHighestFlag(ntsT);

            for (size_t i : notesFlagsConst) {
                if ((note.type & i) != 0) {
                    ntsInac++;
                }
            }

            if (!noteDoedThisFrame && noteTime >= 0.1 && noteTime < 0.15 &&
                ((note.type & notesFlags::nf_picked) == 0) &&
                ((note.type & notesFlags::strmstlrc) == 0)) {
                if ((player.palhetaKey != 0) &&
                    ((note.type & notesFlags::nf_not_hopo) != 0) &&
                    (ntsT != 0 || player.usarPalheta)) {
                    if (checkCorrect(ntsInac, ntsT, noteHF, note)) {
                        note.type |= notesFlags::strmstlrc;
                        strumdelayed = true;
                    }
                }
            }

            if (!noteDoedThisFrame && noteTime > -0.05 && noteTime < 0.15 &&
                !strklinbotsetted) {
                player.strklinent = i;
                player.strklinenttime = noteTime;
                strklinbotsetted = true;
            }

            if (noteTime > -0.05 && noteTime < 0.1 &&
                ((note.type & notesFlags::nf_picked) == 0) &&
                ((note.type & notesFlags::nf_failed) == 0)) {
                if (!noteDoedThisFrame) {
                    bool palhetaTest = player.palhetaKey != 0;

                    if (!player.usarPalheta) {
                        palhetaTest = isTapping(note.type, 0.15 - noteTime);
                    }

                    if (isStrum) {
                        if (player.palhetaKey != 0) {
                            if ((note.type & notesFlags::strmstlrc) != 0) {
                                if (checkCorrect(ntsInac, ntsT, noteHF, note)) {
                                    doNoteFunc(note, i);
                                    player.lastHOPO = 0;
                                } else {

                                    processError();
                                }
                            } else if (ntsInac > 1 &&
                                       fretsPressedFlags == ntsT) {
                                doNoteFunc(note, i);
                                player.lastHOPO = 0;
                            } else if (ntsInac == 1 &&
                                       highestFlagInPressedKey == noteHF) {
                                doNoteFunc(note, i);
                                player.lastHOPO = noteHF;
                            } else if (ntsInac == 0 && fretsPressedFlags == 0) {
                                doNoteFunc(note, i);
                                player.lastHOPO = 0;
                            } else {

                                processError();
                            }
                        } else if (!player.usarPalheta) {
                            if (!player.aError &&
                                ((note.type & notesFlags::nf_open) != 0) &&
                                ntsInac == 0 && fretsPressedFlags == 0) {
                                doNoteFunc(note, i);
                                player.lastHOPO = 0;
                            }
                        }
                    } else if ((note.type & notesFlags::nf_not_hopo) == 0) {

                        if (highestFlagInPressedKey == noteHF) {
                            bool regiserr = false;

                            if (firstNoteToDoSetted && firstNoteToDo != i) {
                                regiserr = true;
                            }

                            if (!player.aError && !regiserr) {
                                if (highestFlagInPressedKey !=
                                    lastHighestFlagInPressedKey) {
                                    doNoteFunc(note, i);
                                    player.lastHOPO = noteHF;
                                } else if (getLastNotePickedTimeDiff(i) <=
                                               BPS &&
                                           player.lastHOPO != noteHF) {
                                    doNoteFunc(note, i);
                                    player.lastHOPO = noteHF;
                                }
                            } else {
                                if (palhetaTest &&
                                    (highestFlagInPressedKey != 0 ||
                                     (player.palhetaKey != 0))) {

                                    player.processErrorNonPickedB(i);

                                    doNoteFunc(note, i);
                                    regiserr = false;
                                    player.lastHOPO = noteHF;
                                }
                            }

                            if (regiserr) {
                                player.aError = true;
                            }
                        }
                    }

                    if ((tappable || !player.usarPalheta) && isStrum &&
                        !noteDoedThisFrame && !errorThisFrame &&
                        isTapping(note.type, 0.15 - noteTime)) {
                        if ((note.type & notesFlags::strmstlrc) != 0) {
                            if (checkCorrect(ntsInac, ntsT, noteHF, note)) {
                                doNoteFunc(note, i);
                                player.lastHOPO = 0;
                            } else {
                            }
                        } else if (ntsInac > 1 && fretsPressedFlags == ntsT) {
                            doNoteFunc(note, i);
                            player.lastHOPO = 0;
                        } else if (ntsInac == 1 &&
                                   highestFlagInPressedKey == noteHF) {
                            doNoteFunc(note, i);
                            player.lastHOPO = noteHF;
                        }
                    }
                }
            } else if (noteTime <= -0.05 && !bIsACharterGP) {
                if (((note.type & notesFlags::nf_failed) == 0) &&
                    ((note.type & notesFlags::nf_picked) == 0) &&
                    ((note.type & notesFlags::nf_doing_slide) == 0)) {
                    note.type |= notesFlags::nf_failed;
                    player.processError();
                    player.aError = true;
                }
            } else {
                if (!firstNoteToDoSetted && !bIsACharterGP) {
                    if (((note.type & notesFlags::nf_failed) == 0) &&
                        ((note.type & notesFlags::nf_picked) == 0) &&
                        ((note.type & notesFlags::nf_doing_slide) == 0)) {
                        firstNoteToDo = i;
                        firstNoteToDoSetted = true;
                    }
                }
            }

            if (endNoteTime < 0.0 &&
                ((note.type & notesFlags::nf_slide) != 0) && !bIsACharterGP) {
                note.type |= notesFlags::nf_picked;
                note.type |= notesFlags::nf_slide_picked;
            }

        } else if (minendtime < -2.0) {
            notes.notePos = minendtimei;
        } else if (noteTime > 5.0) {
            break;
        }
    }

    bool fretpError = false;

    if (!bIsACharterGP) {
        if (!errorThisFrame && !noteDoedThisFrame && (player.palhetaKey != 0) &&
            !hopostrmm && !strumdelayed) {
            int fretPid = 0;
            for (auto &fretP : player.fretsPressed) {
                if (fretP || fretsPressedFlags == 0) {
                    {
                        CEngine::playSoundStream(
                            GPPGame::GuitarPP().errorsSound[fretPid]);
                    }
                }

                fretPid++;
            }

            fretpError = true;
            player.processError();
        }

        bool cklngNote = false;
        double sustainSeconds = 0.0;

        {
            bool cancelAllLongNotes = false;

            for (int ji = 0; ji < 5; ji++) {
                size_t id = player.notesSlide[ji];

                if (id != ~0UL) {
                    cklngNote = true;
                    auto &note = player.Notes.gNotes[id];
                    double noteTime = note.time - musicTime;
                    double endNoteTime = noteTime + note.lTime;
                    // Score only the active overlap, retaining the existing
                    // 130 ms sustain release tolerance. Chords score once.
                    sustainSeconds =
                        std::max(sustainSeconds,
                                 sustainActiveSeconds(note.time, note.lTime,
                                                      musicTime, deltatime));

                    int ntsInac = 0;
                    int ntsT = note.type & CPlayer::notesEnum;

                    for (size_t i : notesFlagsConst) {
                        if ((note.type & i) != 0) {
                            ntsInac++;
                        }
                    }

                    if (fretsPressedFlags != ntsT) {
                        if (ntsInac > 1) {
                            cancelAllLongNotes = true;
                        }

                        else if (highestFlagInPressedKey !=
                                 getHighestFlag(ntsT)) {
                            cancelAllLongNotes = true;
                        }
                    }

                    if (endNoteTime < 0.13) {
                        player.notesSlide[ji] = ~0UL;
                        note.type |= notesFlags::nf_picked;
                        note.type |= notesFlags::nf_slide_picked;
                    } else {

                        if (CEngine::getTime() -
                                player.Notes.fretsNotePickedTime[ji] >
                            0.05) {
                            player.Notes.fretsNotePickedTime[ji] =
                                CEngine::getTime() - 0.05;
                        }
                    }
                }
            }

            if (cancelAllLongNotes || fretpError) {
                cklngNote = false;
                player.muteInstrument();
                for (auto &ji : player.notesSlide) {
                    size_t id = ji;

                    if (id != ~0UL) {
                        auto &note = player.Notes.gNotes[id];

                        ji = ~0UL;
                        note.type |= notesFlags::losted;
                    }

                    ji = ~0UL;
                    inslide2 = false;
                }
            }
        }

        if (cklngNote) {
            player.addPointsByDoingLongNote(sustainSeconds);
        }

        if (noteDoedThisFrame && GPPGame::GuitarPP().showTextsTest) {
            int nowLevel = player.getLevel();
            if (nowLevel != playerLVL) {
                CFonts::textAlert t;

                t.callback = alertTest;
                t.msg = "Level Up! " + std::to_string(nowLevel);
                t.startTime = CEngine::getTime();

                CFonts::fonts().addTextAlert(std::move(t));
            }

            int64_t combo = player.getCombo();

            if (combo > 0) {
                if (combo == 50) {
                    CFonts::textAlert t;

                    t.callback = alertTest;
                    t.msg = "50 Note Streak";
                    t.startTime = CEngine::getTime();

                    CFonts::fonts().addTextAlert(std::move(t));
                } else {
                    int r = combo % 100;

                    if (r == 0) {
                        CFonts::textAlert t;

                        t.callback = alertTest;
                        t.msg = std::to_string(combo) + " Note Streak";
                        t.startTime = CEngine::getTime();

                        CFonts::fonts().addTextAlert(std::move(t));
                    }
                }
            }
        }
    }
}

void CGamePlay::renderFretBoardShadow(const CPlayer &player, double x1,
                                      double x2, double x3, double x4,
                                      unsigned int Text) {
    fretboardLData.clear();
    fretboardLData.useColors = true;
    fretboardLData.autoEnDisaColors = true;

    double size = 2.1;

    auto positionCalcByT = [this, size](double p, double prop) {
        double cCalc = -p * 5.0;
        double propSpeeed = 5.0 / speedMp;

        cCalc /= propSpeeed;

        cCalc *= 1000000.0;
        cCalc = static_cast<double>((int64_t)cCalc %
                                    (int64_t)((size * 1000000) * prop));
        return cCalc / 1000000.0;
    };

    double musicRunningTime = getRunningMusicTimeUpd(player) - 4.0;
    double cCalc = positionCalcByT(musicRunningTime, (x2 - x1));

    CEngine::RenderDoubleStruct FretBoardStruct{};

    FretBoardStruct.x1 = x1;
    FretBoardStruct.x2 = x2;
    FretBoardStruct.x3 = x3;
    FretBoardStruct.x4 = x4;

    FretBoardStruct.TextureX1 = 0.0;
    FretBoardStruct.TextureX2 = 1.0;

    FretBoardStruct.TextureY1 = 0.0;
    FretBoardStruct.TextureY2 = 1.0;

    FretBoardStruct.y1 = -0.5;
    FretBoardStruct.y2 = FretBoardStruct.y1;
    FretBoardStruct.y3 = FretBoardStruct.y1;
    FretBoardStruct.y4 = FretBoardStruct.y1;

    fretboardLData.texture = FretBoardStruct.Text = 0;

    for (intptr_t i = -2; i < 10; i++) {
        FretBoardStruct.z1 = (x2 - x1) * (-size) * i - cCalc;
        FretBoardStruct.z2 = FretBoardStruct.z1;
        FretBoardStruct.z3 = FretBoardStruct.z1 + (x2 - x1) * (-size);
        FretBoardStruct.z4 = FretBoardStruct.z3;

        FretBoardStruct.alphaBottom = pos2Alpha(-FretBoardStruct.z3 / 5.5);
        FretBoardStruct.alphaTop = pos2Alpha(-FretBoardStruct.z2 / 5.5);

        if (FretBoardStruct.alphaBottom <= 0.0 &&
            FretBoardStruct.alphaTop <= 0.0) {
            continue;
        }

        FretBoardStruct.alphaBottom =
            std::max(FretBoardStruct.alphaBottom, 0.0);

        FretBoardStruct.alphaTop = std::max(FretBoardStruct.alphaTop, 0.0);

        CEngine::pushQuad(fretboardLData, FretBoardStruct);
    }

    CEngine::engine().drawTrianglesWithAlpha(fretboardLData);
}

void CGamePlay::renderFretBoard(const CPlayer &player, double x1, double x2,
                                double x3, double x4, unsigned int Text) {
    fretboardLData.clear();
    fretboardLData.useColors = true;
    fretboardLData.autoEnDisaColors = false;

    const double size = 2.1 * 1.33333333;

    auto positionCalcByT = [this, size](double p, double prop) {
        double cCalc = -p * 5.0;
        double propSpeeed = 5.0 / speedMp;

        cCalc /= propSpeeed;

        cCalc *= 1000000.0;
        cCalc = static_cast<double>((int64_t)cCalc %
                                    (int64_t)((size * 1000000) * prop));
        return cCalc / 1000000.0;
    };

    double musicRunningTime = getRunningMusicTimeUpd(player) - 4.0;
    double cCalc = positionCalcByT(musicRunningTime, (x2 - x1));

    CEngine::RenderDoubleStruct FretBoardStruct{};

    FretBoardStruct.x1 = x1;
    FretBoardStruct.x2 = x2;
    FretBoardStruct.x3 = x3;
    FretBoardStruct.x4 = x4;

    FretBoardStruct.TextureX1 = 0.0;
    FretBoardStruct.TextureX2 = 1.0;

    FretBoardStruct.TextureY1 = 0.0;
    FretBoardStruct.TextureY2 = 1.0;

    FretBoardStruct.y1 = -0.5;
    FretBoardStruct.y2 = FretBoardStruct.y1;
    FretBoardStruct.y3 = FretBoardStruct.y1;
    FretBoardStruct.y4 = FretBoardStruct.y1;

    fretboardLData.texture = FretBoardStruct.Text = Text;

    const double x2mx1nsize = (x2 - x1) * (-size);

    for (intptr_t i = -2; i < 12; i++) {
        FretBoardStruct.z1 = x2mx1nsize * i - cCalc;
        FretBoardStruct.z2 = FretBoardStruct.z1;
        FretBoardStruct.z3 = FretBoardStruct.z1 + x2mx1nsize;
        FretBoardStruct.z4 = FretBoardStruct.z3;

        FretBoardStruct.alphaBottom = pos2Alpha(-FretBoardStruct.z3 / 5.5);
        FretBoardStruct.alphaTop = pos2Alpha(-FretBoardStruct.z2 / 5.5);

        bool cont = false;

        if (FretBoardStruct.alphaBottom <= 0.0) {
            FretBoardStruct.alphaBottom = 0.0;

            FretBoardStruct.z3 = 1.6 * -5.5;
            FretBoardStruct.z4 = FretBoardStruct.z3;

            FretBoardStruct.TextureY2 =
                ((FretBoardStruct.z3 - FretBoardStruct.z1) / x2mx1nsize);
            cont = true;
        }

        CEngine::pushQuad(fretboardLData, FretBoardStruct);

        if (cont) {
            break;
        }
    }

    CEngine::engine().drawTrianglesWithAlpha(fretboardLData);
}

void CGamePlay::renderPlayerPylmBar(const CPlayer &player) {
    CEngine::engine().renderAt(0.0, -0.5, 1.1);

    player.guitar->gameplayBar.draw(player.guitar->textureID, false);
}

void CGamePlay::renderPylmBar() {
    CEngine::engine().renderAt(0.0, -0.5, 1.1);

    GPPGame::GuitarPP().pylmbarOBJ.draw(GPPGame::GuitarPP().pylmBarText, false);
}

void CGamePlay::renderLyrics() {
    if (songlyrics.size() > songlyricsIndex) {
        double time = getPlayer(0).musicRunningTime;

        if (time > songlyrics[songlyricsIndex].tend) {
            ++songlyricsIndex;
            return;
        }

        if (time > songlyrics[songlyricsIndex].tstart) {
            std::string &s = songlyrics[songlyricsIndex].lyric;
            CFonts::fonts().drawTextInScreenWithBuffer(
                s, CFonts::fonts().getCenterPos(s, 0.1, 0.0), 0.6, 0.1);
        }
    }
}

void CGamePlay::loadSongLyrics(const std::string &song) {
    songlyrics.clear();
    songlyricsIndex = 0;

    std::string lyricFile = "data/songs/" + song + "/lyrics.srt";
    std::ifstream lyrics(lyricFile);

    auto deduceTime = [](const int *a) {
        double result = 0;
        result += (double)a[0] * 3600.0;
        result += (double)a[1] * 60.0;
        result += a[2];
        result += (double)a[3] / 1000.0;

        return result;
    };

    std::cout << lyricFile << " " << lyrics.is_open() << '\n';

    if (lyrics.is_open()) {
        char temp[1024] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        char temp2[128] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

        int s = 0;
        int id = 0;
        int result = 0;
        int sinc[8] = {0, 0, 0, 0, 0, 0, 0, 0};

        double offset = 0.0;
        double offt = NAN;

        lyrics.seekg(0, std::ios::beg);

        lyricLine l;

        while (lyrics.getline(temp, sizeof(temp))) {
            if (strlen(temp) == 0) {
                continue;
            }

            result = sscanf(temp, "%127s %lf", temp2, &offt);

            if (result == 2 && std::string(temp2) == "offset") {
                offset = offt;

                std::cout << "offset " << offt << '\n';
                continue;
            }

            switch (s++) {
            case 0:
                result = sscanf(temp, "%d", &id);

                if (result != 1) {
                    lyrics.close();
                }

                break;

            case 1:
                result = sscanf(temp, "%d:%d:%d,%d --> %d:%d:%d,%d", &sinc[0],
                                &sinc[1], &sinc[2], &sinc[3], &sinc[4],
                                &sinc[5], &sinc[6], &sinc[7]);

                if (result == 8) {
                    l.tstart = deduceTime(sinc) + offset;
                    l.tend = deduceTime(&sinc[4]) + offset;
                }

                break;

            case 2:
                l.lyric = temp;
                songlyrics.push_back(l);
                break;

            default:
                break;
            }

            if (s > 2) {
                s = 0;
            }
        }
    }
}

void CGamePlay::setHyperSpeed(double s) { speedMp = s; }

void CGamePlay::setMusicSpeed(double s) { gSpeed = s; }

void CGamePlay::renderPlayer(const CPlayer &player,
                             PlayerRenderState &renderState) {
    GPP_PROFILE_SCOPE(RenderPlayer);
    // Same 400 degrees/s clock and integer-degree appearance, sampled per
    // frame.
    renderState.rangle =
        std::floor(std::fmod(CEngine::getTime() * 400.0, 360.0));

    auto bpmlines = std::async(
        std::launch::async,
        [this,
         &renderState](const CPlayer &player) -> CEngine::dTriangleWithAlpha * {
            if (!showBPMLines) {
                return nullptr;
            }

            return drawBPMLines(player, renderState);
        },
        std::cref(player));

    lightData l0{};

    if (bFretboardLightFading) {
        if (fretboardLightFade > 1.0) {
            fretboardLightFade -= engine.getDeltaTime() * 4.0;
        } else {
            bFretboardLightFading = false;
        }
    }

    for (auto &t : l0.ambientLight) {
        t = 0.2F;

        if (bFretboardLightFading) {
            t /= fretboardLightFade;
        }
    }

    for (auto &t : l0.direction) {
        t = 2.5F;
    }

    for (auto &t : l0.position) {
        t = 0.0F;
    }

    for (auto &t : l0.specularLight) {
        t = 1.0F;
    }

    for (auto &t : l0.diffuseLight) {
        t = 1.0F;
    }

    l0.specularLight[3] = 1.0F;
    l0.diffuseLight[3] = 0.3F;

    l0.angle = 120.0F;
    l0.direction[0] = 0.0F;
    l0.direction[1] = -0.5F;
    l0.direction[2] = -5.0F;

    l0.position[3] = 1.0F;
    l0.position[1] = 0.0F;
    l0.position[2] = 2.5F;

    if (bFretboardLightFading) {
        l0.position[2] -= fretboardLightFade - 1.0;
    }

    bool renablel0 = false;

    if (player.bPlusStrike) {
        double difftime = CEngine::getTime() - player.plusThunterStrikeStart;

        if (difftime > 0.0 && difftime <= 4.0) {
            difftime /= 4.0;

            engine.activateLighting(true);

            lightData l{};

            for (auto &t : l.ambientLight) {
                t = static_cast<float>(difftime);
            }

            for (auto &t : l.direction) {
                t = 2.5F;
            }

            for (auto &t : l.position) {
                t = 0.0F;
            }

            for (auto &t : l.specularLight) {
                t = 1.0F;
            }

            for (auto &t : l.diffuseLight) {
                t = 0.2F;
            }

            l.specularLight[2] = 1.0F;
            l.specularLight[3] = 1.0F - static_cast<float>(difftime);
            l.diffuseLight[2] = 1.0F;
            l.diffuseLight[3] = 1.0F - static_cast<float>(difftime);
            l.ambientLight[3] = static_cast<float>(difftime);

            l.angle = 100.0F;
            l.direction[0] = 0.0F;
            l.direction[1] = -0.5F;
            l.direction[2] = -5.0F;

            l.position[3] = 1.0F;
            l.position[1] = 0.0F;
            l.position[2] = 2.5F;

            engine.activateLight(0, false);
            engine.activateLight(1, true);
            CEngine::setLight(l, 1);
            renablel0 = true;
        }
    }

    if (player.plusEnabled) {
        engine.activateLighting(true);

        lightData l{};

        for (auto &t : l.ambientLight) {
            t = 0.1F;
        }

        for (auto &t : l.direction) {
            t = 2.5F;
        }

        for (auto &t : l.position) {
            t = 0.0F;
        }

        for (auto &t : l.specularLight) {
            t = 1.0F;
        }

        for (auto &t : l.diffuseLight) {
            t = 0.2F;
        }

        l.specularLight[2] = 1.0F;
        l.specularLight[3] = 1.0F;
        l.diffuseLight[2] = 1.0F;
        l.diffuseLight[3] = 1.0F;
        l.ambientLight[3] = 0.1F;

        l.angle = 100.0F;
        l.direction[0] = 0.0F;
        l.direction[1] = -0.5F;
        l.direction[2] = -5.0F;

        l.position[3] = 1.0F;
        l.position[1] = 0.0F;
        l.position[2] = 2.5F;

        engine.activateLight(0, false);
        engine.activateLight(1, true);
        CEngine::setLight(l, 1);
    }

    {
        engine.setCamera(player.playerCamera);
    }

    engine.activate3DRender(true);

    CEngine::enableColorsPointer(true);

    if (bFretboardLightFading) {
        engine.activateLighting(true);
        engine.activateLight(1, false);
        engine.activateLight(0, true);

        CEngine::setLight(l0, 0);
    }

    unsigned int fretboardText =
        (player.guitar == nullptr || !player.guitar->isLoaded())
            ? GPPGame::GuitarPP().fretboardText
            : ((player.guitar->fretboardText != 0)
                   ? player.guitar->fretboardText
                   : GPPGame::GuitarPP().fretboardText);

    double fretboardData[] = {-0.51, 0.51, 0.51, -0.51, -1.0, -1.0, 0.4, 0.4};
    renderFretBoard(player, fretboardData[0], fretboardData[1],
                    fretboardData[2], fretboardData[3], fretboardText);

    if (showBPMLines) {
        auto *bpmlinesptr = bpmlines.get();
        if ((bpmlinesptr != nullptr) && !bpmlinesptr->vArray.empty()) {
            CEngine::engine().drawTrianglesWithAlpha(*bpmlinesptr);
        }
    }

    auto renderScene = [&]() {
        engine.setColor(1.0, 1.0, 1.0, 1.0);
        if (player.guitar == nullptr || !player.guitar->isLoaded()) {
            renderPylmBar();
        } else {
            renderPlayerPylmBar(player);
        }

        renderState.tailsData.clear();
        renderNotePlayer(player, renderState);
    };

    if (!bFretboardLightFading) {
        engine.activateLighting(true);

        {
            engine.activateLight(1, false);
            engine.activateLight(0, true);

            CEngine::setLight(l0, 0);
        }
    }

    CEngine::enableColorsPointer(false);

    CEngine::activateNormals(true);

    renderScene();

    engine.setColor(1.0, 1.0, 1.0, 1.0);

    for (size_t i = 0; i < 5; i++) {
        {
            renderIndivdualStrikeButton3D(i, 0.0, 0, 0.0, player);
        }
    }

    for (size_t i = 0; i < 5; i++) {
        double time = CEngine::getTime();
        double pressT = (time - player.Notes.fretsNotePickedTime[i]);
        double calcP = ((sin(pressT / 0.05)) / 15.0) - 0.05;

        if (pressT > 0.15 &&
            player.notesSlide[i] == ~static_cast<size_t>(0UL)) {
            calcP = -10.0;

            if (player.fretsPressed[i]) {
                calcP = -0.025;
            }
        }

        if (calcP < -0.035) {
            calcP = -0.035;
        } else if (calcP > -0.023) {
            {
                CParticle::particleData pd{};

                pd.duration = 0.25;

                double poscalc =
                    -0.41 + (double(i) * 0.2 / 48.0) + (double(i) * 0.2);

                pd.x = poscalc + (rand() % 21 - 10) / 2000.0;
                pd.y = -0.5 + calcP + STRIKE_FIRE_SURFACE;
                pd.z = 0.75 + (rand() % 21 - 10) / 2000.0;
                pd.crossedFlame = true;
                pd.emitter = static_cast<int>(i);
                pd.emitterHeight = pd.y;

                pd.ax = 0.0;
                pd.ay = 0.12;
                pd.az = 0.0;

                pd.sx = (rand() % 21 - 10) / 200.0;
                pd.sy = 0.08;
                pd.sz = 0.0;

                // Compact, broad bursts; the crossed planes narrow in
                // projection.
                pd.sizex = 0.13;
                pd.sizey = 0.11;
                pd.sizez = 0.00;

                pd.desac = 0.0;

                pd.texture = pfireText;

                pd.qtd = 1;

                renderState.playerParticles.emitFire(pd, time);
            }
        }

        renderState.playerParticles.moveEmitter(
            static_cast<int>(i), -0.5 + calcP + STRIKE_FIRE_SURFACE);
        renderIndivdualStrikeButton3DStrike(i, 0.0, 0, calcP, player);
    }

    engine.bindVBOBuffer(0);

    CEngine::activateNormals(false);
    CEngine::engine().matrixReset();

    engine.activateLighting(false);

    if (!renderState.playerParticles.part.empty()) {

        const double publicApproval =
            player.maxPublicAprov > 0
                ? static_cast<double>(player.publicAprov) /
                      static_cast<double>(player.maxPublicAprov)
                : 0.0;
        renderState.playerParticles.renderFire(
            player.plusEnabled, player.comboToMultiplier(), publicApproval);
    }

    engine.setColor(1.0, 1.0, 1.0, 1.0);

    renderTailsBuffer(player);
    // bPlusStrike also drives the existing four-second blue light fade.
    // Draw the short discharge here, after opaque geometry and with this
    // player's camera active, so the fretboard can occlude it correctly.
    if (player.bPlusStrike) {
        renderState.plusLightning.render(
            CEngine::getTime() - player.plusThunterStrikeStart,
            static_cast<unsigned int>(player.lastNotePlusType));
    }

    engine.activate3DRender(false);

    renderHoposLight();

    {
        CEngine::cameraSET usingCamera;
        usingCamera.eye.x = 0.0;
        usingCamera.eye.y = 0.0;
        usingCamera.eye.z = 2.3;
        usingCamera.center.x = 0.0;
        usingCamera.center.y = 0.0;
        usingCamera.center.z = 0.0;
        usingCamera.up.x = 0.0;
        usingCamera.up.y = 1.0;
        usingCamera.up.z = 0.0;

        engine.setCamera(usingCamera);
    }

    if (bRenderHUD) {

        CEngine::RenderDoubleStruct HUDBackground{};

        HUDBackground.Text = GPPGame::GuitarPP().HUDText;

        double neg = 0.1 + player.playerHudOffsetX;
        double negy = 0.05 + player.playerHudOffsetY;

        HUDBackground.x1 = -1.0 + neg;
        HUDBackground.x2 = -0.6 + neg;
        HUDBackground.x3 = -0.6 + neg;
        HUDBackground.x4 = -1.0 + neg;

        HUDBackground.y1 = 0.1875 + negy;
        HUDBackground.y2 = 0.1875 + negy;
        HUDBackground.y3 = -0.5 + negy;
        HUDBackground.y4 = -0.5 + negy;

        HUDBackground.TextureX1 = 0.0;
        HUDBackground.TextureX2 = 1.0;
        HUDBackground.TextureY1 = 1.0;
        HUDBackground.TextureY2 = 0.0;

        engine.Render2DQuad(HUDBackground);

        double multi = player.comboToMultiplierWM();
        double circleMultiPercent = multi >= 4.0 ? 1.0 : (multi - floor(multi));
        double circlePublicAprov = static_cast<double>(player.publicAprov) /
                                   static_cast<double>(player.maxPublicAprov);
        double circleLoadPercent = player.plusLoadB;
        double circlePercent = (player.plusPower / player.maxPlusPower);
        double correctNotes = 0.0;
        size_t gNotesSize = 0;

        {
            correctNotes = player.correctNotes;
            gNotesSize = player.Notes.gNotes.size();
        }

        engine.setColor(1.0, 1.0, 1.0, 1.0);

        if (circleMultiPercent > 0.0) {
            double zeroToOne = circleMultiPercent;

            engine.setColor(0.0, 0.4, 1.0, 1.0);
            engine.Render2DCircleBufferMax(-0.8 + neg, -0.31 + negy,
                                           circleMultiPercent, 0.01, 0.041, 200,
                                           renderState.multiplierBuffer);
        }

        if (gNotesSize > 0) {
            double musicTotalCorrect = (correctNotes / gNotesSize);

            if (musicTotalCorrect > 0.0) {
                engine.setColor(0.4, 1.0, 0.4, 1.0);
                engine.Render2DCircleBufferMax(
                    -0.8 + neg, -0.31 + negy, musicTotalCorrect, 0.05, 0.041,
                    400, renderState.correctNotesBuffer);
            }
        }

        if (circlePublicAprov > 0.0) {
            double zeroToOne = circlePublicAprov;

            engine.setColor(1.0 - (1.0 * zeroToOne), 1.0 * zeroToOne, 0.0, 1.0);
            engine.Render2DCircleBufferMax(-0.8 + neg, -0.31 + negy,
                                           circlePublicAprov, 0.09, 0.041, 600,
                                           renderState.publicApprovBuffer);
        }

        if (circleLoadPercent > circlePercent) {
            if (circleLoadPercent > 0.0) {
                double zeroToOne = circleLoadPercent;

                engine.setColor(0.4, 1.0, 0.4, 1.0);
                engine.Render2DCircleBufferMax(
                    -0.8 + neg, -0.31 + negy, circleLoadPercent, 0.13, 0.04,
                    1000, renderState.plusLoadBuffer);
            }

            if (circlePercent > 0.0) {
                double zeroToOne = circlePercent;

                engine.setColor(0.0, 1.0, 1.0, 1.0);
                engine.Render2DCircleBufferMax(-0.8 + neg, -0.31 + negy,
                                               circlePercent, 0.13, 0.04, 1000,
                                               renderState.plusCircleBuffer);
            }
        } else {
            if (circlePercent > 0.0) {
                double zeroToOne = circlePercent;

                engine.setColor(0.0, 1.0, 1.0, 1.0);
                engine.Render2DCircleBufferMax(-0.8 + neg, -0.31 + negy,
                                               circlePercent, 0.13, 0.04, 1000,
                                               renderState.plusCircleBuffer);
            }

            if (circleLoadPercent > 0.0) {
                double zeroToOne = circleLoadPercent;

                engine.setColor(0.4, 1.0, 0.4, 1.0);
                engine.Render2DCircleBufferMax(
                    -0.8 + neg, -0.31 + negy, circleLoadPercent, 0.13, 0.04,
                    1000, renderState.plusLoadBuffer);
            }
        }

        engine.setColor(1.0, 1.0, 1.0, 1.0);

        CFonts::fonts().drawTextInScreenWithBuffer(
            std::to_string(player.getCombo()), -0.974 + neg, 0.04 + negy, 0.1);
        CFonts::fonts().drawTextInScreenWithBuffer(
            std::to_string(player.getPoints()), -0.99 + neg, -0.1 + negy, 0.06);
        CFonts::fonts().drawTextInScreenWithBuffer(
            std::to_string((int)player.comboToMultiplier()), -0.85 + neg,
            -0.37 + negy, 0.1);
    }
}

void CGamePlay::playSongAudio() {
    std::vector<std::string> activeStems;
    for (const auto &player : players) {
        if (!player->instrumentAudioName.empty()) {
            activeStems.push_back(player->instrumentAudioName);
        }
    }
    getBPlayer().playSongAudio(activeStems);
}

bool CGamePlay::update() {
    GPP_PROFILE_SCOPE(Update);
#ifdef GPP_PROFILE_GAMEPLAY
    gpp::profile::interval();
    const auto lockStart = gpp::profile::Clock::now();
#endif
    std::lock_guard<std::mutex> l(GPPGame::playersMutex);
#ifdef GPP_PROFILE_GAMEPLAY
    gpp::profile::samples[gpp::profile::Mutex].add(
        gpp::profile::elapsed(lockStart));
#endif

    double now = CEngine::getTime();
    double delta = activeDelta.sample(now);
    updateLastTimeCalled = now;
    iFPSCount++;

    if (!players.empty() && now - lastiFPSUpdated > 1.0) {
        iFPS = iFPSCount;
        iFPSCount = 0;
        lastiFPSUpdated = now;

        // getBPlayer().seekSongAudio(getRunningMusicTime(getBPlayer()));
    }

    for (auto &pp : players) {
        auto &p = *pp;
        if (p.bUpdateP) {
            if (!bIsACharterGP) {
                {
                    p.musicRunningTime += delta * gSpeed;
                }
            }

            updatePlayer(p, delta);
        }
    }

    return std::all_of(players.begin(), players.end(), [](const auto &player) {
        return player->isSongChartFinished();
    });
}

// Caller has quiesced the update worker. Use the same duration for all clocks.
void CGamePlay::compensatePause(std::chrono::steady_clock::duration duration) {
    startSongTime += duration;
    const double seconds = std::chrono::duration<double>(duration).count();
    for (auto &player : players) {
        player->startTime += seconds;
    }
}

void CGamePlay::marathonUpdate() {
    double now = CEngine::getTime();
    double delta = activeDelta.sample(now);
    updateLastTimeCalled = now;
    iFPSCount++;

    if (now - lastiFPSUpdated > 1.0) {
        iFPS = iFPSCount;
        iFPSCount = 0;
        lastiFPSUpdated = now;
    }

    for (auto &pp : players) {
        auto &p = *pp;
        if (!bIsACharterGP) {
            {
                p.musicRunningTime += delta * gSpeed;
            }
        }

        if (p.bUpdateP && !p.isSongChartFinished()) {
            {
                updatePlayer(p, delta);
            }
        }
    }
}

void CGamePlay::resetModule() {
    fretboardLightFade = 20.0;
    bFretboardLightFading = true;

    for (auto &p : players) {
        p->resetSongThings();
    }

    players.clear();
    chartInstruments.clear();

    songlyrics.clear();

    songlyricsIndex = 0;
}

void CGamePlay::render() {
    GPP_PROFILE_SCOPE(Render);
    auto &game = GPPGame::GuitarPP();
    auto &engine = CEngine::engine();

    for (auto &p : players) {
        if (p->bRenderP) {
            {
                renderPlayer(*p, p->renderState);
            }
        }
    }

    if (game.showTextsTest) {
        CFonts::fonts().drawTextInScreenWithBuffer(
            std::to_string(engine.getFPS()) + " FPS", 0.8, 0.8, 0.1);

        static int lastInternalFPS = 0;
        static double lastInternalFPST = CEngine::getTime();

        double gtime = CEngine::getTime();

        if ((gtime - lastInternalFPST) > 1.0) {
            lastInternalFPST = gtime;
            lastInternalFPS = (int)(1.0 / (gtime - updateLastTimeCalled));
        }

        CFonts::fonts().drawTextInScreenWithBuffer(
            std::to_string(iFPS) + "i FPS", 0.8, 0.6, 0.1);
    }
}

auto CGamePlay::renderBackground() -> bool {
    {

        engine.activate3DRender(true);
        engine.activateLighting(true);

        {
            double centerx = 0.0;
            double centerz = -650.0;

            double rtime = getBPlayer().musicRunningTime / 10.0;
            double eyexcam = (sin(0) * 1.0) + centerx + (sin(rtime) * 1.0);
            double eyezcam = (cos(0) * 1.0) + centerz + (cos(rtime) * 1.0);

            CEngine::cameraSET usingCamera;

            usingCamera.eye.x = 3.0 + sin(rtime);
            usingCamera.eye.y = 2.5;
            usingCamera.eye.z = 1.0 + cos(rtime);
            usingCamera.center.x = 3.0;
            usingCamera.center.y = 0.5;
            usingCamera.center.z = -5;
            usingCamera.up.x = 0;
            usingCamera.up.y = 1;
            usingCamera.up.z = 0;

            engine.setCamera(usingCamera);
        }

        {
            lightData l{};

            for (auto &t : l.ambientLight) {
                t = 0.1F;
            }

            for (auto &t : l.direction) {
                t = 2.5F;
            }

            for (auto &t : l.position) {
                t = 0.0F;
            }

            for (auto &t : l.specularLight) {
                t = 0.2F;
            }

            for (auto &t : l.diffuseLight) {
                t = 0.2F;
            }

            l.specularLight[1] = 1.0F;
            l.specularLight[2] = 1.0F;
            l.diffuseLight[0] = 1.0F;
            l.diffuseLight[1] = 1.0F;
            l.ambientLight[3] = 0.1F;

            CEngine::colorRGBToArrayf(0xFFF6ED, l.diffuseLight);

            l.angle = 180.0F;
            l.direction[0] = 3.0F;
            l.direction[1] = -0.5F;
            l.direction[2] = -1.5F;

            l.position[0] = 3.0F;
            l.position[1] = 2.7F;
            l.position[2] = -1.5F;
            l.position[3] = 1.0F;

            engine.activateLight(0, false);
            engine.activateLight(1, true);
            CEngine::setLight(l, 1);
        }

        CEngine::activateNormals(true);
        CBackgroundRenderer::inst().draw(GPPGame::GuitarPP().testobj);
        CEngine::activateNormals(false);

        engine.activateLighting(false);
        engine.activate3DRender(false);

        engine.matrixReset();

        CEngine::clear3DBuffer();
    }

    return true;
}

void CGamePlay::startUpdateDelta() {
    activeDelta.reset();
    updateLastTimeCalled = CEngine::getTime();
}

CGamePlay::CGamePlay()
    : engine(CEngine::engine()), speedMp(2.5), gSpeed(1.0),
      fretboardLightFade(20.0), bFretboardLightFading(true), showBPMLines(true),
      bRenderHUD(true), plusNoteLight(hoposLight), songlyricsIndex(0),
      BPMLineText("v.tga"), bIsACharterGP(false), showBPMVlaues(false),
      updateLastTimeCalled(0.0), iFPS(0), iFPSCount(0) {
    auto &Lua = CLuaH::Lua();

    preRenderPlayerSEvent = Lua.idForCallbackEvent("preRenderPlayer");
    posRenderPlayerSEvent = Lua.idForCallbackEvent("posRenderPlayer");

    hopostp.reserve(384);

    auto &guitarPP = GPPGame::GuitarPP();
    hopoLightText =
        guitarPP.loadTexture("data/sprites", "hopolight.tga").getTextId();

    fireText = guitarPP.loadTexture("data/sprites", "flamea.tga").getTextId();
    pfireText = guitarPP.loadTexture("data/sprites", "fire.tga").getTextId();

    BPMTextID = guitarPP.loadTexture("data/sprites", BPMLineText).getTextId();

    for (auto &al : hoposLight.ambientLight) {
        al = 0.0;
    }

    for (auto &al : hoposLight.diffuseLight) {
        al = 1.0;
    }

    hoposLight.diffuseLight[3] = 0.2F;

    for (auto &al : hoposLight.specularLight) {
        al = 1.0;
    }

    hoposLight.specularLight[3] = 0.2F;

    plusNoteLight.diffuseLight[0] = 0.2F;
    plusNoteLight.diffuseLight[1] = 0.2F;
    plusNoteLight.specularLight[0] = 0.2F;
    plusNoteLight.specularLight[1] = 0.2F;

    hoposLight.angle = 90.0F;
}
