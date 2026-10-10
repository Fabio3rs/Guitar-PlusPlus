#include "CScene.h"
#include "CBackgroundRenderer.h"
#include "CShader.h"
#include "GPPGame.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
class Scenes : public ::testing::Test {
  protected:
    std::filesystem::path directory, previous;
    void SetUp() override {
        previous = std::filesystem::current_path();
        directory = std::filesystem::temp_directory_path() /
            ("gpp-scenes-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory / "data/scenes");
        std::filesystem::create_directories(directory / "data/models");
        std::filesystem::current_path(directory);
    }
    void TearDown() override {
        std::filesystem::current_path(previous);
        std::filesystem::remove_all(directory);
    }
    void write(const std::string &path, const std::string &contents) {
        std::ofstream out(directory / path);
        out << contents;
    }
    std::string contents(const std::string &path) {
        std::ifstream in(directory / path);
        return {std::istreambuf_iterator<char>(in), {}};
    }
    void model() {
        write("data/models/room.obj", "mtllib absent.mtl\nv 2 0 -7\nv 4 0 -7\nv 3 1 -6\nusemtl light\nf 1 2 3\n");
        write("data/scenes/room.mtl", "newmtl light\nKd 0.4 0.5 0.6\n");
    }
};
TEST_F(Scenes, RelativePathsAndLightingAreReadWithoutGPU) {
    write("data/scenes/room.lua", "return {model='../models/room.obj',materials='room.mtl',lightPosition={1,2,3}}");
    CScene::Definition definition;
    CLuaH::luaState_t script;
    std::string error;
    ASSERT_TRUE(CScene::readDefinition("data/scenes/room.lua", "room", definition, script, error)) << error;
    EXPECT_EQ(definition.model, "data/models/room.obj");
    EXPECT_EQ(definition.materials, "data/scenes/room.mtl");
    EXPECT_EQ(definition.light, (std::array<float,3>{1,2,3}));
    EXPECT_FALSE(definition.garageMaterials);
    EXPECT_EQ(lua_gettop(script.get()), 1);
}
TEST_F(Scenes, InvalidDefinitionDoesNotReplaceExistingState) {
    CScene::Definition definition = CScene::Definition::garage();
    CLuaH::luaState_t script;
    std::string error;
    for (const auto *source : {"return 2", "return {}", "error('broken')",
        "return {model='a',lightPosition={1,0/0,3}}", "return {model='a',onLoaded=42}",
        "return {model='a',garageMaterials='yes'}"}) {
        write("data/scenes/bad.lua", source);
        EXPECT_FALSE(CScene::readDefinition("data/scenes/bad.lua", "bad", definition, script, error));
        EXPECT_EQ(definition.id, "garage");
        EXPECT_FALSE(error.empty());
        EXPECT_FALSE(script);
    }
    EXPECT_FALSE(CScene::validId("../models"));
    EXPECT_FALSE(CScene::validId(""));
    EXPECT_TRUE(CScene::validId("chapter_2-show"));
}
TEST_F(Scenes, DefinitionFieldsDoNotInvokeMetamethods) {
    CScene::Definition definition;
    CLuaH::luaState_t script;
    std::string error;
    write("data/scenes/meta.lua", "return setmetatable({model='room.obj'}, {__index=function() error('field failed') end})");
    ASSERT_TRUE(CScene::readDefinition("data/scenes/meta.lua", "meta", definition, script, error)) << error;
    // A required field supplied only by __index is missing, not executable.
    write("data/scenes/meta.lua", "return setmetatable({}, {__index=function() error('field failed') end})");
    EXPECT_FALSE(CScene::readDefinition("data/scenes/meta.lua", "meta", definition, script, error));
    EXPECT_NE(error.find("Missing scene model"), std::string::npos);
    EXPECT_EQ(definition.id, "meta");
}
TEST_F(Scenes, GarageDefaultsPreserveExistingAssetPaths) {
    const auto definition = CScene::Definition::garage();
    EXPECT_EQ(definition.model, "test/garage_gpp.obj");
    EXPECT_EQ(definition.materials, "test/garage_gpp.mtl");
    EXPECT_TRUE(definition.garageMaterials);
}

TEST_F(Scenes, TextureDecoderFailureSurvivesMoveAndPublishesOnConsumer) {
    GPPGame::gppTexture texture("data/models", "missing.tga", true);
    texture.tloadAsync("data/models", "missing.tga");
    // Moving waits for decoding because the worker captures the source address.
    GPPGame::gppTexture moved(std::move(texture));
    EXPECT_EQ(texture.getTextId(), 0u);
    EXPECT_EQ(moved.getTextId(), 0u);
    EXPECT_FALSE(moved.isAsyncRunning());
    EXPECT_EQ(moved.getTextIdUpdateAsync(), ~0u);
    EXPECT_EQ(moved.getTextIdUpdateAsync(), ~0u);
}
TEST_F(Scenes, TextureQueueKeepsDistinctFilesAndEachCompletionCallback) {
    auto &game = GPPGame::GuitarPP();
    const auto previousOwner = game.mainthread.load();
    game.mainthread = std::this_thread::get_id();
    int first = 0, second = 0, duplicate = 0;
    const auto path = directory.string();
    auto completion = [](int &count) {
        return [&count](GPPGame::loadTextureBatch *batch) {
            EXPECT_NE(batch->text, nullptr);
            if (batch->text) { EXPECT_EQ(batch->text->getTextId(), ~0u); }
            ++count;
        };
    };
    EXPECT_TRUE(game.loadTextureSingleAsync({path, "first.tga", nullptr, completion(first)}));
    EXPECT_TRUE(game.loadTextureSingleAsync({path, "second.tga", nullptr, completion(second)}));
    EXPECT_TRUE(game.loadTextureSingleAsync({path, "first.tga", nullptr, completion(duplicate)}));
    game.forceTexturesToLoad();
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 1);
    EXPECT_EQ(duplicate, 1);
    EXPECT_EQ(game.getNumTexturesToLoad(), 0);
    game.mainthread = previousOwner;
}
TEST_F(Scenes, EmptyTextureNamePublishesCompletion) {
    GPPGame::gppTexture texture("data/models", "", true);
    texture.tloadAsync("data/models", "");
    EXPECT_EQ(texture.getTextIdUpdateAsync(), ~0u);
    EXPECT_FALSE(texture.isAsyncRunning());
}

class SceneGL : public Scenes {
    GLFWwindow *window = nullptr;
    void SetUp() override {
        Scenes::SetUp();
        ASSERT_TRUE(glfwInit());
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        window = glfwCreateWindow(32, 32, "scene tests", nullptr, nullptr);
        ASSERT_NE(window, nullptr);
        glfwMakeContextCurrent(window);
        ASSERT_EQ(glewInit(), GLEW_OK);
        ShaderProject::CShader::inst().prepareEffects();
    }
    void TearDown() override {
        CBackgroundRenderer::shutdown();
        ShaderProject::CShader::shutdown();
        if (window) { glfwDestroyWindow(window); }
        glfwTerminate();
        Scenes::TearDown();
    }
};
TEST_F(SceneGL, SynchronousTextureUsePreservesQueuedCompletions) {
    auto &game = GPPGame::GuitarPP();
    const auto previousOwner = game.mainthread.load();
    game.mainthread = std::this_thread::get_id();
    std::string pixels(21, '\0');
    pixels[2] = 2; pixels[12] = 1; pixels[14] = 1; pixels[16] = 24;
    pixels[18] = static_cast<char>(255);
    write("pixel.tga", pixels);
    // Exercise both successful upload and failed decoding without a forced drain.
    for (const auto *file : {"pixel.tga", "missing.tga"}) {
        int completions = 0;
        unsigned int callbackId = 0;
        EXPECT_TRUE(game.loadTextureSingleAsync({directory.string(), file, nullptr,
            [&](GPPGame::loadTextureBatch *batch) {
                ++completions;
                callbackId = batch->text->getTextId();
            }}));
        const auto id = game.loadTexture(directory.string(), file).getTextId();
        EXPECT_NE(id, 0u);
        if (std::string(file) == "pixel.tga") { EXPECT_TRUE(glIsTexture(id)); }
        else { EXPECT_EQ(id, ~0u); }
        game.textureStreammingProcess();
        EXPECT_EQ(game.getNumTexturesToLoad(), 0);
        EXPECT_EQ(completions, 1);
        EXPECT_EQ(callbackId, id);
        game.textureStreammingProcess();
        EXPECT_EQ(completions, 1);
    }
    game.mainthread = previousOwner;
}
TEST_F(SceneGL, HookLookupDoesNotInvokeMetamethods) {
    model();
    write("data/scenes/meta.lua", R"(
local scene = {model='../models/room.obj', materials='room.mtl'}
scene.onLoaded = function()
    setmetatable(scene, {__index=function() error('hook lookup failed') end})
end
return scene
)");
    GPPOBJ model;
    CScene scene;
    ASSERT_TRUE(scene.load(model, "meta")) << scene.error();
    scene.sessionStarted("song", -3);
    scene.sessionEnded(10);
    scene.unload(model);
    EXPECT_FALSE(scene.loaded());
}
TEST_F(SceneGL, LifecycleRunsOnceAndFailedReplacementPreservesScene) {
    model();
    write("data/scenes/room.lua", R"(
local function record(event, context)
    local f = assert(io.open('events.txt', 'a'))
    f:write(event .. ':' .. context.sceneId .. ':' .. context.songId .. ':' .. string.format('%g', context.songSeconds) .. '\n')
    f:close()
end
return {
 model='../models/room.obj', materials='room.mtl',
 onLoaded=function(c) record('loaded',c) end,
 onSessionStarted=function(c) record('start',c) end,
 onSessionEnded=function(c) record('end',c) end,
 onUnloaded=function(c) record('unloaded',c) end
})");
    GPPOBJ model;
    CScene scene;
    ASSERT_TRUE(scene.load(model, "room")) << scene.error();
    ASSERT_TRUE(scene.load(model, "room")); // Reuse, no reload callback.
    scene.sessionStarted("song-one", -3.0);
    scene.sessionStarted("song-one", -3.0);
    EXPECT_FALSE(scene.load(model, "other"));
    scene.sessionEnded(12.5);
    scene.sessionEnded(12.5);
    EXPECT_FALSE(scene.load(model, "missing"));
    EXPECT_EQ(scene.definition().id, "room");
    scene.unload(model);
    EXPECT_EQ(contents("events.txt"), "loaded:room::0\nstart:room:song-one:-3\nend:room:song-one:12.5\nunloaded:room::0\n");
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(SceneGL, ReplacementInvalidatesShadowsEvenAtSameModelRevision) {
    model();
    write("data/scenes/one.lua", "return {model='../models/room.obj',materials='room.mtl'}");
    write("data/scenes/two.lua", "return {model='../models/room.obj',materials='room.mtl',lightPosition={2,3,-6}}");
    GPPOBJ model;
    CScene scene;
    auto &renderer = CBackgroundRenderer::inst();
    renderer.quality = 2;
    ASSERT_TRUE(scene.load(model, "one"));
    renderer.draw(model);
    const auto revision = model.revision();
    const auto updates = renderer.shadowUpdates();
    renderer.draw(model);
    EXPECT_EQ(renderer.shadowUpdates(), updates);
    ASSERT_TRUE(scene.load(model, "two"));
    EXPECT_EQ(model.revision(), revision);
    renderer.draw(model);
    if (updates) { EXPECT_EQ(renderer.shadowUpdates(), updates + 1); }
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    scene.unload(model);
}
TEST_F(SceneGL, CancellationAndCallbackErrorsAreContained) {
    model();
    write("data/scenes/room.lua", "return {model='../models/room.obj',materials='room.mtl',onLoaded=function() error('hook failed') end}");
    GPPOBJ model;
    CScene scene;
    EXPECT_FALSE(scene.load(model, "room", [] { return false; }));
    EXPECT_FALSE(scene.loaded());
    ASSERT_TRUE(scene.load(model, "room"));
    EXPECT_EQ(scene.definition().id, "room");
    scene.unload(model);
}
} // namespace
