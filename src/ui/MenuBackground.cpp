#include "CBackgroundRenderer.h"
#include "CMenuSystem.h"
#include "GPPGame.h"

void CMenuSystem::drawBackground() {
    auto &engine = CEngine::engine();
    engine.activate3DRender(true);
    engine.activateLighting(true);

    {
        CEngine::cameraSET usingCamera;
        usingCamera.eye.x = 3.0;
        usingCamera.eye.y = 1.75;
        usingCamera.eye.z = 2.7;
        usingCamera.center.x = 3.0;
        usingCamera.center.y = 1.3;
        usingCamera.center.z = 0;
        usingCamera.up.x = 0;
        usingCamera.up.y = 1;
        usingCamera.up.z = 0;

        engine.setCamera(usingCamera);
    }

    {
        lightData l{};

        for (auto &t : l.ambientLight) {
            t = 0.1f;
        }

        for (auto &t : l.direction) {
            t = 2.5f;
        }

        for (auto &t : l.position) {
            t = 0.0f;
        }

        for (auto &t : l.specularLight) {
            t = 1.0f;
        }

        for (auto &t : l.diffuseLight) {
            t = 0.2f;
        }

        l.specularLight[1] = 1.0f;
        l.specularLight[2] = 1.0f;
        l.diffuseLight[0] = 1.0f;
        l.diffuseLight[1] = 1.0f;
        l.ambientLight[3] = 0.1f;

        CEngine::colorRGBToArrayf(0xFFF6ED, l.diffuseLight);

        l.angle = 180.0f;
        l.direction[0] = 3.0f;
        l.direction[1] = -0.5f;
        l.direction[2] = -1.5f;

        l.position[0] = 3.0f;
        l.position[1] = 2.7f;
        l.position[2] = -1.5f;
        l.position[3] = 1.0f;

        engine.activateLight(0, false);
        engine.activateLight(1, true);
        engine.setLight(l, 1);
    }

    engine.activateNormals(true);
    CBackgroundRenderer::inst().draw(host.testobj);
    engine.activateNormals(false);

    engine.activateLighting(false);
    engine.activate3DRender(false);
}

void CMenuSystem::waitForScreen(double duration) {
    auto &engine = CEngine::engine();
    const double start = engine.getTime();
    while (engine.windowOpened() && engine.getTime() - start < duration) {
        engine.clearScreen();
        drawBackground();
        engine.renderFrame();
    }
}
