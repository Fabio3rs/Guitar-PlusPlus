#include "CGameSettings.h"
#include "CLuaFunctions.hpp"
#include "GPPGame.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
class Settings : public ::testing::Test {
  protected:
    std::filesystem::path directory;
    std::string filename;

    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("gpp-settings-test-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory);
        filename = (directory / "saves/settings.json").string();
    }
    void TearDown() override { std::filesystem::remove_all(directory); }

    std::string contents() {
        std::ifstream input(filename);
        return {std::istreambuf_iterator<char>(input), {}};
    }
};
}

TEST_F(Settings, RoundTripFieldsAndStringsAcrossInstances) {
    CGameSettings store(filename);
    CGameSettings::Window window;
    std::string language = "PT-BR";
    ASSERT_EQ(store.load(window, language), CGameSettings::LoadResult::Missing);
    window.w = 1280;
    window.h = 720;
    window.AA = 4;
    window.colorBits = 32;
    window.VSyncMode = 2;
    window.fullscreen = 3;
    window.name = "Guitarra: çã🎸 " + std::string(1024, 'x');
    ASSERT_TRUE(store.save(window, language)) << store.error();

    CGameSettings other(filename);
    CGameSettings::Window restored;
    std::string restoredLanguage;
    ASSERT_EQ(other.load(restored, restoredLanguage), CGameSettings::LoadResult::Loaded);
    EXPECT_EQ(restored.w, window.w);
    EXPECT_EQ(restored.h, window.h);
    EXPECT_EQ(restored.AA, window.AA);
    EXPECT_EQ(restored.colorBits, window.colorBits);
    EXPECT_EQ(restored.VSyncMode, window.VSyncMode);
    EXPECT_EQ(restored.fullscreen, window.fullscreen);
    EXPECT_EQ(restored.name, window.name);
    EXPECT_EQ(restoredLanguage, language);

    // Rewrite an existing file; empty/short strings must not retain old bytes.
    restored.name.clear();
    ASSERT_TRUE(other.save(restored, "en")) << other.error();
    ASSERT_EQ(store.load(window, language), CGameSettings::LoadResult::Loaded);
    EXPECT_TRUE(window.name.empty());
    EXPECT_EQ(language, "en");
    EXPECT_FALSE(std::filesystem::exists(filename + ".tmp"));
}

TEST_F(Settings, BadOrUnsupportedFilePreservesDefaultsAndOriginal) {
    std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
    for (const std::string bad : {"{broken", "{\"version\": 42}",
                                  "{\"version\": 1, \"window\": {\"width\": 100}}"}) {
        { std::ofstream output(filename); output << bad; }
        CGameSettings store(filename);
        CGameSettings::Window window;
        window.w = 800;
        window.name = "untouched";
        std::string language = "PT-BR";
        EXPECT_EQ(store.load(window, language), CGameSettings::LoadResult::Error);
        EXPECT_EQ(window.w, 800);
        EXPECT_EQ(window.name, "untouched");
        EXPECT_EQ(language, "PT-BR");
        EXPECT_FALSE(store.error().empty());
        EXPECT_FALSE(store.save(window, language));
        EXPECT_EQ(contents(), bad);
    }
}

TEST_F(Settings, FailedWritePreservesPreviousArchive) {
    CGameSettings store(filename);
    CGameSettings::Window window;
    std::string language = "PT-BR";
    ASSERT_EQ(store.load(window, language), CGameSettings::LoadResult::Missing);
    ASSERT_TRUE(store.save(window, language));
    const auto previous = contents();
    // Deterministic failure, including when tests run with elevated privileges.
    std::filesystem::create_directory(filename + ".tmp");
    window.w = 1920;
    EXPECT_FALSE(store.save(window, language));
    EXPECT_EQ(contents(), previous);
}

TEST_F(Settings, RejectsInvalidWindowWithoutOverwritingFile) {
    CGameSettings store(filename);
    CGameSettings::Window window;
    std::string language = "PT-BR";
    ASSERT_EQ(store.load(window, language), CGameSettings::LoadResult::Missing);
    ASSERT_TRUE(store.save(window, language));
    const auto previous = contents();
    window.VSyncMode = 999;
    EXPECT_FALSE(store.save(window, language));
    EXPECT_EQ(contents(), previous);

    std::string invalid = previous;
    const auto vsync = invalid.find("\"vsync\": 3");
    ASSERT_NE(vsync, std::string::npos);
    invalid.replace(vsync, std::string("\"vsync\": 3").size(), "\"vsync\": -1");
    { std::ofstream output(filename); output << invalid; }
    window.VSyncMode = 1;
    EXPECT_EQ(store.load(window, language), CGameSettings::LoadResult::Error);
    EXPECT_EQ(window.VSyncMode, 1);
}

TEST_F(Settings, LuaSyntaxFailureDoesNotReturnAnExecutableScript) {
    const auto file = directory / "bad.lua";
    { std::ofstream output(file); output << "function ("; }
    auto script = CLuaH::Lua().newScript(directory.string(), "bad.lua");
    EXPECT_EQ(script.luaState, nullptr);
}

TEST_F(Settings, LuaRuntimeErrorRetainsDiagnostic) {
    const auto file = directory / "bad.lua";
    { std::ofstream output(file); output << "error('configuration test failure')"; }
    auto script = CLuaH::Lua().newScript(directory.string(), "bad.lua");
    ASSERT_NE(script.luaState, nullptr);
    EXPECT_NE(CLuaH::Lua().runScript(script), 0);
    const char *message = lua_tostring(script.luaState.get(), -1);
    ASSERT_NE(message, nullptr);
    EXPECT_NE(std::string(message).find("configuration test failure"), std::string::npos);
}

TEST_F(Settings, DirectoryLoadingSkipsScriptsThatFailedToCompile) {
    { std::ofstream output(directory / "bad.lua"); output << "function ("; }
    { std::ofstream output(directory / "good.lua"); output << "value = 42"; }
    auto &lua = CLuaH::Lua();
    const auto path = directory.string();
    ASSERT_TRUE(lua.loadFiles(path));
    ASSERT_EQ(lua.files[path].size(), 1u);
    EXPECT_EQ(lua.files[path].count("bad.lua"), 0u);
    EXPECT_EQ(lua.runScript(lua.files[path].at("good.lua")), 0);
    lua.files.erase(path);
    CLuaH::scriptStorage storage;
    ASSERT_TRUE(lua.loadFilesDequeStorage(path, storage));
    ASSERT_EQ(storage.size(), 1u);
    EXPECT_EQ(storage.front().fileName, "good.lua");
    EXPECT_NE(storage.front().luaState, nullptr);
}

TEST_F(Settings, LuaOverridesSavedValuesAndClearsWindowFlags) {
    auto &game = GPPGame::GuitarPP();
    const auto original = game.getWindowConfig();
    const double originalSpeed = game.hyperSpeed;
    auto window = original;
    window.w = 1280;
    window.h = 720;
    window.fullscreen = 3;
    window.VSyncMode = 1;
    window.name = "saved";
    game.settWindowConfigs(window);
    auto state = CLuaH::make_luaState();
    luaL_openlibs(state.get());
    CLuaFunctions::LuaF().registerFunctions(state);
    EXPECT_EQ(luaL_dostring(state.get(),
        "WindowWidth = 800; FullScreen = false; Borderless = false; "
        "customTitle = 'lua'; setConfigs()"), 0);
    EXPECT_EQ(game.getWindowConfig().w, 800);
    EXPECT_EQ(game.getWindowConfig().h, 720);
    EXPECT_EQ(game.getWindowConfig().fullscreen, 0);
    EXPECT_EQ(game.getWindowConfig().name, "lua");
    EXPECT_EQ(luaL_dostring(state.get(),
        "WindowWidth = -1; WindowHeight = math.huge; "
        "VSync = 9; AntiAliasing = 0.5; hyperSpeed = 0; setConfigs()"), 0);
    EXPECT_EQ(game.getWindowConfig().w, 800);
    EXPECT_EQ(game.getWindowConfig().h, 720);
    EXPECT_EQ(game.getWindowConfig().VSyncMode, 1);
    EXPECT_EQ(game.getWindowConfig().AA, original.AA);
    EXPECT_EQ(game.hyperSpeed, originalSpeed);
    game.settWindowConfigs(original);
    game.hyperSpeed = originalSpeed;
}
