#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <string>
#include <vector>

namespace ChartStudio {
// UI-only, nonrecursive browser. Selecting a path never writes or opens a
// document.
struct FileBrowser {
    struct Entry {
        std::filesystem::path path;
        bool directory;
    };
    std::string *destination = nullptr;
    std::string directory, filename, filter, error;
    std::vector<std::string> extensions;
    std::vector<Entry> entries;
    bool requested = false, save = false, allFiles = false;

    void scan() {
        entries.clear();
        error.clear();
        std::error_code ec;
        std::filesystem::directory_iterator it(
            std::filesystem::u8path(directory), ec),
            end;
        while (!ec && it != end) {
            const bool folder = it->is_directory(ec);
            if (!ec && (folder || it->is_regular_file(ec))) {
                entries.push_back({it->path(), folder});
            }
            it.increment(ec);
        }
        if (ec) {
            error = ec.message();
        }
        std::sort(entries.begin(), entries.end(),
                  [](const Entry &a, const Entry &b) {
                      if (a.directory != b.directory) {
                          return a.directory > b.directory;
                      }
                      return a.path.filename().u8string() <
                             b.path.filename().u8string();
                  });
    }

    void choose(std::string &target, std::vector<std::string> types,
                bool saving) {
        destination = &target;
        extensions = std::move(types);
        save = saving;
        allFiles = false;
        filter.clear();
        const auto path = std::filesystem::u8path(target);
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec)) {
            directory = path.u8string();
            filename.clear();
        } else {
            if (!path.parent_path().empty()) {
                directory = path.parent_path().u8string();
            }
            filename = path.filename().u8string();
        }
        if (directory.empty()) {
            directory = std::filesystem::current_path().u8string();
        }
        scan();
        requested = true;
    }

    void draw() {
        if (requested) {
            ImGui::OpenPopup("Browse files");
            requested = false;
        }
        ImGui::SetNextWindowSize({720, 520}, ImGuiCond_FirstUseEver);
        if (!ImGui::BeginPopupModal("Browse files", nullptr)) {
            return;
        }
        ImGui::TextWrapped(
            "%s Selecting a path fills the field; use the corresponding Open, "
            "Load, Save or Export button afterwards.",
            save ? "Choose a destination." : "Choose an existing file.");
        if (ImGui::InputText("Folder", &directory,
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            scan();
        }
        if (ImGui::Button("Go to folder")) {
            scan();
        }
        ImGui::SameLine();
        if (ImGui::Button("Parent folder")) {
            directory =
                std::filesystem::u8path(directory).parent_path().u8string();
            scan();
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            scan();
        }
        ImGui::InputTextWithHint("Filter", "Part of file or folder name",
                                 &filter);
        ImGui::Checkbox("Show all file types", &allFiles);
        ImGui::BeginChild("entries", {0, -100}, ImGuiChildFlags_Borders);
        std::filesystem::path navigate;
        for (const auto &entry : entries) {
            const auto name = entry.path.filename().u8string();
            auto ext = entry.path.extension().u8string();
            std::transform(
                ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return char(std::tolower(c)); });
            if (!filter.empty() && name.find(filter) == std::string::npos) {
                continue;
            }
            if (!entry.directory && !allFiles &&
                std::find(extensions.begin(), extensions.end(), ext) ==
                    extensions.end()) {
                continue;
            }
            ImGui::PushID(name.c_str());
            const auto label = (entry.directory ? "[Folder] " : "") + name;
            if (ImGui::Selectable(label.c_str(),
                                  !entry.directory && filename == name)) {
                if (entry.directory) {
                    navigate = entry.path;
                } else {
                    filename = name;
                }
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (!navigate.empty()) {
            directory = navigate.u8string();
            filename.clear();
            scan();
        }
        ImGui::InputText("File name", &filename);
        if (!error.empty()) {
            ImGui::TextWrapped("%s", error.c_str());
        }
        ImGui::BeginDisabled(filename.empty());
        if (ImGui::Button("Use this path")) {
            const auto path = std::filesystem::u8path(directory) /
                              std::filesystem::u8path(filename);
            std::error_code ec;
            if (save || std::filesystem::is_regular_file(path, ec)) {
                *destination = path.u8string();
                ImGui::CloseCurrentPopup();
            } else {
                error = "File does not exist or cannot be read.";
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
};
} // namespace ChartStudio
