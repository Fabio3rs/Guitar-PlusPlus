#include "camera.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <stdexcept>

namespace Studio {
namespace CameraMath {
glm::vec3 vector(const Vec &v) { return {v[0], v[1], v[2]}; }
Vec array(glm::vec3 v) { return {v.x, v.y, v.z}; }

namespace {
glm::vec3 forward(const Camera &c) {
    return glm::normalize(vector(c.target) - vector(c.eye));
}

glm::vec3 right(const Camera &c) {
    return glm::normalize(glm::cross(forward(c), vector(c.up)));
}

float verticalSpan(const Camera &c) {
    return c.orthographic ? c.orthoHeight
                          : 2 * glm::length(vector(c.eye) - vector(c.target)) *
                                std::tan(glm::radians(c.fov) * .5f);
}
} // namespace

glm::mat4 view(const Camera &c) {
    return glm::lookAt(vector(c.eye), vector(c.target), vector(c.up));
}

glm::mat4 projection(const Camera &c, const Bounds &b, int w, int h) {
    if (w <= 0 || h <= 0) {
        throw std::invalid_argument("Viewport dimensions must be positive");
    }
    const auto f = forward(c), eye = vector(c.eye);
    float low = std::numeric_limits<float>::max(), high = 0;
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 p(i & 1 ? b.maximum[0] : b.minimum[0],
                          i & 2 ? b.maximum[1] : b.minimum[1],
                          i & 4 ? b.maximum[2] : b.minimum[2]);
        const float depth = glm::dot(p - eye, f);
        low = std::min(low, depth);
        high = std::max(high, depth);
    }
    const float focusDistance = glm::length(vector(c.target) - eye);
    const float floor =
        std::max(1.e-5f, std::min(b.scale() * 1.e-5f, focusDistance * .01f));
    // Preserve a representable depth interval even for tiny geometry viewed
    // from far away. A scale-only margin can round near and far to one float.
    const float margin = std::max(
        b.scale() * .05f, std::max(std::abs(low), high) *
                              std::numeric_limits<float>::epsilon() * 8);
    const float near = std::max(floor, low - margin);
    const float far =
        std::max(near + std::max(b.scale(), margin), high + margin);
    const float aspect = float(w) / float(h);
    if (c.orthographic) {
        const float half = c.orthoHeight * .5f;
        return glm::ortho(-half * aspect, half * aspect, -half, half, near,
                          far);
    }
    return glm::perspective(glm::radians(c.fov), aspect, near, far);
}

Ray ray(const Camera &c, float x, float y, int w, int h) {
    if (w <= 0 || h <= 0) {
        throw std::invalid_argument("Viewport dimensions must be positive");
    }
    const auto f = forward(c), r = right(c),
               u = glm::normalize(glm::cross(r, f));
    const float nx = 2 * x / float(w) - 1, ny = 1 - 2 * y / float(h);
    const float half = c.orthographic ? c.orthoHeight * .5f
                                      : std::tan(glm::radians(c.fov) * .5f);
    const auto offset = r * (nx * half * float(w) / float(h)) + u * (ny * half);
    return c.orthographic ? Ray{array(vector(c.eye) + offset), array(f)}
                          : Ray{c.eye, array(glm::normalize(f + offset))};
}

void frame(Camera &c, const Bounds &b, int w, int h) {
    if (!b.valid || w <= 0 || h <= 0) {
        throw std::invalid_argument(
            "No geometry or invalid framing dimensions");
    }
    const auto f = forward(c), r = right(c),
               u = glm::normalize(glm::cross(r, f));
    const auto center = vector(b.center());
    const float ty = std::tan(glm::radians(c.fov) * .5f),
                tx = ty * float(w) / float(h);
    float distance = std::max(.001f, b.scale() * .01f);
    float height = std::max(.001f, b.scale() * .01f);
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 p(i & 1 ? b.maximum[0] : b.minimum[0],
                          i & 2 ? b.maximum[1] : b.minimum[1],
                          i & 4 ? b.maximum[2] : b.minimum[2]);
        const auto offset = p - center;
        const float x = std::abs(glm::dot(offset, r)),
                    y = std::abs(glm::dot(offset, u));
        const float z = glm::dot(offset, f);
        distance = std::max(distance, std::max(x / tx, y / ty) * 1.15f - z);
        height =
            std::max(height, 2 * std::max(y, x * float(h) / float(w)) * 1.15f);
    }
    // Keep the eye outside the bounds for orthographic views as well.
    if (c.orthographic) {
        distance = std::max(distance, b.scale());
    }
    c.target = b.center();
    c.eye = array(center - f * distance);
    c.orthoHeight = height;
}

void projectionMode(Camera &c, bool orthographic) {
    if (c.orthographic == orthographic) {
        return;
    }
    if (orthographic) {
        c.orthoHeight = std::clamp(verticalSpan(c), .001f, 1.e6f);
        c.free = false;
    } else {
        const float distance =
            c.orthoHeight / (2 * std::tan(glm::radians(c.fov) * .5f));
        c.eye = array(vector(c.target) - forward(c) * distance);
    }
    c.orthographic = orthographic;
}

void preset(Camera &c, const std::string &name) {
    glm::vec3 offset, up(0, 1, 0);
    if (name == "front") {
        offset = {0, 0, 1};
    } else if (name == "back") {
        offset = {0, 0, -1};
    } else if (name == "left") {
        offset = {-1, 0, 0};
    } else if (name == "right") {
        offset = {1, 0, 0};
    } else if (name == "top") {
        offset = {0, 1, 0};
        up = {0, 0, -1};
    } else if (name == "bottom") {
        offset = {0, -1, 0};
        up = {0, 0, 1};
    } else {
        throw std::invalid_argument("Unknown camera view");
    }
    const float distance = glm::length(vector(c.eye) - vector(c.target));
    c.eye = array(vector(c.target) + offset * distance);
    c.up = array(up);
}

void orbit(Camera &c, float x, float y, bool look) {
    const auto r = right(c), u = glm::normalize(glm::cross(r, forward(c)));
    const auto rotation = glm::rotate(glm::mat4(1), -x * .008f, u) *
                          glm::rotate(glm::mat4(1), -y * .008f, r);
    const auto offset =
        glm::vec3(rotation * glm::vec4(vector(c.eye) - vector(c.target), 0));
    c.up = array(glm::normalize(glm::vec3(rotation * glm::vec4(u, 0))));
    if (look) {
        c.target = array(vector(c.eye) - offset);
    } else {
        c.eye = array(vector(c.target) + offset);
    }
}

void pan(Camera &c, float x, float y, int height) {
    const auto r = right(c), u = glm::normalize(glm::cross(r, forward(c)));
    const auto shift =
        (-r * x + u * y) * verticalSpan(c) / float(std::max(1, height));
    c.eye = array(vector(c.eye) + shift);
    c.target = array(vector(c.target) + shift);
}

void zoom(Camera &c, float wheel, const Vec &anchor, float scale) {
    if (c.free) {
        const auto shift = forward(c) * wheel * c.speed * .1f;
        c.eye = array(vector(c.eye) + shift);
        c.target = array(vector(c.target) + shift);
        return;
    }
    const float distance = c.orthographic
                               ? c.orthoHeight
                               : glm::length(vector(c.eye) - vector(c.target));
    // The coordinate floor also avoids collapsing two float positions far from
    // zero.
    const float coordinate =
        std::max(glm::length(vector(c.eye)), glm::length(vector(c.target)));
    // A new pivot/selection can change scale. Never reverse wheel direction
    // merely because the previous distance lies outside the new limits.
    const float minimum = std::min(
        distance, std::max({.001f, scale * 1.e-5f, coordinate * 1.e-6f}));
    const float maximum =
        std::max(distance, std::max(minimum, std::min(1.e6f, scale * 1.e4f)));
    const float next =
        std::clamp(distance * std::exp(std::clamp(-wheel * .12f, -20.f, 20.f)),
                   minimum, maximum);
    const float factor = next / distance;
    const auto p = vector(anchor);
    if (c.orthographic) {
        // Shift in the image plane only; preserve eye/target separation.
        const auto f = forward(c);
        auto shift = (p - vector(c.target)) * (1 - factor);
        shift -= f * glm::dot(shift, f);
        c.eye = array(vector(c.eye) + shift);
        c.target = array(vector(c.target) + shift);
        c.orthoHeight = next;
    } else {
        c.eye = array(p + (vector(c.eye) - p) * factor);
        c.target = array(p + (vector(c.target) - p) * factor);
    }
}

void move(Camera &c, const Vec &local, float delta, bool fast) {
    const auto f = forward(c), r = right(c),
               u = glm::normalize(glm::cross(r, f));
    auto direction = r * local[0] + u * local[1] + f * local[2];
    if (glm::length(direction) > 1) {
        direction = glm::normalize(direction);
    }
    const auto shift =
        direction * c.speed * std::clamp(delta, 0.f, .1f) * (fast ? 4.f : 1.f);
    c.eye = array(vector(c.eye) + shift);
    c.target = array(vector(c.target) + shift);
}
} // namespace CameraMath

void Bounds::include(const Vec &low, const Vec &high) {
    if (!valid) {
        minimum = low;
        maximum = high;
        valid = true;
        return;
    }
    for (size_t i = 0; i < 3; ++i) {
        minimum[i] = std::min(minimum[i], low[i]);
        maximum[i] = std::max(maximum[i], high[i]);
    }
}

float Bounds::scale() const {
    return std::max(.01f, glm::length(CameraMath::vector(maximum) -
                                      CameraMath::vector(minimum)));
}

Vec Bounds::center() const {
    return CameraMath::array(
        (CameraMath::vector(minimum) + CameraMath::vector(maximum)) * .5f);
}
} // namespace Studio
