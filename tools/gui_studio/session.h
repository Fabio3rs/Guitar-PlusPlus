#pragma once
#include "CModalManager.h"
#include "fonts.h"
#include <GL/glew.h>
#include <filesystem>

namespace GuiStudio {
namespace fs = std::filesystem;
struct Texture {
    CEngine::GLFWimage image;
    unsigned id = 0;
    ~Texture();
};

class Session {
    std::map<std::string, std::shared_ptr<Texture>> textures;
    std::unique_ptr<CFonts> fontRegistry;
    std::unique_ptr<CMenu> root;
    std::shared_ptr<Texture> texture(const std::string &);
    std::string renameValue = "Guitar++";
    std::vector<int> navigationHistory;

  public:
    fs::path assets, output;
    CMenu::Context context;
    CModalManager modals{&context};
    ui::MenuDefinitions menus = ui::defaultModals();
    FontDefinitions fonts;
    std::vector<std::vector<std::shared_ptr<Texture>>> atlasTextures;
    int selectedMenu = 0, selectedOption = 0, selectedFont = 0,
        selectedAtlas = 0;
    unsigned selectedGlyph = 'g';
    size_t fontGeneration = 0;
    bool testing = false, dirtyMenus = false, dirtyFonts = false;
    std::string error, fontSource, menuSource;
    std::vector<std::string> events;

    Session(fs::path assetRoot, fs::path outputRoot);
    ~Session();
    void loadFonts(const fs::path &);
    void applyFonts(FontDefinitions);
    void loadMenus(const fs::path &);
    void saveFonts(const fs::path &);
    void saveMenus(const fs::path &);
    void rebuild(bool keepNavigation = false);
    void update();
    void render();
    CMenu *preview();
    CFonts &registry() { return *fontRegistry; }
    void setFont(size_t);
    void applyGlyph(unsigned, const CFonts::Glyph &);
    void applyMetrics(CFonts::FaceMetrics);
    void log(const std::string &);
};
} // namespace GuiStudio
