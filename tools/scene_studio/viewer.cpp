#include "studio.h"
#include <algorithm>
#include <cmath>

namespace Studio {
void Session::updateVisibleBounds() {
    visibleBounds = {};
    for (const auto &part : inspection().parts) {
        if (isVisible(part.id) && part.vertices) {
            visibleBounds.include(part.minimum, part.maximum);
        }
    }
}

void Session::select(int part) {
    if (part < -1 || (part >= 0 && size_t(part) >= visible.size())) {
        throw Error("parameters", "Invalid part ID for this model");
    }
    std::fill(highlight.begin(), highlight.end(), 0);
    selected = part;
    if (part >= 0) {
        highlight[size_t(part)] = 1;
    }
}

void Session::visibility(const std::string &action, int part) {
    if (action != "show_all" && action != "hide" && action != "show" &&
        action != "isolate") {
        throw Error(
            "parameters",
            "Visibility action must be hide, show, isolate or show_all");
    }
    if (action != "show_all" && (part < 0 || size_t(part) >= visible.size())) {
        throw Error("parameters", "Invalid part ID for this model");
    }
    bool changed = false;
    for (size_t i = 0; i < visible.size(); ++i) {
        const unsigned char value = action == "show_all"  ? 1
                                    : action == "isolate" ? (i == size_t(part))
                                    : i == size_t(part)   ? action == "show"
                                                          : visible[i];
        changed |= visible[i] != value;
        visible[i] = value;
    }
    if (changed) {
        ++visibilityRevision;
        updateVisibleBounds();
    }
}

GPPOBJ::Hit Session::pick(float x, float y, int w, int h) const {
    if (!scene || !std::isfinite(x) || !std::isfinite(y) || w <= 0 || h <= 0 ||
        w > 8192 || h > 8192 || x < 0 || y < 0 || x >= w || y >= h) {
        throw Error("parameters",
                    "Picking requires coordinates inside a valid viewport");
    }
    const auto ray = CameraMath::ray(settings.camera, x, y, w, h);
    return scene->model.raycast(ray.origin, ray.direction, &visible);
}

void Session::pickSelection(float x, float y, int w, int h, bool pivot) {
    const auto hit = pick(x, y, w, h);
    if (pivot && hit.part >= 0) {
        auto next = settings;
        next.camera.target = hit.point;
        // Orthogonalize up for a valid view after changing the target.
        const auto f =
            CameraMath::vector(hit.point) - CameraMath::vector(next.camera.eye);
        if (glm::length(glm::cross(f, CameraMath::vector(next.camera.up))) <
            .0001f) {
            next.camera.up =
                std::abs(f.y) < std::abs(f.z) ? Vec{0, 1, 0} : Vec{0, 0, 1};
        }
        apply(next);
    }
    select(hit.part);
}

namespace {
// Fixed-function texture transforms keep the inspection path usable on GL 2.0.
// Texture-object parameters are not restored by glPushAttrib(GL_TEXTURE_BIT).
class MaterialTextureState {
    GLint matrixMode = GL_MODELVIEW;
    GLint texture = 0, wrapS = GL_REPEAT, wrapT = GL_REPEAT;

    void restoreWrap() {
        if (texture) {
            GLint current = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &current);
            glBindTexture(GL_TEXTURE_2D, GLuint(texture));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
            glBindTexture(GL_TEXTURE_2D, GLuint(current));
            texture = 0;
        }
    }

  public:
    MaterialTextureState() {
        glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
    }

    ~MaterialTextureState() {
        glActiveTexture(GL_TEXTURE0);
        restoreWrap();
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
        glMatrixMode(GLenum(matrixMode));
    }

    void apply(const ObjModel::Material &m, bool textured) {
        restoreWrap();
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glTranslatef(m.uvOffset[0], m.uvOffset[1], 0);
        glScalef(m.uvScale[0], m.uvScale[1], 1);
        glMatrixMode(GLenum(matrixMode));
        if (textured) {
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrapS);
            glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wrapT);
            const GLint wrap = m.clampMap ? GL_CLAMP_TO_EDGE : GL_REPEAT;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        }
    }
};

void line(const Vec &a, const Vec &b) {
    glVertex3fv(a.data());
    glVertex3fv(b.data());
}

void box(const Vec &low, const Vec &high) {
    for (unsigned corner = 0; corner < 8; ++corner) {
        Vec p{};
        for (unsigned axis = 0; axis < 3; ++axis) {
            p[axis] = (corner & (1U << axis)) ? high[axis] : low[axis];
        }
        for (unsigned axis = 0; axis < 3; ++axis) {
            if (!(corner & (1U << axis))) {
                Vec q = p;
                q[axis] = high[axis];
                line(p, q);
            }
        }
    }
}

void plainLines() {
    glUseProgram(0);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    for (int unit = 2; unit >= 0; --unit) {
        glActiveTexture(GL_TEXTURE0 + GLenum(unit));
        glDisable(GL_TEXTURE_2D);
    }
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glLineWidth(1);
}
} // namespace

void Session::drawModel(bool helpers) {
    if (settings.viewMode == 0 || settings.viewMode == 3) {
        CBackgroundRenderer::inst().draw(scene->model, &visible,
                                         visibilityRevision);
    } else if (settings.viewMode == 1) {
        glUseProgram(0);
        glDisable(GL_LIGHTING);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, .05f);
        glActiveTexture(GL_TEXTURE2);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        MaterialTextureState textureState;
        for (int pass = 0; pass < 2; ++pass) {
            if (pass) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            } else {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            }
            scene->model.drawMaterials(
                [&](const ObjModel::Part &, const ObjModel::Material &m,
                    bool textured) {
                    if ((m.opacity < .999f) != (pass == 1)) {
                        return false;
                    }
                    textureState.apply(m, textured);
                    glColor4f(m.diffuse[0], m.diffuse[1], m.diffuse[2],
                              m.opacity);
                    return true;
                },
                0, &visible);
        }
    }
    if (settings.viewMode == 2 || settings.viewMode == 3) {
        plainLines();
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1, -1);
        // Standalone wireframe writes depth; overlays retain the filled depth.
        glDepthMask(settings.viewMode == 2 ? GL_TRUE : GL_FALSE);
        scene->model.drawMaterials(
            [](const ObjModel::Part &, const ObjModel::Material &, bool) {
                glDisable(GL_TEXTURE_2D);
                glColor3f(.65f, .8f, .95f);
                return true;
            },
            0, &visible);
    }
    if (helpers && selected >= 0 && isVisible(size_t(selected))) {
        plainLines();
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-2, -2);
        scene->model.drawMaterials(
            [](const ObjModel::Part &, const ObjModel::Material &, bool) {
                glDisable(GL_TEXTURE_2D);
                glColor3f(1, .75f, .15f);
                return true;
            },
            0, &highlight);
    }
    if (helpers) {
        drawHelpers();
    }
}

void Session::drawHelpers() {
    if (!settings.grid && !settings.axes && !settings.bounds && selected < 0) {
        return;
    }
    plainLines();
    const auto modelBounds = scene->model.bounds();
    const Bounds bounds =
        visibleBounds.valid
            ? visibleBounds
            : Bounds{modelBounds.first, modelBounds.second, true};
    const auto center = bounds.center();
    const float scale = bounds.scale();
    glBegin(GL_LINES);
    if (settings.grid) {
        const float step = std::pow(10.f, std::floor(std::log10(scale / 10)));
        const float cx = std::floor(center[0] / step) * step;
        const float cz = std::floor(center[2] / step) * step;
        const float y = bounds.minimum[1];
        glColor3f(.25f, .28f, .32f);
        for (int i = -10; i <= 10; ++i) {
            const float offset = float(i) * step;
            line({cx + offset, y, cz - 10 * step},
                 {cx + offset, y, cz + 10 * step});
            line({cx - 10 * step, y, cz + offset},
                 {cx + 10 * step, y, cz + offset});
        }
    }
    if (settings.bounds && visibleBounds.valid) {
        glColor3f(.5f, .7f, 1);
        box(bounds.minimum, bounds.maximum);
    }
    if (selected >= 0 && isVisible(size_t(selected))) {
        const auto &part = inspection().parts[size_t(selected)];
        glColor3f(1, .75f, .15f);
        box(part.minimum, part.maximum);
    }
    glEnd();
    if (settings.axes) {
        // Orientation reference remains readable when the bounds center is
        // inside a solid object. Other helpers retain depth testing.
        glDisable(GL_DEPTH_TEST);
        glBegin(GL_LINES);
        for (size_t axis = 0; axis < 3; ++axis) {
            Vec color{}, end = center;
            color[axis] = 1;
            glColor3fv(color.data());
            end[axis] += scale * .2f;
            line(center, end);
        }
        glEnd();
        glEnable(GL_DEPTH_TEST);
    }
}
} // namespace Studio
