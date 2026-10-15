#include "CChart.h"
#include "CLog.h"
#include "CMidiChart.h"
#include "CPlayer.h"
#include "CText.h"
#include "ChartHopo.h"
#include "GPPGame.h"
#include "SongFiles.h"
#include <algorithm>
#include <cctype>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/deque.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

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

std::string CChart::unquoteSongMetadata(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);

    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }

    return value;
}

bool CChart::hasLegacyTextHeader(const std::string &chartFile) {
    std::ifstream input(chartFile, std::ios::binary);
    std::string line;
    while (std::getline(input, line)) {
        // Old .gpp files contain N records rather than a cereal archive.
        // Reject binary bytes before trying the permissive legacy text reader.
        if (std::any_of(line.begin(), line.end(), [](unsigned char c) {
                return c < 32 && c != '\t' && c != '\r';
            })) {
            return false;
        }
        std::istringstream record(line);
        std::string name;
        if (!(record >> name)) {
            continue;
        }
        if (name.front() != 'N') {
            continue;
        }
        int id = -1;
        double time = 0, length = 0;
        return name.front() == 'N' && (record >> id >> time >> length) &&
               id >= 0 && id <= 6 && std::isfinite(time) &&
               std::isfinite(length);
    }
    return false;
}

// Cereal's native binary format has no magic. Reject an impossible initial map
// count/key size before it can request an enormous allocation for arbitrary
// text. This is a prefix check, not a replacement for archive deserialization.
static bool hasBinaryChartPrefix(std::istream &input) {
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    input.seekg(0);
    cereal::size_type count = 0, keySize = 0;
    input.read(reinterpret_cast<char *>(&count), sizeof(count));
    input.read(reinterpret_cast<char *>(&keySize), sizeof(keySize));
    const bool valid =
        input && size >= 0 && count > 0 && keySize > 0 &&
        count <= static_cast<uint64_t>(size) / (sizeof(count) + 1) &&
        keySize <= static_cast<uint64_t>(size) - 2 * sizeof(count);
    input.clear();
    input.seekg(0);
    return valid;
}

bool CChart::validateChartHeader(const std::string &chartFile) {
    try {
        std::ifstream input(chartFile, std::ios::binary);
        if (!input) {
            return false;
        }
        auto extension = std::filesystem::path(chartFile).extension().string();
        std::transform(
            extension.begin(), extension.end(), extension.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".gpp") {
            if (hasLegacyTextHeader(chartFile)) {
                return true;
            }
            if (!hasBinaryChartPrefix(input)) {
                return false;
            }
            // The historical archive stores notes before metadata, so reading
            // metadata alone is not possible without changing the file format.
            CChart chart;
            cereal::BinaryInputArchive archive(input);
            archive(chart);
            return !chart.instruments.empty() && !chart.BPM.empty() &&
                   std::isfinite(chart.chartData.chartResolutionProp) &&
                   chart.chartData.chartResolutionProp > 0.0 &&
                   std::isfinite(chart.chartData.chartOffset) &&
                   std::all_of(
                       chart.BPM.begin(), chart.BPM.end(), [](const Note_t &b) {
                           return std::isfinite(b.time) &&
                                  std::isfinite(b.lTime) && b.lTime > 0.0;
                       });
        }
        if (extension == ".mid") {
            MidiChart midi;
            std::string error;
            return readMidiChart(chartFile, midi, error) &&
                   !midi.instruments.empty();
        }
        if (extension != ".chart") {
            return false;
        }

        auto strip = [](const std::string &line) {
            const auto first = line.find_first_not_of(" \t\r");
            return first == std::string::npos
                       ? std::string()
                       : line.substr(first, line.find_last_not_of(" \t\r") -
                                                first + 1);
        };
        bool songSection = false;
        bool opened = false;
        std::string line;
        while (std::getline(input, line)) {
            line = strip(line);
            if (line.empty() || line.compare(0, 2, "//") == 0) {
                continue;
            }
            if (!songSection) {
                if (line == "[Song]") {
                    songSection = true;
                }
                continue;
            }
            if (!opened) {
                if (line != "{") {
                    return false;
                }
                opened = true;
                continue;
            }
            if (line == "}") {
                return true;
            }
            if (line.front() == '[') {
                return false;
            }
            const auto equals = line.find('=');
            if (equals == std::string::npos) {
                return false;
            }
            const auto key = strip(line.substr(0, equals));
            if (key == "Resolution" || key == "Offset") {
                const auto value = strip(line.substr(equals + 1));
                size_t used = 0;
                const double number = std::stod(value, &used);
                const auto suffix = strip(value.substr(used));
                if (!std::isfinite(number) ||
                    (key == "Resolution" && number <= 0.0) ||
                    (!suffix.empty() && suffix.compare(0, 2, "//") != 0)) {
                    return false;
                }
            }
        }
    } catch (const std::exception &e) {
        CLog::log() << "Cannot inspect chart " + chartFile + ": " + e.what();
    }
    return false;
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
            std::stod(chk(Song["Resolution"], 0)) / 192.0;
        chartData.songName = unquoteSongMetadata(chk(Song["Name"], 0u));
        chartData.songArtist = unquoteSongMetadata(chk(Song["Artist"], 0u));
        chartData.songCharter = unquoteSongMetadata(chk(Song["Charter"], 0u));
        try {
            chartOffset = std::stod(chk(Song["Offset"], 0));
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

// Runtime loaders moved from CPlayer without changing parsing rules.
std::vector<std::string> CChart::availableInstruments(const std::string &file) {
    return inspect(file).instruments;
}

auto CChart::smartChartSearch(const std::string &path) -> std::string {
    const auto directory =
        std::filesystem::path("data/songs") / std::filesystem::u8path(path);
    try {
        for (const auto &file :
             songfiles::charts(directory, songfiles::list(directory))) {
            if (!availableInstruments(file.u8string()).empty()) {
                return file.filename().u8string();
            }
        }
    } catch (const std::filesystem::filesystem_error &error) {
        CLog::log() << error.what();
    }
    return {};
}

static auto trimRuntimeChart(const std::string &str) -> std::string {
    size_t first = str.find_first_not_of(" \t\r");
    size_t last = str.find_last_not_of(" \t\r");

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

CChart::FeedbackDocument
CChart::readFeedbackDocument(std::string_view chartFile) {
    FeedbackDocument data;
    std::ifstream chart((std::string(chartFile)));
    if (!chart) {
        throw std::runtime_error("Cannot open chart");
    }
    char temp[1024];

    std::string myScope = "nothing";
    bool scopeOpen = false;
    bool awaitingBrace = false;

    while (chart.getline(temp, sizeof(temp))) {
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

        str = trimRuntimeChart(str);

        if (str.empty() || str.compare(0, 2, "//") == 0) {
            continue;
        }
        if (str == "{") {
            if (!awaitingBrace) {
                throw std::runtime_error("Unexpected chart section");
            }
            awaitingBrace = false;
            scopeOpen = true;
            continue;
        }
        if (str == "}") {
            if (!scopeOpen) {
                throw std::runtime_error("Unexpected chart section end");
            }
            scopeOpen = false;
            continue;
        }
        if (str.front() == '[') {
            if (scopeOpen || awaitingBrace || str.back() != ']') {
                throw std::runtime_error("Invalid chart section");
            }
            myScope = str;
            awaitingBrace = true;
        } else {
            if (!scopeOpen) {
                throw std::runtime_error("Missing chart section");
            }
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

            name = trimRuntimeChart(name);

            sC = str.find_first_not_of(' ', ++s);
            sC = sC == std::string::npos ? s : sC;

            std::string contents;
            contents.insert(0, str, sC, std::string::npos);

            data[myScope][name].push_back(contents);
        }
    }
    if (!chart.eof() || scopeOpen || awaitingBrace) {
        throw std::runtime_error("Cannot read complete chart");
    }
    return data;
}

bool CChart::NotesData::loadFeedbackChart(std::string_view file,
                                          CPlayer &player) {
    Metadata metadata;
    const bool valid = loadFeedbackChartCPU(file, player.instrument, metadata);
    player.songName = std::move(metadata.name);
    player.songArtist = std::move(metadata.artist);
    player.songCharter = std::move(metadata.charter);
    return valid;
}

bool CChart::NotesData::loadFeedbackChartCPU(std::string_view chartFile,
                                             const std::string &instrument,
                                             Metadata &metadata,
                                             const FeedbackDocument *document,
                                             bool includeExtendedEvidence) try {
    unloadChart();
    if (!document && !CChart::validateChartHeader(std::string(chartFile))) {
        CLog::log() << "Invalid chart header: " + std::string(chartFile);
        return false;
    }
    using parsedChart = FeedbackDocument;
    chartEnd = 0;
    plusPos = 0;
    double chartOffset = 0.0;

    parsedChart feedBackChartMap =
        document ? *document : CChart::readFeedbackDocument(chartFile);

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
        double step = BPM.empty() ? 60.0 / 120.0 : 60.0 / BPM.back().lTime;
        for (const auto &bpm : BPM) {
            if (bpm.time > note.time) {
                break;
            }
            step = 60.0 / bpm.lTime;
        }
        step /= 2.08;
        const auto *previous = gNotes.empty() ? nullptr : &gNotes.back();
        chart::classifyHopo(note, previous,
                            previous && note.time - previous->time < step,
                            false);
    };

    auto notesCountToReserve = [](const parsedChart &chartMap,
                                  const std::string &difficulty) -> size_t {
        size_t noten_length = 0;
        auto chartmapscope = chartMap.find(difficulty);

        if (chartmapscope == chartMap.end()) {
            return noten_length;
        }

        for (auto &scopeData : chartmapscope->second) {
            noten_length += scopeData.second.size();
        }

        if ((noten_length & 1) != 0) {
            noten_length++;
        }

        return noten_length;
    };

    auto noteRead = [this, &getNoteTime, &getRefBPM, &chartOffset](
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
                        nt.time = std::stoull(scopeData.first);
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
                        nt.time = std::stoull(scopeData.first);
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
            if (nt0.type != -1) {
                source.noteTicks.push_back(static_cast<int64_t>(nt.time));
            }
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
        for (const auto &entry : Song) {
            if (!entry.second.empty()) {
                metadata.fields[entry.first] =
                    CChart::unquoteSongMetadata(entry.second.front());
            }
        }

        try {
            chartResolutionProp = std::stod(chk(Song["Resolution"], 0)) / 192.0;
        } catch (const std::exception &e) {
            chartResolutionProp = 1.0;
            CLog::log().multiRegister(__FILE__ " exception: %0 LINE %1", e,
                                      static_cast<int>(__LINE__));
        }
        metadata.name = CChart::unquoteSongMetadata(chk(Song["Name"], 0u));
        metadata.artist = CChart::unquoteSongMetadata(chk(Song["Artist"], 0u));
        metadata.charter =
            CChart::unquoteSongMetadata(chk(Song["Charter"], 0u));
        try {
            chartOffset = std::stod(chk(Song["Offset"], 0));
        } catch (...) {
            chartOffset = 0;
        }
    };

    fillChartInfo(feedBackChartMap);

    Ntsdata.reserve(notesCountToReserve(feedBackChartMap, instrument));

    BPMRead(BPMsdata, feedBackChartMap);
    if (!std::isfinite(chartResolutionProp) || chartResolutionProp <= 0.0 ||
        !std::isfinite(chartOffset) ||
        std::any_of(BPMsdata.begin(), BPMsdata.end(),
                    [](const SyncTrackBPM &b) {
                        return !std::isfinite(b.offset) || b.BPM <= 0.0;
                    })) {
        throw std::runtime_error("Invalid chart timing");
    }
    const double ppq = chartResolutionProp * 192.0;
    source.ppq =
        ppq <= std::numeric_limits<int>::max() ? static_cast<int>(ppq) : 0;
    source.offset = chartOffset;
    source.hopoPolicy = "feedback-runtime-seconds-2.08";
    auto secondsAt = [&](int64_t tick) {
        return getNoteTime(BPMsdata, getRefBPM(BPMsdata, tick), tick) +
               chartOffset;
    };
    for (const auto &tempo : BPMsdata) {
        // Diagnostic ticks must not introduce an out-of-range floating cast
        // for a legacy chart. Normal gameplay conversion remains unchanged.
        if (tempo.offset < 0 || tempo.offset >= std::ldexp(1.0, 63)) {
            continue;
        }
        source.tempos.push_back({static_cast<int64_t>(tempo.offset),
                                 secondsAt(static_cast<int64_t>(tempo.offset)),
                                 tempo.BPM / 1000.0});
    }
    for (const auto &scope : feedBackChartMap) {
        if (scope.first != "[Events]" && scope.first != instrument &&
            !(includeExtendedEvidence && scope.first == "[SyncTrack]")) {
            continue;
        }
        for (const auto &record : scope.second) {
            for (const auto &value : record.second) {
                std::istringstream fields(value);
                std::string kind;
                fields >> kind;
                if (kind == "TS" && scope.first == "[SyncTrack]") {
                    // Inspection evidence only; preserve the runtime clock.
                    int numerator = 0, exponent = 2;
                    if (fields >> numerator && numerator > 0) {
                        fields >> std::ws;
                        if (!fields.eof() && !(fields >> exponent)) {
                            source.warnings.push_back("Invalid time signature "
                                                      "ignored for inspection");
                            continue;
                        }
                        if (exponent >= 0 && exponent <= 10) {
                            int64_t tick = 0;
                            std::istringstream timestamp(record.first);
                            if (timestamp >> tick && tick >= 0 &&
                                (timestamp >> std::ws).eof()) {
                                source.events.push_back(
                                    {tick, secondsAt(tick), scope.first,
                                     "time-signature",
                                     std::to_string(numerator) + "/2^" +
                                         std::to_string(exponent)});
                            } else {
                                source.warnings.push_back(
                                    "Invalid time signature tick ignored for "
                                    "inspection");
                            }
                        }
                    }
                } else if (includeExtendedEvidence && kind == "S" &&
                           scope.first == instrument) {
                    int marker = 0;
                    int64_t length = 0;
                    if (fields >> marker >> length && marker == 2 &&
                        length >= 0) {
                        const int64_t tick = std::stoll(record.first);
                        if (tick >= 0 &&
                            tick <=
                                std::numeric_limits<int64_t>::max() - length) {
                            source.events.push_back(
                                {tick, secondsAt(tick), scope.first, "plus",
                                 "2 end_tick=" +
                                     std::to_string(tick + length)});
                        }
                    }
                } else if (kind == "E") {
                    std::string text;
                    std::getline(fields, text);
                    text = CChart::unquoteSongMetadata(text);
                    const int64_t tick = std::stoll(record.first);
                    source.events.push_back(
                        {tick, secondsAt(tick), scope.first,
                         text.find("section ") != std::string::npos ? "section"
                                                                    : "text",
                         text});
                } else if (kind == "N") {
                    int lane;
                    int64_t length;
                    if (fields >> lane >> length &&
                        ((lane >= 0 && lane < 5) || lane == 7)) {
                        const int64_t tick = std::stoll(record.first);
                        if (tick < 0 || length < 0 ||
                            tick >
                                std::numeric_limits<int64_t>::max() - length) {
                            continue;
                        }
                        source.gems.push_back(
                            {tick, tick + length,
                             lane == 7 ? nf_open : (1 << lane), secondsAt(tick),
                             secondsAt(tick + length) - secondsAt(tick)});
                    }
                }
            }
        }
    }
    std::stable_sort(
        source.gems.begin(), source.gems.end(),
        [](const auto &a, const auto &b) { return a.tick < b.tick; });
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
        if (!std::isfinite(Nt.time) || !std::isfinite(Nt.lTime)) {
            throw std::runtime_error("Invalid note timing");
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
    return validateLoadedChart();
} catch (const std::exception &e) {
    CLog::log() << "Cannot load chart " + std::string(chartFile) + ": " +
                       e.what();
    unloadChart();
    return false;
}

bool CChart::NotesData::loadMidiChart(std::string_view file, CPlayer &player) {
    unloadChart();
    MidiChart midi;
    std::string error;
    if (!readMidiChart(std::string(file), midi, error)) {
        CLog::log() << "Cannot load MIDI " + std::string(file) + ": " + error;
        return false;
    }
    const auto found = midi.instruments.find(player.instrument);
    if (found == midi.instruments.end()) {
        CLog::log() << "MIDI instrument unavailable: " + player.instrument;
        return false;
    }
    gNotes = std::move(found->second.notes);
    gPlus = std::move(found->second.plus);
    source = std::move(found->second.source);
    BPM = std::move(midi.bpm);
    chartResolutionProp = source.ppq / 192.0;
    if (!validateLoadedChart()) {
        return false;
    }
    player.songName = std::move(midi.name);
    player.songArtist = std::move(midi.artist);
    player.songCharter = std::move(midi.charter);
    for (const auto &warning : source.warnings) {
        CLog::log() << "MIDI " + std::string(file) + ": " + warning;
    }
    return true;
}

void CChart::NotesData::deducePlusLastNotes() {
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

auto CChart::NotesData::getChartEnd(double offset) -> double {
    if (chartEnd > 0) {
        return chartEnd;
    }

    chartEnd = static_cast<const NotesData &>(*this).getChartEnd(offset);
    return chartEnd;
}

auto CChart::NotesData::getChartEnd(double offset) const -> double {
    if (chartEnd > 0) {
        return chartEnd;
    }

    auto size = gNotes.size();

    if (size > 0) {
        auto &lastNote = gNotes[size - 1];

        double finalTime = lastNote.time + lastNote.lTime;
        finalTime += offset;

        return finalTime;
    }

    return 2.0;
}

bool CChart::NotesData::loadChart(std::string_view file, CPlayer &player) {
    Metadata metadata;
    const bool valid = loadChartCPU(file, player.instrument, metadata);
    player.songName = std::move(metadata.name);
    player.songArtist = std::move(metadata.artist);
    player.songCharter = std::move(metadata.charter);
    return valid;
}

bool CChart::NotesData::loadChartCPU(std::string_view chartFile,
                                     const std::string &instrument,
                                     Metadata &metadata) try {
    unloadChart();

    std::string lowerPath(chartFile);
    std::transform(
        lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowerPath.size() >= 4 &&
        lowerPath.compare(lowerPath.size() - 4, 4, ".gpp") == 0 &&
        !CChart::hasLegacyTextHeader(std::string(chartFile))) {
        CChart chart;

        std::fstream chartf(std::string(chartFile),
                            std::ios::binary | std::ios::in);

        if (!hasBinaryChartPrefix(chartf)) {
            return false;
        }

        try {
            cereal::BinaryInputArchive oarchive(chartf);

            oarchive(chart);

            const auto part = chart.instruments.find(instrument);
            if (part == chart.instruments.end()) {
                return false;
            }
            BPM = chart.BPM;
            gNotes = part->second.gNotes;
            gPlus = part->second.gPlus;
            chartResolutionProp = chart.chartData.chartResolutionProp;
            metadata.name = chart.chartData.songName;
            metadata.artist = chart.chartData.songArtist;
            metadata.charter = chart.chartData.songCharter;
            return validateLoadedChart();
        } catch (const std::exception &e) {
            CLog::log().multiRegister("Exception %0", e);
            unloadChart();
            return false;
        }
    }

    const std::string legacyPath(chartFile);
    CText ACTEXTChart(legacyPath.c_str(), true);

    std::deque<CText::field_t> &textArray = ACTEXTChart[""].fields;

    double ntInfoTime = 0;
    double ntInfoLTime = 0;
    int ntIDInfo = 0;
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

            if (scanResult != 3 || !std::isfinite(ntInfoTime) ||
                !std::isfinite(ntInfoLTime)) {
                throw std::runtime_error("Invalid legacy chart record");
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
    return validateLoadedChart();
} catch (const std::exception &e) {
    CLog::log() << "Cannot load chart " + std::string(chartFile) + ": " +
                       e.what();
    unloadChart();
    return false;
}

bool CChart::NotesData::validateLoadedChart() {
    const bool valid = !gNotes.empty() && !BPM.empty() &&
                       std::isfinite(chartResolutionProp) &&
                       chartResolutionProp > 0.0 &&
                       std::is_sorted(gNotes.begin(), gNotes.end()) &&
                       std::is_sorted(BPM.begin(), BPM.end()) &&
                       std::all_of(gNotes.begin(), gNotes.end(),
                                   [](const Note &n) {
                                       return std::isfinite(n.time) &&
                                              std::isfinite(n.lTime) &&
                                              std::isfinite(n.time + n.lTime) &&
                                              n.lTime >= 0.0;
                                   }) &&
                       std::all_of(BPM.begin(), BPM.end(), [](const Note &b) {
                           return std::isfinite(b.time) &&
                                  std::isfinite(b.lTime) && b.lTime > 0.0;
                       });
    if (valid) {
        lastNoteEnd = gNotes.front().time + gNotes.front().lTime;
        for (const auto &note : gNotes) {
            lastNoteEnd = std::max(lastNoteEnd, note.time + note.lTime);
        }
    }
    if (!valid) {
        CLog::log() << "Chart has no playable notes or invalid timing";
        unloadChart();
    }
    return valid;
}

void CChart::NotesData::unloadChart() {
    source = {};
    lastNoteEnd = 0.0;
    chartEnd = 0;
    notePos = 0;
    lastNotePicked = ~0uL;
    chartResolutionProp = 1.0;
    plusPos = 0;

    for (auto &fretNotePickedTime : fretsNotePickedTime) {
        fretNotePickedTime = 0.0;
    }

    gNotes.clear();
    BPM.clear();
    gPlus.clear();
}

CChart::NotesData::NotesData() {
    notePos = 0;
    lastNotePicked = ~0uL;
    chartResolutionProp = 1.0;
    plusPos = 0;
    BPMMinPosition = 0.0;
    chartEnd = 0;
    BPM.reserve(200);
    gPlus.reserve(200);
    gNotes.reserve(10000);

    for (auto &fretNotePickedTime : fretsNotePickedTime) {
        fretNotePickedTime = 0.0;
    }
}

CChart::NotesData::~NotesData() { unloadChart(); }

CChart::Inspection CChart::inspect(const std::string &file) {
    Inspection result;
    try {
        auto extension = std::filesystem::path(file).extension().string();
        std::transform(
            extension.begin(), extension.end(), extension.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".mid") {
            MidiChart midi;
            if (!readMidiChart(file, midi, result.error)) {
                return result;
            }
            // These strings are INI-derived, not embedded MIDI metadata.
            result.trackNames = std::move(midi.trackNames);
            result.identification = std::move(midi.identification);
            result.bpm = std::move(midi.bpm);
            for (const auto &part : midi.instruments) {
                result.instruments.push_back(part.first);
                for (const auto &note : part.second.notes) {
                    result.lastNoteEnd =
                        std::max(result.lastNoteEnd, note.time + note.lTime);
                }
            }
        } else {
            NotesData notes;
            auto collect = [&](const std::string &instrument) {
                result.instruments.push_back(instrument);
                result.lastNoteEnd =
                    std::max(result.lastNoteEnd, notes.lastNoteEnd);
                result.bpm = notes.BPM;
            };
            if (extension == ".chart") {
                if (!validateChartHeader(file)) {
                    throw std::runtime_error("Invalid chart header");
                }
                const auto document = readFeedbackDocument(file);
                for (const auto &section : document) {
                    const auto &name = section.first;
                    if (name.compare(0, 5, "[Easy") != 0 &&
                        name.compare(0, 7, "[Medium") != 0 &&
                        name.compare(0, 5, "[Hard") != 0 &&
                        name.compare(0, 7, "[Expert") != 0) {
                        continue;
                    }
                    if (notes.loadFeedbackChartCPU(file, name, result.metadata,
                                                   &document)) {
                        collect(name);
                    }
                }
            } else if (extension == ".gpp" && hasLegacyTextHeader(file)) {
                if (notes.loadChartCPU(file, "[ExpertSingle]",
                                       result.metadata)) {
                    collect("[ExpertSingle]");
                }
            } else if (extension == ".gpp") {
                CChart compiled;
                std::ifstream input(file, std::ios::binary);
                if (!hasBinaryChartPrefix(input)) {
                    throw std::runtime_error("Invalid compiled chart prefix");
                }
                cereal::BinaryInputArchive archive(input);
                archive(compiled);
                if (!std::isfinite(compiled.chartData.chartOffset)) {
                    throw std::runtime_error("Invalid compiled chart offset");
                }
                result.metadata.name = compiled.chartData.songName;
                result.metadata.artist = compiled.chartData.songArtist;
                result.metadata.charter = compiled.chartData.songCharter;
                for (const auto &part : compiled.instruments) {
                    notes.unloadChart();
                    notes.BPM = compiled.BPM;
                    notes.gNotes = part.second.gNotes;
                    notes.gPlus = part.second.gPlus;
                    notes.chartResolutionProp =
                        compiled.chartData.chartResolutionProp;
                    if (notes.validateLoadedChart()) {
                        collect(part.first);
                    }
                }
            }
        }
        if (result.instruments.empty() && result.error.empty()) {
            result.error = "No playable chart parts";
        }
    } catch (const std::exception &error) {
        result.instruments.clear();
        result.error = error.what();
    }
    return result;
}
