#include "CUIDefinition.h"
#include "CUIOverrides.h"
#include "GPPGame.h"
#include <fstream>
#include <gtest/gtest.h>

namespace {
class MenuPresentationCPU : public ::testing::Test {
  protected:
    CFonts fonts;
    CMenu::Context context;
    std::filesystem::path directory;
    void SetUp() override {
        context.fonts = &fonts;
        directory =
            std::filesystem::temp_directory_path() /
            ("gpp-menu-presentation-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
    }
    void TearDown() override { std::filesystem::remove_all(directory); }
    CMenu::menuOpt option(const std::string &id) {
        CMenu::menuOpt o;
        o.optionName = id;
        o.text = "Test";
        o.type = CMenu::textbtn;
        o.group = 1;
        return o;
    }
    std::filesystem::path script(const std::string &source) {
        auto path = directory / "MenuOverrides.lua";
        std::ofstream(path) << source;
        return path;
    }
};
} // namespace

TEST_F(MenuPresentationCPU,
       SemanticNamesAreStableQualifiedAndRejectDuplicates) {
    CMenu a("main", &context), b("main", &context);
    a.addOpt(option("play"));
    b.addOpt(option("settings"));
    b.addOpt(option("play"));
    EXPECT_EQ(a.options[0].optionName, b.options[1].optionName);
    EXPECT_EQ(a.options[0].optionName, "main.play");
    EXPECT_EQ(a.addOpt(a.options[0]), -1);
    EXPECT_EQ(a.options.size(), 1u);
    a.addOpt(option("auto_0"));
    a.addOpt(option(""));
    EXPECT_EQ(a.options.back().optionName, "main.auto_1");
    EXPECT_EQ(a.findOption("play"), a.findOption("main.play"));
    CMenu copy(a);
    EXPECT_EQ(copy.options[0].optionName, "main.play");
    copy.addOpt(option(""));
    EXPECT_NE(copy.options.back().optionName, "main.auto_1");
}

TEST_F(MenuPresentationCPU, DefinitionIdsSurviveInstantiationAndReordering) {
    ui::MenuDefinition def;
    def.id = "dialog";
    ui::OptionDefinition one, two;
    one.id = "one";
    two.id = "two";
    def.options = {two, one};
    auto menu = ui::instantiate(def, &context);
    EXPECT_EQ(menu->options[0].optionName, "dialog.two");
    EXPECT_EQ(menu->options[1].optionName, "dialog.one");
}

TEST_F(MenuPresentationCPU,
       ThemeReplacementRestoresBaseWithoutTouchingBehavior) {
    CMenu menu("main", &context);
    menu.x = .2;
    auto o = option("play");
    std::string input = "user value";
    o.preText = input;
    o.externalPreTextRef = &input;
    o.menusXRef = {"playOptions"};
    o.optList = {"one", "two"};
    o.listID = 1;
    o.texture = 42;
    int calls = 0;
    o.updateCppCallback = [&calls](CMenu &, CMenu::menuOpt &) {
        return ++calls;
    };
    menu.addOpt(o);
    menu.select(0);
    menu.scrollOffset = .25;
    ui::MenuOverrides first;
    first.x = -.4;
    first.style.padding = .2;
    first.options["play"].y = .6;
    first.options["play"].style.textSize = .12;
    menu.applyPresentationOverrides(first);
    menu.applyPresentationOverrides(first);
    EXPECT_DOUBLE_EQ(menu.x, -.4);
    EXPECT_DOUBLE_EQ(menu.options[0].y, .6);
    ui::MenuOverrides second;
    second.options["play"].fieldWidth = .8;
    menu.applyPresentationOverrides(second);
    EXPECT_DOUBLE_EQ(menu.x, .2);
    EXPECT_FALSE(menu.styleOverrides.padding);
    EXPECT_FALSE(menu.options[0].styleOverrides.textSize);
    EXPECT_DOUBLE_EQ(menu.options[0].y, 0);
    EXPECT_EQ(menu.focusedOption(), 0);
    EXPECT_DOUBLE_EQ(menu.scrollOffset, .25);
    EXPECT_EQ(menu.options[0].preText, input);
    EXPECT_EQ(menu.options[0].externalPreTextRef, &input);
    EXPECT_EQ(menu.options[0].menusXRef.front(), "playOptions");
    EXPECT_EQ(menu.options[0].optList.size(), 2u);
    EXPECT_EQ(menu.options[0].listID, 1);
    EXPECT_EQ(menu.options[0].texture, 42u);
    menu.options[0].updateCppCallback(menu, menu.options[0]);
    EXPECT_EQ(calls, 1);
    menu.applyPresentationOverrides({});
    EXPECT_DOUBLE_EQ(menu.options[0].fieldWidth, .55);
}

TEST_F(MenuPresentationCPU, SpecificOptionStyleWinsOverLuaMenuStyle) {
    CMenu menu("main", &context);
    auto explicitStyle = option("play");
    explicitStyle.styleOverrides.textSize = .11;
    menu.addOpt(explicitStyle);
    menu.addOpt(option("settings"));
    ui::MenuOverrides patch;
    patch.style.textSize = .2;
    menu.applyPresentationOverrides(patch);
    EXPECT_DOUBLE_EQ(menu.resolvedOptionStyle(menu.options[0]).textSize, .11);
    EXPECT_DOUBLE_EQ(menu.resolvedOptionStyle(menu.options[1]).textSize, .2);
    patch.options["play"].style.textSize = .3;
    menu.applyPresentationOverrides(patch);
    EXPECT_DOUBLE_EQ(menu.resolvedOptionStyle(menu.options[0]).textSize, .3);
    context.style.typography.textSize = .4;
    menu.applyPresentationOverrides({});
    EXPECT_DOUBLE_EQ(menu.resolvedOptionStyle(menu.options[1]).textSize, .4);
}

TEST_F(MenuPresentationCPU, FixedAxesReserveFlowSlotsAndUseEffectiveHitboxes) {
    CMenu menu("main", &context);
    menu.setFlow(CMenu::Flow::Vertical, -.5, .5);
    menu.addOpt(option("first"));
    menu.addOpt(option("second"));
    menu.addOpt(option("third"));
    menu.reflow();
    const double secondY = menu.options[1].y, thirdY = menu.options[2].y;
    ui::MenuOverrides patch;
    patch.options["first"].y = -.4;
    patch.options["first"].centerX = .2;
    menu.applyPresentationOverrides(patch);
    menu.reflow();
    EXPECT_DOUBLE_EQ(menu.options[0].y, -.4);
    EXPECT_DOUBLE_EQ(menu.options[1].y, secondY);
    EXPECT_DOUBLE_EQ(menu.options[2].y, thirdY);
    auto rect = menu.optionRect(0);
    EXPECT_NEAR((rect.left + rect.right) / 2, .2, 1e-12);
    CControls::MenuEvent click;
    click.kind = CControls::MenuEvent::Mouse;
    click.key = GLFW_MOUSE_BUTTON_LEFT;
    click.x = (rect.left + rect.right) / 2;
    click.y = (rect.bottom + rect.top) / 2;
    click.action = GLFW_PRESS;
    menu.processEvent(click);
    click.action = GLFW_RELEASE;
    menu.processEvent(click);
    EXPECT_NE(menu.options[0].status & 2, 0);
    menu.beginManualEdit(0);
    menu.options[0].x += .3;
    const double edited = menu.options[0].x;
    menu.reflow();
    EXPECT_DOUBLE_EQ(menu.options[0].x, edited);
    menu.applyPresentationOverrides({});
    EXPECT_EQ(menu.flow, CMenu::Flow::Vertical);
}

TEST_F(MenuPresentationCPU, ExplicitXReplacesInheritedAnchors) {
    CMenu menu("main", &context);
    auto o = option("play");
    o.anchorRight = true;
    o.centerX = .4;
    menu.addOpt(o);
    ui::MenuOverrides patch;
    patch.options["play"].x = -.3;
    menu.applyPresentationOverrides(patch);
    menu.reflow();
    EXPECT_DOUBLE_EQ(menu.options[0].x, -.3);
    EXPECT_FALSE(menu.options[0].anchorRight);
    EXPECT_FALSE(menu.options[0].centerX);
    menu.applyPresentationOverrides({});
    EXPECT_TRUE(menu.options[0].anchorRight);
    EXPECT_EQ(menu.options[0].centerX, .4);
}

TEST_F(MenuPresentationCPU, NewOptionsClonesAndRebuildsHaveIndependentBases) {
    CMenu menu("main", &context);
    menu.addOpt(option("first"));
    ui::MenuOverrides patch;
    patch.x = -.4;
    patch.options["later"].fieldWidth = .8;
    menu.applyPresentationOverrides(patch);
    auto later = option("later");
    later.fieldWidth = .65;
    menu.addOpt(later);
    EXPECT_DOUBLE_EQ(menu.options[1].fieldWidth, .8);
    CMenu copy(menu);
    copy.applyPresentationOverrides({});
    EXPECT_DOUBLE_EQ(copy.options[1].fieldWidth, .65);
    EXPECT_DOUBLE_EQ(menu.options[1].fieldWidth, .8);
    menu.clearOptions();
    EXPECT_FALSE(menu.originalPresentation());
    EXPECT_DOUBLE_EQ(menu.x, 0);
    later.fieldWidth = .7;
    menu.addOpt(later);
    menu.applyPresentationOverrides(patch);
    menu.applyPresentationOverrides({});
    EXPECT_DOUBLE_EQ(menu.options[0].fieldWidth, .7);
}

TEST_F(MenuPresentationCPU, LuaRoundTripRetainsOnlyDeclaredProperties) {
    auto path = script(R"(return { main = { x = 0, anchorLeft = false,
        options = { play = { y = 0, style = { textSize = .12,
        textColor = {r=1,g=.8,b=.2,a=1} } } } } })");
    auto patches = ui::readOverrides(path);
    EXPECT_EQ(patches.at("main").x, 0);
    EXPECT_EQ(patches.at("main").anchorLeft, false);
    EXPECT_FALSE(patches.at("main").y);
    ui::writeOverrides(path, patches);
    patches = ui::readOverrides(path);
    const auto &o = patches.at("main").options.at("play");
    EXPECT_FALSE(o.x);
    EXPECT_EQ(o.y, 0);
    EXPECT_EQ(o.style.textSize, .12);
    ASSERT_TRUE(o.style.textColor);
    EXPECT_DOUBLE_EQ(o.style.textColor->g, .8);
}

TEST_F(MenuPresentationCPU, InvalidDocumentsNeverChangeAnInstance) {
    for (const auto &source :
         {"return {main={options={play={text='No'}}}}",
          "return {main={options={play={fieldWidth=0}}}}",
          "return {main={options={play={x=0,centerX=0}}}}",
          "return {main={options={play={style={textSize=0/0}}}}}",
          "return {main={flow=15}}",
          "return {main={viewport={left=2,right=1}}}",
          "return {main={options={play={x='0'}}}}",
          "return {main={options={play={updateCppCallback=function()end}}}}",
          "error({})", "while true do end"}) {
        EXPECT_THROW(ui::readOverrides(script(source)), std::runtime_error)
            << source;
    }
    CMenu menu("main", &context);
    menu.addOpt(option("play"));
    ui::MenuOverrides patch;
    patch.x = .4;
    menu.applyPresentationOverrides(patch);
    patch.options["play"].fieldWidth = -1;
    EXPECT_THROW(menu.applyPresentationOverrides(patch), std::runtime_error);
    EXPECT_DOUBLE_EQ(menu.x, .4);
    EXPECT_DOUBLE_EQ(menu.options[0].fieldWidth, .55);
}

TEST_F(MenuPresentationCPU,
       ThemeLoadIsExplicitAndMissingOrInvalidFilesResetOverrides) {
    auto &system = GPPGame::GuitarPP().menus;
    auto &menu = system.newNamedMenu("themeTest");
    menu.clearOptions();
    menu.addOpt(option("play"));
    script("return {themeTest={options={play={fieldWidth=.9}}}}");
    system.loadThemePresentation(directory);
    system.preparePresentation(menu);
    EXPECT_DOUBLE_EQ(menu.options[0].fieldWidth, .9);
    script("return {themeTest={options={play={fieldWidth=.7}}}}");
    menu.reflow();
    EXPECT_DOUBLE_EQ(menu.options[0].fieldWidth, .9);
    system.loadThemePresentation(directory);
    EXPECT_DOUBLE_EQ(menu.options[0].fieldWidth, .7);
    script("return {themeTest={options={play={fieldWidth=-1}}}}");
    system.loadThemePresentation(directory);
    EXPECT_DOUBLE_EQ(menu.options[0].fieldWidth, .55);
    std::filesystem::remove(directory / "MenuOverrides.lua");
    system.loadThemePresentation(directory);
    EXPECT_TRUE(system.presentationOverrides().empty());
}

TEST_F(MenuPresentationCPU, LuaEventsFollowSemanticIdsAfterRebuild) {
    auto &game = GPPGame::GuitarPP();
    auto &lua = CLuaH::Lua();
    auto &script = lua.files["presentation-tests"]["callback"];
    script = lua.newScriptR(R"(
        calls = 0
        setCallBackToEvent('stable.play', function(menu, id, value, state)
            if state == 3 then
                assert(menu == 'stable' and id == 'stable.play')
                calls = calls + 1
            end
        end)
    )",
                            "presentation-callback");
    ASSERT_EQ(CLuaH::runScript(script), 0);
    auto &menu = game.menus.newNamedMenu("stable");
    menu.addOpt(option("play"));
    const int event = menu.options[0].eventID;
    menu.clearOptions();
    menu.addOpt(option("settings"));
    menu.addOpt(option("play"));
    EXPECT_EQ(menu.options[1].eventID, event);
    menu.select(1);
    auto &controls = CControls::controls();
    controls.menuInputActive = true;
    CControls::MenuEvent enter;
    enter.key = GLFW_KEY_ENTER;
    enter.action = GLFW_PRESS;
    controls.queueMenuEvent(enter);
    menu.update();
    lua_getglobal(script.luaState.get(), "calls");
    EXPECT_EQ(lua_tointeger(script.luaState.get(), -1), 1);
    lua_pop(script.luaState.get(), 1);
    controls.menuInputActive = false;
    controls.resetMenuInput();
    lua.files.erase("presentation-tests");
}

TEST_F(MenuPresentationCPU, ThemeRefreshReachesOpenModalsAndKeepsTheirBinding) {
    auto &system = GPPGame::GuitarPP().menus;
    auto &root = system.newNamedMenu("modalOwner");
    std::string text = "preserved";
    auto &modal = system.modals.openRename(root, text);
    modal.findOption("name")->preText = "edited";
    const auto focused = modal.focusedOption();
    ui::MenuOverrideSet patches;
    patches["uiRenameMenu"].options["name"].fieldWidth = .9;
    system.replacePresentation(patches);
    EXPECT_DOUBLE_EQ(modal.findOption("name")->fieldWidth, .9);
    EXPECT_EQ(modal.findOption("name")->preText, "edited");
    EXPECT_EQ(modal.findOption("name")->externalPreTextRef, &text);
    EXPECT_EQ(modal.focusedOption(), focused);
    system.replacePresentation({});
    EXPECT_DOUBLE_EQ(modal.findOption("name")->fieldWidth, .55);
    root.closeInterfaces();
}

TEST_F(MenuPresentationCPU, ExplicitFlowOriginReplacesInheritedLeftAnchor) {
    CMenu menu("main", &context);
    menu.anchorLeft = true;
    menu.setFlow(CMenu::Flow::Vertical, -.5, .4);
    menu.addOpt(option("play"));
    ui::MenuOverrides patch;
    patch.flowX = .2;
    menu.applyPresentationOverrides(patch);
    menu.reflow();
    EXPECT_DOUBLE_EQ(menu.options[0].x, .2);
    menu.applyPresentationOverrides({});
    EXPECT_TRUE(menu.anchorLeft);
}

TEST_F(MenuPresentationCPU, DeclarativeFilesStayOutOfRuntimeCallbackRegistry) {
    script("return {}");
    std::ofstream(directory / "Gui.lua") << "return {}";
    std::ofstream(directory / "Theme.lua") << "return {}";
    auto &lua = CLuaH::Lua();
    ASSERT_TRUE(
        lua.loadFiles(directory.string(), {"Gui.lua", "MenuOverrides.lua"}));
    EXPECT_EQ(lua.files[directory.string()].size(), 1u);
    EXPECT_EQ(lua.files[directory.string()].count("Theme.lua"), 1u);
    lua.files.erase(directory.string());
}

TEST(LuaCPU, QueuedScriptReturnsStoredObjectWithoutUsingMovedKeys) {
    auto &lua = CLuaH::Lua();
    auto script = lua.newScriptR("return 42", "queued");
    script.filePath = "queue-regression";
    const auto filename = script.fileName;
    auto *state = script.luaState.get();
    auto *stored = lua.newScriptInQuere(std::move(script));
    EXPECT_EQ(stored, &lua.files.at("queue-regression").at(filename));
    EXPECT_EQ(stored->luaState.get(), state);
    EXPECT_EQ(stored->filePath, "queue-regression");
    lua.files.erase("queue-regression");
}

TEST_F(MenuPresentationCPU, ComposedControlsRoundTripAndResolveTexturesOnce) {
    ui::MenuDefinition def;
    def.id = "table";
    def.tableColumns = {.5, .7};
    ui::OptionDefinition row;
    row.id = "song";
    row.type = CMenu::table_row;
    row.cells = {"Canção", "Artista"};
    row.active = true;
    row.height = .2;
    ui::OptionDefinition image;
    image.id = "cover";
    image.type = CMenu::image;
    image.texture = "cover.tga";
    image.width = image.height = .3;
    ui::OptionDefinition panel = image;
    panel.id = "background";
    panel.type = CMenu::panel;
    panel.texture.clear();
    panel.style.background = ui::Color{.1, .2, .3, .8};
    panel.style.border = ui::Color{1, .5, 0, 1};
    panel.style.borderWidth = .01;
    def.options = {panel, row, image};
    def.style.panelBorderWidth = .02;
    def.style.panelBorder = ui::Color{0, 1, 0, 1};
    auto path = directory / "Gui.lua";
    ui::writeMenus(path, {def});
    const auto read = ui::readMenus(path);
    ASSERT_EQ(read.size(), 1u);
    EXPECT_EQ(read[0].tableColumns, def.tableColumns);
    EXPECT_EQ(read[0].options[1].cells, row.cells);
    EXPECT_TRUE(read[0].options[1].active);
    int resolved = 0;
    auto menu =
        ui::instantiate(read[0], &context, [&](const std::string &name) {
            EXPECT_EQ(name, "cover.tga");
            ++resolved;
            return 42u;
        });
    EXPECT_EQ(resolved, 1);
    EXPECT_EQ(menu->options[2].texture, 42u);
    EXPECT_EQ(menu->options[0].styleOverrides.borderWidth,
              std::optional<double>{.01});
    menu->reflow();
    menu->update();
    EXPECT_EQ(resolved, 1);
    CMenu copy(*menu);
    EXPECT_EQ(copy.tableColumns, menu->tableColumns);
    EXPECT_EQ(copy.options[1].cells, row.cells);
    EXPECT_TRUE(copy.options[1].active);
    copy.options[1].cells[0] = "Independent clone";
    EXPECT_EQ(menu->options[1].cells, row.cells);
}

TEST_F(MenuPresentationCPU,
       BoxAndColumnOverridesPreserveRuntimeStateAndRestoreBase) {
    CMenu menu("table", &context);
    menu.tableColumns = {.4, .6};
    auto row = option("song");
    row.type = CMenu::table_row;
    row.cells = {"Song", "Artist"};
    row.active = true;
    row.height = .2;
    row.texture = 42;
    menu.addOpt(row);
    menu.select(0);
    menu.scrollOffset = .25;
    ui::MenuOverrides patch;
    patch.tableColumns = std::vector<double>{.6, .8};
    patch.options["song"].height = .3;
    patch.options["song"].style.background = ui::Color{0, 0, 1, 1};
    patch.options["song"].style.borderWidth = .02;
    patch.style.panelBorderWidth = .03;
    auto path = directory / "MenuOverrides.lua";
    ui::writeOverrides(path, {{"table", patch}});
    auto read = ui::readOverrides(path);
    ASSERT_EQ(read.size(), 1u);
    menu.applyPresentationOverrides(read.at("table"));
    EXPECT_NEAR(menu.optionRect(0).right - menu.optionRect(0).left, 1.4, 1e-12);
    EXPECT_DOUBLE_EQ(menu.options[0].height, .3);
    EXPECT_TRUE(menu.options[0].active);
    EXPECT_EQ(menu.options[0].cells, row.cells);
    EXPECT_EQ(menu.options[0].texture, 42u);
    EXPECT_EQ(menu.focusedOption(), 0);
    EXPECT_DOUBLE_EQ(menu.scrollOffset, .25);
    CMenu clone(menu);
    menu.applyPresentationOverrides({});
    EXPECT_DOUBLE_EQ(menu.options[0].height, .2);
    EXPECT_EQ(menu.tableColumns, (std::vector<double>{.4, .6}));
    EXPECT_EQ(clone.tableColumns, (std::vector<double>{.6, .8}));
    EXPECT_FALSE(menu.options[0].styleOverrides.background);
    EXPECT_FALSE(menu.options[0].styleOverrides.borderWidth);
}

TEST_F(MenuPresentationCPU,
       InvalidNewControlDocumentsAndPatchesDoNotMutateMenus) {
    ui::MenuDefinition def;
    def.id = "invalid";
    ui::OptionDefinition row;
    row.id = "row";
    row.type = CMenu::table_row;
    row.cells = {"x", "y"};
    def.options = {row};
    EXPECT_THROW(ui::instantiate(def, &context), std::runtime_error);
    def.tableColumns = {.2};
    EXPECT_THROW(ui::validate({def}), std::runtime_error);
    def.tableColumns = {.2, -.3};
    EXPECT_THROW(ui::validate({def}), std::runtime_error);
    def.tableColumns = {.2, .3};
    EXPECT_NO_THROW(ui::validate({def}));
    def.options[0].cells[0] = "\xff";
    EXPECT_THROW(ui::validate({def}), std::runtime_error);
    def.options[0] = {};
    def.options[0].id = "image";
    def.options[0].type = CMenu::image;
    EXPECT_THROW(ui::validate({def}), std::runtime_error);
    CMenu menu("table", &context);
    menu.tableColumns = {.2};
    menu.addOpt(option("song"));
    ui::MenuOverrides patch;
    patch.tableColumns = std::vector<double>{-1};
    patch.x = 3;
    EXPECT_THROW(menu.applyPresentationOverrides(patch), std::runtime_error);
    EXPECT_DOUBLE_EQ(menu.x, 0);
    EXPECT_EQ(menu.tableColumns, (std::vector<double>{.2}));
    EXPECT_EQ(menu.originalPresentation(), nullptr);
    patch.tableColumns.reset();
    patch.options["song"].style.borderWidth = -1;
    EXPECT_THROW(menu.applyPresentationOverrides(patch), std::runtime_error);
}

TEST_F(MenuPresentationCPU, PanelBoundsOverridesRoundTripAndRestoreMode) {
    CMenu root("root", &context), model("child", &context);
    model.viewport = {0, -.5, .5, .5};
    model.addOpt(option("song"));
    auto &child = *CMenu::getUiAt(root.pushPanel(model)).m;
    child.options[0].active = true;
    child.options[0].preText = "search";
    child.scrollOffset = .1;
    auto patches = ui::readOverrides(script(R"(return {
        child = {viewport={left=0,bottom=-.5,right=1,top=.5},
                 panelBounds={left=0,bottom=-.6,right=1.2,top=.6}}
    })"));
    ui::writeOverrides(directory / "roundtrip.lua", patches);
    patches = ui::readOverrides(directory / "roundtrip.lua");
    child.applyPresentationOverrides(patches.at("child"));
    EXPECT_EQ(child.getPanelBounds().right, 1.2);
    EXPECT_TRUE(child.getFixedPanelBounds());
    EXPECT_TRUE(child.options[0].active);
    EXPECT_EQ(child.options[0].preText, "search");
    EXPECT_EQ(child.scrollOffset, .1);
    child.applyPresentationOverrides({});
    EXPECT_FALSE(child.getFixedPanelBounds());
    EXPECT_EQ(child.getPanelBounds().right, .5);

    auto &fixed = *CMenu::getUiAt(root.pushPanel(model, {0, -1, 1, 1})).m;
    patches = ui::readOverrides(script(R"(return {
        child = {panelFollowsViewport=true,
                 viewport={left=0,bottom=-.5,right=.8,top=.5}}
    })"));
    ui::writeOverrides(directory / "roundtrip.lua", patches);
    patches = ui::readOverrides(directory / "roundtrip.lua");
    fixed.applyPresentationOverrides(patches.at("child"));
    EXPECT_FALSE(fixed.getFixedPanelBounds());
    EXPECT_EQ(fixed.getPanelBounds().right, .8);
    fixed.applyPresentationOverrides({});
    EXPECT_TRUE(fixed.getFixedPanelBounds());
    EXPECT_EQ(fixed.getPanelBounds().right, 1);
    ui::MenuOverrides invalid;
    invalid.panelBounds = CMenu::Rect{1, 0, 0, 1};
    invalid.x = 99;
    EXPECT_THROW(fixed.applyPresentationOverrides(invalid), std::runtime_error);
    EXPECT_EQ(fixed.x, 0);
    EXPECT_EQ(fixed.getPanelBounds().right, 1);
    invalid.panelBounds = CMenu::Rect{0, 0, 1, 1};
    invalid.panelFollowsViewport = true;
    EXPECT_THROW(fixed.applyPresentationOverrides(invalid), std::runtime_error);
    invalid.panelBounds.reset();
    invalid.panelFollowsViewport = false;
    EXPECT_THROW(fixed.applyPresentationOverrides(invalid), std::runtime_error);
    EXPECT_EQ(fixed.x, 0);
    EXPECT_EQ(fixed.getPanelBounds().right, 1);
}
