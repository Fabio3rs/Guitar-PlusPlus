#include "GPPOBJ.h"
#include "CLog.h"
#include "GPPGame.h"
#include "SceneAO.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace {
namespace fs = std::filesystem;
struct MaterialResource {
    ObjModel::Material values;
    std::string directory;
    GLuint texture = 0;
    bool attempted = false;
};
using Materials = std::map<std::string, MaterialResource>;
void readLibrary(const fs::path &file, Materials &materials) {
    std::ifstream input(file);
    if (!input)
        return; // OBJ files without an accompanying MTL remain usable.
    for (auto &entry : ObjModel::readMaterials(input)) {
        auto &m = entry.second;
        // Preserve the project's historic material-name.tga convention only
        // when that file really exists; absent textures use neutral white.
        if (m.diffuseMap.empty()) {
            const auto name = fs::path(entry.first).extension() == ".tga"
                                  ? entry.first
                                  : entry.first + ".tga";
            if (fs::is_regular_file(file.parent_path() / name))
                m.diffuseMap = name;
        }
        if (!m.diffuseMap.empty()) {
            const auto texture =
                (file.parent_path() / m.diffuseMap).lexically_normal();
            m.diffuseMap = texture.filename().string();
            materials[entry.first] = {m, texture.parent_path().string(), 0,
                                      false};
        } else
            materials[entry.first] = {m, file.parent_path().string(), 0, false};
    }
}
// Unique ownership, stable addresses, and no pointers into CPU vectors retained
// by the GPU owner. The only integer-to-pointer conversions are GL VBO offsets.
struct Buffer {
    GLuint id = 0;
    size_t uvOffset = 0, normalOffset = 0, aoOffset = 0;
    bool ao = false;
    GLsizei count = 0;
    Buffer() = default;
    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;
    ~Buffer() {
        if (id && glfwGetCurrentContext())
            glDeleteBuffers(1, &id);
    }
    void upload(const ObjModel::Part &part) {
        const size_t countSize = part.positions.size();
        if (countSize != part.normals.size() || countSize != part.uvs.size() ||
            countSize >
                static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
            throw std::runtime_error("invalid OBJ vertex array sizes");
        static_assert(sizeof(ObjModel::Vec3) == 3 * sizeof(float),
                      "packed position array");
        static_assert(sizeof(ObjModel::Vec2) == 2 * sizeof(float),
                      "packed UV array");
        ao = !part.aoUVs.empty();
        if (ao && part.aoUVs.size() != countSize)
            throw std::runtime_error("invalid AO UV count");
        const size_t bytesPerVertex = 2 * sizeof(ObjModel::Vec3) +
                                      (ao ? 2u : 1u) * sizeof(ObjModel::Vec2);
        if (countSize >
            static_cast<size_t>(std::numeric_limits<GLsizeiptr>::max()) /
                bytesPerVertex)
            throw std::runtime_error("OBJ buffer too large");
        count = static_cast<GLsizei>(countSize);
        uvOffset = countSize * sizeof(ObjModel::Vec3);
        normalOffset = uvOffset + countSize * sizeof(ObjModel::Vec2);
        aoOffset = normalOffset + uvOffset;
        glGenBuffers(1, &id);
        glBindBuffer(GL_ARRAY_BUFFER, id);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(countSize * bytesPerVertex),
                     nullptr, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(uvOffset),
                        part.positions.data());
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(uvOffset),
                        static_cast<GLsizeiptr>(normalOffset - uvOffset),
                        part.uvs.data());
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(normalOffset),
                        static_cast<GLsizeiptr>(uvOffset), part.normals.data());
        if (ao)
            glBufferSubData(
                GL_ARRAY_BUFFER, static_cast<GLintptr>(aoOffset),
                static_cast<GLsizeiptr>(countSize * sizeof(ObjModel::Vec2)),
                part.aoUVs.data());
    }
    void draw() const {
        glBindBuffer(GL_ARRAY_BUFFER, id);
        glVertexPointer(3, GL_FLOAT, 0, nullptr);
        glTexCoordPointer(
            2, GL_FLOAT, 0,
            reinterpret_cast<const void *>(static_cast<uintptr_t>(uvOffset)));
        glNormalPointer(GL_FLOAT, 0,
                        reinterpret_cast<const void *>(
                            static_cast<uintptr_t>(normalOffset)));
        glClientActiveTexture(GL_TEXTURE1);
        if (ao) {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, 0,
                              reinterpret_cast<const void *>(
                                  static_cast<uintptr_t>(aoOffset)));
        } else
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glClientActiveTexture(GL_TEXTURE0);
        glDrawArrays(GL_TRIANGLES, 0, count);
    }
};
struct DrawState {
    GLint active = GL_TEXTURE0, client = GL_TEXTURE0, buffer = 0;
    DrawState() {
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &client);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        glPushAttrib(GL_TEXTURE_BIT | GL_ENABLE_BIT | GL_CURRENT_BIT);
        glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        glActiveTexture(GL_TEXTURE0);
        glClientActiveTexture(GL_TEXTURE0);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
    }
    ~DrawState() {
        glPopClientAttrib();
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
        glPopAttrib();
        glActiveTexture(static_cast<GLenum>(active));
        glClientActiveTexture(static_cast<GLenum>(client));
        CEngine::engine().invalidateRenderBindings();
    }
};
} // namespace

struct GPPOBJ::Impl {
    mutable std::mutex mutex;
    ObjModel::Model model;
    SceneAO::Atlas ao;
    GLuint aoTexture = 0;
    ~Impl() {
        if (aoTexture && glfwGetCurrentContext())
            glDeleteTextures(1, &aoTexture);
    }
    Materials materials;
    std::vector<std::unique_ptr<Buffer>> buffers;
    std::string explicitLibrary;
    size_t version = 0, uploadedVersion = 0;
};
GPPOBJ::GPPOBJ() : impl(std::make_unique<Impl>()) {}
GPPOBJ::~GPPOBJ() = default;
GPPOBJ::GPPOBJ(GPPOBJ &&) noexcept = default;
GPPOBJ &GPPOBJ::operator=(GPPOBJ &&) noexcept = default;

void GPPOBJ::loadTextureList(const std::string &path, const std::string &file) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->explicitLibrary = (fs::path(path) / file).string();
    // Decode asynchronously using the existing queue; callbacks do not retain
    // pointers to stack variables or to this model's containers.
    Materials materials;
    try {
        readLibrary(impl->explicitLibrary, materials);
    } catch (const std::exception &e) {
        CLog::log() << e.what();
        return;
    }
    for (const auto &entry : materials) {
        const auto &m = entry.second;
        if (!m.values.diffuseMap.empty() &&
            fs::is_regular_file(fs::path(m.directory) / m.values.diffuseMap))
            GPPGame::GuitarPP().loadTextureSingleAsync(
                GPPGame::loadTextureBatch(m.directory, m.values.diffuseMap,
                                          nullptr, nullptr));
    }
}

bool GPPOBJ::loadInternalObj(const std::string &path, const std::string &file,
                             const std::string &objName, bool /*loadMtlLib*/) {
    try {
        const auto filename = fs::path(path) / file;
        std::ifstream input(filename);
        if (!input)
            return false;
        auto model = ObjModel::read(input, objName);
        if (model.parts.empty())
            return false;
        SceneAO::Atlas ao;
        try {
            ao = SceneAO::read(filename, model);
        } catch (const std::exception &e) {
            CLog::log() << std::string("AO disabled: ") + e.what();
        }
        if (!ao.coordinates.empty()) {
            size_t begin = 0;
            for (auto &part : model.parts) {
                const size_t end = begin + part.positions.size();
                part.aoUVs.assign(
                    ao.coordinates.begin() + static_cast<std::ptrdiff_t>(begin),
                    ao.coordinates.begin() + static_cast<std::ptrdiff_t>(end));
                begin = end;
            }
        }
        Materials materials;
        for (const auto &library : model.libraries)
            readLibrary(filename.parent_path() / library, materials);
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (!impl->explicitLibrary.empty())
            readLibrary(impl->explicitLibrary, materials);
        for (const auto &warning : model.warnings)
            CLog::log() << warning;
        impl->model = std::move(model);
        impl->ao = std::move(ao);
        impl->materials = std::move(materials);
        ++impl->version;
        // Existing VBOs are retired at the next draw, never on this loader
        // thread.
        return true;
    } catch (const std::exception &e) {
        CLog::log() << std::string("OBJ ") + file + ": " + e.what();
        return false;
    }
}
void GPPOBJ::prepareForDraw() {
    if (!impl)
        return;
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (impl->uploadedVersion == impl->version)
        return;
    DrawState state;
    prepareBuffers();
}
void GPPOBJ::prepareBuffers() {
    if (impl->uploadedVersion != impl->version) {
        impl->buffers.clear();
        if (impl->aoTexture) {
            glDeleteTextures(1, &impl->aoTexture);
            impl->aoTexture = 0;
        }
        if (!impl->ao.pixels.empty()) {
            GLint maximum = 0;
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
            if (impl->ao.width <= static_cast<unsigned>(maximum) &&
                impl->ao.height <= static_cast<unsigned>(maximum)) {
                GLint alignment = 4;
                glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glActiveTexture(GL_TEXTURE2);
                glGenTextures(1, &impl->aoTexture);
                glBindTexture(GL_TEXTURE_2D, impl->aoTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8,
                             static_cast<GLsizei>(impl->ao.width),
                             static_cast<GLsizei>(impl->ao.height), 0,
                             GL_LUMINANCE, GL_UNSIGNED_BYTE,
                             impl->ao.pixels.data());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                GL_CLAMP_TO_EDGE);
                glActiveTexture(GL_TEXTURE0);
                glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
            }
        }
        impl->buffers.reserve(impl->model.parts.size());
        for (const auto &part : impl->model.parts) {
            auto buffer = std::make_unique<Buffer>();
            buffer->upload(part);
            impl->buffers.push_back(std::move(buffer));
        }
        // Texture resolution/upload happens only with the GL context current.
        for (auto &entry : impl->materials) {
            auto &m = entry.second;
            if (!m.attempted && !m.values.diffuseMap.empty()) {
                m.attempted = true;
                if (fs::is_regular_file(fs::path(m.directory) /
                                        m.values.diffuseMap))
                    m.texture =
                        GPPGame::GuitarPP()
                            .loadTexture(m.directory, m.values.diffuseMap)
                            .getTextId();
                if (m.texture) {
                    glBindTexture(GL_TEXTURE_2D, m.texture);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                    GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                    GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                    GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                    GL_REPEAT);
                }
            }
        }
        impl->uploadedVersion = impl->version;
    }
}
void GPPOBJ::drawMaterials(const MaterialBinder &bind,
                           unsigned int overrideTexture) {
    if (!impl)
        return;
    std::lock_guard<std::mutex> lock(impl->mutex);
    DrawState state;
    prepareBuffers();
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, impl->aoTexture);
    glActiveTexture(GL_TEXTURE0);
    static const ObjModel::Material neutral;
    for (size_t i = 0; i < impl->model.parts.size(); ++i) {
        const auto &part = impl->model.parts[i];
        auto it = impl->materials.find(part.material);
        const auto &material =
            it == impl->materials.end() ? neutral : it->second.values;
        GLuint texture = overrideTexture               ? overrideTexture
                         : it == impl->materials.end() ? 0
                                                       : it->second.texture;
        if (!part.textured)
            texture = 0;
        glBindTexture(GL_TEXTURE_2D, texture);
        if (texture)
            glEnable(GL_TEXTURE_2D);
        else
            glDisable(GL_TEXTURE_2D);
        if (bind && !bind(part, material, texture != 0))
            continue;
        impl->buffers[i]->draw();
    }
}
void GPPOBJ::draw(unsigned int texture, bool /*autoBindZeroVBO*/) {
    drawMaterials({}, texture);
}
void GPPOBJ::onlyDraw(bool /*autoBindZeroVBO*/) const {
    // Retained legacy entry point also safely handles a not-yet-uploaded model.
    const_cast<GPPOBJ *>(this)->draw(0);
}
void GPPOBJ::load(const std::string &path, const std::string &file) {
    loadInternalObj(path, file);
}
void GPPOBJ::unload() {
    if (!impl)
        return;
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->model = {};
    impl->materials.clear();
    impl->ao = {};
    ++impl->version;
    if (glfwGetCurrentContext()) {
        impl->buffers.clear();
        if (impl->aoTexture)
            glDeleteTextures(1, &impl->aoTexture);
        impl->aoTexture = 0;
    }
}
std::pair<ObjModel::Vec3, ObjModel::Vec3> GPPOBJ::bounds() const {
    std::lock_guard<std::mutex> lock(impl->mutex);
    return {impl->model.minimum, impl->model.maximum};
}
bool GPPOBJ::hasAO() const {
    std::lock_guard<std::mutex> lock(impl->mutex);
    return impl->uploadedVersion == impl->version && impl->aoTexture != 0;
}
size_t GPPOBJ::revision() const {
    std::lock_guard<std::mutex> lock(impl->mutex);
    return impl->version;
}
gppVec3f GPPOBJ::boxTestForMtl(const std::string &name) {
    std::lock_guard<std::mutex> lock(impl->mutex);
    ObjModel::Vec3 minimum{}, maximum{};
    bool first = true;
    for (const auto &part : impl->model.parts)
        if (part.material == name)
            for (const auto &p : part.positions) {
                if (first) {
                    minimum = maximum = p;
                    first = false;
                }
                for (size_t i = 0; i < 3; ++i) {
                    minimum[i] = std::min(minimum[i], p[i]);
                    maximum[i] = std::max(maximum[i], p[i]);
                }
            }
    return {maximum[0] - minimum[0], maximum[1] - minimum[1],
            maximum[2] - minimum[2]};
}
