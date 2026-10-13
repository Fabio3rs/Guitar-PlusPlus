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
    ui::Color currentColor;
    std::vector<ui::Color> rectangleColors;
    std::vector<std::optional<ui::Rect>> clips;
    std::vector<ui::Rect> textBounds;
    ui::Surface begin() override {
        ++begins;
        return {2, 480};
    }
    void end() noexcept override { ++ends; }
    void clip(std::optional<ui::Rect> rect) override { clips.push_back(rect); }
    void color(const ui::Color &value) override { currentColor = value; }
    void rectangle(const ui::Rect &, unsigned texture) override {
        if (failDraw) {
            throw std::runtime_error("draw failure");
        }
        textures.push_back(texture);
        rectangleColors.push_back(currentColor);
    }
    void text(const ui::TextLayout &layout, double x, double y,
              double scale) override {
        ++texts;
        textBounds.push_back(
            {x + layout.left * scale, y + layout.bottom * scale,
             x + layout.right * scale, y + layout.top * scale});
    }
};
} // namespace

TEST(MenuRendererCPU, SizedButtonCentersLabelWithinItsBounds) {
    RecordingRenderer renderer;
    CFonts fonts;
    CMenu::Context context;
    context.fonts = &fonts;
    context.renderer = &renderer;
    CMenu menu("centered", &context);
    CMenu::menuOpt button;
    button.type = CMenu::textbtn;
    button.text = "Ordem: Duração";
    button.x = -.4;
    button.y = .2;
    button.width = .7;
    button.height = .1;
    menu.addOpt(button);
    menu.render();
    ASSERT_EQ(renderer.textBounds.size(), 1U);
    const auto &text = renderer.textBounds.front();
    EXPECT_NEAR((text.left + text.right) / 2, -.05, 1e-9);
    EXPECT_NEAR((text.bottom + text.top) / 2, .25, 1e-9);
}

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

TEST(MenuRendererCPU, TabSelectionDoesNotHideFixedPanelsOrOpenWindows) {
    RecordingRenderer renderer;
    CFonts fonts;
    CMenu::Context context;
    context.fonts = &fonts;
    context.renderer = &renderer;
    CMenu root("root", &context), model("model", &context);
    CMenu::menuOpt image;
    image.type = CMenu::image;
    image.width = image.height = .5;
    image.texture = 1;
    root.addOpt(image);
    model.addOpt(image);
    auto attach = [&](unsigned texture, bool window, CMenu &owner) -> CMenu & {
        model.options[0].texture = texture;
        const int id =
            window ? owner.pushUserInterface(model) : owner.pushPanel(model);
        return *CMenu::getUiAt(static_cast<size_t>(id)).m;
    };
    auto &fixed = attach(2, false, root);
    auto &first = attach(3, false, root);
    auto &second = attach(4, false, root);
    auto &window = attach(5, true, root);
    model.modal = true;
    auto &modal = attach(6, true, second);
    EXPECT_FALSE(window.setTabSelected(false));
    EXPECT_FALSE(modal.setTabSelected(false));
    second.setTabSelected(false);
    root.render();
    CMenu::renderUiList();
    EXPECT_EQ(renderer.textures, (std::vector<unsigned>{1, 2, 3, 5, 6}));
    renderer.textures.clear();
    first.setTabSelected(false);
    second.setTabSelected(true);
    root.render();
    CMenu::renderUiList();
    EXPECT_EQ(renderer.textures, (std::vector<unsigned>{1, 2, 4, 5, 6}));
    (void)fixed;
}

TEST(MenuRendererCPU, EllipsisKeepsFullUtf8TextAndRespondsToColumnResize) {
    RecordingRenderer renderer;
    CFonts fonts;
    CMenu::Context context;
    context.fonts = &fonts;
    context.renderer = &renderer;
    CMenu menu("ellipsis", &context);
    menu.tableColumns = {.3};
    CMenu::menuOpt row;
    row.type = CMenu::table_row;
    row.cells = {"Canção longa com vários caracteres"};
    row.ellipsizeCells = true;
    menu.addOpt(row);
    menu.render();
    auto &cache = menu.options[0].cellLayouts[0];
    const auto fullWidth = cache.layout.width();
    EXPECT_EQ(cache.text, row.cells[0]);
    EXPECT_GT(cache.elidedWidth, 0);
    EXPECT_LE(cache.elidedLayout.width(), cache.elidedWidth);
    EXPECT_LT(cache.elidedLayout.width(), fullWidth);
    const double narrow = cache.elidedWidth;
    menu.tableColumns = {.5};
    menu.render();
    EXPECT_GT(cache.elidedWidth, narrow);
    EXPECT_EQ(cache.layout.width(), fullWidth);
    menu.options[0].cells[0] = "Other long content with different width";
    menu.render();
    EXPECT_EQ(cache.text, menu.options[0].cells[0]);
    EXPECT_LE(cache.elidedLayout.width(), cache.elidedWidth);
    menu.tableColumns = {.001};
    menu.render();
    EXPECT_TRUE(cache.elidedLayout.glyphs.empty());
}

TEST(MenuRendererCPU, FocusUsesDistinctFillWhileActiveSelectionPersists) {
    RecordingRenderer renderer;
    CFonts fonts;
    CMenu::Context context;
    context.fonts = &fonts;
    context.renderer = &renderer;
    CMenu menu("tabs", &context);
    menu.styleOverrides.selection = ui::Color{1, .4, 0, 1};
    menu.styleOverrides.focus = ui::Color{.6, .7, .8, 1};
    menu.styleOverrides.focusBackground = ui::Color{0, 0, 1, .1};
    CMenu::menuOpt tab;
    tab.type = CMenu::textbtn;
    tab.text = "Pastas";
    tab.width = .4;
    tab.height = .1;
    tab.active = true;
    menu.addOpt(tab);
    tab.text = "Música";
    tab.x = .5;
    tab.active = false;
    menu.addOpt(tab);
    menu.select(1);
    menu.render();
    ASSERT_GE(renderer.rectangleColors.size(), 6u);
    EXPECT_DOUBLE_EQ(renderer.rectangleColors[0].r, 1);
    EXPECT_DOUBLE_EQ(renderer.rectangleColors[0].a, 1);
    EXPECT_DOUBLE_EQ(renderer.rectangleColors[1].b, 1);
    EXPECT_DOUBLE_EQ(renderer.rectangleColors[1].a, .1);
    EXPECT_DOUBLE_EQ(renderer.rectangleColors[2].r, .6);
    EXPECT_TRUE(menu.options[0].active);
    EXPECT_FALSE(menu.options[1].active);
}
