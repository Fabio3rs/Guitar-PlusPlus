#include "CGuitars.h"
#include "CLog.h"
#include "CLuaFunctions.hpp"
#include <dirent.h>

int CGuitars::addGuitarScriptEvent = 0, CGuitars::loadGuitarScriptEvent = 0,
    CGuitars::unloadingGuitarScriptEvent = 0;

void CGuitars::unload() { guitars.clear(); }

auto CGuitars::addGuitar(const std::string &path,
                         const std::string &internalName)
    -> CGuitars::CGuitar & {
    std::string fullPath;
    CGuitar &newGuitar = guitars[internalName];

    newGuitar.path = path;
    newGuitar.internalName = internalName;
    fullPath = path + "/" + internalName;

    newGuitar.luaF = CLuaH::Lua().newScript(fullPath, "GuitarScript.lua");

    if (newGuitar.luaF.luaState != nullptr) {
        CLuaH::multiCallBackParams_t g = {path, internalName};

        CLuaH::Lua().runScript(newGuitar.luaF);

        CLuaH::Lua().runInternalEventWithParams(newGuitar.luaF,
                                                addGuitarScriptEvent, g);
    } else {
        CLog::log() << "CGuitars::addGuitar <<script.luaState = nullptr>>";
    }

    return newGuitar;
}

auto CGuitars::getGuitar(const std::string &internalName)
    -> CGuitars::CGuitar & {
    return guitars[internalName];
}

auto CGuitars::getGuitarIfExists(const std::string &internalName)
    -> CGuitars::CGuitar * {
    auto it = guitars.find(internalName);

    if (it != guitars.end()) {
        {
            return &((*it).second);
        }
    }

    return nullptr;
}

void CGuitars::CGuitar::load() {
    if (this == nullptr) {
        {
            return;
        }
    }

    if (!loaded) {
        loaded = true;

        CLuaH::multiCallBackParams_t g = {path, internalName};

        gameplayBar.load(path + "/" + internalName, gameplayBarName);

        CLuaH::Lua().runInternalEventWithParams(luaF, loadGuitarScriptEvent, g);
    }
}

void CGuitars::CGuitar::unload() {
    if (!loaded) {
        CLuaH::multiCallBackParams_t g = {path, internalName};
        CLuaH::Lua().runInternalEventWithParams(luaF,
                                                unloadingGuitarScriptEvent, g);
        gameplayBar.unload();
        loaded = false;
    }
}

CGuitars::CGuitar::CGuitar() noexcept {
    loaded = false;
    textureID = 0;
    fretboardText = 0;
}

void CGuitars::loadAllGuitars() {
    auto extension_from_filename = [](const std::string &fname) {
        size_t s;
        return std::string(((s = fname.find_first_of('.')) != std::string::npos)
                               ? (&fname.c_str()[++s])
                               : (""));
    };

    auto file_exists = [](const std::string &fileName) {
        return std::fstream(fileName).is_open();
    };

    DIR *direntd = opendir("./data/guitars");
    dirent *rrd = nullptr;

    if (direntd != nullptr) {
        rrd = readdir(direntd);
        while ((rrd = readdir(direntd)) != nullptr) {
            std::string name = rrd->d_name;

            if (name != ".." && name != ".") {
                if ((rrd->d_type & DT_DIR) != 0 &&
                    file_exists(std::string("./data/guitars/") + name +
                                "/GuitarScript.lua")) {
                    addGuitar("./data/guitars", rrd->d_name);
                }
            }
        }
        closedir(direntd);
    }
}

auto CGuitars::changeGuitarData(lua_State *L) -> int {
    const static std::map<std::string, int> mp = {{"name", 0},
                                                  {"gameplayBarName", 1},
                                                  {"textureID", 2},
                                                  {"fretboardText", 3}};

    CLuaFunctions::LuaParams p(L);

    if (p.getNumParams() >= 3) {
        std::string targetGuitar;

        p >> targetGuitar;

        auto it = inst().guitars.find(targetGuitar);

        if (it != inst().guitars.end()) {
            std::string dataName;
            p >> dataName;

            auto it2 = mp.find(dataName);

            if (it2 != mp.end()) {
                switch ((*it2).second) {
                case 0:
                    p >> (*it).second.name;
                    break;

                case 1:
                    p >> (*it).second.gameplayBarName;
                    break;

                case 2:
                    p >> (*it).second.textureID;
                    break;

                case 3:
                    p >> (*it).second.fretboardText;
                    break;

                default:
                    break;
                }

                p << true;
            } else {
                p << false;
                p << true;
            }
        } else {
            p << false;
            p << true;
        }
    } else {
        p << false;
        p << true;
    }

    return p.rtn();
}

auto CGuitars::registerFunctions(CLuaH::luaState_t &Lstate) -> int {
    lua_State *L = Lstate.get();

    lua_register(L, "changeGuitarData", changeGuitarData);
    return 0;
}

auto CGuitars::registerGlobals(CLuaH::luaState_t &L) -> int { return 0; }

auto CGuitars::inst() -> CGuitars & {
    static CGuitars guitars;
    return guitars;
}

CGuitars::CGuitars() {
    auto &Lua = CLuaH::Lua();
    addGuitarScriptEvent = Lua.idForCallbackEvent("addGuitar");
    loadGuitarScriptEvent = Lua.idForCallbackEvent("loadGuitar");
    unloadingGuitarScriptEvent = Lua.idForCallbackEvent("unloadingGuitar");

    CLuaFunctions::LuaF().registerLuaFuncsAPI(registerFunctions);
    CLuaFunctions::LuaF().registerLuaFuncsAPI(registerGlobals);
}
