#pragma once
#ifndef _GPP_CCHARTER_H_
#define _GPP_CCHARTER_H_

#include "CChart.h"
#include "CEngine.h"
#include "CGamePlay.h"
#include "CPlayer.h"
#include "GPPGame.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

class CCharter {
    CGamePlay gpModule;
    CChart chart;
    double atMusicTime, divTime;
    bool bForwardBackwardK, bLeftRightK;
    bool fretKeys[5]{};
    int keypts;
    static void eraseNulls(CPlayer &player);
    bool processNewNote;
    std::atomic<bool> loading{}, continueLoading{}, loadThreadEnd{};
    std::atomic<double> readBPMPercent{}, readBPMAtSeconds{};
    std::string instrument;
    std::mutex loadBPMMutex;
    bool backToZero;

    int songAudioID{};

    CPlayer::NotesData::Note *workingNote;
    std::vector<CPlayer::NotesData::Note>::iterator workingNoteIt;
    std::vector<CPlayer::NotesData::Note> songBPM;

    void readSongBPM(unsigned int song);
    double getBPMAt(double time);

    static void loadThreadFun(CCharter &ch);
    void internalLoad();

    std::thread loadThread;

    static double songTimePassBPMBased(const BPM_t &BPMContainer, double pass,
                                       bool forward);

  public:
    void prepareDemoGamePlay(CGamePlay &gp);

    void preRender();
    void render();
    void renderInfo();
    void renderAll();

    CCharter();
    ~CCharter();
};

#endif
