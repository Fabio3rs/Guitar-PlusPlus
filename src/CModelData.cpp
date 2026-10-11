#include "CModelData.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

std::map<std::string, CModelMaterial>
readModelMaterials(const std::string &name) {
    const fs::path file(name);
    std::map<std::string, CModelMaterial> materials;
    std::ifstream input(file);
    if (!input) {
        return materials; // OBJ files without an accompanying MTL remain
                          // usable.
    }
    for (auto &entry : ObjModel::readMaterials(input)) {
        auto &m = entry.second;
        // Preserve the project's historic material-name.tga convention only
        // when that file really exists; absent textures use neutral white.
        if (m.diffuseMap.empty()) {
            const auto name = fs::path(entry.first).extension() == ".tga"
                                  ? entry.first
                                  : entry.first + ".tga";
            if (fs::is_regular_file(file.parent_path() / name)) {
                m.diffuseMap = name;
            }
        }
        if (!m.diffuseMap.empty()) {
            const auto texture = file.parent_path() / m.diffuseMap;
            m.diffuseMap = texture.filename().string();
            materials[entry.first] = {m, texture.parent_path().string()};
        } else {
            materials[entry.first] = {m, file.parent_path().string()};
        }
    }
    return materials;
}

CModelData readModelData(const CModelRequest &request) {
    const fs::path filename(request.file);
    std::ifstream input(filename);
    if (!input) {
        throw std::runtime_error("Cannot open model: " + request.file);
    }
    CModelData data;
    data.model = ObjModel::read(input, request.object);
    if (data.model.parts.empty()) {
        throw std::runtime_error("No geometry in model: " + request.file);
    }
    try {
        data.ao = SceneAO::read(filename, data.model);
    } catch (const std::exception &error) {
        data.model.warnings.push_back(std::string("AO disabled: ") +
                                      error.what());
    }
    if (!data.ao.coordinates.empty()) {
        size_t begin = 0;
        for (auto &part : data.model.parts) {
            const size_t end = begin + part.positions.size();
            if (end > data.ao.coordinates.size()) {
                throw std::runtime_error("Invalid AO coordinates");
            }
            part.aoUVs.assign(data.ao.coordinates.begin() +
                                  static_cast<std::ptrdiff_t>(begin),
                              data.ao.coordinates.begin() +
                                  static_cast<std::ptrdiff_t>(end));
            begin = end;
        }
    }
    auto library = [&](const std::string &file) {
        for (auto &entry : readModelMaterials(file)) {
            data.materials[entry.first] = std::move(entry.second);
        }
    };
    if (!request.materialLibrary.empty()) {
        library(request.materialLibrary);
    } else {
        for (const auto &name : data.model.libraries) {
            library((filename.parent_path() / name).string());
        }
    }
    return data;
}
