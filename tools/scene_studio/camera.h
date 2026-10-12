#pragma once
#include <array>
#include <glm/glm.hpp>
#include <string>

namespace Studio {
using Vec = std::array<float, 3>;

struct Camera {
    Vec eye{4, 3, 5}, target{0, .5f, 0}, up{0, 1, 0};
    float fov = 45;
    bool orthographic = false, free = false;
    float orthoHeight = 10, speed = 1;
};

struct Bounds {
    Vec minimum{}, maximum{};
    bool valid = false;
    void include(const Vec &low, const Vec &high);
    float scale() const;
    Vec center() const;
};

namespace CameraMath {
struct Ray {
    Vec origin, direction;
};
glm::vec3 vector(const Vec &v);
Vec array(glm::vec3 v);
glm::mat4 view(const Camera &camera);
glm::mat4 projection(const Camera &camera, const Bounds &bounds, int w, int h);
Ray ray(const Camera &camera, float x, float y, int w, int h);
void frame(Camera &camera, const Bounds &bounds, int w, int h);
void projectionMode(Camera &camera, bool orthographic);
void preset(Camera &camera, const std::string &name);
void orbit(Camera &camera, float x, float y, bool look = false);
void pan(Camera &camera, float x, float y, int height);
void zoom(Camera &camera, float wheel, const Vec &anchor, float scale);
void move(Camera &camera, const Vec &local, float delta, bool fast);
} // namespace CameraMath
} // namespace Studio
