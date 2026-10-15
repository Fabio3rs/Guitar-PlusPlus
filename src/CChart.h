#pragma once
#ifndef _GUITARPP_CCHART_H_
#define _GUITARPP_CCHART_H_

#include "ChartSource.h"
#include <cereal/cereal.hpp>
#include <cstdint>
#include <deque>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

class CPlayer;
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

class CChart {
  public:
    // CPU-owned inspection data. Never part of historical .gpp archives.
    struct Metadata {
        std::string name, artist, charter;
        std::map<std::string, std::string> fields;
    };
    using FeedbackDocument =
        std::map<std::string, std::map<std::string, std::deque<std::string>>>;

    // Each player owns its own mutable buffers and playback cursors.
    class NotesData {
        friend CCharter;
        friend CChart;
        void deducePlusLastNotes();
        bool validateLoadedChart();

      public:
        size_t notePos, plusPos;
        size_t lastNotePicked;
        double fretsNotePickedTime[5]{};

        double getChartEnd(double offset = 2.0);
        double getChartEnd(double offset = 2.0) const;

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

        ChartSourceInfo source;

        bool loadChart(std::string_view chartFile, CPlayer &player);
        bool loadFeedbackChart(std::string_view chartFile, CPlayer &player);
        bool loadMidiChart(std::string_view chartFile, CPlayer &player);
        // Extended evidence is opt-in for offline tools, never part of .gpp.
        bool loadFeedbackChartCPU(std::string_view file,
                                  const std::string &instrument,
                                  Metadata &metadata,
                                  const FeedbackDocument *document = nullptr,
                                  bool includeExtendedEvidence = false);
        bool loadChartCPU(std::string_view file, const std::string &instrument,
                          Metadata &metadata);
        void unloadChart();

        NotesData();
        NotesData(const NotesData &) = default;
        NotesData &operator=(const NotesData &) = default;
        NotesData(NotesData &&) noexcept = default;
        NotesData &operator=(NotesData &&) noexcept = default;
        ~NotesData();

        // Computed on successful chart loading; moved with the note buffers.
        double lastNoteEnd = 0.0;
        double BPMMinPosition, chartEnd;
        double chartResolutionProp;
    };

    using Note = NotesData::Note;
    using NoteInt = NotesData::NoteInt;
    using plusNote = NotesData::plusNote;
    using BPM_t = std::vector<Note>;
    struct Inspection {
        Metadata metadata;
        std::vector<std::string> instruments, trackNames, identification;
        BPM_t bpm;
        double lastNoteEnd = 0;
        std::string error;
    };
    static Inspection inspect(const std::string &file);

  private:
    friend class cereal::access;

    struct instrumentNotes {
        friend class cereal::access;
        friend class CChart;

        bool present;

        std::vector<Note> gNotes;
        std::vector<plusNote> gPlus;

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
    static FeedbackDocument readFeedbackDocument(std::string_view file);

    template <class Archive> void load(Archive &archive) {
        archive(instruments, BPM, chartData);
    }

    template <class Archive> void save(Archive &archive) const {
        archive(instruments, BPM, chartData);
    }

  public:
    // Strip only the surrounding .chart string delimiters, preserving contents.
    static std::string unquoteSongMetadata(std::string value);

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

    // Cheap header check for text charts; binary archives have no separate
    // header.
    static bool validateChartHeader(const std::string &chartFile);
    static std::string smartChartSearch(const std::string &path);
    static std::vector<std::string>
    availableInstruments(const std::string &file);
    static bool hasLegacyTextHeader(const std::string &chartFile);

    bool open(const std::string &chartFile);
    bool openFromMemory(const char *chart);

    CChart();
    ~CChart() noexcept {}
};

// Compatibility aliases for existing editor and rendering consumers.
using Note_t = CChart::Note;
using NoteInt_t = CChart::NoteInt;
using plusNote_t = CChart::plusNote;
using BPM_t = CChart::BPM_t;

#endif
