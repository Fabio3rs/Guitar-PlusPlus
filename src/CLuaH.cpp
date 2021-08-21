// LuaHooker.cpp : Defines the exported functions for the DLL application.
//

#include "CLuaH.hpp"
#include "CLog.h"
#include "CLuaFunctions.hpp"
#include "Lua/lstate.h"
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

auto CLuaH::make_luaState() -> CLuaH::luaState_t {
    return luaState_t(luaL_newstate());
}

void CLuaH::loadGlobalTable(lua_State *L, customParam &tableData) {
    lua_pushglobaltable(L);
    lua_pushnil(L);
    tableData.loadTableWOPush(L);
}

auto CLuaH::Lua() -> CLuaH & {
    static CLuaH L;
    return L;
}

auto CLuaH::loadFiles(const std::string &path) -> bool {
    auto extension_from_filename = [](const std::string &fname) {
        size_t s;
        return std::string(((s = fname.find_first_of('.')) != std::string::npos)
                               ? (&fname.c_str()[++s])
                               : (""));
    };

    DIR *direntd = opendir((std::string("./") + path).c_str());
    dirent *rrd = nullptr;

    if (direntd != nullptr) {
        rrd = readdir(direntd);
        while ((rrd = readdir(direntd)) != nullptr) {
            if ((rrd->d_type & DT_DIR) == 0 &&
                extension_from_filename(rrd->d_name) == "lua") {
                CLog::log() << ("Loading <<" + path + "/" + rrd->d_name + ">>");
                files[path][rrd->d_name] = newScript(path, rrd->d_name);
            }
        }
        closedir(direntd);
    }

    return true;
}

auto CLuaH::loadFilesDequeStorage(const std::string &path,
                                  scriptStorage &storage) -> bool {
    auto extension_from_filename = [](const std::string &fname) {
        size_t s;
        return std::string(((s = fname.find_first_of('.')) != std::string::npos)
                               ? (&fname.c_str()[++s])
                               : (""));
    };

    DIR *direntd = opendir((std::string("./") + path).c_str());
    dirent *rrd = nullptr;

    if (direntd != nullptr) {
        rrd = readdir(direntd);
        while ((rrd = readdir(direntd)) != nullptr) {
            if ((rrd->d_type & DT_DIR) == 0 &&
                extension_from_filename(rrd->d_name) == "lua") {
                CLog::log() << ("Loading <<" + path + "/" + rrd->d_name + ">>");
                storage.push_back(newScript(path, rrd->d_name));
            }
        }
        closedir(direntd);
    }

    return true;
}

auto CLuaH::newScriptInQuere(luaScript &&lua) -> CLuaH::luaScript * {
    files[lua.filePath][lua.fileName] = std::move(lua);
    return &files[lua.filePath][lua.fileName];
}

auto CLuaH::newScriptR(const std::string &memf, const std::string &name)
    -> CLuaH::luaScript {
    static const std::string barra("/");
    auto file_exists = [](const std::string &fileName) {
        return std::fstream(fileName).is_open();
    };

    if (memf.empty()) {
        luaScript lData;

        lData.luaState = nullptr;

        CLog::log() << ("Fail to load CLuaH::newScriptR - " + name);
        return lData;
    }

    luaScript lData;
    lData.luaState = make_luaState();
    luaL_openlibs(lData.luaState.get());
    lData.filePath = "*";
    lData.fileName = "";

    if (registerCustomFunctions) {
        CLuaFunctions::LuaF().registerFunctions(lData.luaState);
        CLuaFunctions::LuaF().registerGlobals(lData.luaState);
    }

    int load_result = luaL_loadbuffer(lData.luaState.get(), memf.c_str(),
                                      memf.size(), name.c_str());

    if (load_result != 0) {
        CLog::log() << std::string("luaL_loadbuffer(") +
                           std::to_string(reinterpret_cast<uintptr_t>(
                               lData.luaState.get())) +
                           std::string(", ") +
                           std::string(lData.filePath + barra +
                                       lData.fileName) +
                           std::string(") failed to load with result ") +
                           std::to_string(load_result);

        catchErrorString(lData);
    }
    return lData;
}

auto CLuaH::newScriptRBuffer(const char *memf, size_t sz,
                             const std::string &name) -> CLuaH::luaScript {
    static const std::string barra("/");

    if (sz == 0) {
        luaScript lData;

        lData.luaState = nullptr;

        CLog::log() << ("Fail to load CLuaH::newScriptRBuffer - " + name);
        return lData;
    }

    luaScript lData;
    lData.luaState = make_luaState();
    luaL_openlibs(lData.luaState.get());
    lData.filePath = "*";
    lData.fileName = "";

    if (registerCustomFunctions) {
        CLuaFunctions::LuaF().registerFunctions(lData.luaState);
        CLuaFunctions::LuaF().registerGlobals(lData.luaState);
    }

    int load_result =
        luaL_loadbuffer(lData.luaState.get(), memf, sz, name.c_str());

    if (load_result != 0) {
        CLog::log() << std::string("luaL_loadbuffer(") +
                           std::to_string(reinterpret_cast<uintptr_t>(
                               lData.luaState.get())) +
                           std::string(", ") +
                           std::string(lData.filePath + barra +
                                       lData.fileName) +
                           std::string(") failed to load with result ") +
                           std::to_string(load_result);

        catchErrorString(lData);
    }
    return lData;
}

auto CLuaH::newScriptRBuffer(const std::vector<char> &vec,
                             const std::string &name) -> CLuaH::luaScript {
    static const std::string barra("/");

    if (vec.empty()) {
        luaScript lData;

        lData.luaState = nullptr;

        CLog::log() << ("Fail to load CLuaH::newScriptRBuffer - " + name);
        return lData;
    }

    luaScript lData;
    lData.luaState = make_luaState();
    luaL_openlibs(lData.luaState.get());
    lData.filePath = "*";
    lData.fileName = "";

    if (registerCustomFunctions) {
        CLuaFunctions::LuaF().registerFunctions(lData.luaState);
        CLuaFunctions::LuaF().registerGlobals(lData.luaState);
    }

    int load_result = luaL_loadbuffer(lData.luaState.get(), &vec[0], vec.size(),
                                      name.c_str());

    if (load_result != 0) {
        CLog::log() << std::string("luaL_loadbuffer(") +
                           std::to_string(reinterpret_cast<uintptr_t>(
                               lData.luaState.get())) +
                           std::string(", ") +
                           std::string(lData.filePath + barra +
                                       lData.fileName) +
                           std::string(") failed to load with result ") +
                           std::to_string(load_result);

        catchErrorString(lData);
    }
    return lData;
}

auto CLuaH::newScript(const std::string &path, const std::string &f)
    -> CLuaH::luaScript {
    static const std::string barra("/");
    auto file_exists = [](const std::string &fileName) {
        return std::fstream(fileName).is_open();
    };

    if (!file_exists(std::string(path + barra + f))) {
        luaScript lData;

        lData.luaState = nullptr;

        CLog::log() << std::string("Fail to load ") +
                           std::string(path + barra + f);
        return lData;
    }

    luaScript lData;
    lData.luaState = make_luaState();
    luaL_openlibs(lData.luaState.get());
    lData.filePath = path;
    lData.fileName = f;

    if (registerCustomFunctions) {
        CLuaFunctions::LuaF().registerFunctions(lData.luaState);
        CLuaFunctions::LuaF().registerGlobals(lData.luaState);
    }

    int load_result = luaL_loadfile(
        lData.luaState.get(),
        std::string(lData.filePath + barra + lData.fileName).c_str());

    if (load_result != 0) {
        CLog::log() << std::string("luaL_loadfile(") +
                           std::to_string(reinterpret_cast<uintptr_t>(
                               lData.luaState.get())) +
                           std::string(", ") +
                           std::string(lData.filePath + barra +
                                       lData.fileName) +
                           std::string(") failed to load with result ") +
                           std::to_string(load_result);

        catchErrorString(lData);
    }
    return lData;
}

void CLuaH::catchErrorString(lua_State *L) {
    const char *s = lua_tostring(L, -1);
    if (s == NULL) {
        {
            s = "Unrecognized Lua error";
        }
    }

    CLog::log() << s;
}

void CLuaH::catchErrorString(const luaScript &L) {
    static const std::string barra("/");
    lua_State *pState = L.luaState.get();
    const char *s = lua_tostring(pState, -1);
    if (s == NULL) {
        {
            s = "Unrecognized Lua error";
        }
    }

    std::string errorStr =
        std::string(L.filePath + barra + L.fileName) + std::string(" - ") + s;

    CLog::log() << errorStr;
}

auto CLuaH::runScript(luaScript &lua) -> int {
    if (lua.luaState) {
        lua.luaState->script = &lua;
        lua_State *pState = lua.luaState.get();
        auto pcallr = lua_pcall(pState, 0, LUA_MULTRET, 0);
        return pcallr;
    }

    return 0;
}

auto CLuaH::runScript(const std::string &path, const std::string &f) -> int {
    return runScript(files[path][f]);
}

/*
 * Run only one script with args (already in Lua Stack)
 */
auto CLuaH::runScriptWithArgs(luaScript &lua, int args) -> int {
    if (lua.luaState) {
        lua.luaState->script = &lua;
        lua_State *pState = lua.luaState.get();
        auto pcallr = lua_pcall(pState, args, LUA_MULTRET, 0);
        return pcallr;
    }

    return 0;
}

void CLuaH::runEvent(int id) {
    for (auto &pathScripts : Lua().files) {
        for (auto &scripts : pathScripts.second) {
            int luaindex = 0;

            if (scripts.second.callbacksAdded) {
                auto it = scripts.second.callbacks.find(id);

                if (it != scripts.second.callbacks.end()) {
                    luaindex = it->second;

                    if (luaindex != 0) {
                        lua_State *pState = scripts.second.luaState.get();

                        lua_rawgeti(pState, LUA_REGISTRYINDEX, luaindex);
                        if (runScript(scripts.second) != 0) {
                            {
                                catchErrorString(scripts.second);
                            }
                        }
                    }
                }
            }
        }
    }
}

void CLuaH::runEventFromContainer(int id, scriptStorage &storage) {
    for (auto &scripts : storage) {
        int luaindex = 0;

        if (scripts.callbacksAdded) {
            auto it = scripts.callbacks.find(id);

            if (it != scripts.callbacks.end()) {
                luaindex = it->second;

                if (luaindex != 0) {
                    lua_State *pState = scripts.luaState.get();

                    lua_rawgeti(pState, LUA_REGISTRYINDEX, luaindex);
                    if (runScript(scripts) != 0) {
                        {
                            catchErrorString(scripts);
                        }
                    }
                }
            }
        }
    }
}

/*
 * Run a especific with parameteres (calls him specifics callbacks)
 */
void CLuaH::runEventWithParams(int id, const multiCallBackParams_t &params) {
    for (auto &pathScripts : Lua().files) {
        for (auto &scripts : pathScripts.second) {
            auto it = scripts.second.callbacks.find(id);

            if (scripts.second.callbacksAdded &&
                it != scripts.second.callbacks.end()) {
                lua_State *pState = scripts.second.luaState.get();

                lua_rawgeti(pState, LUA_REGISTRYINDEX, (*it).second);

                for (const auto &p : params) {
                    p.pushToLuaStack(pState);
                }

                if (runScriptWithArgs(scripts.second, params.size()) != 0) {
                    std::string errors = "Event name ";
                    errors += /*TODO*/ std::to_string(id);
                    errors += " num params: ";
                    errors += std::to_string(params.size());
                    errors += " data: ";

                    for (const auto &p : params) {
                        switch (p.getType()) {
                        case LUA_TNIL:
                            errors += " <null> ";
                            break;

                        case LUA_TNUMBER:
                            errors += " Number<";
                            errors += std::to_string(p.getNumber());
                            errors += "> ";
                            break;

                        case LUA_TBOOLEAN:
                            errors += ((static_cast<int>(p.getBoolean()) != 0)
                                           ? " Boolean<True> "
                                           : " Boolean<False> ");
                            break;

                        case LUA_TSTRING:
                            errors += " String<";
                            errors += p.getString();
                            errors += "> ";
                            break;

                        case LUA_TTABLE:
                            errors += " <Get LUA_TTABLE: **TODO**> ";
                            break;

                        case LUA_TFUNCTION:
                            errors += " <Get LUA_TFUNCTION: **TODO**> ";
                            break;

                        case LUA_TUSERDATA:
                            errors += " <Get LUA_TUSERDATA: **TODO**> ";
                            break;

                        case LUA_TTHREAD:
                            errors += " <Get LUA_TTHREAD: **TODO**> ";
                            break;

                        case LUA_TLIGHTUSERDATA:
                            errors += " <Get LUA_TLIGHTUSERDATA: **TODO**> ";
                            break;

                        default:
                            errors += " <unknow argment type> ";
                            break;
                        }
                    }

                    CLog::log() << errors;
                    catchErrorString(scripts.second);
                }
            }
        }
    }
}

void CLuaH::runEventWithParamsFromContainer(int id,
                                            const multiCallBackParams_t &params,
                                            scriptStorage &storage) {
    for (auto &scripts : storage) {
        auto it = scripts.callbacks.find(id);

        if (scripts.callbacksAdded && it != scripts.callbacks.end()) {
            lua_State *pState = scripts.luaState.get();

            lua_rawgeti(pState, LUA_REGISTRYINDEX, (*it).second);

            for (const auto &p : params) {
                p.pushToLuaStack(pState);
            }

            if (runScriptWithArgs(scripts, params.size()) != 0) {
                std::string errors = "Event name ";
                errors += std::to_string(id);
                errors += " num params: ";
                errors += std::to_string(params.size());
                errors += " data: ";

                for (const auto &p : params) {
                    switch (p.getType()) {
                    case LUA_TNIL:
                        errors += " <null> ";
                        break;

                    case LUA_TNUMBER:
                        errors += " Number<";
                        errors += std::to_string(p.getNumber());
                        errors += "> ";
                        break;

                    case LUA_TBOOLEAN:
                        errors += ((static_cast<int>(p.getBoolean()) != 0)
                                       ? " Boolean<True> "
                                       : " Boolean<False> ");
                        break;

                    case LUA_TSTRING:
                        errors += " String<";
                        errors += p.getString();
                        errors += "> ";
                        break;

                    case LUA_TTABLE:
                        errors += " <Get LUA_TTABLE: **TODO**> ";
                        break;

                    case LUA_TFUNCTION:
                        errors += " <Get LUA_TFUNCTION: **TODO**> ";
                        break;

                    case LUA_TUSERDATA:
                        errors += " <Get LUA_TUSERDATA: **TODO**> ";
                        break;

                    case LUA_TTHREAD:
                        errors += " <Get LUA_TTHREAD: **TODO**> ";
                        break;

                    case LUA_TLIGHTUSERDATA:
                        errors += " <Get LUA_TLIGHTUSERDATA: **TODO**> ";
                        break;

                    default:
                        errors += " <unknow argment type> ";
                        break;
                    }
                }

                CLog::log() << errors;
                catchErrorString(scripts);
            }
        }
    }
}

void CLuaH::runScriptsFromPath(const std::string &path) {
    for (auto &pathScripts : Lua().files[path]) {
        runScript(pathScripts.second);
    }
}

void CLuaH::runScriptsFromStorage(scriptStorage &storage) {
    for (auto &pathScripts : storage) {
        runScript(pathScripts);
    }
}

void CLuaH::runScripts() {
    static const std::string barra("/");
    for (auto &pathScripts : Lua().files) {
        for (auto &scripts : pathScripts.second) {
            runScript(scripts.second);
        }
    }
}

/*
 * Run a internal event (calls him specifics callbacks)
 */
void CLuaH::runInternalEvent(luaScript &L, int id) noexcept {
    if (L.callbacksAdded) {
        auto it = L.callbacks.find(id);
        if (it != L.callbacks.end()) {
            lua_State *pState = L.luaState.get();
            lua_rawgeti(pState, LUA_REGISTRYINDEX, (*it).second);
            if (runScript(L) != 0) {
                {
                    catchErrorString(L);
                }
            }
        }
    }
}

void CLuaH::runInternalEventWithParams(
    luaScript &L, int id, const multiCallBackParams_t &params) noexcept {
    if (L.callbacksAdded) {
        auto it = L.callbacks.find(id);
        if (it != L.callbacks.end()) {
            lua_State *pState = L.luaState.get();
            lua_rawgeti(pState, LUA_REGISTRYINDEX, (*it).second);

            for (const auto &p : params) {
                p.pushToLuaStack(pState);
            }

            if (runScriptWithArgs(L, params.size()) != 0) {
                {
                    catchErrorString(L);
                }
            }
        }
    }
}

CLuaH::luaScript::luaScript() noexcept {
    luaState = nullptr;
    runAgain = true;
    callbacksAdded = false;
}

auto CLuaH::luaScript::operator=(luaScript &&script) noexcept
    -> CLuaH::luaScript & {
    luaState = std::move(script.luaState);

    savedValues = std::move(script.savedValues);
    filePath = std::move(script.filePath);
    fileName = std::move(script.fileName);
    callbacks = std::move(script.callbacks);

    script.luaState = nullptr;

    return *this;
}

std::vector<char> bytes;

static auto luaWriter(lua_State *L, const void *p, size_t sz, void *ud) -> int {
    const char *pc = (const char *)p;
    bytes.insert(bytes.end(), pc, pc + sz);
    return 0;
}

auto CLuaH::luaScript::clone() const -> CLuaH::luaScript {
    bytes.clear();
    luaScript lData;
    lData.luaState = make_luaState();
    luaL_openlibs(lData.luaState.get());
    lData.customPtr = customPtr;
    CLuaFunctions::LuaF().registerFunctions(lData.luaState);
    CLuaFunctions::LuaF().registerGlobals(lData.luaState);

    lua_dump(luaState.get(), luaWriter, nullptr, 0);

    luaL_loadbuffer(lData.luaState.get(), &bytes[0], bytes.size(), "Clone");

    return lData;
}

auto CLuaH::luaScript::dumpBytecode() const -> std::vector<char> {
    std::vector<char> result;

    auto luaWriterLambda = [](lua_State *L, const void *p, size_t sz,
                              void *ud) {
        std::vector<char> &result = *(std::vector<char> *)ud;

        const char *pc = (const char *)p;
        result.insert(result.end(), pc, pc + sz);
        return 0;
    };

    lua_State *L = luaState.get();

    int dump_result = lua_dump(L, luaWriterLambda, &result, 0);

    if (dump_result != 0) {
        CLog::log() << std::string("lua_dump(") +
                           std::to_string(reinterpret_cast<uintptr_t>(L)) +
                           std::string(", ") +
                           std::string(filePath + "/" + fileName) +
                           std::string(") failed to load with result ") +
                           std::to_string(dump_result);

        CLuaH::Lua().catchErrorString(*this);
    }

    return result;
}

void CLuaH::luaScript::unload() noexcept {
    if (luaState != nullptr) {
        CLuaH::Lua().runInternalEvent(*this, destroyScriptInstance);

        luaState = nullptr;
    }

    callbacksAdded = false;
    savedValues.clear();
    filePath.clear();
    fileName.clear();
    callbacks.clear();
}

CLuaH::luaScript::luaScript(luaScript &&L) noexcept {
    luaState = std::move(L.luaState);

    savedValues = std::move(L.savedValues);
    filePath = std::move(L.filePath);
    fileName = std::move(L.fileName);
    callbacks = std::move(L.callbacks);

    L.luaState = nullptr;

    callbacksAdded = L.callbacksAdded;
}

auto CLuaH::getLuaStateScript(lua_State *L) -> CLuaH::luaScript & {
    return *((luaScript *)L->script);
}

auto CLuaH::getGlobalVarAsString(luaScript &l, const std::string &varname)
    -> std::string {
    lua_State *L = l.luaState.get();
    lua_getglobal(L, varname.c_str());

    return lua_tostring(L, -1);
}

auto CLuaH::getGlobalVarAsString(luaScript &l, const char *varname) noexcept
    -> const char * {
    lua_State *L = l.luaState.get();
    lua_getglobal(L, varname);

    if (lua_isnil(L, -1)) {
        {
            return "";
        }
    }

    return lua_tostring(L, -1);
}

CLuaH::luaScript::~luaScript() noexcept {
    if (luaState) {
        try {
            CLuaH::Lua().runInternalEvent(*this, destroyScriptInstance);
        } catch (...) {
        }

        customPtr = nullptr;
    }
}

void CLuaH::unloadAll() noexcept { files.clear(); }

auto CLuaH::idForCallbackEvent(const std::string &s) -> int {
    nullinit<int> &r = callbacksNamesID[s];

    if (r() == 0) {
        r() = callbacksNamesID.size();
    }

    return r();
}

int CLuaH::destroyScriptInstance = 0;

CLuaH::CLuaH() noexcept {
    registerCustomFunctions = true;
    inited = true; /*loadFiles("LuaScripts");

     runScripts();*/

    destroyScriptInstance = idForCallbackEvent("destroyScriptInstance");
}

void CLuaH::customParam::loadTableWOPush(lua_State *L) {
    int index = -2;

    while ((index = lua_next(L, -2)) != 0) {
        std::string dataStr = std::string(lua_tostring(L, -2));

        const static std::map<std::string, bool> n{
            {"string", true},  {"config", true},    {"io", true},
            {"stderr", true},  {"stdout", true},    {"_G", true},
            {"preload", true}, {"searchers", true}, {"package", true},
            {"utf8", true},    {"debug", true}};

        if (n.find(dataStr) != n.end()) {
            lua_pop(L, 1);
            continue;
        }

        if (lua_istable(L, -1)) {
        }

        customParam np;

        np.getFromArgs(L, -1);

        if (np.getType() != LUA_TFUNCTION) {
            tableData[dataStr] = np;

            const char *ch = lua_tostring(L, -1);

            // std::cout << dataStr << " value " << (ch? ch: "") << std::endl;
        }

        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

void CLuaH::customParam::loadTable(lua_State *L, int idx) {
    lua_pushvalue(L, idx);
    lua_pushnil(L);
    loadTableWOPush(L);
}
