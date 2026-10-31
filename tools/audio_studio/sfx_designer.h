#pragma once
#include "session.h"

namespace AudioStudio {
class SfxDesigner {
    Session &s;
    std::vector<fs::path> files;
    char root[4096] = {}, openPath[4096] = {},
         savePath[4096] = "specs/new_sfx.lua";
    char filter[128] = {}, exportPath[1024] = "sfx.wav";
    size_t selectedLayer = 0;
    bool float32 = false, looping = false, focus = false;
    enum class Action { None, Load, New };
    Action pending = Action::None;
    fs::path pendingPath;
    char confirmSavePath[4096] = {};
    bool askToSave = false;
    std::string error;
    void request(Action action, const fs::path &path = {});
    void perform(Action action, const fs::path &path);
    void confirmChanges();
    void scan();
    void browser();
    void sound();
    void layers();
    void result();

    template <class F> void attempt(F fn) {
        try {
            fn();
            error.clear();
        } catch (const std::exception &e) {
            error = e.what();
        }
    }

  public:
    bool visible = false;
    explicit SfxDesigner(Session &session);
    void open();
    void openRecipe(const fs::path &path);
    void draw();
};
} // namespace AudioStudio
