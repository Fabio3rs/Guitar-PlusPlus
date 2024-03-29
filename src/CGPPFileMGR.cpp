#include "CGPPFileMGR.h"
#include "CEngine.h"
#include "GPPGame.h"

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

    auto filesList = GPPGame::getDirectory(dir.c_str(), true, false);

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
