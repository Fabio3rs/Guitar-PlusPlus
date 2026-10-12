#include "CBackgroundRenderer.h"
#include "CChart.h"
#include "CLog.h"
#include "CMenuSystem.h"
#include "GPPGame.h"
#include <chrono>

void CMenuSystem::continueCampaing(const std::string &name) {
    auto createMMenu = [this]() {
        auto &cntCampaing = newNamedMenu("continueCampaingDemo");
        cntCampaing.clearOptions();

        {
            CMenu::menuOpt opt;
            opt.optionName = "demo";

            opt.text = "Patos voadores";
            opt.y = 0.3;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            cntCampaing.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "back";

            opt.text = "Voltar";
            opt.y = 0.0;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;
            opt.goback = true;

            cntCampaing.addOpt(opt);
        }

        return &cntCampaing;
    };

    auto &continueMenu = *createMMenu();

    auto preFun = []() { return 0; };

    auto midFun = []() {
        CFonts::fonts().drawTextInScreenWithBuffer("Seu status", 0.7, 0.5, 0.1);

        return 0;
    };

    auto posFun = []() { return 0; };

    if (CEngine::engine().windowOpened()) {
        host.clearScreen();

        host.renderFrame();
    }

    openMenus(&continueMenu, preFun, midFun, posFun);
}

#include "CCampaing.h"
#include <random>

auto CCampaing::campaingMainMenu(CMenu &menu) -> int {
    auto &game = GPPGame::GuitarPP();

    menu.openCallback = openCampaingMenuCallback;

    {
        CMenu::menuOpt opt;
        opt.optionName = "new";

        opt.text = "Nova campanha";
        opt.y = 0.4;
        opt.centerX = -0.5;
        opt.group = 1;
        opt.status = 0;
        opt.type = CMenu::menusOPT::textbtn;

        menu.addOpt(opt);
    }

    {
        CMenu::menuOpt opt;
        opt.optionName = "continue";

        opt.text = "Continuar campanha";
        opt.y = 0.3;
        opt.centerX = -0.5;
        opt.group = 1;
        opt.status = 0;
        opt.type = CMenu::menusOPT::textbtn;

        opt.menusXRef.push_back("continueCampaingOptions");
        opt.updateCppCallback = [](CMenu &, CMenu::menuOpt &option) {
            option.enableEnter = campaingMGR().numCampaingSaves > 0;
            return 0;
        };

        menu.addOpt(opt);
    }

    {
        CMenu::menuOpt opt;
        opt.optionName = "back";

        opt.text = "Voltar";
        opt.y = -0.2;
        opt.centerX = -0.5;
        opt.group = 1;
        opt.status = 0;
        opt.type = CMenu::menusOPT::textbtn;
        opt.goback = true;

        menu.addOpt(opt);
    }

    return 0;
}

auto CCampaing::campaingDrawScreen() -> int {
    auto &mgr = campaingMGR();
    auto &engine = CEngine::engine();
    auto &GuitarPP = GPPGame::GuitarPP();

    callOnDctor<void(void)> exitguard([]() {
        auto &mgr = campaingMGR();

        mgr.campaingFunctionGotoID = 0;
        mgr.continueInDrawScreen = 0;
    });

    std::vector<CMenu *> menusStack;

    auto preFun = [&]() {
        CLuaH::Lua().runEventFromContainer(campaingMenuPreFunSE,
                                           mgr.campaingScripts);
        return 0;
    };

    auto midFun = [&]() {
        CLuaH::Lua().runEventFromContainer(campaingMenuMidFunSE,
                                           mgr.campaingScripts);
        return 0;
    };

    auto posFun = [&]() {
        CLuaH::Lua().runEventFromContainer(campaingMenuPosFunSE,
                                           mgr.campaingScripts);
        return 0;
    };

    while (engine.windowOpened() && mgr.continueInDrawScreen) {
        GuitarPP.clearScreen();

        if (engine.getKey(GLFW_KEY_ESCAPE) != 0) {
            break;
        }

        CLuaH::Lua().runEventFromContainer(campaingDScreenUpdateSE,
                                           mgr.campaingScripts);

        GuitarPP.renderFrame();

        switch (mgr.campaingFunctionGotoID) {
        case 0:
            break;

        case 1:
            menusStack = GuitarPP.menus.openMenus(
                &GuitarPP.menus.newNamedMenu("campaingMenu"), preFun, midFun,
                posFun, false, std::move(menusStack));

            if (!mgr.keepMenuStack) {
                menusStack.clear();
            }
            break;

        default:
            break;
        }
    }

    return 0;
}

auto CCampaing::openCampaingMenuCallback(CMenu &menu) -> int {
    campaingMGR().numCampaingSaves = listCampaingSaves().size();

    return 0;
}

void CCampaing::buildMenus() {
    auto &contOptions =
        GPPGame::GuitarPP().menus.newNamedMenu("continueCampaingOptions");

    contOptions.openCallback = [](CMenu &m) {
        m.resetData();
        m.resetBtns();

        CLuaH::Lua().runEventWithParamsFromContainer(
            campaingMainMenuOpenSE, {CLuaH::customParam((&m) != nullptr)},
            campaingMGR().campaingScripts);
        return 0;
    };

    {
        {
            CMenu::menuOpt opt;
            opt.optionName = "title";

            opt.text = "Modo campanha";
            opt.y = 0.8;
            opt.styleOverrides.textSize = 0.1;
            opt.centerX = 0.0;

            opt.status = 0;
            opt.type = CMenu::menusOPT::static_text;

            contOptions.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "play";

            opt.text = "Jogar";
            opt.y = 0.4;
            opt.centerX = -0.5;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            std::string testecallback = GPPGame::GuitarPP().addGameCallbacks(
                "campaingGameplayLoop", campaingPlayLoop);
            opt.menusXRef.push_back(testecallback);

            contOptions.addOpt(opt);
        }

        double infoPos = 0.0;

        {
            CMenu::menuOpt opt;
            opt.optionName = "info";

            opt.text = "Informa\xc3\xa7ões:";
            opt.y = 0.3;
            opt.x = infoPos = CFonts::fonts().getCenterPos(
                opt.text, contOptions.resolvedOptionStyle(opt).textSize, -0.3);

            opt.status = 0;
            opt.type = CMenu::menusOPT::static_text;

            contOptions.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "venue";

            opt.text = "* Show em um lugar público xyz";
            opt.y = 0.2;
            opt.x = infoPos;

            opt.status = 0;
            opt.type = CMenu::menusOPT::static_text;

            contOptions.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "multipleSongs";

            opt.text = "* Várias músicas";
            opt.y = 0.11;
            opt.x = infoPos;

            opt.status = 0;
            opt.type = CMenu::menusOPT::static_text;

            contOptions.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "randomSongs";

            opt.text = "* Possibilidade de música aleatoria";
            opt.y = 0.02;
            opt.x = infoPos;

            opt.status = 0;
            opt.type = CMenu::menusOPT::static_text;

            contOptions.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "reputation";

            opt.text = "* Reputação máxima prevista 10000";
            opt.y = -0.07;
            opt.x = infoPos;

            opt.status = 0;
            opt.type = CMenu::menusOPT::static_text;

            contOptions.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "songList";

            opt.text = "* Musicas previstas:";
            opt.y = -0.16;
            opt.x = infoPos;

            opt.status = 0;
            opt.type = CMenu::menusOPT::static_text;

            contOptions.addOpt(opt);
        }

        {
            auto songs = GPPGame::getDirectory("./data/songs", false, true);

            std::shuffle(songs.begin(), songs.end(),
                         std::mt19937(std::random_device()()));

            const unsigned int songsSize = 2 + rand() % 2;
            const unsigned int size =
                (songs.size() > songsSize) ? songsSize : songs.size();

            if (size < songs.size()) {
                songs.resize(size);
            }

            for (size_t i = 0; i < size; i++) {
                CMenu::menuOpt opt;
                opt.optionName = "song:" + songs[i];

                opt.text = songs[i];
                opt.y = -0.25 - static_cast<double>(i) * 0.08;
                opt.x = infoPos + 0.075 * 2.0;

                opt.status = 0;
                opt.type = CMenu::menusOPT::static_text;

                contOptions.addOpt(opt);
            }

            playSongsList = std::move(songs);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "back";

            opt.text = "Voltar";
            opt.y = -0.8;
            opt.centerX = -0.5;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;
            opt.goback = true;

            contOptions.addOpt(opt);
        }
    }
}
