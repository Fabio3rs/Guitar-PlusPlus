#define __CSHADER_CPP

#include "CShader.h"
#include "FireShader.h"
#include "PlusLightningShader.h"
#include <GL/glew.h>
#include <iterator>
#include <limits>
// #include <Windows.h>
#include "CLog.h"
#include "CLuaFunctions.hpp"

namespace ShaderProject {
namespace {
CShader *liveInstance = nullptr;

void requireShaderAPI() {
    if (!glGetString(GL_VERSION) || !glCreateShader || !glCreateProgram)
        throw std::runtime_error(
            "Shaders require a current OpenGL context and initialized GLEW");
}

template <class Container> auto &checked(Container &items, int id) {
    if (id < 0 || static_cast<size_t>(id) >= items.size())
        throw std::out_of_range("Invalid shader or event ID: " +
                                std::to_string(id));
    return items[static_cast<size_t>(id)];
}

std::string infoLog(GLuint id, bool program) {
    GLint size = 0;
    if (program)
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &size);
    else
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &size);
    if (size <= 0)
        return "No driver log available";
    std::string log(static_cast<size_t>(size), '\0');
    GLsizei written = 0;
    if (program)
        glGetProgramInfoLog(id, size, &written, log.data());
    else
        glGetShaderInfoLog(id, size, &written, log.data());
    log.resize(static_cast<size_t>(written));
    return log;
}
} // namespace

auto CShader::inst() -> CShader & {
    static CShader shaders;
    return shaders;
}

CShader::shaderEvent::shaderEvent(const char *name)
    : shaderProg(make_program()), eventName(name) {}

CShader::shaderInst::shaderInst(const char *fName, shaderTypes shaderType)
    : type(shaderType) {
    if (!fName)
        throw std::invalid_argument("Null shader filename");
    fileName = fName;
    std::ifstream file(fileName, std::ios::binary);
    if (!file)
        throw std::runtime_error("Can't open shader: " + fileName);
    shaderContent.assign(std::istreambuf_iterator<char>(file), {});
    if (file.bad())
        throw std::runtime_error("Can't read shader: " + fileName);
    try {
        compile();
    } catch (const std::exception &error) {
        throw std::runtime_error(fileName + ": " + error.what());
    }
}

void CShader::shaderInst::compile() {
    glID = CShader::compileShader(shaderContent, type);
}

auto CShader::addEvent(const char *name) -> int {
    if (!inst().enableShaders) {
        {
            return -1;
        }
    }

    if (!name)
        throw std::invalid_argument("Null shader event name");
    events.emplace_back(name);
    return static_cast<int>(events.size() - 1);
}

int CShader::addProgram(const char *name, std::string_view vertexSource,
                        std::string_view fragmentSource) {
    if (!enableShaders)
        return -1;
    if (!name)
        throw std::invalid_argument("Null shader event name");
    auto vertex = compileShader(vertexSource, VERTEX);
    auto fragment = compileShader(fragmentSource, FRAGMENT);
    shaderEvent event(name);
    attachShaderToProgram(event.shaderProg, vertex);
    attachShaderToProgram(event.shaderProg, fragment);
    linkShaderProgram(event.shaderProg);
    event.linked = true;
    events.push_back(std::move(event));
    return static_cast<int>(events.size() - 1);
}

int CShader::getEventUniform(int event, const char *name) const {
    if (!name)
        throw std::invalid_argument("Null uniform name");
    return getProgramUniform(checked(events, event).shaderProg, name);
}

void CShader::addShaderToEvent(const char *shaderEvent, int shaderID) {
    if (!inst().enableShaders) {
        {
            return;
        }
    }

    if (!shaderEvent)
        throw std::invalid_argument("Null shader event name");
    int ID = -1;
    for (size_t i = 0, size = events.size(); i < size; i++) {
        if (events[i].eventName == shaderEvent) {
            ID = static_cast<int>(i);
            break;
        }
    }

    if (ID != -1) {
        attachShaderToProgram(checked(events, ID).shaderProg,
                              checked(shadersList, shaderID).glID);
    } else {
        throw std::invalid_argument("Unknown shader event");
    }
}

void CShader::processEvent(int id) {
    if (!enableShaders) {
        {
            return;
        }
    }

    const auto &event = checked(events, id);
    if (!event.linked)
        throw std::runtime_error("Shader event is not linked");
    // Owned programs were validated at link time; no driver queries per frame.
    glUseProgram(event.shaderProg.get());
}

auto CShader::newShader(const char *shaderFile, shaderTypes type,
                        int eventToLink) -> int {
    if (!inst().enableShaders) {
        {
            return -1;
        }
    }

    auto &event = checked(events, eventToLink);
    shadersList.emplace_back(shaderFile, type);
    attachShaderToProgram(event.shaderProg, shadersList.back().glID);
    return static_cast<int>(shadersList.size() - 1);
}

auto CShader::newShader(const char *shaderFile, shaderTypes type,
                        const char *name) -> int {
    if (!inst().enableShaders) {
        {
            return -1;
        }
    }

    if (!name)
        throw std::invalid_argument("Null shader event name");
    int ID = -1;
    for (size_t i = 0, size = events.size(); i < size; i++) {
        if (events[i].eventName == name) {
            ID = static_cast<int>(i);
            break;
        }
    }

    if (ID != -1) {
        return newShader(shaderFile, type, ID);
    }
    return -1;
}

void CShader::deactivateShader() { bindProgram(0); }

void CShader::bindProgram(const ProgramObject &programID) {
    bindProgram(programID.get());
}

bool CShader::isUsingProgram() const {
    if (!glGetString(GL_VERSION))
        return false;
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    return program != 0;
}

void CShader::bindProgram(unsigned int programID) {
    requireShaderAPI();
    if (programID != 0) {
        GLint linked = GL_FALSE;
        if (!glIsProgram(programID))
            throw std::invalid_argument("Invalid shader program");
        glGetProgramiv(programID, GL_LINK_STATUS, &linked);
        if (!linked)
            throw std::runtime_error("Shader program is not linked");
    }
    glUseProgram(programID);
}

auto CShader::createShaderProgram() -> ProgramObject { return make_program(); }

auto CShader::compileShader(const char *shaderSource, int length,
                            shaderTypes type) -> ShaderObject {
    if (!shaderSource || length < 0)
        throw std::invalid_argument("Invalid shader source or length");
    return compileShader(
        std::string_view(shaderSource, static_cast<size_t>(length)), type);
}

ShaderObject CShader::compileShader(std::string_view source, shaderTypes type) {
    if (source.empty())
        throw std::invalid_argument("Empty shader source");
    if (source.size() > static_cast<size_t>(std::numeric_limits<GLint>::max()))
        throw std::length_error("Shader source is too large");
    auto shader = make_shader(type);
    const GLchar *data = source.data();
    const GLint length = static_cast<GLint>(source.size());
    glShaderSource(shader.get(), 1, &data, &length);
    glCompileShader(shader.get());
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &compiled);
    if (!compiled)
        throw std::runtime_error("Shader compilation failed: " +
                                 infoLog(shader.get(), false));
    return shader;
}

void CShader::attachShaderToProgram(const ProgramObject &programID,
                                    const ShaderObject &shaderID) {
    requireShaderAPI();
    if (!glIsProgram(programID.get()) || !glIsShader(shaderID.get()))
        throw std::invalid_argument("Invalid program or shader for attachment");
    glAttachShader(programID.get(), shaderID.get());
}

void CShader::linkShaderProgram(const ProgramObject &programID) {
    requireShaderAPI();
    if (!glIsProgram(programID.get()))
        throw std::invalid_argument("Invalid shader program");
    glLinkProgram(programID.get());
    GLint linked = GL_FALSE;
    glGetProgramiv(programID.get(), GL_LINK_STATUS, &linked);
    if (!linked)
        throw std::runtime_error("Shader link failed: " +
                                 infoLog(programID.get(), true));
}

auto CShader::getProgramUniform(const ProgramObject &program,
                                const char *string) -> int {
    return glGetUniformLocation(program.get(), string);
}

void CShader::setFloatUniform(int uniform, float val) {
    glUniform1f(uniform, val);
}

void CShader::vertexAttribPointerFloat(unsigned int index, int elements,
                                       bool normalized, int stride,
                                       const void *pointer) {
    glVertexAttribPointer(index, elements, GL_FLOAT,
                          static_cast<GLboolean>(normalized), stride, pointer);
}

void CShader::vertexAttribPointerDouble(unsigned int index, int elements,
                                        bool normalized, int stride,
                                        const void *pointer) {
    glVertexAttribPointer(index, elements, GL_DOUBLE,
                          static_cast<GLboolean>(normalized), stride, pointer);
}

void CShader::bindVertexArray(const VertexObject &vertexArray) {
    bindVertexArray(vertexArray.get());
}

void CShader::bindVertexArray(unsigned int vertexArray) {
    if (!glBindVertexArray)
        throw std::runtime_error("Vertex arrays are unavailable");
    glBindVertexArray(vertexArray);
}

void CShader::linkAllShaders() {
    if (!inst().enableShaders) {
        {
            return;
        }
    }

    for (auto &event : events) {
        event.linked = false;
        linkShaderProgram(event.shaderProg);
        event.linked = true;
    }
}

void CShader::prepareEffects() {
    if (effectsPrepared)
        return;
    requireShaderAPI();
    effectsPrepared = true;
    const auto prepare = [this](const char *name, std::string_view vertex,
                                std::string_view fragment,
                                EffectProgram &effect) {
        try {
            effect.event = addProgram(name, vertex, fragment);
            if (effect.event < 0)
                return;
            effect.time = getEventUniform(effect.event, "uTime");
            effect.opacity = getEventUniform(effect.event, "uOpacity");
            effect.spark = getEventUniform(effect.event, "uSpark");
        } catch (const std::exception &error) {
            effect = {};
            CLog::log() << std::string(name) + ": " + error.what();
        }
    };
    prepare("particle fire", FireShader::vertex, FireShader::fragment,
            fireEffect);
    prepare("PLUS lightning", PlusLightningShader::vertex,
            PlusLightningShader::fragment, lightningEffect);
}

CShader::CShader() : enableShaders(true) {
    CLuaFunctions::GameVariables::gv().pushVar("enableShaders", enableShaders);
    liveInstance = this;
}

CShader::~CShader() {
    shutdown();
    liveInstance = nullptr;
}

void CShader::shutdown() {
    if (!liveInstance)
        return;
    if (!liveInstance->events.empty())
        glUseProgram(0);
    liveInstance->events.clear();
    liveInstance->shadersList.clear();
    liveInstance->fireEffect = {};
    liveInstance->lightningEffect = {};
    liveInstance->effectsPrepared = false;
}

auto make_program() -> ProgramObject {
    requireShaderAPI();
    auto id = glCreateProgram();
    if (!id)
        throw std::runtime_error("Can't create shader program");
    return ProgramObject(std::move(id), [](unsigned int programID) {
        if (programID != 0u) {
            glDeleteProgram(programID);
        }
    });
}

auto make_shader(shaderTypes type) -> ShaderObject {
    requireShaderAPI();
    if (type != VERTEX && type != FRAGMENT)
        throw std::invalid_argument("Unsupported shader type");
    auto id =
        glCreateShader(type == VERTEX ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
    if (!id)
        throw std::runtime_error("Can't create shader");
    return ShaderObject(std::move(id), [](unsigned int shaderID) {
        if (shaderID != 0u) {
            glDeleteShader(shaderID);
        }
    });
}

auto make_vertexObject() -> VertexObject {
    requireShaderAPI();
    if (!glGenVertexArrays || !glDeleteVertexArrays)
        throw std::runtime_error("Vertex arrays are unavailable");
    auto genVertexArrays = []() {
        unsigned int result = 0;
        glGenVertexArrays(1, &result);
        return result;
    };
    return VertexObject(genVertexArrays(), [](unsigned int vertexArrayId) {
        if (vertexArrayId != 0u) {
            glDeleteVertexArrays(1, &vertexArrayId);
        }
    });
}

} // namespace ShaderProject
