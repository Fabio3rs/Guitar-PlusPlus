#include "CScene.h"
#include "CGuitars.h"
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
    EXPECT_EQ(std::filesystem::u8path(definition.model), std::filesystem::path("data/models/room.obj"));
    EXPECT_EQ(std::filesystem::u8path(definition.materials), std::filesystem::path("data/scenes/room.mtl"));
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
    GPPGame::GuitarPP().initializeTextures();
    GPPGame::gppTexture texture("data/models", "missing.tga", true);
    texture.tloadAsync("data/models", "missing.tga");
    // Preserve the legacy move contract; the decoder now owns its inputs.
    GPPGame::gppTexture moved(std::move(texture));
    EXPECT_EQ(texture.getTextId(), 0u);
    EXPECT_EQ(moved.getTextId(), 0u);
    EXPECT_FALSE(moved.isAsyncRunning());
    EXPECT_EQ(moved.getTextIdUpdateAsync(), ~0u);
    EXPECT_EQ(moved.getTextIdUpdateAsync(), ~0u);
}
TEST_F(Scenes, TextureQueueKeepsDistinctFilesAndEachCompletionCallback) {
    auto &game = GPPGame::GuitarPP();
    game.initializeTextures();
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
    GPPGame::GuitarPP().initializeTextures();
    GPPGame::gppTexture texture("data/models", "", true);
    texture.tloadAsync("data/models", "");
    EXPECT_EQ(texture.getTextIdUpdateAsync(), ~0u);
    EXPECT_FALSE(texture.isAsyncRunning());
}

TEST_F(Scenes, TextureImmediateCallbackMayDestroyOriginalRequest) {
    auto &game = GPPGame::GuitarPP();
    game.initializeTextures();
    const auto path = directory.string();
    ASSERT_TRUE(
        game.loadTextureSingleAsync({path, "missing.tga", nullptr, nullptr}));
    game.forceTexturesToLoad();

    auto request = std::make_unique<GPPGame::loadTextureBatch>();
    request->path = path;
    request->texture = "missing.tga";
    int callbacks = 0;
    request->targetFun = [&](GPPGame::loadTextureBatch *owned) {
        request.reset();
        EXPECT_EQ(owned->text->getTextId(), ~0u);
        ++callbacks;
    };
    EXPECT_TRUE(game.loadTextureSingleAsync(*request));
    EXPECT_EQ(request, nullptr);
    EXPECT_EQ(callbacks, 1);
    game.shutdownTextures();
}

TEST_F(Scenes, TextureFacadeConcurrentRequestsAllowReentrantCacheCallbacks) {
    auto &game = GPPGame::GuitarPP();
    game.initializeTextures();
    const auto owner = std::this_thread::get_id();
    const auto path = directory.string();
    int callbacks = 0;
    std::vector<std::thread> producers;
    for (int i = 0; i < 12; ++i) {
        producers.emplace_back([&] {
            EXPECT_TRUE(game.loadTextureSingleAsync(
                {path, "missing.tga", nullptr,
                 [&](GPPGame::loadTextureBatch *batch) {
                     EXPECT_EQ(std::this_thread::get_id(), owner);
                     EXPECT_EQ(batch->text->getTextId(), ~0u);
                     ++callbacks;
                     EXPECT_TRUE(game.loadTextureSingleAsync(
                         {path, "missing.tga", nullptr,
                          [&](GPPGame::loadTextureBatch *cached) {
                              EXPECT_EQ(cached->text->getTextId(), ~0u);
                              ++callbacks;
                          }}));
                     EXPECT_TRUE(game.loadTextureSingleAsync(
                         {path, "other.tga", nullptr, nullptr}));
                 }}));
        });
    }
    for (auto &producer : producers) {
        producer.join();
    }
    std::atomic<bool> finished{false};
    std::thread reader([&] {
        while (!finished) {
            const auto id = game.getTextureId(path + "/missing.tga");
            EXPECT_TRUE(id == 0u || id == ~0u);
            game.getTextureInfo(path + "/missing.tga");
        }
    });
    game.forceTexturesToLoad();
    finished = true;
    reader.join();
    EXPECT_EQ(callbacks, 24);
    EXPECT_EQ(game.getNumTexturesToLoad(), 0);
    game.shutdownTextures();
}

TEST_F(Scenes, ModelLegacyCpuLoadPreservesSyncBehaviorAndFailureState) {
    write("model.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    GPPOBJ target;
    ASSERT_TRUE(target.load(directory.string(), "model.obj"));
    EXPECT_EQ(target.inspect().parts.size(), 1u);
    const auto version = target.revision();
    EXPECT_FALSE(target.load(directory.string(), "missing.obj"));
    EXPECT_EQ(target.revision(), version);
    EXPECT_EQ(target.inspect().parts.size(), 1u);
}

TEST_F(Scenes, ModelFacadeReportsDecodeFailureAndRejectsOffOwnerPublication) {
    auto &game = GPPGame::GuitarPP();
    game.initializeModels();
    GPPOBJ target;
    auto handle = game.requestModel(directory.string(), "missing.obj", target);
    ASSERT_NE(handle, nullptr);
    game.modelLoader().finish(handle);
    EXPECT_EQ(handle->snapshot().state, CModelManager::State::Failed);
    std::thread worker([&] {
        EXPECT_EQ(game.requestModel(directory.string(), "missing.obj", target),
                  nullptr);
        EXPECT_FALSE(
            game.loadModelSync(directory.string(), "missing.obj", target));
    });
    worker.join();
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
        GPPGame::GuitarPP().initializeTextures();
        GPPGame::GuitarPP().initializeModels();
        ShaderProject::CShader::inst().prepareEffects();
    }
    void TearDown() override {
        CGuitars::inst().unload();
        GPPGame::GuitarPP().shutdownModels();
        GPPGame::GuitarPP().shutdownTextures();
        CBackgroundRenderer::shutdown();
        ShaderProject::CShader::shutdown();
        if (window) { glfwDestroyWindow(window); }
        glfwTerminate();
        Scenes::TearDown();
    }
};
TEST_F(SceneGL, InitialModelLoadingUsesRuntimeAssetFilenames) {
    const std::string triangle = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    // Match the asset package, including its case-sensitive Linux filenames.
    for (const auto *name : {"GPP_Note.obj", "TriggerBase.obj", "Trigger.obj",
                             "Pylmbar.obj", "GPP_Opennote.obj"}) {
        write(std::string("data/models/") + name, triangle);
    }
    write("data/scenes/startup.lua", "return {model='../models/GPP_Note.obj'}");
    auto &game = GPPGame::GuitarPP();
    struct RestoreScene {
        GPPGame &game;
        std::string previous;
        ~RestoreScene() {
            game.scene.unload(game.testobj);
            game.backgroundScene = previous;
        }
    } restore{game, game.backgroundScene};
    game.backgroundScene = "startup";
    // Preload the scene so initialLoad exercises its cached scene path without
    // requiring the engine's own window/menu loop in this GL fixture.
    ASSERT_TRUE(game.scene.load(game.testobj, "startup")) << game.scene.error();
    ASSERT_NO_THROW(game.initialLoad());
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (game.getNumModelsToLoad() != 0 &&
           std::chrono::steady_clock::now() < deadline) {
        game.textureStreammingProcess();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_EQ(game.getNumModelsToLoad(), 0u);
    EXPECT_TRUE(game.basicModelsReady());
    EXPECT_FALSE(game.pylmbarOBJ.inspect().parts.empty());
}

TEST_F(SceneGL, ModelSyncJoinsPendingDecodeAndSupersedesOlderPublication) {
    write("model.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    auto &game = GPPGame::GuitarPP();
    GPPOBJ target;
    auto pending = game.requestModel(directory.string(), "model.obj", target);
    ASSERT_NE(pending, nullptr);
    EXPECT_TRUE(target.inspect().parts.empty());
    ASSERT_TRUE(game.loadModelSync(directory.string(), "model.obj", target));
    const auto version = target.revision();
    EXPECT_EQ(target.inspect().parts.size(), 1u);
    EXPECT_TRUE(game.modelLoader().finish(pending));
    EXPECT_EQ(pending->snapshot().state, CModelManager::State::Cancelled);
    EXPECT_EQ(target.revision(), version);
    target.draw(0);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(SceneGL, ModelAsyncTargetMayMoveUnloadOrBeDestroyedBeforeCompletion) {
    write("model.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    auto &game = GPPGame::GuitarPP();
    auto target = std::make_unique<GPPOBJ>();
    auto dead = game.requestModel(directory.string(), "model.obj", *target);
    target.reset();
    game.modelLoader().finish(dead);
    EXPECT_EQ(dead->snapshot().state, CModelManager::State::Cancelled);
    GPPOBJ original;
    auto moved = game.requestModel(directory.string(), "model.obj", original);
    GPPOBJ destination(std::move(original));
    game.modelLoader().finish(moved);
    EXPECT_EQ(moved->snapshot().state, CModelManager::State::Ready);
    EXPECT_EQ(destination.inspect().parts.size(), 1u);
    auto unloaded =
        game.requestModel(directory.string(), "model.obj", destination);
    destination.unload();
    game.modelLoader().finish(unloaded);
    EXPECT_EQ(unloaded->snapshot().state, CModelManager::State::Cancelled);
    EXPECT_TRUE(destination.inspect().parts.empty());
}

TEST_F(SceneGL, ModelPublicationAbandonsTargetDestroyedDuringTextureCallback) {
    write("model.obj",
          "mtllib material.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    write("material.mtl", "newmtl surface\nmap_Kd broken.tga\n");
    write("broken.tga", "invalid");
    std::unique_ptr<GPPOBJ> target;
    target = std::make_unique<GPPOBJ>([&](const std::string &) {
        target.reset();
        return 0U;
    });
    auto &game = GPPGame::GuitarPP();
    auto handle = game.requestModel(directory.string(), "model.obj", *target);
    ASSERT_NE(handle, nullptr);
    game.modelLoader().finish(handle);
    EXPECT_EQ(target, nullptr);
    EXPECT_EQ(handle->snapshot().state, CModelManager::State::Cancelled);
}

TEST_F(SceneGL, ModelFailuresPreservePreviouslyPublishedGeometry) {
    write("model.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    write("textured.obj",
          "mtllib material.mtl\nv 0 0 0\nv 2 0 0\nv 0 2 0\nf 1 2 3\n");
    write("material.mtl", "newmtl surface\nmap_Kd broken.tga\n");
    write("broken.tga", "invalid");
    GPPOBJ target([](const std::string &) -> unsigned int {
        throw std::runtime_error("test upload failure");
    });
    auto &game = GPPGame::GuitarPP();
    ASSERT_TRUE(game.loadModelSync(directory.string(), "model.obj", target));
    const auto version = target.revision();
    const auto bounds = target.bounds();
    EXPECT_FALSE(game.loadModelSync(directory.string(), "missing.obj", target));
    EXPECT_FALSE(
        game.loadModelSync(directory.string(), "textured.obj", target));
    EXPECT_EQ(target.revision(), version);
    EXPECT_EQ(target.bounds(), bounds);
    target.draw(0);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(SceneGL, ModelTextureFailureUsesNeutralFallbackInsteadOfMagicId) {
    write("model.obj", "mtllib material.mtl\nusemtl surface\nv 0 0 0\nv 1 0 "
                       "0\nv 0 1 0\nvt 0 0\nf 1/1 2/1 3/1\n");
    write("material.mtl", "newmtl surface\nmap_Kd broken.tga\n");
    write("broken.tga", "invalid");
    auto &game = GPPGame::GuitarPP();
    ASSERT_TRUE(game.loadTextureSingleAsync(GPPGame::loadTextureBatch(
        directory.string(), "broken.tga", nullptr, nullptr)));
    game.forceTexturesToLoad();
    GPPOBJ target;
    ASSERT_TRUE(game.loadModelSync(directory.string(), "model.obj", target));
    int draws = 0;
    target.drawMaterials(
        [&](const ObjModel::Part &, const ObjModel::Material &, bool textured) {
            EXPECT_FALSE(textured);
            ++draws;
            return true;
        });
    EXPECT_EQ(draws, 1);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(SceneGL, ModelShutdownRetiresBasicGpuBuffersBeforeContextDestruction) {
    write("model.obj",
          "v 0 0 0\nv 1 0 0\nv 0 1 0\ng one\nf 1 2 3\ng two\nf 1 2 3\n");
    auto &game = GPPGame::GuitarPP();
    ASSERT_TRUE(
        game.loadModelSync(directory.string(), "model.obj", game.noteOBJ));
    GLint buffer = 0;
    int parts = 0;
    game.noteOBJ.drawMaterials(
        [&](const ObjModel::Part &, const ObjModel::Material &, bool) {
            if (++parts == 2) {
                glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
            }
            return true;
        });
    ASSERT_EQ(parts, 2);
    ASSERT_GT(buffer, 0);
    ASSERT_TRUE(glIsBuffer(static_cast<GLuint>(buffer)));
    game.shutdownModels();
    EXPECT_FALSE(glIsBuffer(static_cast<GLuint>(buffer)));
    EXPECT_TRUE(game.noteOBJ.inspect().parts.empty());
}

TEST_F(SceneGL, ModelBatchReportsRejectionAndLeavesCompletionObservable) {
    auto &game = GPPGame::GuitarPP();
    GPPOBJ target;
    std::deque<GPPGame::loadModelBatch> batch;
    batch.emplace_back(directory.string().c_str(), "missing.obj", target);
    batch.emplace_back();
    EXPECT_FALSE(game.loadModelBatchAsync(batch));
    ASSERT_NE(batch[0].handle, nullptr);
    EXPECT_EQ(batch[1].handle, nullptr);
    game.modelLoader().finish(batch[0].handle);
    EXPECT_EQ(batch[0].handle->snapshot().state, CModelManager::State::Failed);
    EXPECT_FALSE(batch[0].handle->snapshot().error.empty());
}

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

TEST_F(SceneGL, ColdSynchronousTextureIsUsableWithoutProcessingAnotherFrame) {
    auto &game = GPPGame::GuitarPP();
    const auto previousOwner = game.mainthread.load();
    game.mainthread = std::this_thread::get_id();
    std::string pixels(21, '\0');
    pixels[2] = 2;
    pixels[12] = 1;
    pixels[14] = 1;
    pixels[16] = 24;
    write("sync.tga", pixels);

    const auto &texture = game.loadTexture(directory.string(), "sync.tga");
    EXPECT_TRUE(glIsTexture(texture.getTextId()));
    EXPECT_EQ(&texture, &game.loadTexture(directory.string(), "sync.tga"));

    int completions = 0;
    ASSERT_TRUE(game.loadTextureSingleAsync(
        {directory.string(), "sync.tga", nullptr,
         [&](GPPGame::loadTextureBatch *batch) {
             ASSERT_NE(batch->text, nullptr);
             EXPECT_EQ(batch->text->getTextId(), texture.getTextId());
             ++completions;
         }}));
    EXPECT_EQ(completions, 1); // Cache-hit completion precedes the return.
    game.mainthread = previousOwner;
}

TEST_F(SceneGL, TextureBatchCompletesBeforeReturningDespiteAsyncName) {
    auto &game = GPPGame::GuitarPP();
    const auto previousOwner = game.mainthread.load();
    game.mainthread = std::this_thread::get_id();
    std::string pixels(21, '\0');
    pixels[2] = 2;
    pixels[12] = 1;
    pixels[14] = 1;
    pixels[16] = 24;
    write("batch.tga", pixels);
    int completions = 0;
    std::deque<GPPGame::loadTextureBatch> batch;
    batch.emplace_back(directory.string(), "batch.tga", nullptr,
                       [&](GPPGame::loadTextureBatch *result) {
                           ASSERT_NE(result->text, nullptr);
                           EXPECT_TRUE(glIsTexture(result->text->getTextId()));
                           ++completions;
                       });

    EXPECT_TRUE(game.loadTextureBatchAsync(batch));
    EXPECT_EQ(completions, 1);
    ASSERT_NE(batch.front().text, nullptr);
    EXPECT_TRUE(glIsTexture(batch.front().text->getTextId()));
    game.mainthread = previousOwner;
}

TEST_F(SceneGL, TextureBatchPreservesDuplicateAndCachedCompletions) {
    auto &game = GPPGame::GuitarPP();
    std::string pixels(21, '\0');
    pixels[2] = 2;
    pixels[12] = 1;
    pixels[14] = 1;
    pixels[16] = 24;
    write("batch.tga", pixels);
    const auto id =
        game.loadTexture(directory.string(), "batch.tga").getTextId();
    int callbacks = 0;
    std::deque<GPPGame::loadTextureBatch> batch;
    for (int i = 0; i < 3; ++i) {
        batch.emplace_back(directory.string(), "batch.tga", nullptr,
                           [&](GPPGame::loadTextureBatch *result) {
                               ASSERT_NE(result->text, nullptr);
                               EXPECT_EQ(result->text->getTextId(), id);
                               ++callbacks;
                           });
    }
    EXPECT_TRUE(game.loadTextureBatchAsync(batch));
    EXPECT_EQ(callbacks, 3);
    EXPECT_EQ(game.getNumTexturesToLoad(), 0);
    game.textureStreammingProcess();
    EXPECT_EQ(callbacks, 3);
}

TEST_F(SceneGL, TextureShutdownDeletesGpuObjectsBeforeContextDestruction) {
    auto &game = GPPGame::GuitarPP();
    std::string pixels(21, '\0');
    pixels[2] = 2;
    pixels[12] = 1;
    pixels[14] = 1;
    pixels[16] = 24;
    write("shutdown.tga", pixels);
    const auto &texture = game.loadTexture(directory.string(), "shutdown.tga");
    const auto id = texture.getTextId();
    ASSERT_TRUE(glIsTexture(id));
    int callbacks = 0;
    std::thread worker([&] {
        EXPECT_TRUE(game.loadTextureSingleAsync(
            {directory.string(), "shutdown.tga", nullptr,
             [&](GPPGame::loadTextureBatch *) { ++callbacks; }}));
    });
    worker.join();
    game.shutdownTextures();
    EXPECT_FALSE(glIsTexture(id));
    EXPECT_EQ(texture.getTextId(), 0u);
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(game.getNumTexturesToLoad(), 0);
    EXPECT_FALSE(game.loadTextureSingleAsync(
        {directory.string(), "late.tga", nullptr, nullptr}));
}

TEST_F(SceneGL, TextureUploadPreservesParametersOfPreviouslyBoundTexture) {
    GLuint previous = 0;
    glGenTextures(1, &previous);
    glBindTexture(GL_TEXTURE_2D, previous);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    std::string pixels(21, '\0');
    pixels[2] = 2;
    pixels[12] = 1;
    pixels[14] = 1;
    pixels[16] = 24;
    write("parameters.tga", pixels);
    const auto id = GPPGame::GuitarPP()
                        .loadTexture(directory.string(), "parameters.tga")
                        .getTextId();
    EXPECT_TRUE(glIsTexture(id));
    GLint filter = 0, wrap = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
    EXPECT_EQ(filter, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, previous);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap);
    EXPECT_EQ(filter, GL_NEAREST);
    EXPECT_EQ(wrap, GL_CLAMP_TO_EDGE);
    glDeleteTextures(1, &previous);
}

TEST_F(SceneGL, TextureExceedingGpuLimitPublishesFailureInsteadOfReady) {
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (maximum >= 65535) {
        GTEST_SKIP() << "All TGA widths fit this GPU's dimension limit";
    }
    const auto width = maximum + 1;
    std::string pixels(18 + static_cast<size_t>(width) * 3, '\0');
    pixels[2] = 2;
    pixels[12] = static_cast<char>(width & 255);
    pixels[13] = static_cast<char>((width >> 8) & 255);
    pixels[14] = 1;
    pixels[16] = 24;
    write("oversized.tga", pixels);
    auto &game = GPPGame::GuitarPP();
    EXPECT_EQ(game.loadTexture(directory.string(), "oversized.tga").getTextId(),
              0u);
    auto result = game.getTextureInfo(directory.string() + "/oversized.tga");
    EXPECT_EQ(result.state, CTextureResource::State::Failed);
    EXPECT_FALSE(result.error.empty());
    int callbacks = 0;
    EXPECT_TRUE(game.loadTextureSingleAsync(
        {directory.string(), "oversized.tga", nullptr,
         [&](GPPGame::loadTextureBatch *batch) {
             EXPECT_EQ(batch->text->getTextId(), ~0u);
             ++callbacks;
         }}));
    game.forceTexturesToLoad();
    EXPECT_EQ(callbacks, 1);
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
TEST_F(SceneGL, SongEventsAreSeparateFromSessionAndCarryResults) {
    model();
    write("data/scenes/songs.lua", R"(
local function record(text)
    local f = assert(io.open('songs.txt','a')); f:write(text .. '\n'); f:close()
end
return {
 model='../models/room.obj', materials='room.mtl',
 onSessionStarted=function(c) record('session-start') end,
 onSessionEnded=function(c) record('session-end') end,
 onSongStarted=function(c) record('start:' .. c.songId) end,
 onSongEnded=function(c)
    record(c.reason .. ':' .. c.songId .. ':' .. #c.players)
    if c.reason == 'Completed' then assert(c.players[1].score == 125) end
 end
}
)");
    GPPOBJ model;
    CScene scene;
    ASSERT_TRUE(scene.load(model, "songs")) << scene.error();
    scene.sessionStarted("one", -3);
    scene.songStarted("one", -3);
    scene.songStarted("one", -3);
    SongResult first{"one", SongEndReason::Completed, 10, {{125, 3, 3, 4}}};
    scene.songEnded(first);
    scene.songEnded(first); // No duplicate completion.
    scene.songStarted("two", -3);
    scene.songEnded({"two", SongEndReason::Aborted, 2, {}});
    scene.songEnded({"bad", SongEndReason::LoadFailed, 0, {}});
    scene.sessionEnded(2);
    scene.unload(model);
    EXPECT_EQ(contents("songs.txt"),
        "session-start\nstart:one\nCompleted:one:1\nstart:two\nAborted:two:0\nLoadFailed:bad:0\nsession-end\n");
}
} // namespace

namespace {
TEST_F(Scenes, GuitarCatalogueRejectsBrokenScriptsAndDoesNotInsertOnLookup) {
    auto &catalogue = CGuitars::inst();
    catalogue.unload();
    EXPECT_EQ(catalogue.getGuitarIfExists("unknown"), nullptr);
    EXPECT_THROW(catalogue.getGuitar("unknown"), std::out_of_range);
    EXPECT_EQ(catalogue.getGuitarIfExists("unknown"), nullptr);
    std::filesystem::create_directories(directory / "bad");
    for (const auto *script : {"syntax ?", "error('broken')", "",
        "changeGuitarData('bad','gameplayBarName','bar.obj'); setCallBackToEvent('addGuitar',function() error('bad callback') end)"}) {
        write("bad/GuitarScript.lua", script);
        EXPECT_EQ(catalogue.addGuitar(directory.string(), "bad"), nullptr);
        EXPECT_EQ(catalogue.getGuitarIfExists("bad"), nullptr);
    }
    write("bad/GuitarScript.lua", "setCallBackToEvent('addGuitar',function(path,id) assert(changeGuitarData(id,'gameplayBarName','bar.obj')) end)");
    auto *guitar = catalogue.addGuitar(directory.string(), "bad");
    ASSERT_NE(guitar, nullptr);
    EXPECT_EQ(catalogue.addGuitar(directory.string(), "bad"), guitar);
    EXPECT_FALSE(guitar->isLoaded());
    catalogue.unload();
}
TEST_F(SceneGL, GuitarLoadingRetriesAndCachesWithBalancedEvents) {
    auto &catalogue = CGuitars::inst();
    catalogue.unload();
    std::filesystem::create_directories(directory / "guitar");
    write("guitar/GuitarScript.lua", R"(
        local loads, unloads = 0, 0
        setCallBackToEvent('addGuitar',function(path,id)
            assert(changeGuitarData(id,'gameplayBarName','bar.obj'))
        end)
        setCallBackToEvent('loadGuitar',function(path,id)
            loads = loads + 1
            changeGuitarData(id,'name', tostring(loads))
        end)
        setCallBackToEvent('unloadingGuitar',function(path,id)
            unloads = unloads + 1
            local f = assert(io.open('unloads.txt','w'))
            f:write(unloads); f:close()
        end)
    )");
    auto *guitar = catalogue.addGuitar(directory.string(), "guitar");
    ASSERT_NE(guitar, nullptr);
    guitar->unload();
    EXPECT_FALSE(std::filesystem::exists(directory / "unloads.txt"));
    EXPECT_FALSE(guitar->load());
    EXPECT_FALSE(guitar->isLoaded());
    EXPECT_TRUE(guitar->name.empty());
    write("guitar/bar.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    ASSERT_TRUE(guitar->load());
    EXPECT_TRUE(guitar->isLoaded());
    EXPECT_TRUE(guitar->load());
    EXPECT_EQ(guitar->name, "1");
    guitar->unload();
    guitar->unload();
    EXPECT_FALSE(guitar->isLoaded());
    EXPECT_EQ(contents("unloads.txt"), "1");
    ASSERT_TRUE(guitar->load());
    EXPECT_EQ(guitar->name, "2");
    catalogue.unload();
    EXPECT_EQ(contents("unloads.txt"), "2");
    EXPECT_EQ(catalogue.getGuitarIfExists("guitar"), nullptr);
}
TEST_F(SceneGL, GuitarLoadCallbackFailureCanBeRetried) {
    auto &catalogue = CGuitars::inst();
    catalogue.unload();
    std::filesystem::create_directories(directory / "retry");
    write("retry/bar.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    write("retry/GuitarScript.lua", R"(
        changeGuitarData('retry','gameplayBarName','bar.obj')
        local first = true
        setCallBackToEvent('loadGuitar',function()
            if first then first = false; error('retry me') end
        end)
    )");
    auto *guitar = catalogue.addGuitar(directory.string(), "retry");
    ASSERT_NE(guitar, nullptr);
    EXPECT_FALSE(guitar->load());
    EXPECT_FALSE(guitar->isLoaded());
    EXPECT_TRUE(guitar->load());
    EXPECT_TRUE(guitar->isLoaded());
}
}
