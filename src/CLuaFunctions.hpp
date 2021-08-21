#pragma once
#ifndef LUA_HOOKER_CLUAFUNCTIONS_HPP
#define LUA_HOOKER_CLUAFUNCTIONS_HPP

#include "CLuaH.hpp"
#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>

class CLuaFunctions {
    std::deque<std::function<int(CLuaH::luaState_t &)>> registerFunctionsAPICBs;
    std::deque<std::function<int(CLuaH::luaState_t &)>> registerGlobalsAPICBs;
    std::deque<std::function<void(void)>> frameUpdateAPICBs;

  public:
    static CLuaFunctions &LuaF();

    class LuaParams {
        lua_State *L;
        int num_params, ret, stck, fail_bit;

      public:
        LuaParams &operator<<(const std::string &param);
        LuaParams &operator<<(double param);
        LuaParams &operator<<(int32_t param);
        LuaParams &operator<<(uint32_t param);
        LuaParams &operator<<(int64_t param);
        LuaParams &operator<<(bool param);
        LuaParams &operator<<(uint64_t param);

        LuaParams &operator>>(std::string &param);
        LuaParams &operator>>(double &param);
        LuaParams &operator>>(int &param);
        LuaParams &operator>>(int64_t &param);
        LuaParams &operator>>(void *&param);
        LuaParams &operator>>(bool &param);

        template <class T> inline LuaParams &operator>>(T &param) {
            if (stck <= num_params) {
                param = static_cast<T>(lua_tointeger(L, stck));
                ++stck;
            } else {
                fail_bit = 1;
            }

            return *this;
        }

        auto getNumParams() const { return num_params; }
        auto rtn() const -> int { return ret; }

        auto fail() const -> bool { return fail_bit != 0; }

        LuaParams(lua_State *state);
    };

    class GameVariables {
        enum vartype {
            nonev,
            integer64,
            integerv,
            uintegervar,
            doublevar,
            stringvar,
            booleanvar
        };

        struct vard {
            vartype t;
            void *ptr;

            inline vard() {
                t = nonev;
                ptr = nullptr;
            }
        };

        std::map<std::string, vard> vars;

      public:
        void pushVar(const std::string &name, int &var);
        void pushVar(const std::string &name, int64_t &var);
        void pushVar(const std::string &name, double &var);
        void pushVar(const std::string &name, std::string &var);
        void pushVar(const std::string &name, bool &var);

        void setVar(const std::string &name, int value);
        void setVar(const std::string &name, int64_t value);
        void setVar(const std::string &name, double value);
        void setVar(const std::string &name, const std::string &value);
        void setVar(const std::string &name, bool value);

        void pushToLuaStack(const std::string &name, lua_State *L);

        void setVar(const std::string &name, lua_State *L, int stackIDX);

        void removeVar(const std::string &name);

        static GameVariables &gv();

      private:
        GameVariables(GameVariables &) = delete;
        GameVariables();
    };

    // Custom lua functions

    /*
     * Set Window/game configurations
     */
    static int setConfigs(lua_State *L);

    /*
     * Don't run the script again... Deprecated futurely?
     */
    static int doNotRunAgain(lua_State *L);

    /*
     * Set a lua function as callback for a event
     */
    static int setCallBackToEvent(lua_State *L);

    /*
     * CFonts/Draw text
     */
    static int drawTextOnScreen(lua_State *L);

    /*
     * New menu
     */
    static int newMenu(lua_State *L);

    /*
     * New menu option
     */
    static int newMenuOption(lua_State *L);

    /*
     *
     */
    static int getMainMenuName(lua_State *L);

    /*
     * Output a string in game log
     */
    static int printTolog(lua_State *L);

    /*
     *
     */
    static int getMenuOptionName(lua_State *L);

    /*
     *
     */
    static int setMenuBackgroundTexture(lua_State *L);

    /*
     *
     */
    static int assingMenuToOtherMenuOption(lua_State *L);

    /*
     *
     */
    static int assingGameFunctionToMenuOption(lua_State *L);

    /*
     *
     */
    static int getNumOfMenuOptions(lua_State *L);

    /*
     *
     */
    static int getActualMenu(lua_State *L);

    /*
     *
     */
    static int getGameCallback(lua_State *L);

    /*
     * Load a texture (load file & load opengl texture)
     */
    static int loadTexture(lua_State *L);

    /*
     * Adds Texture to font and create a font instance if it doesn't exists
     */
    static int addTextureToFont(lua_State *L);

    /*
     *
     */
    static int newGamePlayModule(lua_State *L);

    static int loadSound(lua_State *L);
    static int loadSoundDecode(lua_State *L);
    static int playSound(lua_State *L);
    static int pauseSound(lua_State *L);
    static int stopSound(lua_State *L);
    static int setSoundTime(lua_State *L);
    static int setSoundVolume(lua_State *L);
    static int getSoundVolume(lua_State *L);
    static int getSoundTime(lua_State *L);
    static int releaseSound(lua_State *L);
    static int setSoundFlags(lua_State *L);
    static int setSoundAttribute(lua_State *L);

    static int getBassError(lua_State *L);

    static int setGameVar(lua_State *L);
    static int getGameVar(lua_State *L);

    static int printGlobals(lua_State *L);

    /*
     * Register custom functions lua state
     */
    static void registerFunctions(CLuaH::luaState_t &Lstate);

    /*
     * Register default game globals
     */
    static void registerGlobals(CLuaH::luaState_t &L);

    void
    registerLuaFuncsAPI(const std::function<int(CLuaH::luaState_t &)> &fun);
    void
    registerLuaGlobalsAPI(const std::function<int(CLuaH::luaState_t &)> &fun);
    void registerFrameUpdateAPI(const std::function<void(void)> &fun);

  private:
    CLuaFunctions();
    CLuaFunctions(CLuaFunctions &) = delete;
    ~CLuaFunctions() = default;
};

template <class T>
static inline void setLuaGlobal(lua_State *L, const std::string &name,
                                const T &value) {
    CLuaH::customParam(value).pushToLuaStack(L);
    lua_setglobal(L, name.c_str());
}

template <class T>
static inline void setLuaGlobal(CLuaH::luaState_t &L, const std::string &name,
                                const T &value) {
    CLuaH::customParam(value).pushToLuaStack(L);
    lua_setglobal(L.get(), name.c_str());
}

#endif
