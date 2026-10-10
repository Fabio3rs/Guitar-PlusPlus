#pragma once

#include "CLuaH.hpp"
#include "GPPOBJ.h"
#include "SongResult.h"
#include <array>
#include <functional>
#include <string>

// Render-thread owned scene state. No gameplay worker reads or writes it.
// Definitions use paths relative to their Lua file; MTL textures remain relative
// to the MTL. The model can therefore live separately under data/models.
class CScene {
  public:
    struct Definition {
        std::string id, model, materials;
        std::array<float, 3> light{3.5f, 2.88f, -6.05f};
        bool garageMaterials = false;
        static Definition garage();
    };

    static bool validId(const std::string &id);
    // Leaves output/script unchanged on failure. Lua must return a table.
    static bool readDefinition(const std::string &file, const std::string &id,
                               Definition &output, CLuaH::luaState_t &script,
                               std::string &error);
    // Call only during loading, on the GL owner. The optional callback draws
    // loading frames and returns false on cancellation. Existing scene survives
    // failed loads. CPU decoding uses a worker; GL upload/retirement stay here.
    bool load(GPPOBJ &model, const std::string &id,
              const std::function<bool()> &loadingFrame = {});
    void sessionStarted(const std::string &song, double songSeconds);
    void sessionEnded(double songSeconds);
    void songStarted(const std::string &song, double songSeconds);
    void songEnded(const SongResult &result);
    void unload(GPPOBJ &model);
    const Definition &definition() const { return current; }
    bool loaded() const { return !current.id.empty(); }
    const std::string &error() const { return lastError; }

  private:
    Definition current;
    CLuaH::luaState_t script;
    std::string lastError, sessionSong, activeSong;
    bool sessionActive = false;
    void event(const char *name, double songSeconds,
               const std::string *song = nullptr, const SongResult *result = nullptr);
};
