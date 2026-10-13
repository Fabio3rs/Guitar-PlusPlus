#include "CChart.h"
#include "CMenuSystem.h"
#include "GPPGame.h"
#include "QuickPlayView.h"
#include <chrono>
#include <set>

namespace {
void addBack(CMenu &menu) {
    CMenu::menuOpt option;
    option.optionName = "back";
    option.text = "Voltar";
    option.x = -.8;
    option.y = .85;
    option.styleOverrides.textSize = .1;
    option.styleOverrides.textColor = ui::Color{.5, .5, .5, 1};
    option.type = CMenu::textbtn;
    option.goback = true;
    option.shortcutKey = GLFW_KEY_ESCAPE;
    menu.addOpt(option);
}
} // namespace

void CMenuSystem::selectPlayerMenu() {
    auto &menu = newNamedMenu("playerSelectMenu");
    menu.clearOptions();
    host.resetMainPlayer();
    addBack(menu);

    // Saved-profile selection is still unimplemented. Keep the existing default
    // player and creation dialog; do not perform the old disabled worker scan.
    auto action = host.getCallBackRealName("menuCreateNewPlayerProfile");
    if (action.empty()) {
        action = host.addGameCallbacks(
            "menuCreateNewPlayerProfile", [this](const std::string &) {
                auto &owner = getMenuByName("playerSelectMenu");
                if (owner.getUIListSize() == 0) {
                    modals.openCreateProfile(owner);
                }
            });
    }
    CMenu::menuOpt option;
    option.optionName = "createProfile";
    option.text = "Criar novo perfil";
    option.anchorRight = true;
    option.x = .6;
    option.y = .85;
    option.styleOverrides.textSize = .1;
    option.type = CMenu::textbtn;
    option.menusXRef.push_back(action);
    menu.addOpt(option);
    openMenus(&menu);
}

namespace {
std::string durationText(const songs::SongRecord &song) {
    if (!song.duration) {
        return "--:--";
    }
    const auto seconds = static_cast<uint64_t>(*song.duration);
    return (song.durationSource == "chart-estimate" ? "~" : "") +
           std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") +
           std::to_string(seconds % 60);
}

std::string progressText(const songs::ScanProgress &progress) {
    std::string text = progress.running     ? "Atualizando biblioteca: "
                       : progress.cancelled ? "Atualização interrompida: "
                                            : "Biblioteca: ";
    text += std::to_string(progress.examined) + " arquivos | " +
            std::to_string(progress.added) + " novas | " +
            std::to_string(progress.modified) + " atualizadas | " +
            std::to_string(progress.removed) + " removidas";
    if (!progress.warning.empty()) {
        text += " | " + progress.warning;
    }
    return text;
}
} // namespace

std::string CMenuSystem::selectSong() {
    auto &menu = newNamedMenu("selectSongMenu");
    std::string selected;
    std::set<std::string> requestedCovers;
    auto &engine = CEngine::engine();
    ui::QuickPlayView view(
        menu,
        engine.windowHeight > 0 ? engine.windowWidth / engine.windowHeight
                                : 1.6,
        [this](CMenu &panel) { preparePresentation(panel); },
        [this, &requestedCovers](const std::string &relative) {
            const auto path = std::filesystem::path("data/songs") / relative;
            if (!requestedCovers.count(relative)) {
                if (host.loadTextureSingleAsync(GPPGame::loadTextureBatch(
                        path.parent_path().string(), path.filename().string(),
                        nullptr, {}))) {
                    requestedCovers.insert(relative);
                }
            }
            const auto info = host.getTextureInfo(
                path.parent_path().string() + "/" + path.filename().string());
            return ui::QuickPlayView::CoverImage{info.id, info.width,
                                                 info.height};
        });
    host.loadTexture("data/sprites", "guitarpp-logo.tga");
    const auto logo = host.getTextureInfo("data/sprites/guitarpp-logo.tga");
    view.setLogo({logo.id, logo.width, logo.height});
    songLibrary.refresh();
    // Ensure exceptions/window close cannot leave a scan using services during
    // shutdown. Normal exit below keeps drawing while cancellation completes.
    struct JoinScan {
        songs::SongLibrary &library;
        explicit JoinScan(songs::SongLibrary &value) : library(value) {}
        JoinScan(const JoinScan &) = delete;
        JoinScan &operator=(const JoinScan &) = delete;
        JoinScan(JoinScan &&) = delete;
        JoinScan &operator=(JoinScan &&) = delete;
        ~JoinScan() {
            library.cancel();
            library.wait();
        }
    } join{songLibrary};
    openMenus(
        &menu,
        [&]() {
            view.synchronize(songLibrary.snapshot());
            return 0;
        },
        [&]() {
            selected = view.interact();
            if (const auto mode = view.takeRefresh()) {
                songLibrary.refresh(*mode);
            }
            return selected.empty() ? 0 : 1;
        });
    songLibrary.cancel();
    while (songLibrary.running() && CEngine::engine().windowOpened()) {
        waitForScreen(.01);
    }
    songLibrary.wait();
    return selected;
}

void CMenuSystem::addSongListToMenu(CMenu &menu,
                                    std::map<int, std::string> &songs) {
    buildSongList(menu, songs, *songLibrary.snapshot());
}

void CMenuSystem::buildSongList(CMenu &menu, std::map<int, std::string> &songs,
                                const songs::LibrarySnapshot &snapshot) {
    menu.clearOptions();
    songs.clear();
    menu.setFlow(CMenu::Flow::Vertical, -.85, .65);
    menu.viewportEnabled = true;
    menu.viewport = {-1.0, -.78, 1.0, .72};
    menu.tableColumns = {.69, .035, .465, .035, .22, .035, .23};
    addBack(menu);
    for (const bool full : {false, true}) {
        CMenu::menuOpt option;
        option.optionName = full ? "verify" : "refresh";
        option.text = full ? "Ctrl+F5  Verificar tudo" : "F5  Atualizar";
        option.x = full ? .25 : -.4;
        option.y = .85;
        option.styleOverrides.textSize = .065;
        option.type = CMenu::textbtn;
        option.shortcutKey = GLFW_KEY_F5;
        option.shortcutMods = full ? GLFW_MOD_CONTROL : 0;
        menu.addOpt(option);
    }
    CMenu::menuOpt status;
    status.optionName = "scanStatus";
    status.type = CMenu::static_text;
    status.x = -.9;
    status.y = -.88;
    status.width = 1.8;
    status.styleOverrides.textSize = .045;
    status.text = progressText(snapshot.progress);
    menu.addOpt(status);
    populateSongs(menu, songs, false, snapshot);
}

void CMenuSystem::populateSongs(CMenu &menu, std::map<int, std::string> &songs,
                                bool returnOnSelection,
                                const songs::LibrarySnapshot &snapshot) {
    std::vector<CMenu::menuOpt> rows;
    rows.reserve(snapshot.songs.size());
    for (const auto &song : snapshot.songs) {
        CMenu::menuOpt option;
        option.optionName = "song:" + song.path;
        option.text = song.title;
        option.cells = {song.title,
                        "",
                        song.artist,
                        "",
                        durationText(song),
                        "",
                        song.available ? song.format : "Indisponível"};
        option.styleOverrides.textSize = .065;
        option.group = 1;
        option.type = CMenu::table_row;
        option.goback = returnOnSelection;
        option.disabled = !song.available;
        rows.push_back(std::move(option));
    }
    const auto indices = menu.addOptions(rows);
    for (size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] >= 0) {
            songs[indices[i]] = snapshot.songs[i].path;
        }
    }
    if (snapshot.songs.empty()) {
        CMenu::menuOpt empty;
        empty.optionName = "emptyLibrary";
        empty.text = "Nenhuma música indexada. F5 para atualizar.";
        empty.type = CMenu::static_text;
        empty.group = 1;
        empty.styleOverrides.textSize = .07;
        menu.addOpt(empty);
    }
}

bool CMenuSystem::updateSongRows(CMenu &menu,
                                 const std::map<int, std::string> &rows,
                                 const songs::LibrarySnapshot &snapshot) {
    if (rows.size() != snapshot.songs.size()) {
        return false;
    }
    size_t index = 0;
    for (const auto &row : rows) {
        if (row.first < 0 ||
            static_cast<size_t>(row.first) >= menu.options.size() ||
            row.second != snapshot.songs[index++].path) {
            return false;
        }
    }
    index = 0;
    for (const auto &row : rows) {
        const auto &song = snapshot.songs[index++];
        auto &option = menu.options[static_cast<size_t>(row.first)];
        option.text = song.title;
        option.cells = {song.title,
                        "",
                        song.artist,
                        "",
                        durationText(song),
                        "",
                        song.available ? song.format : "Indisponível"};
        option.disabled = !song.available;
        // Preserve presentation overrides and all existing layout caches.
        // Disabled rows already get the renderer's disabled opacity.
    }
    return true;
}
