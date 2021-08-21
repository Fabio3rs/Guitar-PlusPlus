#define __CSHADER_CPP

#include "CShader.h"
#include <GL/glew.h>
#include <iostream>
//#include <Windows.h>
#include "CLog.h"
#include "CLuaFunctions.hpp"

namespace ShaderProject {
auto CShader::inst() -> CShader & {
    static CShader shaders;
    return shaders;
}

CShader::shaderEvent::shaderEvent(const char *name)
    : shaderProg(make_program()) {
    if (!inst().enableShaders) {
        {
            return;
        }
    }

    eventName = name;
}

CShader::shaderInst::shaderInst(const char *fName, shaderTypes shaderType) {
    if (!inst().enableShaders) {
        {
            return;
        }
    }

    std::fstream shaderFile(fName);
    fileName = fName;

    if (!shaderFile.good()) {
        {
            std::logic_error("Can't load shader's file");
        }
    }

    shaderSize = fileSize(shaderFile);

    int allocSize = (shaderSize + 2) % 8;
    allocSize += shaderSize;

    shaderContent = std::make_unique<char[]>(allocSize);
    memset(shaderContent.get(), 0, allocSize);
    shaderFile.read(shaderContent.get(), shaderSize);
    //*(short*)&shaderContent[shaderSize] = 0;

    type = shaderType;

    compile();
}

void CShader::shaderInst::compile() {
    if (!inst().enableShaders) {
        {
            return;
        }
    }

    glID = CShader::compileShader(shaderContent.get(), shaderSize, type);
}

auto CShader::addEvent(const char *name) -> int {
    if (!inst().enableShaders) {
        {
            return -1;
        }
    }

    events.push_back(shaderEvent(name));
    return events.size() - 1;
}

void CShader::addShaderToEvent(const char *shaderEvent, int shaderID) {
    if (!inst().enableShaders) {
        {
            return;
        }
    }

    int ID = -1;
    for (int i = 0, size = events.size(); i < size; i++) {
        if (events[i].eventName == shaderEvent) {
            ID = i;
            break;
        }
    }

    if (ID != -1) {
        attachShaderToProgram(events[ID].shaderProg,
                              shadersList[shaderID].glID);
    }
    bindProgram(0);
    usingProgram = false;
}

void CShader::processEvent(int id) {
    if (!enableShaders) {
        {
            return;
        }
    }

    auto &shaderProg = events[id].shaderProg;

    bindProgram(shaderProg);

    if (shaderProg.get() != 0) {
        {
            usingProgram = true;
        }
    }
}

auto CShader::newShader(const char *shaderFile, shaderTypes type,
                        int eventToLink) -> int {
    if (!inst().enableShaders) {
        {
            return 0;
        }
    }

    shadersList.push_back(shaderInst(shaderFile, type));
    attachShaderToProgram(events[eventToLink].shaderProg,
                          shadersList.back().glID);
    usingProgram = false;
    bindProgram(0);
    return shadersList.size() - 1;
}

auto CShader::newShader(const char *shaderFile, shaderTypes type,
                        const char *name) -> int {
    if (!inst().enableShaders) {
        {
            return 0;
        }
    }

    int ID = -1;
    for (int i = 0, size = events.size(); i < size; i++) {
        if (events[i].eventName == name) {
            ID = i;
            break;
        }
    }

    if (ID != -1) {
        return newShader(shaderFile, type, ID);
    }
    return -1;
}

void CShader::deactivateShader() {
    if (!inst().enableShaders) {
        {
            return;
        }
    }

    usingProgram = false;
    bindProgram(0);
}

void CShader::bindProgram(const ProgramObject &programID) {
    bindProgram(programID.get());
}

void CShader::bindProgram(unsigned int programID) { glUseProgram(programID); }

auto CShader::createShaderProgram() -> ProgramObject { return make_program(); }

auto CShader::compileShader(const char *shaderSource, int length,
                            shaderTypes type) -> ShaderObject {
    const GLchar **ch = (const char **)&shaderSource;
    ShaderObject glID(make_shader(type));

    glShaderSource(glID.get(), 1, ch, 0);
    glCompileShader(glID.get());

    GLint compiled;
    glGetShaderiv(glID.get(), GL_COMPILE_STATUS, &compiled);
    if (compiled == 0) {
        char shaderErrorDetail[2048] = {0, 0};
        glGetShaderiv(glID.get(), GL_INFO_LOG_LENGTH, &length);
        glGetShaderInfoLog(glID.get(), sizeof(shaderErrorDetail) - 1, &length,
                           shaderErrorDetail);

        std::cout << shaderErrorDetail << std::endl;
    }

    return glID;
}

void CShader::attachShaderToProgram(const ProgramObject &programID,
                                    const ShaderObject &shaderID) {
    glAttachShader(programID.get(), shaderID.get());
}

void CShader::linkShaderProgram(const ProgramObject &programID) {
    glLinkProgram(programID.get());
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
    glBindVertexArray(vertexArray);
}

void CShader::linkAllShaders() {
    if (!inst().enableShaders) {
        {
            return;
        }
    }

    for (auto &event : events) {
        linkShaderProgram(event.shaderProg);
    }
}

CShader::CShader() {
    enableShaders = true;
    usingProgram = false;
    CLuaFunctions::GameVariables::gv().pushVar("enableShaders", enableShaders);

    GLenum err = glewInit();
    if (GLEW_OK != err) {
        /*Problem: glewInit failed, something is seriously wrong. */
        // MessageBoxA(0, (char*)glewGetErrorString(err), "Error: %s\n", 0);
    }
    // glUseProgram(0);
    /*
    GLint maxPatchVertices = 0;
    glGetIntegerv(GL_MAX_PATCH_VERTICES, &maxPatchVertices);

    glPatchParameteri(GL_PATCHES, maxPatchVertices);
    glPatchParameteri(GL_PATCH_VERTICES, maxPatchVertices);*/
}

auto make_program() -> ProgramObject {
    return ProgramObject(glCreateProgram(), [](unsigned int programID) {
        if (programID != 0u) {
            glUseProgram(0);
            glDeleteProgram(programID);
        }
    });
}

auto make_shader(shaderTypes type) -> ShaderObject {
    return ShaderObject(glCreateShader((type == shaderTypes::VERTEX)
                                           ? GL_VERTEX_SHADER
                                           : GL_FRAGMENT_SHADER),
                        [](unsigned int shaderID) {
                            if (shaderID != 0u) {
                                glDeleteShader(shaderID);
                            }
                        });
}

auto make_vertexObject(VertexObject type) -> VertexObject {
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
