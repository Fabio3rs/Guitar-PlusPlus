#pragma once
#include <cstddef>
#ifndef GUITARPP_CENGINE_H_
#define GUITARPP_CENGINE_H_
// brmodstudio.forumeiros.com

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <dirent.h>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/simd/matrix.h>

class CFiledtor {
  public:
    void operator()(FILE *p) const noexcept { fclose(p); }
};

class direntclose {
  public:
    void operator()(DIR *p) const noexcept { closedir(p); }
};

typedef std::unique_ptr<FILE, CFiledtor> cfile_ptr;
typedef std::unique_ptr<DIR, direntclose> udirent_t;

#ifndef GLFW_KEY_MENU
/* The unknown key */
#define GLFW_KEY_UNKNOWN -1

/* Printable keys */
#define GLFW_KEY_SPACE 32
#define GLFW_KEY_APOSTROPHE 39 /* ' */
#define GLFW_KEY_COMMA 44      /* , */
#define GLFW_KEY_MINUS 45      /* - */
#define GLFW_KEY_PERIOD 46     /* . */
#define GLFW_KEY_SLASH 47      /* / */
#define GLFW_KEY_0 48
#define GLFW_KEY_1 49
#define GLFW_KEY_2 50
#define GLFW_KEY_3 51
#define GLFW_KEY_4 52
#define GLFW_KEY_5 53
#define GLFW_KEY_6 54
#define GLFW_KEY_7 55
#define GLFW_KEY_8 56
#define GLFW_KEY_9 57
#define GLFW_KEY_SEMICOLON 59 /* ; */
#define GLFW_KEY_EQUAL 61     /* = */
#define GLFW_KEY_A 65
#define GLFW_KEY_B 66
#define GLFW_KEY_C 67
#define GLFW_KEY_D 68
#define GLFW_KEY_E 69
#define GLFW_KEY_F 70
#define GLFW_KEY_G 71
#define GLFW_KEY_H 72
#define GLFW_KEY_I 73
#define GLFW_KEY_J 74
#define GLFW_KEY_K 75
#define GLFW_KEY_L 76
#define GLFW_KEY_M 77
#define GLFW_KEY_N 78
#define GLFW_KEY_O 79
#define GLFW_KEY_P 80
#define GLFW_KEY_Q 81
#define GLFW_KEY_R 82
#define GLFW_KEY_S 83
#define GLFW_KEY_T 84
#define GLFW_KEY_U 85
#define GLFW_KEY_V 86
#define GLFW_KEY_W 87
#define GLFW_KEY_X 88
#define GLFW_KEY_Y 89
#define GLFW_KEY_Z 90
#define GLFW_KEY_LEFT_BRACKET 91  /* [ */
#define GLFW_KEY_BACKSLASH 92     /* \ */
#define GLFW_KEY_RIGHT_BRACKET 93 /* ] */
#define GLFW_KEY_GRAVE_ACCENT 96  /* ` */
#define GLFW_KEY_WORLD_1 161      /* non-US #1 */
#define GLFW_KEY_WORLD_2 162      /* non-US #2 */

/* Function keys */
#define GLFW_KEY_ESCAPE 256
#define GLFW_KEY_ENTER 257
#define GLFW_KEY_TAB 258
#define GLFW_KEY_BACKSPACE 259
#define GLFW_KEY_INSERT 260
#define GLFW_KEY_DELETE 261
#define GLFW_KEY_RIGHT 262
#define GLFW_KEY_LEFT 263
#define GLFW_KEY_DOWN 264
#define GLFW_KEY_UP 265
#define GLFW_KEY_PAGE_UP 266
#define GLFW_KEY_PAGE_DOWN 267
#define GLFW_KEY_HOME 268
#define GLFW_KEY_END 269
#define GLFW_KEY_CAPS_LOCK 280
#define GLFW_KEY_SCROLL_LOCK 281
#define GLFW_KEY_NUM_LOCK 282
#define GLFW_KEY_PRINT_SCREEN 283
#define GLFW_KEY_PAUSE 284
#define GLFW_KEY_F1 290
#define GLFW_KEY_F2 291
#define GLFW_KEY_F3 292
#define GLFW_KEY_F4 293
#define GLFW_KEY_F5 294
#define GLFW_KEY_F6 295
#define GLFW_KEY_F7 296
#define GLFW_KEY_F8 297
#define GLFW_KEY_F9 298
#define GLFW_KEY_F10 299
#define GLFW_KEY_F11 300
#define GLFW_KEY_F12 301
#define GLFW_KEY_F13 302
#define GLFW_KEY_F14 303
#define GLFW_KEY_F15 304
#define GLFW_KEY_F16 305
#define GLFW_KEY_F17 306
#define GLFW_KEY_F18 307
#define GLFW_KEY_F19 308
#define GLFW_KEY_F20 309
#define GLFW_KEY_F21 310
#define GLFW_KEY_F22 311
#define GLFW_KEY_F23 312
#define GLFW_KEY_F24 313
#define GLFW_KEY_F25 314
#define GLFW_KEY_KP_0 320
#define GLFW_KEY_KP_1 321
#define GLFW_KEY_KP_2 322
#define GLFW_KEY_KP_3 323
#define GLFW_KEY_KP_4 324
#define GLFW_KEY_KP_5 325
#define GLFW_KEY_KP_6 326
#define GLFW_KEY_KP_7 327
#define GLFW_KEY_KP_8 328
#define GLFW_KEY_KP_9 329
#define GLFW_KEY_KP_DECIMAL 330
#define GLFW_KEY_KP_DIVIDE 331
#define GLFW_KEY_KP_MULTIPLY 332
#define GLFW_KEY_KP_SUBTRACT 333
#define GLFW_KEY_KP_ADD 334
#define GLFW_KEY_KP_ENTER 335
#define GLFW_KEY_KP_EQUAL 336
#define GLFW_KEY_LEFT_SHIFT 340
#define GLFW_KEY_LEFT_CONTROL 341
#define GLFW_KEY_LEFT_ALT 342
#define GLFW_KEY_LEFT_SUPER 343
#define GLFW_KEY_RIGHT_SHIFT 344
#define GLFW_KEY_RIGHT_CONTROL 345
#define GLFW_KEY_RIGHT_ALT 346
#define GLFW_KEY_RIGHT_SUPER 347
#define GLFW_KEY_MENU 348
#define GLFW_KEY_LAST GLFW_KEY_MENU
#endif

enum windowsNum { closed, opened, preUpdate, posUpdate };

template <class T> class callOnDctor {
    std::function<T> f;

  public:
    inline callOnDctor(std::function<T> fun) : f(fun) {}

    callOnDctor() = delete;

    inline ~callOnDctor() {
        if (f)
            f();
    }
};

struct lightData {
    float ambientLight[4], diffuseLight[4], specularLight[4], position[4],
        direction[4], angle;
};
/*
struct gppVec3f
{
        float x, y, z;
};

struct gppVec2f
{
        float x, y;
};*/

typedef glm::vec<3, float> gppVec3f;
typedef glm::vec<2, float> gppVec2f;

typedef void (*customwcallback)(int w, int h, windowsNum state);

class staticCallFunc {

  public:
    template <class T, class... U> staticCallFunc(T fun, U &&...u) {
        fun(std::forward<U>(u)...);
    }
};

class CEngine {
    void *window;
    customwcallback wcallfunc;
    bool openWindowCalled;

    std::array<double, 16> projMatrix{};

  public:
    static cfile_ptr make_cfile(const char *name, const char *mode) {
        return cfile_ptr(fopen(name, mode));
    }

    static udirent_t make_dirent(const char *dir) {
        return std::unique_ptr<DIR, direntclose>(opendir(dir));
    }

    inline const double *getProjMatrix() const { return projMatrix.data(); }

    void setWindowCallbackFunction(customwcallback f) { wcallfunc = f; }

    customwcallback getWindowCallbackFunction() const { return wcallfunc; }

    struct GLFWimageC {
        int Width, Height;
        int Format;
        int BytesPerPixel;
        std::unique_ptr<unsigned char[]> Data;
        std::unique_ptr<unsigned char[]> tmpData;
        bool bRevPixels;
        bool keepData;

        GLFWimageC() {
            keepData = false;
            bRevPixels = false;
        }
    };

    typedef GLFWimageC GLFWimage;

    typedef struct {
        FILE *file;
        std::unique_ptr<unsigned char[]> data;
        long position;
        long size;
    } GLFWstream;

    int readImage(const char *name, GLFWimage *img, int flags);
    static int openFileStream(GLFWstream *stream, const char *name,
                              const char *mode);
    static long readStream(GLFWstream *stream, void *data, long size);
    static void closeStream(GLFWstream *stream);
    static long tellStream(GLFWstream *stream);
    static int seekStream(GLFWstream *stream, long offset, int whence);
    int loadTexture2D(const char *name, int flags, GLFWimage *eimg = nullptr,
                      bool glUpload = true);
    int loadTextureImage2D(GLFWimage *img, int flags, bool glUpload = true);
    auto loadTextureImage2DFinish(GLFWimage *img, int flags) const -> int;
    static void freeImage(GLFWimage *img);

    static unsigned int genNewGLTexture();
    void uploadBytesToGl(unsigned int text, const char *bytes, int width,
                         int height);

    int getKey(int key) const;
    static void Rotate(double a, double x, double y, double z);
    void matrixReset();
    static void pushMatrix();
    static void popMatrix();
    static void multiplyMatrix(float *matrix);

    int AASamples;

    struct RenderStruct {
        float x1, x2, x3, x4, y1, y2, y3, y4, z1, z2, z3, z4, TextureX1,
            TextureY1, TextureX2, TextureY2, alphaTop, alphaBottom;
        unsigned int Text;
    };

    struct RenderDoubleStruct {
        double x1, x2, x3, x4, y1, y2, y3, y4, z1, z2, z3, z4, TextureX1,
            TextureY1, TextureX2, TextureY2, alphaTop, alphaBottom;
        unsigned int Text;
    };

    double windowHeight{}, windowWidth{};
    int glMajor, glMinor;

    double mouseX, mouseY;

    std::function<void(int, const std::string &e)> errorCallbackFun;

  private:
    static void GLFWerrorfun(int error, const char *description);

    double lastFrameTime{}, lastFPSSwapTime{}, DeltaTime;
    int FPS, tmpFPS;
    unsigned int lastUsedTexture, lastUsedVBOBuffer{};

    unsigned int cursorText{};

    std::string keyboardNoise;
    // const static uint32_t bitValues[32];

    std::bitset<0x18000> glStates;

    struct audioInfo {
        const char *fileName;
        int state;
        void *handle;

        audioInfo() {
            state = 0;
            handle = nullptr;
        }
    };

    double lastUpdatedNoise, updateNoiseInterval;

    // TODO: make once allocation
    class CCircleStream {
        unsigned int GlBuffer;
        std::unique_ptr<double[]> verticesBuffer;
        double x, y, percent, radius, lineWeight;
        size_t polysNum;

      public:
        friend CEngine;

        bool isChanged(double x, double y, double percent, double radius,
                       double lineWeight, size_t polysNum) {
            if (percent != this->percent || x != this->x || y != this->y ||
                radius != this->radius || lineWeight != this->lineWeight ||
                polysNum != this->polysNum) {
                this->x = x;
                this->y = y;
                this->percent = percent;
                this->radius = radius;
                this->lineWeight = lineWeight;

                if (polysNum > this->polysNum) {
                    verticesBuffer = std::make_unique<double[]>(polysNum * 12);
                    this->polysNum = polysNum;
                }
                return true;
            }
            return false;
        }

        bool isChangedMX(double x, double y, double radius, double lineWeight,
                         size_t polysNum) {
            if (x != this->x || y != this->y || radius != this->radius ||
                lineWeight != this->lineWeight || polysNum != this->polysNum) {
                this->x = x;
                this->y = y;
                this->radius = radius;
                this->lineWeight = lineWeight;

                if (polysNum > this->polysNum) {
                    verticesBuffer = std::make_unique<double[]>(polysNum * 12);
                    this->polysNum = polysNum;
                }
                return true;
            }
            return false;
        }

        void allocAll(size_t polnum) {
            verticesBuffer = std::make_unique<double[]>(polnum * 12);
            this->polysNum = polnum;
        }

        CCircleStream(const CCircleStream &) = delete;

        inline CCircleStream() {
            GlBuffer = ~0u;
            x = y = percent = radius = lineWeight = 0;
            polysNum = 0;
        }
    };

    std::map<int, CCircleStream> circlesBuffer;
    //*****************************************

    double lrC{}, lgC{}, lbC{}, laC{};

    std::array<double, 3> lastRenderAt{};

    glm::mat<4, 4, double> lookAtMatrix{};

  public:
    static std::mutex m_gl_mutex;

    double volumeMaster;

    const glm::mat<4, 4, double> &getLookAtMatrix() const {
        return lookAtMatrix;
    }

    struct cameraSET {
        glm::vec<3, double> eye;
        glm::vec<3, double> center;
        glm::vec<3, double> up;

        bool operator==(const cameraSET &c) const {
            return (eye == c.eye && center == c.center && up == c.up);
        }

        bool operator!=(const cameraSET &c) const { return !(*this == c); }

        cameraSET(double eyex, double eyey, double eyez, double centerx,
                  double centery, double centerz, double upx, double upy,
                  double upz) {

            eye.x = eyex;
            eye.y = eyey;
            eye.z = eyez;

            center.x = centerx;
            center.y = centery;
            center.z = centerz;

            up.x = upx;
            up.y = upy;
            up.z = upz;
        }

        cameraSET() {
            up.x = 0.0;
            up.y = 0.0;
            up.z = 0.0;

            center.x = 0.0;
            center.y = 0.0;
            center.z = 0.0;

            eye.x = 0.0;
            eye.y = 0.0;
            eye.z = 0.0;

            eye = center = up;
        }
    };

    cameraSET nowCamera;

    struct staticDrawBuffer {
        void *pointer;
        unsigned int vertexL;
        unsigned int uvL;
        unsigned int normalsL;
        unsigned int texture;
        unsigned int bufferID;
        unsigned int sizebytes;
        int count;

        inline staticDrawBuffer() {
            pointer = nullptr;
            count = 0;
            vertexL = uvL = normalsL = texture = 0u;
            bufferID = ~0u;
            sizebytes = 0u;
        }

        void destroy(bool deletePtr = false);
        ~staticDrawBuffer();
    };

    struct dTriangleWithAlpha {
        bool useColors;
        bool autoEnDisaColors;
        unsigned int texture;
        template <class T> struct vType {
            T t;

            vType(T &tdata) { memcpy(t, tdata, sizeof(tdata)); }
        };

        std::vector<vType<double[18]>> vArray;
        std::vector<vType<double[12]>> tArray;
        std::vector<vType<double[24]>> aArray;

        inline void clear() {
            vArray.clear();
            tArray.clear();
            aArray.clear();
        }

        inline dTriangleWithAlpha() { autoEnDisaColors = true; }
    };

    std::function<void(void)> renderFrameCallback;

    static void pushQuad(dTriangleWithAlpha &arr,
                         const RenderDoubleStruct &quad3DData);
    void drawTrianglesWithAlpha(dTriangleWithAlpha &tris);
    static void enableColorsPointer(bool state);

    const inline void *getWindow() const { return window; }

    struct Resolution {
        int width;
        int height;
        int redBits;
        int greenBits;
        int blueBits;
        int refreshRate;
    };

    static std::vector<Resolution> getPossibleVideoModes();

    void setCamera(const cameraSET &cam);
    void openWindow(const char *name, int w, int h, int fullScreen);
    bool windowOpened();
    void renderFrame();
    static void clearScreen();

    /**/
    unsigned int loadTexture(const char *textureFileName,
                             GLFWimage *eimg = nullptr);
    bool loadTextureAsync(const char *textureFileName,
                          GLFWimage *eimg = nullptr);
    unsigned int uploadTextureToOGL(GLFWimage *eimg);
    void bindTexture(unsigned int text);

    static void glEnable(int num);
    static void glDisable(int num);

    void renderAt(double x, double y, double z);

    /* Rendering functions */
    void Render2DQuad(const RenderDoubleStruct &quad2DData);
    void Render3DQuad(const RenderDoubleStruct &quad3DData);
    void RenderCustomVerticesFloat(void *vertexPtr, void *uvPtr, void *normals,
                                   int count, unsigned int texture);
    void RenderCustomVerticesFloat(void *vertexPtr, void *uvPtr, void *normals,
                                   int count, unsigned int texture,
                                   unsigned int &vbuffer,
                                   unsigned int &uvbuffer,
                                   unsigned int &nvbuffer);
    void RenderCustomVerticesFloat(staticDrawBuffer &buffer,
                                   bool autoBindZero = true);
    void renderCustomConstVerticesFloat(const staticDrawBuffer &buffer,
                                        bool autoBindZero = true);
    void RenderMulti3DQuad(const std::deque<RenderDoubleStruct> &quad3DData,
                           unsigned int &bufferID);
    void Render3DQuadWithAlpha(const RenderDoubleStruct &quad3DData);
    void Render2DCircle(double x, double y, double percent, double radius,
                        double lineWeight, int polysNum, int maxPolys,
                        unsigned int &bufferID);
    void Render2DCircleBufferMax(double x, double y, double perone,
                                 double radius, double lineWeight, int maxPolys,
                                 unsigned int &bufferID);
    void draw2DLine(double *linedata, int points);
    void setColor(double r, double g, double b, double a);

    static void addToAccumulationBuffer(double d);
    static void retAccumulationBuffer(double d);
    static void clearAccmumaltionBuffer();

    static float getSoundBPM(unsigned int sound, double at, double interval);
    static double getChannelLength(unsigned int ch);

    struct chdata {
        std::array<float, 4> data;
    };

    /**/
    static float getMainVolume();
    static bool setMainVolume(float v);
    static bool loadSoundStream(const char *fileName, int &handle,
                                bool decode = false);
    static bool loadMusicStream(const char *fileName, int &handle);
    static bool playSoundStream(int handle);
    static bool pauseSoundStream(int handle);
    static bool unloadSoundStream(int &handle) noexcept;
    static void setSoundTime(int handle, double time);
    static double getSoundTime(int handle);
    static double getSoundVolume(int handle);
    auto setSoundVolume(int handle, float volume) const -> bool;
    static int setSoundFlags(int handle, int flags, int mask);
    static int setSoundAttribute(int handle, int attribute, float value);
    static chdata getChannelData(int handle);
    static chdata getChannelData(int handle, int b);

    static int getBassError();

    static void setScale(double x, double y, double z);

    static double getTime();
    inline double getDeltaTime() const { return DeltaTime; }
    int getMouseButton(int btn);
    inline int getFPS() const { return FPS; }

    static unsigned int vboSET(size_t size, void *buffer);
    static void attribVBOBuff(int id, int size, unsigned int buffer);
    static void disableBuf(int id);
    static void drawBufArrays(int size);
    void bindTextOnSlot(int text, int slot);
    static unsigned int getUniformLocation(unsigned int programID,
                                           const char *str);
    static void useShader(unsigned int programID);
    void bindVBOBuffer(unsigned int buffer);

    static void activateNormals(bool a);
    void activate3DRender(bool a);
    void activateAlphaTest(bool a);
    void activateStencilTest(bool a);
    void activateLighting(bool a);
    void activateLight(int id, bool a);
    static void setLight(const lightData &l, int id, bool setAmbient = true);

    static void clear3DBuffer();

    static void startShadowCapture();
    static void endShadowCapture();
    static void shadowMatrix(float shadowMat[4][4], const float groundplane[4],
                             const float lightpos[4]);
    static void findPlane(float plane[4], const float v0[3], const float v1[3],
                          const float v2[3]);

    static std::array<float, 4> planeEquation(const gppVec3f &p1,
                                              const gppVec3f &dir) {
        float a = dir.x, b = dir.y, c = dir.z;
        float d = -(a * p1.x) - (b * p1.y) - (c * p1.z);
        return std::array<float, 4>{a, b, c, d};
    }

    static void enablePolygonOffset();
    static void disablePolygonOffset();

    static void colorRGBToArray(int32_t rgb, double *arr);
    static void colorRGBToArrayf(int32_t rgb, float *arr);

    static void setClearColor(const std::array<double, 4> &color);

    static void setVSyncMode(int mode);

    ~CEngine();

    static CEngine &engine();

    CEngine(const CEngine &) = delete;
    void init();

  private:
    CEngine();
};

#endif
