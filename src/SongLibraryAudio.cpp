#include "SongLibrary.h"
#include <bass.h>
#include <cmath>

namespace songs {
AudioInfo probeAudio(const std::filesystem::path &file) {
    // Serializes only inspection calls, never playback or judgment. Device 0
    // is process-wide even though BASS device selection is thread-local.
    static std::mutex inspectionMutex;
    const std::lock_guard<std::mutex> lock(inspectionMutex);
    // Device selection is thread-local in BASS. Never attach inspection streams
    // to gameplay's output device or alter an existing stream's position.
    const auto previous = BASS_GetDevice();
    const bool owned = BASS_Init(0, 44100, 0, nullptr, nullptr) != 0;
    if (!owned && BASS_ErrorGetCode() != BASS_ERROR_ALREADY) {
        return {{}, "Cannot initialize audio inspection"};
    }
    struct DeviceGuard {
        bool owned;
        DWORD previous;
        DeviceGuard(bool ownsDevice, DWORD oldDevice)
            : owned(ownsDevice), previous(oldDevice) {}
        DeviceGuard(const DeviceGuard &) = delete;
        DeviceGuard &operator=(const DeviceGuard &) = delete;
        DeviceGuard(DeviceGuard &&) = delete;
        DeviceGuard &operator=(DeviceGuard &&) = delete;
        ~DeviceGuard() {
            if (owned) {
                BASS_SetDevice(0);
                BASS_Free();
            }
            if (previous != DWORD(-1) && (!owned || previous != 0)) {
                BASS_SetDevice(previous);
            }
        }
    } device{owned, previous};
    if (BASS_SetDevice(0) == 0) {
        return {{}, "Cannot select audio inspection device"};
    }
    DWORD flags = BASS_STREAM_DECODE | BASS_STREAM_PRESCAN;
#ifdef _WIN32
    flags |= BASS_UNICODE;
#endif
    const HSTREAM stream =
        BASS_StreamCreateFile(FALSE, file.c_str(), 0, 0, flags);
    if (stream == 0) {
        return {{},
                "Cannot inspect audio (BASS " +
                    std::to_string(BASS_ErrorGetCode()) + ")"};
    }
    struct StreamGuard {
        HSTREAM stream;
        explicit StreamGuard(HSTREAM handle) : stream(handle) {}
        StreamGuard(const StreamGuard &) = delete;
        StreamGuard &operator=(const StreamGuard &) = delete;
        StreamGuard(StreamGuard &&) = delete;
        StreamGuard &operator=(StreamGuard &&) = delete;
        ~StreamGuard() { BASS_StreamFree(stream); }
    } guard{stream};
    const auto length = BASS_ChannelGetLength(stream, BASS_POS_BYTE);
    const double duration =
        length == QWORD(-1) ? -1 : BASS_ChannelBytes2Seconds(stream, length);
    if (!std::isfinite(duration) || duration <= 0) {
        return {{}, "Audio duration unavailable"};
    }
    return {duration, {}};
}
} // namespace songs
