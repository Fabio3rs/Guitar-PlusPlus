#include "CFonts.h"
#include "CLuaFunctions.hpp"
#include "GPPGame.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <gtest/gtest.h>
#include <array>
#include <filesystem>
#include <fstream>

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
    auto path = std::filesystem::temp_directory_path() /
                ("gpp-font-test-" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(path);
    {
        std::ofstream file(path / "atlas.tga", std::ios::binary);
        const unsigned char header[18] = {
            0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 96, 0, 64, 0, 32, 8};
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
    ASSERT_EQ(luaL_dostring(L.get(),
        "assert(setFontCharMetrics('metrics-test', 'gé', 0.5, -0.25))\n"
        "assert(not setFontCharMetrics('metrics-test', 'g?', 2, 0))\n"
        "assert(not setFontCharMetrics('missing', 'g', 1, 0))\n"
        "assert(not setFontCharMetrics('metrics-test', 'g', 0, 0))\n"
        "assert(not setFontCharMetrics('metrics-test', 'g', 1, math.huge))\n"
        "assert(not setFontCharMetrics('metrics-test', 'g', '1', 0))\n"
        "assert(not setFontCharMetrics('metrics-test', string.char(255), 1, 0))"), 0);
    EXPECT_DOUBLE_EQ(fonts.getXSizeInScreen("gbé", 0.4, "metrics-test"), width);

    auto capture = [&](int mode, const std::string &text) {
        std::array<GLfloat, 256> result{};
        glFeedbackBuffer(static_cast<GLsizei>(result.size()), GL_3D, result.data());
        glRenderMode(GL_FEEDBACK);
        if (mode == 0)
            fonts.drawTextInScreen(text, -0.8, 0, 0.4, "metrics-test");
        else if (mode == 1) {
            fonts.drawTextInScreenWithBuffer(text, -0.8, 0, 0.4, "metrics-test");
            fonts.drawAllBuffers();
        } else
            fonts.draw3DTextInScreen(text, -0.8, 0, 0, 0.4, 0.4, 0.2, "metrics-test");
        EXPECT_GT(glRenderMode(GL_RENDER), 0);
        return result;
    };
    for (int mode = 0; mode < 3; ++mode) {
        auto result = capture(mode, "gbé");
        // GL feedback: polygon token, vertex count, then XYZ for each vertex.
        EXPECT_FLOAT_EQ(result[3], 55); // g top: 0.1 in normalized coordinates
        EXPECT_FLOAT_EQ(result[9], 45); // g bottom: -0.1
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
