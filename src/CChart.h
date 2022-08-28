#pragma once
#ifndef _GUITARPP_CCHART_H_
#define _GUITARPP_CCHART_H_

#include "CPlayer.h"
#include <cereal/cereal.hpp>
#include <deque>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

typedef CPlayer::NotesData::Note Note_t;
typedef CPlayer::NotesData::NoteInt NoteInt_t;
typedef CPlayer::NotesData::plusNote plusNote_t;
typedef std::vector<Note_t> BPM_t;

class CChart {
    friend class cereal::access;

    struct instrumentNotes {
        friend class cereal::access;
        friend class CChart;

        bool present;

        std::vector<Note_t> gNotes;
        std::vector<plusNote_t> gPlus;

        instrumentNotes();

        template <class Archive> void load(Archive &archive) {
            archive(present, gNotes, gPlus);
        }

        template <class Archive> void save(Archive &archive) const {
            archive(present, gNotes, gPlus);
        }

      private:
        void deducePlusLastNotes();
    };

    std::map<std::string, instrumentNotes> instruments;

    struct chartData_t {
        std::string chartFileName, songName, songArtist, songCharter;
        double chartOffset, chartResolutionProp;
        std::string gameCompiledDateTime;

        template <class Archive> void load(Archive &archive) {
            archive(chartFileName, songName, songArtist, songCharter,
                    chartOffset, chartResolutionProp, gameCompiledDateTime);
        }

        template <class Archive> void save(Archive &archive) const {
            archive(chartFileName, songName, songArtist, songCharter,
                    chartOffset, chartResolutionProp, gameCompiledDateTime);
        }
    } chartData;

    BPM_t BPM;

    bool parseFeebackChart(std::istream &chartStream);

    template <class Archive> void load(Archive &archive) {
        archive(instruments, BPM, chartData);
    }

    template <class Archive> void save(Archive &archive) const {
        archive(instruments, BPM, chartData);
    }

  public:
    instrumentNotes &getInstrument(const std::string &instrument) {
        return instruments[instrument];
    }

    BPM_t &getBPMContainer() { return BPM; }

    bool compileGppChart(const std::string &fileName) const;

    bool loadToPlayerData(CPlayer &player,
                          const std::string &instrument = "") const;
    bool loadToNotesData(CPlayer &player,
                         const std::string &instrument = "") const;

    static const int notesEnum, notesEnumWithOpenNotes;

    bool open(const std::string &chartFile);
    bool openFromMemory(const char *chart);

    CChart();
    ~CChart() noexcept {}
};

#endif
