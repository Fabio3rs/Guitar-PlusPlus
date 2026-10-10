// Run from the directory containing data/sprites/FONT.tga.
#include "CFonts.h"
#include "CMenu.h"
#include "GPPGame.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
int main(int argc, char **argv) {
    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    const int width = argc > 1 ? std::max(1, std::atoi(argv[1])) : 800;
    const int height = argc > 2 ? std::max(1, std::atoi(argv[2])) : 600;
    auto *w = glfwCreateWindow(width, height, "UI benchmark", nullptr, nullptr);

    if (!w) {
        return 2;
    }

    glfwMakeContextCurrent(w);

    if (glewInit() != GLEW_OK) {
        return 3;
    }

    GPPGame::GuitarPP().mainthread = std::this_thread::get_id();
    glViewport(0, 0, width, height);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-static_cast<double>(width) / height,
            static_cast<double>(width) / height, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const auto initialization = std::chrono::steady_clock::now();
    auto &f = CFonts::fonts();
    std::cout << "font initialization us: "
              << std::chrono::duration<double, std::micro>(
                     std::chrono::steady_clock::now() - initialization)
                     .count()
              << '\n';
    f.addTextureToFont(
        "bench", "data/sprites", "FONT.tga",
        "0123456789ABCDEFGHIJKLMNOPQRSTUV\nWXYZabcdefghijklmnopqrstuvwxyz.,\n"
        "ÃÂÁÀÄÊÉÈËÎÍÌÏÕÔÓÒÖŨÛÚÙÜãâáàäêéèë\n"
        "îíìïõôóòöũûúùüÑñÇç:\\|/<>()-+=*[]\n?!{}@#$%&ªº£¬_\n\n\n");
    volatile double sink = 0;

    for (int count : {5, 100, 500, 1000}) {
        std::vector<double> samples;

        for (int run = 0; run < 1100; ++run) {
            auto begin = std::chrono::steady_clock::now();

            for (int i = 0; i < count; ++i) {
                sink += f.getXSizeInScreen("Guitar++ ação Wiii gpqy 012345",
                                           .075, "default");
            }

            auto us = std::chrono::duration<double, std::micro>(
                          std::chrono::steady_clock::now() - begin)
                          .count();

            if (run >= 100) {
                samples.push_back(us);
            }
        }

        std::sort(samples.begin(), samples.end());
        std::cout << "measure " << count
                  << " median/p95/p99 us: " << samples[500] << '/'
                  << samples[950] << '/' << samples[990] << '\n';
    }

    for (int count : {5, 100, 500, 1000}) {
        CMenu menu("benchmark-menu");
#ifdef GPP_UI_CURRENT
        menu.viewportEnabled = true;
        menu.viewport = {-1, -.8, 1, .8};
#endif
        for (int i = 0; i < count; ++i) {
            CMenu::menuOpt opt;
            opt.text = "Guitar++ ação Wiii gpqy";
            opt.type = CMenu::textbtn;
            opt.x = -.9;
            opt.y = .7 - i * .1;

            opt.group = 1;
            menu.addOpt(opt);
        }

        std::vector<double> samples;

        for (int run = 0; run < 220; ++run) {
            glFinish(); // benchmark boundary only; never part of menu rendering
            glClear(GL_COLOR_BUFFER_BIT);
            const auto begin = std::chrono::steady_clock::now();
            menu.render();
            const auto us = std::chrono::duration<double, std::micro>(
                                std::chrono::steady_clock::now() - begin)
                                .count();

            if (run >= 20) {
                samples.push_back(us);
            }
        }

        std::sort(samples.begin(), samples.end());
        std::cout << "menu submit " << count
                  << " median/p95/p99 us: " << samples[100] << '/'
                  << samples[190] << '/' << samples[198] << '\n';
    }

    glfwDestroyWindow(w);
    glfwTerminate();
    return sink == -1;
}
