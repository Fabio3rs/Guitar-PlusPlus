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
