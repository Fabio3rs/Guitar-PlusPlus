// Field-wise runtime-loader snapshot. Never dump padding or vector pointers.
#include "CPlayer.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <stdexcept>

namespace {
void number(std::ostream &out, double value) {
    static_assert(sizeof(double) == sizeof(uint64_t));
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    out << std::hex << bits << std::dec;
}

void snapshot(std::ostream &out, const CPlayer &p, bool success) {
    const auto &n = p.Notes;
    out << "success " << success << "\nmetadata " << std::quoted(p.songName)
        << ' ' << std::quoted(p.songArtist) << ' ' << std::quoted(p.songCharter)
        << "\ninstrument " << std::quoted(p.instrument) << "\npositions "
        << n.notePos << ' ' << n.plusPos << ' ' << n.lastNotePicked << '\n';
    for (double v :
         {n.lastNoteEnd, n.BPMMinPosition, n.chartEnd, n.chartResolutionProp}) {
        number(out, v);
        out << ' ';
    }
    out << "\nfrets ";
    for (double v : n.fretsNotePickedTime) {
        number(out, v);
        out << ' ';
    }
    out << '\n';
    auto notes = [&](const char *name, const auto &items) {
        out << name << ' ' << items.size() << '\n';
        for (const auto &item : items) {
            number(out, item.time);
            out << ' ';
            number(out, item.lTime);
            out << ' ' << item.type << '\n';
        }
    };
    notes("notes", n.gNotes);
    notes("bpm", n.BPM);
    out << "plus " << n.gPlus.size() << '\n';
    for (const auto &item : n.gPlus) {
        number(out, item.time);
        out << ' ';
        number(out, item.lTime);
        out << ' ' << item.type << ' ' << item.firstNote << ' ' << item.lastNote
            << '\n';
    }
}
} // namespace

int main(int argc, char **argv) try {
    if (argc != 5) {
        std::cerr << "Usage: gpp_dump_chart feedback|compiled INPUT INSTRUMENT "
                     "OUTPUT\n";
        return 2;
    }
    const std::string mode = argv[1];
    if (mode != "feedback" && mode != "compiled") {
        throw std::runtime_error("Unknown loader mode");
    }
    std::ofstream output(argv[4], std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output.imbue(std::locale::classic());
    output << "gpp-runtime-chart-dump-v1 double=ieee754-bits\n";
    CPlayer player("chart evidence");
    player.instrument = argv[3];
    auto load = [&] {
        return mode == "feedback"
                   ? player.Notes.loadFeedbackChart(argv[2], player)
                   : player.Notes.loadChart(argv[2], player);
    };
    output << "first\n";
    snapshot(output, player, load());
    output << "chartEndAfterQuery ";
    number(output, player.Notes.getChartEnd());
    output << "\nreload\n";
    snapshot(output, player, load());
    player.Notes.unloadChart();
    output << "unload\n";
    snapshot(output, player, false);
    output.close();
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
