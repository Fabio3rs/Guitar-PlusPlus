#include "CLuaFunctions.hpp"
#include "CControls.h"
#include "CFonts.h"
#include "CLog.h"
#include "GPPGame.h"
#include <cmath>
#include <iostream>
#include <limits>

namespace {
bool validInteger(lua_State *L, int index, double low, double high) {
    if (lua_type(L, index) != LUA_TNUMBER) {
        return false;
    }

    const double v = lua_tonumber(L, index);
    return std::isfinite(v) && std::floor(v) == v && v >= low && v <= high;
}

bool validMenuIndex(lua_State *L, int index, const CMenu &menu) {
    return !menu.options.empty() &&
           validInteger(L, index, 0,
                        static_cast<double>(menu.options.size() - 1));
}

int fontMetrics(lua_State *L) {
    bool ok = lua_gettop(L) == 4 && lua_type(L, 1) == LUA_TSTRING &&
              lua_type(L, 2) == LUA_TNUMBER && lua_type(L, 3) == LUA_TNUMBER &&
              lua_type(L, 4) == LUA_TNUMBER;
    if (ok) {
        ok = CFonts::fonts().setFontMetrics(
            lua_tostring(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3),
            lua_tonumber(L, 4));
    }

    lua_pushboolean(L, ok);
    return 1;
}

int fontSpacing(lua_State *L) {
    bool ok = lua_gettop(L) == 4 && lua_type(L, 1) == LUA_TSTRING &&
              lua_type(L, 2) == LUA_TSTRING && lua_type(L, 3) == LUA_TNUMBER &&
              lua_type(L, 4) == LUA_TNUMBER;
    if (ok) {
        ok = CFonts::fonts().setFontCharSpacing(
            lua_tostring(L, 1), lua_tostring(L, 2), lua_tonumber(L, 3),
            lua_tonumber(L, 4));
    }

    lua_pushboolean(L, ok);
    return 1;
}

int menuBinding(lua_State *L) {
    bool ok = lua_gettop(L) == 5 && lua_type(L, 1) == LUA_TSTRING &&
              lua_type(L, 2) == LUA_TSTRING && validInteger(L, 3, 0, 1023) &&
              validInteger(L, 4, -1, 15) && lua_type(L, 5) == LUA_TSTRING;
    CControls::MenuBinding binding;
    if (ok) {
        binding.guid = lua_tostring(L, 1);
        binding.index = static_cast<int>(lua_tointeger(L, 3));
        binding.direction = static_cast<int>(lua_tointeger(L, 4));
        const std::string source = lua_tostring(L, 2),
                          action = lua_tostring(L, 5);
        if (source == "button") {
            binding.source = CControls::MenuBinding::Button;
        } else if (source == "hat") {
            binding.source = CControls::MenuBinding::Hat;
            ok = binding.direction > 0;
        } else if (source == "axis") {
            binding.source = CControls::MenuBinding::Axis;
            ok = binding.direction == 1 || binding.direction == -1;
        } else {
            ok = false;
        }

        const char *names[] = {"up",    "down",    "left",
                               "right", "confirm", "back"};
        bool found = false;
        for (int i = 0; i < 6; ++i) {
            if (action == names[i]) {
                binding.action = static_cast<CControls::MenuAction>(i);
                found = true;
            }
        }

        ok = ok && found && !binding.guid.empty();
    }

    if (ok) {
        auto &bindings = CControls::controls().menuBindings;
        auto it =
            std::find_if(bindings.begin(), bindings.end(), [&](const auto &b) {
                return b.guid == binding.guid && b.action == binding.action;
            });
        if (it == bindings.end()) {
            bindings.push_back(binding);
        } else {
            *it = binding;
        }
    }

    lua_pushboolean(L, ok);
    return 1;
}
} // namespace

void CLuaFunctions::registerLuaFuncsAPI(
    const std::function<int(CLuaH::luaState_t &)> &fun) {
    registerFunctionsAPICBs.push_back(fun);
}

void CLuaFunctions::registerLuaGlobalsAPI(
    const std::function<int(CLuaH::luaState_t &)> &fun) {
    registerGlobalsAPICBs.push_back(fun);
}

void CLuaFunctions::registerFrameUpdateAPI(
    const std::function<void(void)> &fun) {
    frameUpdateAPICBs.push_back(fun);
}

auto CLuaFunctions::LuaF() -> CLuaFunctions & {
    static CLuaFunctions LuaF;
    return LuaF;
}

CLuaFunctions::LuaParams::LuaParams(lua_State *state) {
    L = state;
    num_params = lua_gettop(L);

    ret = 0;
    stck = 1;
    fail_bit = 0;
}

auto CLuaFunctions::LuaParams::operator<<(const std::string &param)
    -> CLuaFunctions::LuaParams & {
    lua_pushstring(L, param.c_str());
    ++ret;

    return *this;
}

auto CLuaFunctions::LuaParams::operator<<(double param)
    -> CLuaFunctions::LuaParams & {
    lua_pushnumber(L, param);
    ++ret;

    return *this;
}

auto CLuaFunctions::LuaParams::operator<<(int32_t param)
    -> CLuaFunctions::LuaParams & {
    lua_pushinteger(L, param);
    ++ret;

    return *this;
}

auto CLuaFunctions::LuaParams::operator<<(uint32_t param)
    -> CLuaFunctions::LuaParams & {
    lua_pushinteger(L, param);
    ++ret;

    return *this;
}

auto CLuaFunctions::LuaParams::operator<<(int64_t param)
    -> CLuaFunctions::LuaParams & {
    lua_pushinteger(L, param);
    ++ret;

    return *this;
}

auto CLuaFunctions::LuaParams::operator<<(uint64_t param)
    -> CLuaFunctions::LuaParams & {
    lua_pushinteger(L, param);
    ++ret;

    return *this;
}

auto CLuaFunctions::LuaParams::operator<<(bool param)
    -> CLuaFunctions::LuaParams & {
    lua_pushboolean(L, static_cast<int>(param));
    ++ret;

    return *this;
}

auto CLuaFunctions::LuaParams::operator>>(std::string &param)
    -> CLuaFunctions::LuaParams & {
    if (stck <= num_params) {
        param = lua_tostring(L, stck);
        ++stck;
    } else {
        fail_bit = 1;
    }

    return *this;
}

auto CLuaFunctions::LuaParams::operator>>(double &param)
    -> CLuaFunctions::LuaParams & {
    if (stck <= num_params) {
        param = lua_tonumber(L, stck);
        ++stck;
    } else {
        fail_bit = 1;
    }

    return *this;
}

auto CLuaFunctions::LuaParams::operator>>(int &param)
    -> CLuaFunctions::LuaParams & {
    if (stck <= num_params) {
        param = static_cast<int>(lua_tointeger(L, stck));
        ++stck;
    } else {
        fail_bit = 1;
    }

    return *this;
}

auto CLuaFunctions::LuaParams::operator>>(int64_t &param)
    -> CLuaFunctions::LuaParams & {
    if (stck <= num_params) {
        param = lua_tointeger(L, stck);
        ++stck;
    } else {
        fail_bit = 1;
    }

    return *this;
}

auto CLuaFunctions::LuaParams::operator>>(void *&param)
    -> CLuaFunctions::LuaParams & {
    if (stck <= num_params) {
        param = (void *)lua_topointer(L, stck);
        ++stck;
    } else {
        fail_bit = 1;
    }

    return *this;
}

auto CLuaFunctions::LuaParams::operator>>(bool &param)
    -> CLuaFunctions::LuaParams & {
    if (stck <= num_params) {
        param = lua_toboolean(L, stck) != 0;
        ++stck;
    } else {
        fail_bit = 1;
    }

    return *this;
}

auto CLuaFunctions::GameVariables::gv() -> CLuaFunctions::GameVariables & {
    static GameVariables gvarsmgr;
    return gvarsmgr;
}

void CLuaFunctions::GameVariables::pushVar(const std::string &name, int &var) {
    vard v;
    v.ptr = &var;
    v.t = integerv;

    vars[name] = v;
}

void CLuaFunctions::GameVariables::pushVar(const std::string &name,
                                           int64_t &var) {
    vard v;
    v.ptr = &var;
    v.t = integer64;

    vars[name] = v;
}

void CLuaFunctions::GameVariables::pushVar(const std::string &name,
                                           double &var) {
    vard v;
    v.ptr = &var;
    v.t = doublevar;

    vars[name] = v;
}

void CLuaFunctions::GameVariables::pushVar(const std::string &name,
                                           std::string &var) {
    vard v;
    v.ptr = &var;
    v.t = stringvar;

    vars[name] = v;
}

void CLuaFunctions::GameVariables::pushVar(const std::string &name, bool &var) {
    vard v;
    v.ptr = &var;
    v.t = booleanvar;

    vars[name] = v;
}

void CLuaFunctions::GameVariables::setVar(const std::string &name,
                                          int64_t value) {
    auto &v = vars[name];

    switch (v.t) {
    case integerv:
        *(int *)v.ptr = static_cast<int>(value);
        break;

    case integer64:
        *(int64_t *)v.ptr = value;
        break;

    case doublevar:
        *(double *)v.ptr = static_cast<double>(value);
        break;

    case booleanvar:
        *(bool *)v.ptr = value != 0;
        break;

    case stringvar:
        *(std::string *)v.ptr = std::to_string(value);
        break;
    }
}

void CLuaFunctions::GameVariables::setVar(const std::string &name,
                                          double value) {
    auto &v = vars[name];

    switch (v.t) {
    case integerv:
        *(int *)v.ptr = static_cast<int>(value);
        break;

    case integer64:
        *(int64_t *)v.ptr = static_cast<int64_t>(value);
        break;

    case doublevar:
        *(double *)v.ptr = value;
        break;

    case booleanvar:
        *(bool *)v.ptr = value != 0;
        break;

    case stringvar:
        *(std::string *)v.ptr = std::to_string(value);
        break;
    }
}

void CLuaFunctions::GameVariables::setVar(const std::string &name,
                                          const std::string &value) {
    auto &v = vars[name];

    switch (v.t) {
    case integerv:
        *(int *)v.ptr = std::stoi(value);
        break;

    case integer64:
        *(int64_t *)v.ptr = std::stoll(value);
        break;

    case doublevar:
        *(double *)v.ptr = std::stod(value);
        break;

    case booleanvar:
        *(bool *)v.ptr = std::stoi(value) != 0;
        break;

    case stringvar:
        *(std::string *)v.ptr = value;
        break;
    }
}

void CLuaFunctions::GameVariables::setVar(const std::string &name, bool value) {
    auto &v = vars[name];

    switch (v.t) {
    case integerv:
        *(int *)v.ptr = static_cast<int>(value);
        break;

    case integer64:
        *(int64_t *)v.ptr = static_cast<int64_t>(value);
        break;

    case doublevar:
        *(double *)v.ptr = static_cast<double>(value);
        break;

    case booleanvar:
        *(bool *)v.ptr = value;
        break;

    case stringvar:
        *(std::string *)v.ptr = std::to_string(static_cast<int>(value));
        break;
    }
}

void CLuaFunctions::GameVariables::setVar(const std::string &name, int value) {
    auto &v = vars[name];

    switch (v.t) {
    case integerv:
        *(int *)v.ptr = value;
        break;

    case integer64:
        *(int64_t *)v.ptr = value;
        break;

    case doublevar:
        *(double *)v.ptr = value;
        break;

    case booleanvar:
        *(bool *)v.ptr = (value != 0);
        break;

    case stringvar:
        *(std::string *)v.ptr = std::to_string(value);
        break;
    }
}

void CLuaFunctions::GameVariables::setVar(const std::string &name, lua_State *L,
                                          int stackIDX) {
    CLuaH::customParam c;

    c.getFromArgs(L, stackIDX);

    auto &v = vars[name];

    switch (v.t) {
    case integerv:
        setVar(name, c.getNumber());
        break;

    case doublevar:
        setVar(name, c.getNumber());
        break;

    case booleanvar:
        setVar(name, c.getBoolean());
        break;

    case stringvar:
        setVar(name, c.getString());
        break;
    }
}

void CLuaFunctions::GameVariables::pushToLuaStack(const std::string &name,
                                                  lua_State *L) {
    auto &v = vars[name];
    CLuaH::customParam c;

    switch (v.t) {
    case integerv:
        c = CLuaH::customParam(*(int *)v.ptr);
        c.pushToLuaStack(L);
        break;

    case integer64:
        c = CLuaH::customParam(*(int64_t *)v.ptr);
        c.pushToLuaStack(L);
        break;

    case doublevar:
        c = CLuaH::customParam(*(double *)v.ptr);
        c.pushToLuaStack(L);
        break;

    case booleanvar:
        c = CLuaH::customParam(*(bool *)v.ptr);
        c.pushToLuaStack(L);
        break;

    case stringvar:
        c = CLuaH::customParam(*(std::string *)v.ptr);
        c.pushToLuaStack(L);
        break;
    }
}

CLuaFunctions::GameVariables::GameVariables() {}

auto CLuaFunctions::setGameVar(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 2 && (lua_isstring(L, 1) != 0)) {
        std::string varname;

        p >> varname;

        GameVariables::gv().setVar(varname, L, 2);
    }

    return p.rtn();
}

auto CLuaFunctions::getGameVar(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 1 && (lua_isstring(L, 1) != 0)) {
        std::string varname;

        p >> varname;

        GameVariables::gv().pushToLuaStack(varname, L);
    }

    return p.rtn();
}

auto CLuaFunctions::printGlobals(lua_State *L) -> int {
    CLuaH::customParam c;
    CLuaH::loadGlobalTable(L, c);
    return 0;
}

auto CLuaFunctions::setConfigs(lua_State *L) -> int {
    auto cfg = GPPGame::GuitarPP().getWindowConfig();
    auto readWindowInteger = [L](const char *name, int &value, int maximum) {
        lua_getglobal(L, name);
        if (!lua_isnil(L, -1)) {
            const double number = lua_tonumber(L, -1);
            if (lua_isnumber(L, -1) && std::isfinite(number) && number >= 0 &&
                number <= maximum && std::floor(number) == number) {
                value = static_cast<int>(number);
            } else {
                CLog::log()
                    << std::string("Invalid configuration value: ") + name;
            }
        }

        lua_pop(L, 1);
    };

    readWindowInteger("WindowWidth", cfg.w, std::numeric_limits<int>::max());

    readWindowInteger("WindowHeight", cfg.h, std::numeric_limits<int>::max());

    lua_getglobal(L, "FullScreen");

    if (lua_isboolean(L, -1)) {
        cfg.fullscreen = (cfg.fullscreen & ~1) | (lua_toboolean(L, -1) ? 1 : 0);
    }

    lua_getglobal(L, "Borderless");

    if (lua_isboolean(L, -1)) {
        {
            cfg.fullscreen =
                (cfg.fullscreen & ~2) | (lua_toboolean(L, -1) ? 2 : 0);
        }
    }

    lua_getglobal(L, "customTitle");

    if (lua_isstring(L, -1) != 0) {
        {
            cfg.name = lua_tostring(L, -1);
        }
    }

    readWindowInteger("VSync", cfg.VSyncMode, 3);

    readWindowInteger("AntiAliasing", cfg.AA, std::numeric_limits<int>::max());

    lua_getglobal(L, "Green");

    if (lua_isnumber(L, -1) != 0) {
        {
            GPPGame::GuitarPP().strumKeys[0] =
                static_cast<unsigned int>(lua_tointeger(L, -1));
        }
    }

    lua_getglobal(L, "Red");

    if (lua_isnumber(L, -1) != 0) {
        {
            GPPGame::GuitarPP().strumKeys[1] =
                static_cast<unsigned int>(lua_tointeger(L, -1));
        }
    }

    lua_getglobal(L, "Yellow");

    if (lua_isnumber(L, -1) != 0) {
        {
            GPPGame::GuitarPP().strumKeys[2] =
                static_cast<unsigned int>(lua_tointeger(L, -1));
        }
    }

    lua_getglobal(L, "Blue");

    if (lua_isnumber(L, -1) != 0) {
        {
            GPPGame::GuitarPP().strumKeys[3] =
                static_cast<unsigned int>(lua_tointeger(L, -1));
        }
    }

    lua_getglobal(L, "Orange");

    if (lua_isnumber(L, -1) != 0) {
        {
            GPPGame::GuitarPP().strumKeys[4] =
                static_cast<unsigned int>(lua_tointeger(L, -1));
        }
    }

    lua_getglobal(L, "fretOne");

    if (lua_isnumber(L, -1) != 0) {
        {
            GPPGame::GuitarPP().fretOneKey =
                static_cast<unsigned int>(lua_tointeger(L, -1));
        }
    }

    lua_getglobal(L, "fretTwo");

    if (lua_isnumber(L, -1) != 0) {
        {
            GPPGame::GuitarPP().fretTwoKey =
                static_cast<unsigned int>(lua_tointeger(L, -1));
        }
    }

    lua_getglobal(L, "Song");

    if (lua_isstring(L, -1) != 0) {
        {
            GPPGame::GuitarPP().songToLoad = lua_tostring(L, -1);
        }
    }

    lua_getglobal(L, "hyperSpeed");

    if (lua_isnumber(L, -1) != 0) {
        {
            const double value = lua_tonumber(L, -1);
            if (std::isfinite(value) && value > 0.0) {
                GPPGame::GuitarPP().hyperSpeed = value;
            } else {
                CLog::log() << "Invalid configuration value: hyperSpeed";
            }
        }
    }

    GPPGame::GuitarPP().settWindowConfigs(cfg);

    return 0;
}

/*
 *
 */
auto CLuaFunctions::newGamePlayModule(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 1 && (lua_isstring(L, 1) != 0)) {
        std::string name;

        p >> name;

        auto &module = GPPGame::GuitarPP().gameModules[name];

        lua_getglobal(L, "backgroundTexture");

        if (lua_isstring(L, -1) != 0) {
            {
                module.backgroundTexture = lua_tostring(L, -1);
            }
        }

        lua_getglobal(L, "fretsTextures");

        if (lua_isstring(L, -1) != 0) {
            {
                module.fretsTextures = lua_tostring(L, -1);
            }
        }

        lua_getglobal(L, "musicSpeed");

        if (lua_isnumber(L, -1) != 0) {
            {
                module.setMusicSpeed(lua_tonumber(L, -1));
            }
        }

        lua_getglobal(L, "hyperSpeed");

        if (lua_isnumber(L, -1) != 0) {
            {
                module.setHyperSpeed(lua_tonumber(L, -1));
            }
        }
    }

    return p.rtn();
}

auto CLuaFunctions::loadSound(lua_State *L) -> int {
    LuaParams p(L);

    for (size_t i = 0; i < p.getNumParams(); i++) {
        if (lua_isstring(L, i + 1) != 0) {
            std::string str;
            p >> str;

            int result = 0;

            if (CEngine::engine().loadSoundStream(str.c_str(), result)) {
                p << result;
            } else {
                p << false;
            }
        } else {
            p << false;
        }
    }

    return p.rtn();
}

auto CLuaFunctions::loadSoundDecode(lua_State *L) -> int {
    LuaParams p(L);

    for (size_t i = 0; i < p.getNumParams(); i++) {
        if (lua_isstring(L, i + 1) != 0) {
            std::string str = "<<nullstring>>";
            p >> str;

            int result = 0;

            if (CEngine::engine().loadSoundStream(str.c_str(), result, true)) {
                p << result;
            } else {
                p << false;
            }
        } else {
            p << false;
        }
    }

    return p.rtn();
}

auto CLuaFunctions::playSound(lua_State *L) -> int {
    LuaParams p(L);

    for (size_t i = 0; i < p.getNumParams(); i++) {
        if (lua_isnumber(L, i + 1) != 0) {
            int handle = 0;
            p >> handle;

            p << CEngine::engine().playSoundStream(handle);
        } else {
            p << false;
        }
    }

    return p.rtn();
}

auto CLuaFunctions::pauseSound(lua_State *L) -> int {
    LuaParams p(L);

    for (size_t i = 0; i < p.getNumParams(); i++) {
        if (lua_isnumber(L, i + 1) != 0) {
            int handle = 0;
            p >> handle;

            p << CEngine::engine().pauseSoundStream(handle);
        } else {
            p << false;
        }
    }

    return p.rtn();
}

auto CLuaFunctions::stopSound(lua_State *L) -> int {
    LuaParams p(L);

    for (size_t i = 0; i < p.getNumParams(); i++) {
        if (lua_isnumber(L, i + 1) != 0) {
            int handle = 0;
            p >> handle;

            if (CEngine::engine().pauseSoundStream(handle)) {
                p << true;
                CEngine::engine().setSoundTime(handle, 0.0);
            } else {
                p << false;
            }
        } else {
            p << false;
        }
    }

    return p.rtn();
}

auto CLuaFunctions::setSoundTime(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 2 && (lua_isnumber(L, 1) != 0) &&
        (lua_isnumber(L, 2) != 0)) {
        int handle = 0;
        double value = 0.0;

        p >> handle;
        p >> value;

        CEngine::engine().setSoundTime(handle, value);
    }

    return p.rtn();
}

auto CLuaFunctions::setSoundVolume(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 2 && (lua_isnumber(L, 1) != 0) &&
        (lua_isnumber(L, 2) != 0)) {
        int handle = 0;
        double value = 0.0;

        p >> handle;
        p >> value;

        p << CEngine::engine().setSoundVolume(handle,
                                              static_cast<float>(value));
    }

    return p.rtn();
}

auto CLuaFunctions::getSoundVolume(lua_State *L) -> int {
    LuaParams p(L);

    for (size_t i = 0; i < p.getNumParams(); i++) {
        if (lua_isnumber(L, i + 1) != 0) {
            int handle = 0;
            p >> handle;

            p << CEngine::engine().getSoundVolume(handle);
        } else {
            p << false;
        }
    }

    return p.rtn();
}

auto CLuaFunctions::getSoundTime(lua_State *L) -> int {
    LuaParams p(L);

    for (size_t i = 0; i < p.getNumParams(); i++) {
        if (lua_isnumber(L, i + 1) != 0) {
            int handle = 0;
            p >> handle;

            p << CEngine::engine().getSoundTime(handle);
        } else {
            p << false;
        }
    }

    return p.rtn();
}

auto CLuaFunctions::releaseSound(lua_State *L) -> int {
    LuaParams p(L);

    for (size_t i = 0; i < p.getNumParams(); i++) {
        if (lua_isnumber(L, i + 1) != 0) {
            int handle = 0;
            p >> handle;

            p << CEngine::engine().unloadSoundStream(handle);
        } else {
            p << false;
        }
    }

    return p.rtn();
}

auto CLuaFunctions::setSoundFlags(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 3 && (lua_isnumber(L, 1) != 0) &&
        (lua_isnumber(L, 2) != 0) && (lua_isnumber(L, 3) != 0)) {
        int handle = 0;
        int64_t flags = 0;
        int64_t mask = 0;

        p >> handle;
        p >> flags;
        p >> mask;

        p << CEngine::engine().setSoundFlags(handle, static_cast<int>(flags),
                                             static_cast<int>(mask));
    }

    return p.rtn();
}

auto CLuaFunctions::setSoundAttribute(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 3 && (lua_isnumber(L, 1) != 0) &&
        (lua_isnumber(L, 2) != 0) && (lua_isnumber(L, 3) != 0)) {
        int handle = 0;
        int flags = 0;
        double value = 0;

        p >> handle;
        p >> flags;
        p >> value;

        p << CEngine::engine().setSoundAttribute(handle, flags,
                                                 static_cast<float>(value));
    }

    return p.rtn();
}

auto CLuaFunctions::getBassError(lua_State *L) -> int {
    LuaParams p(L);

    p << CEngine::getBassError();

    return p.rtn();
}

/*
 * Load a texture (load file & load opengl texture)
 */
auto CLuaFunctions::loadTexture(lua_State *L) -> int {
    LuaParams p(L);

    for (size_t i = 0; i < p.getNumParams(); i++) {
        if (lua_isstring(L, i + 1) != 0) {
            std::string str;
            p >> str;

            unsigned int result = 0;

            result = GPPGame::GuitarPP()
                         .loadTexture("data/sprites", str,
                                      &CLuaH::Lua().getLuaStateScript(L))
                         .getTextId();

            if (result != 0u) {
                p << result;
            } else {
                p << false;
            }
        } else {
            p << false;
        }
    }

    return p.rtn();
}

auto CLuaFunctions::doNotRunAgain(lua_State *L) -> int {
    CLuaH::Lua().getLuaStateScript(L).runAgain = false;

    return 0;
}

/*
setCallBackToEvent(event, function)
*/
auto CLuaFunctions::setCallBackToEvent(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 2 && (lua_isstring(L, 1) != 0) &&
        lua_isfunction(L, 2)) {
        std::string eventName = "teste";
        eventName = lua_tostring(L, 1);

        int id = CLuaH::Lua().idForCallbackEvent(eventName);

        lua_pushvalue(L, 2);
        int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

        auto &script = CLuaH::Lua().getLuaStateScript(L);
        auto previous = script.callbacks.find(id);
        if (previous != script.callbacks.end()) {
            luaL_unref(L, LUA_REGISTRYINDEX, previous->second);
        }

        script.callbacks[id] = fnRef;
        script.callbacksAdded = true;
    }

    return p.rtn();
}

/**/
auto CLuaFunctions::drawTextOnScreen(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 4 && (lua_isstring(L, 1) != 0) &&
        (lua_isnumber(L, 2) != 0) && (lua_isnumber(L, 3) != 0) &&
        (lua_isnumber(L, 4) != 0)) {
        CFonts::fonts().drawTextInScreen(lua_tostring(L, 1), lua_tonumber(L, 2),
                                         lua_tonumber(L, 3),
                                         lua_tonumber(L, 4));
    }

    return p.rtn();
}

/*
 * New menu
 */
auto CLuaFunctions::newMenu(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 1 && (lua_isstring(L, 1) != 0)) {
        std::string str;
        p >> str;

        auto &game = GPPGame::GuitarPP();
        auto &menu =
            str.empty() ? game.menus.newMenu() : game.menus.newNamedMenu(str);
        p << menu.getName();
    }

    return p.rtn();
}

/*
 *
 */
auto CLuaFunctions::assingGameFunctionToMenuOption(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 3 && (lua_isstring(L, 1) != 0) &&
        (lua_isnumber(L, 2) != 0) && (lua_isstring(L, 3) != 0)) {
        if (!validInteger(L, 2, 0, std::numeric_limits<int>::max())) {
            lua_pushboolean(L, false);
            return 1;
        }

        std::string menuName;
        std::string gameFunction;
        int optID;

        p >> menuName;
        p >> optID;
        p >> gameFunction;

        auto *foundMenu = GPPGame::GuitarPP().menus.findMenu(menuName);
        if (!foundMenu || !validMenuIndex(L, 2, *foundMenu)) {
            lua_pushboolean(L, false);
            return 1;
        }

        auto &menu = *foundMenu;

        menu.options[optID].menusXRef.push_back(gameFunction);
    }

    return p.rtn();
}

/*
 *
 */
auto CLuaFunctions::assingMenuToOtherMenuOption(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 3 && (lua_isstring(L, 1) != 0) &&
        (lua_isnumber(L, 2) != 0) && (lua_isstring(L, 3) != 0)) {
        if (!validInteger(L, 2, 0, std::numeric_limits<int>::max())) {
            lua_pushboolean(L, false);
            return 1;
        }

        std::string menuName;
        std::string targetMenuName;
        int optID;

        p >> menuName;
        p >> optID;
        p >> targetMenuName;

        auto *foundMenu = GPPGame::GuitarPP().menus.findMenu(menuName);
        if (!foundMenu || !validMenuIndex(L, 2, *foundMenu)) {
            lua_pushboolean(L, false);
            return 1;
        }

        auto &menu = *foundMenu;

        menu.options[optID].menusXRef.push_back(targetMenuName);
    }

    return p.rtn();
}

/*
 * Output a string in game log
 */
auto CLuaFunctions::printTolog(lua_State *L) -> int {
    LuaParams p(L);

    std::string str = "Lua script log: ";

    for (size_t i = 1, size = p.getNumParams(); i <= size; i++) {
        switch (lua_type(L, i)) {
        case LUA_TNIL:
            str += " <null> ";
            break;

        case LUA_TNUMBER:
            str += " Number<";
            str += std::to_string(lua_tonumber(L, i));
            str += "> ";
            break;

        case LUA_TBOOLEAN:
            str += ((lua_toboolean(L, i) != 0) ? " Boolean<True> "
                                               : " Boolean<False> ");
            break;

        case LUA_TSTRING:
            str += " String<";
            str += lua_tostring(L, i);
            str += "> ";
            break;

        case LUA_TTABLE:
            str += " <Get LUA_TTABLE: **TODO**> ";
            break;

        case LUA_TFUNCTION:
            str += " <Get LUA_TFUNCTION: **TODO**> ";
            break;

        case LUA_TUSERDATA:
            str += " <Get LUA_TUSERDATA: **TODO**> ";
            break;

        case LUA_TTHREAD:
            str += " <Get LUA_TTHREAD: **TODO**> ";
            break;

        case LUA_TLIGHTUSERDATA:
            str += " <Get LUA_TLIGHTUSERDATA: **TODO**> ";
            break;

        default:
            str += " <unknow argment type> ";
            break;
        }
    }

    CLog::log() << str;

    return 0;
}

/*
 * New menu option
 */
auto CLuaFunctions::newMenuOption(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 8 && (lua_isstring(L, 1) != 0) &&
        (lua_isstring(L, 2) != 0) && (lua_isnumber(L, 3) != 0) &&
        (lua_isnumber(L, 4) != 0) && (lua_isnil(L, 5) || lua_isnumber(L, 5)) &&
        (lua_isnumber(L, 6) != 0) && (lua_isnumber(L, 7) != 0) &&
        lua_isfunction(L, 8)) {
        std::string menuName;
        int optID = 0;

        p >> menuName;

        auto *menu = GPPGame::GuitarPP().menus.findMenu(menuName);
        if (!menu || !validInteger(L, 6, -1, std::numeric_limits<int>::max()) ||
            !validInteger(L, 7, -1, CMenu::static_text) ||
            !std::isfinite(lua_tonumber(L, 3)) ||
            !std::isfinite(lua_tonumber(L, 4)) ||
            (!lua_isnil(L, 5) &&
             (!std::isfinite(lua_tonumber(L, 5)) || lua_tonumber(L, 5) <= 0))) {
            lua_pushboolean(L, false);
            return 1;
        }

        CMenu::menuOpt opt;
        opt.manualPosition = true;

        p >> opt.text;
        p >> opt.x;
        p >> opt.y;
        double size = 0;
        p >> size;
        if (!lua_isnil(L, 5)) {
            opt.styleOverrides.textSize = size;
        }
        p >> opt.group;
        p >> opt.type;

        if (opt.type == CMenu::menusOPT::none) {
            opt.type = CMenu::menusOPT::textbtn;
            opt.goback = true;
        }

        optID = menu->addOpt(opt);
        if (optID < 0) {
            lua_pushboolean(L, false);
            return 1;
        }
        p << optID;

        // ************************************************************** auto
        // callback system
        lua_pushvalue(L, 8);
        int fnRef = luaL_ref(L, LUA_REGISTRYINDEX);

        // event menuOptionName

        int id = CLuaH::Lua().idForCallbackEvent(
            GPPGame::GuitarPP().menus.getMenuByName(menuName).getNameFromOpt(
                optID));
        auto &script = CLuaH::Lua().getLuaStateScript(L);
        auto previous = script.callbacks.find(id);
        if (previous != script.callbacks.end()) {
            luaL_unref(L, LUA_REGISTRYINDEX, previous->second);
        }

        script.callbacks[id] = fnRef;
        script.callbacksAdded = true;
    }

    return p.rtn();
}

/*
 *
 */
auto CLuaFunctions::getMenuOptionName(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 2 && (lua_isstring(L, 1) != 0) &&
        (lua_isnumber(L, 2) != 0)) {
        std::string menuName;
        int optID = 0;

        p >> menuName;
        auto *menu = GPPGame::GuitarPP().menus.findMenu(menuName);
        if (!menu || !validMenuIndex(L, 2, *menu)) {
            lua_pushboolean(L, false);
            return 1;
        }

        p >> optID;
        p << menu->getNameFromOpt(static_cast<size_t>(optID));
    }

    return p.rtn();
}

/*
 *
 */
auto CLuaFunctions::getNumOfMenuOptions(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 1 && (lua_isstring(L, 1) != 0)) {
        std::string menuName;

        p >> menuName;

        auto *menu = GPPGame::GuitarPP().menus.findMenu(menuName);
        if (!menu) {
            lua_pushboolean(L, false);
            return 1;
        }

        p << menu->getNumOfOptions();
    }

    return p.rtn();
}

/*
 *
 */
auto CLuaFunctions::setMenuBackgroundTexture(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 3 && (lua_isstring(L, 1) != 0) &&
        (lua_isstring(L, 2) != 0) && (lua_isstring(L, 3) != 0)) {
        auto &game = GPPGame::GuitarPP();

        std::string menuName;
        std::string texturePath;
        std::string textureName;

        p >> menuName;
        p >> texturePath;
        p >> textureName;

        auto *menu = game.menus.findMenu(menuName);
        if (!menu) {
            lua_pushboolean(L, false);
            return 1;
        }

        menu->backgroundTexture =
            game.loadTexture(texturePath, textureName).getGTextureName();
    }

    return p.rtn();
}

/*
 *
 */
auto CLuaFunctions::getMainMenuName(lua_State *L) -> int {
    LuaParams p(L);

    CMenu *main = GPPGame::GuitarPP().menus.getMainMenu();

    if (main != nullptr) {
        p << main->getName();
    } else {
        p << false;
    }

    return p.rtn();
}

auto CLuaFunctions::setFontCharMetrics(lua_State *L) -> int {
    bool success = false;
    if (lua_gettop(L) == 4 && lua_type(L, 1) == LUA_TSTRING &&
        lua_type(L, 2) == LUA_TSTRING && lua_type(L, 3) == LUA_TNUMBER &&
        lua_type(L, 4) == LUA_TNUMBER) {
        success = CFonts::fonts().setFontCharMetrics(
            lua_tostring(L, 1), lua_tostring(L, 2), lua_tonumber(L, 3),
            lua_tonumber(L, 4));
    }

    lua_pushboolean(L, success);
    return 1;
}

auto CLuaFunctions::addTextureToFont(lua_State *L) -> int {
    const int count = lua_gettop(L);
    if ((count != 4 && count != 6) || lua_type(L, 1) != LUA_TSTRING ||
        lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING ||
        lua_type(L, 4) != LUA_TSTRING) {
        lua_pushboolean(L, false);
        return 1;
    }

    CFonts::AtlasGrid grid;
    if (count == 6) {
        if (!validInteger(L, 5, 1, 65535) || !validInteger(L, 6, 1, 65535)) {
            lua_pushboolean(L, false);
            return 1;
        }

        grid = {static_cast<int>(lua_tointeger(L, 5)),
                static_cast<int>(lua_tointeger(L, 6))};
    }

    const auto result = CFonts::fonts().addTextureToFont(
        lua_tostring(L, 1), lua_tostring(L, 2), lua_tostring(L, 3),
        lua_tostring(L, 4), grid);
    if (result.empty()) {
        lua_pushboolean(L, false);
    } else {
        lua_pushlstring(L, result.data(), result.size());
    }

    return 1;
}

/*
 *
 */
auto CLuaFunctions::getActualMenu(lua_State *L) -> int {
    LuaParams p(L);

    CMenu *menu = GPPGame::GuitarPP().menus.getActualMenu();

    if (menu != nullptr) {
        p << menu->getName();
    } else {
        p << true;
    }

    return p.rtn();
}

/*
 *
 */
auto CLuaFunctions::getGameCallback(lua_State *L) -> int {
    LuaParams p(L);

    if (p.getNumParams() == 1 && (lua_isstring(L, 1) != 0)) {
        std::string param = lua_tostring(L, 1);

        for (auto &s : GPPGame::GuitarPP().gameCallbacksWrapper) {
            if (s.second == param) {
                p << s.first;
                break;
            }
        }
    }

    return p.rtn();
}

void CLuaFunctions::registerFunctions(CLuaH::luaState_t &Lstate) {
    lua_State *L = Lstate.get();

    lua_register(L, "setConfigs", setConfigs);
    lua_register(L, "doNotRunAgain", doNotRunAgain);
    lua_register(L, "setCallBackToEvent", setCallBackToEvent);
    lua_register(L, "drawTextOnScreen", drawTextOnScreen);
    lua_register(L, "newMenuOption", newMenuOption);
    lua_register(L, "newMenu", newMenu);
    lua_register(L, "getMainMenuName", getMainMenuName);
    lua_register(L, "printTolog", printTolog);
    lua_register(L, "getMenuOptionName", getMenuOptionName);
    lua_register(L, "getNumOfMenuOptions", getNumOfMenuOptions);
    lua_register(L, "loadTexture", loadTexture);
    lua_register(L, "addTextureToFont", addTextureToFont);
    lua_register(L, "setFontCharMetrics", setFontCharMetrics);
    lua_register(L, "setFontCharSpacing", fontSpacing);
    lua_register(L, "setFontMetrics", fontMetrics);
    lua_register(L, "setMenuBinding", menuBinding);
    lua_register(L, "assingMenuToOtherMenuOption", assingMenuToOtherMenuOption);
    lua_register(L, "setMenuBackgroundTexture", setMenuBackgroundTexture);
    lua_register(L, "getActualMenu", getActualMenu);
    lua_register(L, "getGameCallback", getGameCallback);
    lua_register(L, "assingGameFunctionToMenuOption",
                 assingGameFunctionToMenuOption);
    lua_register(L, "setGameVar", setGameVar);

    lua_register(L, "loadSound", loadSound);
    lua_register(L, "loadSoundDecode", loadSoundDecode);
    lua_register(L, "playSound", playSound);
    lua_register(L, "stopSound", stopSound);
    lua_register(L, "setSoundTime", setSoundTime);
    lua_register(L, "setSoundVolume", setSoundVolume);
    lua_register(L, "releaseSound", releaseSound);
    lua_register(L, "setSoundFlags", setSoundFlags);
    lua_register(L, "getSoundTime", getSoundTime);
    lua_register(L, "getSoundVolume", getSoundVolume);
    lua_register(L, "setSoundAttribute", setSoundAttribute);
    lua_register(L, "getBassError", getBassError);
    lua_register(L, "printGlobals", printGlobals);

    auto &funList = LuaF().registerFunctionsAPICBs;

    for (auto &fun : funList) {
        fun(Lstate);
    }
}

#include <bass.h>
#include <bass_fx.h>

/*
 * Register default game globals
 */
void CLuaFunctions::registerGlobals(CLuaH::luaState_t &L) {
    setLuaGlobal(L, "TESTE", "abcde");
    setLuaGlobal(L, "VSYNC_OFF", 0);
    setLuaGlobal(L, "VSYNC_ON", 1);
    setLuaGlobal(L, "VSYNC_HALF", 2);
    setLuaGlobal(L, "VSYNC_AUTO", 3);

    setLuaGlobal(L, "BASS_SAMPLE_LOOP", BASS_SAMPLE_LOOP);
    setLuaGlobal(L, "BASS_STREAM_AUTOFREE", BASS_STREAM_AUTOFREE);
    setLuaGlobal(L, "BASS_STREAM_RESTRATE", BASS_STREAM_RESTRATE);
    setLuaGlobal(L, "BASS_MUSIC_NONINTER", BASS_MUSIC_NONINTER);
    setLuaGlobal(L, "BASS_MUSIC_SINCINTER", BASS_MUSIC_SINCINTER);
    setLuaGlobal(L, "BASS_MUSIC_SURROUND", BASS_MUSIC_SURROUND);
    setLuaGlobal(L, "BASS_MUSIC_SURROUND2", BASS_MUSIC_SURROUND2);
    setLuaGlobal(L, "BASS_MUSIC_FT2MOD", BASS_MUSIC_FT2MOD);
    setLuaGlobal(L, "BASS_MUSIC_PT1MOD", BASS_MUSIC_PT1MOD);
    setLuaGlobal(L, "BASS_MUSIC_POSRESET", BASS_MUSIC_POSRESET);
    setLuaGlobal(L, "BASS_MUSIC_POSRESETEX", BASS_MUSIC_POSRESETEX);
    setLuaGlobal(L, "BASS_MUSIC_STOPBACK", BASS_MUSIC_STOPBACK);

    setLuaGlobal(L, "BASS_ATTRIB_EAXMIX", BASS_ATTRIB_EAXMIX);
    setLuaGlobal(L, "BASS_ATTRIB_CPU", BASS_ATTRIB_CPU);
    setLuaGlobal(L, "BASS_ATTRIB_FREQ", BASS_ATTRIB_FREQ);
    setLuaGlobal(L, "BASS_ATTRIB_MUSIC_ACTIVE",
                 /*BASS_ATTRIB_MUSIC_ACTIVE*/ -1);
    setLuaGlobal(L, "BASS_ATTRIB_MUSIC_AMPLIFY", BASS_ATTRIB_MUSIC_AMPLIFY);
    setLuaGlobal(L, "BASS_ATTRIB_MUSIC_BPM", BASS_ATTRIB_MUSIC_BPM);
    setLuaGlobal(L, "BASS_ATTRIB_MUSIC_PANSEP", BASS_ATTRIB_MUSIC_PANSEP);
    setLuaGlobal(L, "BASS_ATTRIB_MUSIC_PSCALER", BASS_ATTRIB_MUSIC_PSCALER);
    setLuaGlobal(L, "BASS_ATTRIB_MUSIC_SPEED", BASS_ATTRIB_MUSIC_SPEED);
    setLuaGlobal(L, "BASS_ATTRIB_MUSIC_VOL_CHAN", BASS_ATTRIB_MUSIC_VOL_CHAN);
    setLuaGlobal(L, "BASS_ATTRIB_MUSIC_VOL_GLOBAL",
                 BASS_ATTRIB_MUSIC_VOL_GLOBAL);
    setLuaGlobal(L, "BASS_ATTRIB_MUSIC_VOL_INST", BASS_ATTRIB_MUSIC_VOL_INST);
    setLuaGlobal(L, "BASS_ATTRIB_NOBUFFER", BASS_ATTRIB_NOBUFFER);
    setLuaGlobal(L, "BASS_ATTRIB_PAN", BASS_ATTRIB_PAN);
    setLuaGlobal(L, "BASS_ATTRIB_SRC", BASS_ATTRIB_SRC);
    setLuaGlobal(L, "BASS_ATTRIB_VOL", BASS_ATTRIB_VOL);
    setLuaGlobal(L, "BASS_ATTRIB_TEMPO", BASS_ATTRIB_TEMPO);
    setLuaGlobal(L, "BASS_ATTRIB_TEMPO_PITCH", BASS_ATTRIB_TEMPO_PITCH);
    setLuaGlobal(L, "BASS_ATTRIB_TEMPO_FREQ", BASS_ATTRIB_TEMPO_FREQ);
    setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_USE_AA_FILTER",
                 BASS_ATTRIB_TEMPO_OPTION_USE_AA_FILTER);
    setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_AA_FILTER_LENGTH",
                 BASS_ATTRIB_TEMPO_OPTION_AA_FILTER_LENGTH);
    setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_USE_QUICKALGO",
                 BASS_ATTRIB_TEMPO_OPTION_USE_QUICKALGO);
    setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_SEQUENCE_MS",
                 BASS_ATTRIB_TEMPO_OPTION_SEQUENCE_MS);
    setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_USE_QUICKALGO",
                 BASS_ATTRIB_TEMPO_OPTION_USE_QUICKALGO);
    setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_SEEKWINDOW_MS",
                 BASS_ATTRIB_TEMPO_OPTION_SEEKWINDOW_MS);
    setLuaGlobal(L, "BASS_ATTRIB_TEMPO_OPTION_OVERLAP_MS",
                 BASS_ATTRIB_TEMPO_OPTION_OVERLAP_MS);
    setLuaGlobal(L, "BASS_ATTRIB_REVERSE_DIR", BASS_ATTRIB_REVERSE_DIR);

    auto &funList = LuaF().registerGlobalsAPICBs;

    for (auto &fun : funList) {
        fun(L);
    }
}

CLuaFunctions::CLuaFunctions() {}
