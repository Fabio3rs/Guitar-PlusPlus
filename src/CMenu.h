#pragma once
#include <cstddef>
#ifndef GUITARPP_CMENU_H
#define GUITARPP_CMENU_H

#include "CEngine.h"
#include "CLuaH.hpp"
#include <cstdint>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <string>

class CMenu {
    std::string menuName;
    static int menusCreated;
    // static std::map <std::string, CMenu*> Menus;

    static std::string textBuffer;

  public:
    struct uiWindowStruct {
        std::unique_ptr<CMenu> m;
        int pos;

        uiWindowStruct() { pos = 0; }
    };

    struct posUiOrder {
        int id;
        int pos;

        bool operator>(const posUiOrder &p0) const { return pos > p0.pos; }

        bool operator<(const posUiOrder &p0) const { return pos < p0.pos; }

        bool operator==(const posUiOrder &p0) const { return pos == p0.pos; }
    };

  private:
    static std::deque<uiWindowStruct> uiList;
    static std::vector<posUiOrder> uiOrderList;
    std::deque<int> myUiList;

    bool uiMenu;
    int thisUiID;

    bool lastEnterOptBtn;
    bool lastMouseClickStatus{};

    static int allocOrGetUiFreeSpace();

    static void refreshMenusUiPosOrder();
    static void interfaceCalcPos();

    posUiOrder &getThisOnUiOrder() const;
    static posUiOrder &getTopUI();
    void putOnTop();
    bool isThisOnTop() const;

  public:
    enum menusOPT {
        none = -1,
        button_ok,
        text_input,
        button_cancel,
        textbtn,
        multi_btn,
        deslizant_Select_list,
        button_ui,
        drag_bar,
        static_text
    };

    static CMenu *getUiMenuOnTop();
    bool isInterfaceOver() const;
    int getUIListSize();
    bool isMouseOnThisMenu();

    static inline uiWindowStruct &getUiAt(size_t index) {
        return uiList[index];
    }
    static void renderUiList();
    static uiWindowStruct &getUILast();

    double x, y;
    bool devMenuNOUpdateOthers;
    std::string mTitle;

    CLuaH::luaScript *lScript;
    std::map<std::string, int> luaCallBacks;

    CEngine::RenderDoubleStruct qbgd{};

    std::string backgroundTexture;
    CMenu *devEditMenu;
    int devEditingOpt;

    bool temp, gameMenu;

    double mouseAX;
    double mouseAY;

    struct menuOpt {
        menusOPT type;
        int group;
        unsigned int texture;
        std::string optionName;

        bool goback;

        bool enableEnter;

        int pressingShortcutKey;
        int shortcutKey;

        std::string text, preText, langEntryKey;
        std::string *externalPreTextRef;
        size_t preTextMaxSize;
        int status;
        int devStatus;
        double optValue, deslizantBarSize;
        int listID;

        bool btnClickStat;

        CLuaH::luaScript *attachedLua;

        std::deque<std::string> optList;

        std::deque<std::string> menusXRef;

        double x, y, size;
        int strEditPoint;

        double color[4];

        std::function<int(CMenu &, menuOpt &)> updateCppCallback;
        std::function<int(CMenu &, menuOpt &)> posUpdateCppCallback;

        void update();

        inline menuOpt() {
            type = button_ok;
            group = -1;
            status = 0;
            x = y = 0.0;
            size = 0.1;
            texture = 0;
            deslizantBarSize = 0.0;
            listID = 0;
            optValue = 0;
            devStatus = 0;
            preTextMaxSize = 30;

            attachedLua = nullptr;
            externalPreTextRef = nullptr;
            strEditPoint = 0;

            goback = false;
            btnClickStat = false;

            enableEnter = true;

            pressingShortcutKey = shortcutKey = 0;

            color[0] = color[1] = color[2] = color[3] = 1.0;

            preText = "type here";
        }
    };

    struct group {
        enum types { none, deslizant_h_menu };
        double x, y, h, w;
        double nx, ny;
        int status;
        int selectedOpt;
        int type;

        group() {
            nx = ny = x = y = h = w = 0.0;
            status = 0;
            selectedOpt = 0;
            type = 0;
        }
    };

    inline const std::string &getName() { return menuName; }

    std::map<int, group> groupInfo;
    std::deque<menuOpt> options;

    std::function<int(CMenu &)> openCallback;

    int pushUserInterface(const CMenu &m);

    size_t getDevSelectedMenuOpt();

    void update();
    void updateDev();
    void shortcutCallback(int key, int scancode, int action, int mods);

    void render();
    int addOpt(const menuOpt &opt);

    size_t getNumOfOptions() { return options.size(); }

    std::string getNameFromOpt(size_t optID) {
        return options[optID].optionName;
    }

    void resetData();
    void resetBtns();

    int status;

    CMenu();
    CMenu(const std::string &name);
    CMenu(const CMenu &) = default;
    ~CMenu();
};

#endif
