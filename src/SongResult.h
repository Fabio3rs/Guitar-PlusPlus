#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class SongEndReason { Completed, Aborted, LoadFailed };

inline const char *songEndReasonName(SongEndReason reason) {
    switch (reason) {
    case SongEndReason::Completed:
        return "Completed";
    case SongEndReason::Aborted:
        return "Aborted";
    case SongEndReason::LoadFailed:
        return "LoadFailed";
    }
    return "Aborted";
}

struct PlayerSongResult {
    int64_t score = 0, maxCombo = 0;
    uint64_t correctNotes = 0, totalNotes = 0;
};

// Value snapshot created at the song boundary, before loading/reset. No runtime
// pointers; consumers keep their copy independently of player/resource
// lifetime.
struct SongResult {
    std::string songId;
    SongEndReason reason = SongEndReason::LoadFailed;
    double elapsedSeconds = 0.0;
    std::vector<PlayerSongResult> players;
};
