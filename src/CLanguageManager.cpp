#include "CLanguageManager.h"
#include "CLog.h"
#include "CLuaFunctions.hpp"
#include <dirent.h>
#include <iostream>

static void PrintTable(lua_State *L) {
    lua_pushnil(L);

    int index = 0;

    while ((index = static_cast<int>(lua_next(L, -2) != 0)) != 0) {
        if (lua_isstring(L, -1) != 0) {
            {
                printf("%s = %s\n", lua_tostring(L, -2), lua_tostring(L, -1));
            }
        } else if (lua_isnumber(L, -1) != 0) {
            { printf("%s = %f\n", lua_tostring(L, -2), lua_tonumber(L, -1)); }
        } else if (lua_istable(L, -1)) {
            { PrintTable(L); }
        }

        lua_pop(L, 1);
    }
}

CLanguageManager::CLanguage::CLanguage(const std::string &path) {
    /*entries["menuPlayTitle"] = CLuaH::getGlobalVarAsString(ls,
    "menuPlayTitle"); entries["menuOptionsTitle"] =
    CLuaH::getGlobalVarAsString(ls, "menuOptionsTitle");
    entries["menuExtrasTitle"] = CLuaH::getGlobalVarAsString(ls,
    "menuExtrasTitle");
    */
    auto &lua = CLuaH::Lua();
    const std::string langPath = "data/languages/" + path;

    lua.registerCustomFunctions = false;

    if (lua.loadFiles(langPath)) {
        lua.runScriptsFromPath(langPath);
        for (auto &langscript : lua.files[langPath]) {
            std::string name = langscript.first;
            size_t p = name.find_first_of('.');
            if (p != std::string::npos) {
                name.resize(p);
            }

            loadFrom(langscript.second);
        }
    }

    lua.registerCustomFunctions = true;

    loaded = true;
}

void CLanguageManager::CLanguage::loadFrom(CLuaH::luaScript &ls) {
    luaf.push_back(&ls);

    std::string tmpName = CLuaH::getGlobalVarAsString(ls, "langName");

    if (!tmpName.empty()) {
        {
            name = tmpName;
        }
    }

    lua_State *L = ls.luaState.get();

    lua_pushglobaltable(L);
    lua_pushnil(L);

    int index = -2;

    while ((index = lua_next(L, -2)) != 0) {
        // std::cout << lua_tostring(L, -2) << " " << lua_isstring(L, -1) <<
        // std::endl;

        if (lua_isstring(L, -1) != 0) {
            entries[lua_tostring(L, -2)] = lua_tostring(L, -1);
        }

        lua_pop(L, 1);
    }

    lua_pop(L, 1);

    loaded = true;
}

CLanguageManager::CLanguage::CLanguage() { loaded = false; }

auto CLanguageManager::getLanguageName(const std::string &lang) -> std::string {
    return langs[lang].name;
}

auto CLanguageManager::getTextDirectFromFile(const std::string &lang,
                                             const std::string &entry)
    -> std::string {
    /*if (langs[lang].luaf == nullptr)
            return "Lang file " + lang + " not loaded";

    return CLuaH::getGlobalVarAsString(*langs[lang].luaf, entry);*/

    return "TODO";
}

auto CLanguageManager::getText(const std::string &lang,
                               const std::string &entry) -> std::string & {
    return langs[lang].entries[entry];
}

CLanguageManager::CLanguageManager() {
    usingLang = "PT-BR";
    CLuaFunctions::LuaF().registerLuaFuncsAPI(registerFunctions);
    CLuaFunctions::LuaF().registerLuaFuncsAPI(registerGlobals);

    auto &lua = CLuaH::Lua();
    const std::string langPath = "./data/languages";

    lua.registerCustomFunctions = false;

    DIR *direntd = opendir(langPath.c_str());
    dirent *rrd = nullptr;

    if (direntd != nullptr) {
        rrd = readdir(direntd);
        while ((rrd = readdir(direntd)) != nullptr) {
            std::string dname = rrd->d_name;
            if ((rrd->d_type & DT_DIR) != 0 && dname != "." && dname != "..") {
                CLog::log() << ("Loading <<" + langPath + "/" + dname + ">>");

                langs[rrd->d_name] = CLanguage(dname);
            }
        }
        closedir(direntd);
    }

    lua.registerCustomFunctions = true;
}

auto CLanguageManager::translateString(lua_State *L) -> int {
    CLuaFunctions::LuaParams p(L);

    if (p.getNumParams() > 1) {
        std::string key;
        std::string usingLangL = langMGR().usingLang;

        p >> key;

        if (p.getNumParams() == 2) {
            {
                p >> usingLangL;
            }
        }

        p << langMGR().getText(usingLangL, key);
    } else {
        p << true;
    }

    return p.rtn();
}

auto CLanguageManager::registerFunctions(CLuaH::luaState_t &Lstate) -> int {
    lua_State *L = Lstate.get();

    lua_register(L, "translateString", translateString);

    return 0;
}

auto CLanguageManager::registerGlobals(CLuaH::luaState_t &L) -> int {
    return 0;
}

auto CLanguageManager::langMGR() -> CLanguageManager & {
    static CLanguageManager mgr;
    return mgr;
}
