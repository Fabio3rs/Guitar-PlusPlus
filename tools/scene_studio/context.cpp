#include "studio.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <png.h>
#ifdef GPP_STUDIO_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

namespace Studio {
namespace {
std::string glString(GLenum name) {
    const auto *value = glGetString(name);
    if (!value) {
        throw Error("context", "No current desktop OpenGL context");
    }
    return reinterpret_cast<const char *>(value);
}
} // namespace

Context::Context(const std::string &backend, bool gui, bool requireHardware) {
    try {
        caps.backend = backend;
        if (backend == "glfw") {
            glfwSetErrorCallback([](int code, const char *message) {
                std::fprintf(stderr, "GLFW %d: %s\n", code, message);
            });
            if (!glfwInit()) {
                throw Error("context", "GLFW initialization failed; headless "
                                       "GLFW requires Xvfb in this session");
            }
            glfwStarted = true;
            glfwWindowHint(GLFW_VISIBLE, gui ? GLFW_TRUE : GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
            window = glfwCreateWindow(1280, 800, "Guitar++ Scene Studio",
                                      nullptr, nullptr);
            if (!window) {
                throw Error("context", "Cannot create GLFW window");
            }
            glfwMakeContextCurrent(window);
            glfwSwapInterval(gui ? 1 : 0);
        } else if (backend == "egl") {
            if (gui) {
                throw Error("arguments", "EGL backend is headless only");
            }
#ifdef GPP_STUDIO_EGL
            const auto platform =
                reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
                    eglGetProcAddress("eglGetPlatformDisplayEXT"));
            if (!platform) {
                throw Error("context", "EGL platform display API unavailable");
            }
            eglDisplay = platform(EGL_PLATFORM_SURFACELESS_MESA,
                                  EGL_DEFAULT_DISPLAY, nullptr);
            EGLint major = 0, minor = 0;
            if (!eglDisplay || !eglInitialize(eglDisplay, &major, &minor) ||
                !eglBindAPI(EGL_OPENGL_API)) {
                throw Error("context",
                            "Cannot initialize surfaceless EGL desktop OpenGL");
            }
            const EGLint attributes[] = {EGL_SURFACE_TYPE,
                                         EGL_PBUFFER_BIT,
                                         EGL_RENDERABLE_TYPE,
                                         EGL_OPENGL_BIT,
                                         EGL_RED_SIZE,
                                         8,
                                         EGL_GREEN_SIZE,
                                         8,
                                         EGL_BLUE_SIZE,
                                         8,
                                         EGL_NONE};
            EGLConfig config{};
            EGLint count = 0;
            if (!eglChooseConfig(eglDisplay, attributes, &config, 1, &count) ||
                !count) {
                throw Error("context", "No EGL desktop OpenGL configuration");
            }
            const EGLint contextAttributes[] = {
                EGL_CONTEXT_MAJOR_VERSION_KHR, 2, EGL_CONTEXT_MINOR_VERSION_KHR,
                0, EGL_NONE};
            eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT,
                                          contextAttributes);
            if (!eglContext || !eglMakeCurrent(eglDisplay, EGL_NO_SURFACE,
                                               EGL_NO_SURFACE, eglContext)) {
                throw Error(
                    "context",
                    "EGL surfaceless compatibility context unavailable");
            }
#else
            throw Error("unsupported", "This build has no EGL backend (install "
                                       "EGL development headers/libraries)");
#endif
        } else {
            throw Error("arguments", "Backend must be glfw or egl");
        }
        const auto glewResult = glewInit();
        if (glewResult != GLEW_OK &&
            !(backend == "egl" && glewResult == GLEW_ERROR_NO_GLX_DISPLAY)) {
            throw Error("context", reinterpret_cast<const char *>(
                                       glewGetErrorString(glewResult)));
        }
        if (!GLEW_VERSION_2_0 || !glCreateShader || !glCreateProgram ||
            !glGenBuffers) {
            throw Error("unsupported",
                        "OpenGL 2.0 shader/VBO entry points unavailable");
        }
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        caps.vendor = glString(GL_VENDOR);
        caps.renderer = glString(GL_RENDERER);
        caps.version = glString(GL_VERSION);
        caps.fbo = GLEW_EXT_framebuffer_object && glGenFramebuffersEXT &&
                   glBindFramebufferEXT;
        caps.npot = GLEW_ARB_texture_non_power_of_two;
        caps.depthTexture = GLEW_ARB_depth_texture;
        if (!caps.fbo || !caps.npot) {
            throw Error("unsupported",
                        "Scene Studio requires EXT_framebuffer_object and "
                        "non-power-of-two textures");
        }
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &caps.maxTexture);
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE_EXT, &caps.maxRenderbuffer);
        std::string renderer = caps.renderer;
        std::transform(renderer.begin(), renderer.end(), renderer.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        caps.acceleration = "unknown";
        if (renderer.find("llvmpipe") != std::string::npos ||
            renderer.find("softpipe") != std::string::npos ||
            renderer.find("swrast") != std::string::npos ||
            renderer.find("software") != std::string::npos ||
            renderer.find("swiftshader") != std::string::npos ||
            renderer.find("gdi generic") != std::string::npos) {
            caps.acceleration = "software";
        } else if (renderer.find("radeon") != std::string::npos ||
                   renderer.find("nvidia") != std::string::npos ||
                   renderer.find("intel") != std::string::npos ||
                   renderer.find("apple m") != std::string::npos) {
            caps.acceleration = "hardware";
        }
        if (requireHardware && caps.acceleration != "hardware") {
            throw Error("hardware_required",
                        "Hardware acceleration not identified: " +
                            caps.renderer);
        }
        std::fprintf(stderr, "Scene Studio: %s / %s / %s (%s, %s)\n",
                     caps.vendor.c_str(), caps.renderer.c_str(),
                     caps.version.c_str(), backend.c_str(),
                     caps.acceleration.c_str());
        // GLEW may leave an extension-enumeration error. Subsequent operations
        // are checked independently, after validating the loaded capabilities.
        while (glGetError() != GL_NO_ERROR) {
        }
        if (backend == "egl") {
            // Surfaceless EGL has no default framebuffer. Legacy attribute
            // restoration needs a valid color target even between scene draws.
            parking = std::make_unique<Target>();
            parking->resize(1, 1, caps);
            parking->bind();
            glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
            glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
        }
    } catch (...) {
        close();
        throw;
    }
}

Context::~Context() {
    close();
}

void Context::close() noexcept {
    parking.reset();
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    if (glfwStarted) {
        glfwTerminate();
        glfwStarted = false;
    }
#ifdef GPP_STUDIO_EGL
    if (eglDisplay) {
        eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        if (eglContext) {
            eglDestroyContext(eglDisplay, eglContext);
        }
        eglTerminate(eglDisplay);
    }
#endif
    eglDisplay = eglContext = nullptr;
}

Target::~Target() {
    clear();
}

void Target::clear() noexcept {
    if (framebuffer) {
        glDeleteFramebuffersEXT(1, &framebuffer);
    }
    if (depth) {
        glDeleteRenderbuffersEXT(1, &depth);
    }
    if (texture) {
        glDeleteTextures(1, &texture);
    }
    framebuffer = depth = texture = 0;
    width = height = 0;
}

void Target::resize(int w, int h, const Capabilities &caps) {
    const int maxSize = std::min({caps.maxTexture, caps.maxRenderbuffer, 8192});
    if (w <= 0 || h <= 0 || w > maxSize || h > maxSize ||
        uint64_t(w) * uint64_t(h) > 32 * 1024 * 1024) {
        throw Error("dimensions",
                    "Render dimensions exceed positive hardware/tool limits "
                    "(8192 per axis, 32M pixels)");
    }
    if (w == width && h == height) {
        return;
    }
    GLint oldFbo = 0, oldTexture = 0, oldDepth = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &oldFbo);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glGetIntegerv(GL_RENDERBUFFER_BINDING_EXT, &oldDepth);
    Target next;
    glGenTextures(1, &next.texture);
    glBindTexture(GL_TEXTURE_2D, next.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glGenRenderbuffersEXT(1, &next.depth);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, next.depth);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, w, h);
    glGenFramebuffersEXT(1, &next.framebuffer);
    next.bind();
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, next.texture, 0);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT, next.depth);
    const GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    const GLenum error = glGetError();
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, GLuint(oldFbo));
    glBindTexture(GL_TEXTURE_2D, GLuint(oldTexture));
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, GLuint(oldDepth));
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT || error != GL_NO_ERROR) {
        throw Error("framebuffer",
                    "Cannot allocate complete render target: status " +
                        std::to_string(status) + ", GL error " +
                        std::to_string(error));
    }
    std::swap(texture, next.texture);
    std::swap(depth, next.depth);
    std::swap(framebuffer, next.framebuffer);
    width = w;
    height = h;
}

void Target::bind() const {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
}

std::vector<unsigned char> readPixels(int width, int height) {
    GLint alignment = 0, buffer = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    if (GLEW_ARB_pixel_buffer_object) {
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &buffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    std::vector<unsigned char> pixels(size_t(width) * size_t(height) * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    if (GLEW_ARB_pixel_buffer_object) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, GLuint(buffer));
    }
    if (glGetError() != GL_NO_ERROR) {
        throw Error("capture", "glReadPixels failed");
    }
    // PNG rows are top-to-bottom.
    const size_t row = size_t(width) * 4;
    for (int y = 0; y < height / 2; ++y) {
        std::swap_ranges(pixels.begin() + size_t(y) * row,
                         pixels.begin() + size_t(y + 1) * row,
                         pixels.begin() + size_t(height - y - 1) * row);
    }
    return pixels;
}

void writePNG(const fs::path &file, int width, int height,
              const std::vector<unsigned char> &rgba) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = png_uint_32(width);
    image.height = png_uint_32(height);
    image.format = PNG_FORMAT_RGBA;
    if (!png_image_write_to_file(&image, file.string().c_str(), 0, rgba.data(),
                                 0, nullptr)) {
        const std::string message = image.message;
        png_image_free(&image);
        throw Error("capture", "PNG write failed: " + message);
    }
    png_image_free(&image);
}
} // namespace Studio
