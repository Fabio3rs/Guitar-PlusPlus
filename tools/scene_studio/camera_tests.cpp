#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

using namespace Studio;
using namespace CameraMath;

namespace {
glm::vec3 screen(const Camera &camera, const Bounds &bounds, Vec point, int w,
                 int h) {
    const auto clip = projection(camera, bounds, w, h) * view(camera) *
                      glm::vec4(vector(point), 1);
    return glm::vec3(clip) / clip.w;
}

void expectVec(const Vec &a, const Vec &b, float tolerance = .0001f) {
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_NEAR(a[i], b[i], tolerance);
    }
}
} // namespace

TEST(Camera, FitsAllCornersAtDifferentScalesAspectsAndViews) {
    for (float size : {.01f, 1.f, 1000.f}) {
        const Bounds bounds{{20 * size, -size, 7 * size},
                            {22 * size, 4 * size, 10 * size},
                            true};
        for (auto dimensions : {std::pair<int, int>{160, 900}, {1920, 1080}}) {
            for (const char *name :
                 {"front", "back", "left", "right", "top", "bottom"}) {
                for (bool ortho : {false, true}) {
                    Camera camera;
                    preset(camera, name);
                    projectionMode(camera, ortho);
                    frame(camera, bounds, dimensions.first, dimensions.second);
                    for (int i = 0; i < 8; ++i) {
                        const Vec point{
                            i & 1 ? bounds.maximum[0] : bounds.minimum[0],
                            i & 2 ? bounds.maximum[1] : bounds.minimum[1],
                            i & 4 ? bounds.maximum[2] : bounds.minimum[2]};
                        const auto p =
                            screen(camera, bounds, point, dimensions.first,
                                   dimensions.second);
                        EXPECT_LE(std::abs(p.x), 1.f) << name;
                        EXPECT_LE(std::abs(p.y), 1.f) << name;
                        EXPECT_LE(std::abs(p.z), 1.f) << name;
                    }
                }
            }
        }
    }
}

TEST(Camera, CursorZoomKeepsAnchorAndReversesAwayFromOrigin) {
    Bounds bounds{{6, 1, 2}, {10, 5, 7}, true};
    for (bool ortho : {false, true}) {
        Camera camera;
        camera.eye = {7, 2, 13};
        camera.target = {7, 2, 3};
        projectionMode(camera, ortho);
        const Camera before = camera;
        const Vec anchor{8, 3, 4};
        const auto original = screen(camera, bounds, anchor, 900, 600);
        zoom(camera, 3, anchor, bounds.scale());
        const auto after = screen(camera, bounds, anchor, 900, 600);
        EXPECT_NEAR(original.x, after.x, .00001f);
        EXPECT_NEAR(original.y, after.y, .00001f);
        zoom(camera, -3, anchor, bounds.scale());
        expectVec(camera.eye, before.eye);
        expectVec(camera.target, before.target);
    }
}

TEST(Camera, ZoomLimitsDoNotCollapseCameraAndAllowReversal) {
    Camera camera;
    camera.eye = {7, 2, 13};
    camera.target = {7, 2, 3};
    for (int i = 0; i < 100; ++i) {
        zoom(camera, 10, camera.target, 10);
    }
    float distance = glm::length(vector(camera.eye) - vector(camera.target));
    EXPECT_GT(distance, .0001f);
    zoom(camera, -10, camera.target, 10);
    EXPECT_GT(glm::length(vector(camera.eye) - vector(camera.target)),
              distance);
    zoom(camera, -1000, camera.target, 10);
    EXPECT_LE(glm::length(vector(camera.eye) - vector(camera.target)), 100001);
}

TEST(Camera, FreeMovementPassesOldPivotAndIsTimeBased) {
    Camera camera;
    camera.free = true;
    camera.eye = {7, 2, 4};
    camera.target = {7, 2, 3};
    camera.speed = 4;
    auto slow = camera, fast = camera;
    for (int i = 0; i < 10; ++i) {
        move(slow, {0, 0, 1}, .1f, false);
    }
    for (int i = 0; i < 200; ++i) {
        move(fast, {0, 0, 1}, .005f, false);
    }
    expectVec(slow.eye, fast.eye);
    EXPECT_LT(slow.eye[2], camera.target[2]);
    auto wheel = camera;
    zoom(wheel, 10, camera.target, 5);
    expectVec(wheel.eye, slow.eye);
    auto paused = camera, bounded = camera;
    move(paused, {1, 0, 0}, 10, true);
    move(bounded, {1, 0, 0}, .1f, true);
    expectVec(paused.eye, bounded.eye);
}

TEST(Camera, OrthographicSwitchPreservesProjectedSizeAndTopViewCanOrbit) {
    Camera camera;
    Bounds bounds{{-1, -1, -1}, {1, 1, 1}, true};
    preset(camera, "front");
    frame(camera, bounds, 800, 600);
    const auto before = screen(camera, bounds, {1, 1, 0}, 800, 600);
    const auto eye = camera.eye;
    projectionMode(camera, true);
    const auto after = screen(camera, bounds, {1, 1, 0}, 800, 600);
    EXPECT_NEAR(before.x, after.x, .00001f);
    EXPECT_NEAR(before.y, after.y, .00001f);
    projectionMode(camera, false);
    expectVec(camera.eye, eye);
    preset(camera, "top");
    orbit(camera, 20, 30);
    const auto r = ray(camera, 400, 300, 800, 600);
    EXPECT_TRUE(std::isfinite(r.direction[0]));
    EXPECT_NEAR(glm::dot(vector(camera.up), vector(r.direction)), 0, .0001f);
}

TEST(Camera, PanUsesPixelsAndProjection) {
    Bounds bounds{{-1, -1, -1}, {1, 1, 1}, true};
    for (bool ortho : {false, true}) {
        Camera camera;
        preset(camera, "front");
        projectionMode(camera, ortho);
        const auto before = screen(camera, bounds, {0, 0, 0}, 800, 600);
        pan(camera, 50, 30, 600);
        const auto after = screen(camera, bounds, {0, 0, 0}, 800, 600);
        EXPECT_NEAR((after.x - before.x) * 400, 50, .001);
        EXPECT_NEAR((after.y - before.y) * -300, 30, .001);
    }
}

TEST(Camera, ChangingFocusScaleDoesNotReverseZoomOrClipCloseDetails) {
    Camera camera;
    camera.eye = {0, 0, .01f};
    camera.target = {0, 0, 0};
    zoom(camera, 1, camera.target, 1.e5f);
    EXPECT_LE(camera.eye[2], .01f);
    camera.eye = {0, 0, 10000};
    zoom(camera, -1, camera.target, .01f);
    EXPECT_GE(camera.eye[2], 10000);
    camera.eye = {0, 0, .01f};
    const Bounds largeScene{
        {-10000, -10000, -10000}, {10000, 10000, 10000}, true};
    const auto clip = screen(camera, largeScene, {0, 0, 0}, 800, 600);
    EXPECT_LE(std::abs(clip.z), 1);
}

TEST(Camera, DistantTinyBoundsHaveFiniteProjection) {
    Camera camera;
    camera.eye = {0, 0, 1.e6f};
    camera.target = {0, 0, 0};
    const Bounds bounds{{-.001f, -.001f, -.001f}, {.001f, .001f, .001f}, true};
    for (bool ortho : {false, true}) {
        camera.orthographic = ortho;
        const auto matrix = projection(camera, bounds, 800, 600);
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                EXPECT_TRUE(std::isfinite(matrix[col][row]));
            }
        }
    }
}
