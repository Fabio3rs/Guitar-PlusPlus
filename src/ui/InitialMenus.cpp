#include "CBackgroundRenderer.h"
#include "CChart.h"
#include "CLog.h"
#include "CMenuSystem.h"
#include "GPPGame.h"
#include <chrono>
#include <cmath>

#include "CCampaing.h"
#include "CLanguageManager.h"

void CMenuSystem::buildInitialMenus(int stage) {
    auto &game = host;
    auto &lua = CLuaH::Lua();
    auto &lngmgr = CLanguageManager::langMGR();
    auto &campaingMgr = CCampaing::campaingMGR();
    const int mainMenuSettedSE = lua.idForCallbackEvent("mainMenuSetted");
    auto &mainMenu = newNamedMenu("mainMenu");
    auto &gmenu = newNamedMenu("playOptions");
    auto &cmpopts = newNamedMenu("mainCampaingOptions");
    auto &optionsmenu = newNamedMenu("optionsmenu");
    auto &audiomenu = newNamedMenu("audiomenu");
    auto &extrasmenu = newNamedMenu("extrasmenu");
    auto &chartsmenu = newNamedMenu("chartsmenu");
    auto &chartCompileMenu = newNamedMenu("chartCompileMenu");

    double proportion = game.getWindowProportion();

    switch (stage) {
    case 1: {

        {
            CMenu::menuOpt opt;
            opt.optionName = "play";

            opt.text = lngmgr.getText(game.glanguage, "menuPlayTitle");
            opt.langEntryKey = "menuPlayTitle";
            opt.y = 0.4;
            opt.x = -proportion + 0.2;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            opt.menusXRef.push_back(gmenu.getName());

            mainMenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "settings";

            opt.text = lngmgr.getText(game.glanguage, "menuOptionsTitle");
            opt.langEntryKey = "menuOptionsTitle";
            opt.y = 0.3;
            opt.x = -proportion + 0.2;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            opt.menusXRef.push_back(optionsmenu.getName());

            mainMenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "extras";

            opt.text = "Extras";
            opt.langEntryKey = "menuExtrasTitle";
            opt.y = 0.2;
            opt.x = -proportion + 0.2;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            opt.menusXRef.push_back(extrasmenu.getName());

            mainMenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "help";

            opt.text = "Ajuda";
            opt.y = 0.1;
            opt.x = -proportion + 0.2;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            std::string testecallback =
                game.addGameCallbacks("gameHelpMenu", GPPGame::helpMenu);
            opt.menusXRef.push_back(testecallback);

            mainMenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "quit";

            opt.text = "Sair";
            opt.y = 0.0;
            opt.x = -proportion + 0.2;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;
            opt.goback = true;

            mainMenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "sampleInput";

            opt.text = "aaaa";
            opt.y = -0.3;
            opt.x = -proportion + 0.2;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::text_input;
            opt.preText = "Teste campo Digita\xc3\xa7\xc3\xa3o";

            mainMenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "marathon";

            opt.text = "Maratona";
            opt.y = 0.4;
            opt.centerX = -0.5;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            std::string testecallback = game.addGameCallbacks(
                "gamePlaySelectedInMenu", GPPGame::startMarathonModule);
            opt.menusXRef.push_back(testecallback);

            gmenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "quickPlay";

            opt.text = "Quick play";
            opt.y = 0.4;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            std::string testecallback = game.addGameCallbacks(
                "gamePlaySelectedInMenu", GPPGame::startModule);
            opt.menusXRef.push_back(testecallback);

            gmenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "campaign";

            opt.text = "Campaing";
            opt.y = 0.3;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            opt.menusXRef.push_back(cmpopts.getName());
            gmenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "server";

            opt.text = "Server MP";
            opt.y = 0.2;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            std::string testecallback = game.addGameCallbacks(
                "gamePlayMPSelectedInMenu", GPPGame::serverModule);
            opt.menusXRef.push_back(testecallback);

            gmenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "client";

            opt.text = "Client MP";
            opt.y = 0.1;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            std::string testecallback =
                game.addGameCallbacks("patosVoadores", GPPGame::testClient);
            opt.menusXRef.push_back(testecallback);

            gmenu.addOpt(opt);
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

            gmenu.addOpt(opt);
        }

        campaingMgr.campaingMainMenu(cmpopts);

    } break;

    case 2: {
        {
            CMenu::menuOpt opt;
            opt.optionName = "graphics";

            opt.text = "Graficos";
            opt.y = 0.4;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            optionsmenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "audio";

            opt.text = "Audio";
            opt.y = 0.3;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            opt.menusXRef.push_back(audiomenu.getName());

            optionsmenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "gameplay";

            opt.text = "Gameplay";
            opt.y = 0.2;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            optionsmenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "back";

            opt.text = "Voltar";
            opt.y = -0.4;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;
            opt.goback = true;

            optionsmenu.addOpt(opt);
        }
    } break;

    case 3: {
        {
            CMenu::menuOpt opt;
            opt.optionName = "volume";

            opt.text = "Volume principal";
            opt.y = 0.3;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::deslizant_Select_list;
            opt.deslizantBarSize = 0.5;

            opt.optList = {"0.0", "0.1", "0.2", "0.3", "0.4", "0.5",
                           "0.6", "0.7", "0.8", "0.9", "1.0"};

            auto syncVolume = [](CMenu::menuOpt &option) {
                const double volume = CEngine::getMainVolume();
                option.listID =
                    std::isfinite(volume)
                        ? static_cast<int>(std::clamp(volume, 0.0, 1.0) * 10)
                        : 0;
            };
            syncVolume(opt);
            opt.updateCppCallback = [syncVolume](CMenu &,
                                                 CMenu::menuOpt &option) {
                if (option.status == 0) {
                    syncVolume(option);
                }
                return 0;
            };
            opt.posUpdateCppCallback = [](CMenu &, CMenu::menuOpt &option) {
                if (option.status != 0) {
                    CEngine::setMainVolume(
                        static_cast<float>(std::clamp(option.listID, 0, 10)) /
                        10.0f);
                }
                return 0;
            };

            audiomenu.addOpt(opt);
        }

        {
            CMenu::menuOpt opt;
            opt.optionName = "back";

            opt.text = "Voltar";
            opt.y = -0.4;
            opt.centerX = 0.0;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;
            opt.goback = true;

            audiomenu.addOpt(opt);
        }
    }

        mainMenu.setFlow(CMenu::Flow::Vertical, -proportion + .2, .4);
        mainMenu.anchorLeft = true;
        for (auto *menu : {&gmenu, &cmpopts, &optionsmenu, &audiomenu,
                           &extrasmenu, &chartsmenu, &chartCompileMenu}) {
            menu->setFlow(CMenu::Flow::Vertical, -.5, .4);
        }

        setMainMenu(mainMenu);
        lua.runEvent(mainMenuSettedSE);
        break;

    case 4: {
        game.loadTexture("data/sprites", "menu.tga");
    } break;

    case 5: {
        {
            CMenu::menuOpt opt;
            opt.optionName = "charts";

            opt.text = "Charts";
            opt.y = 0.4;
            opt.x = -proportion + 0.2;
            opt.group = 1;
            opt.status = 0;
            opt.type = CMenu::menusOPT::textbtn;

            opt.menusXRef.push_back(chartsmenu.getName());

            extrasmenu.addOpt(opt);
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

            extrasmenu.addOpt(opt);
        }
    }

        {
            {
                CMenu::menuOpt opt;
                opt.optionName = "compile";

                opt.text = "Compilador de .chart para .gpp";
                opt.y = 0.4;
                opt.x = -proportion + 0.2;
                opt.group = 1;
                opt.status = 0;
                opt.type = CMenu::menusOPT::textbtn;

                opt.menusXRef.push_back(chartCompileMenu.getName());

                chartsmenu.addOpt(opt);
            }

            {
                CMenu::menuOpt opt;
                opt.optionName = "editor";

                opt.text = "Criador de charts";
                opt.y = 0.3;
                opt.x = -proportion + 0.2;
                opt.group = 1;
                opt.status = 0;
                opt.type = CMenu::menusOPT::textbtn;

                std::string testecallback = game.addGameCallbacks(
                    "gameCharter", GPPGame::charterModule);
                opt.menusXRef.push_back(testecallback);

                chartsmenu.addOpt(opt);
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

                chartsmenu.addOpt(opt);
            }
        }

        {
            {
                CMenu::menuOpt opt;
                opt.optionName = "back";

                opt.text = "Voltar";
                opt.y = 0.85;
                opt.x = -1.0;
                opt.group = 1;
                opt.status = 0;
                opt.type = CMenu::menusOPT::textbtn;
                opt.goback = true;

                chartCompileMenu.addOpt(opt);
            }
        }

        break;

    default:
        break;
    }
}
