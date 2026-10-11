#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <map>
#include <string>
#include <vector>

// CPU-only OBJ/MTL data. No GL handles or pointers into another container.
namespace ObjModel {
using Vec2 = std::array<float, 2>;
using Vec3 = std::array<float, 3>;

struct Material {
    Vec3 ambient{1.f, 1.f, 1.f};
    Vec3 diffuse{1.f, 1.f, 1.f};
    Vec3 specular{0.f, 0.f, 0.f};
    Vec3 emission{0.f, 0.f, 0.f};
    float shininess = 32.f;
    float opacity = 1.f;
    float opticalDensity = 1.f;
    int illumination = 2;
    std::string diffuseMap;
    Vec2 uvScale{1.f, 1.f};
    Vec2 uvOffset{0.f, 0.f};
    bool clampMap = false;
};

struct Part {
    std::string object, group, material;
    // All arrays have the same number of elements, a multiple of three.
    std::vector<Vec3> positions, normals;
    std::vector<Vec2> uvs;
    std::vector<Vec2> aoUVs; // Optional sidecar coordinates, independent of
                             // tiled diffuse UVs.
    bool textured = true;
};

struct Model {
    uint64_t sourceSignature = 14695981039346656037ull;
    std::vector<Part> parts;
    std::vector<std::string> libraries, warnings;
    Vec3 minimum{}, maximum{};
};

// Invalid geometry throws with a line number; missing optional attributes are
// synthesized. Parsing never calls GL or partially replaces a live model.
Model read(std::istream &input, const std::string &objectFilter = "");
std::map<std::string, Material> readMaterials(std::istream &input);
} // namespace ObjModel
