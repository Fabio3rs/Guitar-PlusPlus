#include "CChart.h"
#include "CMenuSystem.h"
#include "GPPGame.h"
#include <chrono>

namespace {
void addBack(CMenu &menu) {
    CMenu::menuOpt option;
    option.optionName = "back";
    option.text = "Voltar";
    option.x = -.8;
    option.y = .85;
    option.styleOverrides.textSize = .1;
    option.styleOverrides.textColor = ui::Color{.5, .5, .5, 1};
    option.type = CMenu::textbtn;
    option.goback = true;
    option.shortcutKey = GLFW_KEY_ESCAPE;
    menu.addOpt(option);
}
} // namespace

void CMenuSystem::selectPlayerMenu() {
    auto &menu = newNamedMenu("playerSelectMenu");
    menu.clearOptions();
    host.resetMainPlayer();
    addBack(menu);

    // Saved-profile selection is still unimplemented. Keep the existing default
    // player and creation dialog; do not perform the old disabled worker scan.
    auto action = host.getCallBackRealName("menuCreateNewPlayerProfile");
    if (action.empty()) {
        action = host.addGameCallbacks(
            "menuCreateNewPlayerProfile", [this](const std::string &) {
                auto &owner = getMenuByName("playerSelectMenu");
                if (owner.getUIListSize() == 0) {
                    modals.openCreateProfile(owner);
                }
            });
    }
    CMenu::menuOpt option;
    option.optionName = "createProfile";
    option.text = "Criar novo perfil";
    option.anchorRight = true;
    option.x = .6;
    option.y = .85;
    option.styleOverrides.textSize = .1;
    option.type = CMenu::textbtn;
    option.menusXRef.push_back(action);
    menu.addOpt(option);
    openMenus(&menu);
}

std::string CMenuSystem::selectSong() {
    auto &menu = newNamedMenu("selectSongMenu");
    std::map<int, std::string> songs;
    addSongListToMenu(menu, songs);
    for (const auto &song : songs) {
        menu.options[static_cast<size_t>(song.first)].goback = true;
    }
    auto &engine = CEngine::engine();
    while (engine.getMouseButton(0) != 0 ||
           engine.getKey(GLFW_KEY_ENTER) != 0) {
        if (!engine.windowOpened()) {
            return {};
        }
        waitForScreen(.1);
    }
    openMenus(&menu);
    if ((menu.findOption("back")->status & 3) == 3) {
        return {};
    }
    for (const auto &song : songs) {
        if ((menu.options[static_cast<size_t>(song.first)].status & 3) == 3) {
            return song.second;
        }
    }
    return {};
}

void CMenuSystem::addSongListToMenu(CMenu &menu,
                                    std::map<int, std::string> &songs) {
    menu.clearOptions();
    songs.clear();
    menu.setFlow(CMenu::Flow::Vertical, -.75, .7);
    menu.viewportEnabled = true;
    menu.viewport = {-1.0, -.8, 1.0, .8};
    addBack(menu);
    populateSongs(menu, songs, false);
}

void CMenuSystem::populateSongs(CMenu &selectSongMenu,
                                std::map<int, std::string> &menuMusics,
                                bool returnOnSelection) {
    auto &game = host;
    {
        auto pending = std::async(std::launch::async, [&game]() {
            std::vector<std::pair<std::string, bool>> songs;
            for (const auto &song :
                 game.getDirectory("./data/songs", false, true)) {
                const auto chart = CPlayer::smartChartSearch(song);
                if (!chart.empty() && CChart::validateChartHeader(
                                          "data/songs/" + song + "/" + chart)) {
                    songs.emplace_back(song,
                                       !CPlayer::smartSongSearch(song).empty());
                }
            }

            return songs;
        });
        while (pending.wait_for(std::chrono::milliseconds(0)) !=
               std::future_status::ready) {
            if (!CEngine::engine().windowOpened()) {
                return;
            }

            waitForScreen(.01);
        }

        for (const auto &song : pending.get()) {
            CMenu::menuOpt opt;
            opt.optionName = "song:" + song.first;
            opt.text = song.first;
            opt.styleOverrides.textSize = .09;
            opt.group = 1;
            opt.type = CMenu::textbtn;
            opt.goback = returnOnSelection;
            if (!song.second) {
                opt.styleOverrides.textColor = ui::Color{1, 0, 0, 1};
            }
            menuMusics[selectSongMenu.addOpt(opt)] = song.first;
        }
    }
}
