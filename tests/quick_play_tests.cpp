#include "CUIDefinition.h"
#include "ui/QuickPlayView.h"
#include <chrono>
#include <gtest/gtest.h>

namespace {
class QuickPlayCPU : public ::testing::Test {
  protected:
    CFonts fonts;
    CMenu::Context context;
    CMenu root{"selectSongMenu", &context};
    std::unique_ptr<ui::QuickPlayView> view;
    std::shared_ptr<songs::LibrarySnapshot> snapshot =
        std::make_shared<songs::LibrarySnapshot>();
    void SetUp() override {
        context.fonts = &fonts;
        context.aspect = 1.78;
        context.mouseX = context.mouseY = 10;
        for (int i = 0; i < 4; ++i) {
            songs::SongRecord song;
            song.path = std::array<std::string, 4>{
                "Pack/A/song", "Pack/A/deep/song", "Pack/B/song",
                "Other/song"}[static_cast<size_t>(i)];
            song.title = std::array<std::string, 4>{
                "Água", "Battery", "Céu", "Dance"}[static_cast<size_t>(i)];
            song.artist = i < 2 ? "Metallica" : "Mindflow";
            song.charter = i % 2 == 0 ? "Autor A" : "Autor B";
            song.available = i != 3;
            song.duration = 200.0 - i * 30;
            song.instruments = {"[ExpertSingle]", "[HardSingle]"};
            song.stems = {"song.ogg", "guitar.ogg"};
            snapshot->songs.push_back(song);
        }
        snapshot->contentRevision = 1;
        view = std::make_unique<ui::QuickPlayView>(
            root, context.aspect, ui::QuickPlayView::Presentation{},
            ui::QuickPlayView::Cover{}, &context);
        view->synchronize(snapshot);
        root.update();
    }
    CMenu &panel(const std::string &name) {
        CMenu *result = nullptr;
        root.visitInterfaces([&](CMenu &child) {
            if (child.getName() == name) {
                result = &child;
            }
        });
        if (!result) {
            throw std::runtime_error("Missing panel " + name);
        }
        return *result;
    }
    void activate(CMenu &menu, const std::string &id) {
        ASSERT_NE(menu.findOption(id), nullptr) << id;
        menu.findOption(id)->status |= 3;
        view->interact();
        view->synchronize(snapshot);
        root.update();
    }
    std::vector<std::string> titles() {
        std::vector<std::string> values;
        for (const auto &o : panel("quickPlayResults").options) {
            if (o.type == CMenu::table_row) {
                values.push_back(o.cells[0]);
            }
        }
        return values;
    }
    void key(int key, int mods = 0) {
        CControls::MenuEvent event;
        event.kind = CControls::MenuEvent::Key;
        event.key = key;
        event.action = GLFW_PRESS;
        event.mods = mods;
        context.events.push_back(event);
        root.update();
    }
};

TEST_F(QuickPlayCPU, RecursiveFoldersIncludeDescendantsAndCollapse) {
    auto &nav = panel("quickPlayNav0");
    EXPECT_NE(nav.findOption("group:Pack"), nullptr);
    EXPECT_EQ(nav.findOption("group:Pack/A"), nullptr);
    activate(nav, "group:Pack");
    EXPECT_EQ(titles().size(), 3u);
    ASSERT_NE(nav.findOption("group:Pack/A"), nullptr);
    activate(nav, "group:Pack/A");
    EXPECT_EQ(titles(), (std::vector<std::string>{"Água", "Battery"}));
    ASSERT_NE(nav.findOption("group:Pack/A/deep"), nullptr);
    activate(nav, "group:Pack/A/deep");
    EXPECT_EQ(titles(), (std::vector<std::string>{"Battery"}));
    activate(nav, "group:Pack");
    EXPECT_EQ(nav.findOption("group:Pack/A"), nullptr);
    EXPECT_EQ(titles().size(), 3u);
}

TEST_F(QuickPlayCPU, ArtistCharterAndTitleGroupingKeepQueryAndSelection) {
    activate(root, "tab1");
    activate(panel("quickPlayNav1"), "group:Metallica");
    EXPECT_EQ(titles().size(), 2u);
    activate(root, "tab3");
    activate(panel("quickPlayNav3"), "group:Autor A");
    EXPECT_EQ(titles(), (std::vector<std::string>{"Água", "Céu"}));
    activate(root, "tab2");
    activate(panel("quickPlayNav2"), "group:C");
    EXPECT_EQ(titles(), (std::vector<std::string>{"Céu"}));
    activate(root, "tab1");
    EXPECT_EQ(titles().size(), 2u);
    auto &controls = panel("quickPlayControls");
    controls.findOption("search")->preText = "AGUA";
    view->interact();
    view->synchronize(snapshot);
    root.update();
    EXPECT_EQ(titles(), (std::vector<std::string>{"Água"}));
    activate(root, "tab3");
    EXPECT_EQ(controls.findOption("search")->preText, "AGUA");
    EXPECT_EQ(titles(), (std::vector<std::string>{"Água"}));
}

TEST_F(QuickPlayCPU, SortingFilteringAndEmptyResultsClearPreview) {
    auto &controls = panel("quickPlayControls");
    for (int i = 0; i < 3; ++i) {
        activate(controls, "sort");
    }
    EXPECT_EQ(titles().front(), "Dance");
    activate(controls, "filter");
    EXPECT_EQ(titles().size(), 3u);
    EXPECT_EQ(titles().front(), "Céu");
    controls.findOption("search")->preText = "missing";
    view->interact();
    view->synchronize(snapshot);
    EXPECT_TRUE(titles().empty());
    EXPECT_NE(panel("quickPlayPreview").findOption("empty"), nullptr);
    EXPECT_EQ(panel("quickPlayPreview").findOption("cover"), nullptr);
}

TEST_F(QuickPlayCPU, KeyboardPreviewRefreshAndModalPrecedence) {
    auto &results = panel("quickPlayResults");
    ASSERT_EQ(root.focusedInterface(), &results);
    key(GLFW_KEY_DOWN);
    EXPECT_TRUE(view->interact().empty());
    EXPECT_EQ(panel("quickPlayPreview").findOption("title")->text, "Battery");
    key(GLFW_KEY_F5, GLFW_MOD_CONTROL);
    view->interact();
    EXPECT_EQ(view->takeRefresh(), songs::ScanMode::Full);
    EXPECT_EQ(root.focusedInterface(), &results);
    key(GLFW_KEY_ENTER);
    EXPECT_EQ(view->interact(), "Pack/A/deep/song");
    key(GLFW_KEY_TAB);
    EXPECT_NE(root.focusedInterface(), &results);
    key(GLFW_KEY_TAB, GLFW_MOD_SHIFT);
    EXPECT_EQ(root.focusedInterface(), &results);
    CMenu modal("modal", &context);
    modal.modal = true;
    CMenu::menuOpt close;
    close.type = CMenu::textbtn;
    close.text = "Close";
    modal.addOpt(close);
    root.pushUserInterface(modal);
    key(GLFW_KEY_F5);
    view->interact();
    EXPECT_FALSE(view->takeRefresh());
}

TEST_F(QuickPlayCPU, PublicationsPreserveStableSelectionAndUseUpdatedMetadata) {
    key(GLFW_KEY_DOWN);
    view->interact();
    auto next = std::make_shared<songs::LibrarySnapshot>(*snapshot);
    next->contentRevision++;
    next->songs[1].title = "Battery remastered";
    snapshot = next;
    view->synchronize(snapshot);
    EXPECT_EQ(panel("quickPlayPreview").findOption("title")->text,
              "Battery remastered");
    auto &results = panel("quickPlayResults");
    EXPECT_EQ(
        results.options[static_cast<size_t>(results.focusedOption())].text,
        "Battery remastered");
    next = std::make_shared<songs::LibrarySnapshot>(*snapshot);
    next->contentRevision++;
    next->songs.erase(next->songs.begin() + 1);
    snapshot = next;
    view->synchronize(snapshot);
    EXPECT_EQ(panel("quickPlayPreview").findOption("title")->text, "Água");
}

TEST_F(QuickPlayCPU, MetadataOnlyPublicationReusesRowAndCellCache) {
    auto &results = panel("quickPlayResults");
    auto *row = results.findOption("song:Pack/A/song");
    ASSERT_NE(row, nullptr);
    row->cellLayouts.resize(5);
    row->cellLayouts[0].text = "shaped";
    auto next = std::make_shared<songs::LibrarySnapshot>(*snapshot);
    ++next->contentRevision;
    next->songs[0].charter = "New charter";
    snapshot = next;
    view->synchronize(snapshot);
    EXPECT_EQ(results.findOption("song:Pack/A/song"), row);
    EXPECT_EQ(row->cells[2], "New charter");
    EXPECT_EQ(row->cellLayouts[0].text, "shaped");
}

TEST_F(QuickPlayCPU,
       SearchHandlesMarkupAccentsAndMalformedUtf8WithoutChangingRecords) {
    auto next = std::make_shared<songs::LibrarySnapshot>(*snapshot);
    ++next->contentRevision;
    next->songs[0].charter = "<color=#ff8800><b>João</b></color>";
    next->songs[1].title = std::string("Bad ") + char(0xff);
    snapshot = next;
    view->synchronize(snapshot);
    panel("quickPlayControls").findOption("search")->preText = "JOAO";
    view->interact();
    view->synchronize(snapshot);
    EXPECT_EQ(titles(), (std::vector<std::string>{"Água"}));
    EXPECT_EQ(
        panel("quickPlayResults").findOption("song:Pack/A/song")->cells[2],
        "João");
    EXPECT_EQ(snapshot->songs[0].charter, "<color=#ff8800><b>João</b></color>");
}

TEST_F(QuickPlayCPU, LargeLibraryKeepsScrollPerTabAndProgressDoesNotRebuild) {
    auto next = std::make_shared<songs::LibrarySnapshot>();
    next->contentRevision = 2;
    for (int i = 0; i < 10000; ++i) {
        auto song = snapshot->songs[0];
        song.path = "Pack/" + std::to_string(i);
        song.title = "Song " + std::to_string(i);
        next->songs.push_back(std::move(song));
    }
    snapshot = next;
    const auto start = std::chrono::steady_clock::now();
    view->synchronize(snapshot);
    const auto milliseconds = std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - start)
                                  .count();
    RecordProperty("build_10000_ms", milliseconds);
    auto &results = panel("quickPlayResults");
    results.scrollOffset = 2.0;
    activate(root, "tab1");
    activate(root, "tab0");
    EXPECT_DOUBLE_EQ(results.scrollOffset, 2.0);
    auto *row = results.findOption("song:Pack/0");
    next = std::make_shared<songs::LibrarySnapshot>(*snapshot);
    next->progress.examined = 123;
    snapshot = next;
    view->synchronize(snapshot);
    EXPECT_EQ(results.findOption("song:Pack/0"), row);
    EXPECT_DOUBLE_EQ(results.scrollOffset, 2.0);
}

TEST_F(QuickPlayCPU, ScrollTargetsCursorPanelAndEditingDoesNotTriggerRandom) {
    auto &preview = panel("quickPlayPreview");
    const auto bounds = preview.getPanelBounds();
    context.mouseX = (bounds.left + bounds.right) / 2;
    context.mouseY = 0;
    CControls::MenuEvent event;
    event.kind = CControls::MenuEvent::Scroll;
    event.y = -2;
    context.events.push_back(event);
    root.update();
    EXPECT_EQ(root.focusedInterface(), &panel("quickPlayResults"));
    // Even when the preview fits, routing must not scroll the results.
    EXPECT_DOUBLE_EQ(panel("quickPlayResults").scrollOffset, 0);
    auto &controls = panel("quickPlayControls");
    ASSERT_TRUE(root.focusInterface(controls));
    const auto it = std::find_if(
        controls.options.begin(), controls.options.end(),
        [](const CMenu::menuOpt &o) { return o.type == CMenu::text_input; });
    controls.select(
        static_cast<int>(std::distance(controls.options.begin(), it)));
    key(GLFW_KEY_ENTER);
    key(GLFW_KEY_R);
    EXPECT_TRUE(view->interact().empty());
    EXPECT_EQ(root.focusedInterface(), &controls);
}

TEST_F(QuickPlayCPU, PreviewWrapsLongNamesWithoutOverlappingRows) {
    auto next = std::make_shared<songs::LibrarySnapshot>(*snapshot);
    ++next->contentRevision;
    next->songs[0].title =
        "A very long song title with Árvore and more words that need wrapping";
    next->songs[0].artist =
        "A very long artist name that must remain completely readable";
    snapshot = next;
    view->synchronize(snapshot);
    auto &preview = panel("quickPlayPreview");
    ASSERT_NE(preview.findOption("title_line1"), nullptr);
    double previousBottom = 1;
    for (auto &option : preview.options) {
        if (option.type != CMenu::table_row) {
            continue;
        }
        CMenu::menuOpt measure;
        measure.type = CMenu::static_text;
        measure.text = option.cells[0];
        preview.ensureLayout(measure);
        EXPECT_LE(measure.layout.width() *
                      preview.resolvedOptionStyle(option).textSize,
                  preview.tableColumns[0]);
        EXPECT_LE(option.y + option.height, previousBottom + 1e-9);
        previousBottom = option.y;
    }
}

TEST_F(QuickPlayCPU, MouseHighlightsWithoutLaunchingAndEnterConfirms) {
    auto &results = panel("quickPlayResults");
    const auto it = std::find_if(
        results.options.begin(), results.options.end(),
        [](const CMenu::menuOpt &o) {
            return o.optionName == "quickPlayResults.song:Pack/A/deep/song";
        });
    ASSERT_NE(it, results.options.end());
    const auto bounds = results.optionRect(
        static_cast<size_t>(std::distance(results.options.begin(), it)));
    context.mouseX = (bounds.left + bounds.right) / 2;
    context.mouseY = (bounds.bottom + bounds.top) / 2;
    for (int action : {GLFW_PRESS, GLFW_RELEASE}) {
        CControls::MenuEvent event;
        event.kind = CControls::MenuEvent::Mouse;
        event.key = GLFW_MOUSE_BUTTON_LEFT;
        event.action = action;
        event.x = context.mouseX;
        event.y = context.mouseY;
        context.events.push_back(event);
        root.update();
        EXPECT_TRUE(view->interact().empty());
    }
    EXPECT_EQ(panel("quickPlayPreview").findOption("title")->text, "Battery");
    key(GLFW_KEY_ENTER);
    EXPECT_EQ(view->interact(), "Pack/A/deep/song");
}

TEST_F(QuickPlayCPU,
       CoverUsesBorrowedTextureWithOriginalAspectAndClearsOnSelection) {
    view.reset();
    snapshot->songs[0].coverPath = "Pack/A/cover.png";
    int calls = 0;
    view = std::make_unique<ui::QuickPlayView>(
        root, context.aspect, ui::QuickPlayView::Presentation{},
        [&](const std::string &path) {
            EXPECT_EQ(path, "Pack/A/cover.png");
            ++calls;
            return ui::QuickPlayView::CoverImage{17, 400, 200};
        },
        &context);
    view->synchronize(snapshot);
    auto *cover = panel("quickPlayPreview").findOption("cover");
    ASSERT_NE(cover, nullptr);
    EXPECT_EQ(cover->texture, 17u);
    EXPECT_DOUBLE_EQ(cover->width / cover->height, 2);
    view->synchronize(snapshot);
    EXPECT_EQ(calls, 1);
    key(GLFW_KEY_DOWN);
    view->interact();
    EXPECT_EQ(panel("quickPlayPreview").findOption("cover")->texture, 0u);
}

TEST_F(QuickPlayCPU, DeclarativeRowsSupportEllipsisAndSelectionOnlyClicks) {
    ui::MenuDefinition definition;
    definition.id = "declared";
    definition.tableColumns = {.5};
    ui::OptionDefinition option;
    option.id = "song";
    option.type = CMenu::table_row;
    option.cells = {"Long song name"};
    option.ellipsizeCells = true;
    option.activateOnClick = false;
    definition.options.push_back(option);
    const auto file =
        std::filesystem::temp_directory_path() /
        ("gpp-quick-play-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".lua");
    ui::writeMenus(file, {definition});
    const auto definitions = ui::readMenus(file);
    std::filesystem::remove(file);
    auto menu = ui::instantiate(definitions.at(0), &context);
    ASSERT_EQ(menu->options.size(), 1u);
    EXPECT_TRUE(menu->options[0].ellipsizeCells);
    EXPECT_FALSE(menu->options[0].activateOnClick);
}

TEST_F(QuickPlayCPU, InitialBackgroundPublicationFocusesResultsWhenReady) {
    view.reset();
    view = std::make_unique<ui::QuickPlayView>(
        root, context.aspect, ui::QuickPlayView::Presentation{},
        ui::QuickPlayView::Cover{}, &context);
    view->synchronize(std::make_shared<songs::LibrarySnapshot>());
    root.update();
    view->synchronize(snapshot);
    EXPECT_EQ(root.focusedInterface(), &panel("quickPlayResults"));
    key(GLFW_KEY_ENTER);
    EXPECT_EQ(view->interact(), "Pack/A/song");
}
} // namespace
