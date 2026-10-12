#pragma once
#include "CBackgroundRenderer.h"
#include "CScene.h"
#include "camera.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <array>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace Studio {
namespace fs = std::filesystem;

struct Error : std::runtime_error {
    std::string code;

    Error(std::string c, const std::string &message)
        : std::runtime_error(message), code(std::move(c)) {}
};

struct Settings {
    Camera camera;
    Vec light{2, 4, 2};
    int quality = 2;
    bool ao = true;
    int viewMode = 0; // game, unlit, wireframe, game_wire
    bool grid = false, axes = false, bounds = false;
    double bias = .001, time = 0;
};

struct Capabilities {
    std::string vendor, renderer, version, acceleration, backend;
    int maxTexture = 0, maxRenderbuffer = 0;
    bool fbo = false, npot = false, depthTexture = false;
};
class Target;

class Context {
    std::unique_ptr<Target> parking;
    void *eglDisplay = nullptr, *eglContext = nullptr;
    bool glfwStarted = false;

  public:
    GLFWwindow *window = nullptr;
    Capabilities caps;
    Context(const std::string &backend, bool gui, bool requireHardware);
    ~Context();
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    void close() noexcept;
};

// All these objects live exclusively on the context owner's thread.
class Target {
    GLuint framebuffer = 0, depth = 0;

  public:
    GLuint texture = 0;
    int width = 0, height = 0;
    ~Target();
    Target() = default;
    Target(const Target &) = delete;
    Target &operator=(const Target &) = delete;
    void resize(int w, int h, const Capabilities &caps);
    void bind() const;
    void clear() noexcept;
};

void writePNG(const fs::path &file, int width, int height,
              const std::vector<unsigned char> &rgba);
std::vector<unsigned char> readPixels(int width, int height);

class Session {
    struct Scene {
        std::map<std::string, GLuint> textures;
        GPPOBJ model;
        CScene::Definition definition;
        GPPOBJ::Inspection inspection;
        std::string source, kind, selection;
        explicit Scene();
        ~Scene();
    };

    std::unique_ptr<Scene> scene;
    Settings initial;
    Target captureTarget;
    GPPOBJ::Visibility visible, highlight;
    size_t visibilityRevision = 0;
    int selected = -1;
    Bounds visibleBounds;
    void updateVisibleBounds();
    void drawModel(bool helpers);
    void drawHelpers();

  public:
    Context &context;
    Settings settings;
    Target viewport;
    fs::path assetRoot, outputRoot;
    double lastFrameMs = 0;
    explicit Session(Context &ctx, fs::path assets, fs::path output);
    ~Session();
    void load(const std::string &kind, const std::string &value);
    void reload();
    void apply(const Settings &value);
    void reset();
    void frame(bool selection = false, int w = 0, int h = 0);
    void resetCamera();
    void setView(const std::string &name);
    void select(int part);
    int selection() const { return selected; }
    bool isVisible(size_t part) const {
        return part < visible.size() && visible[part];
    }
    void visibility(const std::string &action, int part = -1);
    GPPOBJ::Hit pick(float x, float y, int w, int h) const;
    void pickSelection(float x, float y, int w, int h, bool pivot);
    void fly(float x, float y, const Vec &local, float delta, bool fast);
    void navigate(float orbitX, float orbitY, float panX, float panY,
                  float zoom, float mouseX = -1, float mouseY = -1, int w = 0,
                  int h = 0);
    void render(Target &target, int w, int h, bool helpers = true);
    std::string capture(const fs::path &name, int w, int h,
                        const fs::path &metadata = {}, bool helpers = false);
    std::string benchmark(int frames, int warmup, int w, int h,
                          double timeout = 30);
    std::string state() const;
    const GPPOBJ::Inspection &inspection() const;
    fs::path artifact(const fs::path &name) const;
    std::string sceneLabel() const;
};

class UI {
    bool initialized = false;
    std::string ini, error;
    GLFWwindow *window = nullptr;
    bool orbiting = false, panning = false, flying = false, dragged = false;
    double lastMouseX = 0, lastMouseY = 0;
    void releaseMouse();

  public:
    explicit UI(Session &session);
    ~UI();
    void draw(Session &session);
};
} // namespace Studio
