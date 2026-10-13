#include "CMenu.h"
#include "CUIRenderer.h"
#include <gtest/gtest.h>
#include <stdexcept>

namespace {
class RecordingRenderer final : public ui::Renderer {
  public:
    int begins = 0, ends = 0, texts = 0;
    bool failDraw = false;
    std::vector<unsigned> textures;
    std::vector<std::optional<ui::Rect>> clips;
    ui::Surface begin() override {
        ++begins;
        return {2, 480};
    }
    void end() noexcept override { ++ends; }
    void clip(std::optional<ui::Rect> rect) override { clips.push_back(rect); }
    void color(const ui::Color &) override {}
    void rectangle(const ui::Rect &, unsigned texture) override {
        if (failDraw) {
            throw std::runtime_error("draw failure");
        }
        textures.push_back(texture);
    }
    void text(const ui::TextLayout &, double, double, double) override {
        ++texts;
    }
};
} // namespace

TEST(MenuRendererCPU, ControlsDrawThroughHostBackendWithoutGLContext) {
    RecordingRenderer renderer;
    CFonts fonts;
    CMenu::Context context;
    context.fonts = &fonts;
    context.renderer = &renderer;
    CMenu menu("backend", &context);
    CMenu::menuOpt option;
    option.type = CMenu::panel;
    option.x = option.y = -.5;
    option.width = option.height = 1;
    option.styleOverrides.borderWidth = 0;
    menu.addOpt(option);
    option.type = CMenu::image;
    option.texture = 42;
    menu.addOpt(option);
    option.type = CMenu::table_row;
    option.cells = {"song", "artist"};
    menu.tableColumns = {.5, .5};
    menu.addOpt(option);
    // A regular label after clipped cells must clear the local clipping.
    option.type = CMenu::static_text;
    option.text = "label";
    menu.addOpt(option);

    menu.render();

    EXPECT_EQ(renderer.begins, 1);
    EXPECT_EQ(renderer.ends, 1);
    EXPECT_EQ(renderer.textures, (std::vector<unsigned>{0, 42}));
    EXPECT_EQ(renderer.texts, 3);
    ASSERT_GE(renderer.clips.size(), 3U);
    EXPECT_TRUE(renderer.clips[renderer.clips.size() - 2].has_value());
    EXPECT_FALSE(renderer.clips.back().has_value());
}

TEST(MenuRendererCPU, DrawFailureStillClosesPass) {
    RecordingRenderer renderer;
    renderer.failDraw = true;
    CFonts fonts;
    CMenu::Context context;
    context.fonts = &fonts;
    context.renderer = &renderer;
    CMenu menu("failure", &context);
    CMenu::menuOpt option;
    option.type = CMenu::panel;
    option.width = option.height = 1;
    menu.addOpt(option);

    EXPECT_THROW(menu.render(), std::runtime_error);
    EXPECT_EQ(renderer.begins, 1);
    EXPECT_EQ(renderer.ends, 1);
}
