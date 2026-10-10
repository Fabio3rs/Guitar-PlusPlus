#include "CGuitars.h"
#include "CLog.h"
#include "CLuaFunctions.hpp"
#include "CGPPFileMGR.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

int CGuitars::addGuitarScriptEvent = 0, CGuitars::loadGuitarScriptEvent = 0,
    CGuitars::unloadingGuitarScriptEvent = 0;

// Call after players stop using the catalogue, while the GL context is current.
void CGuitars::unload() {
    for (auto &entry : guitars) { entry.second.unload(); }
    guitars.clear();
}

auto CGuitars::addGuitar(const std::string &path,
                         const std::string &internalName) -> CGuitar * {
    if (auto *existing = getGuitarIfExists(internalName)) { return existing; }
    if (staging || internalName.empty()) { return nullptr; }
    // A node transfer preserves the luaScript address stored in its Lua state.
    std::map<std::string, CGuitar> pending;
    auto &candidate = pending[internalName];
    candidate.path = path;
    candidate.internalName = internalName;
    staging = &candidate;
    struct ResetStaging {
        CGuitar *&pointer;
        ~ResetStaging() { pointer = nullptr; }
    } reset{staging};
    candidate.luaF = CLuaH::Lua().newScript(path + "/" + internalName, "GuitarScript.lua");
    const CLuaH::multiCallBackParams_t params = {path, internalName};
    if (!candidate.luaF.luaState || CLuaH::Lua().runScript(candidate.luaF) != 0 ||
        !CLuaH::Lua().runInternalEventWithParams(candidate.luaF, addGuitarScriptEvent, params) ||
        candidate.gameplayBarName.empty()) {
        CLog::log() << std::string("Invalid guitar configuration: ") + internalName;
        return nullptr;
    }
    guitars.insert(pending.extract(internalName));
    return &candidate;
}

auto CGuitars::getGuitar(const std::string &internalName)
    -> CGuitars::CGuitar & {
    return guitars.at(internalName);
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

bool CGuitars::CGuitar::load() {
    if (loaded) { return true; }
    if (!glfwGetCurrentContext()) { return false; }
    GPPOBJ candidate;
    if (!candidate.load(path + "/" + internalName, gameplayBarName)) {
        CLog::log() << std::string("Cannot load guitar model: ") + internalName;
        return false;
    }
    const auto previousTexture = textureID;
    const auto previousFretboard = fretboardText;
    const CLuaH::multiCallBackParams_t params = {path, internalName};
    if (!CLuaH::Lua().runInternalEventWithParams(luaF, loadGuitarScriptEvent, params)) {
        textureID = previousTexture;
        fretboardText = previousFretboard;
        return false;
    }
    // Legacy scripts supply cached texture handles; the guitar does not own them.
    if (textureID != 0 && !glIsTexture(static_cast<GLuint>(textureID))) { textureID = 0; }
    if (fretboardText != 0 && !glIsTexture(fretboardText)) { fretboardText = 0; }
    try {
        candidate.prepareForDraw();
    } catch (const std::exception &error) {
        textureID = previousTexture;
        fretboardText = previousFretboard;
        CLog::log() << std::string("Cannot prepare guitar: ") + error.what();
        return false;
    }
    gameplayBar = std::move(candidate);
    loaded = true;
    return true;
}

void CGuitars::CGuitar::unload() {
    if (loaded) {
        const CLuaH::multiCallBackParams_t params = {path, internalName};
        CLuaH::Lua().runInternalEventWithParams(luaF, unloadingGuitarScriptEvent, params);
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
    const std::string path = "./data/guitars";
    for (const auto &name : CGPPFileMGR::getDirectory(path, false, true)) {
        if (std::ifstream(path + "/" + name + "/GuitarScript.lua").is_open()) {
            addGuitar(path, name);
        }
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

        auto &manager = inst();
        auto *target = manager.staging
            ? (manager.staging->internalName == targetGuitar ? manager.staging : nullptr)
            : manager.getGuitarIfExists(targetGuitar);

        if (target != nullptr) {
            std::string dataName;
            p >> dataName;

            auto it2 = mp.find(dataName);

            if (it2 != mp.end()) {
                switch ((*it2).second) {
                case 0:
                    p >> target->name;
                    break;

                case 1:
                    p >> target->gameplayBarName;
                    break;

                case 2:
                    p >> target->textureID;
                    break;

                case 3:
                    p >> target->fretboardText;
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
