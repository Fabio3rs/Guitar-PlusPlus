#include "CPlusLightning.h"
#include "CShader.h"
#include <GL/glew.h>
#include <algorithm>
#include <cmath>

void CPlusLightning::render(double elapsed, unsigned int noteMask) {
    const unsigned int lanes = noteMask & 0x1fu;
    if (lanes == 0u || !std::isfinite(elapsed) || elapsed < 0.0 ||
        elapsed >= duration) {
        return;
    }
    auto &shaders = ShaderProject::CShader::inst();
    if (!shaders.isShadersEnabled()) {
        return;
    }
    const auto &effect = shaders.lightningProgram();
    if (effect.event < 0) {
        return;
    }
    struct RestoreState {
        GLint program = 0;
        RestoreState() {
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                         GL_POLYGON_BIT | GL_CURRENT_BIT | GL_LIGHTING_BIT);
        }
        ~RestoreState() {
            glUseProgram(static_cast<GLuint>(program));
            glPopAttrib();
        }
    } restore;
    shaders.processEvent(effect.event);
    ShaderProject::CShader::setFloatUniform(effect.time,
                                            static_cast<float>(elapsed));
    const double fade = 1.0 - elapsed / duration;
    ShaderProject::CShader::setFloatUniform(effect.opacity,
                                            static_cast<float>(fade * fade));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    // Emit at each selected button center, using the gameplay lane spacing.
    for (unsigned int lane = 0; lane < 5; ++lane) {
        if ((lanes & (1u << lane)) == 0u) {
            continue;
        }
        const double centerX = -0.41 + lane * (0.2 + 0.2 / 48.0);
        ShaderProject::CShader::setFloatUniform(effect.spark, 0.0f);
        // Two crossed sheets per discharge.

        for (const double direction : {-1.0, 1.0}) {
            constexpr double halfWidth = 0.28;
            constexpr double bottom = -0.44;
            constexpr double top = 0.95;
            constexpr double centerZ = 0.75;
            const double dz = direction * 0.12;
            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(0, 0);
            glVertex3d(centerX - halfWidth, top, centerZ - dz);
            glTexCoord2f(0, 1);
            glVertex3d(centerX - halfWidth, bottom, centerZ - dz);
            glTexCoord2f(1, 0);
            glVertex3d(centerX + halfWidth, top, centerZ + dz);
            glTexCoord2f(1, 1);
            glVertex3d(centerX + halfWidth, bottom, centerZ + dz);
            glEnd();
        }
        // Small quads confine spark shading to their actual screen footprint.
        ShaderProject::CShader::setFloatUniform(effect.spark, 1.0f);
        glBegin(GL_QUADS);
        for (unsigned int i = 0; i < 12; ++i) {
            const double offset = static_cast<double>((i * 7u) % 12u) / 12.0;
            const double age = std::fmod(elapsed * 1.8 + offset, 1.0);
            const double direction = i % 2u ? 1.0 : -1.0;
            const double x = centerX + direction * (0.02 + age * 0.20);
            const double y = -0.44 + std::fmod(offset + age * 0.18, 1.0) * 1.39;
            const double radius = 0.009 * (1.0 - age);
            glTexCoord2f(0, 0);
            glVertex3d(x - radius, y + radius, 0.75);
            glTexCoord2f(0, 1);
            glVertex3d(x - radius, y - radius, 0.75);
            glTexCoord2f(1, 1);
            glVertex3d(x + radius, y - radius, 0.75);
            glTexCoord2f(1, 0);
            glVertex3d(x + radius, y + radius, 0.75);
        }
        glEnd();
    }
}
