#include "CLog.h"
#include "json.h"
#include "studio.h"
#include <iostream>

int main(int argc, char **argv) {
    using namespace Studio;
    CLog::configureStderr();
    try {
        if (argc != 2) {
            throw Error(
                "arguments",
                "Usage: gpp_studio_egl_probe output.png (requires hardware)");
        }
        Context context("egl", false, true);
        Target target;
        target.resize(64, 64, context.caps);
        target.bind();
        glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
        glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
        glViewport(0, 0, 64, 64);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        CEngine::engine().setColor(1, 0, 0, 1);
        glBegin(GL_TRIANGLES);
        glVertex2f(-.8f, -.8f);
        glVertex2f(.8f, -.8f);
        glVertex2f(0, .8f);
        glEnd();
        const auto pixels = readPixels(64, 64);
        if (pixels[(32 * 64 + 32) * 4] != 255 || pixels[0] != 0) {
            throw Error("probe", "Unexpected triangle pixels");
        }
        const auto file = fs::absolute(argv[1]);
        fs::create_directories(file.parent_path());
        writePNG(file, 64, 64, pixels);
        Json result(rapidjson::kObjectType);
        auto &a = result.GetAllocator();
        put(result, "renderer", context.caps.renderer, a);
        put(result, "vendor", context.caps.vendor, a);
        put(result, "version", context.caps.version, a);
        put(result, "acceleration", context.caps.acceleration, a);
        put(result, "capture", file.string(), a);
        put(result, "pixels_verified", true, a);
        std::cout << encode(result) << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
