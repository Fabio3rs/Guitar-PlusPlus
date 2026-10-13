#include "GPPGame.h"
#include "CBackgroundRenderer.h"
#include "CChart.h"
#include "CCharter.h"
#include "CControls.h"
#include "CEngine.h"
#include "CFonts.h"
#include "CGPPFileMGR.h"
#include "CLog.h"
#include "CLuaH.hpp"
#include "CMultiplayer.h"
#include "ui/MetadataText.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <utility>

std::mutex GPPGame::playersMutex;

/*
Provide default settings for the window
*/
auto GPPGame::getWindowDefaults(bool safeMode) -> GPPGame::gameWindow {
    gameWindow w;

    if (safeMode) {
        // Safe mode use reduced configs
        w.w = 800;
        w.h = 600;
        w.AA = 0;
        w.colorBits = 24;

        w.fullscreen = 0;

        w.name = "safe mode"; // Explicit "safe mode" on window title
    } else {
        w.w = 0;
        w.h = 0;
        w.AA = 0;
        w.colorBits = 24;

        w.fullscreen = 1;

        w.name = ""; // Custom name after "Guitar++"
    }

    w.VSyncMode = 3;

    return w;
}

void GPPGame::helpMenu(const std::string &name) {
    // std::cout << "aaa\n";
}

void GPPGame::charterModule(const std::string &name) {
    CCharter cht;

    GPPGame::GuitarPP().HUDText =
        GPPGame::GuitarPP().loadTexture("data/sprites", "HUD.tga").getTextId();
    GPPGame::GuitarPP().fretboardText =
        GPPGame::GuitarPP()
            .loadTexture("data/sprites", "fretboard.tga")
            .getTextId();
    GPPGame::GuitarPP().lineText =
        GPPGame::GuitarPP().loadTexture("data/sprites", "line.tga").getTextId();
    GPPGame::GuitarPP().HOPOSText =
        -1 /*GPPGame::GuitarPP().loadTexture("data/sprites",
              "HOPOS.tga").getTextId()*/
        ;
    GPPGame::GuitarPP().pylmBarText =
        GPPGame::GuitarPP()
            .loadTexture("data/sprites", "pylmbar.tga")
            .getTextId();

    auto &game = GPPGame::GuitarPP();
    auto realname = game.getCallBackRealName(name);
    auto &module = game.gameModules[realname];

    game.setVSyncMode(0);

    double sTime = CEngine::engine().getTime();

    std::deque<double> data;

    bool bGDemo = false;
    bool escape = true;

    while (CEngine::engine().windowOpened()) {
        GPPGame::GuitarPP().clearScreen();

        if (CEngine::engine().getKey(GLFW_KEY_ESCAPE) != 0) {
            if (escape) {
                {
                    break;
                }
            }
        } else {
            escape = true;
        }

        if (CEngine::engine().getKey(' ') != 0) {
            bGDemo = true;
        }

        if (!bGDemo) {
            {
                cht.renderAll();
            }
        }

        if (bGDemo) {
            CGamePlay gp;
            cht.prepareDemoGamePlay(gp);

            bool playSound = true;

            while (CEngine::engine().windowOpened()) {
                GPPGame::GuitarPP().clearScreen();

                if (CEngine::engine().getKey(GLFW_KEY_ESCAPE) != 0) {
                    bGDemo = false;
                    escape = false;
                    break;
                }

                gp.update();

                if (playSound) {
                    gp.getBPlayer().seekSongAudio(
                        gp.getBPlayer().musicRunningTime);

                    for (auto &pp : gp.players) {
                        auto &p = *pp;
                        CEngine::engine().setSoundTime(p.instrumentSound,
                                                       p.musicRunningTime);
                    }

                    gp.playSongAudio();

                    for (auto &pp : gp.players) {
                        auto &p = *pp;
                        CEngine::engine().playSoundStream(p.instrumentSound);
                    }

                    gp.getBPlayer().setSongVolume(game.songVolume);

                    playSound = false;
                }

                gp.render();

                GPPGame::GuitarPP().renderFrame();
            }

            gp.getBPlayer().pauseSongAudio();

            for (auto &pp : gp.players) {
                auto &p = *pp;
                CEngine::engine().pauseSoundStream(p.instrumentSound);
            }
        }

        GPPGame::GuitarPP().renderFrame();
    }
}

void GPPGame::benchmark(const std::string &name) {
    auto &game = GPPGame::GuitarPP();
    auto realname = game.getCallBackRealName(name);
    auto &module = game.gameModules[realname];

    game.setVSyncMode(0);

    module.players.push_back(game.mainPlayer);
    if (!module.getBPlayer().loadSongOnlyChart("TTFAF")) {
        module.resetModule();
        return;
    }

    module.getBPlayer().startTime = CEngine::engine().getTime();

    double sTime = CEngine::engine().getTime();

    /*struct {
    };*/

    std::deque<double> data;

    while (CEngine::engine().windowOpened()) {
        GPPGame::GuitarPP().clearScreen();

        if (CEngine::engine().getKey(GLFW_KEY_ESCAPE) != 0) {
            break;
        }

        module.update();

        module.render();

        GPPGame::GuitarPP().renderFrame();
    }
}

void GPPGame::loadThread(CGamePlay &module, loadThreadData &l) {
    std::lock_guard<std::mutex> m(GPPGame::playersMutex);
    try {
        const std::string song = GuitarPP().songToLoad;
        l.loadFailed = module.players.empty();
        for (auto &p : module.players) {
            p->resetSongThings();
            if (!p->loadSong(song)) {
                CLog::log() << "Cannot start song: " + song;
                l.loadFailed = true;
                break;
            }
        }

        if (!l.loadFailed) {
            module.loadSongLyrics(song);
        }
    } catch (const std::exception &e) {
        CLog::log() << std::string("Song loading failed: ") + e.what();
        l.loadFailed = true;
    }

    l.processing = false;
}

void GPPGame::loadMarathonThread(loadThreadData &l) try {
    // No module/player access here. The main thread owns the live players and
    // holds playersMutex for this mode, preserving the legacy multiplayer
    // exclusion.
    while (l.continueThread) {
        if (!l.loadSong) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        const size_t index = l.songID;
        if (index >= l.songsList.size()) {
            l.listEnd = true;
        } else {
            std::vector<std::unique_ptr<CPlayer>> prepared;
            for (const auto &instrument : l.instruments) {
                auto player = std::make_unique<CPlayer>("");
                player->instrument = instrument;
                if (!player->loadSong(l.songsList[index])) {
                    l.loadFailed = true;
                    break;
                }

                prepared.push_back(std::move(player));
            }

            if (!l.loadFailed) {
                l.prepared = std::move(prepared);
            }
        }

        l.loadSong = false;
        l.processing = false; // Publish data and failure status last.
        if (l.loadFailed) {
            return;
        }
    }
} catch (const std::exception &e) {
    CLog::log() << std::string("Marathon loading failed: ") + e.what();
    l.loadFailed = true;
    l.processing = false;
}

bool GPPGame::installMarathonSong(CGamePlay &module, loadThreadData &l) {
    if (l.processing || l.loadFailed || l.listEnd ||
        l.prepared.size() != module.players.size() || l.prepared.empty()) {
        return false;
    }

    module.loadSongLyrics(l.songsList[l.songID]);
    for (size_t i = 0; i < module.players.size(); ++i) {
        module.getPlayer(i).takeLoadedSong(*l.prepared[i]);
    }

    l.prepared.clear();
    module.startUpdateDelta();
    return true;
}

auto GPPGame::getDirectory(const char *dir, bool getFiles, bool getDirectories)
    -> std::deque<std::string> {
    return CGPPFileMGR::getDirectory(dir, getFiles, getDirectories);
}

auto GPPGame::caseInsensitiveSearchDir(const char *dir, bool files,
                                       bool directories,
                                       const std::string &searchName)
    -> std::string {
    for (const auto &name : getDirectory(dir, files, directories)) {
        if (caseInSensStringCompare(name, searchName)) {
            return name;
        }
    }

    return {};
}

static void defaultTextLoadFun(GPPGame::loadTextureBatch *t) {
    unsigned int *i = reinterpret_cast<unsigned int *>(t->userptr);
    *i = t->text->getTextId();
};

bool GPPGame::prepareScene() {
    const std::string requested = backgroundScene;
    const bool ok = scene.load(testobj, requested, [this]() {
        if (!CEngine::engine().windowOpened()) {
            return false;
        }

        clearScreen();
        CFonts::fonts().drawTextInScreenWithBuffer("Loading scene", -0.4, 0.0,
                                                   0.1);
        renderFrame();
        return CEngine::engine().windowOpened();
    });
    if (!ok) {
        CLog::log() << scene.error();
    }

    return ok;
}

int GPPGame::setScene(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    if (!CScene::validId(id)) {
        lua_pushboolean(L, false);
        return 1;
    }

    GuitarPP().backgroundScene = id; // Selection for the next loading boundary.
    lua_pushboolean(L, true);
    return 1;
}

int GPPGame::getScene(lua_State *L) {
    lua_pushstring(L, GuitarPP().scene.definition().id.c_str());
    return 1;
}

void GPPGame::initialLoad() {
    if (!prepareScene()) {
        if (!CEngine::engine().windowOpened()) {
            return; // Closing the loading window is cancellation, not failure.
        }
        throw std::runtime_error("Cannot prepare initial scene");
    }
    basicModels.clear();
    const std::pair<const char *, GPPOBJ *> models[] = {
        {"GPP_Note.obj", &noteOBJ},
        {"TriggerBase.obj", &triggerBASEOBJ},
        {"Trigger.obj", &triggerOBJ},
        {"Pylmbar.obj", &pylmbarOBJ},
        {"GPP_Opennote.obj", &openNoteOBJ}};
    for (const auto &entry : models) {
        auto handle = requestModel("data/models", entry.first, *entry.second);
        if (!handle) {
            throw std::runtime_error(std::string("Cannot enqueue model: ") +
                                     entry.first);
        }
        basicModels.push_back(std::move(handle));
    }

    try {
        int itext = 0;

        for (auto &s : strumsTexture3D) {
            loadTextureSingleAsync(loadTextureBatch(
                "data/sprites", "strum" + std::to_string(itext++) + ".tga", &s,
                defaultTextLoadFun));
        }

        itext = 0;

        for (auto &s : hopoTexture3D) {
            loadTextureSingleAsync(loadTextureBatch(
                "data/sprites", "hopo" + std::to_string(itext++) + ".tga", &s,
                defaultTextLoadFun));
        }

        itext = 0;

        for (auto &s : tapTexture3D) {
            loadTextureSingleAsync(loadTextureBatch(
                "data/sprites", "tap" + std::to_string(itext++) + ".tga", &s,
                defaultTextLoadFun));
        }

        itext = 0;

        for (auto &sb : sbaseTexture3D) {
            loadTextureSingleAsync(loadTextureBatch(
                "data/sprites", "base" + std::to_string(itext++) + ".tga", &sb,
                defaultTextLoadFun));
        }

        itext = 0;

        for (auto &st : striggerTexture3D) {
            loadTextureSingleAsync(loadTextureBatch(
                "data/sprites", "trigger" + std::to_string(itext++) + ".tga",
                &st, defaultTextLoadFun));
        }

        loadTextureSingleAsync(loadTextureBatch("data/sprites", "Opennote.tga",
                                                &openNoteTexture3D,
                                                defaultTextLoadFun));
        loadTextureSingleAsync(
            loadTextureBatch("data/sprites", "OpennoteHOPO.tga",
                             &openNoteHOPOTexture3D, defaultTextLoadFun));
        loadTextureSingleAsync(loadTextureBatch("data/sprites", "OpennoteP.tga",
                                                &openNotePTexture3D,
                                                defaultTextLoadFun));
        loadTextureSingleAsync(
            loadTextureBatch("data/sprites", "OpennoteHOPOP.tga",
                             &openNoteHOPOPTexture3D, defaultTextLoadFun));

    } catch (const std::exception &ex) {
        std::cout << ex.what() << std::endl;
    }
}

void GPPGame::initialLoad2() {
    // Retained startup stage; model requests are owned by initialLoad().
}

void GPPGame::parseParameters(int argc, char *argv[]) {
    for (size_t i = 0; i < argc; ++i) {
        if (argv[i] != nullptr) {
            if (argv[i][0] == '-' || argv[i][0] == '/') {
                cmdparams[&((argv[i])[1])] = true;
            }
        }
    }

    setDevMode(cmdparams["dev"]);
}

void GPPGame::setDevMode(bool mode) { devMode = mode; }

void GPPGame::testClient(const std::string &name) {
#ifdef COMPILEMP
    CPlayer p("pato voador");
    /*
    while (CEngine::engine().windowOpened())
    {
            GPPGame::GuitarPP().clearScreen();

            CFonts::fonts().drawTextInScreenWithBuffer("patos voadores", -0.4,
    0.0, 0.1);

            GPPGame::GuitarPP().renderFrame();
    }

    */

    auto &game = GPPGame::GuitarPP();
    auto realname = game.getCallBackRealName(name);
    auto &module = game.gameModules[realname];

    module.setHyperSpeed(2.5 * game.hyperSpeed);

    game.setVSyncMode(0);
    /*
    {
    module.players.push_back(CPlayer("xi 3"));
    auto &playerCamera = module.getBPlayer().playerCamera;

    //module.getBPlayer().Notes.instrument = "[ExpertDoubleBass]";

    playerCamera.eye.x = 0.65;
    playerCamera.eye.y = 0.2;
    playerCamera.eye.z = 2.3;
    playerCamera.center.x = 1.3;
    playerCamera.center.y = -0.2;
    playerCamera.center.z = 0;
    playerCamera.up.x = 0;
    playerCamera.up.y= 1;
    playerCamera.up.z = 0;
    }

    {
    module.players.push_back(CPlayer("xi 2"));

    //module.getBPlayer().Notes.instrument = "[ExpertDoubleBass]";

    auto &playerCamera = module.getBPlayer().playerCamera;

    playerCamera.eye.x = -0.65;
    playerCamera.eye.y = 0.2;
    playerCamera.eye.z = 2.3;
    playerCamera.center.x = -1.3;
    playerCamera.center.y = -0.2;
    playerCamera.center.z = 0;
    playerCamera.up.x = 0;
    playerCamera.up.y= 1;
    playerCamera.up.z = 0;
    }

    */

    module.players.push_back(std::make_unique<CPlayer>(p));
    // module.getBPlayer().enableBot = GPPGame::GuitarPP().botEnabled;

    // module.getBPlayer().Notes.instrument = "[ExpertDoubleBass]";
    CMultiplayer mp(false);
    mp.initConnections(ip, port);
    mp.setPlayersData(module.players);
    mp.connectToServer(*module.players.back());

    loadThreadData l;

    l.processing = true;

    std::thread load(loadThread, std::ref(module), std::ref(l));

    while (CEngine::engine().windowOpened() && l.processing) {
        GPPGame::GuitarPP().clearScreen();

        CFonts::fonts().drawTextInScreenWithBuffer("loading", -0.4, 0.0, 0.1);

        GPPGame::GuitarPP().renderFrame();
    }

    if (load.joinable()) {
        load.join();
    }

    if (l.loadFailed || !CEngine::engine().windowOpened()) {
        module.resetModule();
        return;
    }

    for (auto &pp : module.players) {
        auto &p = *pp;
        p.startTime = CEngine::engine().getTime() + 3.0;
        p.musicRunningTime = -3.0;
    }

    GPPGame::GuitarPP().HUDText =
        GPPGame::GuitarPP().loadTexture("data/sprites", "HUD.tga").getTextId();
    GPPGame::GuitarPP().fretboardText =
        GPPGame::GuitarPP()
            .loadTexture("data/sprites", "fretboard.tga")
            .getTextId();
    GPPGame::GuitarPP().lineText =
        GPPGame::GuitarPP().loadTexture("data/sprites", "line.tga").getTextId();
    GPPGame::GuitarPP().HOPOSText =
        -1 /*GPPGame::GuitarPP().loadTexture("data/sprites",
              "HOPOS.tga").getTextId()*/
        ;
    GPPGame::GuitarPP().pylmBarText =
        GPPGame::GuitarPP()
            .loadTexture("data/sprites", "pylmbar.tga")
            .getTextId();

    double startTime = module.getBPlayer().startTime =
        CEngine::engine().getTime() + 3.0;
    double openMenuTime = 0.0;
    module.getBPlayer().musicRunningTime = -3.0;

    bool enterInMenu = false, esc = false;
    int musicstartedg = 0;

    bool songTimeFixed = false;

    bool botK = false;

    // CLog::log() << std::to_string(module.getBPlayer().enableBot) + "bot que
    // voa";

    std::cout << "Plus in chart: " << module.getBPlayer().Notes.gPlus.size()
              << std::endl;

    if (module.getBPlayer().Notes.gPlus.size()) {
        std::cout << "Plus 0 time: " << module.getBPlayer().Notes.gPlus[0].time
                  << std::endl;
        std::cout << "Plus 0 duration: "
                  << module.getBPlayer().Notes.gPlus[0].lTime << std::endl;
    }

    bool firstStartFrame = true;

    auto &playerb = module.getBPlayer();
    std::string songName = playerb.Notes.songName,
                songArtist = playerb.Notes.songArtist,
                songCharter =
                    ui::metadataDisplayText(playerb.Notes.songCharter);

    double fadeoutdsc = CEngine::engine().getTime();

    mp.startMPThread();

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    do {
        GPPGame::GuitarPP().clearScreen();

        CFonts::fonts().drawTextInScreenWithBuffer("Getting data", -0.4, 0.0,
                                                   0.1);

        GPPGame::GuitarPP().renderFrame();
    } while (CEngine::engine().windowOpened() && !mp.playerReady());

    while (CEngine::engine().windowOpened()) {
        GPPGame::GuitarPP().clearScreen();

        if (CEngine::engine().getKey(GLFW_KEY_ESCAPE)) {
            esc = true;
        } else if (esc) {
            enterInMenu = true;
        } else {
            enterInMenu = false;
        }

        if (enterInMenu) {
            if (musicstartedg == 2) {
                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.instrumentPause();
                }

                module.getBPlayer().pauseSongAudio();
                musicstartedg = 0;
            }

            songTimeFixed = false;

            openMenuTime = CEngine::engine().getTime();

            if (game.menus.openPause() == PauseResult::Exit) {
                module.resetModule();
                break;
            }

            double time = CEngine::engine().getTime();

            for (auto &pp : module.players) {
                auto &p = *pp;
                p.startTime += time - openMenuTime;
            }

            enterInMenu = false;
            esc = false;
        } else {
            if (CEngine::engine().getKey('B')) {
                if (!botK) {
                    botK = true;
                    module.getBPlayer().enableBot ^= 1;
                }
            } else if (botK) {
                botK = false;
            }

            module.update();

            if (!songTimeFixed && module.getBPlayer().musicRunningTime > 0.5) {
                module.getBPlayer().seekSongAudio(
                    module.getBPlayer().musicRunningTime);

                for (auto &pp : module.players) {
                    auto &p = *pp;
                    CEngine::engine().setSoundTime(p.instrumentSound,
                                                   p.musicRunningTime);
                }

                // std::cout << "First note position: " <<
                // module.getBPlayer().Notes.gNotes[0].time << std::endl;

                songTimeFixed = true;
            }

            module.render();
            module.renderLyrics();

            // CFonts::fonts().drawTextInScreen("BASS" +
            // std::to_string(CEngine::engine().getSoundTime(module.getBPlayer().songAudioID)),
            // 0.52, -0.4, 0.1); CFonts::fonts().drawTextInScreen("SONG" +
            // std::to_string(CEngine::engine().getTime() - startTime), 0.52,
            // -0.52, 0.1);

            if (firstStartFrame) {
                CEngine::engine().playSoundStream(GuitarPP().startSound);
                firstStartFrame = false;
            }

            double time = CEngine::engine().getTime();

            if (musicstartedg == 0) {
                musicstartedg = 1;
            }

            if ((startTime - time) > 0.0) {

                fadeoutdsc = CEngine::engine().getTime();
                CFonts::fonts().drawTextInScreenWithBuffer(
                    std::to_string((int)(startTime - time)), -0.3, 0.0, 0.3);
                musicstartedg = 0;
            }

            double fadeoutdscAlphaCalc =
                1.0 - ((CEngine::engine().getTime() - fadeoutdsc) / 3.0);

            if (fadeoutdscAlphaCalc >= 0.0) {
                if (fadeoutdscAlphaCalc < 1.0) {
                    CEngine::engine().setColor(1.0, 1.0, 1.0,
                                               fadeoutdscAlphaCalc);
                }

                CFonts::fonts().drawTextInScreen(songName, -0.9, 0.7, 0.1);
                CFonts::fonts().drawTextInScreen(songArtist, -0.88, 0.62, 0.08);
                CFonts::fonts().drawTextInScreen(songCharter, -0.88, 0.54,
                                                 0.08);

                if (fadeoutdscAlphaCalc < 1.0) {
                    CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);
                }
            }

            if (musicstartedg == 1 &&
                module.getBPlayer().musicRunningTime >= 0.0) {
                module.getBPlayer().setSongVolume(game.songVolume);
                module.playSongAudio();

                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.instrumentPlay();
                }

                musicstartedg = 2;
            }
        }

        GPPGame::GuitarPP().renderFrame();
    }

    if (load.joinable()) {
        load.join();
    }
#else

#endif
}

/*

        module.getBPlayer().playerCamera.center.x = -0.6;
        module.getBPlayer().playerCamera.eye.x = -0.4;
        module.getBPlayer().playerHudOffsetX = 1.82;
        module.getBPlayer().playerHudOffsetY = 0.1;
        module.getBPlayer().playerCamera.eye.z = 2.55;

*/
void GPPGame::serverModule(const std::string &name) {
    int state = 0;
    auto &game = GPPGame::GuitarPP();
    auto &engine = CEngine::engine();
    auto &fonts = CFonts::fonts();
    auto &controls = CControls::controls();
    auto realname = game.getCallBackRealName(name);
    auto &module = game.gameModules[realname];

    // Menu to select the song
    game.songToLoad = game.menus.selectSong();

    if (game.songToLoad.empty()) {
        return;
    }

    module.setHyperSpeed(2.5 * game.hyperSpeed);

    module.players.push_back(game.mainPlayer);
    module.getBPlayer().playerCamera.center.x = -0.6;
    module.getBPlayer().playerCamera.eye.x = -0.4;
    module.getBPlayer().playerHudOffsetX = 1.82;
    module.getBPlayer().playerHudOffsetY = 0.1;
    module.getBPlayer().playerCamera.eye.z = 2.55;
    module.getBPlayer().enableBot = true;

    module.players.push_back(std::make_unique<CPlayer>("testp"));
    module.getBPlayer().enableBot = true;

    module.getBPlayer().playerCamera.center.x = 0.6;
    module.getBPlayer().playerCamera.eye.x = 0.4;
    module.getBPlayer().playerCamera.eye.z = 2.55;
    module.getBPlayer().playerHudOffsetX = -0.42;
    module.getBPlayer().playerHudOffsetY = 0.1;
    // module.getBPlayer().remoteControls = true;

    loadThreadData l;

    l.processing = true;

    std::thread load(loadThread, std::ref(module), std::ref(l));

    callOnDctor<void(void)> exitguard([&load, &module]() {
        if (load.joinable()) {
            {
                load.join();
            }
        }

        module.resetModule();
    });

    game.HUDText = game.loadTexture("data/sprites", "HUD.tga").getTextId();
    game.fretboardText =
        game.loadTexture("data/sprites", "fretboard.tga").getTextId();
    game.lineText = game.loadTexture("data/sprites", "line.tga").getTextId();
    game.HOPOSText = -1 /*GPPGame::GuitarPP().loadTexture("data/sprites",
                           "HOPOS.tga").getTextId()*/
        ;
    game.pylmBarText =
        game.loadTexture("data/sprites", "pylmbar.tga").getTextId();

    double startTime = 0.0;
    double openMenuTime = 0.0;
    double fadeoutdsc = engine.getTime();
    int musicstartedg = 0;
    bool firstStartFrame = true;

    bool enterInMenu = false;
    bool esc = false;

    bool songTimeFixed = false;

    std::string songName;
    std::string songArtist;
    std::string songCharter;

    CPlayer *pplayerb = &module.getBPlayer();

    game.setVSyncMode(0);

    while (engine.windowOpened()) {
        game.clearScreen();

        switch (state) {
            // Load thread state
        case 0:

            if (!l.processing) {
                if (l.loadFailed) {
                    return;
                }

                ++state;
            }

            CFonts::fonts().drawTextInScreenWithBuffer("loading", -0.4, 0.0,
                                                       0.1);
            break;

            // Timing and loadings
        case 1: {
            CFonts::fonts().drawTextInScreenWithBuffer("loading", -0.4, 0.0,
                                                       0.1);

            auto &playerb = *pplayerb;
            songName = ui::metadataDisplayText(playerb.songName);
            songArtist = ui::metadataDisplayText(playerb.songArtist);
            songCharter = ui::metadataDisplayText(playerb.songCharter);
            module.getBPlayer().guitar =
                CGuitars::inst().getGuitarIfExists(game.defaultGuitar);

            for (auto &pp : module.players) {
                auto &p = *pp;
                p.startTime = engine.getTime() + 3.0;
                p.musicRunningTime = -3.0;
            }

            startTime = playerb.startTime = engine.getTime() + 3.0;
            playerb.musicRunningTime = -3.0;
            module.startSongTime =
                std::chrono::steady_clock::now() + std::chrono::seconds(3);

            if (playerb.guitar != nullptr) {
                {
                    if (!playerb.guitar->load()) {
                        CLog::log()
                            << "Guitar loading failed; using the default model";
                    }
                }
            }

            playerb.enableBot = game.botEnabled;
            playerb.playerCamera.center.y = 1.0;
            playerb.playerCamera.eye.z += 2.0;
            state++;
        } break;

            // Play state
        case 2: {
            auto &playerb = *pplayerb;

            module.update();

            // Init camera effect
            for (auto &pp : module.players) {
                auto &p = *pp;
                if (p.targetCamera.center.y < p.playerCamera.center.y) {
                    p.playerCamera.center.y -= engine.getDeltaTime() * 0.5;
                }

                if (p.targetCamera.center.y > p.playerCamera.center.y) {
                    p.playerCamera.center.y = p.targetCamera.center.y;
                }

                if (p.targetCamera.eye.z < p.playerCamera.eye.z) {
                    p.playerCamera.eye.z -= engine.getDeltaTime() * 0.75;
                }

                if (p.targetCamera.eye.z > p.playerCamera.eye.z) {
                    p.playerCamera.eye.z = p.targetCamera.eye.z;
                }
            }

            game.gameplayRunningTime = playerb.musicRunningTime;
            game.gamePlayPlusEnabled = playerb.plusEnabled;

            // Countdown
            if (!songTimeFixed && (engine.getTime() - startTime) > 0.5) {
                module.getBPlayer().seekSongAudio(playerb.musicRunningTime);

                for (auto &pp : module.players) {
                    auto &p = *pp;
                    engine.setSoundTime(p.instrumentSound, p.musicRunningTime);
                }

                // std::cout << "First note position: " <<
                // module.getBPlayer().Notes.gNotes[0].time << std::endl;

                songTimeFixed = true;
            }

            /////////////////

            // Rendering gameplay
            module.render();
            module.renderLyrics();

            // First gameplay frame things
            if (firstStartFrame) {
                CLuaH::Lua().runEvent(firstStartFrameSE);
                engine.playSoundStream(GuitarPP().startSound);
                firstStartFrame = false;
            }

            double time = engine.getTime();

            if (musicstartedg == 0) {
                {
                    musicstartedg = 1;
                }
            }

            // Info text show and effect
            if ((startTime - time) > 0.0) {
                fadeoutdsc = engine.getTime();
                std::string count = std::to_string((int)(startTime - time));

                fonts.drawTextInScreenWithBuffer(
                    count, fonts.getCenterPos(count, 0.3, 0.0), 0.0, 0.3);
                musicstartedg = 0;
            }

            double fadeoutdscAlphaCalc =
                1.0 - ((engine.getTime() - fadeoutdsc) / 3.0);

            if (fadeoutdscAlphaCalc >= 0.0) {
                if (fadeoutdscAlphaCalc < 1.0) {
                    {
                        engine.setColor(1.0, 1.0, 1.0, fadeoutdscAlphaCalc);
                    }
                }

                fonts.drawTextInScreen(songName, -0.9, 0.7, 0.1);
                fonts.drawTextInScreen(songArtist, -0.88, 0.62, 0.08);
                fonts.drawTextInScreen(songCharter, -0.88, 0.54, 0.08);

                if (fadeoutdscAlphaCalc < 1.0) {
                    {
                        engine.setColor(1.0, 1.0, 1.0, 1.0);
                    }
                }
            }

            // Music states: play
            if (musicstartedg == 1) {
                module.getBPlayer().setSongVolume(game.songVolume);
                module.playSongAudio();

                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.instrumentPlay();
                }

                musicstartedg = 2;
            }

            // Handles ESC and if true, goto state 3 (Pauses game)
            if (controls.keyEscape() != 0) {
                state++;
            }
        } break;

            // Pause state
        case 3: {
            // Pauses audio
            if (musicstartedg == 2) {
                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.instrumentPause();
                }

                module.getBPlayer().pauseSongAudio();
                musicstartedg = 0;
            }

            songTimeFixed = false;

            //
            openMenuTime = engine.getTime();

            // Pause menu
            if (game.menus.openPause() == PauseResult::Exit) {
                // Exit gameplay
                return;
            }

            double time = engine.getTime();

            for (auto &pp : module.players) {
                auto &p = *pp;
                p.startTime += time - openMenuTime;
            }

            // Back to gameplay
            state = 2;
        } break;

            // Bug state
        default:
            break;
        }

        game.renderFrame();
    }
}

namespace {
// Owned by the main thread. Quick Play calls finish only after joining its
// worker; marathon calls it before installing the next prepared song.
class SongRun {
    GPPGame &game;
    CGamePlay &module;
    std::string active;

  public:
    SongRun(GPPGame &game, CGamePlay &module) : game(game), module(module) {
        game.clearSongResults();
    }

    void start(const std::string &id, double seconds) {
        active = id;
        for (auto &p : module.players) {
            p->beginSongResult();
        }

        game.scene.songStarted(id, seconds);
    }

    bool finish(SongEndReason reason) {
        if (active.empty()) {
            return false;
        }

        SongResult result;
        result.songId = std::move(active);
        active.clear();
        result.reason = reason;
        if (!module.players.empty()) {
            result.elapsedSeconds =
                std::max(0.0, module.getBPlayer().musicRunningTime);
        }

        for (const auto &p : module.players) {
            result.players.push_back(p->songResult());
        }

        game.recordSongResult(std::move(result));
        return true;
    }

    void cancelled(const std::string &id) {
        SongResult result;
        result.songId = id;
        result.reason = SongEndReason::Aborted;
        game.recordSongResult(std::move(result));
    }

    void failed(const std::string &id) {
        SongResult result;
        result.songId = id;
        game.recordSongResult(std::move(result));
    }
};
} // namespace

std::atomic<bool> pausegplay;
std::atomic<bool> continuegplay;

static void updateGamePLay(GPPGame &game, CGamePlay &module,
                           std::atomic<uint64_t> &pauseRequest,
                           std::atomic<uint64_t> &pauseAck,
                           std::atomic<bool> &finished) {
    bool wasPaused = true;
    while (continuegplay) {
        if (pausegplay) {
            wasPaused = true;
            pauseAck =
                pauseRequest
                    .load(); // Publish quiescence for this pause generation.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            if (wasPaused) {
                module.startUpdateDelta();
                wasPaused = false;
            }

            if (module.update()) {
                finished = true;
                break;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }

    pauseAck = pauseRequest.load();
}

void GPPGame::startModule(const std::string &name) {
    continuegplay = true;
    pausegplay = true;
    int state = 0;
    auto &game = GPPGame::GuitarPP();
    auto &engine = CEngine::engine();
    auto &fonts = CFonts::fonts();
    auto &controls = CControls::controls();
    auto realname = game.getCallBackRealName(name);
    auto &module = game.gameModules[realname];

    // Menu to select the song
    game.songToLoad = game.menus.selectSong();

    if (game.songToLoad.empty()) {
        return;
    }

    game.clearSongResults();
    if (!game.prepareScene()) {
        return;
    }

    module.setHyperSpeed(2.5 * game.hyperSpeed);

    module.players.push_back(game.mainPlayer);
    loadThreadData l;
    SongRun songRun(game, module);
    std::atomic<uint64_t> pauseRequest{0}, pauseAck{0};
    std::atomic<bool> songFinished{false};
    SongEndReason endReason = SongEndReason::Aborted;

    l.processing = true;

    std::thread load(loadThread, std::ref(module), std::ref(l));
    std::thread updateThread(updateGamePLay, std::ref(game), std::ref(module),
                             std::ref(pauseRequest), std::ref(pauseAck),
                             std::ref(songFinished));

    callOnDctor<void(void)> exitguard([&]() {
        pausegplay = true;
        continuegplay = false;
        if (load.joinable()) {
            {
                load.join();
            }
        }

        if (updateThread.joinable()) {
            {
                updateThread.join();
            }
        }

        // Neither loader nor gameplay worker may still access player data.
        if (l.loadFailed) {
            songRun.failed(game.songToLoad);
        } else if (!songRun.finish(songFinished ? SongEndReason::Completed
                                                : endReason)) {
            songRun.cancelled(game.songToLoad); // Closed while still loading.
        }

        game.scene.sessionEnded(module.players.empty()
                                    ? 0.0
                                    : module.getBPlayer().musicRunningTime);
        module.resetModule();
    });

    auto defaultTextLoadFun = [](loadTextureBatch *t) {
        if (t != nullptr) {
            unsigned int *i = reinterpret_cast<unsigned int *>(t->userptr);

            if ((i != nullptr) && (t->text != nullptr)) {
                {
                    *i = t->text->getTextId();
                }
            }
        }
    };

    game.loadTextureSingleAsync(loadTextureBatch(
        "data/sprites", "HUD.tga", &game.HUDText, defaultTextLoadFun));
    game.loadTextureSingleAsync(
        loadTextureBatch("data/sprites", "fretboard.tga", &game.fretboardText,
                         defaultTextLoadFun));
    game.loadTextureSingleAsync(loadTextureBatch(
        "data/sprites", "line.tga", &game.lineText, defaultTextLoadFun));

    game.loadTextureSingleAsync(loadTextureBatch(
        "data/sprites", "pylmbar.tga", &game.pylmBarText, defaultTextLoadFun));

    // game.HUDText = game.loadTexture("data/sprites", "HUD.tga").getTextId();
    // game.fretboardText = game.loadTexture("data/sprites",
    // "fretboard.tga").getTextId(); game.lineText =
    // game.loadTexture("data/sprites", "line.tga").getTextId();
    game.HOPOSText = -1 /*GPPGame::GuitarPP().loadTexture("data/sprites",
                           "HOPOS.tga").getTextId()*/
        ;
    // game.pylmBarText = game.loadTexture("data/sprites",
    // "pylmbar.tga").getTextId();

    double startTime = 0.0;
    double openMenuTime = 0.0;
    double fadeoutdsc = engine.getTime();
    int musicstartedg = 0;
    bool firstStartFrame = true;

    bool enterInMenu = false;
    bool esc = false;

    bool songTimeFixed = false;

    std::string songName;
    std::string songArtist;
    std::string songCharter;

    CPlayer *pplayerb = &module.getBPlayer();

    game.setVSyncMode(0);

    CEngine::RenderDoubleStruct RenderData{};

    RenderData.x1 = -1.3;
    RenderData.x2 = 1.3;
    RenderData.x3 = 1.3;
    RenderData.x4 = -1.3;

    RenderData.y1 = 1.0;
    RenderData.y2 = 1.0;
    RenderData.y3 = -1.0;
    RenderData.y4 = -1.0;

    RenderData.z1 = 0.0;
    RenderData.z2 = 0.0;
    RenderData.z3 = 0.0;
    RenderData.z4 = 0.0;

    RenderData.TextureX1 = 0.0;
    RenderData.TextureX2 = 1.0;
    RenderData.TextureY1 = 1.0;
    RenderData.TextureY2 = 0.0;

    unsigned int bgVideoText = engine.genNewGLTexture();
    RenderData.Text = bgVideoText;

    bool updatedTestBG = false;
    bool showTexts = true;

    while (engine.windowOpened()) {
        if (songFinished) {
            endReason = SongEndReason::Completed;
            return;
        }

        if (state != 3) {
            game.clearScreen();
        }

        switch (state) {
            // Load thread state
        case 0:

            if (!l.processing) {
                if (l.loadFailed) {
                    return;
                }

                if (game.getNumTexturesToLoad() == 0) {
                    ++state;
                }
            }

            CFonts::fonts().drawTextInScreenWithBuffer("loading", -0.4, 0.0,
                                                       0.1);
            break;

            // Timing and loadings
        case 1: {
            CFonts::fonts().drawTextInScreenWithBuffer("loading", -0.4, 0.0,
                                                       0.1);

            auto &playerb = *pplayerb;
            songName = ui::metadataDisplayText(playerb.songName);
            songArtist = ui::metadataDisplayText(playerb.songArtist);
            songCharter = ui::metadataDisplayText(playerb.songCharter);
            module.getBPlayer().guitar =
                CGuitars::inst().getGuitarIfExists(game.defaultGuitar);

            for (auto &pp : module.players) {
                auto &p = *pp;
                p.startTime = engine.getTime() + 3.0;
                p.musicRunningTime = -3.0;
            }

            startTime = playerb.startTime = engine.getTime() + 3.0;
            playerb.musicRunningTime = -3.0;
            module.startSongTime =
                std::chrono::steady_clock::now() + std::chrono::seconds(3);

            if (playerb.guitar != nullptr) {
                {
                    if (!playerb.guitar->load()) {
                        CLog::log()
                            << "Guitar loading failed; using the default model";
                    }
                }
            }

            playerb.enableBot = game.botEnabled;
            playerb.playerCamera.center.y = 1.0;
            playerb.playerCamera.eye.z += 2.0;

            game.scene.sessionStarted(game.songToLoad, -3.0);
            songRun.start(game.songToLoad, -3.0);
            // Hooks/loading do not consume the countdown.
            startTime = engine.getTime() + 3.0;
            for (auto &p : module.players) {
                p->startTime = startTime;
            }

            module.startSongTime =
                std::chrono::steady_clock::now() + std::chrono::seconds(3);
            pausegplay = false;
            state++;
        } break;

            // Play state
        case 2: {
            auto &playerb = *pplayerb;

            // module.update();

            // Init camera effect
            for (auto &pp : module.players) {
                auto &p = *pp;
                if (p.targetCamera.center.y < p.playerCamera.center.y) {
                    p.playerCamera.center.y -= engine.getDeltaTime() * 0.5;
                }

                if (p.targetCamera.center.y > p.playerCamera.center.y) {
                    p.playerCamera.center.y = p.targetCamera.center.y;
                }

                if (p.targetCamera.eye.z < p.playerCamera.eye.z) {
                    p.playerCamera.eye.z -= engine.getDeltaTime() * 0.75;
                }

                if (p.targetCamera.eye.z > p.playerCamera.eye.z) {
                    p.playerCamera.eye.z = p.targetCamera.eye.z;
                }
            }

            game.gameplayRunningTime = playerb.musicRunningTime;
            game.gamePlayPlusEnabled = playerb.plusEnabled;

            // Countdown
            if (!songTimeFixed && (engine.getTime() - startTime) > 0.5) {
                module.getBPlayer().seekSongAudio(playerb.musicRunningTime);

                for (auto &pp : module.players) {
                    auto &p = *pp;
                    engine.setSoundTime(p.instrumentSound, p.musicRunningTime);
                }

                // std::cout << "First note position: " <<
                // module.getBPlayer().Notes.gNotes[0].time << std::endl;

                songTimeFixed = true;
            }

            if (module.renderBackground()) {
                CEngine::cameraSET usingCamera;
                usingCamera.eye.x = 0.0;
                usingCamera.eye.y = 0.0;
                usingCamera.eye.z = 2.3;
                usingCamera.center.x = 0;
                usingCamera.center.y = 0;
                usingCamera.center.z = 0.0;
                usingCamera.up.x = 0;
                usingCamera.up.y = 1;
                usingCamera.up.z = 0;

                engine.setCamera(usingCamera);
            }

            // Rendering gameplay
            module.render();
            module.renderLyrics();

            // First gameplay frame things
            if (firstStartFrame) {
                CLuaH::Lua().runEvent(firstStartFrameSE);
                engine.playSoundStream(GuitarPP().startSound);
                firstStartFrame = false;
            }

            double time = engine.getTime();

            if (musicstartedg == 0) {
                {
                    musicstartedg = 1;
                }
            }

            if (showTexts) {
                // Info text show and effect
                if ((startTime - time) > 0.0) {
                    fadeoutdsc = engine.getTime();
                    std::string count = std::to_string((int)(startTime - time));

                    fonts.drawTextInScreenWithBuffer(
                        count, fonts.getCenterPos(count, 0.3, 0.0), 0.0, 0.3);
                    musicstartedg = 0;
                }

                double fadeoutdscAlphaCalc =
                    1.0 - ((engine.getTime() - fadeoutdsc) / 3.0);

                if (fadeoutdscAlphaCalc >= 0.0) {
                    if (fadeoutdscAlphaCalc < 1.0) {
                        {
                            engine.setColor(1.0, 1.0, 1.0, fadeoutdscAlphaCalc);
                        }
                    }

                    fonts.drawTextInScreen(songName, -0.9, 0.7, 0.1);
                    fonts.drawTextInScreen(songArtist, -0.88, 0.62, 0.08);
                    fonts.drawTextInScreen(songCharter, -0.88, 0.54, 0.08);

                    if (fadeoutdscAlphaCalc < 1.0) {
                        {
                            engine.setColor(1.0, 1.0, 1.0, 1.0);
                        }
                    }
                } else {
                    showTexts = false;
                    songTimeFixed = false;
                }
            }

            // Music states: play
            if (musicstartedg == 1) {
                module.getBPlayer().setSongVolume(game.songVolume);
                module.playSongAudio();

                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.instrumentPlay();
                }

                musicstartedg = 2;
            }

            // Handles ESC and if true, goto state 3 (Pauses game)
            if (controls.keyEscape() != 0) {
                pausegplay = true;
                state++;
                // Complete the pause handshake before any potentially blocked
                // swap.
                continue;
            }
        } break;

            // Pause state
        case 3: {
            const auto request = ++pauseRequest;
            while (pauseAck != request && !songFinished) {
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }

            if (songFinished) {
                endReason = SongEndReason::Completed;
                return;
            }

            const auto pauseStart = std::chrono::steady_clock::now();
            // Pauses audio
            const bool resumeAudio = musicstartedg == 2;
            if (resumeAudio) {
                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.instrumentPause();
                }

                module.getBPlayer().pauseSongAudio();
                musicstartedg = 0;
            }

            songTimeFixed = false;

            //
            openMenuTime = engine.getTime();

            // Pause menu
            if (game.menus.openPause() == PauseResult::Exit) {
                // Exit gameplay
                return;
            }

            // Resume audio before releasing the worker and before rendering
            // again.
            if (resumeAudio) {
                module.playSongAudio();
                for (auto &p : module.players) {
                    p->instrumentPlay();
                }

                musicstartedg = 2;
            }

            const auto paused = std::chrono::steady_clock::now() - pauseStart;
            module.compensatePause(paused);
            startTime += std::chrono::duration<double>(paused).count();
            pausegplay = false;
            state = 2;
            continue;
        } break;

            // Bug state
        default:
            break;
        }

        game.renderFrame();
    }
}

void GPPGame::startMarathonModule(const std::string &name) {
    loadThreadData l;

    auto &game = GPPGame::GuitarPP();
    auto &engine = CEngine::engine();
    auto &fonts = CFonts::fonts();
    auto realname = game.getCallBackRealName(name);
    auto &module = game.gameModules[realname];

    game.clearSongResults();
    if (!game.prepareScene()) {
        return;
    }

    game.HUDText = game.loadTexture("data/sprites", "HUD.tga").getTextId();
    game.fretboardText =
        game.loadTexture("data/sprites", "fretboard.tga").getTextId();
    game.lineText = game.loadTexture("data/sprites", "line.tga").getTextId();
    game.HOPOSText = -1 /*GPPGame::GuitarPP().loadTexture("data/sprites",
                           "HOPOS.tga").getTextId()*/
        ;
    game.pylmBarText =
        game.loadTexture("data/sprites", "pylmbar.tga").getTextId();

    module.setHyperSpeed(2.5 * game.hyperSpeed);

    game.setVSyncMode(0);

    module.players.push_back(game.mainPlayer);
    module.fretboardLightFade = 20.0;

    l.processing = true;
    l.continueThread = true;
    l.songID = 0;
    l.songsList = ((game.marathonSongsList.empty())
                       ? getDirectory("./data/songs", false, true)
                       : std::move(game.marathonSongsList));
    l.listEnd = false;
    l.loadSong = true;

    for (auto &p : l.songsList) {
        std::cout << p << std::endl;
    }

    // Keep the old exclusion against multiplayer container changes, but let
    // only this thread install live song data. The worker owns prepared data.
    std::unique_lock<std::mutex> sessionPlayersLock(playersMutex);
    for (auto &p : module.players) {
        p->resetSongThings();
        l.instruments.push_back(p->instrument);
    }

    SongRun songRun(game, module);
    bool reportedLoadFailure = false;
    std::thread load(loadMarathonThread, std::ref(l));
    callOnDctor<void(void)> exitguard([&]() {
        l.continueThread = false;
        if (load.joinable()) {
            load.join();
        }

        if (!songRun.finish(SongEndReason::Aborted) &&
            game.getSongResults().empty() && !l.loadFailed &&
            !l.songsList.empty()) {
            songRun.cancelled(l.songsList.front());
        }

        if (l.loadFailed && !reportedLoadFailure &&
            l.songID < l.songsList.size()) {
            songRun.failed(l.songsList[l.songID]);
        }

        game.scene.sessionEnded(module.players.empty()
                                    ? 0.0
                                    : module.getBPlayer().musicRunningTime);
        module.resetModule();
    });

    while (engine.windowOpened() && l.processing && !l.loadFailed) {
        game.clearScreen();

        CFonts::fonts().drawTextInScreenWithBuffer("loading marathon 0%", -0.4,
                                                   0.0, 0.1);

        game.renderFrame();
    }

    if (!engine.windowOpened() || l.loadFailed) {
        return;
    }

    if (!installMarathonSong(module, l)) {
        return;
    }

    l.songID++;
    l.processing = true;
    l.loadSong = true;

    double startWaitTime = 10.0;

    for (auto &pp : module.players) {
        auto &p = *pp;
        p.startTime = engine.getTime() + startWaitTime;
        p.musicRunningTime = -startWaitTime;
    }

    module.startSongTime =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(static_cast<int64_t>(1000 * startWaitTime));

    double startTime = module.getBPlayer().startTime =
        engine.getTime() + startWaitTime;
    double openMenuTime = 0.0;
    module.getBPlayer().musicRunningTime = -startWaitTime;
    module.getBPlayer().guitar =
        CGuitars::inst().getGuitarIfExists(game.defaultGuitar);

    if (module.getBPlayer().guitar != nullptr) {
        {
            if (!module.getBPlayer().guitar->load()) {
                CLog::log() << "Guitar loading failed; using the default model";
            }
        }
    }

    bool enterInMenu = false;
    bool esc = false;
    int musicstartedg = 0;

    bool songTimeFixed = false;

    module.getBPlayer().enableBot = game.botEnabled;
    module.getBPlayer().playerCamera.center.y = 1.0;
    module.getBPlayer().playerCamera.eye.z += 2.0;

    bool firstStartFrame = true;

    auto &playerb = module.getBPlayer();
    std::string songName = ui::metadataDisplayText(playerb.songName);
    std::string songArtist = ui::metadataDisplayText(playerb.songArtist);
    std::string songCharter = ui::metadataDisplayText(playerb.songCharter);

    double fadeoutdsc = engine.getTime();

    bool interval = false;
    bool waitingIntervalLoad = false;
    bool bOldShowBPMLines = module.showBPMLines;

    double marathonTime = 0.0;

    game.scene.sessionStarted(l.songsList.front(), -startWaitTime);
    songRun.start(l.songsList.front(), -startWaitTime);
    startTime = engine.getTime() + startWaitTime;
    for (auto &p : module.players) {
        p->startTime = startTime;
    }

    module.startSongTime =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(static_cast<int64_t>(1000 * startWaitTime));
    module.startUpdateDelta();

    while (engine.windowOpened()) {
        game.clearScreen();

        if (engine.getKey(GLFW_KEY_ESCAPE) != 0) {
            esc = true;
        } else if (esc) {
            enterInMenu = true;
        } else {
            enterInMenu = false;
        }

        if (enterInMenu) {
            const auto pauseStart = std::chrono::steady_clock::now();
            if (musicstartedg == 2) {
                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.instrumentPause();
                }

                module.getBPlayer().pauseSongAudio();
                musicstartedg = 0;
            }

            songTimeFixed = false;

            openMenuTime = engine.getTime();

            if (game.menus.openPause() == PauseResult::Exit) {
                break;
            }

            double time = engine.getTime();

            for (auto &pp : module.players) {
                auto &p = *pp;
                p.startTime += time - openMenuTime;
            }

            module.startSongTime +=
                std::chrono::steady_clock::now() - pauseStart;
            startTime += time - openMenuTime;
            module.startUpdateDelta();
            enterInMenu = false;
            esc = false;
        } else {
            if (!interval) {
                {
                    module.marathonUpdate();
                }
            } else {
                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.musicRunningTime +=
                        engine.getDeltaTime() * module.getgSpeed();
                }
            }

            bool songChartEnd = true;

            for (auto &pp : module.players) {
                auto &p = *pp;
                if (!interval) {
                    if (songChartEnd) {
                        {
                            songChartEnd = p.isSongChartFinished();
                        }
                    }
                }

                if (p.targetCamera.center.y < p.playerCamera.center.y) {
                    p.playerCamera.center.y -= engine.getDeltaTime() * 0.5;
                }

                if (p.targetCamera.center.y > p.playerCamera.center.y) {
                    p.playerCamera.center.y = p.targetCamera.center.y;
                }

                if (p.targetCamera.eye.z < p.playerCamera.eye.z) {
                    p.playerCamera.eye.z -= engine.getDeltaTime() * 0.75;
                }

                if (p.targetCamera.eye.z > p.playerCamera.eye.z) {
                    p.playerCamera.eye.z = p.targetCamera.eye.z;
                }
            }

            interval = songChartEnd;

            if (interval) {
                if (!waitingIntervalLoad) {
                    songRun.finish(SongEndReason::Completed);
                    if (l.songID >= l.songsList.size()) {
                        return;
                    }

                    waitingIntervalLoad = true;
                    module.showBPMLines = false;
                } else {
                    if (l.processing) {
                        //// TODO

                    } else {
                        if (l.loadFailed) {
                            songRun.failed(l.songsList[l.songID]);
                            reportedLoadFailure = true;
                            return;
                        }

                        double waitTime = 10.0;

                        double deltaTol = engine.getDeltaTime();

                        if (deltaTol < 0.005) {
                            {
                                deltaTol = 0.005;
                            }
                        }

                        deltaTol /= 2.0;

                        double dmaxrntime = 0.0;

                        double rningTimeFretCalcMn =
                            module.fretboardPositionCalcByT(
                                module.getBPlayer().musicRunningTime -
                                    deltaTol - 4.0,
                                1.02, &dmaxrntime);
                        double rningTimeFretCalcMs =
                            module.fretboardPositionCalcByT(
                                module.getBPlayer().musicRunningTime +
                                    deltaTol - 4.0,
                                1.02);

                        rningTimeFretCalcMn = rningTimeFretCalcMn < 0.0
                                                  ? -rningTimeFretCalcMn
                                                  : rningTimeFretCalcMn;
                        rningTimeFretCalcMs = rningTimeFretCalcMs < 0.0
                                                  ? -rningTimeFretCalcMs
                                                  : rningTimeFretCalcMs;

                        double fretBoardPos =
                            module.fretboardPositionCalcByT(-5 - 4.0, 1.02);

                        if (fretBoardPos > 0.0) {
                            fretBoardPos = dmaxrntime - fretBoardPos;
                        } else {
                            fretBoardPos *= -1.0;
                        }

                        if (fretBoardPos > rningTimeFretCalcMn &&
                            fretBoardPos < rningTimeFretCalcMs) {
                            if (!installMarathonSong(module, l)) {
                                return;
                            }

                            songRun.start(l.songsList[l.songID], -waitTime);
                            for (auto &pp : module.players) {
                                auto &p = *pp;
                                p.startTime = engine.getTime() + waitTime;
                                p.musicRunningTime = -waitTime;
                            }

                            startTime = module.getBPlayer().startTime =
                                engine.getTime() + waitTime;
                            openMenuTime = 0.0;
                            module.getBPlayer().musicRunningTime = -waitTime;
                            module.startSongTime =
                                std::chrono::steady_clock::now() +
                                std::chrono::milliseconds(
                                    static_cast<int64_t>(1000 * waitTime));

                            songTimeFixed = false;
                            musicstartedg = 0;

                            l.songID++;
                            interval = false;
                            l.processing = true;
                            l.loadSong = true;
                            waitingIntervalLoad = false;

                            auto &playerb = module.getBPlayer();
                            songName =
                                ui::metadataDisplayText(playerb.songName);
                            songArtist =
                                ui::metadataDisplayText(playerb.songArtist);
                            songCharter =
                                ui::metadataDisplayText(playerb.songCharter);
                            module.showBPMLines = bOldShowBPMLines;
                        }
                    }
                }
            } else {
                if (!songTimeFixed && (engine.getTime() - startTime) > 0.5) {
                    module.getBPlayer().seekSongAudio(
                        module.getBPlayer().musicRunningTime);

                    for (auto &pp : module.players) {
                        auto &p = *pp;
                        engine.setSoundTime(p.instrumentSound,
                                            p.musicRunningTime);
                    }

                    songTimeFixed = true;
                }
            }

            {

                engine.activate3DRender(true);
                engine.activateLighting(true);

                {
                    double centerx = 0.0;
                    double centerz = -650.0;

                    double rtime = marathonTime / 10.0;
                    double eyexcam = sin(0) * 1.0 + centerx + sin(rtime) * 1.0;
                    double eyezcam = cos(0) * 1.0 + centerz + cos(rtime) * 1.0;

                    CEngine::cameraSET usingCamera;

                    usingCamera.eye.x = 3.0 + sin(rtime);
                    usingCamera.eye.y = 2.5;
                    usingCamera.eye.z = 1.0 + cos(rtime);
                    usingCamera.center.x = 3.0;
                    usingCamera.center.y = 0.5;
                    usingCamera.center.z = -5;
                    usingCamera.up.x = 0;
                    usingCamera.up.y = 1;
                    usingCamera.up.z = 0;
                    /*usingCamera.eye.x = eyexcam;
                    usingCamera.eye.y = 130.0;
                    usingCamera.eye.z = eyezcam;
                    usingCamera.center.x = centerx;
                    usingCamera.center.y = 130.0;
                    usingCamera.center.z = centerz;
                    usingCamera.up.x = 0;
                    usingCamera.up.y= 1;
                    usingCamera.up.z = 0;*/

                    engine.setCamera(usingCamera);
                }

                {
                    lightData l{};

                    for (auto &t : l.ambientLight) {
                        t = 0.1f;
                    }

                    for (auto &t : l.direction) {
                        t = 2.5f;
                    }

                    for (auto &t : l.position) {
                        t = 0.0f;
                    }

                    for (auto &t : l.specularLight) {
                        t = 0.2f;
                    }

                    for (auto &t : l.diffuseLight) {
                        t = 0.2f;
                    }

                    l.specularLight[1] = 1.0f;
                    l.specularLight[2] = 1.0f;
                    l.diffuseLight[0] = 1.0f;
                    l.diffuseLight[1] = 1.0f;
                    l.ambientLight[3] = 0.1f;

                    CEngine::colorRGBToArrayf(0xFFF6ED, l.diffuseLight);

                    l.angle = 180.0;
                    l.direction[0] = 3.0f;
                    l.direction[1] = -0.5f;
                    l.direction[2] = -1.5f;

                    l.position[0] = 3.0f;
                    l.position[1] = 2.7f;
                    l.position[2] = -1.5f;
                    l.position[3] = 1.0f;

                    engine.activateLight(0, false);
                    engine.activateLight(1, true);
                    engine.setLight(l, 1);
                }

                /*{
                lightData l;

                for (auto &t : l.ambientLight)
                {
                t = 0.1;
                }

                for (auto &t : l.direction)
                {
                t = 2.5;
                }

                for (auto &t : l.position)
                {
                t = 0.0;
                }

                for (auto &t : l.specularLight)
                {
                t = 1.0;
                }

                for (auto &t : l.diffuseLight)
                {
                t = 0.5;
                }

                l.specularLight[1] = 1.0;
                l.specularLight[2] = 1.0;
                l.specularLight[3] = 0.5;
                l.diffuseLight[3] = 0.5;
                l.ambientLight[3] = 0.1;

                l.angle = 80.0;
                l.direction[0] = 100.0;
                l.direction[1] = -0.5;
                l.direction[2] = 0.0;

                l.position[3] = 0.0;
                l.position[1] = 50.0;
                l.position[2] = 100.5;

                engine.activateLight(0, true);
                engine.setLight(l, 0);
                }*/

                // static const unsigned int cityTextID =
                // game.loadTexture("test", "city.tga").getTextId();
                engine.activateNormals(true);

                CBackgroundRenderer::inst().draw(game.testobj);
                engine.activateNormals(false);

                engine.activateLighting(false);
                engine.activate3DRender(false);

                /*{
                        double size = 0.5;

                        CEngine::RenderDoubleStruct TempStruct3D;

                        static auto hopoLightText =
                GPPGame::GuitarPP().loadTexture("data/sprites",
                "hopolight.tga").getTextId();

                        double flarex = 2.92, flarey = 2.54, flarez = -1.5;

                        TempStruct3D.Text = hopoLightText;
                        TempStruct3D.TextureX1 = 0.0;
                        TempStruct3D.TextureX2 = 1.0;
                        TempStruct3D.TextureY1 = 1.0;
                        TempStruct3D.TextureY2 = 0.0;

                        //CFonts::fonts().drawTextInScreen(std::to_string(hopostp.size()),
                0.0, 0.5, 0.1);

                        TempStruct3D.x1 = flarex;
                        TempStruct3D.x2 = TempStruct3D.x1 + size;
                        TempStruct3D.x3 = TempStruct3D.x1 + size;
                        TempStruct3D.x4 = TempStruct3D.x1;

                        TempStruct3D.y1 = flarey;
                        TempStruct3D.y2 = TempStruct3D.y1;
                        TempStruct3D.y3 = flarey + 0.2;
                        TempStruct3D.y4 = TempStruct3D.y3;

                        TempStruct3D.z1 = flarez + size * 2.0;
                        TempStruct3D.z2 = TempStruct3D.z1;
                        TempStruct3D.z3 = flarez;
                        TempStruct3D.z4 = TempStruct3D.z3;

                        CEngine::engine().Render3DQuad(TempStruct3D);
                }*/

                engine.matrixReset();

                engine.clear3DBuffer();

                {
                    CEngine::cameraSET usingCamera;
                    usingCamera.eye.x = 0.0;
                    usingCamera.eye.y = 0.0;
                    usingCamera.eye.z = 2.3;
                    usingCamera.center.x = 0;
                    usingCamera.center.y = 0;
                    usingCamera.center.z = 0.0;
                    usingCamera.up.x = 0;
                    usingCamera.up.y = 1;
                    usingCamera.up.z = 0;

                    engine.setCamera(usingCamera);
                }
            }

            module.render();

            if (!interval) {
                {
                    module.renderLyrics();
                }
            }

            if (firstStartFrame) {
                engine.playSoundStream(GuitarPP().startSound);
                firstStartFrame = false;
            }

            double time = engine.getTime();

            if (musicstartedg == 0) {
                {
                    musicstartedg = 1;
                }
            }

            if ((startTime - time) > 0.0) {

                fadeoutdsc = engine.getTime();

                std::string count = std::to_string((int)(startTime - time));

                fonts.drawTextInScreenWithBuffer(
                    count, fonts.getCenterPos(count, 0.3, 0.0), 0.0, 0.3);
                musicstartedg = 0;
            }

            double fadeoutdscAlphaCalc =
                1.0 - ((engine.getTime() - fadeoutdsc) / 3.0);

            if (fadeoutdscAlphaCalc >= 0.0) {
                if (fadeoutdscAlphaCalc < 1.0) {
                    {
                        engine.setColor(1.0, 1.0, 1.0, fadeoutdscAlphaCalc);
                    }
                }

                fonts.drawTextInScreen(songName, -0.9, 0.7, 0.1);
                fonts.drawTextInScreen(songArtist, -0.88, 0.62, 0.08);
                fonts.drawTextInScreen(songCharter, -0.88, 0.54, 0.08);

                if (fadeoutdscAlphaCalc < 1.0) {
                    {
                        engine.setColor(1.0, 1.0, 1.0, 1.0);
                    }
                }
            }

            /*{
                    double deltaTol = engine.getDeltaTime();

                    if (deltaTol < 0.005)
                            deltaTol = 0.005;

                    double rningTimeFretCalcMn =
            module.fretboardPositionCalcByT(-9.0 - deltaTol, 1.02); double
            rningTimeFretCalcMs = module.fretboardPositionCalcByT(-9.0 +
            deltaTol, 1.02);

                    rningTimeFretCalcMn = rningTimeFretCalcMn < 0.0 ?
            -rningTimeFretCalcMn : rningTimeFretCalcMn; rningTimeFretCalcMs =
            rningTimeFretCalcMs < 0.0 ? -rningTimeFretCalcMs :
            rningTimeFretCalcMs;

                    fonts.drawTextInScreenWithBuffer(std::to_string(rningTimeFretCalcMn),
            -1.3, 0.8, 0.1);
                    fonts.drawTextInScreenWithBuffer(std::to_string(rningTimeFretCalcMs),
            -1.3, 0.65, 0.1);
                    fonts.drawTextInScreenWithBuffer(std::to_string(dmaxrntime),
            -1.3, 0.50, 0.1);
            }*/

            if (musicstartedg == 1) {
                module.getBPlayer().setSongVolume(game.songVolume);
                module.playSongAudio();

                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.instrumentPlay();
                }

                musicstartedg = 2;
            }
        }

        game.renderFrame();
        marathonTime += engine.getDeltaTime();
    }
}

void GPPGame::callbackRenderFrame() { CFonts::fonts().drawAllBuffers(); }

void GPPGame::callbackKeys(int, int, int, int) {
    // CControls queues ordered UI events; CMenu routes each event once.
}

void GPPGame::callbackJoystick(int jid, int eventId) {
    auto &game = GPPGame::GuitarPP();
    // CLuaH::Lua().runEvent(game.joystickStateCbSE);
}

void GPPGame::campaingPlayModule(const std::string &name) {
    loadThreadData l;

    auto &game = GPPGame::GuitarPP();
    auto &engine = CEngine::engine();
    auto &fonts = CFonts::fonts();
    auto realname = game.getCallBackRealName(name);
    auto &module = game.gameModules[realname];

    game.clearSongResults();
    if (!game.prepareScene()) {
        return;
    }

    game.HUDText = game.loadTexture("data/sprites", "HUD.tga").getTextId();
    game.fretboardText =
        game.loadTexture("data/sprites", "fretboard.tga").getTextId();
    game.lineText = game.loadTexture("data/sprites", "line.tga").getTextId();
    game.HOPOSText = -1 /*GPPGame::GuitarPP().loadTexture("data/sprites",
                           "HOPOS.tga").getTextId()*/
        ;
    game.pylmBarText =
        game.loadTexture("data/sprites", "pylmbar.tga").getTextId();

    module.setHyperSpeed(2.5 * game.hyperSpeed);

    game.setVSyncMode(0);

    module.players.push_back(game.mainPlayer);

    l.processing = true;
    l.continueThread = true;
    l.songID = 0;
    l.songsList = getDirectory("./data/songs", false, true);
    l.listEnd = false;
    l.loadSong = true;

    for (auto &p : l.songsList) {
        std::cout << p << std::endl;
    }

    // Keep the old exclusion against multiplayer container changes, but let
    // only this thread install live song data. The worker owns prepared data.
    std::unique_lock<std::mutex> sessionPlayersLock(playersMutex);
    for (auto &p : module.players) {
        p->resetSongThings();
        l.instruments.push_back(p->instrument);
    }

    SongRun songRun(game, module);
    bool reportedLoadFailure = false;
    std::thread load(loadMarathonThread, std::ref(l));
    callOnDctor<void(void)> exitguard([&]() {
        l.continueThread = false;
        if (load.joinable()) {
            load.join();
        }

        if (!songRun.finish(SongEndReason::Aborted) &&
            game.getSongResults().empty() && !l.loadFailed &&
            !l.songsList.empty()) {
            songRun.cancelled(l.songsList.front());
        }

        if (l.loadFailed && !reportedLoadFailure &&
            l.songID < l.songsList.size()) {
            songRun.failed(l.songsList[l.songID]);
        }

        game.scene.sessionEnded(module.players.empty()
                                    ? 0.0
                                    : module.getBPlayer().musicRunningTime);
        module.resetModule();
    });

    while (engine.windowOpened() && l.processing && !l.loadFailed) {
        game.clearScreen();

        CFonts::fonts().drawTextInScreenWithBuffer("loading marathon 0%", -0.4,
                                                   0.0, 0.1);

        game.renderFrame();
    }

    if (!engine.windowOpened() || l.loadFailed) {
        return;
    }

    if (!installMarathonSong(module, l)) {
        return;
    }

    l.songID++;
    l.processing = true;
    l.loadSong = true;

    for (auto &pp : module.players) {
        auto &p = *pp;
        p.startTime = engine.getTime() + 3.0;
        p.musicRunningTime = -3.0;
    }

    double startTime = module.getBPlayer().startTime = engine.getTime() + 3.0;
    double openMenuTime = 0.0;
    module.getBPlayer().musicRunningTime = -3.0;
    module.getBPlayer().guitar =
        CGuitars::inst().getGuitarIfExists(game.defaultGuitar);

    module.startSongTime =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);

    if (module.getBPlayer().guitar != nullptr) {
        {
            if (!module.getBPlayer().guitar->load()) {
                CLog::log() << "Guitar loading failed; using the default model";
            }
        }
    }

    bool enterInMenu = false;
    bool esc = false;
    int musicstartedg = 0;

    bool songTimeFixed = false;

    module.getBPlayer().enableBot = game.botEnabled;
    module.getBPlayer().playerCamera.center.y = 1.0;
    module.getBPlayer().playerCamera.eye.z += 2.0;

    bool firstStartFrame = true;

    auto &playerb = module.getBPlayer();
    std::string songName = ui::metadataDisplayText(playerb.songName);
    std::string songArtist = ui::metadataDisplayText(playerb.songArtist);
    std::string songCharter = ui::metadataDisplayText(playerb.songCharter);

    double fadeoutdsc = engine.getTime();

    bool interval = false;
    bool waitingIntervalLoad = false;

    game.scene.sessionStarted(l.songsList.front(), -3.0);
    songRun.start(l.songsList.front(), -3.0);
    startTime = engine.getTime() + 3.0;
    for (auto &p : module.players) {
        p->startTime = startTime;
    }

    module.startSongTime =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    module.startUpdateDelta();

    while (engine.windowOpened()) {
        game.clearScreen();

        if (engine.getKey(GLFW_KEY_ESCAPE) != 0) {
            esc = true;
        } else if (esc) {
            enterInMenu = true;
        } else {
            enterInMenu = false;
        }

        if (enterInMenu) {
            const auto pauseStart = std::chrono::steady_clock::now();
            if (musicstartedg == 2) {
                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.instrumentPause();
                }

                module.getBPlayer().pauseSongAudio();
                musicstartedg = 0;
            }

            songTimeFixed = false;

            openMenuTime = engine.getTime();

            if (game.menus.openPause() == PauseResult::Exit) {
                break;
            }

            double time = engine.getTime();

            for (auto &pp : module.players) {
                auto &p = *pp;
                p.startTime += time - openMenuTime;
            }

            module.startSongTime +=
                std::chrono::steady_clock::now() - pauseStart;
            startTime += time - openMenuTime;
            module.startUpdateDelta();
            enterInMenu = false;
            esc = false;
        } else {
            if (!interval) {
                {
                    module.marathonUpdate();
                }
            } else {
                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.musicRunningTime +=
                        engine.getDeltaTime() * module.getgSpeed();
                }
            }

            bool songChartEnd = true;

            for (auto &pp : module.players) {
                auto &p = *pp;
                if (!interval) {
                    if (songChartEnd) {
                        {
                            songChartEnd = p.isSongChartFinished();
                        }
                    }
                }

                if (p.targetCamera.center.y < p.playerCamera.center.y) {
                    p.playerCamera.center.y -= engine.getDeltaTime() * 0.5;
                }

                if (p.targetCamera.center.y > p.playerCamera.center.y) {
                    p.playerCamera.center.y = p.targetCamera.center.y;
                }

                if (p.targetCamera.eye.z < p.playerCamera.eye.z) {
                    p.playerCamera.eye.z -= engine.getDeltaTime() * 0.75;
                }

                if (p.targetCamera.eye.z > p.playerCamera.eye.z) {
                    p.playerCamera.eye.z = p.targetCamera.eye.z;
                }
            }

            interval = songChartEnd;

            if (interval) {
                if (!waitingIntervalLoad) {
                    songRun.finish(SongEndReason::Completed);
                    if (l.songID >= l.songsList.size()) {
                        return;
                    }

                    waitingIntervalLoad = true;
                } else {
                    if (l.processing) {
                        //// TODO

                    } else {
                        if (l.loadFailed) {
                            songRun.failed(l.songsList[l.songID]);
                            reportedLoadFailure = true;
                            return;
                        }

                        double waitTime = 5.0;

                        double deltaTol = engine.getDeltaTime();

                        if (deltaTol < 0.005) {
                            {
                                deltaTol = 0.005;
                            }
                        }

                        deltaTol /= 2.0;

                        double dmaxrntime = 0.0;

                        double rningTimeFretCalcMn =
                            module.fretboardPositionCalcByT(
                                module.getBPlayer().musicRunningTime -
                                    deltaTol - 4.0,
                                1.02, &dmaxrntime);
                        double rningTimeFretCalcMs =
                            module.fretboardPositionCalcByT(
                                module.getBPlayer().musicRunningTime +
                                    deltaTol - 4.0,
                                1.02);

                        rningTimeFretCalcMn = rningTimeFretCalcMn < 0.0
                                                  ? -rningTimeFretCalcMn
                                                  : rningTimeFretCalcMn;
                        rningTimeFretCalcMs = rningTimeFretCalcMs < 0.0
                                                  ? -rningTimeFretCalcMs
                                                  : rningTimeFretCalcMs;

                        double fretBoardPos =
                            module.fretboardPositionCalcByT(-5 - 4.0, 1.02);

                        if (fretBoardPos > 0.0) {
                            fretBoardPos = dmaxrntime - fretBoardPos;
                        } else {
                            fretBoardPos *= -1.0;
                        }

                        if (fretBoardPos > rningTimeFretCalcMn &&
                            fretBoardPos < rningTimeFretCalcMs) {
                            if (!installMarathonSong(module, l)) {
                                return;
                            }

                            songRun.start(l.songsList[l.songID], -waitTime);
                            for (auto &pp : module.players) {
                                auto &p = *pp;
                                p.startTime = engine.getTime() + waitTime;
                                p.musicRunningTime = -waitTime;
                                module.startSongTime =
                                    std::chrono::steady_clock::now() +
                                    std::chrono::milliseconds(
                                        static_cast<int64_t>(1000 * waitTime));
                            }

                            startTime = module.getBPlayer().startTime =
                                engine.getTime() + waitTime;
                            openMenuTime = 0.0;
                            module.getBPlayer().musicRunningTime = -waitTime;

                            songTimeFixed = false;
                            musicstartedg = 0;

                            l.songID++;
                            interval = false;
                            l.processing = true;
                            l.loadSong = true;
                            waitingIntervalLoad = false;

                            auto &playerb = module.getBPlayer();
                            songName =
                                ui::metadataDisplayText(playerb.songName);
                            songArtist =
                                ui::metadataDisplayText(playerb.songArtist);
                            songCharter =
                                ui::metadataDisplayText(playerb.songCharter);
                        }
                    }
                }
            } else {
                if (!songTimeFixed && (engine.getTime() - startTime) > 0.5) {
                    module.getBPlayer().seekSongAudio(
                        module.getBPlayer().musicRunningTime);

                    for (auto &pp : module.players) {
                        auto &p = *pp;
                        engine.setSoundTime(p.instrumentSound,
                                            p.musicRunningTime);
                    }

                    songTimeFixed = true;
                }
            }

            {

                engine.activate3DRender(true);
                engine.activateLighting(true);

                {
                    double centerx = 0.0;
                    double centerz = -650.0;

                    double rtime = module.getBPlayer().musicRunningTime / 10.0;
                    double eyexcam = sin(0) * 1.0 + centerx + sin(rtime) * 1.0;
                    double eyezcam = cos(0) * 1.0 + centerz + cos(rtime) * 1.0;

                    CEngine::cameraSET usingCamera;

                    usingCamera.eye.x = 3.0 + sin(rtime);
                    usingCamera.eye.y = 2.5;
                    usingCamera.eye.z = 1.0 + cos(rtime);
                    usingCamera.center.x = 3.0;
                    usingCamera.center.y = 0.5;
                    usingCamera.center.z = -5;
                    usingCamera.up.x = 0;
                    usingCamera.up.y = 1;
                    usingCamera.up.z = 0;

                    engine.setCamera(usingCamera);
                }

                {
                    lightData l{};

                    for (auto &t : l.ambientLight) {
                        t = 0.1f;
                    }

                    for (auto &t : l.direction) {
                        t = 2.5f;
                    }

                    for (auto &t : l.position) {
                        t = 0.0f;
                    }

                    for (auto &t : l.specularLight) {
                        t = 0.2f;
                    }

                    for (auto &t : l.diffuseLight) {
                        t = 0.2f;
                    }

                    l.specularLight[1] = 1.0f;
                    l.specularLight[2] = 1.0f;
                    l.diffuseLight[0] = 1.0f;
                    l.diffuseLight[1] = 1.0f;
                    l.ambientLight[3] = 0.1f;

                    CEngine::colorRGBToArrayf(0xFFF6ED, l.diffuseLight);

                    l.angle = 180.0f;
                    l.direction[0] = 3.0f;
                    l.direction[1] = -0.5f;
                    l.direction[2] = -1.5f;

                    l.position[0] = 3.0f;
                    l.position[1] = 2.7f;
                    l.position[2] = -1.5f;
                    l.position[3] = 1.0f;

                    engine.activateLight(0, false);
                    engine.activateLight(1, true);
                    engine.setLight(l, 1);
                }

                engine.activateNormals(true);
                CBackgroundRenderer::inst().draw(game.testobj);
                engine.activateNormals(false);

                engine.activateLighting(false);
                engine.activate3DRender(false);

                engine.matrixReset();

                engine.clear3DBuffer();

                {
                    CEngine::cameraSET usingCamera;
                    usingCamera.eye.x = 0.0;
                    usingCamera.eye.y = 0.0;
                    usingCamera.eye.z = 2.3;
                    usingCamera.center.x = 0;
                    usingCamera.center.y = 0;
                    usingCamera.center.z = 0.0;
                    usingCamera.up.x = 0;
                    usingCamera.up.y = 1;
                    usingCamera.up.z = 0;

                    engine.setCamera(usingCamera);
                }
            }

            module.render();

            if (!interval) {
                {
                    module.renderLyrics();
                }
            }

            if (firstStartFrame) {
                engine.playSoundStream(GuitarPP().startSound);
                firstStartFrame = false;
            }

            double time = engine.getTime();

            if (musicstartedg == 0) {
                {
                    musicstartedg = 1;
                }
            }

            if ((startTime - time) > 0.0) {

                fadeoutdsc = engine.getTime();

                std::string count = std::to_string((int)(startTime - time));

                fonts.drawTextInScreenWithBuffer(
                    count, fonts.getCenterPos(count, 0.3, 0.0), 0.0, 0.3);
                musicstartedg = 0;
            }

            double fadeoutdscAlphaCalc =
                1.0 - ((engine.getTime() - fadeoutdsc) / 3.0);

            if (fadeoutdscAlphaCalc >= 0.0) {
                if (fadeoutdscAlphaCalc < 1.0) {
                    {
                        engine.setColor(1.0, 1.0, 1.0, fadeoutdscAlphaCalc);
                    }
                }

                fonts.drawTextInScreen(songName, -0.9, 0.7, 0.1);
                fonts.drawTextInScreen(songArtist, -0.88, 0.62, 0.08);
                fonts.drawTextInScreen(songCharter, -0.88, 0.54, 0.08);

                if (fadeoutdscAlphaCalc < 1.0) {
                    {
                        engine.setColor(1.0, 1.0, 1.0, 1.0);
                    }
                }
            }

            if (musicstartedg == 1) {
                module.getBPlayer().setSongVolume(game.songVolume);
                module.playSongAudio();

                for (auto &pp : module.players) {
                    auto &p = *pp;
                    p.instrumentPlay();
                }

                musicstartedg = 2;
            }
        }

        game.renderFrame();
    }
}

void GPPGame::settWindowConfigs(const gameWindow &w) { windowCFGs = w; }

/*
Clear screen buffer
*/
void GPPGame::clearScreen() {
    CEngine::engine().clearScreen();
    CLuaH::Lua().runEvent(posClearScreenSE);
}

/*
Swap video buffer to screen
*/
void GPPGame::renderFrame() {
    CLuaH::Lua().runEvent(preRenderFrameSE);
    CEngine::engine().renderFrame();
    CLuaH::Lua().runEvent(posRenderFrameSE);

    streammingProcess();
}

auto GPPGame::GuitarPP() -> GPPGame & {
    static GPPGame game;
    return game;
}

auto GPPGame::getWindowProportion() -> double {
    return CEngine::engine().windowWidth / CEngine::engine().windowHeight;
}

void GPPGame::textureStreammingProcess() {
    textureManager->pump();

    modelLoader().pump();
}

void GPPGame::streammingProcess() { textureStreammingProcess(); }

void GPPGame::loadBasicSprites() {
    CLuaH::Lua().runEvent(preLoadSpritesSE);
    SPR["palheta"] = CEngine::engine().loadTexture("data/sprites/palheta.tga");
}

void GPPGame::loadBasicSounds() {
    auto &engine = CEngine::engine();
    engine.loadSoundStream("data/sounds/erro-verde.wav", errorsSound[0]);
    engine.loadSoundStream("data/sounds/erro-vermelho.wav", errorsSound[1]);
    engine.loadSoundStream("data/sounds/erro-amarelo.wav", errorsSound[2]);
    engine.loadSoundStream("data/sounds/erro-azul.wav", errorsSound[3]);
    engine.loadSoundStream("data/sounds/erro-laranja.wav", errorsSound[4]);

    engine.loadSoundStream("data/sounds/fretboard-inicio.wav", startSound);
    engine.loadSoundStream("data/sounds/fretboard-fimdamusica.wav", endSound);
}

auto GPPGame::CTheme::load() -> bool {
    const std::string path = std::string("data/themes/") + themeName;

    if (themeName.empty()) {
        CLog::log()
            << "GPPGame::CTheme::load(): Load theme fail - no name setted";
        return false;
    }

    // main = CLuaH::Lua().newScriptInQuere(CLuaH::Lua().newScript(path,
    // "Theme.lua"));
    CLuaH::Lua().loadFiles(path, {"Gui.lua", "MenuOverrides.lua"});

    if (CLuaH::Lua().getScript(path, "Theme.lua").luaState == nullptr) {
        CLog::log() << "GPPGame::CTheme::load(): Load theme fail - Theme.lua "
                       "wasn't loaded";
        return false;
    }

    CLuaH::Lua().runScript(path, "Theme.lua");

    loaded = true;

    return true;
}

auto GPPGame::loadThemes(const std::string &theme, CLuaH::luaScript *luaScript)
    -> const GPPGame::CTheme & {
    auto &themeInst = gThemes[theme];
    themeInst.themeName = theme;

    if (!themeInst.isloaded()) {
        themeInst.load();
    }

    return gThemes[theme];
}

void GPPGame::loadAllThemes() {
    const std::string path = "data/themes";
    for (const auto &name : getDirectory(path.c_str(), false, true)) {
        if (std::ifstream(path + "/" + name + "/Theme.lua").is_open()) {
            loadThemes(name);
        }
    }
}

auto GPPGame::addGameCallbacks(const std::string &n, func_t function)
    -> const std::string {
    std::string str =
        std::string("game_callback_") + std::to_string(rand()) + "_" + n;

    gameCallbacks[str] = std::move(function);
    gameCallbacksWrapper[str] = n;

    return str;
}

auto GPPGame::getCallBackRealName(const std::string &str) -> std::string {
    for (auto &s : gameCallbacksWrapper) {
        if (s.second == str) {
            return s.first;
        }
    }

    return "";
}

auto GPPGame::getCallback(const std::string &str) -> GPPGame::func_t {
    return gameCallbacks[str];
}

void GPPGame::teste(const std::string &name) { std::cout << name << std::endl; }

int GPPGame::CTheme::applyThemeSE = 0;

void GPPGame::CTheme::apply() {
    const std::string path = std::string("data/themes/") + themeName;

    CLuaH::Lua().runInternalEvent(CLuaH::Lua().getScript(path, "Theme.lua"),
                                  applyThemeSE);
    GuitarPP().menus.loadThemePresentation(path);
}

GPPGame::CTheme::CTheme(const std::string &theme) {
    themeName = theme;

    loaded = true;
}

GPPGame::CTheme::CTheme() { loaded = false; }

/*
Lua events and creates window
*/
auto GPPGame::createWindow() -> int {
    initializeTextures();
    initializeModels();
    mainthread = std::this_thread::get_id();
    CLuaH::Lua().runEvent(preCreateWindowSE);
    std::string title = "Guitar++";

    // Custom title
    if (!getWindowConfig().name.empty()) {
        title += " - ";
        title += getWindowConfig().name;
    }

    // GLFW SAMPLES - Anti aliasing
    CEngine::engine().AASamples = getWindowConfig().AA;

    CEngine::engine().openWindow(title.c_str(), getWindowConfig().w,
                                 getWindowConfig().h,
                                 getWindowConfig().fullscreen);
    CEngine::engine().activateAlphaTest(true);

    auto &controls = CControls::controls();
    controls.init();

    controls.keyCallback = callbackKeys;
    controls.joystickCallback = callbackJoystick;

    if (getWindowConfig().VSyncMode >= 0 && getWindowConfig().VSyncMode <= 2) {
        setVSyncMode(getWindowConfig().VSyncMode);
    }

    try {
        /*CShader::inst();
        CShader::inst().addEvent("test");
        int l0 = CShader::inst().newShader("vert.vert", 0, "test");
        int l = CShader::inst().newShader("frag.frag", 1, "test");
        CShader::inst().addShaderToEvent("test", l0);
        CShader::inst().addShaderToEvent("test", l);
        CShader::inst().linkAllShaders();*/
    } catch (std::exception &e) {
        CLog::log() << e.what();
    }

    CLuaH::Lua().runEvent(posCreateWindowSE);

    return static_cast<int>(CEngine::engine().windowOpened());
}

void GPPGame::setVSyncMode(int mode) { CEngine::engine().setVSyncMode(mode); }

void GPPGame::logError(int code, const std::string &e) {
    CLog::log() << std::to_string(code) + ": " + e;
}

std::string GPPGame::ip = "127.0.0.1";
std::string GPPGame::port = "7777";

auto GPPGame::registerFunctions(CLuaH::luaState_t &Lstate) -> int {
    lua_State *L = Lstate.get();

    lua_register(L, "setScene", setScene);
    lua_register(L, "getScene", getScene);
    lua_register(L, "loadSingleTexture", loadSingleTexture);
    lua_register(L, "getGameplayRunningTime", getGameplayRunningTime);
    lua_register(L, "getDeltaTime", getDeltaTime);
    lua_register(L, "getGamePlayPlusState", getGamePlayPlusState);

    return 0;
}

auto GPPGame::registerGlobals(CLuaH::luaState_t &L) -> int {
    setLuaGlobal(L, "JOYSTICK_CONNECTED",
                 CControls::JOYSTICK_STATE::JOYSTICK_CONNECTED);
    setLuaGlobal(L, "JOYSTICK_DISCONNECTED",
                 CControls::JOYSTICK_STATE::JOYSTICK_DISCONNECTED);

    return 0;
}

auto GPPGame::loadSingleTexture(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    if (p.getNumParams() == 2) {
        std::string path;
        std::string name;
        p >> path;
        p >> name;

        p << GPPGame::GuitarPP()
                 .loadTexture(path, name, &CLuaH::Lua().getLuaStateScript(L))
                 .getTextId();
    } else {
        p << 0;
    }

    return p.rtn();
}

auto GPPGame::getGameplayRunningTime(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    p << GPPGame::GuitarPP().gameplayRunningTime;

    return p.rtn();
}

auto GPPGame::getDeltaTime(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    p << CEngine::engine().getDeltaTime();

    return p.rtn();
}

auto GPPGame::getGamePlayPlusState(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    p << GPPGame::GuitarPP().gamePlayPlusEnabled;

    return p.rtn();
}

int GPPGame::firstStartFrameSE = 0, GPPGame::preCreateWindowSE = 0,
    GPPGame::posCreateWindowSE = 0, GPPGame::preLoadSpritesSE = 0,
    GPPGame::posClearScreenSE = 0, GPPGame::preRenderFrameSE = 0,
    GPPGame::posRenderFrameSE = 0, GPPGame::catchedExceptionSE = 0,
    GPPGame::joystickStateCbSE = 0;

GPPGame::GPPGame() : menus(*this), glanguage("PT-BR") {
    initializeTextures();
    initializeModels();
    CBackgroundRenderer::inst();
    CLuaFunctions::GameVariables::gv().pushVar("backgroundScene",
                                               backgroundScene);
    songVolume = 0.8f;
    drawGamePlayBackground = true;
    showTextsTest = true;

    for (auto &es : errorsSound) {
        es = 0;
    }

    startSound = endSound = 0;

    {
        auto &LuaF = CLuaFunctions::LuaF();
        LuaF.registerLuaFuncsAPI(registerFunctions);
        LuaF.registerLuaFuncsAPI(registerGlobals);

        auto &Lua = CLuaH::Lua();
        CTheme::applyThemeSE = Lua.idForCallbackEvent("applyTheme");
        firstStartFrameSE = Lua.idForCallbackEvent("firstStartFrame");
        preCreateWindowSE = Lua.idForCallbackEvent("preCreateWindow");
        posCreateWindowSE = Lua.idForCallbackEvent("posCreateWindow");
        preLoadSpritesSE = Lua.idForCallbackEvent("preLoadSprites");
        posClearScreenSE = Lua.idForCallbackEvent("posClearScreen");
        preRenderFrameSE = Lua.idForCallbackEvent("preRenderFrame");
        posRenderFrameSE = Lua.idForCallbackEvent("posRenderFrame");
        posClearScreenSE = Lua.idForCallbackEvent("posClearScreen");
        posClearScreenSE = Lua.idForCallbackEvent("posClearScreen");
        catchedExceptionSE = Lua.idForCallbackEvent("catchedException");
        joystickStateCbSE = Lua.idForCallbackEvent("joystickStateCallback");
    }

    gameplayRunningTime = 0.0;

    gamePlayPlusEnabled = false;

    hyperSpeed = 1.0;

    botEnabled = false;
    usarPalheta = true;

    {
        auto &gv = CLuaFunctions::GameVariables::gv();

        gv.pushVar("botEnabled", botEnabled);
        gv.pushVar("usarPalheta", usarPalheta);
        gv.pushVar("multiplayerClientIP", ip);
        gv.pushVar("multiplayerPort", port);
        gv.pushVar("defaultGuitar", defaultGuitar);
    }

    CEngine::engine().renderFrameCallback = callbackRenderFrame;

    windowCFGs = getWindowDefaults();
    if (settings.load(windowCFGs, glanguage) ==
        CGameSettings::LoadResult::Error) {
        CLog::log() << "Settings load failed; using defaults. " +
                           settings.error();
    }

    srand((unsigned int)time(0));

    devMode = false;

    HUDText = 0;
    fretboardText = 0;

    CEngine::engine().errorCallbackFun = logError;

    menus.initialize();

    // Load lua scripts from "data" folder
    CLuaH::Lua().loadFiles("data");
}

bool GPPGame::saveSettings() {
    if (!settings.save(windowCFGs, glanguage)) {
        CLog::log() << "Settings save failed. " + settings.error();
        return false;
    }

    return true;
}

GPPGame::~GPPGame() noexcept = default;

void GPPGame::resetMainPlayer() { mainPlayer = std::make_shared<CPlayer>(); }
