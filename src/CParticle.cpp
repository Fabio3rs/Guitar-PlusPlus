#include "CParticle.h"
#include "CEngine.h"
#include "CShader.h"
#include <GL/glew.h>

void CParticle::addParticle(const particleData &pdata) {
    part.push_front(pdata);

    part.front().addedAt = CEngine::engine().getTime();
}

bool CParticle::emitFire(const particleData &pdata, double now) {
    if (pdata.emitter < 0 || pdata.emitter >= 5 || !std::isfinite(now))
        return false;
    auto &next = nextFireEmission[static_cast<size_t>(pdata.emitter)];
    if (now < next)
        return false;
    constexpr double interval = 1.0 / 30.0;
    next = now - next < interval ? next + interval : now + interval;
    size_t live = 0;
    for (const auto &p : part)
        if (p.emitter == pdata.emitter && now - p.addedAt < p.duration)
            ++live;
    if (live >= 8)
        return false;
    part.push_front(pdata);
    part.front().addedAt = now;
    return true;
}

void CParticle::moveEmitter(int emitter, double height) {
    if (emitter < 0)
        return;
    for (auto &particle : part) {
        if (particle.emitter == emitter) {
            particle.y += height - particle.emitterHeight;
            particle.emitterHeight = height;
        }
    }
}

void CParticle::render() {
    partDrawData.clear();
    partDrawData.autoEnDisaColors = true;
    partDrawData.useColors = true;
    double deltaTime = CEngine::engine().getDeltaTime();
    double time = CEngine::engine().getTime();

    CEngine::RenderDoubleStruct pstruct{};

    pstruct.TextureX1 = 0.0;
    pstruct.TextureX2 = 1.0;

    pstruct.TextureY1 = 0.0;
    pstruct.TextureY2 = 1.0;

    for (auto &p : part) {
        if (p.duration <= 0.0 || (time - p.addedAt) >= p.duration) {
            continue;
        }
        partDrawData.texture = p.texture;

        p.ax -= p.desac * deltaTime;
        p.ay -= p.desac * deltaTime;
        p.az -= p.desac * deltaTime;

        p.sx += p.ax * deltaTime;
        p.sy += p.ay * deltaTime;
        p.sz += p.az * deltaTime;

        p.x += p.sx * deltaTime;
        p.y += p.sy * deltaTime;
        p.z += p.sz * deltaTime;

        pstruct.x1 = p.x;
        pstruct.x2 = p.x + p.sizex;
        pstruct.x3 = p.x + p.sizex;
        pstruct.x4 = p.x;

        pstruct.y1 = p.y + p.sizey;
        pstruct.y2 = p.y + p.sizey;
        pstruct.y3 = p.y;
        pstruct.y4 = p.y;

        pstruct.z1 = p.z + p.sizez;
        pstruct.z2 = p.z + p.sizez;
        pstruct.z3 = p.z;
        pstruct.z4 = p.z;

        double alpha =
            std::clamp(1.0 - (time - p.addedAt) / p.duration, 0.0, 1.0);

        pstruct.alphaTop = alpha;
        pstruct.alphaBottom = alpha;

        if (p.crossedFlame) {
            // Two vertical planes at +/-45 degrees to the X axis. Both have
            // depth, so the flame is not a single sheet when viewed obliquely.
            constexpr double diagonal = 0.7071067811865476;
            const double dx = p.sizex * 0.5 * diagonal;
            const double dz = p.sizex * 0.5 * diagonal;
            pstruct.alphaTop = pstruct.alphaBottom = alpha * 0.65;
            for (const double direction : {-1.0, 1.0}) {
                pstruct.x1 = pstruct.x4 = p.x - dx;
                pstruct.x2 = pstruct.x3 = p.x + dx;
                pstruct.z1 = pstruct.z4 = p.z - direction * dz;
                pstruct.z2 = pstruct.z3 = p.z + direction * dz;
                CEngine::pushQuad(partDrawData, pstruct);
            }
        } else {
            CEngine::pushQuad(partDrawData, pstruct);
        }
    }

    if (!partDrawData.vArray.empty()) {
        {
            CEngine::engine().drawTrianglesWithAlpha(partDrawData);
        }
    }

    for (auto it = part.begin(); it != part.end(); /******/) {
        auto &itinst = (*it);
        if (itinst.duration > 0.0 &&
            (time - itinst.addedAt) < itinst.duration) {
            ++it;
        } else {
            it = part.erase(it);
        }
    }
}

void CParticle::renderFire(bool plusActive, double multiplier,
                           double publicApproval) {
    auto &shaders = ShaderProject::CShader::inst();
    if (!shaders.isShadersEnabled()) {
        render();
        return;
    }
    const auto &effect = shaders.fireProgram();
    if (effect.event < 0) {
        render();
        return;
    }

    // Keep the surrounding fretboard, tails and HUD rendering unchanged.
    struct RestoreState {
        GLint program = 0;
        RestoreState() {
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                         GL_POLYGON_BIT);
        }
        ~RestoreState() {
            glUseProgram(static_cast<GLuint>(program));
            glPopAttrib();
        }
    } restore;
    shaders.processEvent(effect.event);
    ShaderProject::CShader::setFloatUniform(
        effect.time, static_cast<float>(CEngine::engine().getTime()));
    ShaderProject::CShader::setFloatUniform(
        effect.plusActive, plusActive ? 1.0f : 0.0f);
    ShaderProject::CShader::setFloatUniform(
        effect.multiplier, static_cast<float>(multiplier));
    ShaderProject::CShader::setFloatUniform(
        effect.publicApproval, static_cast<float>(publicApproval));
    glEnable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Transparent edges should not hide later particles or tails.
    glDepthMask(GL_FALSE);
    render();
}

CParticle::CParticle() {}
