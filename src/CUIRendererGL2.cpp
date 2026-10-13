#include "CEngine.h"
#include "CFonts.h"
#include "CUIRenderer.h"
#include <GL/glew.h>
#include <algorithm>
#include <array>
#include <vector>

namespace {
// Compatibility backend. Preserve the pre-composition menu/cursor handoff:
// host supplies fixed-function rendering, enabled texturing and CPU client
// arrays. Restore the historical matrices/server groups, but leave texture
// bindings and client-array pointers from the final 2D draw for legacy
// consumers. In particular, do not toggle texturing for solid (texture zero)
// rectangles.
class GL2UIRenderer final : public ui::Renderer {
    struct State {
        GLint matrixMode = GL_MODELVIEW;
        std::array<GLint, 4> viewport{};
        std::array<GLint, 4> scissor{};
        bool hadScissor = false;
        double aspect = 1;
    };
    std::vector<State> states;
    CFonts textDrawer;

  public:
    ui::Surface begin() override {
        // Allocate before modifying GL state, so allocation failure is
        // harmless.
        auto &state = states.emplace_back();
        glGetIntegerv(GL_MATRIX_MODE, &state.matrixMode);
        glGetIntegerv(GL_VIEWPORT, state.viewport.data());
        glGetIntegerv(GL_SCISSOR_BOX, state.scissor.data());
        state.hadScissor = glIsEnabled(GL_SCISSOR_TEST) != 0;
        state.aspect =
            state.viewport[3] > 0
                ? static_cast<double>(state.viewport[2]) / state.viewport[3]
                : 1;

        glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT |
                     GL_SCISSOR_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(-state.aspect, state.aspect, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        return {state.aspect, state.viewport[3]};
    }

    void end() noexcept override {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(static_cast<GLenum>(states.back().matrixMode));
        glPopAttrib();
        states.pop_back();
        CEngine::engine().invalidateRenderBindings();
    }

    void clip(std::optional<ui::Rect> rect) override {
        const auto &state = states.back();
        if (!rect) {
            if (state.hadScissor) {
                glEnable(GL_SCISSOR_TEST);
                glScissor(state.scissor[0], state.scissor[1], state.scissor[2],
                          state.scissor[3]);
            } else {
                glDisable(GL_SCISSOR_TEST);
            }
            return;
        }

        const auto &vp = state.viewport;
        const auto &r = *rect;
        int left =
            vp[0] + static_cast<int>((r.left / state.aspect + 1) * vp[2] / 2);
        int bottom = vp[1] + static_cast<int>((r.bottom + 1) * vp[3] / 2);
        int right =
            vp[0] + static_cast<int>((r.right / state.aspect + 1) * vp[2] / 2);
        int top = vp[1] + static_cast<int>((r.top + 1) * vp[3] / 2);
        if (state.hadScissor) {
            left = std::max(left, state.scissor[0]);
            bottom = std::max(bottom, state.scissor[1]);
            right = std::min(right, state.scissor[0] + state.scissor[2]);
            top = std::min(top, state.scissor[1] + state.scissor[3]);
        }

        glEnable(GL_SCISSOR_TEST);
        glScissor(left, bottom, std::max(0, right - left),
                  std::max(0, top - bottom));
    }

    void color(const ui::Color &c) override {
        CEngine::engine().setColor(c.r, c.g, c.b, c.a);
    }

    void rectangle(const ui::Rect &r, unsigned texture) override {
        CEngine::RenderDoubleStruct q{};
        q.x1 = q.x4 = r.left;
        q.x2 = q.x3 = r.right;
        q.y1 = q.y2 = r.top;
        q.y3 = q.y4 = r.bottom;
        q.TextureX1 = 0;
        q.TextureX2 = 1;
        q.TextureY1 = 1;
        q.TextureY2 = 0;
        q.Text = texture;
        CEngine::engine().Render2DQuad(q);
    }

    void text(const ui::TextLayout &layout, double x, double y,
              double size) override {
        textDrawer.drawLayout(layout, x, y, size);
    }
};
} // namespace

ui::Renderer &CEngine::uiRenderer() {
    static GL2UIRenderer renderer;
    return renderer;
}
