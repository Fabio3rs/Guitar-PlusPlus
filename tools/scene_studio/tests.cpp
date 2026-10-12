#include "CLog.h"
#include "studio.h"
#include <fstream>
#include <gtest/gtest.h>

using namespace Studio;

class StudioGL : public testing::Test {
  protected:
    std::unique_ptr<Context> context;
    std::unique_ptr<Session> session;
    fs::path scratch;

    void SetUp() override {
        CLog::configureStderr();
        scratch =
            fs::temp_directory_path() /
            ("gpp-studio-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
        const char *backend = std::getenv("GPP_STUDIO_TEST_BACKEND");
        context =
            std::make_unique<Context>(backend ? backend : "glfw", false, false);
        session =
            std::make_unique<Session>(*context, scratch, scratch / "output");
        session->load("scene", "synthetic");
    }

    void TearDown() override {
        session.reset();
        context.reset();
        fs::remove_all(scratch);
    }
};

TEST_F(StudioGL, RestoresFramebufferAndReusesTargetsAndShadows) {
    Target external;
    external.resize(32, 32, context->caps);
    external.bind();
    glViewport(1, 2, 20, 21);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    GLint expected = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &expected);
    session->render(session->viewport, 200, 120);
    const auto texture = session->viewport.texture;
    const auto updates = CBackgroundRenderer::inst().shadowUpdates();
    auto settings = session->settings;
    settings.camera.eye[0] += 1;
    session->apply(settings);
    session->render(session->viewport, 200, 120);
    EXPECT_EQ(session->viewport.texture, texture);
    EXPECT_EQ(CBackgroundRenderer::inst().shadowUpdates(), updates);
    GLint actual = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &actual);
    EXPECT_EQ(actual, expected);
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    EXPECT_EQ(viewport[0], 1);
    EXPECT_EQ(viewport[3], 21);
    EXPECT_TRUE(glIsEnabled(GL_BLEND));
    GLboolean mask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
    EXPECT_FALSE(mask);
    settings.light[0] += 1;
    session->apply(settings);
    session->render(session->viewport, 200, 120);
    EXPECT_EQ(CBackgroundRenderer::inst().shadowUpdates(), updates + 1);
    session->capture("capture.png", 100, 80);
    EXPECT_EQ(session->viewport.texture, texture);
    EXPECT_EQ(session->viewport.width, 200);
    session->reload();
    session->render(session->viewport, 200, 120);
    EXPECT_EQ(CBackgroundRenderer::inst().shadowUpdates(), updates + 2);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(StudioGL, FailedChangesKeepPreviousSceneAndNoPersonalFiles) {
    const auto previous = session->state();
    EXPECT_THROW(session->load("model", (scratch / "missing.obj").string()),
                 Error);
    EXPECT_EQ(session->state(), previous);
    auto invalid = session->settings;
    invalid.camera.eye = invalid.camera.target;
    EXPECT_THROW(session->apply(invalid), Error);
    EXPECT_EQ(session->state(), previous);
    EXPECT_THROW(session->capture("../escape.png", 32, 32), Error);
    EXPECT_THROW(session->capture("bad.png", 0, 32), Error);
    EXPECT_FALSE(fs::exists(scratch / "data/saves"));
}

TEST_F(StudioGL, FailedInitialCameraKeepsSceneSettingsAndShadowCache) {
    session->render(session->viewport, 200, 120);
    auto settings = session->settings;
    settings.light = {-2, 4, 1};
    session->apply(settings);
    session->render(session->viewport, 200, 120);
    const auto previous = session->state();
    const auto updates = CBackgroundRenderer::inst().shadowUpdates();
    const auto obj = scratch / "large.obj";
    {
        std::ofstream file(obj);
        file << "v 2000000 0 0\nv 2000001 0 0\nv 2000000 1 0\nf 1 2 3\n";
    }
    EXPECT_THROW(session->load("model", obj.string()), Error);
    EXPECT_EQ(session->state(), previous);
    session->render(session->viewport, 200, 120);
    EXPECT_EQ(CBackgroundRenderer::inst().shadowUpdates(), updates);
    session->reset();
    EXPECT_EQ(session->settings.light, (Vec{2, 4, 2}));
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(StudioGL, RestrictedDefinitionsDoNotRunHooksOrUnboundedLua) {
    const auto path = scratch / "scene.lua";
    CScene::Definition definition = CScene::Definition::garage();
    std::string error;
    for (const auto *text :
         {"while true do end", "io.open('unexpected','w')",
          "return {model='x', lightPosition={0/0,0,0}}",
          "local s='xxxxxxxx'; for i=1,30 do s=s..s end; return {model=s}"}) {
        {
            std::ofstream out(path);
            out << text;
        }
        EXPECT_FALSE(CScene::readInspectionDefinition(path.string(), "test",
                                                      definition, error));
        EXPECT_EQ(definition.id, "garage");
    }
    {
        std::ofstream out(path);
        out << "return {model='x.obj', onLoaded=function() "
               "io.open('unexpected','w') end}";
    }
    EXPECT_TRUE(CScene::readInspectionDefinition(path.string(), "test",
                                                 definition, error));
    EXPECT_EQ(definition.model, (scratch / "x.obj").string());
    EXPECT_FALSE(fs::exists(scratch / "unexpected"));
}

TEST_F(StudioGL, ToolTextureProviderAndTargetsReleaseOwnedObjects) {
    const auto obj = scratch / "textured.obj";
    {
        std::ofstream out(obj);
        out << "mtllib textured.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl test\nf "
               "1 2 3\n";
    }
    {
        std::ofstream out(scratch / "textured.mtl");
        out << "newmtl test\nmap_Kd pixel.tga\n";
    }
    std::string tga(21, '\0');
    tga[2] = 2;
    tga[12] = 1;
    tga[14] = 1;
    tga[16] = 24;
    tga[20] = char(255);
    {
        std::ofstream out(scratch / "pixel.tga", std::ios::binary);
        out.write(tga.data(), std::streamsize(tga.size()));
    }
    session->load("model", obj.string());
    session->render(session->viewport, 80, 60);
    EXPECT_EQ(session->inspection().materials.at("test").diffuseMap,
              "pixel.tga");
    for (int i = 0; i < 3; ++i) {
        session->reload();
        session->render(session->viewport, 80, 60);
    }
    const GLuint texture = session->viewport.texture;
    EXPECT_TRUE(glIsTexture(texture));
    session.reset();
    EXPECT_FALSE(glIsTexture(texture));
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(StudioGL, PickingSelectsNearestVisiblePartOnBothSides) {
    auto settings = session->settings;
    settings.camera.eye = {0, .7f, 5};
    settings.camera.target = {0, .7f, 0};
    session->apply(settings);
    const auto hit = session->pick(400, 300, 800, 600);
    ASSERT_GE(hit.part, 0);
    EXPECT_EQ(session->inspection().parts[size_t(hit.part)].object, "Cube");
    EXPECT_NEAR(hit.point[2], .6, .001);
    session->pickSelection(400, 300, 800, 600, true);
    EXPECT_EQ(session->selection(), hit.part);
    EXPECT_EQ(session->settings.camera.eye, settings.camera.eye);
    EXPECT_EQ(session->settings.camera.target, hit.point);
    session->visibility("hide", hit.part);
    EXPECT_EQ(session->pick(400, 300, 800, 600).part, -1);
    session->frame(true, 200, 800);
    EXPECT_TRUE(session->isVisible(size_t(hit.part)));
    settings.camera.eye = {0, .7f, 0};
    settings.camera.target = {0, .7f, 2};
    session->apply(settings);
    EXPECT_EQ(session->pick(400, 300, 800, 600).part, hit.part);
    EXPECT_THROW(session->pick(-1, 0, 800, 600), Error);
}

TEST_F(StudioGL, VisibilityInvalidatesShadowsButCameraAndSelectionDoNot) {
    session->render(session->viewport, 200, 120);
    const auto before = CBackgroundRenderer::inst().shadowUpdates();
    session->select(1);
    session->setView("top");
    session->frame(true);
    session->render(session->viewport, 200, 120);
    EXPECT_EQ(CBackgroundRenderer::inst().shadowUpdates(), before);
    session->visibility("hide", 1);
    session->render(session->viewport, 200, 120);
    EXPECT_EQ(CBackgroundRenderer::inst().shadowUpdates(), before + 1);
    session->visibility("hide", 1);
    session->render(session->viewport, 200, 120);
    EXPECT_EQ(CBackgroundRenderer::inst().shadowUpdates(), before + 1);
    session->visibility("isolate", 2);
    EXPECT_FALSE(session->isVisible(0));
    EXPECT_FALSE(session->isVisible(1));
    EXPECT_TRUE(session->isVisible(2));
    session->render(session->viewport, 200, 120);
    EXPECT_EQ(CBackgroundRenderer::inst().shadowUpdates(), before + 2);
    session->visibility("show_all");
    session->render(session->viewport, 200, 120);
    EXPECT_EQ(CBackgroundRenderer::inst().shadowUpdates(), before + 3);
}

TEST_F(StudioGL, EveryModeHonorsVisibilityAndRestoresGLState) {
    auto pixels = [&] {
        session->viewport.bind();
        glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
        return readPixels(120, 80);
    };
    for (int mode = 0; mode < 4; ++mode) {
        for (int quality : {0, 3}) {
            auto settings = session->settings;
            settings.viewMode = mode;
            settings.quality = quality;
            session->apply(settings);
            session->visibility("show_all");
            session->render(session->viewport, 120, 80, false);
            const auto full = pixels();
            for (size_t i = 0; i < session->inspection().parts.size(); ++i) {
                session->visibility("hide", int(i));
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            glEnable(GL_BLEND);
            glDepthMask(GL_FALSE);
            session->render(session->viewport, 120, 80, false);
            GLint modeAfter[2]{};
            glGetIntegerv(GL_POLYGON_MODE, modeAfter);
            EXPECT_EQ(modeAfter[0], GL_POINT);
            EXPECT_TRUE(glIsEnabled(GL_BLEND));
            GLboolean depth = GL_TRUE;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depth);
            EXPECT_FALSE(depth);
            const auto empty = pixels();
            EXPECT_NE(full, empty);
            for (size_t i = 4; i < empty.size(); ++i) {
                ASSERT_EQ(empty[i], empty[i % 4]);
            }
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
        }
    }
}

TEST_F(StudioGL, ReloadClearsPartIDsAndFailedOperationsPreserveSession) {
    session->select(1);
    session->visibility("isolate", 1);
    auto settings = session->settings;
    settings.viewMode = 3;
    settings.axes = true;
    settings.camera.orthographic = true;
    session->apply(settings);
    const auto previous = session->state();
    EXPECT_THROW(session->load("model", "missing.obj"), Error);
    EXPECT_THROW(session->select(1000), Error);
    EXPECT_THROW(session->visibility("isolate", 1000), Error);
    EXPECT_THROW(session->setView("invalid"), Error);
    EXPECT_THROW(session->frame(true, -1, 100), Error);
    EXPECT_EQ(previous, session->state());
    session->reload();
    EXPECT_EQ(session->selection(), -1);
    EXPECT_EQ(session->settings.camera.eye, settings.camera.eye);
    EXPECT_TRUE(session->settings.camera.orthographic);
    EXPECT_EQ(session->settings.viewMode, 3);
    for (size_t i = 0; i < session->inspection().parts.size(); ++i) {
        EXPECT_TRUE(session->isVisible(i));
    }
    EXPECT_THROW(session->frame(true), Error);
}

TEST_F(StudioGL, CleanCapturesOmitHelpersAndDoNotResizeOrMoveViewport) {
    session->render(session->viewport, 300, 150);
    session->select(1);
    auto settings = session->settings;
    settings.grid = settings.axes = settings.bounds = true;
    session->apply(settings);
    session->capture("clean.png", 120, 200);
    session->capture("helpers.png", 120, 200, {}, true);
    EXPECT_EQ(session->settings.camera.eye, settings.camera.eye);
    EXPECT_EQ(session->viewport.width, 300);
    EXPECT_EQ(session->viewport.height, 150);
    session->render(session->viewport, 300, 150, true);
    session->viewport.bind();
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    const auto pixels = readPixels(300, 150);
    bool greenAxis = false;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        greenAxis |=
            pixels[i] == 0 && pixels[i + 1] == 255 && pixels[i + 2] == 0;
    }
    EXPECT_TRUE(
        greenAxis); // Orientation must remain visible inside solid geometry.
    auto bytes = [&](const char *name) {
        std::ifstream file(session->outputRoot / name, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), {});
    };
    EXPECT_NE(bytes("clean.png"), bytes("helpers.png"));
    EXPECT_NE(bytes("clean.png.json").find("\"helpers_included\":false"),
              std::string::npos);
    EXPECT_NE(bytes("helpers.png.json").find("\"helpers_included\":true"),
              std::string::npos);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(StudioGL, FreeNavigationAndProjectionValidateTransactionally) {
    auto settings = session->settings;
    settings.camera.free = true;
    settings.camera.eye = {7, 2, 4};
    settings.camera.target = {7, 2, 3};
    settings.camera.speed = 4;
    session->apply(settings);
    for (int i = 0; i < 10; ++i) {
        session->fly(0, 0, {0, 0, 1}, .1f, false);
    }
    EXPECT_LT(session->settings.camera.eye[2], 3);
    const auto before = session->state();
    settings = session->settings;
    settings.camera.orthographic = true;
    EXPECT_THROW(session->apply(settings), Error);
    EXPECT_THROW(session->navigate(0, 0, 0, 1, 0, 0, 0, -1, 200), Error);
    EXPECT_THROW(session->fly(0, 0, {0, 0, 1}, -1, false), Error);
    EXPECT_EQ(session->state(), before);
}

TEST_F(StudioGL, SmallSelectionInLargeSceneCanContinueZooming) {
    const auto obj = scratch / "detail.obj";
    {
        std::ofstream out(obj);
        out << "o floor\nv -10000 -1 -10000\nv 10000 -1 -10000\nv 0 -1 "
               "10000\nf 1 2 3\n"
               "o detail\nv -.01 0 0\nv .01 0 0\nv 0 .02 0\nf 4 5 6\n";
    }
    session->load("model", obj.string());
    session->select(1);
    session->frame(true);
    const auto before = session->settings.camera;
    session->navigate(0, 0, 0, 0, 1);
    const auto after = session->settings.camera;
    EXPECT_LT(glm::length(CameraMath::vector(after.eye) -
                          CameraMath::vector(after.target)),
              glm::length(CameraMath::vector(before.eye) -
                          CameraMath::vector(before.target)));
    session->render(session->viewport, 200, 120);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(StudioGL, TinyModelCanLoadFrameAndSwitchProjection) {
    const auto obj = scratch / "tiny.obj";
    {
        std::ofstream out(obj);
        out << "v 0 0 0\nv .00001 0 0\nv 0 .00001 0\nf 1 2 3\n";
    }
    ASSERT_NO_THROW(session->load("model", obj.string()));
    ASSERT_NO_THROW(session->frame());
    auto settings = session->settings;
    CameraMath::projectionMode(settings.camera, true);
    ASSERT_NO_THROW(session->apply(settings));
    ASSERT_NO_THROW(session->frame());
    session->render(session->viewport, 200, 120);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(StudioGL,
       UnlitHonorsMaterialUVTransformAndClampAndRestoresTextureMatrix) {
    {
        // The game texture loader accepts TGA; use a two-color 4x1 image.
        const unsigned char tga[] = {0,   0, 2, 0,   0,   0, 0,  0,   0, 0,
                                     0,   0, 4, 0,   1,   0, 24, 0,   0, 0,
                                     255, 0, 0, 255, 255, 0, 0,  255, 0, 0};
        std::ofstream image(scratch / "uv.tga", std::ios::binary);
        image.write(reinterpret_cast<const char *>(tga), sizeof(tga));
    }
    {
        std::ofstream mtl(scratch / "uv.mtl");
        mtl << "newmtl transform\nKd 1 1 1\nmap_Kd -s 2 1 -o .5 0 uv.tga\n"
               "newmtl clamp\nKd 1 1 1\nmap_Kd -clamp on uv.tga\n"
               "newmtl repeat\nKd 1 1 1\nmap_Kd uv.tga\n";
        std::ofstream obj(scratch / "uv.obj");
        obj << "mtllib uv.mtl\nvt .125 .5\nvt 1.125 .5\n";
        for (int part = 0; part < 3; ++part) {
            const char *material[] = {"transform", "clamp", "repeat"};
            obj << "o " << material[part] << "\nusemtl " << material[part]
                << '\n';
            const int x = part * 2 - 3, v = part * 4 + 1;
            obj << "v " << x << " -1 0\nv " << x + 2 << " -1 0\nv " << x + 2
                << " 1 0\nv " << x << " 1 0\n";
            const int uv = part == 0 ? 1 : 2;
            obj << "f " << v << '/' << uv << ' ' << v + 1 << '/' << uv << ' '
                << v + 2 << '/' << uv << ' ' << v + 3 << '/' << uv << '\n';
        }
    }
    session->load("model", (scratch / "uv.obj").string());
    auto settings = session->settings;
    settings.viewMode = 1;
    settings.camera.eye = {0, 0, 5};
    settings.camera.target = {0, 0, 0};
    settings.camera.up = {0, 1, 0};
    settings.camera.orthographic = true;
    settings.camera.orthoHeight = 2;
    session->apply(settings);
    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(.3f, .2f, 0);
    GLfloat before[16], after[16];
    glGetFloatv(GL_TEXTURE_MATRIX, before);
    session->render(session->viewport, 300, 100, false);
    glActiveTexture(GL_TEXTURE0);
    glGetFloatv(GL_TEXTURE_MATRIX, after);
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(before[i], after[i]);
    }
    session->viewport.bind();
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    const auto pixels = readPixels(300, 100);
    for (int part = 0; part < 3; ++part) {
        const size_t offset = size_t(50 * 300 + 50 + part * 100) * 4;
        EXPECT_GT(pixels[offset + (part == 2 ? 0 : 2)], 240) << part;
        EXPECT_LT(pixels[offset + (part == 2 ? 2 : 0)], 15) << part;
    }
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
