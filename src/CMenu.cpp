#include "CMenu.h"
#include "CControls.h"
#include "CEngine.h"
#include "CFonts.h"
#include "CLanguageManager.h"
#include "CLog.h"
#include "GPPGame.h"
#include "utf8.h"
#include <iostream>

// std::map <std::string, CMenu*> CMenu::Menus = std::map <std::string,
// CMenu*>();
std::deque<CMenu::uiWindowStruct> CMenu::uiList =
    std::deque<CMenu::uiWindowStruct>();
std::vector<CMenu::posUiOrder> CMenu::uiOrderList =
    std::vector<CMenu::posUiOrder>();
int CMenu::menusCreated = 0;

auto CMenu::getUIListSize() -> int { return myUiList.size(); }

void CMenu::menuOpt::update() {
    if (!langEntryKey.empty()) {
        auto &lngmgr = CLanguageManager::langMGR();
        auto &game = GPPGame::GuitarPP();
        const std::string txt = lngmgr.getText(game.glanguage, langEntryKey);

        if (!txt.empty()) {
            { text = std::move(txt); }
        } else {
            CLog::log() << "On CMenu::menuOpt::update():\n		"
                           "		<<" +
                               text + ">>	 language entry key <<" +
                               langEntryKey + ">> is null or not exists.";
            CLog::log().multiRegister("game.glanguage <<%0>>, <<%1>> ",
                                      game.glanguage, langEntryKey);

            if (text.empty()) {
                text = langEntryKey + "n/f/";
            }
        }
    }
}

auto CMenu::getUILast() -> CMenu::uiWindowStruct & {
    if (!uiList.empty()) {
        { return uiList.back(); }
    }

    static uiWindowStruct result;
    return result;
}

auto CMenu::addOpt(const menuOpt &opt) -> int {
    options.push_back(opt);
    options.back().update();

    auto genName = [this](int optID) {
        return menuName + std::string("_") + std::to_string(optID) +
               std::to_string((int)(options[optID].group));
    };

    options.back().optionName = genName(options.size() - 1);

    auto &gp = groupInfo[opt.group];

    return options.size() - 1;
}

void CMenu::resetData() {
    for (auto &opt : options) {
        opt.status = 0;
    }
}

void CMenu::resetBtns() {
    lastEnterOptBtn = false;
    lastMouseClickStatus = false;

    status = 0;

    for (auto &opt : options) {
        opt.btnClickStat = false;
    }
}

void CMenu::render() {
    auto textSizeInScreen = [](const std::string &s, double size) {
        double charnums = CFonts::utf8Size(s);
        return (charnums * size / 1.5) + (size / 2.0);
    };

    auto textSizeInScreenCharNum = [](double charnums, double size) {
        return (charnums * size / 1.5) + (size / 2.0);
    };

    if (qbgd.Text != 0) {
        if (x != 0.0 || y != 0.0) {
            CEngine::RenderDoubleStruct RenderData = qbgd;
            RenderData.x1 += x;
            RenderData.x2 += x;
            RenderData.x3 += x;
            RenderData.x4 += x;

            RenderData.y1 += y;
            RenderData.y2 += y;
            RenderData.y3 += y;
            RenderData.y4 += y;
            CEngine::engine().Render3DQuadWithAlpha(RenderData);
        } else {
            CEngine::engine().Render3DQuadWithAlpha(qbgd);
        }
    }

    for (auto &grp : groupInfo) {
        switch (grp.second.type) {
        case group::deslizant_h_menu:
            CEngine::RenderDoubleStruct RenderData{};

            RenderData.Text = 0;

            RenderData.x1 = grp.second.x + x;
            RenderData.x2 = RenderData.x1 + grp.second.w + x;
            RenderData.x3 = RenderData.x1 + grp.second.w + x;
            RenderData.x4 = grp.second.x + x;

            RenderData.y1 = grp.second.y + grp.second.h + y;
            RenderData.y2 = RenderData.y1 + grp.second.h + y;
            RenderData.y3 = RenderData.y1 + y;
            RenderData.y4 = grp.second.y + y;

            RenderData.TextureX1 = 0.0;
            RenderData.TextureX2 = 1.0;
            RenderData.TextureY1 = 1.0;
            RenderData.TextureY2 = 0.0;

            CEngine::engine().Render2DQuad(RenderData);
            break;
        }
    }

    for (auto &opt : options) {
        switch (opt.type) {
        case button_ok:
            break;

        case text_input:
            CEngine::engine().setColor(opt.color[0], opt.color[1], opt.color[2],
                                       opt.color[3]);
            {
                double textSize = CFonts::fonts().getXSizeInScreen(
                    opt.preText, opt.size) /*textSizeInScreen(opt.preText,
                                              opt.size) + opt.size * 0.5*/
                    ;

                {
                    CEngine::RenderDoubleStruct RenderData{};

                    RenderData.x1 = opt.x + x;
                    RenderData.x2 = opt.x + textSize + x;
                    RenderData.x3 = opt.x + textSize + x;
                    RenderData.x4 = opt.x + x;

                    RenderData.y1 = opt.y + opt.size + y;
                    RenderData.y2 = opt.y + opt.size + y;
                    RenderData.y3 = opt.y + y;
                    RenderData.y4 = opt.y + y;

                    RenderData.z1 = 0.0;
                    RenderData.z2 = 0.0;
                    RenderData.z3 = 0.0;
                    RenderData.z4 = 0.0;

                    RenderData.TextureX1 = 0.0;
                    RenderData.TextureX2 = 1.0;
                    RenderData.TextureY1 = 1.0;
                    RenderData.TextureY2 = 0.0;

                    RenderData.Text = 0;

                    CEngine::engine().Render2DQuad(RenderData);
                }

                CFonts::fonts().drawTextInScreen(opt.preText, opt.x + x,
                                                 opt.y + y, opt.size);

                if ((opt.status & 4) != 0) {
                    CEngine::RenderDoubleStruct RenderData{};

                    double ptrAt =
                        textSizeInScreenCharNum(opt.strEditPoint, opt.size);

                    RenderData.x1 = 0.0;
                    RenderData.x1 += ptrAt - opt.size / 4.0;
                    RenderData.x2 = RenderData.x1;
                    RenderData.x3 = RenderData.x2 + opt.size;
                    RenderData.x4 = RenderData.x1 + opt.size;

                    RenderData.y1 = 0.0 - opt.size / 2.0;
                    RenderData.y2 = RenderData.y1 + opt.size;
                    RenderData.y3 = RenderData.y1 + opt.size;
                    RenderData.y4 = RenderData.y1;

                    RenderData.Text = GPPGame::GuitarPP().SPR["palheta"];

                    RenderData.TextureX1 = 0.0;
                    RenderData.TextureX2 = 1.0;
                    RenderData.TextureY1 = 1.0;
                    RenderData.TextureY2 = 0.0;

                    CEngine::engine().renderAt(opt.x + x, opt.y + y, 0.0);

                    // CEngine::engine().Rotate(90.0, 0.0, 0.0, 1.0);

                    CEngine::engine().Render2DQuad(RenderData);
                    CEngine::engine().matrixReset();
                }
            }
            break;

        case textbtn:
            CEngine::engine().setColor(opt.color[0], opt.color[1], opt.color[2],
                                       opt.color[3]);
            if ((opt.status & 1) != 0) {
                CEngine::RenderDoubleStruct RenderData{};
                RenderData.x1 = opt.x - opt.size + x;
                RenderData.x2 = opt.x /*- opt.size + opt.size*/ + x;
                RenderData.x3 = opt.x /*- opt.size + opt.size*/ + x;
                RenderData.x4 = opt.x - opt.size + x;

                RenderData.y1 = opt.y + opt.size + y;
                RenderData.y2 = opt.y + opt.size + y;
                RenderData.y3 = opt.y + y;
                RenderData.y4 = opt.y + y;

                RenderData.Text = GPPGame::GuitarPP().SPR["palheta"];

                RenderData.TextureX1 = 0.0;
                RenderData.TextureX2 = 1.0;
                RenderData.TextureY1 = 1.0;
                RenderData.TextureY2 = 0.0;

                CEngine::engine().Render2DQuad(RenderData);
            }
            CFonts::fonts().drawTextInScreen(opt.text, opt.x + x, opt.y + y,
                                             opt.size);
            break;

        case deslizant_Select_list:
            CEngine::engine().setColor(opt.color[0], opt.color[1], opt.color[2],
                                       opt.color[3]);

            {
                double textSize = textSizeInScreen(opt.text, opt.size);
                double barPosX1 = opt.x + textSize + opt.size;
                double barPosX2;
                barPosX1 += x;
                barPosX2 = barPosX1 + opt.deslizantBarSize;
                double barPosXStep = opt.deslizantBarSize / opt.optList.size();
                barPosXStep *= opt.listID;

                {
                    CEngine::RenderDoubleStruct RenderData{};

                    RenderData.x1 = barPosX1;
                    RenderData.x2 = barPosX1 + opt.deslizantBarSize;
                    RenderData.x3 = barPosX1 + opt.deslizantBarSize;
                    RenderData.x4 = barPosX1;

                    RenderData.y1 = opt.y + opt.size + y;
                    RenderData.y2 = opt.y + opt.size + y;
                    RenderData.y3 = opt.y + y;
                    RenderData.y4 = opt.y + y;

                    RenderData.z1 = 0.0;
                    RenderData.z2 = 0.0;
                    RenderData.z3 = 0.0;
                    RenderData.z4 = 0.0;

                    RenderData.TextureX1 = 0.0;
                    RenderData.TextureX2 = 1.0;
                    RenderData.TextureY1 = 1.0;
                    RenderData.TextureY2 = 0.0;

                    RenderData.Text = 0;

                    CEngine::engine().Render2DQuad(RenderData);
                }

                double absolutePos = barPosX1 + barPosXStep;

                CEngine::RenderDoubleStruct RenderData{};
                RenderData.x1 = absolutePos;
                RenderData.x2 = absolutePos + opt.size;
                RenderData.x3 = absolutePos + opt.size;
                RenderData.x4 = absolutePos;

                RenderData.y1 = opt.y + opt.size + y;
                RenderData.y2 = opt.y + opt.size + y;
                RenderData.y3 = opt.y + y;
                RenderData.y4 = opt.y + y;

                RenderData.Text = GPPGame::GuitarPP().SPR["palheta"];

                RenderData.TextureX1 = 0.0;
                RenderData.TextureX2 = 1.0;
                RenderData.TextureY1 = 1.0;
                RenderData.TextureY2 = 0.0;

                CEngine::engine().Render2DQuad(RenderData);

                barPosX1 += opt.deslizantBarSize + opt.size;

                CFonts::fonts().drawTextInScreen(opt.text, opt.x + x, opt.y + y,
                                                 opt.size);
                CFonts::fonts().drawTextInScreen(opt.optList[opt.listID],
                                                 barPosX1, opt.y + y, opt.size);
            }
            break;

        case drag_bar:
            CEngine::engine().setColor(opt.color[0], opt.color[1], opt.color[2],
                                       opt.color[3]);

            {
                CEngine::RenderDoubleStruct RenderData{};

                RenderData.x1 = opt.x + x;
                RenderData.x2 = opt.x + opt.deslizantBarSize + x;
                RenderData.x3 = opt.x + opt.deslizantBarSize + x;
                RenderData.x4 = opt.x + x;

                RenderData.y1 = opt.y + opt.size + y;
                RenderData.y2 = opt.y + opt.size + y;
                RenderData.y3 = opt.y + y;
                RenderData.y4 = opt.y + y;

                RenderData.z1 = 0.0;
                RenderData.z2 = 0.0;
                RenderData.z3 = 0.0;
                RenderData.z4 = 0.0;

                RenderData.TextureX1 = 0.0;
                RenderData.TextureX2 = 1.0;
                RenderData.TextureY1 = 1.0;
                RenderData.TextureY2 = 0.0;

                RenderData.Text = 0;

                CEngine::engine().Render2DQuad(RenderData);
            }
            break;

        case static_text:
            CEngine::engine().setColor(opt.color[0], opt.color[1], opt.color[2],
                                       opt.color[3]);

            CFonts::fonts().drawTextInScreen(opt.text, opt.x + x, opt.y + y,
                                             opt.size);
            break;

        default:
            break;
        }
    }
    CEngine::engine().setColor(1.0, 1.0, 1.0, 1.0);

    /*for (auto &ui : uiList)
    {
            ui->render();
    }*/
}

auto CMenu::isMouseOnThisMenu() -> bool {
    auto textSizeInScreen = [](const std::string &s, double size) {
        double charnums = CFonts::utf8Size(s);
        return (charnums * size / 1.5) + (size / 2.0);
    };

    auto isMouseOver2DQuad = [this](double x, double y, double w, double h) {
        double mx = CEngine::engine().mouseX;
        double my = CEngine::engine().mouseY;
        x += this->x;
        y += this->y;

        return mx >= x && mx <= (x + w) && my >= (y - h) && my <= y;
    };

    double textSize = 0.0;
    double barPosX1 = 0.0;
    double barPosX2 = 0.0;

    if (qbgd.Text != 0) {
        double w = qbgd.x1 - qbgd.x2;

        if (w < 0.0) {
            { w = -w; }
        }

        double h = qbgd.y1 - qbgd.y3;

        if (h < 0.0) {
            { h = -h; }
        }

        bool isinbg = isMouseOver2DQuad(qbgd.x1, qbgd.y1, w, h);

        if (isinbg) {
            { return true; }
        }
    }

    for (auto &opt : options) {
        switch (opt.type) {
        case button_ok:

            break;

        case text_input:
            textSize = textSizeInScreen(opt.preText, opt.size) + 0.1;

            if (isMouseOver2DQuad(opt.x - (opt.size * 0.05), opt.y - opt.size,
                                  textSize, opt.size * 1.05)) {
                return true;
            }

            break;

        case textbtn:
            textSize = textSizeInScreen(opt.text, opt.size);

            if (isMouseOver2DQuad(opt.x - (opt.size * 0.05), opt.y, textSize,
                                  opt.size * 1.05)) {
                return true;
            }
            break;

        case deslizant_Select_list:
            textSize = textSizeInScreen(opt.text, opt.size);
            barPosX1 = opt.x + textSize + opt.size;
            barPosX2 = barPosX1 + opt.deslizantBarSize;

            if (isMouseOver2DQuad(barPosX1, opt.y, opt.deslizantBarSize,
                                  opt.size)) {
                return true;
            }
            break;

        case drag_bar:
            if (isMouseOver2DQuad(opt.x, opt.y, opt.deslizantBarSize,
                                  opt.size)) {
                return true;
            }

            break;

        case static_text:
            textSize = textSizeInScreen(opt.text, opt.size);

            if (isMouseOver2DQuad(opt.x - (opt.size * 0.05), opt.y, textSize,
                                  opt.size * 1.05)) {
                return true;
            }
            break;

        default:
            break;
        }
    }

    return false;
}

auto CMenu::getUiMenuOnTop() -> CMenu * {
    for (intptr_t i = uiOrderList.size() - 1; i >= 0; i--) {
        CMenu *m = uiList[uiOrderList[i].id].m.get();
        if (m != nullptr) {
            return m;
        }
    }

    return nullptr;
}

auto CMenu::isInterfaceOver() const -> bool {
    bool result = false;
    if (thisUiID >= 0) {
        for (intptr_t i = uiOrderList.size() - 1; i >= 0; i--) {
            if (uiOrderList[i].id == thisUiID) {
                break;
            }

            CMenu *m = uiList[uiOrderList[i].id].m.get();
            if ((m != nullptr) && m->isMouseOnThisMenu()) {
                result = true;
                break;
            }
        }
    } else {
        for (intptr_t i = uiOrderList.size() - 1; i >= 0; i--) {
            CMenu *m = uiList[uiOrderList[i].id].m.get();
            if ((m != nullptr) && m->isMouseOnThisMenu()) {
                result = true;
                break;
            }
        }
    }

    return result;
}

auto CMenu::getThisOnUiOrder() const -> CMenu::posUiOrder & {
    for (intptr_t i = uiOrderList.size() - 1; i >= 0; i--) {
        if (uiOrderList[i].id == thisUiID) {
            return uiOrderList[i];
        }
    }

    throw std::logic_error("CMenu::getThisOnUiOrder()");
}

auto CMenu::getTopUI() -> CMenu::posUiOrder & {
    for (intptr_t i = uiOrderList.size() - 1; i >= 0; i--) {
        CMenu *m = uiList[uiOrderList[i].id].m.get();

        if (m != nullptr) {
            return uiOrderList[i];
        }
    }

    throw std::logic_error("CMenu::getTopUI()");
}

void CMenu::refreshMenusUiPosOrder() {
    for (auto &i : uiOrderList) {
        uiList[i.id].pos = i.pos;
    }
}

void CMenu::putOnTop() {
    if (uiMenu) {
        posUiOrder &ui = getThisOnUiOrder();
        const posUiOrder cpyui = ui;

        posUiOrder &ui2 = getTopUI();
        const posUiOrder cpyui2 = ui2;

        ui.id = cpyui2.id;
        ui2.id = cpyui.id;

        ui.pos = cpyui2.pos;
        ui2.pos++;

        uiList[ui.id].pos = ui.pos;
        uiList[ui2.id].pos = ui2.pos;

        std::sort(uiOrderList.begin(), uiOrderList.end());

        // std::cout << ui.id << "  " << ui2.id << std::endl;

        // uiList[ui.id].pos = ui.pos;
        // uiList[ui2.id].pos = ui2.pos;
        // refreshMenusUiPosOrder();
        interfaceCalcPos();
    }
}

void CMenu::update() {
    CLuaH::multiCallBackParams_t menucallback;
    menucallback.push_back(CLuaH::customParam(menuName));

    /// CLuaH::Lua().runEventWithParams(std::string("pre") + menuName +
    /// std::string("update"), menucallback);

    auto textSizeInScreen = [](const std::string &s, double size) {
        double charnums = CFonts::utf8Size(s);
        return (charnums * size / 1.5) + (size / 2.0);
    };

    ////std::cout << CFonts::utf8Size("aaa") << std::endl;

    const bool mouseOnThisMenu = !isInterfaceOver();

    auto isMouseOver2DQuad = [this, &mouseOnThisMenu](double x, double y,
                                                      double w, double h) {
        if (!mouseOnThisMenu) {
            { return false; }
        }

        double mx = CEngine::engine().mouseX;
        double my = CEngine::engine().mouseY;
        x += this->x;
        y += this->y;

        return mx >= x && mx <= x + w && my >= y - h && my <= y;
    };

    auto desselectAllFromGroup = [this](int group) {
        if (group < 0) {
            { return; }
        }
        for (auto &opt : options) {
            if (opt.group == group) {
                opt.status &= ~(1);
            }
        }
    };

    auto desselectAllTextClick = [this](CMenu::menuOpt &optd) {
        for (auto &opt : options) {
            if (&optd != &opt) {
                opt.status &= ~(/*1 |*/ 4);
            }
        }
    };

    // int leftKey = CEngine::engine().getKey(GLFW_KEY_LEFT), rightKey =
    // CEngine::engine().getKey(GLFW_KEY_RIGHT);

    bool enterOpt = false;
    bool mBTNClick = false;

    double mxd = CEngine::engine().mouseX - mouseAX;
    double myd = CEngine::engine().mouseY - mouseAY;

    mouseAX = CEngine::engine().mouseX;
    mouseAY = CEngine::engine().mouseY;

    auto desselectAllClick = [this]() {
        for (auto &opt : options) {
            opt.devStatus &= ~2;
        }
    };

    if (CEngine::engine().getMouseButton(0) != 0) {
        mBTNClick = true;
    }

    bool lastMouseClick = lastMouseClickStatus;
    lastMouseClickStatus = mBTNClick;

    {
        bool ontop = isThisOnTop();

        if (uiMenu && (mBTNClick && !lastMouseClick) && mouseOnThisMenu) {
            if (!ontop && isMouseOnThisMenu()) {
                putOnTop();
            }
        }

        if (mouseOnThisMenu) {
        }
    }

    if (((CEngine::engine().getKey(GLFW_KEY_ENTER) != 0) || mBTNClick) &&
        lastEnterOptBtn) {
        enterOpt = (status = 1 != 0);
    }

    for (auto &grp : groupInfo) {
        switch (grp.second.type) {
        case group::deslizant_h_menu:
            if (isMouseOver2DQuad(grp.second.x, grp.second.y, grp.second.w,
                                  grp.second.h)) {
            }
        }
    }

    double textSize = 0.0;
    double barPosX1 = 0.0;
    double barPosX2 = 0.0;

    auto kprocess = [](CControls::keyState &k, int i) {
        if (k.pressed && !k.lastFramePressed) {
            k.t = CEngine::engine().getTime() + 0.1;
            k.lastFramePressed = k.pressed;
            return i;
        }

        if (k.pressed && k.lastFramePressed &&
            (CEngine::engine().getTime() - k.t) > 0.1) {
            k.t = CEngine::engine().getTime();
            k.lastFramePressed = k.pressed;
            return i;
        }

        return 0;
    };

    bool ontop = true;

    if (uiMenu) {
        ontop = isThisOnTop();
    }

    int i = 0;
    for (auto &opt : options) {
        // std::string optCallBackTXT = opt.optionName;
        if (opt.updateCppCallback) {
            { opt.updateCppCallback(*this, opt); }
        }

        CLuaH::multiCallBackParams_t params;

        params.push_back(CLuaH::customParam(menuName));
        params.push_back(CLuaH::customParam(opt.optionName));
        params.push_back(CLuaH::customParam(opt.optValue));

        if (opt.shortcutKey != 0) {
            int pressing = CEngine::engine().getKey(opt.shortcutKey);
            if ((opt.pressingShortcutKey != 0) && (pressing == 0)) {
                desselectAllFromGroup(opt.group);
                opt.status |= 1 | 2;
                opt.pressingShortcutKey = 0;
            }

            if (pressing != 0) {
                opt.pressingShortcutKey = 1;

                params.push_back(CLuaH::customParam(2.0));
            }
        }

        if (ontop) {
            switch (opt.type) {
            case button_ok:

                break;

            case text_input: {
                auto &controls = CControls::controls();

                textSize = textSizeInScreen(opt.preText, opt.size) + 0.1;

                if (isMouseOver2DQuad(opt.x - (opt.size * 0.05),
                                      opt.y - (opt.size), textSize,
                                      opt.size * 1.10) &&
                    enterOpt) {
                    desselectAllTextClick(opt);

                    int textChars = CFonts::utf8Size(opt.preText);

                    // return (charnums * size / 1.5) + (size / 2.0);

                    double xtest = (mouseAX - x) - opt.x;
                    xtest -= (opt.size / 2.0);
                    xtest /= opt.size / 1.5;

                    if (xtest < 0) {
                        { xtest = 0; }
                    }

                    if (xtest > textChars) {
                        { xtest = textChars; }
                    }

                    desselectAllFromGroup(opt.group);
                    opt.status = 4;
                    opt.strEditPoint = static_cast<int>(xtest);

                    if (params.size() <= 3) {
                        {
                            params.push_back(
                                CLuaH::customParam((double)opt.status));
                        }
                    }

                    if (opt.group >= 0) {
                        groupInfo[opt.group].selectedOpt = i;
                    }
                }

                if ((opt.status & 4) != 0) {
                    controls.update();
                    int ch = 0;

                    std::string utf8CharOrStr;

                    {
                        char utftempbuf[8] = {0};

                        if (controls.lastChar != 0) {
                            utf8::append(controls.lastChar, utftempbuf);
                            controls.lastChar = 0;
                            utf8CharOrStr = utftempbuf;
                            ch = 1;
                        }
                    }

                    if (ch != 0) {
                        if (opt.externalPreTextRef != nullptr) {
                            opt.preText = *opt.externalPreTextRef;
                        }

                        {
                            if (opt.preText.size() < opt.preTextMaxSize) {
                                opt.strEditPoint = CFonts::utf8InsertAt(
                                    opt.preText, utf8CharOrStr,
                                    opt.strEditPoint);
                            }
                        }
                    }

                    if (opt.externalPreTextRef != nullptr) {
                        *opt.externalPreTextRef = opt.preText;
                    }
                }
            } break;

            case textbtn:
                textSize = textSizeInScreen(opt.text, opt.size);

                if (isMouseOver2DQuad(opt.x - (opt.size * 0.05), opt.y,
                                      textSize, opt.size * 1.05)) {
                    desselectAllFromGroup(opt.group);
                    opt.status = (enterOpt && opt.enableEnter) ? 1 | 2 : 1;

                    if (enterOpt) {
                        { desselectAllTextClick(opt); }
                    }

                    if (params.size() <= 3) {
                        {
                            params.push_back(
                                CLuaH::customParam((double)opt.status));
                        }
                    }

                    if (opt.group >= 0) {
                        groupInfo[opt.group].selectedOpt = i;
                    }
                } else {
                    opt.status = opt.status & 1;
                }
                break;

            case deslizant_Select_list:
                textSize = textSizeInScreen(opt.text, opt.size);
                barPosX1 = opt.x + textSize + opt.size;
                barPosX2 = barPosX1 + opt.deslizantBarSize;

                if (enterOpt &&
                    isMouseOver2DQuad(barPosX1, opt.y, opt.deslizantBarSize,
                                      opt.size)) {
                    opt.status |= 1;
                }

                if ((opt.status & 1) != 0) {
                    if (mBTNClick) {
                        double subX = (CEngine::engine().mouseX - barPosX1);

                        desselectAllTextClick(opt);
                        opt.status |= 1;

                        if (subX >= 0.0) {
                            double optListSized = opt.optList.size();

                            opt.listID = static_cast<int>(floor(
                                (optListSized / opt.deslizantBarSize) * subX));

                            if (opt.listID >=
                                static_cast<int>(opt.optList.size())) {
                                opt.listID = opt.optList.size() - 1;
                            } else if (opt.listID == (~(size_t)(0u))) {
                                opt.listID = 0;
                            }
                        }

                        if (params.size() <= 3) {
                            { params.push_back(CLuaH::customParam(2.0)); }
                        }
                    }
                }
                break;

            case drag_bar:
                if (!mBTNClick) {
                    opt.btnClickStat = false;
                }

                if (isMouseOver2DQuad(opt.x, opt.y, opt.deslizantBarSize,
                                      opt.size)) {
                    if (enterOpt && !opt.btnClickStat) {
                        desselectAllClick();
                        opt.devStatus |= 1;
                        opt.devStatus |= 2;
                        opt.btnClickStat = true;
                    }
                }

                if (!opt.btnClickStat) {
                    if ((opt.devStatus & 1) != 0) {
                        opt.devStatus &= ~1;
                    }
                } else {
                    if ((opt.devStatus & 1) != 0) {
                        // std::cout << (opt.devStatus & 1) << std::endl;
                        x += mxd;
                        y += myd;
                    }
                }

                break;

            case static_text:
                break;

            default:
                break;
            }
        }

        if (opt.posUpdateCppCallback) {
            { opt.posUpdateCppCallback(*this, opt); }
        }

        if (params.size() <= 3) {
            { params.push_back(CLuaH::customParam(-1.0)); }
        }
        /// CLuaH::Lua().runEventWithParams(opt.optionName, params);
        ++i;
    }

    /// CLuaH::Lua().runEventWithParams(std::string("pos") + menuName +
    /// std::string("update"), menucallback);

    bool erase = false;
    // auto er = uiList.begin();

    for (auto it = myUiList.begin(); it != myUiList.end(); /*****/) {
        auto &m = *it;
        if (m == -1) {
            it = myUiList.erase(it);
        } else {
            if (myUiList.empty()) {
                { break; }
            }

            bool erase = false;

            auto &uiw = uiList[m];
            if (uiw.m != nullptr) {
                auto &ui = *uiw.m;
                ui.update();

                for (auto &opt : ui.options) {
                    if ((opt.status & 3) == 3) {
                        if (opt.goback) {
                            erase = true;
                            break;
                        }
                    }
                }
            }

            if (erase) {
                uiw.m->resetData();
                uiw.m = nullptr;
                uiw.pos = 0;

                m = -1;

                it = myUiList.erase(it);
            } else {
                ++it;
            }
        }
    }

    lastEnterOptBtn = mBTNClick;
}

void CMenu::updateDev() {
    auto textSizeInScreen = [](const std::string &s, double size) {
        double charnums = CFonts::utf8Size(s);
        return (charnums * size / 1.5) + (size / 2.0);
    };

    const bool mouseOnThisMenu = !isInterfaceOver();

    auto isMouseOver2DQuad = [this, &mouseOnThisMenu](double x, double y,
                                                      double w, double h) {
        if (!mouseOnThisMenu) {
            { return false; }
        }

        double mx = CEngine::engine().mouseX;
        double my = CEngine::engine().mouseY;
        x += this->x;
        y += this->y;

        return mx >= x && mx <= x + w && my >= y - h && my <= y;
    };

    auto desselectAllFromGroup = [this](int group) {
        if (group < 0) {
            { return; }
        }
        for (auto &opt : options) {
            if (opt.group == group) {
                opt.status &= ~1;
            }
        }
    };

    auto desselectAllClick = [this]() {
        for (auto &opt : options) {
            opt.devStatus &= ~2;
        }
    };

    int leftKey = CEngine::engine().getKey(GLFW_KEY_LEFT);
    int rightKey = CEngine::engine().getKey(GLFW_KEY_RIGHT);

    int plusK = CEngine::engine().getKey(GLFW_KEY_KP_ADD);
    int minusK = CEngine::engine().getKey(GLFW_KEY_KP_SUBTRACT);

    bool enterOpt = false;
    bool mBTNClick = false;

    static bool btnClickStat = false;

    double mxd = CEngine::engine().mouseX - mouseAX;
    double myd = CEngine::engine().mouseY - mouseAY;

    mouseAX = CEngine::engine().mouseX;
    mouseAY = CEngine::engine().mouseY;

    if (CEngine::engine().getMouseButton(0) != 0) {
        mBTNClick = true;
    } else {
        btnClickStat = false;
    }

    double textSize = 0.0;
    double barPosX1 = 0.0;
    double barPosX2 = 0.0;

    int i = 0;
    for (auto &opt : options) {
        switch (opt.type) {
        case button_ok:

            break;

        case textbtn:
            textSize = textSizeInScreen(opt.text, opt.size);

            if (isMouseOver2DQuad(opt.x - (opt.size * 0.05), opt.y, textSize,
                                  opt.size * 1.05)) {
                if (mBTNClick && !btnClickStat) {
                    desselectAllClick();
                    opt.devStatus |= 1;
                    opt.devStatus |= 2;
                    btnClickStat = true;
                }
            }

            if (!btnClickStat) {
                if ((opt.devStatus & 1) != 0) {
                    opt.devStatus &= ~1;
                }
            } else {
                if ((opt.devStatus & 1) != 0) {
                    opt.x += mxd;
                    opt.y += myd;

                    if (plusK != 0) {
                        opt.size += CEngine::engine().getDeltaTime() * 0.1;
                    } else {
                        if (minusK != 0) {
                            opt.size -= CEngine::engine().getDeltaTime() * 0.1;
                        }
                    }
                }
            }
            break;

        case deslizant_Select_list:
            textSize = textSizeInScreen(opt.text, opt.size);
            barPosX1 = opt.x + textSize + opt.size;
            barPosX2 = barPosX1 + opt.deslizantBarSize;

            if (isMouseOver2DQuad(barPosX1, opt.y, opt.deslizantBarSize,
                                  opt.size)) {
                if (mBTNClick) {
                }
            }
            break;
        }

        ++i;
    }
}

void CMenu::shortcutCallback(int key, int scancode, int action, int mods) {
    auto keyTest = [key, action](int k) {
        if (key != k) {
            { return 0; }
        }
        return (action == 1 || action == 2) ? key : 0;
    };

    int i = 0;
    for (auto &opt : options) {
        switch (opt.type) {
        case button_ok:

            break;

        case text_input: {
            auto &controls = CControls::controls();

            if ((opt.status & 4) != 0) {
                controls.update();
                int ch = 0;

                {
                    int chtmp = keyTest(GLFW_KEY_BACKSPACE);

                    if (chtmp != 0) {
                        { ch = chtmp; }
                    }
                }

                {
                    int chtmp = keyTest(GLFW_KEY_LEFT);

                    if (chtmp != 0) {
                        --opt.strEditPoint;

                        if (opt.strEditPoint < 0) {
                            { opt.strEditPoint = 0; }
                        }
                    }
                }

                {
                    int chtmp = keyTest(GLFW_KEY_RIGHT);

                    if (chtmp != 0) {
                        ++opt.strEditPoint;

                        int textChars = CFonts::utf8Size(opt.preText);

                        if (opt.strEditPoint > textChars) {
                            { opt.strEditPoint = textChars; }
                        }
                    }
                }

                if (ch != 0) {
                    if (opt.externalPreTextRef != nullptr) {
                        opt.preText = *opt.externalPreTextRef;
                    }

                    if (ch != GLFW_KEY_BACKSPACE) {
                        char utftempbuf[8] = {0};
                        utf8::append(ch, utftempbuf);
                        controls.lastChar = 0;

                        opt.strEditPoint = CFonts::utf8InsertAt(
                            opt.preText, utftempbuf, opt.strEditPoint);
                    } else {
                        if (!opt.preText.empty()) {
                            // CFonts::utf8RemoveLast(opt.preText);
                            CFonts::utf8RemoveAtRange(opt.preText,
                                                      opt.strEditPoint - 1, 1);
                            --opt.strEditPoint;

                            if (opt.strEditPoint < 0) {
                                { opt.strEditPoint = 0; }
                            }
                        }
                    }
                }

                if (opt.externalPreTextRef != nullptr) {
                    *opt.externalPreTextRef = opt.preText;
                }
            }
        } break;

        case textbtn:
            break;

        case deslizant_Select_list:
            if ((opt.status & 1) != 0 && !opt.optList.empty()) {
                if (keyTest(GLFW_KEY_LEFT) != 0) {
                    if (opt.listID > 0) {
                        { opt.listID--; }
                    }
                }

                if (keyTest(GLFW_KEY_RIGHT) != 0) {
                    if (opt.listID < (opt.optList.size() - 1)) {
                        opt.listID++;
                    }
                }
            }
            break;

        case drag_bar:
            break;

        case static_text:
            break;

        default:
            break;
        }
    }
}

auto CMenu::isThisOnTop() const -> bool {
    for (intptr_t i = uiOrderList.size() - 1; i >= 0; i--) {
        CMenu *m = uiList[uiOrderList[i].id].m.get();
        if (m != nullptr) {
            return uiOrderList[i].id == thisUiID;
        }
    }
    return false;
}

auto CMenu::allocOrGetUiFreeSpace() -> int {
    int result = -1;

    for (size_t i = 0, size = uiList.size(); i < size; ++i) {
        if (uiList[i].m == nullptr) {
            result = i;
            break;
        }
    }

    if (result == -1) {
        uiWindowStruct w;
        uiList.push_back(std::move(w));

        result = uiList.size() - 1;
    }

    return result;
}

void CMenu::interfaceCalcPos() {
    if (uiList.size() != uiOrderList.size()) {
        uiOrderList.clear();
        for (size_t i = 0, size = uiList.size(); i < size; ++i) {
            posUiOrder b{};
            b.id = i;
            b.pos = uiList[i].pos;
            uiOrderList.push_back(b);
        }
    }

    std::sort(uiOrderList.begin(), uiOrderList.end());
}

void CMenu::renderUiList() {
    interfaceCalcPos();

    int i = 1;

    for (auto &uiTest : uiOrderList) {
        auto &uiw = uiList[uiTest.id];

        uiw.pos = (++i);
        uiTest.pos = uiw.pos;

        if (uiw.m) {
            uiw.m->render();
        }
    }
}

auto CMenu::pushUserInterface(const CMenu &m) -> int {
    int index = allocOrGetUiFreeSpace();

    uiList[index].m = nullptr;
    uiList[index].m = std::make_unique<CMenu>(m);
    uiList[index].m->uiMenu = true;
    uiList[index].m->thisUiID = index;
    uiList[index].pos = 100;

    myUiList.push_back(index);

    return index;
}

auto CMenu::getDevSelectedMenuOpt() -> size_t {
    for (size_t i = 0, size = options.size(); i < size; ++i) {
        if ((options[i].devStatus & 2) != 0) {
            { return i; }
        }
    }

    return ~static_cast<size_t>(0UL);
}

CMenu::CMenu() {
    lastMouseClickStatus = false;
    lastEnterOptBtn = false;
    status = 0;
    thisUiID = -1;

    devEditingOpt = 0;

    devMenuNOUpdateOthers = false;

    ++menusCreated;
    lScript = nullptr;

    menuName = "menu_main_";
    menuName += std::to_string(menusCreated);

    // Menus[menuName] = this;

    temp = false;

    gameMenu = false;
    devEditMenu = nullptr;
    uiMenu = false;

    x = y = 0.0;

    mouseAX = CEngine::engine().mouseX;
    mouseAY = CEngine::engine().mouseY;
}

CMenu::CMenu(const std::string &name) {
    /*{
            auto m = Menus.find(name);

            if (m != Menus.end())
            {
                    throw std::runtime_error(CLog::log().multiRegister("Error
    menu %0 already exists", name));
            }
    }*/

    lastEnterOptBtn = false;
    status = 0;
    thisUiID = -1;

    devEditingOpt = 0;

    ++menusCreated;
    lScript = nullptr;
    devEditMenu = nullptr;

    devMenuNOUpdateOthers = false;

    menuName = name;

    // Menus[menuName] = this;

    temp = false;

    gameMenu = false;
    uiMenu = false;

    x = y = 0.0;

    mouseAX = CEngine::engine().mouseX;
    mouseAY = CEngine::engine().mouseY;
}

CMenu::~CMenu() {
    // Menus.erase(menuName);
}
