#pragma once
// brmodstudio.forumeiros.com
#ifndef __GUITARPP_CPLAYER_H_
#define __GUITARPP_CPLAYER_H_

#include "CChart.h"
#include "CEngine.h"
#include "CFonts.h"
#include "CGuitars.h"
#include "CParticle.h"
#include "CPlusLightning.h"
#include "SongResult.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

class CCharter;

enum playerTypes { local_main, local, lan, lan_main };

struct lineData {
    int type;
    double top;
    double bottom;
};

using AtomicDouble = std::atomic<double>;

/*struct fretsPosition{
        double lineFretSize, columnSize;
        unsigned int notesTexture, strikeLineTexture;

        struct ps {
                int x, y;

                inline ps(int x, int y) {
                        this->x = x;
                        this->y = y;
                }

                inline ps() { }
        };

        ps FretIMGPos[4 * 8];

        static int sAIDTI(int state, int id)
        {
                return (state + 1) * 5 + id;
        }

        fretsPosition(const fretsPosition&) = default;
        fretsPosition(fretsPosition&&) = default;

        fretsPosition()
        {
                lineFretSize = 1.0 / 4.0;
                columnSize = 1.0 / 8.0;

                FretIMGPos[sAIDTI(0, 0)] = ps(0, 0);
                FretIMGPos[sAIDTI(0, 1)] = ps(1, 0);
                FretIMGPos[sAIDTI(0, 2)] = ps(2, 0);
                FretIMGPos[sAIDTI(0, 3)] = ps(3, 0);
                FretIMGPos[sAIDTI(0, 4)] = ps(4, 0);

                FretIMGPos[sAIDTI(1, 0)] = ps(5, 0);
                FretIMGPos[sAIDTI(1, 1)] = ps(6, 0);
                FretIMGPos[sAIDTI(1, 2)] = ps(7, 0);
                FretIMGPos[sAIDTI(1, 3)] = ps(0, 1);
                FretIMGPos[sAIDTI(1, 3)] = ps(0, 1);
                FretIMGPos[sAIDTI(1, 4)] = ps(1, 1);

                FretIMGPos[sAIDTI(2, 0)] = ps(2, 1);
                FretIMGPos[sAIDTI(2, 1)] = ps(3, 1);
                FretIMGPos[sAIDTI(2, 2)] = ps(4, 1);
                FretIMGPos[sAIDTI(2, 3)] = ps(5, 1);
                FretIMGPos[sAIDTI(2, 4)] = ps(6, 1);

                FretIMGPos[sAIDTI(3, 0)] = ps(7, 1);
                FretIMGPos[sAIDTI(3, 1)] = ps(0, 2);
                FretIMGPos[sAIDTI(3, 2)] = ps(1, 2);
                FretIMGPos[sAIDTI(3, 3)] = ps(2, 2);
                FretIMGPos[sAIDTI(3, 4)] = ps(3, 2);

                FretIMGPos[sAIDTI(4, 0)] = ps(4, 2);
                FretIMGPos[sAIDTI(4, 1)] = ps(5, 2);
                FretIMGPos[sAIDTI(4, 2)] = ps(6, 2);
                FretIMGPos[sAIDTI(4, 3)] = ps(7, 2);
                FretIMGPos[sAIDTI(4, 4)] = ps(0, 3);

                notesTexture = strikeLineTexture = 0;
        }
};*/

extern std::atomic<int> palhetaNpKey;

// Owned by the render thread (BPM generation is delegated and joined per frame).
// Reset only at quiescent song/editor boundaries. No authoritative gameplay data.
struct PlayerRenderState {
    std::vector<lineData> tailsData;
    size_t BPMNowBuffer{};
    double rangle{};
    unsigned int plusCircleBuffer = ~0U, plusLoadBuffer = ~0U,
        publicApprovBuffer = ~0U, correctNotesBuffer = ~0U,
        multiplierBuffer = ~0U;
    CParticle playerParticles;
    CPlusLightning plusLightning;

    PlayerRenderState() { tailsData.reserve(32); }
};

class CPlayer {
    friend CCharter;

  public:
    using NotesData = CChart::NotesData;

  private:
    /**/

    // The editor can open audio while creating an empty chart.
    bool loadSongAudio(const std::string &path);

    double points{};
    int64_t combo{};
    double experience{};
    double songScoreStart = 0.0;
    uint64_t songCorrectStart = 0;
    int64_t songMaxCombo = 0;

  public:
    // Update-owned hot state; read by rendering where needed (legacy races).
    size_t BPMNowUpdate{}; // Used on updatePlayer
    double npPsetted{};

    int lastHOPO{};

    double musicRunningTime{};
    bool lastFretsPressed[5]{};
    bool bRenderP{}, bUpdateP{}, bPlusStrike{};
    bool fretsPressed[5]{};
    bool plusEnabled{};
    bool canDoHOPO{};
    double fretsPressedTime[5]{};
    double strklinenttime{};

    int lastNotePlusType{};

    size_t notesSlide[5]{};
    size_t strklinent{};

    int palhetaKeyLast{};
    int palhetaKey{};

    bool usarPalheta{};

    bool enableBot{};

    bool aError{};

    bool remoteControls{};

    double startTime{}, plusThunterStrikeStart{}, plusPower{},
        plusParticleEffectPosition{}, maxPlusPower{};
    AtomicDouble plusLoadF{}, plusLoadB{}, plusLoadInterval{};

    uint64_t publicAprov{}, correctNotes{}, correctNotesMarathon{},
        maxPublicAprov{};

    // The vector headers and chart metadata are colder than judgment/input.
    NotesData Notes;

    int songAudioID{};
    int instrumentSound{};
    std::string instrumentAudioName;
    std::map<std::string, int> backingStems;
    std::vector<std::string> activeAudioStems;
    float backingVolume = 1.0f;

    void playSongAudio(const std::vector<std::string> &activeStems);
    void pauseSongAudio() const;
    void seekSongAudio(double seconds) const;
    void setSongVolume(float volume);

    double playerHudOffsetX, playerHudOffsetY;

    CGuitars::CGuitar *guitar{};

    CFonts::textAlert lvlUpAlert, notesStreakAlert;

    CEngine::cameraSET playerCamera, targetCamera;

    static constexpr int notesEnum =
        nf_green | nf_red | nf_yellow | nf_blue | nf_orange;
    static constexpr int notesEnumWithOpenNotes =
        nf_green | nf_red | nf_yellow | nf_blue | nf_orange | nf_open;

    std::string playerSave;
    std::string playerName;
    std::string plname;

    int playerType{};

    struct MPInfo {
        void *i{nullptr};
        bool ready{};

    } multiPlayerInfo{};

    std::string instrument;
    std::string songName, songArtist, songCharter;
    std::string songFullPath;

    // Kept after cold metadata so renderer writes do not interleave with input,
    // score and Plus fields. No cache-line alignment/extra allocation required.
    PlayerRenderState renderState;

    ///////////////////////////////// FUNCTIONS BELOW
    template <class Archive> void load(Archive &archive) {
        archive(playerName);
    }

    template <class Archive> void save(Archive &archive) const {
        archive(playerName);
    }

    static std::string smartChartSearch(const std::string &path);
    static std::string smartSongSearch(const std::string &path);

    void addPointsByDoingLongNote(double activeSeconds);
    [[nodiscard]] double getExperience() const { return experience; }
    void processErrorNonPickedB(size_t pos);
    void doNote(size_t i);
    [[nodiscard]] double comboToMultiplier() const;
    double comboToMultiplierWM() const;

    [[nodiscard]] int getLevel() const;

    void resetData();

    void resetCamera();

    void breakCombo();
    void breakPlus();
    void processError();
    void releaseSong();

    void update();

    void instrumentPlay() const;
    void instrumentPause() const;
    void muteInstrument() const;
    void unmuteInstrument() const;

    void setInstrument(std::string instr = "[ExpertSingle]");

    [[nodiscard]] auto getFretsPressedFlags() const -> int;
    [[nodiscard]] auto getLastFretsPressedFlags() const -> int;

    bool loadSong(const std::string &path);
    bool loadSongOnlyChart(const std::string &path);

    [[nodiscard]] int64_t getCombo() const;
    // Called by the main thread between songs, while gameplay is quiescent.
    void takeLoadedSong(CPlayer &source);
    void beginSongResult();
    [[nodiscard]] PlayerSongResult songResult() const;
    [[nodiscard]] int64_t getPoints() const;

    [[nodiscard]] bool isSongChartFinished() const;

    CPlayer(const char *name);
    ~CPlayer() noexcept;
    void resetSongThings();

    inline CPlayer() : CPlayer("default") {}
};

typedef std::vector<std::shared_ptr<CPlayer>> CPlayersContainer_t;

#endif
