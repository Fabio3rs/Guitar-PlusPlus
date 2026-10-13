#include "session.h"
#include <cmath>
#include <stdexcept>

namespace GuiStudio {
Texture::~Texture() {
    if (id) {
        CEngine::engine().destroyTexture(id);
    }
}

namespace {
std::map<std::string, std::string> readTranslations(const fs::path &directory) {
    std::map<std::string, std::string> result;
    if (!fs::is_directory(directory)) {
        return result;
    }
    std::vector<fs::path> files;
    for (const auto &entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto &file : files) {
        std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(),
                                                               lua_close);
        auto *L = state.get();
        if (!L) {
            throw std::runtime_error("Cannot create translation Lua state");
        }
        lua_sethook(
            L,
            [](lua_State *lua, lua_Debug *) {
                luaL_error(lua, "Translation instruction limit exceeded");
            },
            LUA_MASKCOUNT, 1000000);
        if (luaL_loadfilex(L, file.string().c_str(), "t") ||
            lua_pcall(L, 0, 0, 0)) {
            const char *message = lua_tostring(L, -1);
            throw std::runtime_error(message ? message
                                             : "Lua raised a non-string error");
        }
        lua_pushglobaltable(L);
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            if (lua_type(L, -2) == LUA_TSTRING &&
                lua_type(L, -1) == LUA_TSTRING) {
                result[lua_tostring(L, -2)] = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
        }
    }
    return result;
}

std::shared_ptr<Texture> readTexture(const fs::path &path) {
    auto texture = std::make_shared<Texture>();
    auto &im = texture->image;
    if (!CEngine::engine().readImage(path.string().c_str(), &im, 0)) {
        throw std::runtime_error("Cannot read TGA: " + path.string());
    }
    texture->id = CEngine::engine().uploadUITexture(im);
    return texture;
}
} // namespace

Session::Session(fs::path assetRoot, fs::path outputRoot)
    : assets(fs::absolute(assetRoot)), output(fs::absolute(outputRoot)) {
    fontRegistry = std::make_unique<CFonts>();
    context.fonts = fontRegistry.get();
    modals.setTextures(
        [this](const std::string &path) { return texture(path)->id; });
    try {
        auto translations =
            readTranslations(assets / "data/languages" / context.language);
        context.translate = [translations = std::move(translations)](
                                const std::string &key) {
            const auto found = translations.find(key);
            return found == translations.end() ? std::string() : found->second;
        };
    } catch (const std::exception &e) {
        error = e.what();
    }
    const auto theme = assets / "data/themes/gppdefaulttheme/Theme.lua";
    try {
        if (fs::exists(theme)) {
            loadFonts(theme);
        } else {
            applyFonts(defaultFonts());
        }
    } catch (const std::exception &e) {
        error = e.what();
    }
    const auto gui = assets / "data/themes/gppdefaulttheme/Gui.lua";
    if (fs::exists(gui)) {
        try {
            loadMenus(gui);
        } catch (const std::exception &e) {
            error = e.what();
        }
    }
    rebuild();
}

Session::~Session() {
    root.reset();
    fontRegistry.reset();
    atlasTextures.clear();
    textures.clear();
}

std::shared_ptr<Texture> Session::texture(const std::string &file) {
    auto path = fs::path(file);
    if (!path.is_absolute()) {
        path = assets / path;
    }
    const auto key = path.lexically_normal().string();
    auto found = textures.find(key);
    if (found != textures.end()) {
        return found->second;
    }
    auto next = readTexture(path);
    textures[key] = next;
    return next;
}

void Session::applyFonts(FontDefinitions next) {
    auto registry = std::make_unique<CFonts>();
    std::vector<std::vector<std::shared_ptr<Texture>>> resources;
    for (const auto &font : next) {
        if (font.name.empty() || font.atlases.empty()) {
            throw std::runtime_error("Font needs a name and an atlas");
        }
        resources.emplace_back();
        for (const auto &atlas : font.atlases) {
            // The editor displays an explicit grid; unlike the runtime's
            // registration API, zero cannot mean automatic dimensions here.
            if (atlas.grid.columns < 1 || atlas.grid.rows < 1 ||
                atlas.grid.columns > 4096 || atlas.grid.rows > 4096) {
                throw std::runtime_error(
                    "Atlas grid must be between 1 and 4096");
            }
            auto path = fs::path(atlas.path) / atlas.file;
            if (!path.is_absolute()) {
                path = assets / path;
            }
            auto resource = readTexture(path);
            const auto &im = resource->image;
            CFonts::ImageView view{
                im.Data.get(),
                im.DataBytes,
                im.Width,
                im.Height,
                im.BytesPerPixel,
                im.BytesPerPixel == 4
                    ? 3
                    : (im.Format == CTextureImage::PixelFormat::Alpha ? 0
                                                                      : -1)};
            if (!registry->addImageToFont(font.name, path.string(),
                                          resource->id, view, atlas.mapping,
                                          atlas.grid)) {
                throw std::runtime_error("Invalid atlas grid or mapping: " +
                                         path.string());
            }
            resources.back().push_back(std::move(resource));
        }
        if (!registry->setFontMetrics(font.name, font.metrics.baseline,
                                      font.metrics.ascent,
                                      font.metrics.descent)) {
            throw std::runtime_error("Invalid face metrics");
        }
        for (const auto &entry : font.adjustments) {
            const auto text = character(entry.first);
            const auto &a = entry.second;
            if ((a.height && a.offset &&
                 !registry->setFontCharMetrics(font.name, text, *a.height,
                                               *a.offset)) ||
                (a.advance && a.bearing &&
                 !registry->setFontCharSpacing(font.name, text, *a.advance,
                                               *a.bearing))) {
                throw std::runtime_error(
                    "Invalid adjustment or missing codepoint: " + text);
            }
        }
    }
    root.reset();
    fontRegistry = std::move(registry);
    atlasTextures = std::move(resources);
    fonts = std::move(next);
    ++fontGeneration;
    selectedFont = std::clamp(selectedFont, 0,
                              std::max(0, static_cast<int>(fonts.size()) - 1));
    selectedAtlas =
        fonts.empty()
            ? 0
            : std::clamp(
                  selectedAtlas, 0,
                  static_cast<int>(
                      fonts[static_cast<size_t>(selectedFont)].atlases.size()) -
                      1);
    context.fonts = fontRegistry.get();
    context.font = fonts.empty()
                       ? "default"
                       : fonts[static_cast<size_t>(selectedFont)].name;
    dirtyFonts = true;
}

void Session::loadFonts(const fs::path &path) {
    applyFonts(readFonts(path));
    fontSource = path.string();
    dirtyFonts = false;
    rebuild();
}

void Session::loadMenus(const fs::path &path) {
    auto next = ui::readMenus(path);
    if (next.empty()) {
        throw std::runtime_error("No menus in file");
    }
    menus = std::move(next);
    selectedMenu = selectedOption = 0;
    menuSource = path.string();
    dirtyMenus = false;
    rebuild();
}

void Session::saveFonts(const fs::path &path) {
    if (path.extension() != ".lua") {
        throw std::runtime_error("Font export requires a .lua filename");
    }
    writeFonts(path, fonts);
    fontSource = path.string();
    dirtyFonts = false;
}

void Session::saveMenus(const fs::path &path) {
    if (path.extension() != ".lua") {
        throw std::runtime_error("Menu export requires a .lua filename");
    }
    ui::writeMenus(path, menus);
    menuSource = path.string();
    dirtyMenus = false;
}

void Session::rebuild(bool keepNavigation) {
    if (!keepNavigation) {
        navigationHistory.clear();
    }
    if (menus.empty()) {
        return;
    }
    selectedMenu =
        std::clamp(selectedMenu, 0, static_cast<int>(menus.size()) - 1);
    auto &definition = menus[static_cast<size_t>(selectedMenu)];
    auto resolve = [this](const std::string &path) {
        try {
            return texture(path)->id;
        } catch (const std::exception &e) {
            error = e.what();
            return 0u;
        }
    };
    auto next = ui::instantiate(definition, &context, resolve);
    root.reset();
    context.events.clear();
    if (testing && definition.modal) {
        root = std::make_unique<CMenu>("preview-underlay", &context);
        CMenu::menuOpt option;
        option.text = "Underlying menu (blocked by modal)";
        option.type = CMenu::textbtn;
        option.x = -1;
        option.y = .7;
        root->addOpt(option);
        modals.replace(menus);
        modals.setTextures(resolve);
        if (definition.id == "uiRenameMenu") {
            modals.openRename(*root, renameValue);
        } else if (definition.id == "uiCreateProfileMenu") {
            modals.openCreateProfile(*root);
        } else {
            root->pushUserInterface(*next);
        }
    } else {
        root = std::move(next);
    }
}

void Session::log(const std::string &message) {
    if (events.size() >= 100) {
        events.erase(events.begin());
    }
    events.push_back(message);
}

void Session::update() {
    if (!root || !testing) {
        context.events.clear();
        return;
    }
    root->update();
    auto *current = preview();
    if (!current) {
        return;
    }
    for (const auto &option : current->options) {
        if ((option.status & 3) != 3) {
            continue;
        }
        if (option.goback) {
            log("Back / close");
            if (current == root.get() && !navigationHistory.empty()) {
                selectedMenu = navigationHistory.back();
                navigationHistory.pop_back();
                rebuild(true);
                return;
            }
        }
        for (const auto &action : option.menusXRef) {
            log(action);
            const bool modal = action.rfind("modal:", 0) == 0;
            const std::string target = modal ? action.substr(6)
                                       : action.rfind("menu:", 0) == 0
                                           ? action.substr(5)
                                           : std::string();
            for (size_t i = 0; !target.empty() && i < menus.size(); ++i) {
                if (menus[i].id != target) {
                    continue;
                }
                if (modal) {
                    auto model = ui::instantiate(menus[i], &context,
                                                 [this](const std::string &p) {
                                                     return texture(p)->id;
                                                 });
                    current->pushUserInterface(*model);
                } else {
                    navigationHistory.push_back(selectedMenu);
                    selectedMenu = static_cast<int>(i);
                    rebuild(true);
                }
                return;
            }
        }
    }
}

CMenu *Session::preview() {
    auto *top = CMenu::getUiMenuOnTop();
    return top ? top : root.get();
}

void Session::render() {
    if (root) {
        root->render();
        CMenu::renderUiList();
    }
}

void Session::setFont(size_t index) {
    selectedFont = static_cast<int>(index);
    selectedAtlas = 0;
    context.font = fonts.at(index).name;
    rebuild();
}

void Session::applyGlyph(unsigned code, const CFonts::Glyph &glyph) {
    if (!std::isfinite(glyph.height) || glyph.height <= 0 ||
        !std::isfinite(glyph.offset) || !std::isfinite(glyph.advance) ||
        glyph.advance < 0 || !std::isfinite(glyph.bearing)) {
        throw std::runtime_error("Invalid glyph metrics");
    }
    auto &f = fonts.at(static_cast<size_t>(selectedFont));
    const auto text = character(code);
    if (!registry().setFontCharMetrics(f.name, text, glyph.height,
                                       glyph.offset) ||
        !registry().setFontCharSpacing(f.name, text, glyph.advance,
                                       glyph.bearing)) {
        throw std::runtime_error("Missing codepoint");
    }
    f.adjustments[code] = {glyph.height, glyph.offset, glyph.advance,
                           glyph.bearing};
    dirtyFonts = true;
}

void Session::applyMetrics(CFonts::FaceMetrics metrics) {
    auto &f = fonts.at(static_cast<size_t>(selectedFont));
    if (!registry().setFontMetrics(f.name, metrics.baseline, metrics.ascent,
                                   metrics.descent)) {
        throw std::runtime_error("Invalid face metrics");
    }
    f.metrics = metrics;
    dirtyFonts = true;
}
} // namespace GuiStudio
