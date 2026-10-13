#include "CUIDefinition.h"
#include "CUIOverrides.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace ui {
namespace {
#define FIELD(name) f(#name, v.name)
template <class F> void fields(Color &v, F &f) {
    FIELD(r);
    FIELD(g);
    FIELD(b);
    FIELD(a);
}

template <class F> void fields(CMenu::Rect &v, F &f) {
    FIELD(left);
    FIELD(bottom);
    FIELD(right);
    FIELD(top);
}

template <class F> void fields(StyleOverrides &v, F &f) {
    FIELD(textSize);
    FIELD(rowStep);
    FIELD(padding);
    FIELD(pickGap);
    FIELD(textColor);
    FIELD(accent);
    FIELD(selection);
    FIELD(field);
    FIELD(control);
    FIELD(panel);
    FIELD(animateSelection);
    FIELD(panelBorder);
    FIELD(panelBorderWidth);
}

template <class F> void fields(OptionStyleOverrides &v, F &f) {
    FIELD(textSize);
    FIELD(textColor);
    FIELD(background);
    FIELD(border);
    FIELD(borderWidth);
}

template <class F> void fields(OptionDefinition &v, F &f) {
    FIELD(id);
    FIELD(text);
    FIELD(value);
    FIELD(languageKey);
    FIELD(texture);
    FIELD(type);
    FIELD(group);
    FIELD(shortcut);
    FIELD(x);
    FIELD(y);
    FIELD(fieldWidth);
    FIELD(listWidth);
    FIELD(width);
    FIELD(height);
    FIELD(disabled);
    FIELD(manual);
    FIELD(anchorRight);
    FIELD(back);
    FIELD(enableEnter);
    FIELD(centerX);
    FIELD(maxBytes);
    FIELD(items);
    FIELD(cells);
    FIELD(active);
    FIELD(actions);
    FIELD(style);
}

template <class F> void fields(MenuDefinition &v, F &f) {
    FIELD(id);
    FIELD(title);
    FIELD(background);
    FIELD(modal);
    FIELD(viewportEnabled);
    FIELD(anchorLeft);
    FIELD(x);
    FIELD(y);
    FIELD(flowX);
    FIELD(flowY);
    FIELD(leftMargin);
    FIELD(flowGroup);
    FIELD(flow);
    FIELD(viewport);
    FIELD(panel);
    FIELD(style);
    FIELD(options);
    FIELD(tableColumns);
}
template <class F> void fields(OptionLayoutOverrides &v, F &f) {
    FIELD(x);
    FIELD(y);
    FIELD(fieldWidth);
    FIELD(listWidth);
    FIELD(width);
    FIELD(height);
    FIELD(centerX);
    FIELD(anchorRight);
    FIELD(style);
}
template <class F> void fields(MenuOverrides &v, F &f) {
    FIELD(x);
    FIELD(y);
    FIELD(flowX);
    FIELD(flowY);
    FIELD(leftMargin);
    FIELD(flowGroup);
    FIELD(flow);
    FIELD(viewportEnabled);
    FIELD(anchorLeft);
    FIELD(viewport);
    FIELD(panel);
    FIELD(panelBounds);
    FIELD(panelFollowsViewport);
    FIELD(style);
    FIELD(options);
    FIELD(tableColumns);
}
#undef FIELD

template <class T> struct Optional : std::false_type {};
template <class T> struct Optional<std::optional<T>> : std::true_type {};
template <class T> struct Vector : std::false_type {};
template <class T> struct Vector<std::vector<T>> : std::true_type {};

template <class T> struct Map : std::false_type {};
template <class T> struct Map<std::map<std::string, T>> : std::true_type {};

struct FieldNames {
    std::set<std::string> names;
    template <class T> void operator()(const char *key, T &) {
        names.insert(key);
    }
};

template <class T> void read(lua_State *, T &);
template <class T> void write(std::ostream &, T &);

struct Reader {
    lua_State *L;
    template <class T> void operator()(const char *key, T &value) {
        lua_pushstring(L, key);
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1)) {
            try {
                read(L, value);
            } catch (const std::exception &e) {
                throw std::runtime_error(std::string(key) + ": " + e.what());
            }
        }
        lua_pop(L, 1);
    }
};

struct Writer {
    std::ostream &out;
    template <class T> void operator()(const char *key, T &value) {
        if constexpr (Optional<T>::value) {
            if (!value) {
                return;
            }
        }
        out << key << " = ";
        write(out, value);
        out << ", ";
    }
};

template <class T> void read(lua_State *L, T &v) {
    if constexpr (std::is_same_v<T, std::string>) {
        if (lua_type(L, -1) != LUA_TSTRING) {
            throw std::runtime_error("Expected string");
        }
        size_t size = 0;
        const char *text = lua_tolstring(L, -1, &size);
        v.assign(text, size);
        if (v.size() > 1024 * 1024 ||
            CFonts::utf8Size(v) == std::string::npos) {
            throw std::runtime_error("Invalid or oversized UTF-8 string");
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!lua_isboolean(L, -1)) {
            throw std::runtime_error("Expected boolean");
        }
        v = lua_toboolean(L, -1) != 0;
    } else if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>) {
        if (lua_type(L, -1) != LUA_TNUMBER) {
            throw std::runtime_error("Expected number");
        }
        const double n = lua_tonumber(L, -1);
        if (!std::isfinite(n) || std::abs(n) > 1000000 ||
            ((std::is_integral_v<T> || std::is_enum_v<T>) &&
             std::floor(n) != n)) {
            throw std::runtime_error("Invalid number");
        }
        if constexpr (std::is_unsigned_v<T>) {
            if (n < 0) {
                throw std::runtime_error("Expected nonnegative number");
            }
        }
        v = static_cast<T>(n);
    } else if constexpr (Optional<T>::value) {
        typename T::value_type item{};
        read(L, item);
        v = item;
    } else {
        if (!lua_istable(L, -1)) {
            throw std::runtime_error("Expected table");
        }
        if constexpr (Map<T>::value) {
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                if (lua_type(L, -2) != LUA_TSTRING || v.size() >= 4096) {
                    throw std::runtime_error(
                        "Expected string IDs, at most 4096 entries");
                }
                const std::string id = lua_tostring(L, -2);
                typename T::mapped_type item{};
                try {
                    read(L, item);
                } catch (const std::exception &e) {
                    throw std::runtime_error(id + ": " + e.what());
                }
                v.emplace(id, std::move(item));
                lua_pop(L, 1);
            }
        } else if constexpr (Vector<T>::value) {
            const size_t count = lua_rawlen(L, -1);
            if (count > 4096) {
                throw std::runtime_error("Too many entries");
            }
            v.clear();
            for (size_t i = 0; i < count; ++i) {
                lua_rawgeti(L, -1, static_cast<lua_Integer>(i) + 1);
                typename T::value_type item{};
                read(L, item);
                v.push_back(std::move(item));
                lua_pop(L, 1);
            }
        } else {
            FieldNames known;
            fields(v, known);
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                if (lua_type(L, -2) != LUA_TSTRING ||
                    !known.names.count(lua_tostring(L, -2))) {
                    throw std::runtime_error("Unknown UI property");
                }
                lua_pop(L, 1);
            }
            Reader reader{L};
            fields(v, reader);
        }
    }
}

template <class T> void write(std::ostream &out, T &v) {
    if constexpr (std::is_same_v<T, std::string>) {
        out << luaQuote(v);
    } else if constexpr (std::is_same_v<T, bool>) {
        out << (v ? "true" : "false");
    } else if constexpr (std::is_enum_v<T>) {
        out << static_cast<int>(v);
    } else if constexpr (std::is_arithmetic_v<T>) {
        out << v;
    } else if constexpr (Optional<T>::value) {
        if (v) {
            write(out, *v);
        } else {
            out << "nil";
        }
    } else {
        out << "{ ";
        if constexpr (Map<T>::value) {
            out << '\n';
            for (auto &entry : v) {
                out << '[' << luaQuote(entry.first) << "] = ";
                write(out, entry.second);
                out << ",\n";
            }
        } else if constexpr (Vector<T>::value) {
            out << '\n';
            for (auto &item : v) {
                write(out, item);
                out << ",\n";
            }
        } else {
            Writer writer{out};
            fields(v, writer);
        }
        out << "}";
    }
}

template <class T> void check(T &);
struct Checker {
    template <class T> void operator()(const char *, T &value) { check(value); }
};

template <class T> void check(T &value) {
    if constexpr (std::is_same_v<T, std::string>) {
        if (value.size() > 1024 * 1024 ||
            CFonts::utf8Size(value) == std::string::npos) {
            throw std::runtime_error("Invalid UI UTF-8 string");
        }
    } else if constexpr (std::is_arithmetic_v<T>) {
        if (!std::isfinite(static_cast<double>(value)) ||
            std::abs(static_cast<double>(value)) > 1000000) {
            throw std::runtime_error("Invalid UI number");
        }
    } else if constexpr (Optional<T>::value) {
        if (value) {
            check(*value);
        }
    } else if constexpr (Map<T>::value) {
        if (value.size() > 4096) {
            throw std::runtime_error("Too many UI entries");
        }
        for (auto &entry : value) {
            auto key = entry.first;
            check(key);
            check(entry.second);
        }
    } else if constexpr (Vector<T>::value) {
        if (value.size() > 4096) {
            throw std::runtime_error("Too many UI entries");
        }
        for (auto &item : value) {
            check(item);
        }
    } else if constexpr (!std::is_enum_v<T>) {
        Checker checker;
        fields(value, checker);
    }
}

void budget(lua_State *L, lua_Debug *) {
    luaL_error(L, "UI definition instruction limit exceeded");
}

void require(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
void validateColumns(const std::vector<double> &columns) {
    double total = 0;

    for (double width : columns) {
        require(width > 0, "Invalid table column width");
        total += width;
    }

    require(std::isfinite(total) && total <= 1000000, "Invalid table width");
}

void validateStyle(const OptionStyleOverrides &style) {
    require(!style.textSize || *style.textSize > 0, "Invalid option text size");
    require(!style.borderWidth || *style.borderWidth >= 0,
            "Invalid option border width");
}

void validateStyle(const StyleOverrides &style) {
    require(!style.textSize || *style.textSize > 0, "Invalid text size");
    require(!style.rowStep || *style.rowStep > 0, "Invalid row step");
    require(!style.padding || *style.padding >= 0, "Invalid padding");
    require(!style.pickGap || *style.pickGap >= 0, "Invalid pick gap");
    require(!style.panelBorderWidth || *style.panelBorderWidth >= 0,
            "Invalid panel border width");
}
} // namespace

std::string luaQuote(const std::string &text) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') {
            out << '\\' << c;
        } else if (c < 32 || c == 127) {
            out << '\\' << std::setw(3) << std::setfill('0') << unsigned(c);
        } else {
            out << c;
        }
    }
    out << '"';

    return out.str();
}

void writeTextFile(const std::filesystem::path &path, const std::string &text) {
    static std::atomic<unsigned long long> serial{0};

    auto temporary = path;
    temporary +=
        ".tmp-" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) +
        "-" + std::to_string(serial.fetch_add(1));

    try {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file.exceptions(std::ios::badbit | std::ios::failbit);
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        file.close();
        std::filesystem::rename(temporary, path);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

MenuDefinitions defaultModals() {
    MenuDefinition rename;
    rename.id = "uiRenameMenu";
    rename.modal = true;
    rename.background = "data/sprites/interfacebg.tga";

    OptionDefinition bar;
    bar.id = "title";
    bar.type = CMenu::drag_bar;
    bar.x = -.1;
    bar.y = .15;
    bar.listWidth = 1.5;
    bar.group = 2;
    bar.style.textSize = .1;
    bar.style.textColor = Color{63.0 / 255, 81.0 / 255, 181.0 / 255, 1};

    OptionDefinition name;
    name.id = "name";
    name.type = CMenu::text_input;
    name.text = "aaaa";
    name.value = "type here";
    name.y = .05;

    OptionDefinition close;
    close.id = "close";
    close.text = "Close";
    close.y = -.5;
    close.back = true;
    close.style.textColor = Color{1, 0, 0, 1};
    rename.options = {bar, name, close};

    auto profile = rename;
    profile.id = "uiCreateProfileMenu";

    OptionDefinition path;
    path.id = "path";
    path.text = "Path:";
    path.type = CMenu::static_text;
    path.y = -.05;
    profile.options.insert(profile.options.end() - 1, path);

    return {rename, profile};
}

void validate(const MenuDefinitions &definitions) {
    auto checked = definitions;
    check(checked);
    std::set<std::string> ids;
    for (const auto &menu : definitions) {
        require(!menu.id.empty() && ids.insert(menu.id).second,
                "Empty/duplicate menu id");
        require(menu.flow >= CMenu::Flow::Manual &&
                    menu.flow <= CMenu::Flow::Horizontal,
                "Invalid menu flow");
        require(menu.viewport.left <= menu.viewport.right &&
                    menu.viewport.bottom <= menu.viewport.top,
                "Invalid viewport");
        require(menu.panel.left <= menu.panel.right &&
                    menu.panel.bottom <= menu.panel.top,
                "Invalid panel");

        validateStyle(menu.style);
        validateColumns(menu.tableColumns);

        std::set<std::string> controls;
        for (const auto &o : menu.options) {
            const std::string prefix = menu.id + ".";
            const std::string qualified =
                o.id.compare(0, prefix.size(), prefix) == 0 ? o.id
                                                            : prefix + o.id;
            require(!o.id.empty() && controls.insert(qualified).second,
                    "Empty/duplicate control id");
            require(o.type >= CMenu::button_ok && o.type <= CMenu::panel,
                    "Invalid control type");
            require(o.fieldWidth > 0 && o.listWidth >= 0 &&
                        o.maxBytes <= 1000000,
                    "Invalid control dimensions");
            require(o.width >= 0 && o.height >= 0, "Invalid box dimensions");
            if (o.type == CMenu::image || o.type == CMenu::panel) {
                require(o.width > 0 && o.height > 0,
                        "Image/panel requires dimensions");
            }
            if (o.type == CMenu::table_row) {
                require(!menu.tableColumns.empty() &&
                            o.cells.size() <= menu.tableColumns.size(),
                        "Table row requires enough columns");
            }
            validateStyle(o.style);
        }

        if (menu.id == "uiRenameMenu" || menu.id == "uiCreateProfileMenu") {
            require(menu.modal, "Built-in dialog must remain modal");
            const auto has = [&](const std::string &id, CMenu::menusOPT type) {
                for (const auto &o : menu.options) {
                    if (o.id == id && o.type == type) {
                        return true;
                    }
                }
                return false;
            };
            require(has("name", CMenu::text_input),
                    "Modal requires text_input 'name'");
            if (menu.id == "uiCreateProfileMenu") {
                require(has("path", CMenu::static_text),
                        "Profile modal requires static_text 'path'");
            }
            bool closes = false;
            for (const auto &o : menu.options) {
                closes = closes || (o.back && !o.disabled && o.enableEnter &&
                                    (o.type == CMenu::textbtn ||
                                     o.type == CMenu::button_ok ||
                                     o.type == CMenu::button_cancel));
            }
            require(closes, "Modal requires an enabled close button");
        }
    }
}

MenuDefinitions readMenus(const std::filesystem::path &path) {
    std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(),
                                                           lua_close);
    auto *L = state.get();
    if (!L) {
        throw std::runtime_error("Cannot create Lua state");
    }
    lua_sethook(L, budget, LUA_MASKCOUNT, 1000000);
    if (luaL_loadfilex(L, path.string().c_str(), "t") ||
        lua_pcall(L, 0, 1, 0)) {
        const char *message = lua_tostring(L, -1);
        throw std::runtime_error(message ? message
                                         : "Lua raised a non-string error");
    }
    MenuDefinitions result;
    read(L, result);
    validate(result);
    return result;
}

void writeMenus(const std::filesystem::path &path,
                const MenuDefinitions &definitions) {
    validate(definitions);
    auto copy = definitions;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17)
        << "-- Guitar++ GUI definitions. Actions are host-owned "
           "identifiers.\nreturn ";
    write(out, copy);
    out << '\n';
    writeTextFile(path, out.str());
}

void validateOverrides(const MenuOverrideSet &input) {
    auto checked = input;
    check(checked);

    for (const auto &entry : input) {
        require(!entry.first.empty(), "Empty menu id");
        const auto &m = entry.second;
        require(!(m.flowX && m.anchorLeft.value_or(false)),
                "Conflicting flow origin and left anchor");

        if (m.flow) {
            require(*m.flow >= CMenu::Flow::Manual &&
                        *m.flow <= CMenu::Flow::Horizontal,
                    "Invalid flow");
        }

        for (const auto &rect : {m.viewport, m.panel}) {
            if (rect) {
                require(rect->left <= rect->right && rect->bottom <= rect->top,
                        "Invalid presentation rectangle");
            }
        }

        require(!(m.panelBounds && m.panelFollowsViewport.value_or(false)),
                "Conflicting panel bounds and viewport following");
        require(!m.panelFollowsViewport || *m.panelFollowsViewport ||
                    m.panelBounds,
                "Independent panel bounds must be explicit");
        if (m.panelBounds) {
            require(m.panelBounds->left < m.panelBounds->right &&
                        m.panelBounds->bottom < m.panelBounds->top,
                    "Invalid panel bounds");
        }

        validateStyle(m.style);
        if (m.tableColumns) {
            validateColumns(*m.tableColumns);
        }

        for (const auto &item : m.options) {
            require(!item.first.empty(), "Empty option id");
            const auto &o = item.second;
            require(!o.fieldWidth || *o.fieldWidth > 0, "Invalid field width");
            require(!o.listWidth || *o.listWidth >= 0, "Invalid list width");
            validateStyle(o.style);
            require((!o.width || *o.width >= 0) &&
                        (!o.height || *o.height >= 0),
                    "Invalid box dimensions");

            const int horizontal = int(o.x.has_value()) +
                                   int(o.centerX.has_value()) +
                                   int(o.anchorRight.value_or(false));
            require(horizontal <= 1, "Conflicting horizontal presentation");
        }
    }
}

MenuOverrideSet readOverrides(const std::filesystem::path &path) {
    std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(),
                                                           lua_close);
    auto *L = state.get();
    if (!L) {
        throw std::runtime_error("Cannot create Lua state");
    }

    lua_sethook(L, budget, LUA_MASKCOUNT, 1000000);

    if (luaL_loadfilex(L, path.string().c_str(), "t") ||
        lua_pcall(L, 0, 1, 0)) {
        const char *message = lua_tostring(L, -1);
        throw std::runtime_error(message ? message
                                         : "Lua raised a non-string error");
    }

    MenuOverrideSet result;
    read(L, result);
    validateOverrides(result);

    return result;
}

void writeOverrides(const std::filesystem::path &path,
                    const MenuOverrideSet &input) {
    validateOverrides(input);

    auto copy = input;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17)
        << "-- Guitar++ presentation overrides.\nreturn ";
    write(out, copy);
    out << '\n';

    writeTextFile(path, out.str());
}

std::unique_ptr<CMenu> instantiate(const MenuDefinition &d,
                                   CMenu::Context *context,
                                   const TextureResolver &textures) {
    validate({d});

    auto m = std::make_unique<CMenu>(d.id, context);
    m->mTitle = d.title;
    m->x = d.x;
    m->y = d.y;
    m->modal = d.modal;
    m->gameMenu = true;

    m->styleOverrides = d.style;
    m->tableColumns = d.tableColumns;

    m->setFlow(d.flow, d.flowX, d.flowY, d.flowGroup);
    m->anchorLeft = d.anchorLeft;
    m->leftMargin = d.leftMargin;

    m->viewportEnabled = d.viewportEnabled;
    m->viewport = d.viewport;

    m->qbgd.x1 = m->qbgd.x4 = d.panel.left;
    m->qbgd.x2 = m->qbgd.x3 = d.panel.right;
    m->qbgd.y1 = m->qbgd.y2 = d.panel.top;
    m->qbgd.y3 = m->qbgd.y4 = d.panel.bottom;
    m->qbgd.alphaTop = m->qbgd.alphaBottom = 1;
    m->qbgd.Text =
        textures && !d.background.empty() ? textures(d.background) : 0;

    for (const auto &o : d.options) {
        CMenu::menuOpt opt;
        opt.optionName = o.id;
        opt.type = o.type;
        opt.width = o.width;
        opt.height = o.height;
        opt.cells = o.cells;
        opt.active = o.active;

        opt.text = o.text;
        opt.preText = o.value;
        opt.langEntryKey = o.languageKey;

        opt.x = o.x;
        opt.y = o.y;
        opt.group = o.group;
        opt.shortcutKey = o.shortcut;
        opt.fieldWidth = o.fieldWidth;
        opt.deslizantBarSize = o.listWidth;
        opt.disabled = o.disabled;
        opt.manualPosition = o.manual;
        opt.anchorRight = o.anchorRight;
        opt.centerX = o.centerX;

        opt.goback = o.back;
        opt.enableEnter = o.enableEnter;
        opt.preTextMaxSize = o.maxBytes;

        opt.styleOverrides = o.style;
        opt.optList.assign(o.items.begin(), o.items.end());
        opt.menusXRef.assign(o.actions.begin(), o.actions.end());
        opt.texture = textures && !o.texture.empty() ? textures(o.texture) : 0;

        if (m->addOpt(opt) < 0) {
            throw std::runtime_error("Cannot add UI option: " + o.id);
        }
    }
    return m;
}
} // namespace ui
