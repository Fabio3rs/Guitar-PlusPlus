#include "ui.h"
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <png.h>
#include <stdexcept>

namespace {
void capture(const std::filesystem::path &file, int width, int height) {
    std::vector<unsigned char> pixels(size_t(width) * size_t(height) * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    std::vector<unsigned char> flipped(pixels.size());
    const size_t stride = size_t(width) * 4;
    for (int y = 0; y < height; ++y) {
        std::copy_n(pixels.data() + size_t(height - y - 1) * stride, stride,
                    flipped.data() + size_t(y) * stride);
    }
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = static_cast<png_uint_32>(width);
    image.height = static_cast<png_uint_32>(height);
    image.format = PNG_FORMAT_RGBA;
    if (!png_image_write_to_file(&image, file.string().c_str(), 0,
                                 flipped.data(), 0, nullptr)) {
        throw std::runtime_error("PNG capture: " + std::string(image.message));
    }
}
} // namespace

int main(int argc, char **argv) {
    using namespace GuiStudio;
    GLFWwindow *window = nullptr;
    try {
        fs::path assets,
            output = fs::path(GPP_GUI_WORKSPACE) / "build/gui-studio";
        fs::path font, menus, screenshot;
        int width = 1280, height = 800, frames = 0, panel = 0;
        bool hidden = false;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            auto value = [&]() -> std::string {
                if (++i == argc) {
                    throw std::runtime_error("Missing value for " + option);
                }
                return argv[i];
            };
            if (option == "--help") {
                std::cout
                    << "gpp_gui_studio [--asset-root DIR] [--output-dir DIR]\n"
                       "  [--font Theme.lua] [--menus Gui.lua] [--panel "
                       "menus|fonts]\n"
                       "  [--width N] [--height N] [--frames N] [--ui-capture "
                       "FILE.png] [--hidden]\n"
                       "Paths inside font scripts are relative to the asset "
                       "root. Exports are explicit.\n";
                return 0;
            } else if (option == "--asset-root") {
                assets = value();
            } else if (option == "--output-dir") {
                output = value();
            } else if (option == "--font") {
                font = value();
            } else if (option == "--menus") {
                menus = value();
            } else if (option == "--panel") {
                const auto which = value();
                if (which != "menus" && which != "fonts") {
                    throw std::runtime_error("Panel must be menus or fonts");
                }
                panel = which == "fonts" ? 1 : 0;
            } else if (option == "--width") {
                width = std::stoi(value());
            } else if (option == "--height") {
                height = std::stoi(value());
            } else if (option == "--frames") {
                frames = std::stoi(value());
                if (frames < 1) {
                    throw std::runtime_error("Frames must be positive");
                }
            } else if (option == "--ui-capture") {
                screenshot = value();
            } else if (option == "--hidden") {
                hidden = true;
            } else {
                throw std::runtime_error("Unknown argument: " + option);
            }
        }
        if (width < 640 || height < 480 || width > 4096 || height > 4096) {
            throw std::runtime_error(
                "Window dimensions must be 640x480 to 4096x4096");
        }
        if (assets.empty()) {
            assets = fs::path(GPP_GUI_WORKSPACE) / "build/bin";
            if (!fs::exists(assets / "data/sprites/FONT.tga")) {
                assets = GPP_GUI_WORKSPACE;
            }
        }
        fs::create_directories(output);
        if (!screenshot.empty() && !frames) {
            frames = 5;
        }
        if (hidden && !frames) {
            throw std::runtime_error(
                "--hidden requires --frames or --ui-capture");
        }
        if (!glfwInit()) {
            throw std::runtime_error(
                "Cannot initialize GLFW (use Xvfb on headless Linux)");
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_VISIBLE, hidden ? GLFW_FALSE : GLFW_TRUE);
        window = glfwCreateWindow(width, height, "Guitar++ GUI Studio", nullptr,
                                  nullptr);
        if (!window) {
            throw std::runtime_error("Cannot create OpenGL 2.0 window");
        }
        glfwMakeContextCurrent(window);
        if (glewInit() != GLEW_OK) {
            throw std::runtime_error("Cannot initialize GLEW");
        }
        while (glGetError() != GL_NO_ERROR) {
        }
        glfwSwapInterval(1);
        {
            Session session(assets, output);
            if (!font.empty()) {
                session.loadFonts(font);
            }
            if (!menus.empty()) {
                session.loadMenus(menus);
            }
            UI ui(session, window);
            int count = 0;
            while (!ui.closeApproved) {
                glfwPollEvents();
                if (glfwWindowShouldClose(window)) {
                    glfwSetWindowShouldClose(window, GLFW_FALSE);
                    ui.requestClose();
                }
                int w = 0, h = 0;
                glfwGetFramebufferSize(window, &w, &h);
                ui.draw(count == 0 ? panel : -1);
                glViewport(0, 0, w, h);
                glClearColor(.08f, .09f, .11f, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                ++count;
                if (frames && count >= frames && !screenshot.empty()) {
                    capture(output / screenshot, w, h);
                }
                glfwSwapBuffers(window);
                if (frames && count >= frames) {
                    break;
                }
            }
        }
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "GUI Studio: " << e.what() << '\n';
        if (window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
        return 1;
    }
}
