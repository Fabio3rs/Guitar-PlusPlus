#pragma once

#include "ObjModel.h"
#include "SceneAO.h"
#include <tuple>

// Owned CPU data; safe to decode without the game, Lua, or a GL context.
struct CModelRequest {
    std::string file, object, materialLibrary;
    bool operator<(const CModelRequest &other) const {
        return std::tie(file, object, materialLibrary) <
               std::tie(other.file, other.object, other.materialLibrary);
    }
};

struct CModelMaterial {
    ObjModel::Material values;
    std::string directory;
};

struct CModelData {
    ObjModel::Model model;
    SceneAO::Atlas ao;
    std::map<std::string, CModelMaterial> materials;
};

std::map<std::string, CModelMaterial>
readModelMaterials(const std::string &file);
CModelData readModelData(const CModelRequest &request);
