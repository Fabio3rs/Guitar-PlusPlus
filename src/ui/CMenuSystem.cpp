#include "CMenuSystem.h"
#include "CBackgroundRenderer.h"
#include "CChart.h"
#include "CLog.h"
#include "GPPGame.h"
#include <chrono>

auto CMenuSystem::getMenuByName(const std::string &name) -> CMenu & {
    return gameMenus.at(name);
}

CMenu *CMenuSystem::findMenu(const std::string &name) {
    auto found = gameMenus.find(name);
    return found == gameMenus.end() ? nullptr : &found->second;
}

void CMenuSystem::eraseGameMenusAutoCreateds() {
    for (auto it = gameMenus.begin(); it != gameMenus.end();) {
        if ((*it).second.gameMenu) {
            ++it;
        } else {
            it = gameMenus.erase(it);
        }
    }
}

auto CMenuSystem::newMenu() -> CMenu & {
    CMenu nameSource;
    return newNamedMenu(nameSource.getName());
}

auto CMenuSystem::newNamedMenu(const std::string &name) -> CMenu & {
    auto result = gameMenus.try_emplace(name, name);
    result.first->second.gameMenu = true;
    return result.first->second;
}

void CMenuSystem::setMainMenu(CMenu &m) { mainMenu = &m; }

auto CMenuSystem::getMainMenu() -> CMenu * { return mainMenu; }

auto CMenuSystem::openMenus(CMenu *startMenu,
                            const std::function<int(void)> &preFun,
                            const std::function<int(void)> &midFun,
                            const std::function<int(void)> &posFun, bool dev,
                            std::vector<CMenu *> stackTest)
    -> std::vector<CMenu *> {
    CMenu *previousMenu = currentMenu;
    auto &menuControls = CControls::controls();
    const bool previousInput = menuControls.menuInputActive;
    menuControls.menuInputActive = true;
    menuControls.resetMenuInput();
    callOnDctor<void(void)> exitguard(
        [this, previousMenu, previousInput, &menuControls]() {
            currentMenu = previousMenu;
            menuControls.menuInputActive = previousInput;
            menuControls.resetMenuInput();
        });
    if (!startMenu && stackTest.empty()) {
        return {};
    }

    auto &engine = CEngine::engine();
    auto &lua = CLuaH::Lua();
    double waitForTime = 0.0;

    std::vector<CMenu *> menusStack = std::move(stackTest);

    if (menusStack.empty()) {
        menusStack.push_back(startMenu);

        currentMenu = startMenu;
    } else {
        currentMenu = menusStack.back();
    }

    auto create_menu = [&](const std::deque<std::string> &menusXRef) {
        auto &menu = newMenu();

        menu.temp = true;

        int optn = 0;

        for (const auto &m : menusXRef) {
            CMenu::menuOpt opt;

            opt.text = m;
            opt.y = 0.5 - optn * 0.2;
            opt.x = 0.15;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;
            opt.menusXRef.push_back(m);

            menu.addOpt(opt);

            ++optn;
        }

        CMenu::menuOpt opt;

        opt.text = "Back/voltar";
        opt.y = -0.75;
        opt.x = 0.5;
        opt.group = 1;
        opt.status = 0;
        opt.type = CMenu::menusOPT::textbtn;
        opt.goback = true;

        menu.addOpt(opt);

        preparePresentation(menu);
        return &menu;
    };

    for (auto *menu : menusStack) {
        if (!menu->originalPresentation()) {
            preparePresentation(*menu);
        }
    }
    if (dev && !devMenus.originalPresentation()) {
        preparePresentation(devMenus);
    }

    CEngine::RenderDoubleStruct RenderData{};

    RenderData.y1 = 1.0;
    RenderData.y2 = 1.0;
    RenderData.y3 = -1.0;
    RenderData.y4 = -1.0;

    RenderData.z1 = 0.0;
    RenderData.z2 = 0.0;
    RenderData.z3 = 0.0;
    RenderData.z4 = 0.0;

    RenderData.TextureX1 = 0.0;
    RenderData.TextureX2 = 1.0;
    RenderData.TextureY1 = 1.0;
    RenderData.TextureY2 = 0.0;

    bool back = false;
    bool updateRender = true;

    bool menuExit = false;

    bool waitReleaseKeys = false;

    while (!menusStack.empty() && engine.windowOpened()) {
        host.clearScreen();

        if (menuExit) {
            currentMenu = nullptr;
            return menusStack;
        }

        if (waitReleaseKeys) {
            if ((CEngine::engine().getMouseButton(0) == 0) &&
                (CEngine::engine().getKey(GLFW_KEY_ENTER) == 0) &&
                (CEngine::engine().getKey(GLFW_KEY_ESCAPE) == 0)) {
                waitReleaseKeys = false;
            }
        }

        updateRender =
            ((engine.getTime() - waitForTime) >= 0.0) && (!waitReleaseKeys);

        engine.activate3DRender(false);

        auto &menu = *menusStack.back();
        currentMenu = &menu;

        if (dev) {
            devMenus.devEditMenu = &menu;
        }

        if (preFun) {
            int preFunResult = preFun();

            switch (preFunResult) {
            case 0:
                break;

            case 1:
                menuExit = true;
                break;

            default:
                break;
            }
        }

        if (dev) {
            devMenus.update();

            for (auto &opt : devMenus.options) {
                if ((opt.status & 3) == 3) {
                    if (!opt.menusXRef.empty()) {

                        {
                            auto function = host.getCallback(opt.menusXRef[0]);
                            if (function) {
                                function(opt.menusXRef[0]);
                            }
                        }
                    }
                }
            }
        }

        drawBackground();

        engine.matrixReset();

        {
            CEngine::cameraSET usingCamera;
            usingCamera.eye.x = 0.0;
            usingCamera.eye.y = 0.0;
            usingCamera.eye.z = 2.3;
            usingCamera.center.x = 0;
            usingCamera.center.y = 0;
            usingCamera.center.z = 0.0;
            usingCamera.up.x = 0;
            usingCamera.up.y = 1;
            usingCamera.up.z = 0;

            engine.setCamera(usingCamera);
        }

        if (updateRender) {
            if (dev) {
                menu.updateDev();
            } else {
                {
                    menu.update();
                }
            }
        }

        const auto texture = host.getTextureInfo(menu.backgroundTexture);

        RenderData.Text = texture.id;
        if (RenderData.Text != 0u && texture.height > 0) {
            double prop = (double)texture.width / (double)texture.height;

            RenderData.x1 = -prop;
            RenderData.x2 = prop;
            RenderData.x3 = prop;
            RenderData.x4 = -prop;

            engine.Render2DQuad(RenderData);
        }

        if (midFun) {
            int midFunResult = midFun();

            switch (midFunResult) {
            case 0:
                break;

            case 1:
                menuExit = true;
                break;

            default:
                break;
            }
        }

        menu.render();

        if (dev) {
            devMenus.render();
        }

        CMenu::renderUiList();

        for (auto &opt : menu.options) {
            if ((opt.status & 3) == 3) {
                if (opt.goback) {
                    menu.closeInterfaces();

                    if (menu.temp) {

                        currentMenu = nullptr;
                        devMenus.devEditMenu = nullptr;
                        devMenus.closeInterfaces();
                        const std::string erasedName = menu.getName();
                        gameMenus.erase(erasedName);
                    }

                    menusStack.pop_back();
                    currentMenu =
                        menusStack.empty() ? nullptr : menusStack.back();

                    if (!menusStack.empty() != 0u) {
                        CMenu *mback = menusStack.back();

                        if (mback != nullptr) {
                            {
                                mback->resetData();
                            }
                        }
                    }

                    waitForTime = engine.getTime() + 0.5;

                    lua.runEvent(menusGoBackSE);
                    break;
                }

                if (opt.menusXRef.size() > 1) {
                    menu.closeInterfaces();
                    menusStack.push_back(create_menu(opt.menusXRef));
                    lua.runEvent(menusCMMenuSE);
                    lua.runEvent(menusNextSE);
                    waitForTime = engine.getTime() + 0.5;
                    waitReleaseKeys = true;
                    break;
                }

                if (opt.menusXRef.size() == 1) {
                    menu.closeInterfaces();
                    const std::string target = opt.menusXRef.front();
                    if (auto *next = findMenu(target)) {
                        menusStack.push_back(next);
                        currentMenu = next;
                        if (next->openCallback) {
                            next->openCallback(*next);
                        }

                        if (!next->originalPresentation()) {
                            preparePresentation(*next);
                        }
                        next->resetBtns();
                        next->resetData();
                        waitReleaseKeys = true;
                        lua.runEvent(menusNextSE);
                    } else if (auto function = host.getCallback(target)) {
                        lua.runEvent(menusGameCbNextSE);
                        function(target);
                        currentMenu =
                            menusStack.empty() ? nullptr : menusStack.back();
                        if (currentMenu) {
                            currentMenu->resetBtns();
                            currentMenu->resetData();
                        }

                        menuControls.resetMenuInput();
                    } else {
                        CLog::log() << "Unknown menu/callback: " + target;
                        opt.status &= ~2;
                    }

                    break;
                }
            }
        }

        currentMenu = menusStack.empty() ? nullptr : menusStack.back();

        if (posFun) {
            int posFunResult = posFun();

            switch (posFunResult) {
            case 0:
                break;

            case 1:
                menuExit = true;
                break;

            default:
                break;
            }
        }

        CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);

        host.renderFrame();
    }

    menusStack.clear();

    currentMenu = nullptr;

    return std::vector<CMenu *>();
}
