#include "CFonts.h"
#include "CLuaFunctions.hpp"
#include "CMenu.h"
#include "CUIOverrides.h"
#include "GPPGame.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

TEST(FontMetricsGL, LuaMetricsAffectDirectBufferedAnd3DText) {
    ASSERT_TRUE(glfwInit());
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    auto *window = glfwCreateWindow(100, 100, "font metrics", nullptr, nullptr);
    ASSERT_NE(window, nullptr);
    glfwMakeContextCurrent(window);
    ASSERT_EQ(glewInit(), GLEW_OK);
    glViewport(0, 0, 100, 100);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    GPPGame::GuitarPP().mainthread = std::this_thread::get_id();
    GPPGame::GuitarPP().initializeTextures();

    // Two rows of three square cells, independent of the installed theme.
    auto path =
        std::filesystem::temp_directory_path() /
        ("gpp-font-test-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(path);
    {
        std::ofstream file(path / "atlas.tga", std::ios::binary);
        const unsigned char header[18] = {0, 0, 2, 0,  0, 0,  0, 0,  0,
                                          0, 0, 0, 96, 0, 64, 0, 32, 8};
        file.write(reinterpret_cast<const char *>(header), sizeof(header));
        std::string pixels(96 * 64 * 4, '\xff');
        file.write(pixels.data(), static_cast<std::streamsize>(pixels.size()));
    }

    auto &fonts = CFonts::fonts();
    fonts.addTextureToFont("metrics-test", path.string(), "atlas.tga", "bgé\n");
    auto L = CLuaH::make_luaState();
    luaL_openlibs(L.get());
    CLuaFunctions::LuaF().registerFunctions(L);
    const double width = fonts.getXSizeInScreen("gbé", 0.4, "metrics-test");
    ASSERT_EQ(
        luaL_dostring(
            L.get(),
            "assert(setFontCharMetrics('metrics-test', 'gé', 0.5, -0.25))\n"
            "assert(not setFontCharMetrics('metrics-test', 'g?', 2, 0))\n"
            "assert(not setFontCharMetrics('missing', 'g', 1, 0))\n"
            "assert(not setFontCharMetrics('metrics-test', 'g', 0, 0))\n"
            "assert(not setFontCharMetrics('metrics-test', 'g', 1, "
            "math.huge))\n"
            "assert(not setFontCharMetrics('metrics-test', 'g', '1', 0))\n"
            "assert(not setFontCharMetrics('metrics-test', string.char(255), "
            "1, 0))"),
        0);
    EXPECT_DOUBLE_EQ(fonts.getXSizeInScreen("gbé", 0.4, "metrics-test"), width);

    auto capture = [&](int mode, const std::string &text) {
        std::array<GLfloat, 256> result{};
        glFeedbackBuffer(static_cast<GLsizei>(result.size()), GL_3D,
                         result.data());
        glRenderMode(GL_FEEDBACK);
        if (mode == 0) {
            fonts.drawTextInScreen(text, -0.8, 0, 0.4, "metrics-test");
        } else if (mode == 1) {
            fonts.drawTextInScreenWithBuffer(text, -0.8, 0, 0.4,
                                             "metrics-test");
            fonts.drawAllBuffers();
        } else {
            fonts.draw3DTextInScreen(text, -0.8, 0, 0, 0.4, 0.4, 0.2,
                                     "metrics-test");
        }

        EXPECT_GT(glRenderMode(GL_RENDER), 0);
        return result;
    };

    for (int mode = 0; mode < 3; ++mode) {
        auto result = capture(mode, "gbé");
        // GL feedback: polygon token, vertex count, then XYZ for each vertex.
        EXPECT_FLOAT_EQ(result[3], 55);  // g top: 0.1 in normalized coordinates
        EXPECT_FLOAT_EQ(result[9], 45);  // g bottom: -0.1
        EXPECT_FLOAT_EQ(result[25], 70); // b remains at original height
        EXPECT_FLOAT_EQ(result[31], 50); // b remains on the original line
        EXPECT_FLOAT_EQ(result[47], 55); // UTF-8 é gets the same metrics as g
        if (mode == 2) {
            EXPECT_FLOAT_EQ(result[4], 0.525f);
            EXPECT_FLOAT_EQ(result[10], 0.475f);
        }
    }

    fonts.addTextureToFont("metrics-test", path.string(), "atlas.tga", "bgé\n");
    auto reset = capture(0, "g");
    EXPECT_FLOAT_EQ(reset[3], 70);
    EXPECT_FLOAT_EQ(reset[9], 50);
    std::filesystem::remove_all(path);
    GPPGame::GuitarPP().shutdownTextures();
    glfwDestroyWindow(window);
    glfwTerminate();
}

namespace {
class FontAtlasGL : public ::testing::Test {
  protected:
    GLFWwindow *window = nullptr;
    std::filesystem::path path;
    void SetUp() override {
        ASSERT_TRUE(glfwInit());
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        window = glfwCreateWindow(128, 128, "atlas tests", nullptr, nullptr);
        ASSERT_NE(window, nullptr);
        glfwMakeContextCurrent(window);
        ASSERT_EQ(glewInit(), GLEW_OK);
        glViewport(0, 0, 128, 128);
        // Match the engine's initial fixed-function state; menus inherit it.
        glEnable(GL_TEXTURE_2D);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        GPPGame::GuitarPP().mainthread = std::this_thread::get_id();
        GPPGame::GuitarPP().initializeTextures();
        CEngine::engine().invalidateRenderBindings();
        path =
            std::filesystem::temp_directory_path() /
            ("gpp-atlas-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(path);
    }

    void TearDown() override {
        std::filesystem::remove_all(path);
        GPPGame::GuitarPP().shutdownTextures();
        if (window) {
            glfwDestroyWindow(window);
        }

        glfwTerminate();
    }

    void atlas(const char *name, bool solid = false) {
        std::ofstream out(path / name, std::ios::binary);
        const unsigned char header[18] = {0, 0, 2, 0,  0, 0, 0, 0,  0,
                                          0, 0, 0, 24, 0, 8, 0, 32, 8};
        out.write(reinterpret_cast<const char *>(header), 18);
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 24; ++x) {
                // Upper row: narrow a, wide b, transparent c. Lower row: ?.
                const bool ink =
                    solid ||
                    (y >= 4 && ((x >= 2 && x < 4) || (x >= 9 && x < 15))) ||
                    (y < 4 && x < 8);
                const unsigned char pixel[4] = {
                    255, 255, 255, static_cast<unsigned char>(ink ? 255 : 0)};
                out.write(reinterpret_cast<const char *>(pixel), 4);
            }
        }
    }
};
} // namespace

TEST_F(FontAtlasGL,
       ExplicitGridPartialRowsRecadastroAndFailureAreTransactional) {
    atlas("atlas.tga");
    auto &f = CFonts::fonts();
    ASSERT_FALSE(f.addTextureToFont("grid-test", path.string(), "atlas.tga",
                                    "abc\n?", {3, 2})
                     .empty());
    CFonts::TextLayout l;
    f.layoutText("abc?", l, "grid-test");
    ASSERT_EQ(l.carets.size(), 5u);
    EXPECT_LT(l.carets[1], l.carets[2] - l.carets[1]);
    EXPECT_EQ(l.glyphs.size(), 3u);
    const double width = f.getXSizeInScreen("abc?", 1, "grid-test");
    ASSERT_FALSE(f.addTextureToFont("grid-test", path.string(), "atlas.tga",
                                    "abc\n?\n", {3, 2})
                     .empty());
    EXPECT_DOUBLE_EQ(f.getXSizeInScreen("abc?", 1, "grid-test"), width);
    EXPECT_TRUE(f.addTextureToFont("grid-test", path.string(), "atlas.tga",
                                   "abcdefg", {3, 2})
                    .empty());
    EXPECT_TRUE(f.addTextureToFont("grid-test", path.string(), "atlas.tga",
                                   "abc", {5, 2})
                    .empty());
    EXPECT_TRUE(f.addTextureToFont("grid-test", path.string(), "atlas.tga",
                                   "\xff", {3, 2})
                    .empty());
    EXPECT_DOUBLE_EQ(f.getXSizeInScreen("abc?", 1, "grid-test"), width);
    EXPECT_TRUE(f.setFontCharSpacing("grid-test", "a", .2, -.1));
    ASSERT_FALSE(f.addTextureToFont("grid-test", path.string(), "atlas.tga",
                                    "abc\n?", {3, 2})
                     .empty());
    EXPECT_DOUBLE_EQ(f.getXSizeInScreen("abc?", 1, "grid-test"), width);
    EXPECT_DOUBLE_EQ(f.getXSizeInScreen("!", 1, "grid-test"),
                     f.getXSizeInScreen("?", 1, "grid-test"));
    const auto revision = f.revision();
    f.getXSizeInScreen("unknown", 1, "not-registered");
    EXPECT_EQ(f.revision(), revision);
}

TEST_F(FontAtlasGL,
       PreviouslyUploadedAtlasCanBeRegisteredWithoutAnotherUpload) {
    atlas("late.tga");
    auto &game = GPPGame::GuitarPP();
    const auto &texture = game.loadTexture(path.string(), "late.tga");
    const auto id = texture.getTextId();
    ASSERT_TRUE(glIsTexture(id));
    EXPECT_EQ(texture.getImageData(), nullptr);
    auto &fonts = CFonts::fonts();
    EXPECT_FALSE(fonts
                     .addTextureToFont("late-atlas", path.string(), "late.tga",
                                       "abc\n?", {3, 2})
                     .empty());
    EXPECT_EQ(game.getTextureId(path.string() + "/late.tga"), id);
    EXPECT_GT(fonts.getXSizeInScreen("ab?", 1, "late-atlas"), 0);
}

TEST_F(FontAtlasGL, DirectBufferedAnd3DHaveIdenticalProportionalPositions) {
    atlas("atlas.tga");
    auto &f = CFonts::fonts();
    ASSERT_FALSE(f.addTextureToFont("positions-test", path.string(),
                                    "atlas.tga", "abc\n?", {3, 2})
                     .empty());
    auto capture = [&](int mode) {
        std::array<GLfloat, 512> data{};
        glFeedbackBuffer(static_cast<GLsizei>(data.size()), GL_3D, data.data());
        glRenderMode(GL_FEEDBACK);
        if (mode == 0) {
            f.drawTextInScreen("aba", -.8, 0, .2, "positions-test");
        }

        if (mode == 1) {
            f.drawTextInScreenWithBuffer("aba", -.8, 0, .2, "positions-test");
            f.drawAllBuffers();
        }

        if (mode == 2) {
            f.draw3DTextInScreen("aba", -.8, 0, 0, .2, .2, 0, "positions-test");
        }

        EXPECT_GT(glRenderMode(GL_RENDER), 0);
        return data;
    };

    auto direct = capture(0), buffered = capture(1), threeD = capture(2);
    EXPECT_EQ(direct, buffered);
    EXPECT_EQ(direct, threeD);
    CFonts::TextLayout l;
    f.layoutText("aba", l, "positions-test");
    EXPECT_DOUBLE_EQ(f.getXSizeInScreen("aba", .2, "positions-test"),
                     l.width() * .2);
    EXPECT_DOUBLE_EQ(f.getCenterPos("aba", .2, 0, "positions-test"),
                     -(l.left + l.right) * .1);
}

TEST_F(FontAtlasGL, BufferPreservesAtlasOrderAndCapturedColor) {
    atlas("one.tga", true);
    atlas("two.tga", true);
    auto &f = CFonts::fonts();
    ASSERT_FALSE(
        f.addTextureToFont("order-test", path.string(), "one.tga", "a", {1, 1})
            .empty());
    ASSERT_FALSE(
        f.addTextureToFont("order-test", path.string(), "two.tga", "b", {1, 1})
            .empty());
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor4d(1, 0, 0, 1);
    f.drawTextInScreenWithBuffer("a", -.5, -.5, .5, "order-test");
    glColor4d(0, 1, 0, 1);
    f.drawTextInScreenWithBuffer("b", -.5, -.5, .5, "order-test");
    glColor4d(0, 0, 1, 1);
    f.drawTextInScreenWithBuffer("a", -.5, -.5, .5, "order-test");
    f.drawAllBuffers();
    unsigned char pixel[4];
    glReadPixels(50, 50, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    EXPECT_EQ(pixel[0], 0);
    EXPECT_EQ(pixel[1], 0);
    EXPECT_EQ(pixel[2], 255);
}

TEST_F(FontAtlasGL, UnchangedRegistrationDoesNotInvalidateLayouts) {
    atlas("atlas.tga");
    auto &f = CFonts::fonts();
    ASSERT_FALSE(f.addTextureToFont("stable-test", path.string(), "atlas.tga",
                                    "abc\n?", {3, 2})
                     .empty());
    const size_t generation = f.revision();
    ASSERT_FALSE(f.addTextureToFont("stable-test", path.string(), "atlas.tga",
                                    "abc\n?", {3, 2})
                     .empty());
    EXPECT_EQ(f.revision(), generation);
}

TEST_F(FontAtlasGL, MenuRestoresMatricesColorAndScissor) {
    atlas("atlas.tga");
    auto &f = CFonts::fonts();
    ASSERT_FALSE(f.addTextureToFont("default", path.string(), "atlas.tga",
                                    "abc\n?", {3, 2})
                     .empty());
    CMenu menu;
    CMenu::menuOpt o;
    o.text = "aba";
    o.type = CMenu::textbtn;
    menu.addOpt(o);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glScaled(.7, .8, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslated(.1, .2, 0);
    glMatrixMode(GL_TEXTURE);
    glEnable(GL_SCISSOR_TEST);
    glScissor(10, 11, 70, 60);
    glColor4d(.2, .3, .4, .5);
    GLdouble projection[16], model[16], color[4];
    GLint scissor[4], mode;
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    menu.render();
    GLdouble after[16];
    glGetDoublev(GL_PROJECTION_MATRIX, after);
    EXPECT_TRUE(std::equal(after, after + 16, projection));
    glGetDoublev(GL_MODELVIEW_MATRIX, after);
    EXPECT_TRUE(std::equal(after, after + 16, model));
    glGetIntegerv(GL_MATRIX_MODE, &mode);
    EXPECT_EQ(mode, GL_TEXTURE);
    glGetIntegerv(GL_SCISSOR_BOX, scissor);
    EXPECT_EQ(scissor[0], 10);
    EXPECT_EQ(scissor[2], 70);
    glGetDoublev(GL_CURRENT_COLOR, color);
    EXPECT_NEAR(color[3], .5, 1e-6);
    EXPECT_TRUE(glIsEnabled(GL_SCISSOR_TEST));
}

TEST_F(FontAtlasGL, MenuStyleChangesColorsAndKeepsUnitGlyphCache) {
    struct RestoreStyle {
        ui::Style previous = ui::activeStyle();
        ~RestoreStyle() { ui::setActiveStyle(previous); }
    } restore;
    atlas("style.tga", true);
    auto &fonts = CFonts::fonts();
    ASSERT_FALSE(fonts
                     .addTextureToFont("default", path.string(), "style.tga",
                                       "a", {1, 1})
                     .empty());
    CMenu menu;
    CMenu::menuOpt option;
    option.type = CMenu::textbtn;
    option.text = "a";
    option.x = -.7;
    option.y = -.6;
    menu.addOpt(option);
    option.y = .1;
    option.styleOverrides.textColor = ui::Color{0, 1, 0, 1};
    menu.addOpt(option);
    ui::Style style;
    style.typography.textSize = .25;
    style.typography.textColor = {1, 0, 0, 1};
    ui::setActiveStyle(style);
    auto countColors = [&]() {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        menu.render();
        std::array<unsigned char, 128 * 128 * 3> pixels{};
        glReadPixels(0, 0, 128, 128, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        std::array<int, 3> count{};
        for (size_t i = 0; i < pixels.size(); i += 3) {
            for (size_t channel = 0; channel < 3; ++channel) {
                if (pixels[i + channel] > 200 &&
                    pixels[i + (channel + 1) % 3] < 20 &&
                    pixels[i + (channel + 2) % 3] < 20) {
                    ++count[channel];
                }
            }
        }

        return count;
    };
    auto first = countColors();
    EXPECT_GT(first[0], 0);
    EXPECT_GT(first[1], 0);
    EXPECT_EQ(first[2], 0);
    const auto *glyphs = menu.options[0].layout.glyphs.data();
    const auto revision = menu.options[0].cachedRevision;
    const auto carets = menu.options[0].layout.carets;
    style.typography.textColor = {0, 0, 1, 1};
    style.typography.textSize = .35;
    ui::setActiveStyle(style);
    auto second = countColors();
    EXPECT_EQ(second[0], 0);
    EXPECT_GT(second[1], first[1]);
    EXPECT_GT(second[2], first[0]);
    EXPECT_EQ(menu.options[0].layout.glyphs.data(), glyphs);
    EXPECT_EQ(menu.options[0].cachedRevision, revision);
    EXPECT_EQ(menu.options[0].layout.carets, carets);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

namespace {
std::array<unsigned char, 3> menuPixel(int x, int y) {
    std::array<unsigned char, 3> pixel{};
    glReadPixels(x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel.data());
    return pixel;
}
} // namespace

TEST_F(FontAtlasGL, MenuImagesPanelsAndBordersUseExplicitRectangles) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    const unsigned char blue[] = {0, 0, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 blue);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    CMenu::Context context;
    CMenu menu("visual", &context);
    CMenu::menuOpt panel;
    panel.type = CMenu::panel;
    panel.x = panel.y = -.75;
    panel.width = panel.height = 1.5;
    panel.styleOverrides.background = ui::Color{1, 0, 0, 1};
    panel.styleOverrides.border = ui::Color{0, 1, 0, 1};
    panel.styleOverrides.borderWidth = .1;
    menu.addOpt(panel);
    CMenu::menuOpt image;
    image.type = CMenu::image;
    image.x = image.y = -.25;
    image.width = image.height = .5;
    image.texture = texture;
    menu.addOpt(image);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    menu.render();
    EXPECT_EQ(menuPixel(64, 64), (std::array<unsigned char, 3>{0, 0, 255}));
    EXPECT_EQ(menuPixel(32, 32), (std::array<unsigned char, 3>{255, 0, 0}));
    EXPECT_EQ(menuPixel(18, 64), (std::array<unsigned char, 3>{0, 255, 0}));
    EXPECT_EQ(menuPixel(8, 64), (std::array<unsigned char, 3>{0, 0, 0}));
    menu.options[1].texture = 0;
    glClear(GL_COLOR_BUFFER_BIT);
    menu.render();
    EXPECT_EQ(menuPixel(64, 64), (std::array<unsigned char, 3>{255, 0, 0}));
    glDeleteTextures(1, &texture);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(FontAtlasGL,
       TableCellsClipAgainstColumnViewportAndCallerAndCacheLayouts) {
    atlas("solid.tga", true);
    auto &fonts = CFonts::fonts();
    ASSERT_FALSE(fonts
                     .addTextureToFont("table-test", path.string(), "solid.tga",
                                       "aé", {2, 1})
                     .empty());
    CMenu::Context context;
    context.fonts = &fonts;
    context.font = "table-test";
    context.style.typography.textSize = .3;
    context.style.layout.padding = 0;
    CMenu menu("table", &context);
    menu.tableColumns = {.5, .5};
    menu.viewportEnabled = true;
    menu.viewport = {-.5, -.5, .5, .5};
    CMenu::menuOpt row;
    row.type = CMenu::table_row;
    row.x = -.5;
    row.y = -.25;
    row.height = .5;
    row.group = 1;
    row.cells = {"aéaaaaaaaaaaaaaaaaa", ""};
    menu.addOpt(row);
    row.y = -2;
    menu.addOpt(row);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(40, 48, 55, 32);
    menu.render();
    constexpr size_t pixelSide = 128;
    std::array<unsigned char, pixelSide * pixelSide * 3> pixels{};
    glReadPixels(0, 0, 128, 128, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    int white = 0;
    for (size_t y = 0; y < 128; ++y) {
        for (size_t x = 0; x < 128; ++x) {
            const auto offset = (y * 128 + x) * 3;
            if (pixels[offset] > 200) {
                ++white;
                EXPECT_GE(x, 40);
                EXPECT_LT(x, 64); // The next, empty column must stay empty.
                EXPECT_GE(y, 48);
                EXPECT_LT(y, 80);
            }
        }
    }
    EXPECT_GT(white, 0);
    ASSERT_EQ(menu.options[0].cellLayouts.size(), 2u);
    EXPECT_TRUE(menu.options[1].cellLayouts.empty());
    const auto *cached = menu.options[0].cellLayouts[0].layout.glyphs.data();
    menu.render();
    EXPECT_EQ(menu.options[0].cellLayouts[0].layout.glyphs.data(), cached);
    menu.options[0].cells[0] = "é";
    menu.render();
    EXPECT_EQ(menu.options[0].cellLayouts[0].text, "é");
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(FontAtlasGL, PanelLayerAndPersistentSelectionDoNotFollowKeyboardFocus) {
    CMenu::Context context;
    context.mouseX = context.mouseY = 10;
    context.style.selection.background = {0, 0, 1, 1};
    context.style.selection.accent = {1, 1, 0, 1};
    CMenu root("root", &context);
    CMenu model("rows", &context);
    model.tableColumns = {1};
    CMenu::menuOpt row;
    row.type = CMenu::table_row;
    row.x = -.5;
    row.y = -.5;
    row.height = .3;
    row.active = true;
    model.addOpt(row);
    row.y = .2;
    row.active = false;
    model.addOpt(row);
    auto &child = *CMenu::getUiAt(static_cast<size_t>(
                                      root.pushPanel(model, {-.4, -1, 1, 1})))
                       .m;
    root.update();
    root.focusInterface(child);
    child.select(1);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    CMenu::renderUiList();
    EXPECT_EQ(menuPixel(64, 40), (std::array<unsigned char, 3>{0, 0, 255}));
    EXPECT_EQ(menuPixel(64, 86), (std::array<unsigned char, 3>{0, 0, 255}));
    EXPECT_EQ(menuPixel(33, 40), (std::array<unsigned char, 3>{
                                     0, 0, 0})); // Panel clips its children.
    CMenu modal("modal", &context);
    modal.modal = true;
    CMenu::menuOpt cover;
    cover.type = CMenu::panel;
    cover.x = cover.y = -.5;
    cover.width = cover.height = 1;
    cover.styleOverrides.background = ui::Color{1, 0, 0, 1};
    modal.addOpt(cover);
    root.pushUserInterface(modal);
    root.pushPanel(model, {-1, -1, 1, 1});
    glClear(GL_COLOR_BUFFER_BIT);
    CMenu::renderUiList();
    EXPECT_EQ(menuPixel(64, 40), (std::array<unsigned char, 3>{255, 0, 0}));
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(FontAtlasGL, MenuKeepsLegacyClientArrayHandoffForCursor) {
    // The host enters menus with CPU arrays and the fixed-function pipeline.
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    const GLfloat before[] = {0, 0, 0};
    glVertexPointer(3, GL_FLOAT, 0, before);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    const unsigned char white[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 white);
    glBindTexture(GL_TEXTURE_2D, 0);

    CMenu::Context context;
    CMenu menu("legacy-handoff", &context);
    CMenu::menuOpt panel;
    panel.type = CMenu::image;
    panel.texture = texture;
    panel.width = panel.height = .5;
    menu.addOpt(panel);
    menu.render();

    GLint size = 0, type = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_SIZE, &size);
    glGetIntegerv(GL_VERTEX_ARRAY_TYPE, &type);
    EXPECT_EQ(size, 2);
    EXPECT_EQ(type, GL_DOUBLE);
    GLint binding = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    EXPECT_EQ(static_cast<GLuint>(binding), texture);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &texture);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(FontAtlasGL, ResizingPanelBoundsUpdatesClippingAndRestoresWithTheme) {
    CMenu::Context context;
    CMenu root("root", &context), model("panel", &context);
    model.viewport = {-.5, -.5, 0, .5};
    CMenu::menuOpt box;
    box.type = CMenu::panel;
    box.x = box.y = -.5;
    box.width = box.height = 1;
    box.styleOverrides.background = ui::Color{1, 0, 0, 1};
    model.addOpt(box);
    auto &child = *CMenu::getUiAt(root.pushPanel(model)).m;
    auto draw = [&] {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        CMenu::renderUiList();
    };
    const std::array<unsigned char, 3> black{0, 0, 0}, red{255, 0, 0};
    draw();
    EXPECT_EQ(menuPixel(48, 64), red);
    EXPECT_EQ(menuPixel(80, 64), black);
    ui::MenuOverrides patch;
    patch.viewport = CMenu::Rect{-.5, -.5, .5, .5};
    child.applyPresentationOverrides(patch);
    draw();
    EXPECT_EQ(menuPixel(80, 64), red);
    child.applyPresentationOverrides({});
    draw();
    EXPECT_EQ(menuPixel(80, 64), black);
    patch.panelBounds = CMenu::Rect{0, -.5, .5, .5};
    child.applyPresentationOverrides(patch);
    draw();
    EXPECT_EQ(menuPixel(48, 64), black);
    EXPECT_EQ(menuPixel(80, 64), red);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(FontAtlasGL, RendererNestedClipsUseViewportOriginAndKeepCallerClip) {
    glViewport(16, 8, 96, 64);
    glEnable(GL_SCISSOR_TEST);
    glScissor(30, 20, 60, 40);
    auto &renderer = CEngine::engine().uiRenderer();
    {
        ui::RenderPass outer(renderer);
        EXPECT_DOUBLE_EQ(outer.surface.aspect, 1.5);
        renderer.clip(ui::Rect{0, -1, 1.5, 1});
        GLint box[4];
        glGetIntegerv(GL_SCISSOR_BOX, box);
        EXPECT_EQ(box[0], 64);
        EXPECT_EQ(box[1], 20);
        EXPECT_EQ(box[2], 26);
        EXPECT_EQ(box[3], 40);
        {
            ui::RenderPass inner(renderer);
            renderer.clip(ui::Rect{-1.5, -1, 1.5, 0});
            renderer.clip(std::nullopt);
            glGetIntegerv(GL_SCISSOR_BOX, box);
            EXPECT_EQ(box[0], 64);
            EXPECT_EQ(box[2], 26);
            EXPECT_EQ(box[3], 40);
        }
        renderer.clip(std::nullopt);
        glGetIntegerv(GL_SCISSOR_BOX, box);
        EXPECT_EQ(box[0], 30);
        EXPECT_EQ(box[2], 60);
    }
    EXPECT_TRUE(glIsEnabled(GL_SCISSOR_TEST));
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(FontAtlasGL, EngineColorCacheTracksRestoredStateAndBufferedFonts) {
    auto &engine = CEngine::engine();
    engine.invalidateRenderBindings();
    engine.setColor(.2, .3, .4, .5);
    glPushAttrib(GL_CURRENT_BIT);
    engine.setColor(1, 0, 0, 1);
    glPopAttrib();
    engine.invalidateRenderBindings();
    // Reapply the last requested color after the real state was restored.
    engine.setColor(1, 0, 0, 1);
    EXPECT_EQ(engine.getColor(), (std::array<double, 4>{1, 0, 0, 1}));

    atlas("color-cache.tga", true);
    CFonts fonts;
    ASSERT_FALSE(fonts
                     .addTextureToFont("default", path.string(),
                                       "color-cache.tga", "a", {1, 1})
                     .empty());
    engine.setColor(0, 1, 0, 1);
    fonts.drawTextInScreenWithBuffer("a", 0, 0, .2);
    engine.setColor(0, 0, 1, 1);
    fonts.drawAllBuffers();
    EXPECT_EQ(engine.getColor(), (std::array<double, 4>{0, 0, 1, 1}));
    engine.setColor(0, 1, 0, 1);
    EXPECT_EQ(engine.getColor(), (std::array<double, 4>{0, 1, 0, 1}));
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(FontAtlasGL, EngineUIUploadMapsCPUFormatsAndRestoresBindings) {
    auto &engine = CEngine::engine();
    GLuint previous = 0;
    glGenTextures(1, &previous);
    glBindTexture(GL_TEXTURE_2D, previous);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
    using Format = CTextureImage::PixelFormat;
    for (const auto format :
         {Format::Alpha, Format::Luminance, Format::RGB, Format::RGBA}) {
        CTextureImage image;
        image.Width = image.Height = 1;
        image.Format = format;
        image.BytesPerPixel = format == Format::RGBA  ? 4
                              : format == Format::RGB ? 3
                                                      : 1;
        image.DataBytes = static_cast<size_t>(image.BytesPerPixel);
        image.Data = std::make_unique<unsigned char[]>(image.DataBytes);
        std::fill_n(image.Data.get(), image.DataBytes, 255);
        const unsigned texture = engine.uploadUITexture(image);
        ASSERT_TRUE(engine.isTextureValid(texture));
        GLint binding = 0, alignment = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        EXPECT_EQ(static_cast<unsigned>(binding), previous);
        EXPECT_EQ(alignment, 8);
        glBindTexture(GL_TEXTURE_2D, texture);
        std::array<unsigned char, 4> pixels{};
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                      pixels.data());
        EXPECT_EQ(pixels[3], 255);
        EXPECT_EQ(pixels[0], format == Format::Alpha ? 0 : 255);
        engine.destroyTexture(texture);
        EXPECT_FALSE(engine.isTextureValid(texture));
        glBindTexture(GL_TEXTURE_2D, previous);
    }
    engine.destroyTexture(previous);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
