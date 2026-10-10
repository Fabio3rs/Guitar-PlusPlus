#pragma once
#ifndef GUITARPP_CGPPFILEMGR_H
#define GUITARPP_CGPPFILEMGR_H

#include <cereal/archives/binary.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/deque.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

class GPPPackage {
    friend class CGPPFileMGR;
    bool loaded;

  public:
    struct item {
        bool encrypted;
        std::string name;
        std::vector<unsigned char> bytes;

        template <class Archive> void load(Archive &archive) {
            archive(encrypted, name, bytes);
        }

        template <class Archive> void save(Archive &archive) const {
            archive(encrypted, name, bytes);
        }

        item();

      private:
        friend class GPPPackage;
        bool decrypted;
    };

    std::string packageDescription;
    std::deque<item> items;

    template <class Archive> void load(Archive &archive) {
        archive(packageDescription, items);
    }

    template <class Archive> void save(Archive &archive) const {
        archive(packageDescription, items);
    }

    void decrypt();
    void pushFile(const std::string &file, const std::string &name,
                  bool encrypt);

    GPPPackage();
};

class CGPPFileMGR {

  public:
    static CGPPFileMGR &mgr();

    // Non-recursive, unsorted basenames. Follows links to regular files/directories;
    // ignores dangling links and special files. I/O errors are logged and optional
    // error output distinguishes an unavailable directory from an empty one.
    static std::deque<std::string> getDirectory(const std::string &directory,
        bool getFiles, bool getDirectories, std::error_code *error = nullptr);

    GPPPackage loadPackage(const std::string &file);

    template <class T> inline static std::streamsize fileSize(T &file) {
        file.seekg(0, std::ios::beg);
        file.ignore(std::numeric_limits<std::streamsize>::max());
        std::streamsize offset = file.gcount();
        file.seekg(0, std::ios::beg);
        file.clear();
        return offset;
    }

    GPPPackage newPackageFromDirectory(const std::string &packName,
                                       const std::string &dir);

  private:
    CGPPFileMGR();
};

#endif
