#include "fonts.h"
#include "CUIDefinition.h"
#include "utf8.h"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace GuiStudio {
namespace {
struct Import {
    FontDefinitions fonts;
    std::string error;
    int callback = LUA_NOREF;
};

std::string string(lua_State *L, int index) {
    if (lua_type(L, index) != LUA_TSTRING) {
        throw std::runtime_error("Expected UTF-8 string");
    }
    size_t size = 0;
    const char *data = lua_tolstring(L, index, &size);
    std::string result(data, size);
    if (size > 1024 * 1024 || CFonts::utf8Size(result) == std::string::npos) {
        throw std::runtime_error("Invalid or oversized UTF-8 string");
    }
    return result;
}

double number(lua_State *L, int index) {
    if (lua_type(L, index) != LUA_TNUMBER ||
        !std::isfinite(lua_tonumber(L, index))) {
        throw std::runtime_error("Expected finite number");
    }
    return lua_tonumber(L, index);
}

int command(lua_State *L) {
    auto &input =
        *static_cast<Import *>(lua_touserdata(L, lua_upvalueindex(1)));
    const int operation =
        static_cast<int>(lua_tointeger(L, lua_upvalueindex(2)));
    try {
        if (operation == 4) {
            if (lua_gettop(L) != 2) {
                throw std::runtime_error(
                    "setCallBackToEvent expects two arguments");
            }
            if (string(L, 1) != "applyTheme" || !lua_isfunction(L, 2)) {
                throw std::runtime_error(
                    "Only applyTheme callbacks are supported");
            }
            luaL_unref(L, LUA_REGISTRYINDEX, input.callback);
            lua_pushvalue(L, 2);
            input.callback = luaL_ref(L, LUA_REGISTRYINDEX);
        } else {
            if (operation != 0 && lua_gettop(L) != 4) {
                throw std::runtime_error("Font metrics expect four arguments");
            }
            const std::string name = string(L, 1);
            if (name.empty()) {
                throw std::runtime_error("Empty font name");
            }
            FontDefinition *font = nullptr;
            for (auto &item : input.fonts) {
                if (item.name == name) {
                    font = &item;
                }
            }
            if (!font && operation == 0) {
                input.fonts.push_back({});
                font = &input.fonts.back();
                font->name = name;
                if (name == "default") {
                    font->metrics = {14.0 / 64, 50.0 / 64, 14.0 / 64};
                }
            }
            if (!font) {
                throw std::runtime_error("Unknown font: " + name);
            }
            if (operation == 0) {
                AtlasDefinition atlas{
                    string(L, 2), string(L, 3), string(L, 4), {}};
                if (lua_gettop(L) != 4 && lua_gettop(L) != 6) {
                    throw std::runtime_error(
                        "addTextureToFont expects 4 or 6 arguments");
                }
                if (lua_gettop(L) == 6) {
                    const double columns = number(L, 5), rows = number(L, 6);
                    if (columns < 1 || rows < 1 || columns > 4096 ||
                        rows > 4096 || std::floor(columns) != columns ||
                        std::floor(rows) != rows) {
                        throw std::runtime_error("Invalid atlas grid");
                    }
                    atlas.grid = {static_cast<int>(columns),
                                  static_cast<int>(rows)};
                } else {
                    int columns = 0, rows = 1, current = 0;
                    for (auto it = atlas.mapping.begin();
                         it != atlas.mapping.end();) {
                        if (utf8::next(it, atlas.mapping.end()) == '\n') {
                            columns = std::max(columns, current);
                            current = 0;
                            ++rows;
                        } else {
                            ++current;
                        }
                    }
                    atlas.grid = {std::max(columns, current), rows};
                }
                for (auto it = atlas.mapping.begin();
                     it != atlas.mapping.end();) {
                    font->adjustments.erase(
                        utf8::next(it, atlas.mapping.end()));
                }
                bool replaced = false;
                for (auto &old : font->atlases) {
                    if (old.path == atlas.path && old.file == atlas.file) {
                        old = atlas;
                        replaced = true;
                    }
                }
                if (!replaced) {
                    font->atlases.push_back(std::move(atlas));
                }
            } else if (operation == 1) {
                font->metrics = {number(L, 2), number(L, 3), number(L, 4)};
                if (font->metrics.ascent < 0 || font->metrics.descent < 0 ||
                    font->metrics.ascent + font->metrics.descent <= 0) {
                    throw std::runtime_error("Invalid face metrics");
                }
            } else {
                const auto text = string(L, 2);
                const double a = number(L, 3), b = number(L, 4);
                if (text.empty() || (operation == 2 ? a <= 0 : a < 0)) {
                    throw std::runtime_error("Invalid glyph metrics");
                }
                for (auto it = text.begin(); it != text.end();) {
                    auto &adjustment =
                        font->adjustments[utf8::next(it, text.end())];
                    if (operation == 2) {
                        adjustment.height = a;
                        adjustment.offset = b;
                    } else {
                        adjustment.advance = a;
                        adjustment.bearing = b;
                    }
                }
            }
        }
        lua_pushboolean(L, true);
    } catch (const std::exception &e) {
        input.error = e.what();
        lua_pushboolean(L, false);
    }
    return 1;
}

void budget(lua_State *L, lua_Debug *) {
    luaL_error(L, "Font script instruction limit exceeded");
}
} // namespace

std::string character(unsigned cp) {
    std::string text;
    utf8::append(cp, std::back_inserter(text));
    return text;
}

FontDefinitions defaultFonts() {
    FontDefinition font;
    font.name = "default";
    font.metrics = {14.0 / 64, 50.0 / 64, 14.0 / 64};
    font.atlases.push_back(
        {"data/sprites",
         "FONT.tga",
         "0123456789ABCDEFGHIJKLMNOPQRSTUV\nWXYZabcdefghijklmnopqrstuvwxyz.,\n"
         "ÃÂÁÀÄÊÉÈËÎÍÌÏÕÔÓÒÖŨÛÚÙÜãâáàäêéèë\n"
         "îíìïõôóòöũûúùüÑñÇç:\\|/<>()-+=*[]\n?!{}@#$%&ªº£¬_",
         {32, 8}});
    return {font};
}

FontDefinitions readFonts(const std::filesystem::path &path) {
    std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(),
                                                           lua_close);
    auto *L = state.get();
    if (!L) {
        throw std::runtime_error("Cannot create Lua state");
    }
    // Only Lua's base/string/math/table helpers, without file/process loaders.
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);
    for (const char *name :
         {"dofile", "loadfile", "load", "collectgarbage", "print"}) {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }
    luaL_requiref(L, "string", luaopen_string, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "math", luaopen_math, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "table", luaopen_table, 1);
    lua_pop(L, 1);
    Import input;
    int index = 0;
    for (const char *name :
         {"addTextureToFont", "setFontMetrics", "setFontCharMetrics",
          "setFontCharSpacing", "setCallBackToEvent"}) {
        lua_pushlightuserdata(L, &input);
        lua_pushinteger(L, index++);
        lua_pushcclosure(L, command, 2);
        lua_setglobal(L, name);
    }
    lua_sethook(L, budget, LUA_MASKCOUNT, 1000000);
    if (luaL_loadfilex(L, path.string().c_str(), "t") ||
        lua_pcall(L, 0, 0, 0)) {
        const char *message = lua_tostring(L, -1);
        throw std::runtime_error(message ? message
                                         : "Lua raised a non-string error");
    }
    if (input.callback != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, input.callback);
        if (lua_pcall(L, 0, 0, 0)) {
            const char *message = lua_tostring(L, -1);
            throw std::runtime_error(message ? message
                                             : "Lua raised a non-string error");
        }
    }
    if (!input.error.empty() || input.fonts.empty()) {
        throw std::runtime_error(input.error.empty() ? "No font definitions"
                                                     : input.error);
    }
    return input.fonts;
}

void writeFonts(const std::filesystem::path &path,
                const FontDefinitions &fonts) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17)
        << "-- Guitar++ Font Studio\nlocal function applyFonts()\n";
    for (const auto &f : fonts) {
        for (const auto &atlas : f.atlases) {
            out << "    assert(addTextureToFont(" << ui::luaQuote(f.name)
                << ", " << ui::luaQuote(atlas.path) << ", "
                << ui::luaQuote(atlas.file) << ", "
                << ui::luaQuote(atlas.mapping) << ", " << atlas.grid.columns
                << ", " << atlas.grid.rows << "))\n";
        }
        out << "    assert(setFontMetrics(" << ui::luaQuote(f.name) << ", "
            << f.metrics.baseline << ", " << f.metrics.ascent << ", "
            << f.metrics.descent << "))\n";
        for (const auto &entry : f.adjustments) {
            const auto prefix = ui::luaQuote(f.name) + ", " +
                                ui::luaQuote(character(entry.first)) + ", ";
            const auto &a = entry.second;
            if (a.height && a.offset) {
                out << "    assert(setFontCharMetrics(" << prefix << *a.height
                    << ", " << *a.offset << "))\n";
            }
            if (a.advance && a.bearing) {
                out << "    assert(setFontCharSpacing(" << prefix << *a.advance
                    << ", " << *a.bearing << "))\n";
            }
        }
    }
    out << "end\nsetCallBackToEvent(\"applyTheme\", applyFonts)\n";
    ui::writeTextFile(path, out.str());
}
} // namespace GuiStudio
