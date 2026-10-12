#include "CModalManager.h"
#include <algorithm>
#include <stdexcept>

namespace {
size_t field(const ui::MenuDefinition &def, const std::string &id) {
    for (size_t i = 0; i < def.options.size(); ++i) {
        if (def.options[i].id == id) {
            return i;
        }
    }
    throw std::runtime_error("Missing modal field: " + id);
}
} // namespace

const ui::MenuDefinition &
CModalManager::definition(const std::string &id) const {
    for (const auto &def : definitions) {
        if (def.id == id) {
            return def;
        }
    }
    throw std::runtime_error("Unknown modal: " + id);
}

void CModalManager::replace(const ui::MenuDefinitions &input) {
    ui::validate(input);
    auto next = ui::defaultModals();
    for (auto &modal : next) {
        for (const auto &candidate : input) {
            if (modal.id == candidate.id) {
                modal = candidate;
            }
        }
    }
    definitions = std::move(next);
}

bool CModalManager::load(const std::filesystem::path &path,
                         std::string &error) {
    try {
        auto next = ui::readMenus(path);
        replace(next);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}

CMenu &CModalManager::open(CMenu &owner, const std::string &id) {
    auto model = ui::instantiate(definition(id), context, textures);
    if (presentation) {
        presentation(*model);
    }
    const int slot = owner.pushUserInterface(*model);
    return *CMenu::getUiAt(static_cast<size_t>(slot)).m;
}

CMenu &CModalManager::openRename(CMenu &owner, std::string &text) {
    auto &menu = open(owner, "uiRenameMenu");
    auto &option = menu.options[field(definition("uiRenameMenu"), "name")];
    option.preText = text;
    option.externalPreTextRef = &text;
    return menu;
}

CMenu &CModalManager::openCreateProfile(CMenu &owner) {
    auto &menu = open(owner, "uiCreateProfileMenu");
    const auto &def = definition("uiCreateProfileMenu");
    const size_t pathIndex = field(def, "path");
    menu.options[field(def, "name")].posUpdateCppCallback =
        [pathIndex](CMenu &instance, CMenu::menuOpt &option) {
            instance.options[pathIndex].text = "Path: /" + option.preText;
            return 0;
        };
    return menu;
}
