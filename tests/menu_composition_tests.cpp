#include "CMenu.h"
#include "CUIDefinition.h"
#include "CUIOverrides.h"
#include <cmath>
#include <gtest/gtest.h>
#include <limits>

namespace {
class MenuCompositionCPU : public ::testing::Test {
  protected:
    CFonts fonts;
    CMenu::Context context;
    CMenu root{"composition", &context};
    void SetUp() override {
        context.fonts = &fonts;
        context.mouseX = context.mouseY = 10;
        root.addOpt(button());
        root.update();
    }
    static CMenu::menuOpt button() {
        CMenu::menuOpt o;
        o.type = CMenu::textbtn;
        o.text = "Button";
        o.width = .4;
        o.height = .1;
        o.group = 1;
        return o;
    }
    CMenu &addPanel(double x, int count = 20) {
        CMenu model("panel", &context);
        model.x = x;
        model.viewportEnabled = true;
        model.viewport = {0, -.4, .8, .4};
        model.setFlow(CMenu::Flow::Vertical, .1, .2);
        for (int i = 0; i < count; ++i) {
            model.addOpt(button());
        }
        return *CMenu::getUiAt(
                    static_cast<size_t>(root.pushPanel(model, model.viewport)))
                    .m;
    }
    void key(int code, int mods = 0) {
        CControls::MenuEvent event;
        event.key = code;
        event.mods = mods;
        event.action = GLFW_PRESS;
        context.events.push_back(event);
        root.update();
    }
    void mouse(double x, double y, int action) {
        context.mouseX = x;
        context.mouseY = y;
        CControls::MenuEvent event;
        event.kind = CControls::MenuEvent::Mouse;
        event.key = GLFW_MOUSE_BUTTON_LEFT;
        event.x = x;
        event.y = y;
        event.action = action;
        context.events.push_back(event);
        root.update();
    }
    void scroll(double x, double y, double amount) {
        context.mouseX = x;
        context.mouseY = y;
        CControls::MenuEvent event;
        event.kind = CControls::MenuEvent::Scroll;
        event.y = amount;
        context.events.push_back(event);
        root.update();
    }
};
} // namespace

TEST_F(MenuCompositionCPU, PanelsDoNotStealFocusAndTabPreservesLocalSelection) {
    auto &left = addPanel(-1);
    auto &right = addPanel(.1);
    auto &empty = addPanel(0, 0);
    empty.addOpt(button());
    empty.options[0].disabled = true;
    EXPECT_EQ(root.focusedInterface(), &root);
    key(GLFW_KEY_TAB);
    EXPECT_EQ(root.focusedInterface(), &left);
    key(GLFW_KEY_DOWN);
    EXPECT_EQ(left.focusedOption(), 1);
    key(GLFW_KEY_TAB);
    EXPECT_EQ(root.focusedInterface(), &right);
    key(GLFW_KEY_TAB);
    EXPECT_EQ(root.focusedInterface(), &root);
    key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    EXPECT_EQ(root.focusedInterface(), &right);
    key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    EXPECT_EQ(root.focusedInterface(), &left);
    EXPECT_EQ(left.focusedOption(), 1);
    EXPECT_FALSE(root.focusInterface(empty));
}

TEST_F(MenuCompositionCPU, WheelTargetsEmptyViewportSpaceWithoutMovingFocus) {
    auto &left = addPanel(-1);
    auto &right = addPanel(.1);
    root.focusInterface(left);
    scroll(.85, 0, -1);
    EXPECT_GT(right.scrollOffset, 0);
    EXPECT_DOUBLE_EQ(left.scrollOffset, 0);
    EXPECT_EQ(root.focusedInterface(), &left);
    scroll(-.25, 0, -1);
    EXPECT_GT(left.scrollOffset, 0);
    const double leftScroll = left.scrollOffset;
    scroll(10, 10, -1);
    EXPECT_DOUBLE_EQ(left.scrollOffset, leftScroll);
}

TEST_F(MenuCompositionCPU, OffscreenOptionsDoNotInterceptUnderlyingClicks) {
    auto &left = addPanel(-1);
    left.options[0].manualPosition = true;
    left.options[0].x = 1;
    left.options[0].y = 0;
    mouse(.2, .05, GLFW_PRESS);
    mouse(.2, .05, GLFW_RELEASE);
    EXPECT_EQ(root.options[0].status & 3, 3);
    EXPECT_EQ(left.options[0].status & 2, 0);
}

TEST_F(MenuCompositionCPU,
       CaptureSurvivesFocusTransferAndCancelsAcrossTargets) {
    auto &left = addPanel(-1);
    auto &right = addPanel(.1);
    mouse(-.8, .25, GLFW_PRESS);
    ASSERT_TRUE(root.focusInterface(right));
    mouse(-.8, .25, GLFW_RELEASE);
    EXPECT_EQ(left.options[0].status & 3, 3);
    EXPECT_EQ(right.options[0].status & 2, 0);
    mouse(-.8, .25, GLFW_PRESS);
    mouse(.3, .25, GLFW_RELEASE);
    EXPECT_EQ(left.options[0].status & 2, 0);
    EXPECT_EQ(right.options[0].status & 2, 0);
}

TEST_F(MenuCompositionCPU,
       FocusLossCancelsCaptureWithoutLosingActiveSelection) {
    auto &left = addPanel(-1);
    left.options[0].active = true;
    mouse(-.8, .25, GLFW_PRESS);
    CControls::MenuEvent event;
    event.kind = CControls::MenuEvent::Focus;
    context.events.push_back(event);
    root.update();
    mouse(-.8, .25, GLFW_RELEASE);
    EXPECT_EQ(left.options[0].status & 2, 0);
    EXPECT_TRUE(left.options[0].active);
}

TEST_F(MenuCompositionCPU, ModalBlocksLatePanelsAndRestoresFocusWhenClosed) {
    auto &left = addPanel(-1);
    root.focusInterface(left);
    mouse(-.8, .25, GLFW_PRESS);
    CMenu model("modal", &context);
    model.modal = true;
    auto close = button();
    close.goback = true;
    model.addOpt(close);
    const int id = root.pushUserInterface(model);
    auto &modal = *CMenu::getUiAt(static_cast<size_t>(id)).m;
    auto &late = addPanel(.1);
    EXPECT_EQ(CMenu::getUiMenuOnTop(), &modal);
    EXPECT_FALSE(root.focusInterface(late));
    scroll(-.2, 0, -2);
    EXPECT_DOUBLE_EQ(left.scrollOffset, 0);
    mouse(-.8, .25, GLFW_RELEASE);
    EXPECT_EQ(left.options[0].status & 2, 0);
    key(GLFW_KEY_ENTER);
    EXPECT_FALSE(CMenu::getUiAt(static_cast<size_t>(id)).m);
    EXPECT_EQ(root.focusedInterface(), &left);
}

TEST_F(MenuCompositionCPU, NestedModalReturnsToItsOwningPanel) {
    auto &left = addPanel(-1);
    root.focusInterface(left);
    CMenu model("modal", &context);
    model.modal = true;
    auto close = button();
    close.goback = true;
    model.addOpt(close);
    const int outerId = root.pushUserInterface(model);
    auto &outer = *CMenu::getUiAt(static_cast<size_t>(outerId)).m;
    CMenu panelModel("inside", &context);
    panelModel.addOpt(button());
    auto &inside =
        *CMenu::getUiAt(
             static_cast<size_t>(outer.pushPanel(panelModel, {-1, -1, 1, 1})))
             .m;
    ASSERT_TRUE(root.focusInterface(inside));
    const int innerId = inside.pushUserInterface(model);
    key(GLFW_KEY_ENTER);
    EXPECT_FALSE(CMenu::getUiAt(static_cast<size_t>(innerId)).m);
    EXPECT_EQ(root.focusedInterface(), &inside);
    EXPECT_FALSE(root.focusInterface(left));
    root.focusInterface(outer);
    key(GLFW_KEY_ENTER);
    EXPECT_FALSE(CMenu::getUiAt(static_cast<size_t>(outerId)).m);
    EXPECT_EQ(root.focusedInterface(), &left);
}

TEST_F(MenuCompositionCPU, PanelRebuildAndClonesLeaveSiblingsAndBindingsAlone) {
    auto &left = addPanel(-1);
    auto &right = addPanel(.1);
    root.focusInterface(left);
    right.select(8);
    const double offset = right.scrollOffset;
    left.clearOptions();
    left.addOpt(button());
    root.update();
    EXPECT_EQ(root.focusedInterface(), &left);
    EXPECT_EQ(right.focusedOption(), 8);
    EXPECT_DOUBLE_EQ(right.scrollOffset, offset);
    CMenu copy(root);
    EXPECT_EQ(copy.getUIListSize(), 0);
    root.closeInterfaces();
    root.update();
    EXPECT_EQ(root.focusedInterface(), &root);
}

TEST_F(MenuCompositionCPU, IndependentRootsCannotRouteToEachOthersPanels) {
    auto &left = addPanel(-1);
    CMenu other("other", &context);
    CMenu model("foreign", &context);
    model.modal = true;
    model.addOpt(button());
    auto &foreign =
        *CMenu::getUiAt(static_cast<size_t>(other.pushUserInterface(model))).m;
    EXPECT_FALSE(root.focusInterface(foreign));
    ASSERT_TRUE(root.focusInterface(left));
    key(GLFW_KEY_ENTER);
    EXPECT_EQ(left.options[0].status & 3, 3);
    EXPECT_EQ(foreign.options[0].status & 2, 0);
}

TEST_F(MenuCompositionCPU, TabLeavesEditingButLegacyMenusStillVisitItems) {
    root.options[0].type = CMenu::text_input;
    auto &left = addPanel(-1);
    key(GLFW_KEY_ENTER);
    ASSERT_NE(root.options[0].status & 4, 0);
    key(GLFW_KEY_TAB);
    EXPECT_EQ(root.options[0].status & 4, 0);
    EXPECT_EQ(root.focusedInterface(), &left);
    root.closeInterfaces();
    root.options[0].type = CMenu::textbtn;
    root.addOpt(button());
    key(GLFW_KEY_TAB);
    EXPECT_EQ(root.focusedOption(), 1);
}

TEST_F(MenuCompositionCPU, TableRowsUseSharedColumnsForHitboxesAndFlow) {
    CMenu table("table", &context);
    table.tableColumns = {.3, .6};
    table.styleOverrides.rowStep = .2;
    table.setFlow(CMenu::Flow::Vertical, -.4, .4);
    CMenu::menuOpt row;
    row.type = CMenu::table_row;
    row.cells = {"Música muito longa ação", ""};
    row.group = 1;
    row.active = true;
    table.addOpt(row);
    row.height = .3;
    table.addOpt(row);
    auto r = table.optionRect(0);
    EXPECT_DOUBLE_EQ(r.left, -.4);
    EXPECT_NEAR(r.right, .5, 1e-12);
    EXPECT_NEAR(r.top - r.bottom, .2, 1e-12);
    EXPECT_NEAR(table.options[1].y, .1, 1e-12);
    CControls::MenuEvent click;
    click.kind = CControls::MenuEvent::Mouse;
    click.key = GLFW_MOUSE_BUTTON_LEFT;
    click.x = .45;
    click.y = .45;
    click.action = GLFW_PRESS;
    table.processEvent(click);
    click.action = GLFW_RELEASE;
    table.processEvent(click);
    EXPECT_EQ(table.options[0].status & 3, 3);
    table.select(1);
    table.resetBtns();
    table.update();
    EXPECT_TRUE(table.options[0].active);
    EXPECT_TRUE(
        table.options[0].cellLayouts.empty()); // CPU geometry needs no glyphs.
}

TEST_F(MenuCompositionCPU, PassiveBoxesCannotActivateAndAnchorsUseDimensions) {
    CMenu menu("boxes", &context);
    for (auto type : {CMenu::image, CMenu::panel}) {
        auto o = button();
        o.type = type;
        o.anchorRight = true;
        o.width = .7;
        o.shortcutKey = GLFW_KEY_F1;
        menu.addOpt(o);
    }
    menu.addOpt(button());
    menu.select(0);
    EXPECT_EQ(menu.focusedOption(), -1);
    CControls::MenuEvent event;
    event.key = GLFW_KEY_F1;
    event.action = GLFW_PRESS;
    menu.processEvent(event);
    EXPECT_EQ(menu.focusedOption(), 2);
    EXPECT_EQ(menu.options[0].status & 2, 0);
    EXPECT_NEAR(menu.optionRect(0).right, .9, 1e-12);
    menu.options[0].anchorRight = false;
    menu.options[0].centerX = -.2;
    auto r = menu.optionRect(0);
    EXPECT_NEAR((r.left + r.right) / 2, -.2, 1e-12);
    menu.options[0].width = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(std::isfinite(menu.optionRect(0).right));
}

TEST_F(MenuCompositionCPU, InvalidBoundsAndEmptyTablesStaySafe) {
    CMenu model("empty", &context);
    EXPECT_THROW(root.pushPanel(model, {1, 0, 0, 1}), std::invalid_argument);
    EXPECT_EQ(root.getUIListSize(), 0);
    auto row = button();
    row.type = CMenu::table_row;
    model.addOpt(row);
    auto r = model.optionRect(0);
    EXPECT_DOUBLE_EQ(r.left, r.right);
    model.tableColumns = {-1, std::numeric_limits<double>::infinity(), .5};
    EXPECT_DOUBLE_EQ(model.optionRect(0).right, .5);
}

TEST_F(MenuCompositionCPU,
       ResizingViewportRevealsFocusAndClampsShorterContent) {
    auto &left = addPanel(-1);
    root.focusInterface(left);
    left.select(8);
    const auto before = left.optionRect(8);
    ui::MenuOverrides patch;
    patch.viewport = CMenu::Rect{0, -.2, .8, .2};
    left.applyPresentationOverrides(patch);
    const auto after = left.optionRect(8);
    EXPECT_GE(after.bottom, -.2 - 1e-12);
    EXPECT_LE(after.top, .2 + 1e-12);
    EXPECT_NE(before.bottom, after.bottom);
    left.clearOptions();
    left.addOpt(button());
    left.scrollOffset = 100;
    left.reflow();
    EXPECT_DOUBLE_EQ(left.scrollOffset, 0);
}

TEST_F(MenuCompositionCPU, OverlappingPanelsKeepTheirRegistrationOrderOnClick) {
    auto &back = addPanel(-.8);
    auto &front = addPanel(-.4);
    ASSERT_EQ(CMenu::getUiMenuOnTop(), &front);
    mouse(-.6, .25, GLFW_PRESS); // Only the back panel covers this point.
    EXPECT_EQ(root.focusedInterface(), &back);
    EXPECT_EQ(CMenu::getUiMenuOnTop(), &front);
    mouse(-.6, .25, GLFW_RELEASE);
    mouse(-.2, .25, GLFW_PRESS);
    EXPECT_EQ(root.focusedInterface(), &front);
}

TEST_F(MenuCompositionCPU,
       ChangingTheActiveRootCancelsItsPreviousMouseCapture) {
    auto &left = addPanel(-1);
    mouse(-.8, .25, GLFW_PRESS);
    CMenu other("other", &context);
    other.addOpt(button());
    other.update();
    mouse(-.8, .25, GLFW_RELEASE);
    EXPECT_EQ(left.options[0].status & 2, 0);
    EXPECT_EQ(root.focusedInterface(), &root);
}

TEST_F(MenuCompositionCPU, PanelCommandsReachRootWithoutChangingFocus) {
    root.options[0].goback = true;
    root.options[0].shortcutKey = GLFW_KEY_F5;
    root.addOpt(button());
    root.select(1);
    auto &child = addPanel(-1, 1);
    ASSERT_TRUE(root.focusInterface(child));
    key(GLFW_KEY_ESCAPE);
    EXPECT_EQ(root.options[0].status & 3, 3);
    EXPECT_EQ(root.focusedInterface(), &child);
    EXPECT_EQ(root.focusedOption(), 1);
    key(GLFW_KEY_F5);
    EXPECT_EQ(root.options[0].status & 3, 3);
    EXPECT_EQ(root.focusedInterface(), &child);
}

TEST_F(MenuCompositionCPU, PanelCommandPropagationRespectsEditingAndModals) {
    root.options[0].goback = true;
    root.options[0].shortcutKey = GLFW_KEY_R;
    auto &child = addPanel(-1, 1);
    child.options[0].type = CMenu::text_input;
    ASSERT_TRUE(root.focusInterface(child));
    key(GLFW_KEY_ENTER);
    key(GLFW_KEY_R);
    EXPECT_EQ(root.options[0].status & 2, 0);
    key(GLFW_KEY_ESCAPE);
    EXPECT_EQ(child.options[0].status & 4, 0);
    EXPECT_EQ(root.options[0].status & 2, 0);
    key(GLFW_KEY_ESCAPE);
    EXPECT_EQ(root.options[0].status & 3, 3);

    CMenu dialog("dialog", &context);
    dialog.modal = true;
    dialog.addOpt(button());
    auto &modal = *CMenu::getUiAt(root.pushUserInterface(dialog)).m;
    auto &nested = *CMenu::getUiAt(modal.pushPanel(dialog, {-1, -1, 1, 1})).m;
    ASSERT_TRUE(root.focusInterface(nested));
    key(GLFW_KEY_R);
    EXPECT_EQ(root.options[0].status & 2, 0);
    modal.options[0].shortcutKey = GLFW_KEY_R;
    key(GLFW_KEY_R);
    EXPECT_EQ(modal.options[0].status & 3, 3);
    EXPECT_EQ(root.options[0].status & 2, 0);
}

TEST_F(MenuCompositionCPU, LocalCommandsWinAndLegacyWindowsDoNotBubble) {
    root.options[0].shortcutKey = GLFW_KEY_F5;
    auto &child = addPanel(-1, 1);
    child.options[0].shortcutKey = GLFW_KEY_F5;
    ASSERT_TRUE(root.focusInterface(child));
    key(GLFW_KEY_F5);
    EXPECT_EQ(child.options[0].status & 3, 3);
    EXPECT_EQ(root.options[0].status & 2, 0);
    CMenu window("window", &context);
    window.addOpt(button());
    root.pushUserInterface(window);
    key(GLFW_KEY_F5);
    EXPECT_EQ(root.options[0].status & 2, 0);
}

TEST_F(MenuCompositionCPU, FollowingViewportResizesClicksWithoutRebuilding) {
    CMenu model("resizable", &context);
    model.viewportEnabled = true;
    model.viewport = {0, -.4, .4, .4};
    auto o = button();
    o.x = .5;
    o.y = 0;
    o.manualPosition = true;
    o.active = true;
    o.preText = "preserved search";
    model.addOpt(o);
    auto &child = *CMenu::getUiAt(root.pushPanel(model)).m;
    root.focusInterface(child);
    auto click = [&] {
        mouse(.55, .05, GLFW_PRESS);
        mouse(.55, .05, GLFW_RELEASE);
    };
    click();
    EXPECT_EQ(child.options[0].status & 2, 0);
    ui::MenuOverrides patch;
    patch.viewport = CMenu::Rect{0, -.4, .8, .4};
    child.applyPresentationOverrides(patch);
    click();
    EXPECT_EQ(child.options[0].status & 3, 3);
    EXPECT_EQ(root.focusedInterface(), &child);
    EXPECT_TRUE(child.options[0].active);
    EXPECT_EQ(child.options[0].preText, "preserved search");
    EXPECT_FALSE(child.getFixedPanelBounds());
    child.applyPresentationOverrides({});
    click();
    EXPECT_EQ(child.options[0].status & 2, 0);
    EXPECT_EQ(child.getPanelBounds().right, .4);
    EXPECT_EQ(child.options.size(), 1u);
}

TEST_F(MenuCompositionCPU, IndependentBoundsCanResizeAndRejectInvalidChanges) {
    auto &child = addPanel(0, 1);
    const auto original = child.getPanelBounds();
    child.viewport.right = 1.5;
    EXPECT_EQ(child.getPanelBounds().right, original.right);
    child.setPanelBounds(CMenu::Rect{0, -.5, 1.5, .5});
    EXPECT_EQ(child.getPanelBounds().right, 1.5);
    EXPECT_THROW(child.setPanelBounds(CMenu::Rect{0, 0, 0, 1}),
                 std::invalid_argument);
    EXPECT_THROW(child.setPanelBounds(CMenu::Rect{
                     0, 0, std::numeric_limits<double>::infinity(), 1}),
                 std::invalid_argument);
    EXPECT_EQ(child.getPanelBounds().right, 1.5);
    child.setPanelBounds(std::nullopt);
    child.viewport.right = 1.2;
    EXPECT_EQ(child.getPanelBounds().right, 1.2);
}

TEST_F(MenuCompositionCPU,
       PanelFallbackDoesNotForwardNavigationOrRepeatCommands) {
    auto &child = addPanel(-1, 1);
    child.options[0].enableEnter = false;
    root.focusInterface(child);
    root.options[0].shortcutKey = GLFW_KEY_ENTER;
    key(GLFW_KEY_ENTER);
    EXPECT_EQ(root.options[0].status & 2, 0);
    root.options[0].shortcutKey = GLFW_KEY_F5;
    CControls::MenuEvent event;
    event.key = GLFW_KEY_F5;
    for (int action : {GLFW_REPEAT, GLFW_RELEASE}) {
        event.action = action;
        context.events.push_back(event);
        root.update();
        EXPECT_EQ(root.options[0].status & 2, 0);
    }
    key(GLFW_KEY_F5);
    EXPECT_EQ(root.options[0].status & 3, 3);
    EXPECT_EQ(root.focusedInterface(), &child);
}
