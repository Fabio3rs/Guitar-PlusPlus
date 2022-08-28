#include "CChart.h"
#include "CLog.h"
#include <cereal/archives/binary.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/deque.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

const int CChart::notesEnum =
    nf_green | nf_red | nf_yellow | nf_blue | nf_orange;
const int CChart::notesEnumWithOpenNotes =
    nf_green | nf_red | nf_yellow | nf_blue | nf_orange | nf_open;

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

auto CChart::parseFeebackChart(std::istream &chartStream) -> bool {
    typedef std::map<std::string,
                     std::map<std::string, std::deque<std::string>>>
        parsedChart;
    double chartOffset = 0.0;

    parsedChart feedBackChartMap;

    struct SyncTrackBPM {
        double BPM, offset;

        auto operator<(const SyncTrackBPM &b) const -> bool {
            return offset < b.offset;
        }
        auto operator>(const SyncTrackBPM &b) const -> bool {
            return offset > b.offset;
        }
    };

    using noteContainer = std::vector<Note_t>;
    using BPMContainer = std::vector<SyncTrackBPM>;
    BPMContainer BPMs;

    auto parseFeedBackChart = [&chartStream](parsedChart &data) {
        auto &chart = chartStream;
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
                        bp.offset = std::stod(SyncTrack.first);

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
        }
    };

    auto pureBPMToCalcBPM = [this](double BPM) {
        return (BPM / 1000.0 * 3.2) * chartData.chartResolutionProp;
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
        size_t result = 0;
        size_t irbp = 0;

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

    auto noteRead = [&getNoteTime, &getRefBPM, &chartOffset](
                        noteContainer &NTS, const BPMContainer &BPMs,
                        parsedChart &chartMap, const std::string &difficulty) {
        std::vector<NoteInt_t> ntsI;
        for (auto &scopeData : chartMap[difficulty]) {
            char c[32] = {0};
            int i = 0;
            int j = 0;
            for (auto &inst : scopeData.second) {
                if (sscanf(inst.c_str(), "%31s %d %d", c, &i, &j) == 3) {
                    if (std::string(c) == "N") {
                        NoteInt_t nt;
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
                        NoteInt_t nt;
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

        chartData.chartResolutionProp =
            std::stod(Song["Resolution"][0]) / 192.0;
        chartData.songName = chk(Song["Name"], 0u);
        chartData.songArtist = chk(Song["Artist"], 0u);
        chartData.songCharter = chk(Song["Charter"], 0u);
        try {
            chartOffset = std::stod(Song["Offset"][0]);
        } catch (...) {
            chartOffset = 0;
        }

        std::cout << chartData.chartResolutionProp << std::endl;
    };

    parseFeedBackChart(feedBackChartMap);
    fillChartInfo(feedBackChartMap);
    BPMRead(BPMs, feedBackChartMap);

    noteContainer Nts;

    noteRead(Nts, BPMs, feedBackChartMap,
             "[ExpertSingle]"); // Default: "[ExpertSingle]"

    /*
    int bpdqpos = 0;
    for (auto &bpdq : BPMs)
    {
    std::cout << "BPM " << bpdq.BPM / 1000.0 << "  offset " << bpdq.offset << "
    " << getNoteTime(BPMs, bpdqpos, bpdq.offset) << std::endl; bpdqpos++;
    }
    */

    noteContainer Nts2;

    noteRead(Nts2, BPMs, feedBackChartMap, "[ExpertDoubleBass]");

    int p = 0;
    for (auto &BP : BPMs) {
        Note_t newNote;
        newNote.time = getNoteTime(BPMs, p, (int64_t)BP.offset);
        newNote.lTime = BP.BPM / 1000.0;

        if (newNote.time != 0.0) {
            newNote.time += chartOffset;
        }

        BPM.push_back(newNote);

        ++p;
    }

    auto roundTo = [](double num, double dec) {
        double v = pow(10.0, dec);
        num *= v;
        num = floor(num);
        num /= v;
        return num;
    };

    auto posProcess = [this](noteContainer &Nts, instrumentNotes &in) {
        auto &gNotes = in.gNotes;
        auto &gPlus = in.gPlus;

        auto hopoTest = [this, &gNotes](Note_t &note) {
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

                int type1 = note.type & notesEnumWithOpenNotes;
                int type2 = gNotes[i].type & notesEnumWithOpenNotes;

                if (type1 == type2) {
                    note.type |= nf_not_hopo;
                }
            } else {
                note.type |= nf_not_hopo;
            }
        };

        for (auto &Nt : Nts) {
            if (std::isnan(Nt.time) || std::isnan(Nt.lTime)) {
                continue;
            }

            if (Nt.type == -1) {
                plusNote_t plusT;

                plusT.time = Nt.time;
                plusT.lTime = Nt.lTime;
                plusT.type = 0;

                gPlus.push_back(plusT);
            } else {
                Note_t newNote;
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

        in.deducePlusLastNotes();
    };

    posProcess(Nts, instruments["[ExpertSingle]"]);
    posProcess(Nts2, instruments["[ExpertDoubleBass]"]);

    return true;
}

auto CChart::compileGppChart(const std::string &fileName) const -> bool {
    std::fstream out(fileName,
                     std::ios::out | std::ios::binary | std::ios::trunc);

    if (!out.is_open()) {
        return false;
    }

    {
        cereal::BinaryOutputArchive oarchive(out); // Create an output archive

        oarchive(*this);
    }

    out.close();

    return true;
}

auto CChart::loadToPlayerData(CPlayer &player,
                              const std::string &instrument) const -> bool {
    auto it = instruments.find(instrument.empty() ? player.instrument
                                                  : instrument);

    if (it != instruments.end()) {
        const auto &instrumentData = *it;
        player.Notes.unloadChart();
        player.Notes.BPM = BPM;
        player.Notes.gNotes = instrumentData.second.gNotes;
        player.Notes.gPlus = instrumentData.second.gPlus;

        player.songArtist = chartData.songArtist;
        player.songCharter = chartData.songCharter;
        player.songName = chartData.songName;
        player.Notes.chartResolutionProp = chartData.chartResolutionProp;

        return true;
    }

    return false;
}

auto CChart::loadToNotesData(CPlayer &player,
                             const std::string &instrument) const -> bool {
    auto it =
        instruments.find(instrument.empty() ? player.instrument : instrument);

    if (it != instruments.end()) {
        const auto &instrumentData = *it;
        player.Notes.unloadChart();
        player.Notes.BPM = BPM;
        player.Notes.gNotes = instrumentData.second.gNotes;
        player.Notes.gPlus = instrumentData.second.gPlus;

        player.songArtist = chartData.songArtist;
        player.songCharter = chartData.songCharter;
        player.songName = chartData.songName;
        player.Notes.chartResolutionProp = chartData.chartResolutionProp;

        return true;
    }

    return false;
}

auto CChart::open(const std::string &chartFile) -> bool {
    std::ifstream chart(chartFile);
    if (chart.is_open()) {
        {
            return parseFeebackChart(chart);
        }
    }

    return false;
}

auto CChart::openFromMemory(const char *chart) -> bool {
    std::stringstream schart(chart);
    return parseFeebackChart(schart);
}

CChart::CChart() {
    chartData.chartResolutionProp = 1.0;
    chartData.chartOffset = 0.0;
    chartData.gameCompiledDateTime = (__DATE__ " " __TIME__);

    BPM.reserve(200);
}

CChart::instrumentNotes::instrumentNotes() {
    present = false;

    gPlus.reserve(200);
    gNotes.reserve(10000);
}

void CChart::instrumentNotes::deducePlusLastNotes() {
    size_t plusPosTemp = 0;

    for (size_t i = 0, size = gNotes.size(); i < size; ++i) {
        const auto &note = gNotes[i];

        if (plusPosTemp < gPlus.size()) {
            auto &plusNote = gPlus[plusPosTemp];

            if (note.time >= plusNote.time &&
                note.time < (plusNote.time + plusNote.lTime)) {
                if (plusNote.firstNote == -1) {
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
