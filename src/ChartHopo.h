#pragma once

#include "CChart.h"

namespace chart {
constexpr int laneMask =
    nf_green | nf_red | nf_yellow | nf_blue | nf_orange | nf_open;

inline bool isChord(int mask) {
    mask &= laneMask;
    return (mask & (mask - 1)) != 0;
}

// The loader supplies the temporal policy. Feedback keeps its historical
// seconds/BPM test; MIDI uses musical ticks and the MIDI chord-subset rule.
inline void classifyHopo(CChart::Note &note, const CChart::Note *previous,
                         bool closeEnough, bool excludeChordSubset) {
    const int lanes = note.type & laneMask;
    const int previousLanes = previous ? previous->type & laneMask : 0;
    if (!previous || !closeEnough || isChord(lanes) || lanes == previousLanes ||
        (excludeChordSubset && isChord(previousLanes) &&
         (lanes & previousLanes) == lanes)) {
        note.type |= nf_not_hopo;
    }
}
} // namespace chart
