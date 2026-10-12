#include "CFonts.h"
#include "CMenu.h"
#include "GPPGame.h"
#include <climits>
#include <gtest/gtest.h>
#include <limits>

extern int _glfwReadTGA(CEngine::GLFWstream *, CEngine::GLFWimage *, int);

TEST(FontCPU, TruncatedAndOversizedTgaPacketsAreRejected) {
    const auto decode = [](const std::vector<unsigned char> &bytes) {
        CEngine::GLFWstream stream{};
        stream.size = static_cast<long>(bytes.size());
        stream.data = std::make_unique<unsigned char[]>(bytes.size());
        std::copy(bytes.begin(), bytes.end(), stream.data.get());
        CEngine::GLFWimage image;
        return _glfwReadTGA(&stream, &image, 0);
    };

    EXPECT_EQ(decode(std::vector<unsigned char>(17, 0)), 0);
    std::vector<unsigned char> bytes(18, 0);
    bytes[2] = 10; // RLE true color, one RGBA pixel.
    bytes[12] = bytes[14] = 1;
    bytes[16] = 32;
    EXPECT_EQ(decode(bytes), 0);
    bytes.insert(bytes.end(),
                 {0x81, 0, 0, 0, 255}); // Two pixels exceed output.
    EXPECT_EQ(decode(bytes), 0);
    bytes[18] = 0x80;
    EXPECT_NE(decode(bytes), 0);
    bytes.pop_back();
    EXPECT_EQ(decode(bytes), 0);
    bytes[2] = 2; // Uncompressed image must also contain a complete pixel.
    bytes.resize(21);
    EXPECT_EQ(decode(bytes), 0);
}

TEST(FontCPU, AlphaBoundsRespectRectangularCellsAndOrientation) {
    std::vector<unsigned char> image(12 * 8 * 4, 0);
    // Top row, second 6x4 cell; two isolated ink pixels.
    image[((4 + 1) * 12 + 6 + 2) * 4 + 3] = 255;
    image[((4 + 3) * 12 + 6 + 5) * 4 + 3] = 1;
    CFonts::ImageView view{image.data(), image.size(), 12, 8, 4, 3};
    CFonts::InkBounds bounds;
    ASSERT_TRUE(CFonts::analyzeCell(view, {2, 2}, 1, 0, bounds));
    EXPECT_TRUE(bounds.visible);
    EXPECT_EQ(bounds.left, 2);
    EXPECT_EQ(bounds.right, 6);
    EXPECT_EQ(bounds.bottom, 1);
    EXPECT_EQ(bounds.top, 4);
    ASSERT_TRUE(CFonts::analyzeCell(view, {2, 2}, 0, 1, bounds));
    EXPECT_FALSE(bounds.visible);
    EXPECT_EQ(bounds.right, 0);
    EXPECT_FALSE(CFonts::analyzeCell(view, {0, 2}, 0, 0, bounds));
    EXPECT_FALSE(CFonts::analyzeCell(view, {5, 2}, 0, 0, bounds));
    EXPECT_FALSE(CFonts::analyzeCell(view, {2, 2}, 2, 0, bounds));
    view.bytes--;
    EXPECT_FALSE(CFonts::analyzeCell(view, {2, 2}, 0, 0, bounds));
}

TEST(FontCPU, UTF8EditingUsesScalarOffsetsAndSurvivesReallocation) {
    std::string text = "aé𝄞z";
    EXPECT_EQ(CFonts::utf8Size(text), 4u);
    EXPECT_EQ(CFonts::utf8InsertAt(text, "ç", 2), 3u);
    EXPECT_EQ(text, "aéç𝄞z");
    const std::string large(5000, 'x');
    EXPECT_EQ(CFonts::utf8InsertAt(text, large, 1), 5001u);
    CFonts::utf8RemoveAtRange(text, 1, 5000);
    EXPECT_EQ(text, "aéç𝄞z");
    CFonts::utf8RemoveAtRange(text, 1, 2);
    EXPECT_EQ(text, "a𝄞z");
    CFonts::utf8RemoveLast(text);
    EXPECT_EQ(text, "a𝄞");
    CFonts::utf8RemoveLast(text);
    EXPECT_EQ(text, "a");
    CFonts::utf8RemoveLast(text);
    CFonts::utf8RemoveLast(text);
    EXPECT_TRUE(text.empty());
    text = "é";
    EXPECT_EQ(CFonts::utf8InsertAt(text, text, 1), 2u);
    EXPECT_EQ(text, "éé");
    CFonts::utf8RemoveAtRange(text, 0, 0);
    EXPECT_EQ(text, "éé");
    CFonts::utf8RemoveAtRange(text, INT_MAX, INT_MAX);
    EXPECT_EQ(text, "éé");
    CFonts::utf8RemoveAtRange(text, 1, INT_MAX);
    EXPECT_EQ(text, "é");
}

TEST(FontCPU, InvalidUTF8NeverPartiallyMutatesInput) {
    for (const std::string bad : {std::string("\xff"), std::string("\xc3"),
                                  std::string("\xed\xa0\x80")}) {
        std::string text = "aé";
        EXPECT_EQ(CFonts::utf8InsertAt(text, bad, 1), std::string::npos);
        EXPECT_EQ(text, "aé");
        text = bad;
        EXPECT_EQ(CFonts::utf8Size(text), std::string::npos);
        CFonts::utf8RemoveLast(text);
        CFonts::utf8RemoveAtRange(text, 0, 1);
        EXPECT_EQ(text, bad);
    }
}

TEST(FontCPU, CursorChoosesNearestBoundary) {
    CFonts::TextLayout layout;
    layout.carets = {0, .2, 1.3, 1.6};
    EXPECT_EQ(layout.cursorAt(-10), 0u);
    EXPECT_EQ(layout.cursorAt(.3), 1u);
    EXPECT_EQ(layout.cursorAt(1), 2u);
    EXPECT_EQ(layout.cursorAt(10), 3u);
}

namespace {
CControls::MenuEvent key(int code, int action = GLFW_PRESS) {
    CControls::MenuEvent e;
    e.key = code;
    e.action = action;
    return e;
}

CControls::MenuEvent mouse(double x, double y, int action) {
    CControls::MenuEvent e;
    e.kind = CControls::MenuEvent::Mouse;
    e.key = GLFW_MOUSE_BUTTON_LEFT;
    e.x = x;
    e.y = y;
    e.action = action;
    return e;
}

CMenu::menuOpt button(std::string text = "Test") {
    CMenu::menuOpt o;
    o.text = std::move(text);
    o.type = CMenu::textbtn;
    o.styleOverrides.textSize = .1;
    return o;
}
} // namespace

TEST(MenuCPU, KeyboardActivationIsAnEdgeAndSkipsDisabledItems) {
    CMenu menu;
    menu.addOpt(button());
    auto disabled = button();
    disabled.disabled = true;
    menu.addOpt(disabled);
    menu.addOpt(button());
    menu.select(0);
    menu.processEvent(key(GLFW_KEY_DOWN));
    EXPECT_EQ(menu.focusedOption(), 2);
    menu.processEvent(key(GLFW_KEY_ENTER, GLFW_REPEAT));
    EXPECT_EQ(menu.options[2].status & 2, 0);
    menu.processEvent(key(GLFW_KEY_ENTER));
    EXPECT_EQ(menu.options[2].status & 3, 3);
    menu.resetBtns();
    EXPECT_EQ(menu.options[2].status & 2, 0);
}

TEST(MenuCPU, ClickRequiresPressAndReleaseOnSameTarget) {
    CMenu menu;
    menu.addOpt(button());
    const auto r = menu.optionRect(0);
    const double x = (r.left + r.right) / 2, y = (r.bottom + r.top) / 2;
    menu.processEvent(mouse(x, y, GLFW_RELEASE));
    EXPECT_EQ(menu.options[0].status & 2, 0);
    menu.processEvent(mouse(x, y, GLFW_PRESS));
    EXPECT_EQ(menu.options[0].status & 2, 0);
    menu.processEvent(mouse(x, y, GLFW_RELEASE));
    EXPECT_EQ(menu.options[0].status & 3, 3);
    menu.resetBtns();
    menu.processEvent(mouse(x, y, GLFW_PRESS));
    menu.processEvent(mouse(100, 100, GLFW_RELEASE));
    EXPECT_EQ(menu.options[0].status & 2, 0);
}

TEST(MenuCPU, ActivationSurvivesFocusChangeUntilNextUpdate) {
    CMenu menu;
    menu.addOpt(button());
    menu.addOpt(button());
    menu.select(0);
    menu.processEvent(key(GLFW_KEY_ENTER));
    menu.select(1);
    EXPECT_EQ(menu.focusedOption(), 1);
    EXPECT_EQ(menu.options[0].status & 3, 3);
    menu.update();
    EXPECT_EQ(menu.options[0].status & 3, 0);
    EXPECT_EQ(menu.options[1].status & 3, 1);
}

TEST(MenuCPU, EmptyListsAndZeroWidthAreSafe) {
    CMenu menu;
    auto o = button();
    o.type = CMenu::deslizant_Select_list;
    o.listID = INT_MAX;
    o.deslizantBarSize = 0;
    menu.addOpt(o);
    menu.select(0);
    menu.processEvent(key(GLFW_KEY_LEFT));
    menu.optionRect(0);
    EXPECT_EQ(menu.options[0].listID, 0);
    menu.options[0].optList = {"one"};
    menu.options[0].listID = -42;
    menu.optionRect(0);
    EXPECT_EQ(menu.options[0].listID, 0);
    menu.options[0].listID = INT_MAX;
    menu.processEvent(key(GLFW_KEY_RIGHT));
    EXPECT_EQ(menu.options[0].listID, 0);
    menu.options[0].listID = INT_MIN;
    menu.processEvent(key(GLFW_KEY_LEFT));
    EXPECT_EQ(menu.options[0].listID, 0);
}

TEST(MenuCPU, OrderedCharactersRespectByteBudgetWithoutSplittingUnicode) {
    CMenu menu;
    auto o = button();
    o.type = CMenu::text_input;
    o.preText = "";
    o.preTextMaxSize = 4;
    menu.addOpt(o);
    menu.select(0);
    menu.processEvent(key(GLFW_KEY_ENTER));
    CControls::MenuEvent e;
    e.kind = CControls::MenuEvent::Character;
    e.codepoint = 0xe9;
    menu.processEvent(e);
    menu.processEvent(e);
    menu.processEvent(e);
    EXPECT_EQ(menu.options[0].preText, "éé");
    EXPECT_EQ(menu.options[0].strEditPoint, 2);
    menu.processEvent(key(GLFW_KEY_BACKSPACE));
    EXPECT_EQ(menu.options[0].preText, "é");
}

TEST(MenuCPU, ModalConsumesInputAndCloneDoesNotCopyBindingsOrChildren) {
    CMenu root;
    root.addOpt(button());
    CMenu model;
    model.modal = true;
    auto o = button();
    o.goback = true;
    std::string external = "safe";
    o.externalPreTextRef = &external;
    model.addOpt(o);
    const int id = root.pushUserInterface(model);
    auto *child = CMenu::getUiAt(static_cast<size_t>(id)).m.get();
    ASSERT_NE(child, nullptr);
    EXPECT_EQ(child->options[0].externalPreTextRef, nullptr);
    CMenu copy(root);
    EXPECT_EQ(copy.getUIListSize(), 0);
    auto &controls = CControls::controls();
    controls.menuInputActive = true;
    controls.queueMenuEvent(key(GLFW_KEY_ENTER));
    root.update();
    EXPECT_EQ(root.options[0].status & 2, 0);
    EXPECT_EQ(root.getUIListSize(), 0);
    controls.resetMenuInput();
    controls.menuInputActive = false;
}

TEST(MenuCPU, RepeatDoesNotCatchUpAfterStall) {
    bool previous = false;
    double next = 0;
    EXPECT_TRUE(CControls::menuRepeat(true, previous, next, 1, true));
    EXPECT_FALSE(CControls::menuRepeat(true, previous, next, 1.1, true));
    EXPECT_TRUE(CControls::menuRepeat(true, previous, next, 5, true));
    EXPECT_FALSE(CControls::menuRepeat(true, previous, next, 5, true));
    EXPECT_FALSE(CControls::menuRepeat(true, previous, next, 10, false));
}

TEST(MenuCPU, NamedMenuLookupDoesNotCreateOrReplace) {
    auto &game = GPPGame::GuitarPP();
    EXPECT_EQ(game.menus.findMenu("ui-missing-test"), nullptr);
    EXPECT_THROW(game.menus.getMenuByName("ui-missing-test"),
                 std::out_of_range);
    auto &menu = game.menus.newNamedMenu("ui-existing-test");
    menu.addOpt(button());
    EXPECT_EQ(&game.menus.newNamedMenu("ui-existing-test"), &menu);
    EXPECT_EQ(menu.options.size(), 1u);
}

TEST(MenuCPU, LuaCallbacksCanAppendWithoutInvalidatingDispatch) {
    auto &lua = CLuaH::Lua();
    auto source = lua.newScriptR(R"(
        calls=0
        local menu=newMenu('ui-lua-test')
        assert(menu=='ui-lua-test')
        assert(getNumOfMenuOptions('missing-lua-menu')==false)
        assert(getMenuOptionName(menu,-1)==false)
        newMenuOption(menu,'Lua button',0,0,0.1,1,3,function(m,n,v,a)
            if a==3 then
                calls=calls+1
                newMenuOption(m,'Added',0,-0.2,0.1,1,3,function() end)
            end
        end)
    )",
                                 "ui-test");
    auto &script = lua.files["ui-tests"]["callbacks"];
    script = std::move(source);
    ASSERT_EQ(CLuaH::runScript(script), 0);
    auto *menu = GPPGame::GuitarPP().menus.findMenu("ui-lua-test");
    ASSERT_NE(menu, nullptr);
    auto &controls = CControls::controls();
    controls.menuInputActive = true;
    controls.queueMenuEvent(key(GLFW_KEY_ENTER));
    menu->update();
    EXPECT_EQ(menu->options.size(), 2u);
    lua_getglobal(script.luaState.get(), "calls");
    EXPECT_EQ(lua_tointeger(script.luaState.get(), -1), 1);
    lua_pop(script.luaState.get(), 1);
    menu->update();
    EXPECT_EQ(menu->options.size(), 2u);
    controls.resetMenuInput();
    controls.menuInputActive = false;
    lua.files.erase("ui-tests");
}

TEST(MenuCPU, FocusLossCancelsMouseCapture) {
    CMenu menu;
    menu.addOpt(button());
    const auto r = menu.optionRect(0);
    const double x = (r.left + r.right) / 2, y = (r.bottom + r.top) / 2;
    menu.processEvent(mouse(x, y, GLFW_PRESS));
    CControls::MenuEvent cancel;
    cancel.kind = CControls::MenuEvent::Focus;
    cancel.action = 0;
    menu.processEvent(cancel);
    menu.processEvent(mouse(x, y, GLFW_RELEASE));
    EXPECT_EQ(menu.options[0].status & 2, 0);
}

TEST(MenuCPU, OverflowPublishesCancellation) {
    auto &controls = CControls::controls();
    controls.menuInputActive = true;
    controls.resetMenuInput();
    for (int i = 0; i < 4097; ++i) {
        controls.queueMenuEvent(key(GLFW_KEY_UP));
    }

    ASSERT_EQ(controls.menuEvents.size(), 1u);
    EXPECT_EQ(controls.menuEvents.front().kind, CControls::MenuEvent::Focus);
    controls.resetMenuInput();
    controls.menuInputActive = false;
}

namespace {
class MenuStyleCPU : public ::testing::Test {
    ui::Style previous = ui::activeStyle();

  protected:
    void SetUp() override { ui::setActiveStyle(ui::Style{}); }

    void TearDown() override { ui::setActiveStyle(previous); }
};
} // namespace

TEST_F(MenuStyleCPU, OpenMenusAndClonesInheritPerProperty) {
    CMenu root, model;
    model.modal = true;
    model.styleOverrides.panel = ui::Color{.2, .3, .4, 1};
    CMenu::menuOpt option;
    option.type = CMenu::text_input;
    option.preText = "preserved";
    model.addOpt(option);
    model.select(0);
    const int id = root.pushUserInterface(model);
    auto &child = *CMenu::getUiAt(static_cast<size_t>(id)).m;
    CMenu copy(model);
    child.select(0);
    child.options[0].listID = 2;
    auto style = ui::activeStyle();
    style.typography.textSize = .15;
    style.typography.textColor = {.1, .8, .3, 1};
    style.panel.background = {.9, .9, .9, 1};
    ui::setActiveStyle(style);

    for (const auto *menu : {&model, &child, &copy}) {
        EXPECT_DOUBLE_EQ(menu->resolvedOptionStyle(menu->options[0]).textSize,
                         .15);
        EXPECT_DOUBLE_EQ(
            menu->resolvedOptionStyle(menu->options[0]).textColor.g, .8);
        EXPECT_DOUBLE_EQ(menu->resolvedStyle().panel.background.r, .2);
    }

    EXPECT_EQ(child.focusedOption(), 0);
    EXPECT_EQ(child.options[0].preText, "preserved");
    EXPECT_EQ(child.options[0].listID, 2);
    child.styleOverrides.textSize = .2;
    child.options[0].styleOverrides.textSize = .3;
    EXPECT_DOUBLE_EQ(child.resolvedOptionStyle(child.options[0]).textSize, .3);
    child.options[0].styleOverrides.textSize.reset();
    EXPECT_DOUBLE_EQ(child.resolvedOptionStyle(child.options[0]).textSize, .2);
    child.styleOverrides.textSize.reset();
    child.styleOverrides.panel.reset();
    EXPECT_DOUBLE_EQ(child.resolvedOptionStyle(child.options[0]).textSize, .15);
    EXPECT_DOUBLE_EQ(child.resolvedStyle().panel.background.r, .9);
    EXPECT_FALSE(child.options[0].styleOverrides.textSize.has_value());
}

TEST_F(MenuStyleCPU, InvalidSizesFallBackWithoutFreezingInheritance) {
    CMenu menu;
    CMenu::menuOpt option;
    menu.styleOverrides.textSize = .12;
    for (double invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
        option.styleOverrides.textSize = invalid;
        EXPECT_DOUBLE_EQ(menu.resolvedOptionStyle(option).textSize, .12);
        menu.styleOverrides.textSize = invalid;
        EXPECT_DOUBLE_EQ(menu.resolvedOptionStyle(option).textSize, .075);
        menu.styleOverrides.textSize = .12;
    }

    ui::Style style;
    style.typography.textSize = 0;
    ui::setActiveStyle(style);
    EXPECT_DOUBLE_EQ(ui::activeStyle().typography.textSize, .075);
}

TEST_F(MenuStyleCPU, GeometryAndMouseCursorUseInheritedSize) {
    CMenu menu;
    CMenu::menuOpt option;
    option.type = CMenu::text_input;
    option.preText = "abcd";
    menu.addOpt(option);
    auto before = menu.optionRect(0);
    EXPECT_DOUBLE_EQ(before.top, .075 * 1.08);
    auto style = ui::activeStyle();
    style.typography.textSize = .2;
    style.layout.padding = .1;
    ui::setActiveStyle(style);
    auto after = menu.optionRect(0);
    EXPECT_DOUBLE_EQ(after.top, .2 * 1.1);
    EXPECT_DOUBLE_EQ(after.left, -.2 * .1);
    const auto &carets = menu.options[0].layout.carets;
    ASSERT_GT(carets.size(), 2u);
    menu.processEvent(mouse(carets[2] * .2, .15, GLFW_PRESS));
    EXPECT_EQ(menu.focusedOption(), 0);
    EXPECT_EQ(menu.options[0].strEditPoint, 2);
    EXPECT_FALSE(menu.options[0].styleOverrides.textSize.has_value());
}

TEST_F(MenuStyleCPU, FlowAnchorsAndControlsFollowSizeWithoutRelayoutOfGlyphs) {
    CMenu menu;
    CMenu::menuOpt option;
    option.text = "Anchored";
    option.centerX = .3;
    option.type = CMenu::textbtn;
    menu.addOpt(option);
    menu.reflow();
    const auto oldRevision = menu.options[0].cachedRevision;
    const auto oldCarets = menu.options[0].layout.carets;
    auto style = ui::activeStyle();
    style.typography.textSize = .15;
    style.layout.rowStep = .5;
    ui::setActiveStyle(style);
    const auto centered = menu.optionRect(0);
    EXPECT_NEAR((centered.left + centered.right) / 2, .3, 1e-12);
    EXPECT_EQ(menu.options[0].cachedRevision, oldRevision);
    EXPECT_EQ(menu.options[0].layout.carets, oldCarets);
    option.centerX.reset();
    option.type = CMenu::deslizant_Select_list;
    option.deslizantBarSize = .5;
    option.optList = {"low", "high"};
    menu.addOpt(option);
    menu.setFlow(CMenu::Flow::Vertical, -.4, .5, -1);
    menu.reflow();
    EXPECT_DOUBLE_EQ(menu.options[0].x, -.4);
    EXPECT_DOUBLE_EQ(menu.options[1].y, 0);
    auto track = menu.controlRect(1);
    EXPECT_DOUBLE_EQ(track.top - track.bottom, .15);
    EXPECT_NEAR(track.right - track.left, .5, 1e-12);
    menu.select(1);
    menu.processEvent(key(GLFW_KEY_RIGHT));
    EXPECT_EQ(menu.options[1].listID, 1);
    auto &engine = CEngine::engine();
    const double oldWidth = engine.windowWidth, oldHeight = engine.windowHeight;
    engine.windowWidth = 800;
    engine.windowHeight = 600;
    menu.options[1].anchorRight = true;
    menu.options[1].manualPosition = true;
    menu.reflow();
    EXPECT_NEAR(menu.options[1].x + menu.options[1].layout.right * .15,
                800.0 / 600 - .1, 1e-12);
    engine.windowWidth = oldWidth;
    engine.windowHeight = oldHeight;
}

TEST_F(MenuStyleCPU, LuaNilSizeInheritsAndNumericSizeOverrides) {
    auto &lua = CLuaH::Lua();
    auto script = lua.newScriptR(R"(
        local m = newMenu('ui-style-lua-test')
        newMenuOption(m, 'Inherited', 0, 0, nil, 1, 3, function() end)
        newMenuOption(m, 'Explicit', 0, 0, 0.1, 1, 3, function() end)
        assert(newMenuOption(m, 'Invalid', 0, 0, -1, 1, 3, function() end)==false)
    )",
                                 "style-test");
    // Callback registration needs a script owned by the Lua registry.
    lua.files["ui-style-tests"]["options"] = std::move(script);
    auto &registered = lua.files["ui-style-tests"]["options"];
    ASSERT_EQ(CLuaH::runScript(registered), 0);
    auto *menu = GPPGame::GuitarPP().menus.findMenu("ui-style-lua-test");
    ASSERT_NE(menu, nullptr);
    ASSERT_EQ(menu->options.size(), 2u);
    auto style = ui::activeStyle();
    style.typography.textSize = .2;
    ui::setActiveStyle(style);
    EXPECT_DOUBLE_EQ(menu->resolvedOptionStyle(menu->options[0]).textSize, .2);
    EXPECT_DOUBLE_EQ(menu->resolvedOptionStyle(menu->options[1]).textSize, .1);
    lua.files.erase("ui-style-tests");
}

TEST_F(MenuStyleCPU, MetricChangesRevealFocusButScrollingAndColorsDoNot) {
    CMenu menu;
    menu.viewportEnabled = true;
    menu.viewport = {-1, -.3, 1, .3};
    menu.setFlow(CMenu::Flow::Vertical, -.5, .2);
    for (int i = 0; i < 10; ++i) {
        CMenu::menuOpt option;
        option.text = "Option";
        option.type = CMenu::textbtn;
        option.group = 1;
        menu.addOpt(option);
    }

    menu.select(8);
    auto style = ui::activeStyle();
    style.layout.rowStep = .2;
    ui::setActiveStyle(style);
    menu.update();
    const auto visible = menu.optionRect(8);
    EXPECT_GE(visible.bottom, menu.viewport.bottom - 1e-12);
    EXPECT_LE(visible.top, menu.viewport.top + 1e-12);
    EXPECT_EQ(menu.focusedOption(), 8);

    CControls::MenuEvent scroll;
    scroll.kind = CControls::MenuEvent::Scroll;
    scroll.y = 10;
    menu.processEvent(scroll);
    const double manualOffset = menu.scrollOffset;
    ASSERT_FALSE(menu.optionRect(8).intersects(menu.viewport));
    style.typography.textColor = {1, 0, 0, 1};
    ui::setActiveStyle(style);
    menu.update();
    EXPECT_DOUBLE_EQ(menu.scrollOffset, manualOffset);
    EXPECT_FALSE(menu.optionRect(8).intersects(menu.viewport));

    menu.options[8].styleOverrides.textSize = .18;
    const auto resized = menu.optionRect(8);
    EXPECT_GE(resized.bottom, menu.viewport.bottom - 1e-12);
    EXPECT_LE(resized.top, menu.viewport.top + 1e-12);
    // Stable metrics leave a later manual scroll alone as well.
    menu.processEvent(scroll);
    const double nextOffset = menu.scrollOffset;
    menu.reflow();
    EXPECT_DOUBLE_EQ(menu.scrollOffset, nextOffset);
}

TEST_F(MenuStyleCPU, ManualEditingKeepsOtherFlowItemsInPlace) {
    CMenu menu;
    menu.setFlow(CMenu::Flow::Vertical, -.5, .4);
    for (int i = 0; i < 3; ++i) {
        CMenu::menuOpt option;
        option.text = "Audio";
        option.type = CMenu::textbtn;
        option.group = 1;
        option.centerX = 0;
        menu.addOpt(option);
    }

    CMenu::menuOpt anchored;
    anchored.text = "Separate";
    anchored.type = CMenu::static_text;
    anchored.centerX = .6;
    anchored.group = 2;
    menu.addOpt(anchored);
    menu.reflow();
    const auto untouched = menu.optionRect(1);
    const auto separate = menu.optionRect(3);
    const double editedX = menu.options[0].x;
    menu.beginManualEdit(0);
    menu.options[0].x += .1;
    menu.reflow();
    EXPECT_EQ(menu.flow, CMenu::Flow::Manual);
    EXPECT_DOUBLE_EQ(menu.options[0].x, editedX + .1);
    EXPECT_DOUBLE_EQ(menu.optionRect(1).left, untouched.left);
    EXPECT_DOUBLE_EQ(menu.optionRect(1).top, untouched.top);
    EXPECT_DOUBLE_EQ(menu.optionRect(3).left, separate.left);
    EXPECT_TRUE(menu.options[3].centerX.has_value());
    EXPECT_FALSE(menu.options[1].centerX.has_value());
    // Starting another drag must also preserve the first edit.
    menu.beginManualEdit(1);
    menu.options[1].y -= .1;
    menu.reflow();
    EXPECT_DOUBLE_EQ(menu.options[0].x, editedX + .1);
    EXPECT_DOUBLE_EQ(menu.options[1].x, -.5);
}
