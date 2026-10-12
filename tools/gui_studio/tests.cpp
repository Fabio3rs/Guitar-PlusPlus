#include "session.h"
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <locale>

namespace {
class GuiFiles : public ::testing::Test {
  protected:
    GuiStudio::fs::path directory;
    void SetUp() override {
        directory =
            GuiStudio::fs::temp_directory_path() /
            ("gpp-gui-test-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        GuiStudio::fs::create_directories(directory);
    }
    void TearDown() override { GuiStudio::fs::remove_all(directory); }
};

void makeAtlas(const GuiStudio::fs::path &path) {
    GuiStudio::fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    const unsigned char header[18]{0, 0, 2, 0,  0, 0,  0, 0,  0,
                                   0, 0, 0, 64, 0, 64, 0, 32, 8};
    file.write(reinterpret_cast<const char *>(header), sizeof(header));
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            const unsigned char pixel[]{
                255, 255, 255,
                static_cast<unsigned char>(x % 32 >= 4 && x % 32 < 20 &&
                                                   y % 32 >= 3 && y % 32 < 25
                                               ? 255
                                               : 0)};
            file.write(reinterpret_cast<const char *>(pixel), 4);
        }
    }
}

class GuiGL : public GuiFiles {
  protected:
    GLFWwindow *window = nullptr;
    void SetUp() override {
        GuiFiles::SetUp();
        ASSERT_TRUE(glfwInit());
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        window = glfwCreateWindow(640, 480, "GUI test", nullptr, nullptr);
        ASSERT_NE(window, nullptr);
        glfwMakeContextCurrent(window);
        ASSERT_EQ(glewInit(), GLEW_OK);
        while (glGetError() != GL_NO_ERROR) {
        }
        makeAtlas(directory / "data/sprites/FONT.tga");
        GuiStudio::FontDefinition font;
        font.name = "default";
        font.atlases.push_back({"data/sprites", "FONT.tga", "Ag\néy", {2, 2}});
        GuiStudio::fs::create_directories(directory /
                                          "data/themes/gppdefaulttheme");
        GuiStudio::writeFonts(
            directory / "data/themes/gppdefaulttheme/Theme.lua", {font});
    }
    void TearDown() override {
        if (window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
        GuiFiles::TearDown();
    }
};
} // namespace

TEST_F(GuiFiles, MenuLuaRoundTripRetainsStylesIdsUnicodeAndActions) {
    auto definitions = ui::defaultModals();
    auto &menu = definitions[0];
    menu.style.padding = .123456789012345;
    menu.style.animateSelection = false;
    menu.options[1].text = "ação \"\\\nç";
    menu.options[1].style.textColor = ui::Color{.1, .2, .3, .4};
    menu.options[1].actions = {"menu:next", "modal:dialog", "start-game"};
    std::swap(menu.options[0], menu.options[1]);
    const auto path = directory / "Gui.lua";
    ui::writeMenus(path, definitions);
    auto read = ui::readMenus(path);
    ASSERT_EQ(read.size(), 2u);
    EXPECT_EQ(read[0].options[0].id, "name");
    EXPECT_EQ(read[0].options[0].text, menu.options[0].text);
    EXPECT_EQ(read[0].options[0].actions, menu.options[0].actions);
    EXPECT_EQ(read[0].style.padding, menu.style.padding);
    EXPECT_EQ(read[0].style.animateSelection, menu.style.animateSelection);
    EXPECT_FALSE(read[0].options[0].style.textSize.has_value());
    EXPECT_DOUBLE_EQ(read[0].options[0].style.textColor->a, .4);
}

TEST_F(GuiFiles, MalformedMenusDoNotReplaceModalDefinitions) {
    CModalManager manager;
    const auto path = directory / "Gui.lua";
    ui::writeTextFile(path,
                      "return {{id='uiRenameMenu', modal=true, options={}}}");
    std::string error;
    EXPECT_FALSE(manager.load(path, error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(manager.inspect()[0].options.size(), 3u);
    ui::writeTextFile(path, "while true do end");
    EXPECT_FALSE(manager.load(path, error));
    ui::writeTextFile(path, "return {{ id='menu', x=0/0 }}");
    EXPECT_THROW(ui::readMenus(path), std::runtime_error);
    auto definitions = ui::defaultModals();
    definitions[0].options[1].type = CMenu::static_text;
    EXPECT_THROW(ui::validate(definitions), std::runtime_error);
}

TEST_F(GuiFiles, FontLuaRoundTripKeepsMappingMetricsAndPerCodepointOverrides) {
    auto fonts = GuiStudio::defaultFonts();
    fonts[0].adjustments['g'] = {.95, -.123456789012345, .7, -.05};
    fonts[0].adjustments[0xe9] = {1.1, .1, std::nullopt, std::nullopt};
    fonts[0].atlases.push_back({"data/sprites", "extra.tga", "漢\n字", {1, 2}});
    auto path = directory / "Theme.lua";
    GuiStudio::writeFonts(path, fonts);
    const auto result = GuiStudio::readFonts(path);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].atlases[0].mapping, fonts[0].atlases[0].mapping);
    EXPECT_EQ(result[0].atlases[1].mapping, "漢\n字");
    EXPECT_EQ(result[0].adjustments.at('g').offset, -.12346);
    EXPECT_DOUBLE_EQ(result[0].metrics.baseline, 14.0 / 64);
}

TEST_F(GuiFiles, FontExportRoundsToFiveDecimalsWithoutTrailingZeros) {
    auto fonts = GuiStudio::defaultFonts();
    fonts[0].adjustments['g'] = {1.0000000000000002, .095000000000000001,
                                 .69374999999999998, -.15312500000000001};
    fonts[0].adjustments['A'] = {1.123456, -.000001, .123454, -.123456};
    const auto path = directory / "Theme.lua";
    GuiStudio::writeFonts(path, fonts);
    std::ifstream input(path);
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    EXPECT_NE(
        text.find("setFontMetrics(\"default\", 0.21875, 0.78125, 0.21875)"),
        std::string::npos);
    EXPECT_NE(text.find("setFontCharMetrics(\"default\", \"g\", 1, 0.095)"),
              std::string::npos);
    EXPECT_NE(
        text.find("setFontCharSpacing(\"default\", \"g\", 0.69375, -0.15313)"),
        std::string::npos);
    EXPECT_NE(text.find("setFontCharMetrics(\"default\", \"A\", 1.12346, 0)"),
              std::string::npos);
    const auto loaded = GuiStudio::readFonts(path);
    EXPECT_EQ(loaded[0].adjustments.at('A').advance, .12345);
    EXPECT_EQ(loaded[0].adjustments.at('A').bearing, -.12346);
}

TEST_F(GuiFiles, FontScriptCannotRunGameOrFileAPIsAndErrorsRemainVisible) {
    auto path = directory / "Theme.lua";
    ui::writeTextFile(path, "newMenu('unexpected')");
    EXPECT_THROW(GuiStudio::readFonts(path), std::runtime_error);
    ui::writeTextFile(path, "dofile('anything')");
    EXPECT_THROW(GuiStudio::readFonts(path), std::runtime_error);
    ui::writeTextFile(path, "while true do end");
    EXPECT_THROW(GuiStudio::readFonts(path), std::runtime_error);
    ui::writeTextFile(path,
                      "addTextureToFont('default','x','a.tga','A',2.5,2)");
    EXPECT_THROW(GuiStudio::readFonts(path), std::runtime_error);
}

TEST_F(GuiFiles, ModalBindingsFollowIdsAfterReorderAndDoNotLeakOnReopen) {
    CFonts fonts;
    CMenu::Context context;
    context.fonts = &fonts;
    CMenu root("root", &context);
    CModalManager manager(&context);
    auto definitions = ui::defaultModals();
    std::swap(definitions[0].options[0], definitions[0].options[1]);
    std::swap(definitions[1].options[1], definitions[1].options[2]);
    manager.replace(definitions);
    std::string value = "original";
    auto &rename = manager.openRename(root, value);
    ASSERT_EQ(rename.options[0].externalPreTextRef, &value);
    EXPECT_EQ(rename.options[0].preText, "original");
    root.closeInterfaces();
    auto &profile = manager.openCreateProfile(root);
    ASSERT_EQ(profile.options[2].type, CMenu::text_input);
    EXPECT_EQ(profile.options[2].externalPreTextRef, nullptr);
    profile.options[2].preText = "Player ç";
    profile.options[2].posUpdateCppCallback(profile, profile.options[2]);
    EXPECT_EQ(profile.options[1].text, "Path: /Player ç");
    root.closeInterfaces();
    auto &again = manager.openRename(root, value);
    EXPECT_EQ(again.options[0].preText, "original");
    EXPECT_FALSE(again.options[0].posUpdateCppCallback);
}

TEST_F(GuiGL, GlyphEditsSurviveExportAndFailedReloadPreservesPreviousFont) {
    GuiStudio::Session session(directory, directory);
    ASSERT_EQ(session.fonts.size(), 1u);
    const auto before = session.registry().inspect("default");
    auto glyph = before.glyphs.at('g');
    const auto revision = session.registry().revision();
    glyph.offset = -.2;
    glyph.height = .8;
    session.applyGlyph('g', glyph);
    EXPECT_GT(session.registry().revision(), revision);
    auto edited = session.registry().inspect("default");
    EXPECT_DOUBLE_EQ(edited.glyphs.at('g').offset, -.2);
    EXPECT_DOUBLE_EQ(edited.glyphs.at('A').offset, 0);
    session.saveFonts(directory / "export.lua");
    session.loadFonts(directory / "export.lua");
    EXPECT_DOUBLE_EQ(
        session.registry().inspect("default").glyphs.at('g').height, .8);
    auto bad = session.fonts;
    bad[0].atlases[0].file = "missing.tga";
    EXPECT_THROW(session.applyFonts(bad), std::runtime_error);
    EXPECT_DOUBLE_EQ(
        session.registry().inspect("default").glyphs.at('g').offset, -.2);
    EXPECT_TRUE(glIsTexture(
        session.registry().inspect("default").glyphs.at('g').texture));
    bad = session.fonts;
    bad[0].atlases[0].grid = {0, 0};
    EXPECT_THROW(session.applyFonts(bad), std::runtime_error);
    EXPECT_EQ(session.fonts[0].atlases[0].grid.columns, 2);
    EXPECT_TRUE(glIsTexture(
        session.registry().inspect("default").glyphs.at('g').texture));
    bad = session.fonts;
    bad[0].atlases[0].grid.columns = 3;
    EXPECT_THROW(session.applyFonts(bad), std::runtime_error);
    EXPECT_DOUBLE_EQ(
        session.registry().inspect("default").glyphs.at('g').offset, -.2);
}

TEST_F(GuiGL, LocalMenuContextRendersAndModalBlocksUnderlyingInput) {
    GuiStudio::Session session(directory, directory);
    session.testing = true;
    session.rebuild();
    auto *modal = session.preview();
    ASSERT_TRUE(modal->modal);
    modal->select(1);
    CControls::MenuEvent event;
    event.key = GLFW_KEY_ENTER;
    event.action = GLFW_PRESS;
    session.context.events.push_back(event);
    session.update();
    event.kind = CControls::MenuEvent::Character;
    event.codepoint = 'A';
    session.context.events.push_back(event);
    session.update();
    EXPECT_NE(modal->options[1].preText.find('A'), std::string::npos);
    glViewport(0, 0, 640, 480);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_TEXTURE_2D);
    CEngine::engine().invalidateRenderBindings();
    glEnable(GL_SCISSOR_TEST);
    glScissor(10, 20, 600, 400);
    session.render();
    GLint scissor[4]{};
    glGetIntegerv(GL_SCISSOR_BOX, scissor);
    EXPECT_EQ(scissor[0], 10);
    EXPECT_EQ(scissor[1], 20);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    event = {};
    event.key = GLFW_KEY_ESCAPE;
    event.action = GLFW_PRESS;
    session.context.events.push_back(event);
    session.update();
    session.context.events.push_back(event);
    session.update();
    EXPECT_FALSE(session.preview()->modal);
    session.rebuild();
    EXPECT_TRUE(session.preview()->modal);
}

TEST_F(GuiGL, LocalTranslationsAndProtectedAtlasExport) {
    GuiStudio::fs::create_directories(directory / "data/languages/PT-BR");
    ui::writeTextFile(directory / "data/languages/PT-BR/PT-BR.lua",
                      "menuPlayTitle = 'Jogar'\n");
    GuiStudio::Session session(directory, directory);
    auto definition = ui::defaultModals()[0];
    definition.options[2].languageKey = "menuPlayTitle";
    auto menu = ui::instantiate(definition, &session.context);
    EXPECT_EQ(menu->options[2].text, "Jogar");
    const auto atlas = directory / "data/sprites/FONT.tga";
    const auto size = GuiStudio::fs::file_size(atlas);
    EXPECT_THROW(session.saveFonts(atlas), std::runtime_error);
    EXPECT_EQ(GuiStudio::fs::file_size(atlas), size);
}

TEST_F(GuiGL, PreviewNavigationReturnsWithoutExecutingExternalActions) {
    GuiStudio::Session session(directory, directory);
    ui::MenuDefinition first, second;
    first.id = "first";
    second.id = "second";
    ui::OptionDefinition forward, back;
    forward.id = "forward";
    forward.text = "Next";
    forward.shortcut = GLFW_KEY_1;
    forward.actions = {"menu:second"};
    first.options.push_back(forward);
    back.id = "back";
    back.text = "Back";
    back.shortcut = GLFW_KEY_2;
    back.back = true;
    second.options.push_back(back);
    session.menus = {first, second};
    session.testing = true;
    session.rebuild();
    CControls::MenuEvent event;
    event.key = GLFW_KEY_1;
    event.action = GLFW_PRESS;
    session.context.events.push_back(event);
    session.update();
    EXPECT_EQ(session.selectedMenu, 1);
    event.key = GLFW_KEY_2;
    session.context.events.push_back(event);
    session.update();
    EXPECT_EQ(session.selectedMenu, 0);
    session.menus[0].options[0].actions = {"start-game"};
    session.rebuild();
    event.key = GLFW_KEY_1;
    session.context.events.push_back(event);
    session.update();
    EXPECT_EQ(session.events.back(), "start-game");
    EXPECT_EQ(session.selectedMenu, 0);
}

TEST_F(GuiFiles, NonStringLuaErrorsProduceUsefulDiagnostics) {
    const auto path = directory / "bad.lua";
    ui::writeTextFile(path, "error({reason='invalid'})");
    try {
        GuiStudio::readFonts(path);
        FAIL() << "Expected import failure";
    } catch (const std::runtime_error &error) {
        EXPECT_STREQ(error.what(), "Lua raised a non-string error");
    }
}

TEST_F(GuiFiles, LuaExportsIgnoreTheProcessNumericLocale) {
    struct Comma : std::numpunct<char> {
        char do_decimal_point() const override { return ','; }
        char do_thousands_sep() const override { return '.'; }
        std::string do_grouping() const override { return "\3"; }
    };
    struct RestoreLocale {
        std::locale original = std::locale();
        ~RestoreLocale() { std::locale::global(original); }
    } restore;
    std::locale::global(std::locale(std::locale::classic(), new Comma));
    auto menus = ui::defaultModals();
    menus[0].x = .125;
    menus[0].options[1].maxBytes = 1234;
    ui::writeMenus(directory / "Gui.lua", menus);
    const auto loadedMenus = ui::readMenus(directory / "Gui.lua");
    EXPECT_DOUBLE_EQ(loadedMenus[0].x, .125);
    EXPECT_EQ(loadedMenus[0].options[1].maxBytes, 1234u);
    auto fonts = GuiStudio::defaultFonts();
    fonts[0].adjustments['g'] = {.95, -.125, .7, -.05};
    GuiStudio::writeFonts(directory / "Font.lua", fonts);
    const auto loadedFonts = GuiStudio::readFonts(directory / "Font.lua");
    EXPECT_EQ(loadedFonts[0].adjustments.at('g').offset, -.125);
}
