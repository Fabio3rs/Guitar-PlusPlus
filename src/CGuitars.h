#pragma once
#ifndef _CGUITARPP_CGUITARS_H_
#define _CGUITARPP_CGUITARS_H_

#include "CLuaH.hpp"
#include "GPPOBJ.h"
#include <fstream>
#include <map>
#include <string>

class CGamePlay;

class CGuitars {
    static int addGuitarScriptEvent, loadGuitarScriptEvent,
        unloadingGuitarScriptEvent;

  public:
    class CGuitar {
        friend CGuitars;
        friend CGamePlay;
        std::string path, internalName;
        std::string gameplayBarName;
        unsigned int textureID;
        unsigned int fretboardText;
        GPPOBJ gameplayBar;
        bool loaded;
        CLuaH::luaScript luaF;

      public:
        inline const std::string &getPath() noexcept { return path; }

        inline const std::string &getInternalName() noexcept {
            return internalName;
        }

        std::string name;

        // Loading/unloading belongs to the GL owner, outside gameplay.
        bool load();
        bool isLoaded() const noexcept { return loaded; }
        void unload();

        CGuitar() noexcept;
    };

    static CGuitars &inst();

    // Throws std::out_of_range for missing IDs; never inserts.
    CGuitar &getGuitar(const std::string &internalName);
    CGuitar *getGuitarIfExists(const std::string &internalName);
    // Returns nullptr on invalid configuration; existing IDs retain their cache.
    CGuitar *addGuitar(const std::string &path,
                       const std::string &internalName);

    void loadAllGuitars();

    void unload();

  protected:
    static int registerFunctions(CLuaH::luaState_t &L);
    static int registerGlobals(CLuaH::luaState_t &L);

    static int changeGuitarData(lua_State *L);

  private:
    // Entries remain stable until explicit shutdown; no eviction during sessions.
    std::map<std::string, CGuitar> guitars;
    CGuitar *staging = nullptr;

    CGuitars();
};

#endif
