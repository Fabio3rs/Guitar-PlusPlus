#include "CSaveSystem.h"
#include "CLog.h"

auto CSaveSystem::saveSystem() -> CSaveSystem & {
    static CSaveSystem savesystemmgr;
    return savesystemmgr;
}

auto CSaveSystem::CSave::loads() noexcept -> bool {
    try {
        std::fstream svfstream(fpath, std::ios::in | std::ios::binary);

        if (!svfstream.is_open()) {
            return false;
        }

        cereal::BinaryInputArchive iarchive(svfstream);

        iarchive(*this);

        loaded = true;
    } catch (const std::exception &e) {
        CLog::log() << e.what();
        loaded = false;
        return false;
    } catch (...) {
        CLog::log() << "Fail to load save " + fpath;
        return false;
    }

    return true;
}

auto CSaveSystem::CSave::saves() noexcept -> bool {
    try {
        std::fstream svfstream(fpath, std::ios::out | std::ios::trunc |
                                          std::ios::binary);

        if (!svfstream.is_open()) {
            return false;
        }

        for (auto &vardata : values) {
            auto &v = vardata.second;

            if (v.dynamic && (v.ptr != nullptr)) {
                for (size_t i = 0; i < v.size; i++) {
                    v.svcontent[i] = ((uint8_t *)(v.ptr))[i];
                }
            }
        }

        cereal::BinaryOutputArchive oarchive(
            svfstream); // Create an output archive

        oarchive(*this);
    } catch (const std::exception &e) {
        CLog::log() << e.what();
        return false;
    } catch (...) {
        return false;
    }
    return true;
}

auto CSaveSystem::CSave::createNew() noexcept -> bool {
    if (fpath.empty()) {
        {
            return false;
        }
    }

    if (std::fstream(fpath, std::ios::out | std::ios::trunc | std::ios::binary)
            .is_open()) {
        {
            loaded = true;
        }
    } else {
        { loaded = false; }
    }

    return loaded;
}

auto CSaveSystem::CSave::loadn(const std::string &savepath) -> bool {
    fpath = savepath;
    return loads();
}

CSaveSystem::CSave::CSave(const std::string &savepath) {
    fpath = savepath;
    loaded = false;
}

CSaveSystem::CSaveSystem() {}
