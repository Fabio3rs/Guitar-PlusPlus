#include "SceneAO.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace SceneAO {
namespace {
using Vec = ObjModel::Vec3;
Vec sub(const Vec &a, const Vec &b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
Vec add(const Vec &a, const Vec &b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
Vec scale(Vec v, float f) {
    for (auto &c : v)
        c *= f;
    return v;
}
float dot(const Vec &a, const Vec &b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
Vec cross(const Vec &a, const Vec &b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}
Vec normal(Vec v) {
    return scale(v, 1.f / std::sqrt(std::max(dot(v, v), 1.e-20f)));
}
size_t vertices(const ObjModel::Model &m) {
    size_t n = 0;
    for (const auto &p : m.parts)
        n += p.positions.size();
    return n;
}
struct Triangle {
    Vec a, b, c, na, nb, nc, minimum, maximum, center;
    size_t index = 0;
    unsigned tile = 0, x = 0, y = 0;
    bool occluder = true;
};
struct Node {
    Vec minimum{}, maximum{};
    size_t begin = 0, end = 0;
    int left = -1, right = -1;
};
struct BVH {
    const std::vector<Triangle> &triangles;
    std::vector<size_t> indices;
    std::vector<Node> nodes;
    explicit BVH(const std::vector<Triangle> &t) : triangles(t) {
        for (size_t i = 0; i < t.size(); ++i)
            if (t[i].occluder)
                indices.push_back(i);
        nodes.reserve(indices.size() * 2);
        if (!indices.empty())
            build(0, indices.size());
    }
    int build(size_t begin, size_t end) {
        Node node;
        node.begin = begin;
        node.end = end;
        node.minimum = triangles[indices[begin]].minimum;
        node.maximum = triangles[indices[begin]].maximum;
        for (size_t i = begin + 1; i < end; ++i)
            for (size_t j = 0; j < 3; ++j) {
                node.minimum[j] =
                    std::min(node.minimum[j], triangles[indices[i]].minimum[j]);
                node.maximum[j] =
                    std::max(node.maximum[j], triangles[indices[i]].maximum[j]);
            }
        int id = static_cast<int>(nodes.size());
        nodes.push_back(node);
        if (end - begin > 8) {
            size_t axis = 0;
            for (size_t j = 1; j < 3; ++j)
                if (node.maximum[j] - node.minimum[j] >
                    node.maximum[axis] - node.minimum[axis])
                    axis = j;
            size_t middle = begin + (end - begin) / 2;
            std::nth_element(
                indices.begin() + static_cast<std::ptrdiff_t>(begin),
                indices.begin() + static_cast<std::ptrdiff_t>(middle),
                indices.begin() + static_cast<std::ptrdiff_t>(end),
                [&](size_t a, size_t b) {
                    return triangles[a].center[axis] <
                           triangles[b].center[axis];
                });
            const int left = build(begin, middle), right = build(middle, end);
            nodes[static_cast<size_t>(id)].left = left;
            nodes[static_cast<size_t>(id)].right = right;
        }
        return id;
    }
    bool hit(int id, const Vec &origin, const Vec &direction, float radius,
             size_t exclude) const {
        const auto &node = nodes[static_cast<size_t>(id)];
        float near = 0, far = radius;
        for (size_t axis = 0; axis < 3; ++axis) {
            if (std::abs(direction[axis]) < 1.e-8f) {
                if (origin[axis] < node.minimum[axis] ||
                    origin[axis] > node.maximum[axis])
                    return false;
            } else {
                float a = (node.minimum[axis] - origin[axis]) / direction[axis],
                      b = (node.maximum[axis] - origin[axis]) / direction[axis];
                if (a > b)
                    std::swap(a, b);
                near = std::max(near, a);
                far = std::min(far, b);
                if (near > far)
                    return false;
            }
        }
        if (node.left >= 0)
            return hit(node.left, origin, direction, radius, exclude) ||
                   hit(node.right, origin, direction, radius, exclude);
        for (size_t i = node.begin; i < node.end; ++i) {
            const auto &t = triangles[indices[i]];
            if (t.index == exclude)
                continue;
            const Vec e1 = sub(t.b, t.a), e2 = sub(t.c, t.a),
                      p = cross(direction, e2);
            const float determinant = dot(e1, p);
            if (std::abs(determinant) < 1.e-9f)
                continue;
            const float inverse = 1.f / determinant;
            const Vec delta = sub(origin, t.a);
            const float u = dot(delta, p) * inverse;
            if (u < 0 || u > 1)
                continue;
            const Vec q = cross(delta, e1);
            const float v = dot(direction, q) * inverse;
            if (v < 0 || u + v > 1)
                continue;
            const float distance = dot(e2, q) * inverse;
            if (distance > .0005f && distance < radius)
                return true;
        }
        return false;
    }
};
} // namespace
uint64_t signature(const ObjModel::Model &model) {
    uint64_t hash = model.sourceSignature;
    auto byte = [&](unsigned char v) {
        hash ^= v;
        hash *= 1099511628211ull;
    };
    auto word = [&](uint32_t v) {
        for (unsigned i = 0; i < 4; ++i)
            byte(static_cast<unsigned char>((v >> (8 * i)) & 255u));
    };
    auto number = [&](float f) {
        uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(f), "32-bit float");
        std::memcpy(&bits, &f, sizeof(bits));
        word(bits);
    };
    for (const auto &part : model.parts) {
        for (char c : part.material)
            byte(static_cast<unsigned char>(c));
        byte(0);
        for (const auto &p : part.positions)
            for (float v : p)
                number(v);
        // Source text already covers supplied normals/smoothing groups. Do not
        // hash computed normals: last-bit rounding can vary across CPU builds.
        for (const auto &uv : part.uvs)
            for (float v : uv)
                number(v);
        byte(255);
    }
    return hash;
}
Atlas read(const std::filesystem::path &obj, const ObjModel::Model &model) {
    std::ifstream input(obj.string() + ".ao");
    if (!input)
        return {};
    Atlas atlas;
    std::string magic;
    unsigned version = 0;
    uint64_t hash = 0;
    size_t count = 0;
    if (!(input >> magic >> version >> hash >> atlas.width >> atlas.height >>
          count) ||
        magic != "GPP_AO" || version != 1 || hash != signature(model) ||
        count != vertices(model) || atlas.width == 0 || atlas.height == 0 ||
        atlas.width > 4096 || atlas.height > 4096)
        throw std::runtime_error("invalid or stale AO sidecar");
    atlas.coordinates.resize(count);
    for (auto &uv : atlas.coordinates)
        if (!(input >> uv[0] >> uv[1]) || !std::isfinite(uv[0]) ||
            !std::isfinite(uv[1]) || uv[0] < 0 || uv[0] > 1 || uv[1] < 0 ||
            uv[1] > 1)
            throw std::runtime_error("invalid AO coordinates");
    std::ifstream image(obj.string() + ".ao.pgm", std::ios::binary);
    unsigned w = 0, h = 0, max = 0;
    if (!(image >> magic >> w >> h >> max) || magic != "P5" ||
        w != atlas.width || h != atlas.height || max != 255 ||
        image.get() != '\n')
        throw std::runtime_error("invalid AO image");
    atlas.pixels.reserve(static_cast<size_t>(w) * h);
    for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
        const int value = image.get();
        if (value == std::char_traits<char>::eof())
            throw std::runtime_error("truncated AO image");
        atlas.pixels.push_back(static_cast<unsigned char>(value));
    }
    return atlas;
}
void write(const std::filesystem::path &obj, const ObjModel::Model &model,
           const Atlas &atlas) {
    if (atlas.coordinates.size() != vertices(model) ||
        atlas.pixels.size() != static_cast<size_t>(atlas.width) * atlas.height)
        throw std::runtime_error("invalid AO atlas");
    std::ofstream image(obj.string() + ".ao.pgm", std::ios::binary);
    image << "P5\n" << atlas.width << ' ' << atlas.height << "\n255\n";
    for (auto value : atlas.pixels)
        image.put(static_cast<char>(value));
    if (!image)
        throw std::runtime_error("cannot write AO image");
    std::ofstream output(obj.string() + ".ao");
    output << "GPP_AO 1 " << signature(model) << ' ' << atlas.width << ' '
           << atlas.height << ' ' << atlas.coordinates.size() << '\n'
           << std::setprecision(9);
    for (const auto &uv : atlas.coordinates)
        output << uv[0] << ' ' << uv[1] << '\n';
    if (!output)
        throw std::runtime_error("cannot write AO coordinates");
}
Atlas bake(const ObjModel::Model &model, unsigned samples, float radius) {
    if (samples < 1 || samples > 256 || !std::isfinite(radius) || radius <= 0)
        throw std::runtime_error("invalid AO settings");
    std::vector<Triangle> triangles;
    for (const auto &part : model.parts)
        for (size_t i = 0; i < part.positions.size(); i += 3) {
            Triangle t;
            t.a = part.positions[i];
            t.b = part.positions[i + 1];
            t.c = part.positions[i + 2];
            t.na = part.normals[i];
            t.nb = part.normals[i + 1];
            t.nc = part.normals[i + 2];
            t.index = triangles.size();
            const Vec crossProduct = cross(sub(t.b, t.a), sub(t.c, t.a));
            const float area2 = std::sqrt(dot(crossProduct, crossProduct));
            t.tile = static_cast<unsigned>(std::clamp(
                std::ceil(std::sqrt(area2) * 16.f) + 4.f, 8.f, 128.f));
            t.center = scale(add(add(t.a, t.b), t.c), 1.f / 3.f);
            for (size_t j = 0; j < 3; ++j) {
                t.minimum[j] = std::min({t.a[j], t.b[j], t.c[j]});
                t.maximum[j] = std::max({t.a[j], t.b[j], t.c[j]});
            }
            t.occluder =
                part.material != "decal_oil" && part.material != "light";
            triangles.push_back(t);
        }
    Atlas atlas;
    if (triangles.empty())
        return atlas;
    atlas.width = 1024;
    std::vector<size_t> order(triangles.size());
    std::iota(order.begin(), order.end(), size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return triangles[a].tile > triangles[b].tile;
    });
    unsigned x = 0, y = 0, row = 0;
    for (auto index : order) {
        auto &t = triangles[index];
        if (x + t.tile > atlas.width) {
            x = 0;
            y += row;
            row = 0;
        }
        t.x = x;
        t.y = y;
        x += t.tile;
        row = std::max(row, t.tile);
    }
    atlas.height = 1;
    while (atlas.height < y + row)
        atlas.height *= 2;
    if (atlas.height > 4096)
        throw std::runtime_error("AO atlas too large");
    atlas.pixels.assign(static_cast<size_t>(atlas.width) * atlas.height, 255);
    atlas.coordinates.reserve(triangles.size() * 3);
    const BVH bvh(triangles);
    for (const auto &t : triangles) {
        const float span = static_cast<float>(t.tile) - 4.f;
        const float left = static_cast<float>(t.x) + 1.5f,
                    bottom = static_cast<float>(t.y) + 1.5f;
        atlas.coordinates.push_back(
            {left / static_cast<float>(atlas.width),
             bottom / static_cast<float>(atlas.height)});
        atlas.coordinates.push_back(
            {(left + span) / static_cast<float>(atlas.width),
             bottom / static_cast<float>(atlas.height)});
        atlas.coordinates.push_back(
            {left / static_cast<float>(atlas.width),
             (bottom + span) / static_cast<float>(atlas.height)});
        if (!t.occluder)
            continue;
        for (unsigned iy = 0; iy < t.tile; ++iy)
            for (unsigned ix = 0; ix < t.tile; ++ix) {
                float u = std::max(0.f, (static_cast<float>(ix) - 1.f) / span),
                      v = std::max(0.f, (static_cast<float>(iy) - 1.f) / span);
                if (u + v > 1.f) {
                    const float sum = u + v;
                    u /= sum;
                    v /= sum;
                }
                const float w = 1.f - u - v;
                const Vec n = normal(
                    add(add(scale(t.na, w), scale(t.nb, u)), scale(t.nc, v)));
                const Vec origin =
                    add(add(add(scale(t.a, w), scale(t.b, u)), scale(t.c, v)),
                        scale(n, .004f));
                const Vec tangent = normal(cross(
                    std::abs(n[1]) < .9f ? Vec{0, 1, 0} : Vec{1, 0, 0}, n));
                const Vec bitangent = cross(n, tangent);
                unsigned blocked = 0;
                for (unsigned sample = 0; sample < samples; ++sample) {
                    const float a = (static_cast<float>(sample) + .5f) /
                                    static_cast<float>(samples),
                                phi = static_cast<float>(sample) * 2.39996323f;
                    const Vec direction =
                        add(add(scale(tangent, std::sqrt(a) * std::cos(phi)),
                                scale(bitangent, std::sqrt(a) * std::sin(phi))),
                            scale(n, std::sqrt(1.f - a)));
                    if (!bvh.nodes.empty() &&
                        bvh.hit(0, origin, direction, radius, t.index))
                        ++blocked;
                }
                const float ao = 1.f - static_cast<float>(blocked) /
                                           static_cast<float>(samples);
                atlas.pixels[static_cast<size_t>(t.y + iy) * atlas.width + t.x +
                             ix] =
                    static_cast<unsigned char>(std::lround(255.f * ao));
            }
    }
    return atlas;
}
} // namespace SceneAO
