#include "CParticle.h"
#include "CBackgroundRenderer.h"
#include "BackgroundShader.h"
#include "GPPOBJ.h"
#include "SceneAO.h"
#include <glm/gtc/type_ptr.hpp>
#include "CPlusLightning.h"
#include "CShader.h"
#include "FireShader.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <vector>

using namespace ShaderProject;

namespace {
std::vector<unsigned int> deleted;
void recordDelete(unsigned int id) { deleted.push_back(id); }

TEST(ShaderOwnership, EmptyMovedAndRepeatedResetAreSafe) {
    deleted.clear();
    {
        ShaderObject empty;
        empty.deleteObject();
        ShaderObject first(7u, recordDelete);
        ShaderObject moved(std::move(first));
        first.deleteObject();
        ShaderObject second(8u, recordDelete);
        second = std::move(moved);
        second.deleteObject();
        second.deleteObject();
    }
    EXPECT_EQ(deleted, (std::vector<unsigned int>{8u, 7u}));
}

class ShaderGL : public testing::Test {
    GLFWwindow *window = nullptr;

  protected:
    void SetUp() override {
        ASSERT_TRUE(glfwInit());
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        window = glfwCreateWindow(32, 32, "shader tests", nullptr, nullptr);
        ASSERT_NE(window, nullptr);
        glfwMakeContextCurrent(window);
        ASSERT_EQ(glewInit(), GLEW_OK);
        CShader::inst().prepareEffects();
        ASSERT_GE(CShader::inst().fireProgram().event, 0);
        ASSERT_GE(CShader::inst().lightningProgram().event, 0);
    }
    void TearDown() override {
        CBackgroundRenderer::shutdown();
        CShader::shutdown();
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
    }
};

constexpr std::string_view vertex =
    "#version 110\nvoid main() { gl_Position = gl_Vertex; }";
constexpr std::string_view fragment =
    "#version 110\nvoid main() { gl_FragColor = vec4(1.0); }";

TEST_F(ShaderGL, ExplicitLengthAndStringViewDoNotReadTrailingBytes) {
    const std::string source =
        std::string(vertex) + "\ninvalid trailing source";
    EXPECT_NO_THROW(CShader::compileShader(
        source.data(), static_cast<int>(vertex.size()), VERTEX));
    EXPECT_NO_THROW(CShader::compileShader(
        std::string_view(source.data(), vertex.size()), VERTEX));
    EXPECT_THROW(CShader::compileShader(source, VERTEX), std::runtime_error);
    EXPECT_THROW(CShader::compileShader(nullptr, 1, VERTEX),
                 std::invalid_argument);
    EXPECT_THROW(CShader::compileShader("", -1, VERTEX), std::invalid_argument);
}

TEST_F(ShaderGL, LinkFailureAndUnlinkedBindingAreReported) {
    auto program = make_program();
    EXPECT_THROW(CShader::bindProgram(program), std::runtime_error);
    auto a = CShader::compileShader(
        "#version 110\nvarying vec3 value; void main() { value = vec3(1.0); "
        "gl_Position = gl_Vertex; }",
        VERTEX);
    auto b = CShader::compileShader("#version 110\nvarying vec4 value; void "
                                    "main() { gl_FragColor = value; }",
                                    FRAGMENT);
    CShader::attachShaderToProgram(program, a);
    CShader::attachShaderToProgram(program, b);
    EXPECT_THROW(CShader::linkShaderProgram(program), std::runtime_error);
}

TEST_F(ShaderGL, TracksActualProgramAndDeletionDoesNotUnbindAnotherProgram) {
    auto a = CShader::compileShader(vertex, VERTEX);
    auto b = CShader::compileShader(fragment, FRAGMENT);
    auto program = make_program();
    CShader::attachShaderToProgram(program, a);
    CShader::attachShaderToProgram(program, b);
    CShader::linkShaderProgram(program);
    CShader::bindProgram(program);
    EXPECT_TRUE(CShader::inst().isUsingProgram());
    {
        auto unrelated = make_program();
    }
    GLint current = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    EXPECT_EQ(static_cast<GLuint>(current), program.get());
    glUseProgram(0);
    EXPECT_FALSE(CShader::inst().isUsingProgram());
}

TEST_F(ShaderGL, InvalidFilesAndIdsDoNotMutateRegistry) {
    auto &manager = CShader::inst();
    const int event = manager.addEvent("test");
    EXPECT_THROW(manager.processEvent(-1), std::out_of_range);
    EXPECT_THROW(manager.processEvent(100), std::out_of_range);
    EXPECT_THROW(manager.newShader("missing.vert", VERTEX, -1),
                 std::out_of_range);
    EXPECT_THROW(
        manager.newShader("/nonexistent/guitarpp/shader.vert", VERTEX, event),
        std::runtime_error);
    EXPECT_THROW(manager.newShader(nullptr, VERTEX, event),
                 std::invalid_argument);
    EXPECT_THROW(manager.addShaderToEvent("test", 0), std::out_of_range);
    EXPECT_THROW(manager.addShaderToEvent("missing", 0), std::invalid_argument);
    EXPECT_EQ(manager.shadersListSize(), 0u);
}

TEST_F(ShaderGL, FileWithFormerProblematicSizeLoadsAndShutdownIsRepeatable) {
    auto &manager = CShader::inst();
    const int event = manager.addEvent("file");
    auto source = std::string(vertex);
    while (source.size() % 8 != 6)
        source += ' ';
    const auto path =
        std::filesystem::temp_directory_path() /
        ("guitarpp-shader-" +
         std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".vert");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup{path};
    {
        std::ofstream file(path, std::ios::binary);
        file << source;
    }
    EXPECT_GE(manager.newShader(path.string().c_str(), VERTEX, event), 0);
    EXPECT_EQ(manager.shadersListSize(), 1u);
    CShader::shutdown();
    CShader::shutdown();
    EXPECT_EQ(manager.shadersListSize(), 0u);
    EXPECT_EQ(manager.eventsSize(), 0u);
}
TEST_F(ShaderGL, FireProgramProducesTransparentEdgesAndAnimatedFlame) {
    auto &manager = CShader::inst();
    const auto count = manager.eventsSize();
    EXPECT_THROW(manager.addProgram("broken", FireShader::vertex, "invalid"),
                 std::runtime_error);
    EXPECT_EQ(manager.eventsSize(), count);
    const int event =
        manager.addProgram("fire", FireShader::vertex, FireShader::fragment);
    const int time = manager.getEventUniform(event, "uTime");
    ASSERT_GE(time, 0);
    manager.processEvent(event);
    const int multiplier = manager.getEventUniform(event, "uMultiplier");
    const int approval = manager.getEventUniform(event, "uPublicApproval");
    const int plus = manager.getEventUniform(event, "uPlusActive");
    ASSERT_GE(multiplier, 0);
    ASSERT_GE(approval, 0);
    ASSERT_GE(plus, 0);
    CShader::setFloatUniform(multiplier, 4.0f);
    CShader::setFloatUniform(approval, 1.0f);
    glViewport(0, 0, 32, 32);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const auto draw = [&](float seconds, float opacity) {
        CShader::setFloatUniform(time, seconds);
        glColor4f(1, 1, 1, opacity);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 1);
        glVertex3f(-1, -1, 0);
        glTexCoord2f(1, 1);
        glVertex3f(1, -1, 0);
        glTexCoord2f(1, 0);
        glVertex3f(1, 1, 0);
        glTexCoord2f(0, 0);
        glVertex3f(-1, 1, 0);
        glEnd();
        std::array<unsigned char, 32 * 32 * 4> pixels{};
        glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    const auto first = draw(0.0f, 1.0f);
    const size_t center = (8 * 32 + 16) * 4;
    const size_t edge = (8 * 32) * 4;
    EXPECT_GT(first[center], 200);
    EXPECT_GT(first[center + 1], first[center + 2]);
    EXPECT_GT(first[center + 3], 150);
    EXPECT_LT(first[edge + 3], 5);
    EXPECT_NE(first, draw(0.4f, 1.0f));
    const auto faded = draw(0.0f, 0.25f);
    EXPECT_LT(faded[center + 3], first[center + 3] / 2);
    // Compare rendered coverage, not just uniform values, across multipliers.
    const auto coverage = [](const auto &pixels) {
        unsigned int total = 0;
        for (size_t i = 3; i < pixels.size(); i += 4)
            total += pixels[i];
        return total;
    };
    unsigned int previousCoverage = 0;
    for (float value : {1.0f, 2.0f, 3.0f, 4.0f}) {
        CShader::setFloatUniform(multiplier, value);
        const auto current = draw(0.0f, 1.0f);
        EXPECT_GT(coverage(current), previousCoverage);
        previousCoverage = coverage(current);
    }
    CShader::setFloatUniform(multiplier, 8.0f);
    EXPECT_EQ(first, draw(0.0f, 1.0f));
    CShader::setFloatUniform(approval, 0.0f);
    const auto red = draw(0.0f, 1.0f);
    EXPECT_EQ(red[center], first[center]);
    EXPECT_LT(red[center + 1], first[center + 1]);
    EXPECT_EQ(coverage(red), coverage(first));
    CShader::setFloatUniform(plus, 1.0f);
    const auto blue = draw(0.0f, 1.0f);
    EXPECT_GT(blue[center + 2], blue[center]);
    CShader::setFloatUniform(approval, 1.0f);
    EXPECT_EQ(blue, draw(0.0f, 1.0f));
    CShader::setFloatUniform(plus, 0.0f);
    EXPECT_EQ(first, draw(0.0f, 1.0f));
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    manager.deactivateShader();
}

TEST_F(ShaderGL, FireParticlesRestoreRenderStateAndCompileOnce) {
    auto &manager = CShader::inst();
    const auto previousEvent = manager.addProgram("previous", vertex, fragment);
    manager.processEvent(previousEvent);
    GLint previousProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_CULL_FACE);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDepthMask(GL_TRUE);
    CParticle particles;
    CParticle::particleData particle{};
    particle.duration = 10.0;
    particle.crossedFlame = true;
    particle.sizex = 0.2;
    particle.sizey = 0.4;
    particles.addParticle(particle);
    const auto before = manager.eventsSize();
    particles.renderFire();
    EXPECT_EQ(manager.eventsSize(), before);
    particles.renderFire();
    EXPECT_EQ(manager.eventsSize(), before);
    GLint currentProgram = 0, sourceBlend = 0, destinationBlend = 0;
    GLboolean depthWrite = GL_FALSE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    glGetIntegerv(GL_BLEND_SRC, &sourceBlend);
    glGetIntegerv(GL_BLEND_DST, &destinationBlend);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
    EXPECT_EQ(currentProgram, previousProgram);
    EXPECT_EQ(glIsEnabled(GL_BLEND), GL_FALSE);
    EXPECT_EQ(glIsEnabled(GL_ALPHA_TEST), GL_TRUE);
    EXPECT_EQ(glIsEnabled(GL_CULL_FACE), GL_TRUE);
    EXPECT_EQ(sourceBlend, GL_ONE);
    EXPECT_EQ(destinationBlend, GL_ZERO);
    EXPECT_EQ(depthWrite, GL_TRUE);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    manager.deactivateShader();
}

TEST_F(ShaderGL, CrossedFlamesFollowOnlyTheirEmitterAndHaveDepth) {
    CParticle particles;
    CParticle::particleData particle{};
    particle.duration = 10.0;
    particle.x = 0.2;
    particle.y = -0.48;
    particle.z = 0.75;
    particle.sizex = 0.07;
    particle.sizey = 0.16;
    particle.crossedFlame = true;
    particle.emitter = 2;
    particle.emitterHeight = -0.48;
    particles.addParticle(particle);
    particles.moveEmitter(1, 0.0);
    EXPECT_DOUBLE_EQ(particles.part.front().y, -0.48);
    particles.moveEmitter(2, -0.44);
    EXPECT_DOUBLE_EQ(particles.part.front().y, -0.44);
    particles.moveEmitter(2, -0.44);
    EXPECT_DOUBLE_EQ(particles.part.front().y, -0.44);
    // Preserve upward particle motion relative to the moving emitter.
    particles.part.front().y += 0.02;
    particles.moveEmitter(2, -0.47);
    EXPECT_NEAR(particles.part.front().y, -0.45, 1e-12);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    particles.renderFire();
    ASSERT_EQ(particles.partDrawData.vArray.size(), 2u);
    const auto &a = particles.partDrawData.vArray[0].t;
    const auto &b = particles.partDrawData.vArray[1].t;
    EXPECT_LT(a[0], a[3]);
    EXPECT_GT(a[2], a[5]);
    EXPECT_LT(b[2], b[5]);
    EXPECT_NEAR((a[2] + a[5]) * 0.5, particle.z, 1e-12);
    EXPECT_NEAR(a[7], particles.part.front().y, 1e-12);
    EXPECT_NEAR(a[1] - a[7], particle.sizey, 1e-12);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

TEST_F(ShaderGL, PlusLightningRendersAnimatesExpiresAndRestoresState) {
    CPlusLightning lightning;
    auto &manager = CShader::inst();
    const auto before = manager.eventsSize();
    lightning.render(-0.1, 4u);
    lightning.render(CPlusLightning::duration, 4u);
    EXPECT_EQ(manager.eventsSize(), before);
    glViewport(0, 0, 32, 32);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_ALPHA_TEST);
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_ONE, GL_ZERO);
    glClearColor(0, 0, 0, 0);
    const auto draw = [&](double elapsed) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        lightning.render(elapsed, 4u);
        std::array<unsigned char, 32 * 32 * 4> pixels{};
        glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    const auto first = draw(0.0);
    unsigned int red = 0, blue = 0;
    for (size_t i = 0; i < first.size(); i += 4) {
        red += first[i];
        blue += first[i + 2];
    }
    EXPECT_GT(blue, 100u);
    EXPECT_GT(blue, red);
    EXPECT_NE(first, draw(0.15));
    const auto expired = draw(CPlusLightning::duration);
    EXPECT_EQ(expired, (std::array<unsigned char, 32 * 32 * 4>{}));
    EXPECT_EQ(manager.eventsSize(), before);
    GLint program = -1, sourceBlend = 0, destinationBlend = 0;
    GLboolean depthWrite = GL_FALSE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_BLEND_SRC, &sourceBlend);
    glGetIntegerv(GL_BLEND_DST, &destinationBlend);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
    EXPECT_EQ(program, 0);
    EXPECT_EQ(glIsEnabled(GL_BLEND), GL_FALSE);
    EXPECT_EQ(glIsEnabled(GL_DEPTH_TEST), GL_FALSE);
    EXPECT_EQ(glIsEnabled(GL_CULL_FACE), GL_TRUE);
    EXPECT_EQ(glIsEnabled(GL_ALPHA_TEST), GL_TRUE);
    EXPECT_EQ(sourceBlend, GL_ONE);
    EXPECT_EQ(destinationBlend, GL_ZERO);
    EXPECT_EQ(depthWrite, GL_TRUE);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(ShaderGL, PlusLightningUsesChordBitsAndIgnoresStatusFlags) {
    CPlusLightning lightning;
    auto &manager = CShader::inst();
    const auto before = manager.eventsSize();
    lightning.render(0.0, 0u);
    lightning.render(0.0, 32u | 256u | 512u);
    EXPECT_EQ(manager.eventsSize(), before);
    glViewport(0, 0, 32, 32);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0, 0, 0, 0);
    const auto draw = [&](unsigned int mask) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        lightning.render(0.0, mask);
        std::array<unsigned char, 32 * 32 * 4> pixels{};
        glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    const auto green = draw(1u);
    const auto orange = draw(16u);
    const auto chord = draw(1u | 16u);
    EXPECT_NE(green, orange);
    EXPECT_NE(chord, green);
    EXPECT_NE(chord, orange);
    EXPECT_EQ(chord, draw(1u | 16u | 32u | 256u | 512u));
    // Under identity projection, the outer buttons occupy the left/right
    // halves.
    const auto blueEnergy = [](const auto &pixels, int begin, int end) {
        unsigned int result = 0;
        for (int y = 0; y < 32; ++y)
            for (int x = begin; x < end; ++x)
                result += pixels[static_cast<size_t>((y * 32 + x) * 4 + 2)];
        return result;
    };
    EXPECT_GT(blueEnergy(green, 0, 16), 100u);
    EXPECT_EQ(blueEnergy(green, 16, 32), 0u);
    EXPECT_GT(blueEnergy(orange, 16, 32), 100u);
    EXPECT_EQ(blueEnergy(orange, 0, 16), 0u);
    EXPECT_GT(blueEnergy(chord, 0, 16), 100u);
    EXPECT_GT(blueEnergy(chord, 16, 32), 100u);
    EXPECT_EQ(manager.eventsSize(), before);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST(ParticleEmission, RateIsBoundedAcrossFrameRatesAndDoesNotCatchUp) {
    for (const int fps : {30, 60, 144, 240, 1000}) {
        CParticle particles;
        CParticle::particleData data{};
        data.emitter = 0;
        data.duration = 0.25;
        int emitted = 0;
        for (int frame = 0; frame < fps * 10; ++frame) {
            const double now = static_cast<double>(frame) / fps;
            auto &live = particles.part;
            live.erase(std::remove_if(live.begin(), live.end(),
                                      [&](const auto &p) {
                                          return now - p.addedAt >= p.duration;
                                      }),
                       live.end());
            emitted += particles.emitFire(data, now) ? 1 : 0;
            ASSERT_LE(live.size(), 8u);
        }
        EXPECT_GE(emitted, 295) << fps;
        EXPECT_LE(emitted, 301) << fps;
        EXPECT_TRUE(particles.emitFire(data, 100.0));
        for (int i = 0; i < 10; ++i)
            EXPECT_FALSE(particles.emitFire(data, 100.0));
    }
}

TEST(ParticleEmission, PerLaneLimitRejectsLongLivedExcess) {
    CParticle particles;
    CParticle::particleData data{};
    data.duration = 100.0;
    data.emitter = 0;
    for (int i = 0; i < 100; ++i)
        particles.emitFire(data, i * 0.1);
    EXPECT_EQ(particles.part.size(), 8u);
    data.emitter = 1;
    EXPECT_TRUE(particles.emitFire(data, 10.0));
    data.emitter = 5;
    EXPECT_FALSE(particles.emitFire(data, 10.0));
}

TEST_F(ShaderGL, EffectsArePreparedOnceAndSharedAcrossInstances) {
    auto &manager = CShader::inst();
    EXPECT_EQ(manager.eventsSize(), 2u);
    for (int i = 0; i < 20; ++i) {
        manager.prepareEffects();
        CParticle fire;
        CPlusLightning lightning;
        fire.renderFire();
        lightning.render(0.0, 31u);
    }
    EXPECT_EQ(manager.eventsSize(), 2u);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    CShader::shutdown();
    EXPECT_LT(manager.fireProgram().event, 0);
    EXPECT_LT(manager.lightningProgram().event, 0);
    CParticle fire;
    CPlusLightning lightning;
    fire.renderFire();
    lightning.render(0.0, 31u);
    EXPECT_EQ(manager.eventsSize(), 0u); // Drawing must never compile.
    manager.prepareEffects();
    EXPECT_EQ(manager.eventsSize(), 2u);
}

} // namespace

namespace {
struct ObjFixture {
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "gpp-background-tests";
    ObjFixture() {
        std::filesystem::create_directories(directory);
        std::ofstream file(directory / "scene.obj");
        file << "o floor\nv 2 0 -7\nv 4 0 -7\nv 4 0 -5\nv 2 0 -5\n"
                "f 4 3 2 1\no box\nv 3 1 -6\nv 3.5 1 -6\nv 3.5 1 -5.5\nf 5 6 "
                "7\n";
    }
    ~ObjFixture() {
        std::filesystem::remove(directory / "scene.obj");
        std::filesystem::remove(directory / "scene.obj.ao");
        std::filesystem::remove(directory / "scene.obj.ao.pgm");
        std::filesystem::remove(directory / "scene.mtl");
        std::filesystem::remove(directory);
    }
};
TEST_F(ShaderGL, BackgroundShadersCompileAndLinkOnGL20) {
    EXPECT_GE(CShader::inst().addProgram("background-test",
                                         BackgroundShader::vertex,
                                         BackgroundShader::fragment),
              0);
    EXPECT_GE(CShader::inst().addProgram("background-depth-test",
                                         BackgroundShader::depthVertex,
                                         BackgroundShader::depthFragment),
              0);
}
TEST_F(ShaderGL, ObjUploadMoveReloadAndUntexturedMaterialAreSafe) {
    ObjFixture fixture;
    GPPOBJ model;
    ASSERT_TRUE(model.loadInternalObj(fixture.directory.string(), "scene.obj"));
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glEnable(GL_TEXTURE_2D);
    int calls = 0;
    model.drawMaterials([&](const ObjModel::Part &part,
                            const ObjModel::Material &, bool textured) {
        ++calls;
        EXPECT_FALSE(textured);
        EXPECT_FALSE(part.textured);
        GLint bound = -1;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        EXPECT_EQ(bound, 0);
        return true;
    });
    EXPECT_EQ(calls, 2);
    GLint bound = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    EXPECT_EQ(bound, static_cast<GLint>(texture));
    GPPOBJ moved(std::move(model));
    moved.draw(0);
    ASSERT_TRUE(moved.loadInternalObj(fixture.directory.string(), "scene.obj"));
    moved.draw(0);
    EXPECT_FALSE(
        moved.loadInternalObj(fixture.directory.string(), "missing.obj"));
    moved.draw(0);
    moved.unload();
    moved.draw(0);
    glDeleteTextures(1, &texture);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}
TEST_F(ShaderGL, BackgroundAOChangesAmbientPixelsAndCanBeDisabled) {
    ObjFixture fixture;
    {
        std::ofstream obj(fixture.directory / "scene.obj");
        obj << "mtllib scene.mtl\nusemtl matte\nv 2 0 -7\nv 4 0 -7\nv 4 0 "
               "-5\nv 2 0 -5\nf 4 3 2 1\n";
        std::ofstream mtl(fixture.directory / "scene.mtl");
        mtl << "newmtl matte\nKa 1 1 1\nKd .2 .2 .2\nKs 0 0 0\n";
    }
    std::ifstream obj(fixture.directory / "scene.obj");
    auto parsed = ObjModel::read(obj);
    SceneAO::Atlas atlas;
    atlas.width = atlas.height = 2;
    atlas.pixels.assign(4, 0);
    atlas.coordinates.assign(6, ObjModel::Vec2{.5f, .5f});
    SceneAO::write(fixture.directory / "scene.obj", parsed, atlas);
    GPPOBJ model;
    ASSERT_TRUE(model.loadInternalObj(fixture.directory.string(), "scene.obj"));
    auto &renderer = CBackgroundRenderer::inst();
    renderer.quality = 1;
    CEngine::engine().nowCamera.eye = {3, 3, -4};
    const auto projection =
        glm::perspective(glm::radians(45.f), 1.f, .1f, 20.f) *
        glm::lookAt(glm::vec3(3, 3, -4), glm::vec3(3, 0, -6),
                    glm::vec3(0, 1, 0));
    glViewport(0, 0, 32, 32);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0, 0, 0, 1);
    auto pixel = [&](bool ao) {
        renderer.ambientOcclusion = ao;
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderer.draw(model);
        std::array<unsigned char, 4> color{};
        glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color.data());
        return color;
    };
    const auto occluded = pixel(true), plain = pixel(false);
    EXPECT_TRUE(model.hasAO());
    EXPECT_GT(occluded[0], 10);
    EXPECT_GT(plain[0], occluded[0] + 3);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    renderer.quality = 2;
    renderer.ambientOcclusion = true;
}
TEST_F(ShaderGL, BackgroundRestoresStateAndReusesStaticShadow) {
    ObjFixture fixture;
    GPPOBJ model;
    ASSERT_TRUE(model.loadInternalObj(fixture.directory.string(), "scene.obj"));
    auto &renderer = CBackgroundRenderer::inst();
    renderer.quality = 2;
    auto p = make_program();
    auto v = CShader::compileShader(vertex, VERTEX),
         f = CShader::compileShader(fragment, FRAGMENT);
    CShader::attachShaderToProgram(p, v);
    CShader::attachShaderToProgram(p, f);
    CShader::linkShaderProgram(p);
    glUseProgram(p.get());
    glViewport(1, 2, 20, 21);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(.2f, .3f, .4f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glScalef(.7f, .8f, .9f);
    std::array<float, 16> before{}, after{};
    glGetFloatv(GL_PROJECTION_MATRIX, before.data());
    glActiveTexture(GL_TEXTURE1);
    renderer.draw(model);
    GLint value = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &value);
    EXPECT_EQ(value, static_cast<GLint>(p.get()));
    glGetIntegerv(GL_ACTIVE_TEXTURE, &value);
    EXPECT_EQ(value, GL_TEXTURE1);
    glGetIntegerv(GL_MATRIX_MODE, &value);
    EXPECT_EQ(value, GL_PROJECTION);
    glGetFloatv(GL_PROJECTION_MATRIX, after.data());
    EXPECT_EQ(before, after);
    std::array<GLint, 4> viewport{};
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    EXPECT_EQ(viewport, (std::array<GLint, 4>{1, 2, 20, 21}));
    GLboolean mask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
    EXPECT_FALSE(mask);
    EXPECT_TRUE(glIsEnabled(GL_BLEND));
    const auto updates = renderer.shadowUpdates();
    if (GLEW_EXT_framebuffer_object)
        EXPECT_EQ(updates, 1u);
    CEngine::engine().nowCamera.eye.x += 1.;
    renderer.draw(model);
    EXPECT_EQ(renderer.shadowUpdates(), updates);
    ASSERT_TRUE(model.loadInternalObj(fixture.directory.string(), "scene.obj"));
    renderer.draw(model);
    if (updates)
        EXPECT_EQ(renderer.shadowUpdates(), updates + 1);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    renderer.quality = 0;
    renderer.draw(model);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    renderer.quality = 2;
}
} // namespace
