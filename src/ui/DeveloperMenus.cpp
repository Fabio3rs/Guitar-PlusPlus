#include "CBackgroundRenderer.h"
#include "CChart.h"
#include "CLog.h"
#include "CMenuSystem.h"
#include "GPPGame.h"
#include <chrono>

void CMenuSystem::initialize() {
    auto &lua = CLuaH::Lua();
    menusGoBackSE = lua.idForCallbackEvent("menusGoBack");
    menusCMMenuSE = lua.idForCallbackEvent("menusCustomMultiMenu");
    menusNextSE = lua.idForCallbackEvent("menusNext");
    menusGameCbNextSE = lua.idForCallbackEvent("menusGameCallbackNext");
    devMenus.gameMenu = true;
    modals.setPresentation([this](CMenu &menu) { preparePresentation(menu); });
    modals.setTextures([this](const std::string &file) {
        const std::filesystem::path path(file);
        return host
            .loadTexture(path.parent_path().string(), path.filename().string())
            .getTextId();
    });

    {
        CMenu::menuOpt opt;
        opt.optionName = "rename";

        opt.text = "Renomear";

        opt.y = 0.8;
        opt.x = -1.0;
        opt.group = 1;
        opt.status = 0;
        opt.type = CMenu::menusOPT::textbtn;

        auto devMenuRename = [this](const std::string &) {
            if ((devMenus.devEditMenu != nullptr) &&
                devMenus.getUIListSize() == 0) {
                size_t op = devMenus.devEditMenu->getDevSelectedMenuOpt();

                if (op != ~static_cast<size_t>(0UL)) {
                    devMenus.devEditingOpt = op;

                    modals.openRename(devMenus,
                                      devMenus.devEditMenu->options[op].text);

                    devMenus.devMenuNOUpdateOthers = true;
                }
            }
        };

        opt.menusXRef.push_back(
            host.addGameCallbacks("devMenuRename", devMenuRename));

        devMenus.addOpt(opt);
    }
}
