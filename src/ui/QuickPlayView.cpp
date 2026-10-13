#include "QuickPlayView.h"
#include "MetadataText.h"
#include "utf8.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <string_view>
#include <unordered_map>

namespace ui {
namespace {
constexpr std::array<const char *, 4> tabs{"Pastas", "Artista", "Música",
                                           "Charter"};
constexpr std::array<const char *, 4> orders{"Nome", "Artista", "Charter",
                                             "Duração"};

std::string levels(const songs::SongRecord &song) {
    std::string result;
    const std::array<const char *, 4> names{"Easy", "Medium", "Hard", "Expert"};
    for (size_t i = 0; i < names.size(); ++i) {
        const bool present = std::any_of(
            song.instruments.begin(), song.instruments.end(),
            [&](const std::string &part) {
                return part.compare(part.empty() || part.front() != '[' ? 0 : 1,
                                    std::char_traits<char>::length(names[i]),
                                    names[i]) == 0;
            });
        if (present) {
            static constexpr std::array<char, 4> initials{'F', 'M', 'D', 'E'};
            result += initials.at(i);
        }
    }
    return result.empty() ? "--" : result;
}

// Predictable UTF-8 search for Portuguese/Latin titles, independent of the
// process locale. Other codepoints are retained, not split into bytes.
std::string searchKey(const std::string &text) {
    std::string result;
    std::string valid;
    const auto plainText = metadataDisplayText(text);
    utf8::replace_invalid(plainText.begin(), plainText.end(),
                          std::back_inserter(valid));
    auto it = valid.begin();
    while (it != valid.end()) {
        auto c = utf8::next(it, valid.end());
        if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';
        } else if (c >= 0xc0 && c <= 0xde && c != 0xd7) {
            c += 0x20;
        }
        static constexpr std::u32string_view accents =
            U"àáâãäåèéêëìíîïòóôõöùúûüçñ";
        static constexpr std::string_view plain = "aaaaaaeeeeiiiiooooouuuucn";
        const auto pos = accents.find(static_cast<char32_t>(c));
        if (pos != std::u32string_view::npos) {
            c = static_cast<unsigned char>(plain[pos]);
        }
        utf8::append(c, std::back_inserter(result));
    }
    return result;
}

std::string duration(const songs::SongRecord &song) {
    if (!song.duration || !std::isfinite(*song.duration) ||
        *song.duration < 0) {
        return "--:--";
    }
    const auto seconds = static_cast<uint64_t>(*song.duration);
    return (song.durationSource == "chart-estimate" ? "~" : "") +
           std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") +
           std::to_string(seconds % 60);
}

bool within(const std::string &path, const std::string &folder) {
    return folder.empty() || path == folder ||
           (path.size() > folder.size() &&
            path.compare(0, folder.size(), folder) == 0 &&
            path[folder.size()] == '/');
}

CMenu::menuOpt item(const std::string &id, const std::string &text,
                    CMenu::menusOPT type, double x, double y, double width = 0,
                    double height = 0) {
    CMenu::menuOpt option;
    option.optionName = id;
    option.text = text;
    option.type = type;
    option.x = x;
    option.y = y;
    option.width = width;
    option.height = height;
    return option;
}

void background(CMenu &menu, CMenu::Rect rect) {
    auto option = item("surface", "", CMenu::panel, rect.left, rect.bottom,
                       rect.right - rect.left, rect.top - rect.bottom);
    menu.addOpt(option);
}

// Wrap informational text using the actual active atlas metrics. This runs on
// selection/publication changes, not in the per-frame draw path.
std::vector<std::string> wrapText(CMenu &menu, const std::string &text,
                                  double width, double size) {
    std::string remaining;
    const auto plain = metadataDisplayText(text);
    utf8::replace_invalid(plain.begin(), plain.end(),
                          std::back_inserter(remaining));
    CMenu::menuOpt measure;
    measure.type = CMenu::static_text;
    const auto fits = [&](const std::string &value) {
        measure.text = value;
        menu.ensureLayout(measure);
        return measure.layout.width() * size <= width;
    };
    std::vector<std::string> lines;
    while (!remaining.empty()) {
        if (fits(remaining)) {
            lines.push_back(remaining);
            break;
        }
        std::vector<size_t> ends{0};
        auto it = remaining.cbegin();
        while (it != remaining.cend()) {
            utf8::next(it, remaining.cend());
            ends.push_back(
                static_cast<size_t>(std::distance(remaining.cbegin(), it)));
        }
        size_t low = 1, high = ends.size() - 1;
        while (low < high) {
            const size_t middle = low + (high - low + 1) / 2;
            if (fits(remaining.substr(0, ends[middle]))) {
                low = middle;
            } else {
                high = middle - 1;
            }
        }
        size_t cut = ends[low];
        const auto space = remaining.rfind(' ', cut);
        if (space != std::string::npos && space > 0) {
            cut = space;
        }
        lines.push_back(remaining.substr(0, cut));
        remaining.erase(0, cut);
        remaining.erase(0, remaining.find_first_not_of(' '));
    }
    if (lines.empty()) {
        lines.emplace_back();
    }
    return lines;
}

std::string fitLabel(CMenu &menu, const std::string &text, double width,
                     double size = 0) {
    if (size <= 0) {
        size = menu.resolvedStyle().typography.textSize;
    }
    const auto lines = wrapText(menu, text, width, size);
    if (lines.size() == 1) {
        return lines.front();
    }
    CMenu::menuOpt dots;
    dots.type = CMenu::static_text;
    dots.text = "...";
    menu.ensureLayout(dots);
    return wrapText(menu, text, width - dots.layout.width() * size, size)
               .front() +
           "...";
}

bool activated(CMenu &menu, const std::string &id) {
    const auto *option = menu.findOption(id);
    return option && (option->status & 3) == 3;
}

} // namespace

QuickPlayView::QuickPlayView(CMenu &root, double aspect,
                             Presentation presentation, Cover cover,
                             CMenu::Context *context)
    : root_(root), presentation_(std::move(presentation)),
      cover_(std::move(cover)) {
    root_.clearOptions();
    root_.setFlow(CMenu::Flow::Manual, 0, 0);
    root_.viewportEnabled = false;
    const double left = -aspect + .06, right = aspect - .06;
    const double width = right - left;
    contentWidth_ = width;
    const double navRight = left + width * (aspect < 1.5 ? .20 : .22);
    const double previewLeft = right - width * .28;
    const double centerLeft = navRight + .025;
    const double centerRight = previewLeft - .025;
    root_.styleOverrides.textSize = aspect < 1.5 ? .040 : .036;
    root_.styleOverrides.textColor = Color{.89, .92, .94, 1};
    root_.styleOverrides.focus = Color{.60, .73, .80, 1};
    root_.styleOverrides.focusBackground = Color{.26, .36, .42, .18};
    root_.styleOverrides.accent = Color{1, .49, .08, 1};
    root_.styleOverrides.selection = Color{.48, .23, .045, .96};
    root_.styleOverrides.panel = Color{.025, .035, .045, .97};
    root_.styleOverrides.panelBorder = Color{.19, .24, .28, 1};
    root_.styleOverrides.panelBorderWidth = .002;
    auto logo = item("brand", "Guitar++", CMenu::static_text, left, .84);
    logo.styleOverrides.textSize = .12;
    logo.styleOverrides.textColor = Color{1, .49, .08, 1};
    root_.addOpt(logo);
    auto title =
        item("title", "Quick Play", CMenu::static_text, left + .83, .86);
    title.styleOverrides.textSize = .08;
    root_.addOpt(title);
    auto subtitle = item("subtitle", "Escolha sua próxima música",
                         CMenu::static_text, left + .84, .78);
    subtitle.styleOverrides.textColor = Color{.64, .72, .77, 1};
    root_.addOpt(subtitle);
    auto info = item("projectInfo", "open source", CMenu::static_text,
                     right - .75, .89);
    info.styleOverrides.textSize = .035;
    if (width > 3) {
        root_.addOpt(info);
    }
    for (size_t i = 0; i < tabs.size(); ++i) {
        auto option = item("tab" + std::to_string(i), tabs[i], CMenu::textbtn,
                           left + static_cast<double>(i) * .40, .64, .38, .10);
        option.active = i == tab_;
        option.styleOverrides.background = Color{.06, .075, .085, .95};
        root_.addOpt(option);
    }
    background(root_, {left, -.98, right, -.82});
    const std::array<const char *, 5> actionIds{"back", "random", "play",
                                                "refresh", "verify"};
    const std::array<const char *, 5> actionLabels{
        "Esc  Voltar", "R  Aleatória", "Enter  Selecionar", "F5  Atualizar",
        "Ctrl+F5  Verificar tudo"};
    const std::array<double, 5> actionWidths{.15, .17, .23, .18, .27};
    const double actionWidth = std::min(width - .06, 3.4);
    double actionX = left + .03;
    for (size_t i = 0; i < actionIds.size(); ++i) {
        auto action = item(actionIds[i], actionLabels[i], CMenu::textbtn,
                           actionX, -.89, actionWidth * actionWidths[i], .055);
        action.styleOverrides.textSize = aspect < 1.5 ? .037 : .032;
        action.goback = i == 0;
        action.shortcutKey = i == 0   ? GLFW_KEY_ESCAPE
                             : i == 1 ? GLFW_KEY_R
                             : i >= 3 ? GLFW_KEY_F5
                                      : 0;
        action.shortcutMods = i == 4 ? GLFW_MOD_CONTROL : 0;
        root_.addOpt(action);
        actionX += action.width;
    }
    auto status = item("scanStatus", "", CMenu::static_text, left + .03, -.955);
    status.styleOverrides.textSize = aspect < 1.5 ? .036 : .030;
    status.styleOverrides.textColor = Color{.59, .67, .72, 1};
    root_.addOpt(status);

    auto attach = [&](const std::string &name, Rect bounds, bool scroll) {
        CMenu model(name, context);
        model.styleOverrides = root_.styleOverrides;
        model.styleOverrides.rowStep = .080;
        model.styleOverrides.padding = .12;
        model.viewportEnabled = scroll;
        model.viewport = {bounds.left + .015, bounds.bottom + .015,
                          bounds.right - .015, bounds.top - .15};
        model.setFlow(CMenu::Flow::Vertical, bounds.left + .02,
                      bounds.top - .23);
        background(model, bounds);
        const int id = root_.pushPanel(model, bounds);
        return CMenu::getUiAt(static_cast<size_t>(id)).m.get();
    };
    toolbar_ = attach("quickPlayControls", {left, .48, right, .62}, false);
    auto searchLabel = item("searchLabel", "Buscar música, artista, charter",
                            CMenu::static_text, left + .02, .578);
    searchLabel.styleOverrides.textSize = .030;
    searchLabel.styleOverrides.textColor = Color{.64, .72, .77, 1};
    toolbar_->addOpt(searchLabel);
    auto search = item("search", "", CMenu::text_input, left + .03, .505);
    search.preText.clear();
    search.preTextMaxSize = 256;
    search.fieldWidth = width * .55 - .04;
    toolbar_->addOpt(search);
    auto hint = item("searchHint", "Digite para buscar...", CMenu::static_text,
                     left + .035, .505);
    hint.styleOverrides.textColor = Color{.54, .63, .69, 1};
    toolbar_->addOpt(hint);
    toolbar_->styleOverrides.field = Color{.10, .14, .17, 1};
    toolbar_->addOpt(item("sort", "Ordem: Nome", CMenu::textbtn,
                          left + width * .57, .50, width * .23, .074));
    toolbar_->addOpt(item("filter", "Todas", CMenu::textbtn, left + width * .82,
                          .50, width * .18 - .025, .074));
    for (const auto *id : {"sort", "filter"}) {
        toolbar_->findOption(id)->styleOverrides.background =
            Color{.08, .105, .13, 1};
    }
    for (size_t i = 0; i < navigation_.size(); ++i) {
        navigation_[i] = attach("quickPlayNav" + std::to_string(i),
                                {left, -.79, navRight, .46}, true);
        navigation_[i]->tableColumns = {navRight - left - .04};
        navigation_[i]->setTabSelected(i == tab_);
    }
    results_ =
        attach("quickPlayResults", {centerLeft, -.79, centerRight, .46}, true);
    const double tableWidth = centerRight - centerLeft - .04;
    results_->styleOverrides.padding = .42;
    const double durationWidth = std::max(.18, tableWidth * .12);
    const double levelsWidth = std::max(.20, tableWidth * .13);
    const double namesWidth = tableWidth - durationWidth - levelsWidth;
    results_->tableColumns = {namesWidth * .46, namesWidth * .33,
                              namesWidth * .21, durationWidth, levelsWidth};
    preview_ =
        attach("quickPlayPreview", {previewLeft, -.79, right, .46}, true);
    preview_->viewport.top = .34;
    preview_->flowY = .36;
    preview_->styleOverrides.rowStep = .067;
    preview_->tableColumns = {right - previewLeft - .04};
    prepare(root_);
    prepare(*toolbar_);
}

QuickPlayView::~QuickPlayView() { root_.closeInterfaces(); }

void QuickPlayView::setLogo(CoverImage image) {
    auto *logo = root_.findOption("brand");
    if (!logo || image.texture == 0 || image.width <= 0 || image.height <= 0) {
        return;
    }
    logo->type = CMenu::image;
    logo->texture = image.texture;
    logo->text.clear();
    logo->width = .74;
    logo->height = logo->width * image.height / image.width;
    if (logo->height > .22) {
        logo->width *= .22 / logo->height;
        logo->height = .22;
    }
    logo->y = .87 - logo->height / 2;
    logo->styleOverrides.textColor = Color{1, 1, 1, 1};
}

void QuickPlayView::setIcons(const std::array<CoverImage, 5> &icons) {
    icons_ = icons;
    for (size_t i = 0; i < tabs.size(); ++i) {
        const auto *tab = root_.findOption("tab" + std::to_string(i));
        if (icons[i].texture == 0 || !tab) {
            continue;
        }
        auto icon = item("tabIcon" + std::to_string(i), "", CMenu::image,
                         tab->x + .015, tab->y + .025, .05, .05);
        icon.texture = icons[i].texture;
        root_.addOpt(icon);
        root_.findOption("tab" + std::to_string(i))->text =
            "  " + std::string(tabs[i]);
    }
    root_.reflow();
}

void QuickPlayView::prepare(CMenu &menu) {
    if (presentation_) {
        presentation_(menu);
    }
    menu.reflow();
}

std::string QuickPlayView::groupKey(const songs::SongRecord &song,
                                    size_t tab) const {
    if (tab == 0) {
        return song.path == "." ? "" : song.path;
    }
    if (tab == 1) {
        return song.artist.empty() ? "Artista não informado"
                                   : metadataDisplayText(song.artist);
    }
    if (tab == 3) {
        return song.charter.empty() ? "Charter não informado"
                                    : metadataDisplayText(song.charter);
    }
    const auto title = searchKey(song.title);
    return !title.empty() && title.front() >= 'a' && title.front() <= 'z'
               ? std::string(1, static_cast<char>(title.front() - 'a' + 'A'))
               : "#";
}

void QuickPlayView::indexGroups() {
    std::array<std::map<std::string, Group>, 4> groups;
    searchKeys_.clear();
    for (const auto &song : snapshot_->songs) {
        searchKeys_.push_back(
            searchKey(song.title + "\n" + song.artist + "\n" + song.charter));
        for (size_t tab = 0; tab < 4; ++tab) {
            auto key = groupKey(song, tab);
            if (tab == 0) {
                while (!key.empty()) {
                    auto &group = groups[tab][key];
                    group.key = key;
                    const auto slash = key.rfind('/');
                    group.label = slash == std::string::npos
                                      ? key
                                      : key.substr(slash + 1);
                    group.depth = static_cast<size_t>(
                        std::count(key.begin(), key.end(), '/'));
                    ++group.count;
                    if (slash == std::string::npos) {
                        break;
                    }
                    key.resize(slash);
                    groups[tab][key].children = true;
                }
            } else {
                auto &group = groups[tab][key];
                group.key = group.label = key;
                ++group.count;
            }
        }
    }
    for (size_t tab = 0; tab < 4; ++tab) {
        groups_[tab].clear();
        for (auto &entry : groups[tab]) {
            groups_[tab].push_back(std::move(entry.second));
        }
        // Folder prefix order keeps a parent and all descendants contiguous.
        if (tab == 0) {
            std::sort(
                groups_[tab].begin(), groups_[tab].end(),
                [](const Group &a, const Group &b) {
                    return std::lexicographical_compare(
                        a.key.begin(), a.key.end(), b.key.begin(), b.key.end(),
                        [](unsigned char ac, unsigned char bc) {
                            return (ac == '/' ? 0 : ac) < (bc == '/' ? 0 : bc);
                        });
                });
        } else {
            std::sort(groups_[tab].begin(), groups_[tab].end(),
                      [](const Group &a, const Group &b) {
                          const auto ak = searchKey(a.label),
                                     bk = searchKey(b.label);
                          return ak == bk ? a.key < b.key : ak < bk;
                      });
        }
        if (!states_[tab].group.empty() &&
            !groups[tab].count(states_[tab].group)) {
            states_[tab].group.clear();
        }
        buildNavigation(tab);
    }
}

void QuickPlayView::buildNavigation(size_t tab) {
    auto &menu = *navigation_[tab];
    const auto bounds = menu.getPanelBounds();
    const double scroll = menu.scrollOffset;
    std::string focus;
    const auto found = navigationRows_[tab].find(menu.focusedOption());
    if (found != navigationRows_[tab].end()) {
        focus = found->second;
    }
    menu.clearOptions();
    background(menu, bounds);
    menu.addOpt(
        item("heading", tabs[tab], CMenu::static_text, bounds.left + .02, .39));
    std::vector<CMenu::menuOpt> rows;
    std::vector<std::string> keys;
    auto append = [&](const std::string &key, const std::string &label) {
        auto row = item("group:" + key, label, CMenu::table_row, 0, 0);
        row.group = 1;
        row.ellipsizeCells = true;
        row.cells = {label};
        row.active = key == states_[tab].group;
        if (tab == 0) {
            row.navigationKey = [this, key](CMenu &, int command) {
                if (command != GLFW_KEY_LEFT && command != GLFW_KEY_RIGHT) {
                    return false;
                }
                const auto group = std::find_if(
                    groups_[0].begin(), groups_[0].end(),
                    [&](const Group &value) { return value.key == key; });
                if (group != groups_[0].end() && group->children) {
                    if (command == GLFW_KEY_RIGHT) {
                        expanded_.insert(key);
                    } else {
                        expanded_.erase(key);
                    }
                    navigationDirty_ = true;
                }
                return true;
            };
        }
        rows.push_back(std::move(row));
        keys.push_back(key);
    };
    append("", "Todas as músicas (" + std::to_string(snapshot_->songs.size()) +
                   ")");
    for (const auto &group : groups_[tab]) {
        bool visible = true;
        if (tab == 0) {
            for (auto slash = group.key.find('/'); slash != std::string::npos;
                 slash = group.key.find('/', slash + 1)) {
                if (!expanded_.count(group.key.substr(0, slash))) {
                    visible = false;
                    break;
                }
            }
        }
        if (visible) {
            const auto marker =
                tab == 0 ? (group.children ? "   " : "   ") : "";
            append(group.key,
                   std::string(std::min(group.depth, size_t(12)) * 2, ' ') +
                       marker + group.label + " (" +
                       std::to_string(group.count) + ")");
        }
    }
    navigationRows_[tab].clear();
    const auto indices = menu.addOptions(rows);
    for (size_t i = 0; i < indices.size(); ++i) {
        navigationRows_[tab][indices[i]] = keys[i];
        if (keys[i] == focus) {
            menu.select(indices[i]);
        }
    }
    prepare(menu);
    if (tab == 0) {
        // Overlay only the disclosure area; the remaining row selects context.
        // Place after rows so normal CMenu hit routing gives the arrow
        // priority.
        std::unordered_map<std::string, int> visibleRows;
        for (size_t i = 0; i < keys.size(); ++i) {
            visibleRows.emplace(keys[i], indices[i]);
        }
        std::vector<CMenu::menuOpt> arrows;
        std::vector<std::string> arrowKeys;
        for (const auto &group : groups_[0]) {
            const auto foundRow = visibleRows.find(group.key);
            if (!group.children || foundRow == visibleRows.end()) {
                continue;
            }
            const auto &row =
                menu.options[static_cast<size_t>(foundRow->second)];
            auto arrow =
                item("expand:" + group.key,
                     expanded_.count(group.key) ? "v" : ">", CMenu::textbtn,
                     row.x, row.y, .055, menu.resolvedStyle().layout.rowStep);
            auto indent = item(
                "", std::string(std::min(group.depth, size_t(12)) * 2, ' '),
                CMenu::static_text, 0, 0);
            menu.ensureLayout(indent);
            arrow.x += indent.layout.width() *
                       menu.resolvedStyle().typography.textSize;
            arrow.group = 1;
            arrow.manualPosition = true;
            arrow.keyboardFocus = false;
            arrows.push_back(std::move(arrow));
            arrowKeys.push_back(group.key);
        }
        const auto arrowIndices = menu.addOptions(arrows);
        for (size_t i = 0; i < arrowIndices.size(); ++i) {
            navigationRows_[tab][arrowIndices[i]] = arrowKeys[i];
        }
        prepare(menu);
    }
    menu.scrollOffset = scroll;
}

void QuickPlayView::buildResults() {
    auto &menu = *results_;
    auto &state = states_[tab_];
    const auto query = searchKey(query_);
    std::vector<size_t> matches;
    for (size_t i = 0; i < snapshot_->songs.size(); ++i) {
        const auto &song = snapshot_->songs[i];
        const bool groupMatches =
            tab_ == 0
                ? within(groupKey(song, tab_), state.group)
                : state.group.empty() || groupKey(song, tab_) == state.group;
        if (groupMatches && (!availableOnly_ || song.available) &&
            (query.empty() ||
             searchKeys_[i].find(query) != std::string::npos)) {
            matches.push_back(i);
        }
    }
    // Compute collation keys once per changed query/publication, never per
    // frame.
    std::unordered_map<size_t, std::string> keys;
    for (auto i : matches) {
        const auto &song = snapshot_->songs[i];
        keys[i] = searchKey(order_ == 1   ? song.artist
                            : order_ == 2 ? song.charter
                                          : song.title);
    }
    std::sort(matches.begin(), matches.end(), [&](size_t a, size_t b) {
        const auto &sa = snapshot_->songs[a], &sb = snapshot_->songs[b];
        if (order_ == 3 && sa.duration != sb.duration) {
            return sa.duration.value_or(
                       std::numeric_limits<double>::infinity()) <
                   sb.duration.value_or(
                       std::numeric_limits<double>::infinity());
        }
        return keys.at(a) == keys.at(b) ? sa.path < sb.path
                                        : keys.at(a) < keys.at(b);
    });
    const auto bounds = menu.getPanelBounds();
    const std::string heading =
        (state.group.empty() ? "Biblioteca" : state.group) + " | " +
        std::to_string(matches.size()) +
        (matches.size() == 1 ? " música" : " músicas");
    bool sameRows =
        matches.size() == resultRows_.size() && menu.findOption("context");
    size_t match = 0;
    for (const auto &entry : resultRows_) {
        if (!sameRows ||
            menu.options[static_cast<size_t>(entry.first)].optionName !=
                menu.getName() +
                    ".song:" + snapshot_->songs[matches[match++]].path) {
            sameRows = false;
            break;
        }
    }
    if (!sameRows) {
        menu.clearOptions();
        background(menu, bounds);
        menu.addOpt(
            item("context",
                 fitLabel(menu, heading, bounds.right - bounds.left - .04),
                 CMenu::static_text, bounds.left + .02, .39));
        const auto style = menu.resolvedStyle();
        const double padding = style.layout.padding * style.typography.textSize;
        double x = bounds.left + .02 + padding;
        const std::array<const char *, 5> columns{"Música", "Artista", "Autor",
                                                  "Min", "Níveis"};
        for (size_t i = 0; i < columns.size(); ++i) {
            menu.addOpt(item(
                "column" + std::to_string(i),
                fitLabel(menu, columns[i], menu.tableColumns[i] - 2 * padding),
                CMenu::static_text, x, .315));
            menu.options.back().styleOverrides.textColor =
                Color{.60, .69, .75, 1};
            menu.options.back().styleOverrides.textSize =
                menu.resolvedStyle().typography.textSize * .92;
            x += menu.tableColumns[i];
        }
    } else {
        menu.findOption("context")->text =
            fitLabel(menu, heading, bounds.right - bounds.left - .04);
    }
    std::vector<CMenu::menuOpt> rows;
    for (auto index : matches) {
        const auto &song = snapshot_->songs[index];
        auto row = item("song:" + song.path, metadataDisplayText(song.title),
                        CMenu::table_row, 0, 0);
        row.cells = {
            metadataDisplayText(song.title), metadataDisplayText(song.artist),
            metadataDisplayText(song.charter), duration(song), levels(song)};
        row.group = 1;
        row.ellipsizeCells = true;
        row.activateOnClick = false;
        row.disabled = !song.available;
        row.active = song.path == state.selected;
        if (rows.size() % 2 == 0) {
            row.styleOverrides.background = Color{.10, .125, .145, .65};
        }
        rows.push_back(std::move(row));
    }
    std::vector<int> indices;
    if (sameRows) {
        size_t i = 0;
        for (const auto &entry : resultRows_) {
            auto &option = menu.options[static_cast<size_t>(entry.first)];
            option.text = rows[i].text;
            option.cells = std::move(rows[i].cells);
            option.disabled = rows[i].disabled;
            option.active = rows[i++].active;
            indices.push_back(entry.first);
        }
    } else {
        indices = menu.addOptions(rows);
    }
    resultRows_.clear();
    int selection = -1;
    for (size_t i = 0; i < indices.size(); ++i) {
        resultRows_[indices[i]] = matches[i];
        if (snapshot_->songs[matches[i]].path == state.selected) {
            selection = indices[i];
        }
    }
    if (selection < 0) {
        const auto playable =
            std::find_if(indices.begin(), indices.end(), [&](int i) {
                return !menu.options[static_cast<size_t>(i)].disabled;
            });
        if (playable != indices.end()) {
            selection = *playable;
        }
    }
    if (matches.empty() && !sameRows) {
        menu.addOpt(item("empty", "Nenhuma música neste filtro.",
                         CMenu::static_text, bounds.left + .02, .15));
    }
    prepare(menu);
    menu.select(selection);
    menu.scrollOffset = state.scroll;
    if (selection >= 0) {
        selectSong(resultRows_.at(selection));
    } else {
        state.selected.clear();
        showPreview(nullptr);
    }
}

void QuickPlayView::selectSong(size_t index) {
    const auto &song = snapshot_->songs[index];
    if (states_[tab_].selected == song.path && previewPath_ == song.path &&
        !previewDirty_) {
        return;
    }
    states_[tab_].selected = song.path;
    for (const auto &entry : resultRows_) {
        results_->options[static_cast<size_t>(entry.first)].active =
            entry.second == index;
    }
    if (previewPath_ != song.path || previewDirty_) {
        showPreview(&song);
    }
}

void QuickPlayView::showPreview(const songs::SongRecord *song) {
    const auto bounds = preview_->getPanelBounds();
    const bool sameSong = song && previewPath_ == song->path;
    const double scroll = sameSong ? preview_->scrollOffset : 0;
    preview_->clearOptions();
    background(*preview_, bounds);
    previewPath_ = song ? song->path : "";
    coverPath_ = song ? song->coverPath : "";
    preview_->setFlow(CMenu::Flow::Manual, 0, 0);
    auto scrollControl =
        item("scrollPreview", "Detalhes  /  PgUp PgDn", CMenu::textbtn,
             bounds.left + .02, .36, bounds.right - bounds.left - .04, .07);
    scrollControl.styleOverrides.textSize = .030;
    scrollControl.styleOverrides.textColor = Color{.60, .69, .75, 1};
    scrollControl.enableEnter = false;
    scrollControl.navigationKey = [](CMenu &menu, int key) {
        if (key != GLFW_KEY_UP && key != GLFW_KEY_DOWN &&
            key != GLFW_KEY_PAGE_UP && key != GLFW_KEY_PAGE_DOWN) {
            return false;
        }
        CControls::MenuEvent scroll;
        scroll.kind = CControls::MenuEvent::Scroll;
        scroll.y = (key == GLFW_KEY_UP || key == GLFW_KEY_PAGE_UP) ? 1 : -1;
        if (key == GLFW_KEY_PAGE_UP || key == GLFW_KEY_PAGE_DOWN) {
            scroll.y *= 4;
        }
        menu.processEvent(scroll);
        return true;
    };
    preview_->addOpt(scrollControl);
    double cursor = .32;
    const double textSize = preview_->resolvedStyle().typography.textSize;
    const double textWidth = bounds.right - bounds.left - .065;
    const Color muted{.61, .70, .76, 1};
    auto label = [&](const std::string &id, const std::string &text,
                     double size = 0,
                     size_t limit = std::numeric_limits<size_t>::max(),
                     bool secondary = false) {
        if (size <= 0) {
            size = textSize;
        }
        const double height = size * 1.48;
        auto lines = wrapText(*preview_, text, textWidth, size);
        if (lines.size() > limit) {
            lines.resize(limit);
            lines.back() =
                fitLabel(*preview_, lines.back() + " ...", textWidth, size);
        }
        for (size_t i = 0; i < lines.size(); ++i) {
            cursor -= height;
            auto row =
                item(i == 0 ? id : id + "_line" + std::to_string(i), lines[i],
                     CMenu::static_text, bounds.left + .025, cursor);
            row.group = 1;
            row.styleOverrides.textSize = size;
            if (secondary) {
                row.styleOverrides.textColor = muted;
            }
            preview_->addOpt(row);
        }
    };
    auto section = [&](const std::string &id, const std::string &text) {
        cursor -= .030;
        auto line = item(id + "Divider", "", CMenu::panel, bounds.left + .025,
                         cursor, textWidth, .002);
        line.group = 1;
        line.styleOverrides.background = Color{.20, .27, .31, 1};
        line.styleOverrides.borderWidth = 0;
        preview_->addOpt(line);
        label(id, text, textSize, 1);
        cursor -= .010;
    };
    if (!song) {
        label("empty", "Selecione uma música");
    } else {
        label("title", song->title, textSize * 1.35);
        label("artist",
              song->artist.empty() ? "Artista não informado" : song->artist,
              textSize, std::numeric_limits<size_t>::max(), true);
        cursor -= .245;
        const double artSize = .22;
        auto art = item("cover", "", CMenu::image, bounds.left + .025, cursor,
                        artSize, artSize);
        art.group = 1;
        auto frame = art;
        frame.optionName = "coverFrame";
        frame.type = CMenu::panel;
        frame.styleOverrides.background = Color{.07, .10, .12, 1};
        frame.styleOverrides.borderWidth = 0;
        preview_->addOpt(frame);
        preview_->addOpt(art);
        auto placeholder = item("coverPlaceholder", "", CMenu::image,
                                art.x + .06, art.y + .075, .10, .10);
        placeholder.group = 1;
        placeholder.texture = icons_[4].texture;
        preview_->addOpt(placeholder);
        auto fallback = item("artFallback", "Sem capa", CMenu::static_text,
                             bounds.left + .04, cursor + .018);
        fallback.group = 1;
        fallback.styleOverrides.textSize = .027;
        fallback.styleOverrides.textColor = muted;
        preview_->addOpt(fallback);
        auto detail = [&](const std::string &id, const std::string &value,
                          double y) {
            auto text =
                item(id, fitLabel(*preview_, value, textWidth - .25, textSize),
                     CMenu::static_text, bounds.left + .275, y);
            text.group = 1;
            text.styleOverrides.textSize = textSize;
            preview_->addOpt(text);
        };
        detail("duration", duration(*song), cursor + .15);
        if (song->bpmInitial > 0) {
            detail(
                "bpm",
                std::to_string(static_cast<int>(std::round(song->bpmInitial))) +
                    " BPM",
                cursor + .085);
        }
        cursor -= .012;
        label("charter",
              "Charter: " +
                  (song->charter.empty() ? "Não informado" : song->charter),
              0, 2, true);
        if (!song->album.empty()) {
            label("album", "Álbum: " + song->album, 0, 2, true);
        }
        if (!song->year.empty() || !song->genre.empty()) {
            label("genre", song->year + "  " + song->genre, 0, 2, true);
        }
        section("parts", "Instrumentos / níveis");
        const std::array<std::string, 4> difficultyNames{"Easy", "Medium",
                                                         "Hard", "Expert"};
        const std::array<std::string, 4> difficultyLabels{"F", "M", "D", "E"};
        std::map<std::string, std::set<size_t>> parts;
        for (auto part : song->instruments) {
            if (!part.empty() && part.front() == '[') {
                part.erase(0, 1);
            }
            if (!part.empty() && part.back() == ']') {
                part.pop_back();
            }
            size_t difficulty = 4;
            for (size_t i = 0; i < difficultyNames.size(); ++i) {
                if (part.compare(0, difficultyNames[i].size(),
                                 difficultyNames[i]) == 0) {
                    part.erase(0, difficultyNames[i].size());
                    difficulty = i;
                    break;
                }
            }
            const std::map<std::string, std::string> names{
                {"Single", "Guitarra"},    {"DoubleBass", "Baixo"},
                {"DoubleRhythm", "Ritmo"}, {"DoubleGuitar", "Coop"},
                {"Drums", "Bateria"},      {"Keyboard", "Teclado"},
                {"Keys", "Teclado"},       {"Vocals", "Voz"}};
            const auto name = names.find(part);
            parts[name == names.end() ? part : name->second].insert(difficulty);
        }
        if (parts.empty()) {
            label("noParts", "Não informados");
        }
        size_t partId = 0;
        const std::array<Color, 4> levelColors{{{.45, .83, .51, 1},
                                                {1, .78, .30, 1},
                                                {1, .48, .40, 1},
                                                {.48, .76, 1, 1}}};
        for (const auto &part : parts) {
            const auto id = "part" + std::to_string(partId++);
            const double badgeWidth = .060;
            const double badgeArea = badgeWidth * 4;
            const double rowSize = textSize;
            const double badgeX =
                std::min(bounds.right - .035 - badgeArea, bounds.left + .49);
            cursor -= rowSize * 1.65;
            auto name = item(id,
                             fitLabel(*preview_, part.first,
                                      badgeX - bounds.left - .04, rowSize),
                             CMenu::static_text, bounds.left + .025, cursor);
            name.group = 1;
            name.styleOverrides.textSize = rowSize;
            preview_->addOpt(name);
            for (size_t level = 0; level < 4; ++level) {
                const bool present = part.second.count(level) != 0;
                auto badge = item(id + "Level" + std::to_string(level),
                                  present ? difficultyLabels[level] : "-",
                                  CMenu::static_text,
                                  badgeX + level * badgeWidth, cursor);
                badge.group = 1;
                badge.styleOverrides.textSize = rowSize;
                badge.styleOverrides.textColor =
                    present ? levelColors[level] : Color{.25, .31, .35, 1};
                preview_->addOpt(badge);
            }
        }
        label("difficultyLegend", "F Fácil / M Médio / D Difícil / E Expert",
              textSize * .88, 2, true);
        section("stemsHeading", "Áudio disponível");
        if (!song->audioPath.empty()) {
            label(
                "mainAudio",
                "Principal: " +
                    std::filesystem::path(song->audioPath).filename().string());
        }
        if (song->stems.empty() && song->audioPath.empty()) {
            label("noStems", "Nenhum áudio indexado");
        }
        std::string stems;
        for (const auto &stem : song->stems) {
            if (!stems.empty()) {
                stems += ", ";
            }
            stems += stem;
        }
        if (!stems.empty()) {
            label("stems", "Stems: " + stems, 0, 3, true);
        }
        if (!song->available) {
            label("unavailable", "Indisponível: " + song->error);
        }
        section("location", "Arquivo");
        label("format", "Formato: " + song->format, 0, 1, true);
        if (!song->origin.empty()) {
            label("origin", "Origem: " + song->origin, 0, 1, true);
        }
        label("folder", "Pasta: " + song->path, 0, 2, true);
    }
    prepare(*preview_);
    preview_->scrollOffset = scroll;
    previewDirty_ = false;
}

void QuickPlayView::synchronize(
    std::shared_ptr<const songs::LibrarySnapshot> snapshot) {
    const bool first = !snapshot_;
    const bool wasEmpty = first || snapshot_->songs.empty();
    const bool changed =
        !snapshot_ || snapshot_->contentRevision != snapshot->contentRevision;
    snapshot_ = std::move(snapshot);
    if (changed) {
        indexGroups();
        previewDirty_ = true;
        dirty_ = true;
    }
    if (navigationDirty_) {
        buildNavigation(tab_);
        navigationDirty_ = false;
    }
    if (dirty_) {
        buildResults();
        dirty_ = false;
    }
    const int rootFocus = root_.focusedOption();
    const bool initialTabFocused =
        root_.focusedInterface() == &root_ && rootFocus >= 0 &&
        root_.options[static_cast<size_t>(rootFocus)].optionName ==
            root_.getName() + ".tab0";
    if (first || (wasEmpty && initialTabFocused && !resultRows_.empty())) {
        root_.focusInterface(*results_);
    }
    toolbar_->findOption("searchHint")->text =
        query_.empty() ? "Digite para buscar..." : "";
    const auto &progress = snapshot_->progress;
    const std::string status =
        std::string(progress.running ? "Atualizando: " : "Biblioteca: ") +
        std::to_string(snapshot_->songs.size()) + " músicas | " +
        std::to_string(progress.examined) + " arquivos | " +
        std::to_string(progress.added) + " novas | " +
        std::to_string(progress.modified) + " alteradas" +
        (progress.warning.empty() ? "" : " | " + progress.warning);
    if (lastStatus_ != status) {
        lastStatus_ = status;
        root_.findOption("scanStatus")->text =
            fitLabel(root_, status, contentWidth_ - .06,
                     root_.resolvedOptionStyle(*root_.findOption("scanStatus"))
                         .textSize);
    }
    if (cover_ && !coverPath_.empty()) {
        if (auto *art = preview_->findOption("cover")) {
            if (art->texture == 0) {
                const auto cover = cover_(coverPath_);
                art->texture = cover.texture;
                if (cover.texture != 0 && cover.width > 0 && cover.height > 0) {
                    const double scale = std::min(art->width / cover.width,
                                                  art->height / cover.height);
                    art->width = cover.width * scale;
                    art->height = cover.height * scale;
                    if (const auto *frame =
                            preview_->findOption("coverFrame")) {
                        art->x = frame->x + (frame->width - art->width) / 2;
                        art->y = frame->y + (frame->height - art->height) / 2;
                    }
                }
            }
            preview_->findOption("coverPlaceholder")->texture =
                art->texture ? 0 : icons_[4].texture;
            preview_->findOption("artFallback")->text =
                art->texture ? "" : "Sem capa";
        }
    }
}

std::string QuickPlayView::interact() {
    if (activated(root_, "refresh")) {
        refresh_ = songs::ScanMode::Incremental;
    }
    if (activated(root_, "verify")) {
        refresh_ = songs::ScanMode::Full;
    }
    const auto &search = toolbar_->findOption("search")->preText;
    if (query_ != search) {
        query_ = search;
        states_[tab_].scroll = 0;
        dirty_ = true;
    }
    if (activated(*toolbar_, "sort")) {
        order_ = (order_ + 1) % 4;
        toolbar_->findOption("sort")->text =
            std::string("Ordem: ") + orders[static_cast<size_t>(order_)];
        dirty_ = true;
    }
    if (activated(*toolbar_, "filter")) {
        availableOnly_ = !availableOnly_;
        toolbar_->findOption("filter")->text =
            availableOnly_ ? "Disponíveis" : "Todas";
        dirty_ = true;
    }
    for (size_t tab = 0; tab < tabs.size(); ++tab) {
        if (tab != tab_ && activated(root_, "tab" + std::to_string(tab))) {
            states_[tab_].scroll = results_->scrollOffset;
            navigation_[tab_]->setTabSelected(false);
            if (states_[tab].selected.empty()) {
                states_[tab].selected = states_[tab_].selected;
            }
            tab_ = tab;
            navigation_[tab_]->setTabSelected(true);
            dirty_ = true;
            for (size_t i = 0; i < tabs.size(); ++i) {
                root_.findOption("tab" + std::to_string(i))->active = i == tab_;
            }
            return {};
        }
    }
    for (const auto &entry : navigationRows_[tab_]) {
        if ((navigation_[tab_]
                 ->options[static_cast<size_t>(entry.first)]
                 .status &
             3) == 3) {
            const auto &option =
                navigation_[tab_]->options[static_cast<size_t>(entry.first)];
            if (tab_ == 0 &&
                option.optionName ==
                    navigation_[tab_]->getName() + ".expand:" + entry.second) {
                if (!expanded_.erase(entry.second)) {
                    expanded_.insert(entry.second);
                }
                navigationDirty_ = true;
                return {};
            }
            states_[tab_].group = entry.second;
            states_[tab_].scroll = 0;
            // Deferred until the next pre-dispatch phase, like result changes.
            navigationDirty_ = true;
            dirty_ = true;
            return {};
        }
    }
    if (dirty_) {
        return {};
    }
    states_[tab_].scroll = results_->scrollOffset;
    const auto focused = resultRows_.find(results_->focusedOption());
    if (root_.focusedInterface() == results_ && focused != resultRows_.end()) {
        selectSong(focused->second);
    }
    if (activated(root_, "random")) {
        std::vector<int> playable;
        for (const auto &entry : resultRows_) {
            if (snapshot_->songs[entry.second].available) {
                playable.push_back(entry.first);
            }
        }
        if (!playable.empty()) {
            static std::mt19937 random{std::random_device{}()};
            const int index = playable[std::uniform_int_distribution<size_t>(
                0, playable.size() - 1)(random)];
            results_->select(index);
            root_.focusInterface(*results_);
            selectSong(resultRows_.at(index));
        }
    }
    for (const auto &entry : resultRows_) {
        if ((results_->options[static_cast<size_t>(entry.first)].status & 3) ==
                3 &&
            snapshot_->songs[entry.second].available) {
            return snapshot_->songs[entry.second].path;
        }
    }
    if (activated(root_, "play")) {
        for (const auto &entry : resultRows_) {
            const auto &song = snapshot_->songs[entry.second];
            if (song.path == states_[tab_].selected && song.available) {
                return song.path;
            }
        }
    }
    return {};
}

std::optional<songs::ScanMode> QuickPlayView::takeRefresh() {
    const auto result = refresh_;
    refresh_.reset();
    return result;
}
} // namespace ui
