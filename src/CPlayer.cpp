#include "CPlayer.h"
#include "CChart.h"
#include "CEngine.h"
#include "CFonts.h"
#include "CLog.h"
#include "CMultiplayer.h"
#include "CText.h"
#include "GPPGame.h"
#include <algorithm>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>

const int CPlayer::notesEnum =
    nf_green | nf_red | nf_yellow | nf_blue | nf_orange;
const int CPlayer::notesEnumWithOpenNotes =
    nf_green | nf_red | nf_yellow | nf_blue | nf_orange | nf_open;

auto CPlayer::smartChartSearch(const std::string &path) -> std::string {
    auto file_exists = [](const std::string &fileName) {
        return std::fstream(fileName).is_open();
    };

    const std::string fullPath = std::string("data/songs/") + path;

    const std::string chartFormats[] = {path + ".gpp", path + ".chart",
                                        "Chart.gpp",   "Notes.gpp",
                                        "Chart.chart", "notes.chart"};

    std::string tmp;

    for (const auto &str : chartFormats) {
        tmp = GPPGame::caseInsensitiveSearchDir(fullPath.c_str(), true, false,
                                                str);

        if (!tmp.empty()) {
            {
                return tmp;
            }
        }
    }

    return "";
}

void CPlayer::muteInstrument() const {
    CEngine::engine().setSoundVolume(instrumentSound, 0.0f);
}

void CPlayer::unmuteInstrument() const {
    CEngine::engine().setSoundVolume(instrumentSound, 1.0f);
}

static auto trim(const std::string &str) -> std::string {
    size_t first = str.find_first_not_of(' ');
    size_t last = str.find_last_not_of(' ');

    if (first == std::string::npos) {
        {
            return str;
        }
    }

    if (last == std::string::npos) {
        {
            last = str.size();
        }
    }

    return str.substr(first, last - first + 1);
}

std::atomic<int> palhetaNpKey;

void CPlayer::update() {
    int i = 0;
    int pklast = palhetaKeyLast;

    if ((CEngine::engine().getTime() - npPsetted) > 0.01 && enableBot) {
        palhetaNpKey = 0;
    }

    bool clearFretsP = true;

    if (enableBot) {
        std::copy(fretsPressed, fretsPressed + 5, lastFretsPressed);

        // if (GPPGame::GuitarPP().showTextsTest)
        //	CFonts::fonts().drawTextInScreen("BOT PLAYING", -1.65, -0.9,
        // 0.05);

        for (bool &i : fretsPressed) {
            i = false;
        }

        bool dngslide = false;

        if (Notes.lastNotePicked != -1 && strklinent != -1) {
            auto &note = Notes.gNotes[Notes.lastNotePicked];

            double rtime = (note.time - musicRunningTime);

            if (rtime < 0.1 && rtime > -0.1) {
                for (int i = 0; i < 5; ++i) {
                    if ((note.type & (int)pow(2, i)) != 0) {
                        fretsPressed[i] = true;
                        clearFretsP = false;
                    }
                }
            } else {
                for (bool &i : fretsPressed) {
                    i = false;
                }
            }
        }

        for (int i = 0; i < 5; ++i) {
            if (notesSlide[i] != -1) {
                fretsPressed[i] = true;
                clearFretsP = false;
                dngslide = true;
            }
        }

        if (strklinent != -1 && !dngslide) {
            auto &note = Notes.gNotes[strklinent];

            strklinenttime = (note.time - musicRunningTime);

            if (strklinenttime >= -0.05 &&
                ((strklinenttime < 0.14 &&
                  ((note.type & notesFlags::nf_not_hopo) != 0)) ||
                 (strklinenttime < 0.04))) {
                for (int i = 0; i < 5; ++i) {
                    if ((note.type & (int)pow(2, i)) != 0) {
                        fretsPressed[i] = true;
                    } else {
                        fretsPressed[i] = false;
                    }
                }

                if (strklinenttime < 0.04 &&
                    ((note.type & notesFlags::nf_picked) == 0) &&
                    ((note.type & notesFlags::nf_failed) == 0) &&
                    ((note.type & notesFlags::nf_doing_slide) == 0) &&
                    ((note.type & notesFlags::nf_slide_picked) == 0)) {
                    palhetaKey = 1;
                    palhetaNpKey = palhetaKey;
                    npPsetted = CEngine::engine().getTime();
                } else {
                    palhetaKey = 0;
                    palhetaKeyLast = palhetaKey;
                }
            }
        } else {
            palhetaKey = 0;
            palhetaKeyLast = palhetaKey;
        }
    } else if (!remoteControls) {
        palhetaNpKey = palhetaKey =
            (static_cast<int>(CEngine::engine().getKey(
                                  GPPGame::GuitarPP().fretOneKey) != 0) ||
             (CEngine::engine().getKey(GPPGame::GuitarPP().fretTwoKey)) != 0);

        if (pklast == palhetaKey) {
            palhetaKey = 0;
        } else {
            palhetaKeyLast = palhetaKey;
        }

        std::copy(fretsPressed, fretsPressed + 5, lastFretsPressed);

        int countI = 0;

        double timeC = CEngine::engine().getTime();

        for (auto &f : fretsPressed) {
            bool ftemp = f;
            f = (CEngine::engine().getKey(GPPGame::GuitarPP().strumKeys[i++]) !=
                 0);

            if (ftemp != f) {
                palhetaKey = 0;

                if (f) {
                    {
                        fretsPressedTime[countI] = timeC;
                    }
                }
            }

            countI++;
        }
    } else {
#ifdef COMPILEMP
        auto &pData = CMultiplayer::sgetPlData();

        CMultiplayer::playersData pdata;

        for (int i = 0, size = pData.size(); i < size; i++) {
            if (plname == pData[i].name) {
                // std::cout << pData[i].keys << std::endl;
                pdata = pData[i];
                break;
            }
        }

        palhetaNpKey = palhetaKey = pdata.strum;

        if (pklast == palhetaKey) {
            palhetaKey = false;
        } else {
            palhetaKeyLast = palhetaKey;
        }

        std::copy(fretsPressed, fretsPressed + 5, lastFretsPressed);

        int keys[5];

        for (int j = 0; j < 5; j++) {
            keys[j] = 0;

            if (pdata.keys & (int)pow(2, j)) {
                keys[j] = 1;
            }
        }

        double timeC = CEngine::engine().getTime();

        for (auto &f : fretsPressed) {
            bool ftemp = f;
            f = keys[i++];

            if (ftemp != f) {
                palhetaKey = false;

                if (f)
                    fretsPressedTime[i] = timeC;
            }
        }
#endif
    }
}

auto CPlayer::smartSongSearch(const std::string &path) -> std::string {
    auto file_exists = [](const std::string &fileName) {
        return std::fstream(fileName).is_open();
    };

    const std::string fullPath = std::string("data/songs/") + path;

    const std::string songsFormats[] = {path + ".ogg", path + ".mp3",
                                        "song.ogg", "song.mp3"};

    std::string tmp;

    for (const auto &str : songsFormats) {
        tmp = GPPGame::caseInsensitiveSearchDir(fullPath.c_str(), true, false,
                                                str);

        if (!tmp.empty()) {
            {
                return tmp;
            }
        }
    }

    return "";
}

auto CPlayer::loadSongOnlyChart(const std::string &path) -> bool {
    std::string fullFilePath = (std::string("data/songs/") + path +
                                std::string("/") + smartChartSearch(path));

    bool isChartOpen = fullFilePath.find(".chart") != std::string::npos
                           ? Notes.loadFeedbackChart(fullFilePath.c_str())
                           : Notes.loadChart(fullFilePath.c_str());

    if (songAudioID != -1 && (songAudioID != 0)) {
        CEngine::engine().unloadSoundStream(songAudioID);
    }

    songAudioID = -1;

    return true;
}

auto CPlayer::loadSong(const std::string &path) -> bool {
    std::string fullFilePath = (std::string("data/songs/") + path +
                                std::string("/") + smartChartSearch(path));

    CLog::log().multiRegister("Smart chart search result: %0", fullFilePath);

    bool isChartOpen = fullFilePath.find(".chart") != std::string::npos
                           ? Notes.loadFeedbackChart(fullFilePath.c_str())
                           : Notes.loadChart(fullFilePath.c_str());

    if (songAudioID != -1 && (songAudioID != 0)) {
        CEngine::engine().unloadSoundStream(songAudioID);
    }

    if (instrumentSound != -1 && (instrumentSound != 0)) {
        CEngine::engine().unloadSoundStream(instrumentSound);
    }

    songAudioID = -1;

    if (isChartOpen) {
        CLog::log() << smartSongSearch(path);
        Notes.songFullPath = (std::string("data/songs/") + path +
                              std::string("/") + smartSongSearch(path));

        CLog::log() << "loadSoundStream result: " +
                           std::to_string(static_cast<int>(
                               CEngine::engine().loadSoundStream(
                                   Notes.songFullPath.c_str(), songAudioID)));
        std::string fullPath = std::string("data/songs/") + path;

        if (Notes.instrument == "[ExpertSingle]") {
            CLog::log() << "loadSoundStream instrumentSound result: " +
                               std::to_string(static_cast<int>(
                                   CEngine::engine().loadSoundStream(
                                       (fullPath + std::string("/") +
                                        GPPGame::caseInsensitiveSearchDir(
                                            fullPath.c_str(), true, false,
                                            "guitar.ogg"))
                                           .c_str(),
                                       instrumentSound)));
        } else if (Notes.instrument == "[ExpertDoubleBass]") {
            CLog::log() << "loadSoundStream instrumentSound result: " +
                               std::to_string(static_cast<int>(
                                   CEngine::engine().loadSoundStream(
                                       (fullPath + std::string("/") +
                                        GPPGame::caseInsensitiveSearchDir(
                                            fullPath.c_str(), true, false,
                                            "rhythm.ogg"))
                                           .c_str(),
                                       instrumentSound)));
        }
    }

    return true;
}

#include <iostream>

auto CPlayer::getLevel() const -> int {
    return static_cast<int>(log(experience));
}

auto CPlayer::NotesData::loadFeedbackChart(const char *chartFile) -> bool {
    typedef std::map<std::string,
                     std::map<std::string, std::deque<std::string>>>
        parsedChart;
    chartEnd = 0;
    chartFileName = chartFile;
    plusPos = 0;
    double chartOffset = 0.0;

    parsedChart feedBackChartMap;

    struct SyncTrackBPM {
        double BPM, offset;

        auto operator<(const SyncTrackBPM &b) const noexcept -> bool {
            return offset < b.offset;
        }
        auto operator>(const SyncTrackBPM &b) const noexcept -> bool {
            return offset > b.offset;
        }
    };

    using noteContainer = std::vector<Note>;
    using BPMContainer = std::vector<SyncTrackBPM>;
    noteContainer Ntsdata;
    BPMContainer BPMsdata;

    BPMsdata.reserve(64);

    auto parseFeedBackChart = [](parsedChart &data,
                                 const std::string &chartFile) {
        std::ifstream chart(chartFile);
        char temp[1024];

        std::string myScope = "nothing";

        while (chart.getline(temp, sizeof(temp))) {
            if (chart.fail()) {
                {
                    return;
                }
            }

            {
                char *tln = strchr(temp, '\n');
                if (tln != nullptr) {
                    {
                        *tln = '\0';
                    }
                }
                tln = strchr(temp, '\r');
                if (tln != nullptr) {
                    {
                        *tln = '\0';
                    }
                }
            }

            std::string str = temp;

            str = trim(str);

            if (str.c_str()[0] == '[') {
                myScope = str;
            } else {
                size_t s = str.find_first_of('=');
                size_t sB;
                size_t sC;
                size_t d = str.find_first_not_of('	');

                d = (d == std::string::npos) ? str.find_first_not_of(' ') : d;
                d = (d == std::string::npos) ? 0uL : d;

                if (s == std::string::npos) {
                    {
                        continue;
                    }
                }

                sB = str.find_first_of(' ', d);
                sB = (sB == std::string::npos || sB >= s) ? (s - 1) : (sB);

                std::string name;

                if (isblank(str[0]) == 0) {
                    d = 0;
                }

                name.insert(0, str, d, sB);

                name = trim(name);

                sC = str.find_first_not_of(' ', ++s);
                sC = sC == std::string::npos ? s : sC;

                std::string contents;
                contents.insert(0, str, sC, std::string::npos);

                data[myScope][name].push_back(contents);
            }
        }
    };

    auto BPMRead = [](BPMContainer &BPMs, parsedChart &chartMap) {
        for (auto &SyncTrack : chartMap["[SyncTrack]"]) {
            char c[32] = {0};
            int i = 0;
            for (auto &inst : SyncTrack.second) {
                if (sscanf(inst.c_str(), "%31s %d", c, &i) == 2) {
                    if (std::string(c) == "B") {
                        SyncTrackBPM bp{};
                        bp.BPM = i;
                        try {
                            bp.offset = std::stod(SyncTrack.first);
                        } catch (const std::exception &e) {
                            CLog::log().multiRegister(__FILE__
                                                      " exception: %0 LINE %1",
                                                      e, (int)__LINE__);
                        }
                        BPMs.push_back(bp);
                    }
                }
            }
        }

        std::sort(BPMs.begin(), BPMs.end());

        if (BPMs.empty()) {
            SyncTrackBPM bp{};
            bp.BPM = 120000;
            bp.offset = 0.0;

            BPMs.push_back(bp);
        }
    };

    auto pureBPMToCalcBPM = [this](double BPM) {
        return (BPM / 1000.0 * 3.2) * chartResolutionProp;
    };

    auto getNoteTime = [&pureBPMToCalcBPM](const BPMContainer &BPMs, size_t pos,
                                           int64_t off) {
        double timeT = 0.0;
        size_t i = 0;

        if (pos > (BPMs.size() - 1)) {
            pos = BPMs.size() - 1;
        }

        for (; i < pos; i++) {
            timeT += (BPMs[i + 1].offset - BPMs[i].offset) /
                     pureBPMToCalcBPM(BPMs[i].BPM);
        }

        /*if (off < BPMs[pos].offset)
        {
                timeT -= (BPMs[i + 1].offset - BPMs[i].offset) /
        pureBPMToCalcBPM(BPMs[i].BPM); return timeT + (off - BPMs[i].offset) /
        pureBPMToCalcBPM(BPMs[i].BPM);
        }*/

        return timeT +
               (off - BPMs[pos].offset) / pureBPMToCalcBPM(BPMs[pos].BPM);
    };

    auto getRefBPM = [](const BPMContainer &BPMs, int64_t tick) {
        int result = 0;
        int irbp = 0;

        for (const auto &b : BPMs) {
            if (b.offset < tick) {
                result = irbp;
            } else {
                break;
            }
            irbp++;
        }

        return result;
    };

    auto hopoTest = [this](Note &note) {
        size_t size = gNotes.size();
        size_t i;
        i = size - 1;
        if (size > 0) {
            size_t BPMsize = BPM.size();
            size_t BPMi;
            BPMi = BPMsize - 1;

            double BPMStepCalc = 60.0 / 120.0;

            if (BPMsize > 0) {
                BPMStepCalc = 60.0 / BPM[BPMi].lTime;
            }

            for (auto &b : BPM) {
                if (b.time > note.time) {
                    {
                        break;
                    }
                }

                BPMStepCalc = 60.0 / b.lTime;
            }

            BPMStepCalc /= 2.08;

            if ((note.time - gNotes[i].time) >= BPMStepCalc) {
                note.type |= nf_not_hopo;
            }

            int type1 = note.type & notesEnumWithOpenNotes;
            int type2 = gNotes[i].type & notesEnumWithOpenNotes;

            if (type1 == type2) {
                note.type |= nf_not_hopo;
            }
        } else {
            note.type |= nf_not_hopo;
        }
    };

    auto notesCountToReserve = [](const parsedChart &chartMap,
                         const std::string &difficulty) -> size_t {
        size_t noten_length = 0;
        auto chartmapscope = chartMap.find(difficulty);

        if (chartmapscope == chartMap.end())
        {
            return noten_length;
        }

        for (auto &scopeData : chartmapscope->second) {
            noten_length += scopeData.second.size();
        }

        if ((noten_length & 1) != 0)
        {
            noten_length++;
        }

        return noten_length;
    };

    auto noteRead = [&getNoteTime, &getRefBPM, &chartOffset](
                        noteContainer &NTS, const BPMContainer &BPMs,
                        parsedChart &chartMap, const std::string &difficulty) {
        std::vector<NoteInt> ntsI;
        for (auto &scopeData : chartMap[difficulty]) {
            char c[32] = {0};
            int i = 0;
            int j = 0;
            for (auto &inst : scopeData.second) {
                if (sscanf(inst.c_str(), "%31s %d %d", c, &i, &j) == 3) {
                    if (std::string(c) == "N") {
                        NoteInt nt;
                        nt.time = std::stoll(scopeData.first);
                        nt.lTime = j;
                        nt.type = i;

                        if (nt.type >= 0 && nt.type < 5) {
                            nt.type = (int)pow(2, nt.type);

                            if (nt.lTime > 0) {
                                {
                                    nt.type |= nf_slide;
                                }
                            }

                            if (!ntsI.empty()) {
                                auto &last = ntsI[ntsI.size() - 1];

                                if (last.time == nt.time) {
                                    last.type |= nt.type;
                                    last.type |= nf_not_hopo;
                                } else {
                                    /*auto noteType0 = last.type &
                                    notesEnumWithOpenNotes; auto noteType1 =
                                    nt.type & notesEnumWithOpenNotes;

                                    if (noteType0 == noteType1)
                                    {
                                            nt.type |= nf_not_hopo;
                                    }*/

                                    ntsI.push_back(nt);
                                }
                            } else {
                                ntsI.push_back(nt);
                            }
                        } else if (nt.type == 5) {
                            if (!ntsI.empty()) {
                                auto &last = ntsI[ntsI.size() - 1];
                                if (last.time == nt.time) {
                                    last.type |= nf_reverseStrumHopo;
                                }
                            }
                        } else if (nt.type == 6) {
                            if (!ntsI.empty()) {
                                auto &last = ntsI[ntsI.size() - 1];
                                if (last.time == nt.time) {
                                    last.type |= noteTap;
                                    last.type |= nf_not_hopo;
                                }
                            }
                        } else if (nt.type == 7) {
                            nt.type = nf_open;

                            nt.lTime = 0;

                            if (!ntsI.empty()) {
                                auto &last = ntsI[ntsI.size() - 1];
                                if (last.time == nt.time) {
                                    last.type = nt.type;
                                    last.type |= nf_not_hopo;
                                } else {
                                    ntsI.push_back(nt);
                                }
                            } else {
                                ntsI.push_back(nt);
                            }
                        }
                    }

                    if (std::string(c) == "S" && i == 2) {
                        NoteInt nt;
                        nt.time = std::stoll(scopeData.first);
                        nt.lTime = j;
                        nt.type = -1;

                        ntsI.push_back(nt);
                    }
                }
            }
        }

        std::sort(ntsI.begin(), ntsI.end());

        uint64_t loffset = ~((uint64_t)0uLL);
        double lnotet = -1.0;
        double llngnotet = -1.0;

        size_t BPM = 0;
        for (auto &nt : ntsI) {
            NTS.push_back({});

            auto &nt0 = NTS.back();
            nt0 = nt;

            uint64_t loffsettmp = nt.time;
            if (nt0.type == -1) {
                nt0.time = getNoteTime(BPMs, getRefBPM(BPMs, nt.time), nt.time);
                nt0.lTime =
                    getNoteTime(BPMs, getRefBPM(BPMs, loffsettmp + nt.lTime),
                                loffsettmp + nt.lTime) -
                    nt0.time;

                nt0.time += chartOffset;
                continue;
            }

            if (BPM < (BPMs.size() - 1)) {
                if (BPMs[BPM + 1].offset < nt.time) {
                    {
                        ++BPM;
                    }
                }
            }

            if (loffset == nt.time) {
                nt0.time = lnotet;
                nt0.lTime = llngnotet;
            } else {
                nt0.time = getNoteTime(BPMs, getRefBPM(BPMs, nt.time), nt.time);
                nt0.lTime =
                    getNoteTime(BPMs, getRefBPM(BPMs, loffsettmp + nt.lTime),
                                loffsettmp + nt.lTime) -
                    nt0.time;

                nt0.time += chartOffset;
            }

            lnotet = nt0.time;
            llngnotet = nt0.lTime;
            loffset = loffsettmp;
        }
    };

    auto fillChartInfo = [&](parsedChart &chartMap) {
        auto chk = [](std::deque<std::string> &s, size_t i) {
            std::string result;

            if (i < s.size()) {
                {
                    result = s[i];
                }
            }

            return result;
        };

        auto &Song = chartMap["[Song]"];

        try {
            chartResolutionProp = std::stod(Song["Resolution"][0]) / 192.0;
        } catch (const std::exception &e) {
            chartResolutionProp = 1.0;
            CLog::log().multiRegister(__FILE__ " exception: %0 LINE %1", e,
                                      static_cast<int>(__LINE__));
        }
        songName = chk(Song["Name"], 0u);
        songArtist = chk(Song["Artist"], 0u);
        songCharter = chk(Song["Charter"], 0u);
        try {
            chartOffset = std::stod(Song["Offset"][0]);
        } catch (...) {
            chartOffset = 0;
        }
    };

    parseFeedBackChart(feedBackChartMap, chartFile);

    fillChartInfo(feedBackChartMap);

    Ntsdata.reserve(notesCountToReserve(feedBackChartMap, instrument));

    BPMRead(BPMsdata, feedBackChartMap);
    noteRead(Ntsdata, BPMsdata, feedBackChartMap,
             instrument); // Default: "[ExpertSingle]"

    /*
    int bpdqpos = 0;
    for (auto &bpdq : BPMs)
    {
            std::cout << "BPM " << bpdq.BPM / 1000.0 << "  offset " <<
    bpdq.offset << "   " << getNoteTime(BPMs, bpdqpos, bpdq.offset) <<
    std::endl; bpdqpos++;
    }
    */
    int p = 0;
    for (auto &BP : BPMsdata) {
        Note newNote;
        newNote.time = getNoteTime(BPMsdata, p, (int64_t)BP.offset);
        newNote.lTime = BP.BPM / 1000.0;

        if (newNote.time != 0.0) {
            newNote.time += chartOffset;
        }

        BPM.push_back(newNote);

        ++p;
    }

    for (auto &Nt : Ntsdata) {
        if (std::isnan(Nt.time) || std::isnan(Nt.lTime)) {
            continue;
        }

        if (Nt.type == -1) {
            plusNote plusT;

            plusT.time = Nt.time;
            plusT.lTime = Nt.lTime;
            plusT.type = 0;

            gPlus.push_back(plusT);
        } else {
            Note newNote;
            newNote.time = Nt.time;
            newNote.lTime = Nt.lTime;
            newNote.type = Nt.type;

            hopoTest(newNote);

            if ((newNote.type & nf_reverseStrumHopo) != 0) {
                newNote.type ^= nf_not_hopo;
            }

            gNotes.push_back(newNote);
        }
    }
    deducePlusLastNotes();

    return true;
}

void CPlayer::NotesData::deducePlusLastNotes() {
    size_t plusPosTemp = 0;

    for (size_t i = 0, size = gNotes.size(); i < size; ++i) {
        const auto &note = gNotes[i];

        if (plusPosTemp < gPlus.size()) {
            auto &plusNote = gPlus[plusPosTemp];

            if (note.time >= (plusNote.time - 0.0001) &&
                note.time < (plusNote.time + plusNote.lTime)) {
                if (plusNote.firstNote == ~0uLL) {
                    plusNote.firstNote = i;
                }

                plusNote.lastNote = i;
            }

            if (note.time >= (plusNote.time + plusNote.lTime)) {
                ++plusPosTemp;
            }
        } else {
            break;
        }
    }
}

auto CPlayer::NotesData::getChartEnd(double offset) -> double {
    if (chartEnd > 0) {
        return chartEnd;
    }

    auto size = gNotes.size();

    if (size > 0) {
        auto &lastNote = gNotes[size - 1];

        double finalTime = lastNote.time + lastNote.lTime;
        finalTime += offset;

        chartEnd = finalTime;
        return finalTime;
    }

    chartEnd = 2;
    return 2.0;
}

auto CPlayer::NotesData::loadChart(const char *chartFile) -> bool {
    chartEnd = 0;
    chartFileName = chartFile;

    if (chartFileName.find(".gpp") != std::string::npos) {
        CChart chart;

        std::fstream chartf(chartFile, std::ios::binary | std::ios::in);

        try {
            cereal::BinaryInputArchive oarchive(chartf);

            oarchive(chart);

            chart.loadToNotesData(*this, instrument);

            return true;
        } catch (const std::exception &e) {
            CLog::log().multiRegister("Exception %0", e);
        }
    }

    CText ACTEXTChart(chartFile, true);

    std::deque<CText::field_t> &textArray = ACTEXTChart[""].fields;

    double ntInfoTime;
    double ntInfoLTime;
    int ntIDInfo;
    int scanResult;

    Note PlusNow;

    PlusNow.type = -1;

    auto plusFunc = [&PlusNow](Note &note) {
        if (PlusNow.type == -1) {
            {
                return;
            }
        }

        if (note.time >= PlusNow.time && note.time <= PlusNow.lTime) {
            note.type |= plus_mid;
        }
    };

    auto hopoTest = [this](Note &note) {
        size_t size = gNotes.size();
        size_t i;
        i = size - 1;
        if (size > 0) {
            size_t BPMsize = BPM.size();
            size_t BPMi;
            BPMi = BPMsize - 1;

            double BPMStepCalc = 60.0 / 120.0;

            if (BPMsize > 0) {
                BPMStepCalc = 60.0 / BPM[BPMi].lTime;
            }

            for (auto &b : BPM) {
                if (b.time > note.time) {
                    {
                        break;
                    }
                }

                BPMStepCalc = 60.0 / b.lTime;
            }

            BPMStepCalc /= 2.05;

            if ((note.time - gNotes[i].time) >= BPMStepCalc) {
                note.type |= nf_not_hopo;
            }

            int type1 = note.type & notesEnum;
            int type2 = gNotes[i].type & notesEnum;

            if (type1 == type2) {
                note.type |= nf_not_hopo;
            }
        } else {
            note.type |= nf_not_hopo;
        }
    };

    for (auto &i : textArray) {
        if (i.name[0] == 'N') {
            scanResult = sscanf(i.content.c_str(), "%d %lf %lf", &ntIDInfo,
                                &ntInfoTime, &ntInfoLTime);

            if (std::isnan(ntInfoTime) || std::isnan(ntInfoLTime)) {
                continue;
            }

            if (scanResult == 3) {
                if (ntIDInfo >= 0 && ntIDInfo < 5 && ntInfoTime > 0.0) {
                    if (gNotes.empty()) {
                        Note newNote;
                        newNote.time = ntInfoTime;
                        // newNote.unmodifiedTime = ntInfoTime;
                        newNote.lTime = ntInfoLTime;
                        newNote.type = (int)pow(2, ntIDInfo);
                        if (newNote.lTime > 0.0) {
                            {
                                newNote.type |= nf_slide;
                            }
                        }

                        plusFunc(newNote);
                        hopoTest(newNote);

                        gNotes.push_back(newNote);
                    } else {
                        if (gNotes[gNotes.size() - 1].time == ntInfoTime) {
                            gNotes[gNotes.size() - 1].type |=
                                (int)pow(2, ntIDInfo);
                            gNotes[gNotes.size() - 1].type |= nf_not_hopo;
                        } else {
                            Note newNote;
                            newNote.time = ntInfoTime;
                            // newNote.unmodifiedTime = ntInfoTime;
                            newNote.lTime = ntInfoLTime;
                            newNote.type = (int)pow(2, ntIDInfo);
                            if (newNote.lTime > 0.0) {
                                {
                                    newNote.type |= nf_slide;
                                }
                            }

                            plusFunc(newNote);
                            hopoTest(newNote);

                            gNotes.push_back(newNote);
                        }
                    }
                } else if (ntIDInfo == 5) {
                    PlusNow.type = 1;
                    PlusNow.time = ntInfoTime;
                    PlusNow.lTime = ntInfoLTime;
                } else if (ntIDInfo == 6) {
                    Note newNote;
                    newNote.time = ntInfoTime;
                    newNote.lTime = ntInfoLTime;

                    BPM.push_back(newNote);
                }
            }
        }
    }

    deducePlusLastNotes();

    return true;
}

void CPlayer::NotesData::unloadChart() {
    notePos = 0;
    lastNotePicked = -1;
    longNoteComb = 0;
    chartResolutionProp = 1.0;
    plusPos = 0;

    for (auto &fretNotePickedTime : fretsNotePickedTime) {
        fretNotePickedTime = 0.0;
    }

    for (auto &inLN : inLongNote) {
        inLN = false;
    }

    for (auto &LNID : longNoteID) {
        LNID = -1;
    }

    chartFileName.clear();
    gNotes.clear();
    BPM.clear();
    gPlus.clear();
}

void CPlayer::resetData() {
    correctNotes = 0;
    bRenderP = bUpdateP = true;
    bPlusStrike = false;
}

CPlayer::NotesData::NotesData() {
    notePos = 0;
    lastNotePicked = -1;
    longNoteComb = 0;
    chartResolutionProp = 1.0;
    plusPos = 0;
    BPMMinPosition = 0.0;
    chartEnd = 0;
    BPM.reserve(200);
    gPlus.reserve(200);
    gNotes.reserve(10000);

    instrument = "[ExpertSingle]";

    for (auto &fretNotePickedTime : fretsNotePickedTime) {
        fretNotePickedTime = 0.0;
    }

    for (auto &inLN : inLongNote) {
        inLN = false;
    }

    for (auto &LNID : longNoteID) {
        LNID = -1;
    }
}

void CPlayer::addPointsByDoingLongNote() {
    points += comboToMultiplier() * 50.0 * CEngine::engine().getDeltaTime();
    experience += 50.0 * comboToMultiplier() / 100.0;
}

void CPlayer::processError() {
    breakCombo();
    aError = true;
    muteInstrument();

    if (publicAprov > 0) {
        {
            publicAprov--;
        }
    }
}

void CPlayer::processErrorNonPickedB(size_t pos) {
    if (pos > 0 && pos < Notes.gNotes.size()) {
        auto &noteRef = Notes.gNotes[pos];
        double tMin = noteRef.time - 0.5;

        pos--;

        int count = 0;

        for (int64_t i = pos; i >= 0; i--) {
            auto &note = Notes.gNotes[i];
            if (note.time > tMin &&
                ((note.type & notesFlags::nf_picked) == 0) &&
                ((note.type & notesFlags::nf_failed) == 0) &&
                ((note.type & notesFlags::nf_doing_slide) == 0)) {
                note.type |= notesFlags::nf_failed;
                ++count;
            }
        }

        if (count > 0) {
            breakCombo();

            publicAprov -= count;

            if (publicAprov < 0) {
                publicAprov = 0.0;
            }
        }
    }
}

void CPlayer::releaseSong() {
    if (songAudioID != -1 && (songAudioID != 0)) {
        CEngine::engine().unloadSoundStream(songAudioID);
    }

    if (instrumentSound != -1 && (instrumentSound != 0)) {
        CEngine::engine().unloadSoundStream(instrumentSound);
    }

    songAudioID = -1;

    BPMNowBuffer = 0;
}

void CPlayer::breakCombo() { combo = 0; }

auto CPlayer::comboToMultiplierWM() -> double { return comboToMultiplier(); }

auto CPlayer::getCombo() const -> int64_t { return combo; }

auto CPlayer::getPoints() const -> int64_t {
    return static_cast<int64_t>(points);
}

auto CPlayer::isSongChartFinished() -> bool {
    size_t size = Notes.gNotes.size();
    if (size > 0) {
        auto &lastNote = Notes.gNotes[size - 1];

        double finalTime = lastNote.time + lastNote.lTime;

        if ((musicRunningTime - finalTime) > 3.0) {
            return true;
        }
    } else {
        return true;
    }

    return false;
}

auto CPlayer::comboToMultiplier() const -> double {
    double result = double(combo) / 10.0;

    result = result < 1.0 ? 1.0 : result;
    result = result > 4.0 ? 4.0 : result;

    return result * (static_cast<double>(plusEnabled) + 1.0);
}

auto CPlayer::getLastFretsPressedFlags() const -> int {
    int result = 0;
    for (int i = 0; i < 5; ++i) {
        if (lastFretsPressed[i]) {
            result |= static_cast<int>(pow(2, i));
        }
    }

    return result;
}

auto CPlayer::getFretsPressedFlags() const -> int {
    int result = 0;
    for (int i = 0; i < 5; ++i) {
        if (fretsPressed[i]) {
            result |= static_cast<int>(pow(2, i));
        }
    }

    return result;
}

void CPlayer::doNote(size_t i) {
    if ((Notes.gNotes[i].type & notesFlags::nf_picked) == 0) {
        ++combo;
        ++correctNotes;

        Notes.lastNotePicked = i;

        points += comboToMultiplier() * 50.0;

        publicAprov++;

        aError = false;

        unmuteInstrument();

        experience += 50.0 * comboToMultiplier() / 100.0;

        if (publicAprov > maxPublicAprov) {
            publicAprov = maxPublicAprov;
        }
    }
}

CPlayer::NotesData::~NotesData() { unloadChart(); }

void CPlayer::instrumentPlay() const {
    CEngine::engine().playSoundStream(instrumentSound);
}

void CPlayer::instrumentPause() const {
    CEngine::engine().pauseSoundStream(instrumentSound);
}

CPlayer::CPlayer(const char *name) {
    bPlusStrike = false;
    usarPalheta = GPPGame::GuitarPP().usarPalheta;
    correctNotesMarathon = 0;
    guitar = nullptr;
    npPsetted = 0;
    playerHudOffsetX = playerHudOffsetY = 0.0;
    remoteControls = false;
    bRenderP = bUpdateP = true;
    plname = name;
    songAudioID = -1;
    points = 0.0;
    combo = 0;
    startTime = CEngine::engine().getTime();
    plusEnabled = false;
    musicRunningTime = 0.0;
    rangle = 0;
    palhetaKeyLast = palhetaKey = 0;
    aError = false;
    lastHOPO = 0;
    enableBot = false;

    memset(notesSlide, -1, sizeof(notesSlide));
    memset(lastFretsPressed, 0, sizeof(lastFretsPressed));
    memset(fretsPressed, 0, sizeof(fretsPressed));
    memset(fretsPressedTime, 0, sizeof(fretsPressedTime));

    instrumentSound = 0;
    strklinenttime = -5.0;

    BPMNowBuffer = 0;

    plusThunterStrikeStart = plusPower = plusParticleEffectPosition =
        maxPlusPower = plusLoadF = plusLoadB = plusLoadInterval = 0.0;
    plusCircleBuffer = plusLoadBuffer = publicApprovBuffer =
        correctNotesBuffer = multiplierBuffer = ~0u;

    correctNotes = 0;

    maxPlusPower = 1.0;
    maxPublicAprov = 120.0;
    publicAprov = maxPublicAprov / 2.0;

    strklinent = ~(static_cast<size_t>(0));

    playerCamera.eye.x = 0.0;
    playerCamera.eye.y = 0.5;
    playerCamera.eye.z = 2.42;
    playerCamera.center.x = 0.0;
    playerCamera.center.y = -0.2;
    playerCamera.center.z = 0.0;
    playerCamera.up.x = 0.0;
    playerCamera.up.y = 1.0;
    playerCamera.up.z = 0.0;

    targetCamera = playerCamera;

    experience = 100.0;
    tailsData.reserve(30);
}

CPlayer::~CPlayer() noexcept {
    CEngine::engine().unloadSoundStream(instrumentSound);
    CEngine::engine().unloadSoundStream(songAudioID);
}

void CPlayer::resetSongThings() {
    CEngine::engine().unloadSoundStream(instrumentSound);
    CEngine::engine().unloadSoundStream(songAudioID);
    // buffer.clear();
    Notes.unloadChart();

    songAudioID = -1;
    points = 0.0;
    combo = 0;

    correctNotesMarathon = 0;

    memset(notesSlide, -1, sizeof(notesSlide));
    memset(lastFretsPressed, 0, sizeof(lastFretsPressed));
    memset(fretsPressed, 0, sizeof(fretsPressed));
    memset(fretsPressedTime, 0, sizeof(fretsPressedTime));

    instrumentSound = 0;
    strklinenttime = -5.0;

    BPMNowBuffer = 0;

    plusThunterStrikeStart = plusPower = plusParticleEffectPosition =
        maxPlusPower = plusLoadF = plusLoadB = plusLoadInterval = 0.0;
    plusCircleBuffer = plusLoadBuffer = publicApprovBuffer =
        correctNotesBuffer = multiplierBuffer = ~0uL;

    correctNotes = 0;

    maxPlusPower = 1.0;
    maxPublicAprov = 120.0;
    publicAprov = maxPublicAprov / 2.0;

    correctNotes = 0;
    strklinent = ~(static_cast<size_t>(0));
    musicRunningTime = 0.0;
    points = 0.0;
    combo = 0;
    plusEnabled = false;
    bPlusStrike = false;
}
