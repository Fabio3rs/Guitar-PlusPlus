#pragma once
#include "CMenu.h"
#include "CModalManager.h"
#include "CUIOverrides.h"

class GPPGame;
enum class PauseResult { Resume, Exit };

// All state belongs to the UI thread. Gameplay enters this service only at
// explicit screen transitions; there is no per-frame gameplay integration.
class CMenuSystem {
    GPPGame &host;
    ui::MenuOverrideSet presentation;
    std::map<std::string, CMenu> gameMenus;
    CMenu *mainMenu = nullptr, *currentMenu = nullptr;
    void drawBackground();
    void waitForScreen(double duration);
    void populateSongs(CMenu &, std::map<int, std::string> &, bool);
    int menusGoBackSE = 0, menusCMMenuSE = 0, menusNextSE = 0,
        menusGameCbNextSE = 0;

  public:
    CMenu &devMenus;
    CModalManager modals;
    explicit CMenuSystem(GPPGame &game)
        : host(game), devMenus(newNamedMenu("devMenus")) {}
    CMenuSystem(const CMenuSystem &) = delete;
    CMenuSystem &operator=(const CMenuSystem &) = delete;
    void initialize();
    // Explicit UI-thread entry point for theme changes (and future file
    // notifications).
    void loadThemePresentation(const std::filesystem::path &directory);
    void replacePresentation(const ui::MenuOverrideSet &);
    void preparePresentation(CMenu &);
    const ui::MenuOverrideSet &presentationOverrides() const {
        return presentation;
    }
    CMenu &newMenu();
    CMenu &newNamedMenu(const std::string &);
    CMenu &getMenuByName(const std::string &);
    CMenu *findMenu(const std::string &);
    void setMainMenu(CMenu &);
    CMenu *getMainMenu();
    CMenu *getActualMenu() { return currentMenu; }
    void eraseGameMenusAutoCreateds();
    std::vector<CMenu *> openMenus(CMenu *,
                                   const std::function<int()> &pre = nullptr,
                                   const std::function<int()> &mid = nullptr,
                                   const std::function<int()> &post = nullptr,
                                   bool dev = false,
                                   std::vector<CMenu *> stack = {});
    void selectPlayerMenu();
    std::string selectSong();
    void addSongListToMenu(CMenu &, std::map<int, std::string> &);
    void continueCampaing(const std::string &);
    PauseResult openPause();
    void buildInitialMenus(int stage);
};
