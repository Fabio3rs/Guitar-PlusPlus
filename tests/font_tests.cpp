#include "CFonts.h"
#include "CLuaFunctions.hpp"
#include "CMenu.h"
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
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        GPPGame::GuitarPP().mainthread = std::this_thread::get_id();
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
