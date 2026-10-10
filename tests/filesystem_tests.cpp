#include "CGPPFileMGR.h"
#include "CLuaH.hpp"
#include "GPPGame.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>

namespace {
class FileSystem : public ::testing::Test {
  protected:
    std::filesystem::path directory;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("gpp-directory-test-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory);
    }
    void TearDown() override {
        CLuaH::Lua().files.erase(directory.string());
        std::filesystem::remove_all(directory);
    }
    std::set<std::string> names(bool files, bool directories) {
        const auto found = CGPPFileMGR::getDirectory(directory.string(), files, directories);
        return {found.begin(), found.end()};
    }
};
}

TEST_F(FileSystem, FiltersBasenamesWithoutSkippingEntriesOrRecursing) {
    std::filesystem::create_directory(directory / "guitar test");
    std::ofstream(directory / "notes.chart") << "chart";
    EXPECT_EQ(names(true, false), (std::set<std::string>{"notes.chart"}));
    std::ofstream(directory / "canção.ogg") << "audio";
    std::ofstream(directory / "guitar test/nested.lua") << "value = 1";
    EXPECT_EQ(names(true, false), (std::set<std::string>{"notes.chart", "canção.ogg"}));
    EXPECT_EQ(names(false, true), (std::set<std::string>{"guitar test"}));
    EXPECT_EQ(names(true, true),
              (std::set<std::string>{"notes.chart", "canção.ogg", "guitar test"}));
    EXPECT_TRUE(names(false, false).empty());
    const auto path = directory.string();
    EXPECT_EQ(GPPGame::caseInsensitiveSearchDir(path.c_str(), true, false, "NOTES.CHART"),
              "notes.chart");
    EXPECT_EQ(GPPGame::caseInsensitiveSearchDir(path.c_str(), false, true, "GUITAR TEST"),
              "guitar test");
    EXPECT_TRUE(GPPGame::caseInsensitiveSearchDir(path.c_str(), true, false, "guitar test").empty());
}

TEST_F(FileSystem, DistinguishesEmptyMissingAndNonDirectoryPaths) {
    std::error_code error;
    EXPECT_TRUE(CGPPFileMGR::getDirectory(directory.string(), true, true, &error).empty());
    EXPECT_FALSE(error);
    const auto missing = (directory / "missing").string();
    EXPECT_TRUE(CGPPFileMGR::getDirectory(missing, true, true, &error).empty());
    EXPECT_TRUE(error);
    const auto file = (directory / "file").string();
    std::ofstream(file) << "not a directory";
    EXPECT_TRUE(CGPPFileMGR::getDirectory(file, true, true, &error).empty());
    EXPECT_TRUE(error);
    EXPECT_FALSE(CLuaH::Lua().loadFiles(missing));
    CLuaH::scriptStorage storage;
    EXPECT_FALSE(CLuaH::Lua().loadFilesDequeStorage(missing, storage));
    EXPECT_TRUE(storage.empty());
}

TEST_F(FileSystem, FollowsFileAndDirectoryLinksAndIgnoresDanglingLinks) {
    std::ofstream(directory / "song.ogg") << "audio";
    std::filesystem::create_directory(directory / "songs");
    std::error_code error;
    std::filesystem::create_symlink("song.ogg", directory / "audio link", error);
    if (error) GTEST_SKIP() << "Symlink creation unavailable: " << error.message();
    std::filesystem::create_directory_symlink("songs", directory / "songs link", error);
    if (error) GTEST_SKIP() << "Directory symlink creation unavailable: " << error.message();
    std::filesystem::create_symlink("missing", directory / "dangling.lua", error);
    ASSERT_FALSE(error) << error.message();
    EXPECT_EQ(names(true, false), (std::set<std::string>{"song.ogg", "audio link"}));
    EXPECT_EQ(names(false, true), (std::set<std::string>{"songs", "songs link"}));
    CGPPFileMGR::getDirectory(directory.string(), true, true, &error);
    EXPECT_FALSE(error);
}

TEST_F(FileSystem, LuaUsesFinalExtensionAndDoesNotLoadDirectories) {
    std::ofstream(directory / "menu.extra.lua") << "value = 42";
    std::ofstream(directory / "ignored.lua.bak") << "error('backup executed')";
    std::filesystem::create_directory(directory / "folder.lua");
    auto &lua = CLuaH::Lua();
    ASSERT_TRUE(lua.loadFiles(directory.string()));
    ASSERT_EQ(lua.files[directory.string()].size(), 1u);
    EXPECT_EQ(lua.files[directory.string()].count("menu.extra.lua"), 1u);
    CLuaH::scriptStorage storage;
    ASSERT_TRUE(lua.loadFilesDequeStorage(directory.string(), storage));
    ASSERT_EQ(storage.size(), 1u);
    EXPECT_EQ(storage.front().fileName, "menu.extra.lua");
    ASSERT_EQ(lua.runScript(storage.front()), 0);
    lua_getglobal(storage.front().luaState.get(), "value");
    EXPECT_EQ(lua_tointeger(storage.front().luaState.get(), -1), 42);
}
