#pragma once
#include "CMenu.h"
#include "SongLibrary.h"
#include <array>
#include <set>

namespace ui {
// UI-thread composition over immutable library snapshots. No filesystem or GL
// access: the host supplies presentation and borrowed cover textures.
class QuickPlayView {
  public:
    using Presentation = std::function<void(CMenu &)>;
    struct CoverImage {
        unsigned texture = 0;
        int width = 0, height = 0;
    };
    using Cover = std::function<CoverImage(const std::string &)>;
    QuickPlayView(CMenu &root, double aspect, Presentation presentation = {},
                  Cover cover = {}, CMenu::Context *context = nullptr);
    ~QuickPlayView();
    QuickPlayView(const QuickPlayView &) = delete;
    QuickPlayView &operator=(const QuickPlayView &) = delete;
    void setLogo(CoverImage image);
    void setIcons(const std::array<CoverImage, 5> &icons);
    // Before dispatch: apply publications and pending filter/layout changes.
    void synchronize(std::shared_ptr<const songs::LibrarySnapshot> snapshot);
    // After dispatch: observe controls. Returns an explicitly activated song.
    std::string interact();
    std::optional<songs::ScanMode> takeRefresh();

  private:
    struct TabState {
        std::string group, selected;
        double scroll = 0;
    };
    struct Group {
        std::string key, label;
        size_t count = 0, depth = 0;
        bool children = false;
    };
    CMenu &root_;
    CMenu *toolbar_ = nullptr, *results_ = nullptr, *preview_ = nullptr;
    std::array<CMenu *, 4> navigation_{};
    std::array<TabState, 4> states_{};
    std::array<std::vector<Group>, 4> groups_{};
    std::array<std::map<int, std::string>, 4> navigationRows_{};
    std::map<int, size_t> resultRows_;
    std::set<std::string> expanded_;
    std::vector<std::string> searchKeys_;
    std::shared_ptr<const songs::LibrarySnapshot> snapshot_;
    Presentation presentation_;
    Cover cover_;
    std::array<CoverImage, 5> icons_{};
    size_t tab_ = 0;
    int order_ = 0;
    double contentWidth_ = 0;
    bool availableOnly_ = false, dirty_ = true, navigationDirty_ = false,
         previewDirty_ = true;
    std::string query_, previewPath_, coverPath_, lastStatus_;
    std::optional<songs::ScanMode> refresh_;
    void prepare(CMenu &menu);
    void indexGroups();
    void buildNavigation(size_t tab);
    void buildResults();
    void showPreview(const songs::SongRecord *song);
    void selectSong(size_t index);
    std::string groupKey(const songs::SongRecord &song, size_t tab) const;
};
} // namespace ui
