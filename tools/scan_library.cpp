// Offline library inspection. Uses the same index and audio probe as Quick
// Play.
#include "SongLibrary.h"
#include <cereal/archives/json.hpp>
#include <iostream>

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4 ||
        (argc == 4 && std::string(argv[3]) != "--full")) {
        std::cerr << "Usage: gpp_scan_library SONG_ROOT CACHE_DB [--full]\n";
        return 2;
    }
    try {
        songs::SongLibrary library(std::filesystem::u8path(argv[1]),
                                   std::filesystem::u8path(argv[2]));
        const auto start = std::chrono::steady_clock::now();
        library.refresh(argc == 4 ? songs::ScanMode::Full
                                  : songs::ScanMode::Incremental);
        library.wait();
        const auto snapshot = library.snapshot();
        const auto &p = snapshot->progress;
        const double elapsed = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        std::cerr << snapshot->songs.size() << " songs; " << p.examined
                  << " files; " << p.hashes << " hashes; " << p.chartParses
                  << " chart parses; " << p.audioProbes << " audio probes; "
                  << elapsed << " seconds\n";
        if (!p.warning.empty()) {
            std::cerr << p.warning << '\n';
        }
        {
            cereal::JSONOutputArchive json(std::cout);
            json(cereal::make_nvp("songs", snapshot->songs));
        }
        std::cout << '\n';
        return p.complete ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
