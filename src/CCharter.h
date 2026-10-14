#pragma once
#ifndef GPP_CCHARTER_H
#define GPP_CCHARTER_H

#include "CGamePlay.h"
#include "charter/Document.h"
#include <atomic>
#include <mutex>
#include <optional>
#include <thread>

class GPPGame;

// Runtime adapter only. The shared document has no dependency on this host,
// and the game does not depend on the external Chart Studio or ImGui.
class CCharter {
    CGamePlay gpModule;
    gpp::charter::Editor editor;
    std::optional<uint64_t> workingNote;
    double atMusicTime = 0, divTime = .5;
    bool forwardHeld = false, divisionHeld = false, analysisHeld = false,
         applyHeld = false, undoHeld = false, redoHeld = false;
    int previousFrets = 0;
    std::atomic<bool> loading{false}, continueLoading{false}, bpmReady{false};
    std::atomic<double> readBPMPercent{0};
    std::mutex loadBPMMutex;
    std::vector<CChart::Note> songBPM;
    std::string analysisError, songPath;
    std::thread loadThread;

    void synchronizeNotes();
    void startAnalysis();
    void readSongBPM(const std::string &path);
    double beatSeconds() const;

  public:
    explicit CCharter(GPPGame &game);
    ~CCharter();
    void prepareDemoGamePlay(CGamePlay &gp);
    void preRender();
    void render();
    void renderInfo();
    void renderAll();
};
#endif
