#include "CBackgroundRenderer.h"
#include "CChart.h"
#include "CLog.h"
#include "CMenuSystem.h"
#include "GPPGame.h"
#include <chrono>

PauseResult CMenuSystem::openPause() {
    CMenu menu("pauseMenu");
    menu.gameMenu = true;
    menu.setFlow(CMenu::Flow::Vertical, -.5, .4);
    CMenu::menuOpt option;
    option.optionName = "resume";
    option.text = "Voltar";
    option.type = CMenu::textbtn;
    option.group = 1;
    option.goback = true;
    menu.addOpt(option);
    option.optionName = "exit";
    option.text = "Sair";
    const int exit = menu.addOpt(option);
    openMenus(&menu);
    return (menu.options[static_cast<size_t>(exit)].status & 2) != 0
               ? PauseResult::Exit
               : PauseResult::Resume;
}
