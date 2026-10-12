#include "CBackgroundRenderer.h"
// Guitar++.cpp : Defines the entry point for the console application.
//
// brmodstudio.forumeiros.com

#include "CCampaing.h"
#include "CChart.h"
#include "CControls.h"
#include "CEngine.h"
#include "CFonts.h"
#include "CGPPFileMGR.h"
#include "CGuitars.h"
#include "CLanguageManager.h"
#include "CLog.h"
#include "CLuaH.hpp"
#include "CMenu.h"
#include "CPlayer.h"
#include "CShader.h"
#include "GPPGame.h"
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "CMultiplayer.h"

static auto load_data() -> bool {
    auto &lngmgr = CLanguageManager::langMGR();

    auto &game = GPPGame::GuitarPP();

    auto &lua = CLuaH::Lua();

    // Window
    game.createWindow();

    auto &guitars = CGuitars::inst();
    auto &campaingMgr = CCampaing::campaingMGR();

    CLog::log().multiRegister(
        "Shaders status: %0",
        ShaderProject::CShader::inst().isShadersEnabled());

    double ltime = CEngine::engine().getTime();

    int loadLoop = 0;

    bool wOpened = true;

    while ((wOpened = CEngine::engine().windowOpened()) &&
           ((CEngine::engine().getTime() - ltime) < 0.2 || loadLoop < 10)) {
        if (CEngine::engine().getKey(GLFW_KEY_ESCAPE) != 0) {
            return 0;
        }

        game.clearScreen();

        switch (loadLoop) {
        case 0:
            guitars.loadAllGuitars();
            break;

        case 1:
            game.loadAllThemes();
            game.gThemes["gppdefaulttheme"].apply();
            break;

        default:
            break;
        }

        if (loadLoop != 0x7FFFFFFF) {
            {
                ++loadLoop;
            }
        }

        game.renderFrame();
    }

    if (!wOpened) {
        return false;
    }

    const auto &logotext = game.loadTexture("data/sprites", "logo.tga");

    CEngine::engine().activate3DRender(false);

    CEngine::RenderDoubleStruct lgrdata{};

    lgrdata.x1 = -0.5;
    lgrdata.x2 = -0.5 + 1.0;
    lgrdata.x3 = -0.5 + 1.0;
    lgrdata.x4 = -0.5;

    lgrdata.y1 = -0.5 + 1.0;
    lgrdata.y2 = -0.5 + 1.0;
    lgrdata.y3 = -0.5;
    lgrdata.y4 = -0.5;

    lgrdata.TextureX1 = 0.0;
    lgrdata.TextureX2 = 1.0;

    lgrdata.TextureY1 = 1.0;
    lgrdata.TextureY2 = 0.0;

    lgrdata.Text = logotext.getTextId();

    std::string str2nibblePresents = "2Nibble Studios presents...";

    loadLoop = 0;

    ltime = CEngine::engine().getTime();

    while ((wOpened = CEngine::engine().windowOpened()) &&
           (((CEngine::engine().getTime() - ltime) < 3.0 || loadLoop < 100) ||
            game.getNumTexturesToLoad() > 0 || game.getNumModelsToLoad() > 0)) {
        if (CControls::controls().keyEscape() != 0) {
            return false;
        }

        game.clearScreen();

        {
            switch (loadLoop) {
            case 0:
                game.loadBasicSprites();
                ShaderProject::CShader::inst().prepareEffects();
                CBackgroundRenderer::inst().prepare();
                break;

            case 1:
                game.loadBasicSounds();
                game.menus.buildInitialMenus(1);
                break;
            case 2:
            case 3:
                game.menus.buildInitialMenus(loadLoop);
                break;
            case 4:
                game.loadTexture("data/sprites", "menu.tga");
                break;
            case 5:
                game.menus.buildInitialMenus(5);
                campaingMgr.loadCampaingModes();
                break;

            case 6:
                game.initialLoad();
                break;

            case 7:
                game.initialLoad2();
                break;

            default:
                break;
            }

            if (loadLoop != 0x7FFFFFFF) {
                {
                    ++loadLoop;
                }
            }
        }

        double t = (CEngine::engine().getTime() - ltime) * 3.14 / 3.0;

        CEngine::engine().setColor(1.0, 1.0, 1.0, sin(t));
        CEngine::engine().Render2DQuad(lgrdata);

        CEngine::engine().setColor(1.0, 1.0, 1.0, sin(t + 0.1));
        CFonts::fonts().drawTextInScreenWithBuffer(
            str2nibblePresents + std::to_string(game.getNumTexturesToLoad()),
            CFonts::fonts().getCenterPos(str2nibblePresents, 0.1, 0.0), -0.6,
            0.1);

        CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);
        game.renderFrame();

        // Queue entries are owned by the streaming loader; avoid inspecting raw
        // batch pointers here while other threads may enqueue textures.
    }

    if (CEngine::engine().windowOpened() && loadLoop >= 8 &&
        !game.basicModelsReady()) {
        throw std::runtime_error("Required gameplay models failed to load");
    }
    return CEngine::engine().windowOpened() && loadLoop >= 8;
}

auto main(int argc, char *argv[]) -> int {
    const char *stage = "engine initialization";
    try {
        CEngine::engine().errorCallbackFun = GPPGame::logError;
        CLog::log();
        CEngine::engine().init();

        CLanguageManager::langMGR();

        auto &game = GPPGame::GuitarPP();

        // Runs on normal exit, cancelled startup and exception unwinding,
        // while the engine's window/context and callback targets still exist.
        struct AssetShutdown {
            GPPGame &game;
            CGuitars *guitars = nullptr;
            ~AssetShutdown() {
                if (guitars != nullptr) {
                    guitars->unload();
                }
                game.shutdownModels();
                game.shutdownTextures();
            }
        } assetShutdown{game};

        game.parseParameters(argc, argv);

        auto &lua = CLuaH::Lua();

        // Register enableShaders before Config.lua can set it. No GL upload
        // here.
        ShaderProject::CShader::inst();

        // Configuration file
        stage = "Config.lua";
        auto script = lua.newScript(".", "Config.lua");

        if (script.luaState) {
            if (lua.runScript(script) != 0) {
                const char *error = lua_tostring(script.luaState.get(), -1);
                throw std::runtime_error(error ? error
                                               : "Config.lua execution failed");
            }
        } else if (std::filesystem::exists("Config.lua")) {
            throw std::runtime_error(
                "Cannot load Config.lua; see Guitar++.log");
        } else {
            CLog::log()
                << "Config.lua not found; using saved settings/defaults";
        }

        // Run all scripts in quere
        stage = "data scripts";
        lua.runScripts();

        stage = "window and assets";
        if (!load_data()) {
            CLog::log() << "Startup loading cancelled";
            return 0;
        }

        auto &guitars = CGuitars::inst();
        assetShutdown.guitars = &guitars;

        GPPGame::GuitarPP().setVSyncMode(1);

        const int atExitSE = lua.idForCallbackEvent("atExit");

        /*CMultiplayer mp;

        std::cout << "Trying to start mp\n\n";
        mp.startServer("7777");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        mp.clientConnect("127.0.0.1", "7777");*/

        stage = "campaign loading";
        {
            cfile_ptr svfstream(CEngine::make_cfile(
                "./data/saves/campaings/campaingZoeira/save", "rb"));

            if (svfstream) {
                svfstream.reset();

                if (!CCampaing::campaingMGR().loadCampaingF(
                        "./data/saves/campaings/campaingZoeira/save")) {
                    throw std::runtime_error(
                        "Cannot load campaign save; file preserved");
                }
            } else {
                svfstream.reset();

                CCampaing::campaingMGR().newCampaing();
            }
        }

        stage = "menus/gameplay";
        game.menus.selectPlayerMenu();
        game.menus.openMenus(game.menus.getMainMenu(), nullptr, nullptr,
                             nullptr, false);

        lua.runEvent(atExitSE);
        game.saveSettings();
        guitars.unload();
        game.scene.unload(game.testobj);
        lua.unloadAll();
    } catch (const std::exception &e) {
        const std::string message =
            std::string("Failure during ") + stage + ": " + e.what();
        std::cerr << message << '\n';
        // Keep the terminal diagnostic even if logging itself fails.
        try {
            CLog::log() << message;
            CLog::log().SaveBuffer();
        } catch (const std::exception &) {
        }

        return 1;
    }

    return 0;
}
