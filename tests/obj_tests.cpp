#include "ObjModel.h"
#include "SceneAO.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

namespace {
ObjModel::Model parse(const char *text) {
    std::istringstream input(text);
    return ObjModel::read(input);
}
TEST(ObjLoader, TriangulatesQuadAndPreservesNamesAndUVs) {
    auto m = parse("o shelf\ng steel\nusemtl metal\nv 0 0 0\nv 2 0 0\nv 2 2 "
                   "0\nv 0 2 0\nvt 0 0\nvt 2 0\nvt 2 2\nvt 0 2\nvn 0 0 1\nf "
                   "1/1/1 2/2/1 3/3/1 4/4/1\n");
    ASSERT_EQ(m.parts.size(), 1u);
    const auto &p = m.parts[0];
    EXPECT_EQ(p.object, "shelf");
    EXPECT_EQ(p.group, "steel");
    EXPECT_EQ(p.material, "metal");
    ASSERT_EQ(p.positions.size(), 6u);
    EXPECT_TRUE(p.textured);
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(p.uvs[i][0], p.positions[i][0]);
        EXPECT_FLOAT_EQ(p.normals[i][2], 1.f);
    }
    EXPECT_EQ(m.maximum, (ObjModel::Vec3{2, 2, 0}));
}
TEST(ObjLoader, MissingAttributesAndNegativeIndices) {
    auto m = parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3 -2 -1\n");
    ASSERT_EQ(m.parts.size(), 1u);
    EXPECT_FALSE(m.parts[0].textured);
    for (const auto &n : m.parts[0].normals)
        EXPECT_EQ(n, (ObjModel::Vec3{0, 0, 1}));
    EXPECT_EQ(m.parts[0].uvs.size(), 3u);
    auto uv = parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0.5\nf 1/1 2/1 3/1\n");
    EXPECT_TRUE(uv.parts[0].textured);
    EXPECT_EQ(uv.parts[0].uvs[0], (ObjModel::Vec2{.5f, 0}));
}
TEST(ObjLoader, RejectsOutOfRangeIndicesAndSkipsDegenerateFaces) {
    EXPECT_THROW(parse("v 0 0 0\nf 1 2 3\n"), std::runtime_error);
    EXPECT_THROW(parse("v 0 0 0\nf 0 1 1\n"), std::runtime_error);
    EXPECT_THROW(parse("v 0 0 0\nf 1//2 1//2 1//2\n"), std::runtime_error);
    auto m = parse("v 0 0 0\nv 1 0 0\nv 2 0 0\nf 1 2 3\n");
    EXPECT_TRUE(m.parts.empty());
    EXPECT_EQ(m.warnings.size(), 1u);
}
TEST(ObjLoader, ConcavePolygonRetainsAreaAndWinding) {
    auto m =
        parse("v 0 0 0\nv 3 0 0\nv 3 3 0\nv 1.5 1 0\nv 0 3 0\nf 1 2 3 4 5\n");
    ASSERT_EQ(m.parts.size(), 1u);
    const auto &v = m.parts[0].positions;
    ASSERT_EQ(v.size(), 9u);
    float area = 0;
    for (size_t i = 0; i < v.size(); i += 3) {
        float a = ((v[i + 1][0] - v[i][0]) * (v[i + 2][1] - v[i][1]) -
                   (v[i + 1][1] - v[i][1]) * (v[i + 2][0] - v[i][0])) *
                  .5f;
        EXPECT_GT(a, 0);
        area += a;
    }
    EXPECT_FLOAT_EQ(area, 6.f);
}
TEST(ObjLoader, SmoothingCrossesMaterialBoundariesButPreservesExplicitNormals) {
    auto m = parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\nvn 1 0 0\ns 1\nusemtl "
                   "a\nf 1 2 3\nusemtl b\nf 1//1 4 2\n");
    ASSERT_EQ(m.parts.size(), 2u);
    EXPECT_NEAR(m.parts[0].normals[0][1], std::sqrt(.5f), 1.e-6f);
    EXPECT_NEAR(m.parts[0].normals[0][2], std::sqrt(.5f), 1.e-6f);
    EXPECT_EQ(m.parts[1].normals[0], (ObjModel::Vec3{1, 0, 0}));
}
TEST(ObjLoader, FlatFacesDoNotSmoothAndMixedUVsAreSafe) {
    auto m = parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\nvt 1 1\ns off\nf 1/1 "
                   "2/1 3/1\nf 1 4 2\n");
    ASSERT_EQ(m.parts.size(), 2u);
    EXPECT_EQ(m.parts[0].normals[0], (ObjModel::Vec3{0, 0, 1}));
    EXPECT_EQ(m.parts[1].normals[0], (ObjModel::Vec3{0, 1, 0}));
    EXPECT_TRUE(m.parts[0].textured);
    EXPECT_FALSE(m.parts[1].textured);
}
TEST(ObjLoader, ReadsMaterialPropertiesAndTextureOptions) {
    std::istringstream input(
        "newmtl metal\nKa .1 .2 .3\nKd .4 .5 .6\nKs .7 .8 .9\nKe 1 2 3\nNs "
        "100\nTr .25\nNi 1.5\nillum 2\nmap_Kd -s 2 3 1 -o .1 .2 0 -clamp on "
        "textures/metal sheet.tga\n");
    const auto m = ObjModel::readMaterials(input).at("metal");
    EXPECT_EQ(m.diffuse, (ObjModel::Vec3{.4f, .5f, .6f}));
    EXPECT_EQ(m.emission, (ObjModel::Vec3{1, 2, 3}));
    EXPECT_FLOAT_EQ(m.opacity, .75f);
    EXPECT_EQ(m.uvScale, (ObjModel::Vec2{2, 3}));
    EXPECT_EQ(m.uvOffset, (ObjModel::Vec2{.1f, .2f}));
    EXPECT_EQ(m.diffuseMap, "textures/metal sheet.tga");
    EXPECT_TRUE(m.clampMap);
}
TEST(ObjLoader, ObjectFilterAndLongFacesDoNotUseFixedBuffers) {
    std::istringstream input(
        "o a\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\no b\nf 1 2 3\n");
    auto m = ObjModel::read(input, "b");
    ASSERT_EQ(m.parts.size(), 1u);
    EXPECT_EQ(m.parts[0].object, "b");
    std::string text =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf " + std::string(2048, ' ') + "1 2 3\n";
    EXPECT_EQ(parse(text.c_str()).parts.size(), 1u);
}
} // namespace

TEST(ObjLoader, AOBakeIsNeutralOnIsolatedPlaneAndRejectsStaleGeometry) {
    auto model = parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    const auto atlas = SceneAO::bake(model, 8, .5f);
    ASSERT_EQ(atlas.coordinates.size(), 3u);
    for (auto pixel : atlas.pixels)
        EXPECT_EQ(pixel, 255);
    const auto dir = std::filesystem::temp_directory_path() / "gpp-ao-test";
    std::filesystem::create_directories(dir);
    const auto file = dir / "plane.obj";
    SceneAO::write(file, model, atlas);
    auto loaded = SceneAO::read(file, model);
    EXPECT_EQ(loaded.coordinates, atlas.coordinates);
    EXPECT_EQ(loaded.pixels, atlas.pixels);
    model.parts[0].positions[0][0] += .5f;
    EXPECT_THROW(SceneAO::read(file, model), std::runtime_error);
    std::filesystem::remove(file.string() + ".ao");
    std::filesystem::remove(file.string() + ".ao.pgm");
    std::filesystem::remove(dir);
}
TEST(ObjLoader, AOBakeDetectsNearbyOccluder) {
    auto model = parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 .1\nv 1 0 .1\nv 0 1 "
                       ".1\nf 1 2 3\nf 4 5 6\n");
    const auto atlas = SceneAO::bake(model, 8, .5f);
    EXPECT_LT(*std::min_element(atlas.pixels.begin(), atlas.pixels.end()), 128);
}
