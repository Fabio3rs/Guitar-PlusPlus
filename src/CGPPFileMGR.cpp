#include "CGPPFileMGR.h"
#include "CLog.h"
#include <filesystem>

std::deque<std::string> CGPPFileMGR::getDirectory(const std::string &directory,
                                                  bool getFiles,
                                                  bool getDirectories,
                                                  std::error_code *error) {
    std::deque<std::string> result;
    if (error) {
        error->clear();
    }
    if (!getFiles && !getDirectories) {
        return result;
    }

    auto report = [&](const std::filesystem::path &path,
                      const std::error_code &ec) {
        if (error && !*error) {
            *error = ec;
        }
        CLog::log() << "Cannot list " + path.string() + ": " + ec.message();
    };
    std::error_code ec;
    std::filesystem::directory_iterator entry(directory, ec), end;
    if (ec) {
        report(directory, ec);
        return result;
    }
    while (entry != end) {
        const auto status = entry->status(ec);
        if (ec) {
            // A dangling link or an entry removed during enumeration is
            // harmless.
            if (ec != std::errc::no_such_file_or_directory) {
                report(entry->path(), ec);
            }
        } else if ((getDirectories && std::filesystem::is_directory(status)) ||
                   (getFiles && std::filesystem::is_regular_file(status))) {
            result.push_back(entry->path().filename().string());
        }
        entry.increment(ec);
        if (ec) {
            report(directory, ec);
            break;
        }
    }
    return result;
}

void GPPPackage::decrypt() {}

void GPPPackage::pushFile(const std::string &file, const std::string &name,
                          bool encrypt) {
    item ni;

    std::fstream f(file, std::ios::in | std::ios::binary);

    if (!f.is_open()) {
        return;
    }

    size_t s = static_cast<size_t>(CGPPFileMGR::fileSize(f));

    ni.bytes.insert(ni.bytes.begin(), s, '\0');
    f.read((char *)&ni.bytes[0], s);

    ni.encrypted = encrypt;
    ni.decrypted = false;

    items.push_back(std::move(ni));
}

GPPPackage::GPPPackage() { loaded = false; }

GPPPackage::item::item() {
    encrypted = false;
    decrypted = false;
}

CGPPFileMGR &CGPPFileMGR::mgr() {
    static CGPPFileMGR MGR;
    return MGR;
}

GPPPackage CGPPFileMGR::loadPackage(const std::string &file) {
    GPPPackage result;
    std::fstream fs(file);

    cereal::BinaryInputArchive iarchive(fs);

    iarchive(result);

    result.loaded = true;

    return result;
}

GPPPackage CGPPFileMGR::newPackageFromDirectory(const std::string &packName,
                                                const std::string &dir) {
    GPPPackage newPack;

    auto filesList = getDirectory(dir, true, false);

    for (auto &f : filesList) {
        newPack.pushFile(dir + "/" + f, f, true);
    }

    std::fstream pack(packName, std::ios::out | std::ios::trunc);

    cereal::BinaryOutputArchive oarchive(pack);

    newPack.loaded = true;

    oarchive(newPack);

    return newPack;
}

CGPPFileMGR::CGPPFileMGR() {}
