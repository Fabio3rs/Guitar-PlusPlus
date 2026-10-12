#pragma once
#include "CUIDefinition.h"

// UI-thread service. CMenu continues to own the generic interface stack.
class CModalManager {
    ui::MenuDefinitions definitions = ui::defaultModals();
    ui::TextureResolver textures;
    CMenu::Context *context = nullptr;
    const ui::MenuDefinition &definition(const std::string &) const;

  public:
    explicit CModalManager(CMenu::Context *host = nullptr) : context(host) {}
    void setTextures(ui::TextureResolver resolver) {
        textures = std::move(resolver);
    }
    const ui::MenuDefinitions &inspect() const { return definitions; }
    void replace(const ui::MenuDefinitions &);
    bool load(const std::filesystem::path &, std::string &error);
    CMenu &open(CMenu &owner, const std::string &id);
    CMenu &openRename(CMenu &owner, std::string &text);
    CMenu &openCreateProfile(CMenu &owner);
};
