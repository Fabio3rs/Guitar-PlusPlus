#pragma once
// brmodstudio.forumeiros.com
#ifndef __GUITARPP_CPLAYER_H_
#define __GUITARPP_CPLAYER_H_

#include "CEngine.h"
#include "CFonts.h"
#include "CGuitars.h"
#include "CParticle.h"
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

enum notesFlags {
    nf_green = 1,
    nf_red = 2,
    nf_yellow = 4,
    nf_blue = 8,
    nf_orange = 16,
    nf_not_hopo = 32,
    nf_doing_slide = 64,
    nf_failed = 128,
    nf_picked = 256,
    nf_slide = 512,
    nf_slide_picked = 1024,
    losted = 2048,
    plus_mid = 4096,
    plus_end = 8192,
    hopontstrmmd = 16384,
    strmstlrc = 32768,
    noteTap = 65536,
    nf_open = 131072,
    nf_reverseStrumHopo = 262144
};

enum playerTypes { local_main, local, lan, lan_main };

struct lineData {
    int type;
    double top;
    double bottom;
};

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

class CPlayer {
    friend CCharter;

  public:
    class NotesData {
        friend CCharter;
        void deducePlusLastNotes();

      public:
        size_t notePos, plusPos;
        size_t lastNotePicked;
        double fretsNotePickedTime[5]{};

        double getChartEnd(double offset = 2.0);

        struct NoteInt {
            uint64_t time, lTime;

            int type;

            inline bool operator<(const NoteInt &b) const noexcept {
                return time < b.time;
            }

            inline bool operator>(const NoteInt &b) const noexcept {
                return time > b.time;
            }

            template <class Archive> void load(Archive &archive) {
                archive(time, lTime, type);
            }

            template <class Archive> void save(Archive &archive) const {
                archive(time, lTime, type);
            }

            NoteInt() noexcept {
                time = 0uLL;
                lTime = 0uLL;
                type = 0;
            }
        };

        struct Note {
            double time;
            double lTime;

            int type;

            inline bool operator<(const Note &b) const noexcept {
                return time < b.time;
            }

            inline bool operator>(const Note &b) const noexcept {
                return time > b.time;
            }

            template <class Archive> void load(Archive &archive) {
                archive(time, lTime, type);
            }

            template <class Archive> void save(Archive &archive) const {
                archive(time, lTime, type);
            }

            Note(NoteInt &&n) noexcept {
                time = static_cast<double>(n.time);
                lTime = static_cast<double>(n.lTime);
                type = n.type;
            }

            Note(const NoteInt &n) noexcept {
                time = static_cast<double>(n.time);
                lTime = static_cast<double>(n.lTime);
                type = n.type;
            }

            Note() noexcept {
                time = 0.0;
                lTime = 0.0;
                type = 0;
            }
        };

        struct plusNote {
            double time, lTime /*, unmodifiedTime*/;
            int type;

            size_t firstNote, lastNote;

            inline bool operator<(const plusNote &b) const noexcept {
                return time < b.time;
            }

            inline bool operator>(const plusNote &b) const noexcept {
                return time > b.time;
            }

            template <class Archive> void load(Archive &archive) {
                archive(time, lTime, type, firstNote, lastNote);
            }

            template <class Archive> void save(Archive &archive) const {
                archive(time, lTime, type, firstNote, lastNote);
            }

            plusNote() noexcept {
                time = 0.0;
                lTime = 0.0;
                // unmodifiedTime = 0.0;
                type = 0;
                firstNote = ~0uLL;
                lastNote = 0;
            }
        };

        std::vector<Note> gNotes;
        std::vector<Note> BPM;
        std::vector<plusNote> gPlus;

        bool loadChart(std::string_view chartFile, CPlayer &player);
        bool loadFeedbackChart(std::string_view chartFile, CPlayer &player);
        void unloadChart();

        NotesData();
        ~NotesData();

        double BPMMinPosition, chartEnd;
        double chartResolutionProp;
    };

  private:
    /**/

    double points;
    int64_t combo;
    double experience;

  public:
    NotesData Notes;
    std::vector<lineData> tailsData;
    double rangle;
    double npPsetted;
    double updatedMusicRunningTime;

    int lastHOPO;
    size_t BPMNowBuffer;

    double musicRunningTime;
    bool lastFretsPressed[5]{};
    bool bRenderP, bUpdateP, bPlusStrike;
    bool fretsPressed[5]{};
    bool plusEnabled;
    bool canDoHOPO{};
    double fretsPressedTime[5]{};
    double strklinenttime;

    size_t notesSlide[5]{};
    size_t strklinent;

    int palhetaKeyLast;
    int palhetaKey;

    bool usarPalheta;

    bool enableBot;

    bool aError;

    bool remoteControls;

    double startTime, plusThunterStrikeStart, plusPower,
        plusParticleEffectPosition, maxPlusPower, plusLoadF, plusLoadB,
        plusLoadInterval;

    uint64_t publicAprov, correctNotes, correctNotesMarathon, maxPublicAprov;

    unsigned int plusCircleBuffer, plusLoadBuffer, publicApprovBuffer,
        correctNotesBuffer, multiplierBuffer;

    int songAudioID;
    int instrumentSound;

    double playerHudOffsetX, playerHudOffsetY;

    CParticle playerParticles;

    CGuitars::CGuitar *guitar;

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
        void *i;
        bool ready;

        inline MPInfo() noexcept {
            i = nullptr;
            ready = false;
        }

    } multiPlayerInfo;

    std::string instrument;
    std::string songName, songArtist, songCharter;
    std::string songFullPath;

    ///////////////////////////////// FUNCTIONS BELOW
    template <class Archive> void load(Archive &archive) {
        archive(playerName);
    }

    template <class Archive> void save(Archive &archive) const {
        archive(playerName);
    }

    static std::string smartChartSearch(const std::string &path);
    static std::string smartSongSearch(const std::string &path);

    void addPointsByDoingLongNote();
    void processErrorNonPickedB(size_t pos);
    void doNote(size_t i);
    double comboToMultiplier() const;
    double comboToMultiplierWM();

    int getLevel() const;

    void resetData();

    void breakCombo();
    void processError();
    void releaseSong();

    void update();

    void instrumentPlay() const;
    void instrumentPause() const;
    void muteInstrument() const;
    void unmuteInstrument() const;

    auto getFretsPressedFlags() const -> int;
    auto getLastFretsPressedFlags() const -> int;

    bool loadSong(const std::string &path);
    bool loadSongOnlyChart(const std::string &path);

    int64_t getCombo() const;
    int64_t getPoints() const;

    bool isSongChartFinished();

    CPlayer(const char *name);
    ~CPlayer() noexcept;
    void resetSongThings();

    inline CPlayer() : CPlayer("default") {}
};

typedef std::vector<std::shared_ptr<CPlayer>> CPlayersContainer_t;

#endif
