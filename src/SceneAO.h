#pragma once
#include "ObjModel.h"
#include <cstdint>
#include <filesystem>

namespace SceneAO {
struct Atlas {
    unsigned width = 0, height = 0;
    std::vector<ObjModel::Vec2> coordinates;
    std::vector<unsigned char> pixels;
};
uint64_t signature(const ObjModel::Model &model);
// Versioned optional sidecars; mismatched geometry is rejected before upload.
Atlas read(const std::filesystem::path &obj, const ObjModel::Model &model);
void write(const std::filesystem::path &obj, const ObjModel::Model &model,
           const Atlas &atlas);
// Offline only. Never called by the game loop or asynchronous model loader.
Atlas bake(const ObjModel::Model &model, unsigned samples = 32,
           float radius = .65f);
} // namespace SceneAO
