#pragma once
#include "CUIDrawTypes.h"
#include "CUIStyle.h"
#include <optional>

namespace ui {
struct Surface {
    double aspect = 1;
    int pixelHeight = 1;
};

// Synchronous UI drawing on the render thread. Coordinates span [-aspect,
// aspect] horizontally and [-1, 1] vertically, with Y increasing upwards.
// Layouts and textures remain owned by the caller; implementations must consume
// them before returning. The interface deliberately contains no graphics API
// types.
class Renderer {
  public:
    virtual ~Renderer() = default;
    // Begin/end are balanced, including nested passes. Begin establishes an
    // unclipped UI pass inside any clipping inherited from its caller.
    virtual Surface begin() = 0;
    virtual void end() noexcept = 0;
    // Replaces the local clip, intersecting the caller's clip. Null removes
    // only the local clip; it must never remove clipping inherited from the
    // caller.
    virtual void clip(std::optional<Rect>) = 0;
    virtual void color(const Color &) = 0;
    // Zero texture is the existing solid rectangle path.
    virtual void rectangle(const Rect &, unsigned texture = 0) = 0;
    virtual void text(const TextLayout &, double x, double y, double size) = 0;
};

class RenderPass {
    Renderer &renderer;

  public:
    const Surface surface;
    explicit RenderPass(Renderer &r) : renderer(r), surface(r.begin()) {}
    ~RenderPass() { renderer.end(); }
    RenderPass(const RenderPass &) = delete;
    RenderPass &operator=(const RenderPass &) = delete;
};
} // namespace ui
