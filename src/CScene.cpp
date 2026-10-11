#include "CScene.h"
#include "CBackgroundRenderer.h"
#include "CLog.h"
#include "GPPGame.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <limits>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {
// Definitions are plain tables. Looking up a field must never execute an
// unprotected __index metamethod outside the script/hook's lua_pcall.
void rawField(lua_State *L, int table, const char *key) {
    lua_pushstring(L, key);
    lua_rawget(L, table < 0 ? table - 1 : table);
}
} // namespace

CScene::Definition CScene::Definition::garage() {
    Definition result;
    result.id = "garage";
    result.model = "test/garage_gpp.obj";
    result.materials = "test/garage_gpp.mtl";
    result.garageMaterials = true;
    return result;
}

bool CScene::validId(const std::string &id) {
    return !id.empty() && std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

namespace {
struct LuaBudget {
    size_t used = 0, limit = 8 * 1024 * 1024;
};

void *limitedLuaAlloc(void *user, void *ptr, size_t oldSize, size_t size) {
    auto &budget = *static_cast<LuaBudget *>(user);
    if (!ptr) {
        oldSize = 0; // Lua passes a type tag for new allocations.
    }
    if (!size) {
        budget.used -= oldSize;
        std::free(ptr);
        return nullptr;
    }
    if (size > oldSize && size - oldSize > budget.limit - budget.used) {
        return nullptr;
    }
    void *next = std::realloc(ptr, size);
    if (next) {
        budget.used = budget.used - oldSize + size;
    }
    return next;
}

bool readDefinitionImpl(const std::string &file, const std::string &id,
                        CScene::Definition &output,
                        CLuaH::luaState_t &scriptOut, std::string &error,
                        bool inspection) {
    LuaBudget budget; // Must outlive the restricted state, including lua_close.

    try {
        if (!CScene::validId(id)) {
            throw std::runtime_error("Invalid scene identifier");
        }
        auto state =
            inspection
                ? CLuaH::luaState_t(lua_newstate(limitedLuaAlloc, &budget))
                : CLuaH::make_luaState();
        if (!state) { throw std::runtime_error("Cannot allocate scene Lua state"); }
        auto *L = state.get();
        if (!inspection) {
            luaL_openlibs(L);
        } else {
            lua_sethook(
                L,
                [](lua_State *vm, lua_Debug *) {
                    luaL_error(vm,
                               "Scene inspection instruction limit exceeded");
                },
                LUA_MASKCOUNT, 100000);
        }
        const int status =
            luaL_loadfilex(L, file.c_str(), inspection ? "t" : nullptr);
        const int executed = status ? status : lua_pcall(L, 0, 1, 0);
        // Parsing below performs only raw table lookups, with no script calls.
        // Allow allocation of diagnostic keys outside Lua's protected call.
        budget.limit = std::numeric_limits<size_t>::max();
        if (executed) {
            const char *message = lua_tostring(L, -1);
            throw std::runtime_error(message ? message : "Scene script failed");
        }
        if (!lua_istable(L, -1)) { throw std::runtime_error("Scene must return a table"); }
        CScene::Definition candidate;
        candidate.id = id;
        auto pathField = [&](const char *key, bool required) {
            rawField(L, -1, key);
            std::string value;
            if (!lua_isnil(L, -1)) {
                if (lua_type(L, -1) != LUA_TSTRING) {
                    throw std::runtime_error(std::string(key) + " must be a path string");
                }
                value = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
            if (required && value.empty()) {
                throw std::runtime_error(std::string("Missing scene ") + key);
            }
            return value.empty() ? value :
                (fs::path(file).parent_path() / value).lexically_normal().string();
        };
        candidate.model = pathField("model", true);
        candidate.materials = pathField("materials", false);
        rawField(L, -1, "garageMaterials");
        if (!lua_isnil(L, -1) && !lua_isboolean(L, -1)) {
            throw std::runtime_error("garageMaterials must be boolean");
        }
        candidate.garageMaterials = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        rawField(L, -1, "lightPosition");
        if (!lua_isnil(L, -1)) {
            if (!lua_istable(L, -1)) { throw std::runtime_error("lightPosition must be a table"); }
            for (int i = 0; i < 3; ++i) {
                lua_rawgeti(L, -1, i + 1);
                if (lua_type(L, -1) != LUA_TNUMBER) {
                    throw std::runtime_error("lightPosition requires three numbers");
                }
                const double value = lua_tonumber(L, -1);
                if (!std::isfinite(value) || std::abs(value) > 1e6) {
                    throw std::runtime_error("Invalid lightPosition");
                }
                candidate.light[static_cast<size_t>(i)] = static_cast<float>(value);
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
        for (const char *hook : {"onLoaded", "onSessionStarted", "onSessionEnded", "onUnloaded", "onSongStarted", "onSongEnded"}) {
            rawField(L, -1, hook);
            if (!lua_isnil(L, -1) && !lua_isfunction(L, -1)) {
                throw std::runtime_error(std::string(hook) + " must be a function");
            }
            lua_pop(L, 1);
        }
        // Keep the returned table rooted at stack index 1 for lifecycle hooks.
        output = std::move(candidate);
        if (!inspection) {
            scriptOut = std::move(state);
        }
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = file + ": " + e.what();
        return false;
    }
}

} // namespace

bool CScene::readDefinition(const std::string &file, const std::string &id,
                            Definition &output, CLuaH::luaState_t &script,
                            std::string &error) {
    return readDefinitionImpl(file, id, output, script, error, false);
}

bool CScene::readInspectionDefinition(const std::string &file,
                                      const std::string &id, Definition &output,
                                      std::string &error) {
    CLuaH::luaState_t unused;
    return readDefinitionImpl(file, id, output, unused, error, true);
}

bool CScene::load(GPPOBJ &model, const std::string &id,
                  const std::function<bool()> &loadingFrame) {
    lastError.clear();
    if (sessionActive) { lastError = "Cannot replace a scene during gameplay"; return false; }
    if (!validId(id)) { lastError = "Invalid scene identifier"; return false; }
    if (current.id == id) { return true; }
    Definition definition;
    CLuaH::luaState_t newScript;
    try {
        const auto file = fs::path("data/scenes") / (id + ".lua");
        if (id == "garage" && !fs::exists(file)) {
            definition = Definition::garage();
        } else if (!readDefinition(file.string(), id, definition, newScript, lastError)) {
            return false;
        }
        if (!fs::is_regular_file(definition.model) ||
            (!definition.materials.empty() && !fs::is_regular_file(definition.materials))) {
            lastError = "Scene model or materials missing: " + id;
            return false;
        }
        GPPOBJ candidate;
        candidate.setMaterialLibrary(definition.materials);
        auto &loader = GPPGame::GuitarPP().modelLoader();
        const fs::path path(definition.model);
        auto handle = GPPGame::GuitarPP().requestModel(
            path.parent_path().string(), path.filename().string(), candidate);
        if (!handle) {
            lastError = "Cannot enqueue scene: " + id;
            return false;
        }
        struct CancelPending {
            CModelManager &loader;
            CModelManager::Handle handle;
            ~CancelPending() { loader.cancel(handle); }
        } cancel{loader, handle};
        // Check cancellation even when the CPU job completed immediately.
        do {
            if (loadingFrame && !loadingFrame()) {
                lastError = "Scene loading cancelled";
                return false;
            }
            if (loadingFrame) {
                GPPGame::GuitarPP().textureStreammingProcess();
            }
            if (loader.finish(handle, !loadingFrame)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (true);
        const auto result = handle->snapshot();
        if (result.state != CModelManager::State::Ready) {
            lastError = "Cannot load scene " + id + ": " + result.error;
            return false;
        }
        event("onUnloaded", 0.0);
        model = std::move(candidate); // Old GL objects retired on the context owner.
        current = std::move(definition);
        script = std::move(newScript);
        CBackgroundRenderer::inst().setScene(current.light, current.garageMaterials);
        event("onLoaded", 0.0);
        return true;
    } catch (const std::exception &e) {
        lastError = std::string("Scene loading failed: ") + e.what();
        return false;
    }
}

void CScene::event(const char *name, double songSeconds,
                   const std::string *song, const SongResult *result) {
    if (!script) { return; }
    auto *L = script.get();
    rawField(L, 1, name);
    if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
    lua_newtable(L);
    lua_pushstring(L, current.id.c_str()); lua_setfield(L, -2, "sceneId");
    lua_pushstring(L, (song ? *song : sessionSong).c_str()); lua_setfield(L, -2, "songId");
    lua_pushnumber(L, songSeconds); lua_setfield(L, -2, "songSeconds");
    if (result) {
        lua_pushstring(L, songEndReasonName(result->reason)); lua_setfield(L, -2, "reason");
        lua_newtable(L);
        for (size_t i = 0; i < result->players.size(); ++i) {
            const auto &p = result->players[i];
            lua_newtable(L);
            lua_pushnumber(L, static_cast<lua_Number>(p.score)); lua_setfield(L, -2, "score");
            lua_pushnumber(L, static_cast<lua_Number>(p.maxCombo)); lua_setfield(L, -2, "maxCombo");
            lua_pushnumber(L, static_cast<lua_Number>(p.correctNotes)); lua_setfield(L, -2, "correctNotes");
            lua_pushnumber(L, static_cast<lua_Number>(p.totalNotes)); lua_setfield(L, -2, "totalNotes");
            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }
        lua_setfield(L, -2, "players");
    }
    if (lua_pcall(L, 1, 0, 0)) {
        const char *message = lua_tostring(L, -1);
        CLog::log() << std::string("Scene ") + current.id + " " + name + ": " +
            (message ? message : "Lua callback failed");
        lua_pop(L, 1);
    }
}
void CScene::sessionStarted(const std::string &song, double songSeconds) {
    if (sessionActive || !loaded()) { return; }
    sessionActive = true;
    sessionSong = song;
    event("onSessionStarted", songSeconds);
}
void CScene::sessionEnded(double songSeconds) {
    if (!sessionActive) { return; }
    if (!activeSong.empty()) {
        SongResult aborted;
        aborted.songId = activeSong;
        aborted.reason = SongEndReason::Aborted;
        aborted.elapsedSeconds = std::max(0.0, songSeconds);
        songEnded(aborted);
    }
    event("onSessionEnded", songSeconds);
    sessionActive = false;
    sessionSong.clear();
}
void CScene::songStarted(const std::string &song, double songSeconds) {
    if (!sessionActive || !activeSong.empty()) { return; }
    activeSong = song;
    event("onSongStarted", songSeconds, &activeSong);
}
void CScene::songEnded(const SongResult &result) {
    if (activeSong.empty()) {
        if (result.reason != SongEndReason::LoadFailed) { return; }
    } else if (activeSong != result.songId) { return; }
    event("onSongEnded", result.elapsedSeconds, &result.songId, &result);
    activeSong.clear();
}
void CScene::unload(GPPOBJ &model) {
    sessionEnded(0.0);
    event("onUnloaded", 0.0);
    script.reset();
    current = Definition{};
    model.unload();
}
