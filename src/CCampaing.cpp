#include "CCampaing.h"
#include "CFonts.h"
#include "CGPPFileMGR.h"
#include "CLog.h"
#include "GPPGame.h"
#include <cereal/archives/binary.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/deque.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <random>

CCampaing::CCampaingData::CCampaingData() {
    money = 0.0;
    playedTime = 0.0;
    reputationPoints = 0;
    mode = "default";
}

auto CCampaing::loadCampaingF(const std::string &filepath) -> bool {
    try {
        std::fstream svfstream(filepath, std::ios::in | std::ios::binary);

        if (!svfstream.is_open()) {
            return false;
        }

        cereal::BinaryInputArchive iarchive(svfstream);

        iarchive(campaingNow);

        campaingLoaded = true;
        loadedCampaingFilepath = filepath;

        std::string scriptsPath =
            campaingScriptsDirectory + "/" + campaingNow.mode;

        campaingScripts.clear();

        for (size_t i = 0, size = campaingNow.scripts.size(); i < size; i++) {
            auto lscript = CLuaH::Lua().newScriptRBuffer(
                campaingNow.scripts[i].byteCode, campaingNow.scripts[i].name);

            if (lscript.luaState == nullptr) {
                CLog::log() << std::string("Lua script load failed") +
                                   std::to_string(
                                       campaingNow.scripts[i].byteCode.size());
                continue;
            }

            campaingScripts.push_back(std::move(lscript));
            auto &tdata = campaingNow.scripts[i].scriptVars.getTableData();

            for (auto &t : tdata) {
                t.second.pushToLuaStack(campaingScripts[i].luaState.get());
                lua_setglobal(campaingScripts[i].luaState.get(),
                              t.first.c_str());
            }

            CLuaFunctions::LuaF().registerFunctions(
                campaingScripts[i].luaState);
            CLuaFunctions::LuaF().registerGlobals(campaingScripts[i].luaState);
        }

        CLuaH::Lua().runScriptsFromStorage(campaingScripts);
        CLuaH::Lua().runEventFromContainer(campaingLoadSE, campaingScripts);
    } catch (const std::exception &e) {
        CLog::log() << e.what();
        campaingLoaded = false;
        return false;
    } catch (...) {
        CLog::log() << "Fail to load campaing data " + filepath;
        campaingLoaded = false;
        return false;
    }

    return true;
}

auto CCampaing::saveCampaingF() -> bool {
    try {
        std::fstream svfstream(loadedCampaingFilepath, std::ios::out |
                                                           std::ios::trunc |
                                                           std::ios::binary);

        if (!svfstream.is_open()) {
            return false;
        }

        for (size_t i = 0, size = campaingNow.scripts.size(); i < size; i++) {
            campaingNow.scripts[i].scriptVars.clear();
            CLuaH::loadGlobalTable(campaingScripts[i].luaState.get(),
                                   campaingNow.scripts[i].scriptVars);
        }

        cereal::BinaryOutputArchive oarchive(
            svfstream); // Create an output archive

        oarchive(campaingNow);
    } catch (const std::exception &e) {
        CLog::log() << e.what();
        return false;
    } catch (...) {
        CLog::log() << "Fail to save campaing data " + loadedCampaingFilepath;
        return false;
    }
    return true;
}

auto CCampaing::getCampaingList(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    p << true;

    return p.rtn();
}

auto CCampaing::registerLuaFunctions(CLuaH::luaState_t &Lstate) -> int {
    lua_State *L = Lstate.get();

    lua_register(L, "getCampaingList", getCampaingList);
    lua_register(L, "getBandName", getBandName);
    lua_register(L, "getCampaingMode", getCampaingMode);
    lua_register(L, "getBandMoney", getBandMoney);
    lua_register(L, "getBandReputationPoints", getBandReputationPoints);
    lua_register(L, "isCampaingLoaded", isCampaingLoaded);
    lua_register(L, "getLoadedCampaingPath", getLoadedCampaingPath);
    lua_register(L, "keepCampaingMenuStack", keepCampaingMenuStack);
    lua_register(L, "exitCampaingScreen", exitCampaingScreen);

    return 0;
}

auto CCampaing::campaingMGR() -> CCampaing & {
    static CCampaing campaing;
    return campaing;
}

auto CCampaing::newCampaing() -> int {
    campaingScripts.clear();

    campaingNow = CCampaingData();

    std::string scriptsPath = campaingScriptsDirectory + "/" + campaingNow.mode;
    CLuaH::Lua().loadFilesDequeStorage(scriptsPath, campaingScripts);

    campaingNow.scripts.clear();

    for (auto &campaingScript : campaingScripts) {
        luaScriptSave nluascriptsave;

        nluascriptsave.byteCode = campaingScript.dumpBytecode();
        nluascriptsave.name = campaingScript.fileName;
        nluascriptsave.scriptVars.clear();

        if (nluascriptsave.byteCode.empty()) {
            {
                CLog::log()
                    << std::string("Script bytecode ") +
                           std::to_string(nluascriptsave.byteCode.size()) +
                           " " + campaingScript.fileName;
            }
        }

        campaingNow.scripts.push_back(nluascriptsave);
    }

    CLuaH::Lua().runScriptsFromStorage(campaingScripts);
    CLuaH::Lua().runEventFromContainer(campaingInitSE, campaingScripts);

    saveCampaingF();

    return 0;
}

auto CCampaing::continueCampaing(const std::string &path) -> int {
    // campaingScripts.clear();

    return 0;
}

void CCampaing::loadCampaingModes() {
    auto &lua = CLuaH::Lua();
    const std::string campaingModesPath = "data/campaings";

    for (const auto &name :
         CGPPFileMGR::getDirectory(campaingModesPath, false, true)) {
        const std::string path = campaingModesPath + "/" + name;
        const std::string infoPath = path + "/info.lua";
        CLog::log() << ("Loading <<" + infoPath + ">>");
        if (!std::ifstream(infoPath).is_open()) {
            CLog::log() << ("<<" + infoPath + ">> does not exist");
            continue;
        }

        auto infoScript = lua.newScript(path, "info.lua");
        if (infoScript.luaState) {
            lua.runScript(infoScript);
            auto &mode = campaingModes[name];
            mode.name = CLuaH::getGlobalVarAsString(infoScript, "name");
            mode.description =
                CLuaH::getGlobalVarAsString(infoScript, "description");
            mode.author = CLuaH::getGlobalVarAsString(infoScript, "author");
        }
    }
}

auto CCampaing::listCampaingSaves() -> std::deque<std::string> {
    return GPPGame::getDirectory("./data/saves/campaings", false, true);
}

auto CCampaing::getBandName(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    p << campaingMGR().campaingNow.bandName;

    return p.rtn();
}

auto CCampaing::getCampaingMode(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    p << campaingMGR().campaingNow.mode;

    return p.rtn();
}

auto CCampaing::getBandMoney(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    p << campaingMGR().campaingNow.money;

    return p.rtn();
}

auto CCampaing::getBandReputationPoints(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    p << campaingMGR().campaingNow.reputationPoints;

    return p.rtn();
}

auto CCampaing::isCampaingLoaded(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    p << campaingMGR().campaingLoaded;

    return p.rtn();
}

auto CCampaing::getLoadedCampaingPath(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    p << campaingMGR().loadedCampaingFilepath;

    return p.rtn();
}

auto CCampaing::keepCampaingMenuStack(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    p >> campaingMGR().keepMenuStack;

    return p.rtn();
}

auto CCampaing::exitCampaingScreen(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    campaingMGR().continueInDrawScreen = false;

    return p.rtn();
}

auto CCampaing::campaingLoop() -> int {
    /*auto &mgr = campaingMGR();
    auto &engine = CEngine::engine();
    auto &GuitarPP = GPPGame::GuitarPP();
    */
    // bool windowOpened = true;

    return 0;
}

void CCampaing::campaingPlayLoop(const std::string &n) {
    auto &game = GPPGame::GuitarPP();
    {
        game.marathonSongsList = campaingMGR().playSongsList;
    }

    GPPGame::startMarathonModule(n);
}

int CCampaing::campaingLoadSE = 0, CCampaing::campaingInitSE = 0,
    CCampaing::campaingMenuPreFunSE = 0, CCampaing::campaingMenuMidFunSE = 0,
    CCampaing::campaingMenuPosFunSE = 0, CCampaing::campaingDScreenUpdateSE = 0,
    CCampaing::campaingMainMenuOpenSE = 0;

CCampaing::CCampaing() : campaingScriptsDirectory("./data/campaings") {
    auto &Lua = CLuaH::Lua();
    campaingLoadSE = Lua.idForCallbackEvent("campaingLoad");
    campaingInitSE = Lua.idForCallbackEvent("campaingInit");
    campaingMenuPreFunSE = Lua.idForCallbackEvent("campaingMenuPreFun");
    campaingMenuMidFunSE = Lua.idForCallbackEvent("campaingMenuMidFun");
    campaingMenuPosFunSE = Lua.idForCallbackEvent("campaingMenuPosFun");
    campaingInitSE = Lua.idForCallbackEvent("campaingInit");
    campaingInitSE = Lua.idForCallbackEvent("campaingInit");
    campaingDScreenUpdateSE =
        Lua.idForCallbackEvent("campaingDrawScreenUpdate");
    campaingMainMenuOpenSE = Lua.idForCallbackEvent("campaingMainMenuOpen");

    buildMenus();

    numCampaingSaves = 0;
    campaingFunctionGotoID = 0;
    keepMenuStack = false;
    campaingLoaded = false;
    continueInDrawScreen = false;
    loadedCampaingFilepath = "./data/saves/campaings/campaingZoeira/save";
    CLuaFunctions::LuaF().registerLuaFuncsAPI(registerLuaFunctions);
}
