#include "CLog.h"
#include "CMenuSystem.h"
#include <set>

void CMenuSystem::preparePresentation(CMenu &menu) {
    const auto found = presentation.find(menu.getName());
    menu.applyPresentationOverrides(
        found == presentation.end() ? ui::MenuOverrides{} : found->second);
}

void CMenuSystem::replacePresentation(const ui::MenuOverrideSet &next) {
    ui::validateOverrides(next);
    presentation = next;
    std::set<CMenu *> visited;
    auto refresh = [this, &visited](CMenu &menu) {
        if (menu.originalPresentation() && visited.insert(&menu).second) {
            preparePresentation(menu);
        }
    };
    for (auto &entry : gameMenus) {
        refresh(entry.second);
        entry.second.visitInterfaces(refresh);
    }
    refresh(devMenus);
    devMenus.visitInterfaces(refresh);
    if (currentMenu) {
        refresh(*currentMenu);
        currentMenu->visitInterfaces(refresh);
    }
}

void CMenuSystem::loadThemePresentation(
    const std::filesystem::path &directory) {
    // A new theme always starts from defaults, even if a document is invalid.
    modals.replace(ui::defaultModals());
    const auto definitions = directory / "Gui.lua";
    std::error_code error;
    if (std::filesystem::exists(definitions, error)) {
        std::string message;
        if (!modals.load(definitions, message)) {
            CLog::log() << "GUI theme: " + message;
        }
    }
    ui::MenuOverrideSet next;
    const auto overrides = directory / "MenuOverrides.lua";
    try {
        if (std::filesystem::exists(overrides)) {
            next = ui::readOverrides(overrides);
        }
    } catch (const std::exception &e) {
        CLog::log() << "GUI presentation (" + overrides.string() +
                           "): " + e.what();
    }
    replacePresentation(next);
}
