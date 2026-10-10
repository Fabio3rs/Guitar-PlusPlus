#include "studio.h"
#include "CShader.h"
#include "json.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>

namespace Studio {
namespace {
using Clock = std::chrono::steady_clock;

glm::vec3 vector(const Vec &v) {
    return {v[0], v[1], v[2]};
}

Vec array(glm::vec3 v) {
    return {v.x, v.y, v.z};
}

void validateSettings(const Settings &s) {
    for (const auto &v :
         {s.camera.eye, s.camera.target, s.camera.up, s.light}) {
        for (float x : v) {
            if (!std::isfinite(x) || std::abs(x) > 1.e6f) {
                throw Error("parameters",
                            "Coordinates must be finite and within +/-1000000");
            }
        }
    }
    const auto forward = vector(s.camera.target) - vector(s.camera.eye);
    if (glm::length(forward) < .0001f ||
        glm::length(glm::cross(forward, vector(s.camera.up))) < .0001f) {
        throw Error("parameters", "Camera eye/target/up are degenerate");
    }
    if (!std::isfinite(s.camera.fov) || s.camera.fov < 5 ||
        s.camera.fov > 120 || s.quality < 0 || s.quality > 3 ||
        !std::isfinite(s.bias) || s.bias < 0 || s.bias > .02 ||
        !std::isfinite(s.time) || s.time < 0 || s.time > 1.e9) {
        throw Error("parameters", "Invalid FOV, quality, bias or time");
    }
}

Settings framedSettings(const GPPOBJ &model, const Settings &settings) {
    const auto bounds = model.bounds();
    const glm::vec3 center =
        (vector(bounds.first) + vector(bounds.second)) * .5f;
    const float radius = std::max(
        .1f, glm::length(vector(bounds.second) - vector(bounds.first)) * .5f);
    Settings s = settings;
    s.camera.target = array(center);
    s.camera.eye =
        array(center + glm::normalize(glm::vec3(1, .65f, 1)) * radius /
                           std::sin(glm::radians(s.camera.fov) * .5f) * 1.15f);
    s.camera.up = {0, 1, 0};
    return s;
}

void regularFile(const fs::path &file) {
    if (!fs::is_regular_file(file) || fs::file_size(file) > 256 * 1024 * 1024) {
        throw Error("load", "Missing, non-regular or oversized asset: " +
                                file.string());
    }
}

// Tool-only guard. Shared renderer hot paths retain their existing costs.
struct State {
    GLint fbo = 0, program = 0, matrix = 0, active = 0, client = 0, buffer = 0;

    State() {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &fbo);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_MATRIX_MODE, &matrix);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &client);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
    }

    ~State() {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, GLuint(fbo));
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glPopClientAttrib();
        glPopAttrib();
        glMatrixMode(GLenum(matrix));
        glUseProgram(GLuint(program));
        glActiveTexture(GLenum(active));
        glClientActiveTexture(GLenum(client));
        glBindBuffer(GL_ARRAY_BUFFER, GLuint(buffer));
        CEngine::engine().invalidateRenderBindings();
    }
};
} // namespace

Session::Scene::Scene()
    : model([this](const std::string &file) {
          const std::string key = fs::weakly_canonical(file).string();
          auto found = textures.find(key);
          if (found != textures.end()) {
              return found->second;
          }
          regularFile(key);
          GLuint texture = CEngine::engine().loadTexture(key.c_str());
          if (!texture) {
              throw Error("texture", "Cannot decode texture: " + key);
          }
          textures.emplace(key, texture);
          return texture;
      }) {}

Session::Scene::~Scene() {
    model.unload();
    for (const auto &item : textures) {
        glDeleteTextures(1, &item.second);
    }
}

Session::Session(Context &ctx, fs::path assets, fs::path output)
    : context(ctx) {
    assetRoot = fs::absolute(assets.empty() ? fs::current_path() : assets);
    outputRoot = fs::weakly_canonical(fs::absolute(output));
    fs::create_directories(outputRoot);
}

Session::~Session() {
    scene.reset();
    viewport.clear();
    captureTarget.clear();
    CBackgroundRenderer::shutdown();
    ShaderProject::CShader::shutdown();
}

void Session::load(const std::string &kind, const std::string &value) {
    auto candidate = std::make_unique<Scene>();
    candidate->kind = kind;
    candidate->selection = value;
    CScene::Definition definition;
    fs::path file;
    std::string error;
    if (kind == "model") {
        file = fs::absolute(value);
        definition.id = "model";
        definition.model = file.string();
        definition.light = {2, 4, 2};
    } else if (kind == "file") {
        file = fs::absolute(value);
        regularFile(file);
        if (!CScene::readInspectionDefinition(file.string(), "file", definition,
                                              error)) {
            throw Error("scene", error);
        }
    } else if (kind == "scene") {
        if (!CScene::validId(value)) {
            throw Error("scene", "Invalid scene identifier");
        }
        file = value == "synthetic"
                   ? fs::path(GPP_STUDIO_FIXTURES) / "synthetic.lua"
                   : assetRoot / "data/scenes" / (value + ".lua");
        if (value == "garage" && !fs::is_regular_file(file)) {
            definition = CScene::Definition::garage();
            definition.model = (assetRoot / definition.model).string();
            definition.materials = (assetRoot / definition.materials).string();
            file = definition.model;
        } else {
            regularFile(file);
            if (!CScene::readInspectionDefinition(file.string(), value,
                                                  definition, error)) {
                throw Error("scene", error);
            }
        }
    } else if (kind == "auto") {
        // Search only known project/runtime roots, never the user's HOME.
        for (const auto &root :
             {assetRoot, fs::path(GPP_STUDIO_WORKSPACE) / "build/bin"}) {
            if (fs::is_regular_file(root / "test/garage_gpp.obj") &&
                fs::is_regular_file(root / "test/garage_gpp.mtl")) {
                definition = CScene::Definition::garage();
                definition.model = (root / definition.model).string();
                definition.materials = (root / definition.materials).string();
                file = definition.model;
                break;
            }
        }
        if (definition.model.empty()) {
            load("scene", "synthetic");
            return;
        }
    } else {
        throw Error("parameters", "Load kind must be scene, file or model");
    }
    regularFile(definition.model);
    if (!definition.materials.empty()) {
        regularFile(definition.materials);
    }
    candidate->model.setMaterialLibrary(definition.materials);
    const fs::path modelFile(definition.model);
    if (!candidate->model.loadInternalObj(modelFile.parent_path().string(),
                                          modelFile.filename().string())) {
        throw Error("load", "OBJ decoding failed: " + modelFile.string());
    }
    {
        State guard;
        candidate->model.prepareForDraw();
        if (glGetError() != GL_NO_ERROR) {
            throw Error("upload", "OpenGL model upload failed");
        }
    }
    candidate->definition = definition;
    candidate->source = fs::absolute(file).lexically_normal().string();
    candidate->inspection = candidate->model.inspect();
    Settings prepared;
    prepared.light = definition.light;
    prepared = framedSettings(candidate->model, prepared);
    if (definition.id == "garage") {
        // Match the existing background preview's useful interior viewpoint.
        prepared.camera.eye = {3, 2.5f, 2};
        prepared.camera.target = {3, .5f, -5};
    }
    validateSettings(prepared);

    // Commit only after all fallible loading and initial-state validation.
    scene.swap(candidate);
    CBackgroundRenderer::inst().setScene(prepared.light,
                                         definition.garageMaterials);
    apply(prepared);
    initial = settings;
}

void Session::reload() {
    if (!scene) {
        throw Error("scene", "No scene loaded");
    }
    const std::string kind = scene->kind, selection = scene->selection;
    const Settings previous = settings;
    load(kind, selection);
    apply(previous);
}

void Session::apply(const Settings &s) {
    validateSettings(s);
    if (s.light != settings.light && scene) {
        CBackgroundRenderer::inst().setScene(s.light,
                                             scene->definition.garageMaterials);
    }
    settings = s;
    auto &renderer = CBackgroundRenderer::inst();
    renderer.quality = s.quality;
    renderer.shadowBias = s.bias;
    renderer.ambientOcclusion = s.ao;
}

void Session::reset() {
    apply(initial);
}

void Session::frame() {
    if (!scene) {
        return;
    }
    apply(framedSettings(scene->model, settings));
}

void Session::navigate(float orbitX, float orbitY, float panX, float panY,
                       float zoom) {
    Settings s = settings;
    glm::vec3 target = vector(s.camera.target), eye = vector(s.camera.eye);
    glm::vec3 offset = eye - target;
    float distance = glm::length(offset);
    float yaw = std::atan2(offset.x, offset.z) - orbitX * .008f;
    float pitch = std::asin(std::clamp(offset.y / distance, -1.f, 1.f));
    pitch = std::clamp(pitch + orbitY * .008f, -1.5f, 1.5f);
    distance = std::clamp(distance * std::exp(-zoom * .12f), .02f, 1.e5f);
    offset = glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch),
                       std::cos(yaw) * std::cos(pitch)) *
             distance;
    const glm::vec3 right =
        glm::normalize(glm::cross(-offset, glm::vec3(0, 1, 0)));
    const glm::vec3 up = glm::normalize(glm::cross(right, -offset));
    target += (-right * panX + up * panY) * distance * .0015f;
    s.camera.target = array(target);
    s.camera.eye = array(target + offset);
    s.camera.up = {0, 1, 0};
    apply(s);
}

void Session::render(Target &target, int w, int h) {
    if (!scene) {
        throw Error("scene", "No scene loaded");
    }
    const auto start = Clock::now();
    State guard;
    target.resize(w, h, context.caps);
    target.bind();
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDepthRange(0, 1);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(.035f, .043f, .06f, 1);
    glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glClientActiveTexture(GL_TEXTURE0);
    glColor4f(1, 1, 1, 1);
    const auto &camera = settings.camera;
    const auto bounds = scene->model.bounds();
    const float radius = std::max(
        .1f, glm::length(vector(bounds.second) - vector(bounds.first)));
    const float distance =
        glm::length(vector(camera.eye) - vector(camera.target));
    const auto projection =
        glm::perspective(glm::radians(camera.fov), float(w) / float(h),
                         std::max(.001f, radius * .0001f),
                         distance + radius * 4 + 10) *
        glm::lookAt(vector(camera.eye), vector(camera.target),
                    vector(camera.up));
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    auto &engine = CEngine::engine();
    engine.nowCamera.eye = {camera.eye[0], camera.eye[1], camera.eye[2]};
    engine.nowCamera.center = {camera.target[0], camera.target[1],
                               camera.target[2]};
    engine.nowCamera.up = {camera.up[0], camera.up[1], camera.up[2]};
    if (settings.quality == 0) {
        const GLfloat ambient[] = {.2f, .2f, .2f, 1},
                      diffuse[] = {1, .94f, .83f, 1};
        const GLfloat light[] = {settings.light[0], settings.light[1],
                                 settings.light[2], 1};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
        glLightfv(GL_LIGHT1, GL_POSITION, light);
        glEnable(GL_LIGHT1);
        glEnable(GL_LIGHTING);
        glEnable(GL_NORMALIZE);
        glEnable(GL_COLOR_MATERIAL);
    }
    CBackgroundRenderer::inst().draw(scene->model);
    const auto error = glGetError();
    if (error != GL_NO_ERROR) {
        throw Error("render", "OpenGL error " + std::to_string(error));
    }
    lastFrameMs =
        std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

fs::path Session::artifact(const fs::path &name) const {
    if (name.empty()) {
        throw Error("path", "Empty artifact path");
    }
    const fs::path resolved =
        fs::weakly_canonical(name.is_absolute() ? name : outputRoot / name);
    const auto relative = resolved.lexically_relative(outputRoot);
    if (relative.empty() || *relative.begin() == "..") {
        throw Error("path", "Artifact must be inside --output-dir");
    }
    if (scene &&
        (resolved == fs::weakly_canonical(scene->definition.model) ||
         (!scene->definition.materials.empty() &&
          resolved == fs::weakly_canonical(scene->definition.materials)) ||
         resolved == fs::weakly_canonical(scene->source))) {
        throw Error("path", "Cannot overwrite scene sources");
    }
    fs::create_directories(resolved.parent_path());
    return resolved;
}

std::string Session::capture(const fs::path &name, int w, int h,
                             const fs::path &metadata) {
    const auto destination = artifact(name);
    if (destination.extension() != ".png") {
        throw Error("path", "Capture filename must end in .png");
    }
    const fs::path meta = metadata.empty()
                              ? fs::path(destination.string() + ".json")
                              : artifact(metadata);
    if (meta == destination) {
        throw Error("path", "PNG and metadata paths must differ");
    }
    render(captureTarget, w, h);
    State guard;
    captureTarget.bind();
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    const auto pixels = readPixels(w, h);
    Json result = parse(state());
    auto &a = result.GetAllocator();
    put(result, "capture", destination.string(), a);
    put(result, "metadata", meta.string(), a);
    put(result, "width", w, a);
    put(result, "height", h, a);
    put(result, "image_kind", "clean_viewport", a);
    const std::string json = encode(result);
    const fs::path temporary = destination.string() + ".tmp",
                   metaTemp = meta.string() + ".tmp";
    try {
        writePNG(temporary, w, h, pixels);
        std::ofstream out(metaTemp, std::ios::binary | std::ios::trunc);
        out << json << '\n';
        out.close();
        if (!out) {
            throw Error("capture", "Cannot write metadata: " + meta.string());
        }
        fs::rename(temporary, destination);
        fs::rename(metaTemp, meta);
    } catch (...) {
        std::error_code ec;
        fs::remove(temporary, ec);
        fs::remove(metaTemp, ec);
        throw;
    }
    return json;
}

std::string Session::benchmark(int frames, int warmup, int w, int h,
                               double timeout) {
    if (frames < 1 || frames > 10000 || warmup < 0 || warmup > 1000 ||
        !std::isfinite(timeout) || timeout <= 0 || timeout > 300) {
        throw Error("parameters", "Invalid benchmark frame count or timeout");
    }
    const auto start = Clock::now();
    std::vector<double> timings;
    for (int i = 0; i < frames + warmup; ++i) {
        if (std::chrono::duration<double>(Clock::now() - start).count() >
            timeout) {
            throw Error("timeout", "Benchmark deadline exceeded");
        }
        const auto frameStart = Clock::now();
        render(captureTarget, w, h);
        glFinish();
        if (i >= warmup) {
            timings.push_back(std::chrono::duration<double, std::milli>(
                                  Clock::now() - frameStart)
                                  .count());
        }
    }
    std::sort(timings.begin(), timings.end());
    Json result = parse(state());
    auto &a = result.GetAllocator();
    put(result, "median_ms", timings[timings.size() / 2], a);
    put(result, "p95_ms", timings[size_t(double(timings.size() - 1) * .95)], a);
    put(result, "frames", frames, a);
    put(result, "warmup", warmup, a);
    put(result, "width", w, a);
    put(result, "height", h, a);
    put(result, "measurement",
        "CPU submission plus GPU completion; no swap/readback", a);
    return encode(result);
}

const GPPOBJ::Inspection &Session::inspection() const {
    if (!scene) {
        throw Error("scene", "No scene loaded");
    }
    return scene->inspection;
}

std::string Session::sceneLabel() const {
    return scene ? scene->definition.id : "none";
}

std::string Session::state() const {
    Json result(rapidjson::kObjectType);
    auto &a = result.GetAllocator();
    put(result, "schema_version", 1, a);
    put(result, "scene", sceneLabel(), a);
    if (scene) {
        put(result, "source", scene->source, a);
        put(result, "model", scene->definition.model, a);
        put(result, "material_library", scene->definition.materials, a);
        const auto bounds = scene->model.bounds();
        put(result, "bounds_min", bounds.first, a);
        put(result, "bounds_max", bounds.second, a);
    }
    put(result, "eye", settings.camera.eye, a);
    put(result, "target", settings.camera.target, a);
    put(result, "up", settings.camera.up, a);
    put(result, "fov", double(settings.camera.fov), a);
    put(result, "light", settings.light, a);
    put(result, "quality", settings.quality, a);
    put(result, "ao", settings.ao, a);
    put(result, "shadow_bias", settings.bias, a);
    put(result, "time", settings.time, a);
    put(result, "animated", false, a);
    const auto d = CBackgroundRenderer::inst().diagnostics();
    const bool lit = settings.quality > 0 && d.lighting;
    const bool shadows = lit && settings.quality >= 2 && d.shadowSupported &&
                         !d.shadowFailed && d.shadowResolution > 0;
    put(result, "lighting_active", lit, a);
    put(result, "shadows_active", shadows, a);
    put(result, "shadow_supported", d.shadowSupported, a);
    put(result, "shadow_resolution", shadows ? d.shadowResolution : 0, a);
    put(result, "shadow_updates", CBackgroundRenderer::inst().shadowUpdates(),
        a);
    put(result, "ao_atlas", scene && scene->inspection.aoAtlas, a);
    put(result, "ao_active",
        lit && settings.ao && scene && scene->model.hasAO(), a);
    put(result, "ao_reason",
        !settings.ao                           ? "disabled"
        : !scene || !scene->inspection.aoAtlas ? "no_valid_atlas"
        : !lit                                 ? "lighting_inactive"
        : !scene->model.hasAO()                ? "atlas_upload_unavailable"
                                               : "active",
        a);
    put(result, "shadow_reason",
        settings.quality < 2      ? "disabled"
        : !d.lighting             ? "lighting_unavailable"
        : !d.shadowSupported      ? "unsupported"
        : d.shadowFailed          ? "allocation_failed"
        : d.shadowResolution == 0 ? "not_rendered"
                                  : "active",
        a);
    const auto &c = context.caps;
    put(result, "backend", c.backend, a);
    put(result, "vendor", c.vendor, a);
    put(result, "renderer", c.renderer, a);
    put(result, "version", c.version, a);
    put(result, "acceleration", c.acceleration, a);
    put(result, "fbo", c.fbo, a);
    put(result, "npot", c.npot, a);
    put(result, "depth_texture", c.depthTexture, a);
    put(result, "max_texture", c.maxTexture, a);
    put(result, "max_renderbuffer", c.maxRenderbuffer, a);
    put(result, "viewport_width", viewport.width, a);
    put(result, "viewport_height", viewport.height, a);
    Value parts(rapidjson::kArrayType), materials(rapidjson::kObjectType),
        warnings(rapidjson::kArrayType);
    if (scene) {
        for (const auto &part : scene->inspection.parts) {
            Value p(rapidjson::kObjectType);
            put(p, "object", part.object, a);
            put(p, "group", part.group, a);
            put(p, "material", part.material, a);
            put(p, "vertices", part.vertices, a);
            parts.PushBack(p, a);
        }
        for (const auto &entry : scene->inspection.materials) {
            Value m(rapidjson::kObjectType);
            const auto &v = entry.second;
            put(m, "ambient", v.ambient, a);
            put(m, "diffuse", v.diffuse, a);
            put(m, "specular", v.specular, a);
            put(m, "emission", v.emission, a);
            put(m, "shininess", double(v.shininess), a);
            put(m, "opacity", double(v.opacity), a);
            put(m, "texture", v.diffuseMap, a);
            materials.AddMember(str(entry.first, a), m, a);
        }
        for (const auto &message : scene->inspection.warnings) {
            warnings.PushBack(str(message, a), a);
        }
    }
    result.AddMember("parts", parts, a);
    result.AddMember("materials", materials, a);
    result.AddMember("warnings", warnings, a);
    return encode(result);
}
} // namespace Studio
