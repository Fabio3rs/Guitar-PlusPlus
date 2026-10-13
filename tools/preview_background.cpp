#include "CBackgroundRenderer.h"
#include "CShader.h"
#include "GPPGame.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>

// Hidden-window diagnostic. Run from a scratch directory because the existing
// game texture manager initializes the game's save/configuration singleton.
int main(int argc, char **argv) {
    if (argc < 4) {
        std::cerr << "Usage: gpp_preview_background model.obj output.ppm "
                     "quality [frames]\n";
        return 1;
    }
    if (!glfwInit())
        return 2;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    constexpr int width = 960, height = 540;
    auto *window =
        glfwCreateWindow(width, height, "background preview", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 2;
    }
    glfwMakeContextCurrent(window);
    if (glewInit() != GLEW_OK)
        return 2;
    int result = 0;
    {
        GPPGame::GuitarPP().mainthread = std::this_thread::get_id();
        GPPOBJ model;
        const auto path = std::filesystem::absolute(argv[1]);
        if (!model.loadInternalObj(path.parent_path().string(),
                                   path.filename().string()))
            return 3;
        auto &renderer = CBackgroundRenderer::inst();
        renderer.quality = std::stoi(argv[3]);
        auto &engine = CEngine::engine();
        engine.nowCamera.eye = {3.0, 2.5, 2.0};
        engine.nowCamera.center = {3.0, .5, -5.0};
        engine.nowCamera.up = {0, 1, 0};
        const auto projection =
            glm::perspective(glm::radians(45.f), float(width) / float(height),
                             .005f, 100.f) *
            glm::lookAt(glm::vec3(engine.nowCamera.eye),
                        glm::vec3(engine.nowCamera.center),
                        glm::vec3(engine.nowCamera.up));
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(glm::value_ptr(projection));
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_NORMALIZE);
        glClearColor(.03f, .03f, .03f, 1);
        // Match the old fixed-function background's light for quality=0.
        const GLfloat ambient[] = {.1f, .1f, .1f, 1},
                      diffuse[] = {1, .965f, .93f, 1},
                      position[] = {3, 2.7f, -1.5f, 1};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
        glLightfv(GL_LIGHT1, GL_AMBIENT, ambient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
        glLightfv(GL_LIGHT1, GL_POSITION, position);
        glEnable(GL_LIGHT1);
        glEnable(GL_LIGHTING);
        glEnable(GL_COLOR_MATERIAL);
        CEngine::engine().setColor(1, 1, 1, 1);
        std::vector<double> timings;
        int frames = argc > 4 ? std::max(2, std::stoi(argv[4])) : 12;
        for (int i = 0; i < frames; ++i) {
            auto start = std::chrono::steady_clock::now();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderer.draw(model);
            glFinish();
            if (i > 1)
                timings.push_back(std::chrono::duration<double, std::milli>(
                                      std::chrono::steady_clock::now() - start)
                                      .count());
        }
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        std::vector<unsigned char> pixels(static_cast<size_t>(width) * height *
                                          3);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE,
                     pixels.data());
        std::ofstream output(argv[2], std::ios::binary);
        output << "P6\n" << width << ' ' << height << "\n255\n";
        for (int y = height - 1; y >= 0; --y)
            for (int x = 0; x < width * 3; ++x)
                output.put(static_cast<char>(
                    pixels[static_cast<size_t>(y) * width * 3 +
                           static_cast<size_t>(x)]));
        std::sort(timings.begin(), timings.end());
        std::cout << "Renderer: " << glGetString(GL_RENDERER)
                  << "\nGL: " << glGetString(GL_VERSION) << "\n";
        if (!timings.empty())
            std::cout << "Median ms: " << timings[timings.size() / 2]
                      << ", p95 ms: "
                      << timings[static_cast<size_t>(
                             double(timings.size() - 1) * .95)]
                      << '\n';
        std::cout << "Shadow updates: " << renderer.shadowUpdates()
                  << ", AO: " << model.hasAO() << '\n';
        auto error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "GL error " << error << '\n';
            result = 4;
        }
        CBackgroundRenderer::shutdown();
        ShaderProject::CShader::shutdown();
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
