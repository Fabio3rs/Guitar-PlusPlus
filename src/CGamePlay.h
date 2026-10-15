#include "ActiveDelta.h"
#pragma once
#include <cstddef>
#include <future>
#ifndef _GUITAR_PP_CGAMEPLAY_h_
#define _GUITAR_PP_CGAMEPLAY_h_
// brmodstudio.forumeiros.com

#include "CParticle.h"
#include "CPlayer.h"
#include "GPPOBJ.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>

class CCharter;

class CGamePlay {
    friend CCharter;

    void renderFretBoard(const CPlayer &player, double x1, double x2, double x3,
                         double x4, unsigned int Text);
    void renderFretBoardShadow(const CPlayer &player, double x1, double x2, double x3,
                               double x4, unsigned int Text);
    // void renderIndivdualStrikeButton(int id, double pos, unsigned int
    // Texture, int state, const CPlayer &player);
    void renderIndivdualStrikeButton3D(int id, double pos, unsigned int Texture,
                                       double state, const CPlayer &player) const;
    void renderIndivdualStrikeButton3DStrike(size_t id, double pos,
                                             unsigned int Texture, double state,
                                             const CPlayer &player) const;
    static void renderIndivdualFlame(int id, double pos, unsigned int Texture,
                                     int state, double sizeproportion,
                                     const CPlayer &player);
    static double pos2Alpha(double pos);
    void updatePlayer(CPlayer &player, double deltatime);
    void renderPlayer(const CPlayer &player, PlayerRenderState &renderState);
    bool renderIndivdualNote(unsigned int texture,
                             CEngine::RenderDoubleStruct &TempStruct3D,
                             const CPlayer &player, bool rotated);
    bool renderOpenNote(double pos, unsigned int Texture, int type,
                        const CPlayer &player);
    void renderIndivdualNoteShadow(int id, double pos, unsigned int Texture,
                                   bool tail, const CPlayer &player);
    double getRunningMusicTime(const CPlayer &player) const;
    double getRunningMusicTimeUpd(const CPlayer &player) const;
    double time2Position(double Time, const CPlayer &player);
    void renderNoteNoAdd(CPlayer::NotesData::Note &note, const CPlayer &player);
    bool renderNote(const CPlayer::NotesData::Note &note, const CPlayer &player,
                    double ltimet, PlayerRenderState &renderState);
    void renderNotePlayer(const CPlayer &player, PlayerRenderState &renderState);
    void renderIndividualLine(int id, double pos1, double pos2,
                              unsigned int Texture, const CPlayer &player);
    void renderTimeOnNote(double pos, double time, const CPlayer &player);
    void renderHoposLight();
    void addTailToBuffer(const CPlayer::NotesData::Note &note, double pos1,
                         double pos2, double runningTime,
                         PlayerRenderState &renderState) const;
    void renderTailsBuffer(const CPlayer &player);
    static double getBPMAt(const CPlayer &player, double time);
    static size_t getBPMAtI(const CPlayer &player, double time);
    static CPlayer::NotesData::Note getBPMAtStruct(const CPlayer &player,
                                                   double time);
    static std::vector<CPlayer::NotesData::Note>::iterator
    getBPMAtIt(CPlayer &player, double time);
    static void alertLVLUP(const CPlayer &player);
    static void alertNotesStreak(const CPlayer &player);
    static bool alertTest(CFonts::textAlert &t);

    static void renderPylmBar();
    static void renderPlayerPylmBar(const CPlayer &player);

    CEngine::dTriangleWithAlpha *drawBPMLines(
        const CPlayer &Player, PlayerRenderState &renderState);
    void drawBPMLine(double position, unsigned int Texture, const CPlayer &Player);

    CEngine &engine;
    double speedMp, gSpeed;

  public:
    std::chrono::steady_clock::time_point startSongTime;
    double fretboardLightFade;
    unsigned int fireText, pfireText;
    unsigned int BPMTextID;
    unsigned int hopoLightText;

    bool enableTails{}, bFretboardLightFading;
    bool showBPMLines;
    bool bRenderHUD;

    // Render-owned scratch. BPMl is delegated to one joined async task at a
    // time; update never accesses these buffers.
    std::vector<gppVec3f> hopostp;
    CEngine::dTriangleWithAlpha BPMl, fretboardLData;

    lightData hoposLight{}, plusNoteLight{};

    CPlayersContainer_t players;
    std::vector<std::string> chartInstruments;

    double fretboardPositionCalcByT(double time, double prop,
                                    double *max = nullptr) const;

    static const int notesFlagsConst[5];

    struct lyricLine {
        double tstart, tend;
        std::string lyric;
    };

    std::deque<lyricLine> songlyrics;
    size_t songlyricsIndex;

    void renderLyrics();

    void setHyperSpeed(double s);
    void setMusicSpeed(double s);

    inline double getgSpeed() const { return gSpeed; }

    // fretsPosition fretsText;
    std::string fretsTextures, BPMLineText;

    inline CPlayer &getPlayer(size_t id = 0) { return *players[id]; }

    void playSongAudio();

    inline CPlayer &getBPlayer() { return *players.back(); }

    std::string backgroundTexture;

    void loadSongLyrics(const std::string &song);

    // static CGamePlay &gamePlay();

    // Returns completion while still holding the player-container lock.
    bool update();
    void compensatePause(std::chrono::steady_clock::duration duration);
    void marathonUpdate();
    void render();

    bool renderBackground();
    void resetModule();
    void startUpdateDelta();

    CGamePlay(const CGamePlay &) = default;
    CGamePlay(CGamePlay &&) = default;
    CGamePlay();

  private:
    int preRenderPlayerSEvent, posRenderPlayerSEvent;

    bool bIsACharterGP;

    bool showBPMVlaues;

    // Update-owned cadence. The optional debug HUD still reads iFPS and
    // updateLastTimeCalled without publication (preexisting race).
    ActiveDelta activeDelta;
    double updateLastTimeCalled, lastiFPSUpdated{};
    int iFPS, iFPSCount;
};

#endif
