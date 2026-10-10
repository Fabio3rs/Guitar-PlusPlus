#include "CBackgroundRenderer.h"
#include "BackgroundShader.h"
#include "CLog.h"
#include "CLuaFunctions.hpp"
#include "CShader.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>

namespace {
using namespace ShaderProject;
CBackgroundRenderer *live = nullptr;
ProgramObject program(std::string_view vertex, std::string_view fragment) {
    auto v = CShader::compileShader(vertex, VERTEX);
    auto f = CShader::compileShader(fragment, FRAGMENT);
    auto p = CShader::createShaderProgram();
    CShader::attachShaderToProgram(p, v);
    CShader::attachShaderToProgram(p, f);
    CShader::linkShaderProgram(p);
    return p;
}
struct Uniforms {
    GLint texture = -1, textured = -1, scale = -1, offset = -1, clamp = -1,
          opacity = -1;
    GLint diffuse = -1, specular = -1, emission = -1, shininess = -1, eye = -1,
          light = -1;
    GLint shadow = -1, shadows = -1, lightMatrix = -1, texel = -1, bias = -1,
          ao = -1, aoEnabled = -1, ambient = -1, cutoff = -1;
    void read(const ProgramObject &p) {
        auto get = [&](const char *name) {
            return CShader::getProgramUniform(p, name);
        };
        cutoff = get("uAlphaCutoff");
        ao = get("uAO");
        aoEnabled = get("uAOEnabled");
        ambient = get("uAmbient");
        texture = get("uTexture");
        textured = get("uTextured");
        scale = get("uUVScale");
        offset = get("uUVOffset");
        clamp = get("uClampUV");
        opacity = get("uOpacity");
        diffuse = get("uDiffuse");
        specular = get("uSpecular");
        emission = get("uEmission");
        shininess = get("uShininess");
        eye = get("uEye");
        light = get("uLight");
        shadow = get("uShadow");
        shadows = get("uShadows");
        lightMatrix = get("uLightMatrix");
        texel = get("uShadowTexel");
        bias = get("uShadowBias");
    }
    void material(const ObjModel::Material &m, bool hasTexture) const {
        glUniform1i(textured, hasTexture);
        glUniform2fv(scale, 1, m.uvScale.data());
        glUniform2fv(offset, 1, m.uvOffset.data());
        glUniform1i(clamp, m.clampMap);
        glUniform1f(opacity, m.opacity);
    }
};
struct State {
    GLint program = 0, active = GL_TEXTURE0, matrix = GL_MODELVIEW,
          framebuffer = 0;
    bool fbo = false;
    explicit State(bool supportsFBO) : fbo(supportsFBO) {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_MATRIX_MODE, &matrix);
        if (fbo)
            glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &framebuffer);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
    }
    ~State() {
        if (fbo)
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,
                                 static_cast<GLuint>(framebuffer));
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glPopAttrib();
        glMatrixMode(static_cast<GLenum>(matrix));
        glUseProgram(static_cast<GLuint>(program));
        glActiveTexture(static_cast<GLenum>(active));
        CEngine::engine().invalidateRenderBindings();
    }
};
bool decal(const ObjModel::Part &part) { return part.material == "decal_oil"; }
struct Surface {
    ObjModel::Vec3 specular, emission;
    float shininess;
};
Surface garageMaterial(const ObjModel::Part &part,
                       const ObjModel::Material &source) {
    Surface m{source.specular, source.emission, source.shininess};
    // Garage MTL was exported with the same specular settings for almost every
    // surface. These narrowly scoped overrides do not affect other OBJ users.
    if (part.material == "light") {
        m.emission = {1.3f, 1.22f, 1.1f};
        m.specular = {0, 0, 0};
    } else if (part.material == "shelf" || part.material == "suport" ||
               part.material == "metal_1" || part.material == "rim") {
        m.specular = {.28f, .28f, .28f};
        m.shininess = 64.f;
    } else {
        m.specular = {.025f, .025f, .025f};
        m.shininess = 12.f;
    }
    if (source.illumination < 2)
        m.specular = {0, 0, 0};
    return m;
}
} // namespace
struct CBackgroundRenderer::Impl {
    ProgramObject lighting, depth;
    Uniforms lit, dep;
    bool attempted = false, fbo = false, shadowFailed = false;
    GLuint framebuffer = 0, depthTexture = 0;
    int resolution = 0;
    const GPPOBJ *cachedModel = nullptr;
    size_t cachedRevision = 0, updates = 0;
    bool garageMaterials = true;
    glm::vec3 light{3.5f, 2.88f, -6.05f};
    glm::mat4 lightMatrix{1.f};
    void releaseShadow() {
        if (framebuffer)
            glDeleteFramebuffersEXT(1, &framebuffer);
        if (depthTexture)
            glDeleteTextures(1, &depthTexture);
        framebuffer = depthTexture = 0;
        resolution = 0;
        cachedModel = nullptr;
    }
    ~Impl() {
        if (glfwGetCurrentContext())
            releaseShadow();
    }
    bool allocate(int size) {
        if (shadowFailed || !fbo)
            return false;
        if (resolution == size)
            return true;
        releaseShadow();
        GLint maxSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
        if (size > maxSize) {
            shadowFailed = true;
            return false;
        }
        glActiveTexture(GL_TEXTURE1);
        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glGenFramebuffersEXT(1, &framebuffer);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                  GL_TEXTURE_2D, depthTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) !=
            GL_FRAMEBUFFER_COMPLETE_EXT) {
            CLog::log() << "Background: shadow framebuffer unavailable; using "
                           "lighting only";
            releaseShadow();
            shadowFailed = true;
            return false;
        }
        resolution = size;
        lightMatrix = glm::perspective(glm::radians(150.f), 1.f, .1f, 20.f) *
                      glm::lookAt(light, light + glm::vec3(0, -1, 0),
                                  glm::vec3(0, 0, -1));
        return true;
    }
};
CBackgroundRenderer::CBackgroundRenderer() : impl(std::make_unique<Impl>()) {
    live = this;
    auto &vars = CLuaFunctions::GameVariables::gv();
    vars.pushVar("backgroundQuality", quality);
    vars.pushVar("backgroundAO", ambientOcclusion);
    vars.pushVar("backgroundShadowBias", shadowBias);
}
CBackgroundRenderer::~CBackgroundRenderer() { live = nullptr; }
CBackgroundRenderer &CBackgroundRenderer::inst() {
    static CBackgroundRenderer renderer;
    return renderer;
}
void CBackgroundRenderer::shutdown() {
    if (live)
        live->impl = std::make_unique<Impl>();
}
size_t CBackgroundRenderer::shadowUpdates() const { return impl->updates; }
void CBackgroundRenderer::setScene(const std::array<float, 3> &light, bool garageMaterials) {
    auto &r = *impl;
    r.light = {light[0], light[1], light[2]};
    r.garageMaterials = garageMaterials;
    r.lightMatrix = glm::perspective(glm::radians(150.f), 1.f, .1f, 20.f) *
                    glm::lookAt(r.light, r.light + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
    // The same GPPOBJ address/revision can be reused after moving a new scene.
    r.cachedModel = nullptr;
}
void CBackgroundRenderer::prepare() {
    if (impl->attempted)
        return;
    impl->attempted = true;
    impl->fbo = GLEW_EXT_framebuffer_object && glGenFramebuffersEXT &&
                glBindFramebufferEXT;
    try {
        impl->lighting =
            program(BackgroundShader::vertex, BackgroundShader::fragment);
        impl->lit.read(impl->lighting);
    } catch (const std::exception &e) {
        CLog::log() << std::string("Background lighting: ") + e.what();
    }
    try {
        impl->depth = program(BackgroundShader::depthVertex,
                              BackgroundShader::depthFragment);
        impl->dep.read(impl->depth);
    } catch (const std::exception &e) {
        CLog::log() << std::string("Background shadows: ") + e.what();
    }
}
void CBackgroundRenderer::draw(GPPOBJ &model) {
    if (quality <= 0 || !CShader::inst().isShadersEnabled()) {
        model.draw(0);
        return;
    }
    prepare();
    auto &r = *impl;
    if (!r.lighting.get()) {
        model.draw(0);
        return;
    }
    model.prepareForDraw();
    State state(r.fbo);
    bool shadows = false;
    if (quality >= 2 && r.depth.get()) {
        // Separate state scope so depth-pass matrices/viewport/color masks
        // never leak into the color pass or the fretboard/HUD.
        State shadowState(r.fbo);
        if (r.allocate(quality >= 3 ? 1024 : 512)) {
            shadows = true;
            if (r.cachedModel != &model ||
                r.cachedRevision != model.revision()) {
                glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, r.framebuffer);
                glViewport(0, 0, r.resolution, r.resolution);
                glDrawBuffer(GL_NONE);
                glReadBuffer(GL_NONE);
                glDisable(GL_SCISSOR_TEST);
                glDisable(GL_BLEND);
                glDisable(GL_ALPHA_TEST);
                glDisable(GL_CULL_FACE);
                glDisable(GL_STENCIL_TEST);
                glDisable(GL_POLYGON_OFFSET_FILL);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                glDepthRange(0.0, 1.0);
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LESS);
                glDepthMask(GL_TRUE);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glClearDepth(1.0);
                glClear(GL_DEPTH_BUFFER_BIT);
                glMatrixMode(GL_PROJECTION);
                glLoadMatrixf(glm::value_ptr(r.lightMatrix));
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                glUseProgram(r.depth.get());
                glUniform1i(r.dep.texture, 0);
                const auto renderedRevision = model.revision();
                model.drawMaterials([&](const ObjModel::Part &part,
                                        const ObjModel::Material &m,
                                        bool textured) {
                    if (m.opacity < .5f || (r.garageMaterials &&
                        (decal(part) || part.material == "light")))
                        return false;
                    r.dep.material(m, textured);
                    return true;
                });
                r.cachedModel = &model;
                r.cachedRevision = renderedRevision;
                ++r.updates;
            }
        }
    }
    glUseProgram(r.lighting.get());
    glDisable(GL_LIGHTING);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadows ? r.depthTexture : 0);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(r.lit.ao, 2);
    glUniform1i(r.lit.aoEnabled, ambientOcclusion && model.hasAO());
    glUniform1i(r.lit.texture, 0);
    glUniform1i(r.lit.shadow, 1);
    glUniform1i(r.lit.shadows, shadows);
    glUniformMatrix4fv(r.lit.lightMatrix, 1, GL_FALSE,
                       glm::value_ptr(r.lightMatrix));
    glUniform3fv(r.lit.light, 1, glm::value_ptr(r.light));
    const auto &eye = CEngine::engine().nowCamera.eye;
    glUniform3f(r.lit.eye, static_cast<float>(eye.x), static_cast<float>(eye.y),
                static_cast<float>(eye.z));
    glUniform1f(r.lit.texel,
                shadows ? 1.f / static_cast<float>(r.resolution) : 0.f);
    glUniform1f(r.lit.bias,
                static_cast<float>(std::clamp(
                    std::isfinite(shadowBias) ? shadowBias : .001, 0., .02)));
    // Draw opaque geometry before decals and translucent material surfaces.
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 0) {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
        } else {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
        }
        glUniform1f(r.lit.cutoff, pass == 0 ? .5f : .05f);
        model.drawMaterials([&](const ObjModel::Part &part,
                                const ObjModel::Material &source,
                                bool textured) {
            const bool translucent = (r.garageMaterials && decal(part)) || source.opacity < .999f;
            if (translucent != (pass == 1))
                return false;
            const auto m = r.garageMaterials ? garageMaterial(part, source) :
                Surface{source.specular, source.emission, source.shininess};
            r.lit.material(source, textured);
            glUniform3fv(r.lit.ambient, 1, source.ambient.data());
            glUniform3fv(r.lit.diffuse, 1, source.diffuse.data());
            glUniform3fv(r.lit.specular, 1, m.specular.data());
            glUniform3fv(r.lit.emission, 1, m.emission.data());
            glUniform1f(r.lit.shininess, m.shininess);
            return true;
        });
    }
}
