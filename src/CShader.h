#pragma once
#ifndef GPP_GLSL_SHADERS_CLASS_CSHADER
#define GPP_GLSL_SHADERS_CLASS_CSHADER
#define GL_GLEXT_PROTOTYPES
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace ShaderProject {
enum shaderTypes { FRAGMENT, VERTEX };

struct GLvb {
    unsigned int VBO, VAO;
};

template <class T, const T nullValue, class Deleter> class unique_object {
    T object;
    std::function<Deleter> deleter;

    unique_object &operator=(const unique_object &obj) = delete;
    unique_object(const unique_object &obj) = delete;

  public:
    const T &get() const { return object; }
    const T &operator()() const { return object; }

    void deleteObject() noexcept {
        const T old = std::exchange(object, nullValue);
        if (old != nullValue && deleter)
            deleter(old);
    }

    unique_object &operator=(unique_object &&obj) noexcept {
        if (this == std::addressof(obj))
            return *this;

        deleteObject();
        object = obj.object;
        obj.object = nullValue;

        deleter = std::move(obj.deleter);

        return *this;
    }

    unique_object() : object(nullValue) {}

    unique_object(T &&value, Deleter delFun) : unique_object() {
        object = std::move(value);
        deleter = delFun;
    }

    unique_object(unique_object &&obj) noexcept : unique_object() {
        object = obj.object;
        obj.object = nullValue;
        deleter = std::move(obj.deleter);
    }

    ~unique_object() { deleteObject(); }
};

typedef unique_object<unsigned int, 0u, void(unsigned int)> ProgramObject;
typedef unique_object<unsigned int, 0u, void(unsigned int)> ShaderObject;
typedef unique_object<unsigned int, 0u, void(unsigned int)> VertexObject;
ProgramObject make_program();
ShaderObject make_shader(shaderTypes type);
VertexObject make_vertexObject();

class CShader {
    bool enableShaders;

    struct shaderInst {
        shaderTypes type;
        ShaderObject glID;
        std::string fileName;
        std::string shaderContent;

        void compile();
        shaderInst(const char *fName, shaderTypes shaderType);
    };

    struct shaderEvent {
        ProgramObject shaderProg;
        std::string eventName;
        bool linked = false;
        shaderEvent(const char *name);
    };

    std::deque<shaderEvent> events;
    std::deque<shaderInst> shadersList;

  public:
    struct EffectProgram {
        int event = -1;
        int time = -1;
        int opacity = -1;
        int spark = -1;
        int plusActive = -1;
        int multiplier = -1;
        int publicApproval = -1;
    };

  private:
    bool effectsPrepared = false;
    EffectProgram fireEffect, lightningEffect;

  public:
    // Run during loading with the owning GL context current. Idempotent.
    void prepareEffects();
    const EffectProgram &fireProgram() const { return fireEffect; }
    const EffectProgram &lightningProgram() const { return lightningEffect; }
    static CShader &inst();

    bool isShadersEnabled() const { return enableShaders; }

    bool isUsingProgram() const;

    // Call while the owning OpenGL context is current, before destroying it.
    static void shutdown();
    ~CShader();

    size_t eventsSize() const { return events.size(); }

    size_t shadersListSize() const { return shadersList.size(); }

    void processEvent(int id);
    int addEvent(const char *name);
    // Compile/link transactionally; no event is retained on failure.
    int addProgram(const char *name, std::string_view vertexSource,
                   std::string_view fragmentSource);
    int getEventUniform(int event, const char *name) const;
    void addShaderToEvent(const char *shaderEvent, int shaderID);
    int newShader(const char *shaderFile, shaderTypes type, int eventToLink);
    int newShader(const char *shaderFile, shaderTypes type, const char *name);
    void linkAllShaders();
    void deactivateShader();

    static void bindProgram(const ProgramObject &programID);
    static void bindProgram(unsigned int programID);
    static ProgramObject createShaderProgram();
    static ShaderObject compileShader(const char *shaderSource, int length,
                                      shaderTypes type);
    static ShaderObject compileShader(std::string_view source,
                                      shaderTypes type);
    static void attachShaderToProgram(const ProgramObject &programID,
                                      const ShaderObject &shaderID);
    static void linkShaderProgram(const ProgramObject &programID);

    static int getProgramUniform(const ProgramObject &program,
                                 const char *string);
    static void setFloatUniform(int uniform, float val);

    static void vertexAttribPointerFloat(unsigned int index, int elements,
                                         bool normalized, int stride,
                                         const void *pointer);
    static void vertexAttribPointerDouble(unsigned int index, int elements,
                                          bool normalized, int stride,
                                          const void *pointer);
    static void bindVertexArray(const VertexObject &vertexArray);
    static void bindVertexArray(unsigned int vertexArray);

  private:
    CShader();
    CShader(const CShader &) = delete;
};
} // namespace ShaderProject

#endif
