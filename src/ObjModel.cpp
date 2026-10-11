#include "ObjModel.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace ObjModel {
namespace {
float length2(const Vec3 &v) { return v[0] * v[0] + v[1] * v[1] + v[2] * v[2]; }
Vec3 normalized(Vec3 v) {
    const float n = std::sqrt(length2(v));
    if (!(n > 1.e-15f)) {
        return {0.f, 1.f, 0.f};
    }
    for (auto &c : v) {
        c /= n;
    }
    return v;
}
void add(Vec3 &a, const Vec3 &b) {
    for (size_t i = 0; i < 3; ++i) {
        a[i] += b[i];
    }
}
std::string rest(std::istringstream &line) {
    std::string result;
    std::getline(line >> std::ws, result);
    const auto end = result.find_last_not_of(" \t\r");
    return end == std::string::npos ? "" : result.substr(0, end + 1);
}
bool finite(const Vec3 &v) {
    return std::all_of(v.begin(), v.end(),
                       [](float f) { return std::isfinite(f); });
}
struct Corner {
    size_t position;
    int uv = -1, normal = -1;
};
struct Face {
    std::vector<Corner> corners;
    std::vector<std::array<size_t, 3>> triangles;
    std::string object, group, material, smoothing;
    Vec3 normal{};
};
int index(const std::string &value, size_t size) {
    if (value.empty()) {
        return -1;
    }
    size_t end = 0;
    const auto raw = std::stoll(value, &end);
    if (end != value.size() || raw == 0 ||
        size > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("invalid index");
    }
    const auto resolved =
        raw > 0 ? raw - 1 : static_cast<long long>(size) + raw;
    if (resolved < 0 || resolved >= static_cast<long long>(size)) {
        throw std::runtime_error("index outside attribute array");
    }
    return static_cast<int>(resolved);
}
Corner corner(const std::string &token, size_t p, size_t t, size_t n) {
    std::array<std::string, 3> values;
    size_t begin = 0, field = 0;
    for (;;) {
        const auto slash = token.find('/', begin);
        if (field == 3) {
            throw std::runtime_error("too many face attributes");
        }
        values[field++] = token.substr(
            begin, slash == std::string::npos ? slash : slash - begin);
        if (slash == std::string::npos) {
            break;
        }
        begin = slash + 1;
    }
    const int pos = index(values[0], p);
    if (pos < 0) {
        throw std::runtime_error("missing position index");
    }
    return {static_cast<size_t>(pos), index(values[1], t), index(values[2], n)};
}
// Ear clipping preserves winding and supports concave simple planar polygons.
// Refuse invalid/degenerate polygons instead of indexing or emitting junk.
void triangulate(Face &face, const std::vector<Vec3> &positions) {
    const auto count = face.corners.size();
    if (count < 3) {
        throw std::runtime_error("face has fewer than three vertices");
    }
    // Local double-precision coordinates avoid cancellation in translated or
    // tiny faces (and avoid FMA-dependent "nonzero" normals on collinear data).
    const auto &origin = positions[face.corners[0].position];
    std::array<double, 3> sum{};
    double edge2 = 0.;
    for (size_t i = 1; i < count; ++i) {
        const auto &a = positions[face.corners[i].position];
        const auto &b = positions[face.corners[(i + 1) % count].position];
        std::array<double, 3> u{}, v{};
        double size = 0.;
        for (size_t j = 0; j < 3; ++j) {
            u[j] = double(a[j]) - double(origin[j]);
            v[j] = double(b[j]) - double(origin[j]);
            size += u[j] * u[j];
        }
        edge2 = std::max(edge2, size);
        sum[0] += u[1] * v[2] - u[2] * v[1];
        sum[1] += u[2] * v[0] - u[0] * v[2];
        sum[2] += u[0] * v[1] - u[1] * v[0];
    }
    const double area =
        std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
    if (!(area > edge2 * 1.e-10)) {
        throw std::runtime_error("degenerate face");
    }
    for (size_t j = 0; j < 3; ++j) {
        face.normal[j] = static_cast<float>(sum[j]);
    }
    if (count == 3) {
        face.triangles.push_back({0, 1, 2});
        return;
    }
    size_t axis = 0;
    for (size_t i = 1; i < 3; ++i) {
        if (std::abs(face.normal[i]) > std::abs(face.normal[axis])) {
            axis = i;
        }
    }
    const size_t x = (axis + 1) % 3, y = (axis + 2) % 3;
    const double sign = face.normal[axis] > 0.f ? 1. : -1.;
    auto signedArea = [&](size_t i, size_t j, size_t k) {
        const auto &a = positions[face.corners[i].position];
        const auto &b = positions[face.corners[j].position];
        const auto &c = positions[face.corners[k].position];
        return sign * ((double(b[x]) - a[x]) * (double(c[y]) - a[y]) -
                       (double(b[y]) - a[y]) * (double(c[x]) - a[x]));
    };
    const double epsilon = std::abs(double(face.normal[axis])) * 1.e-10;
    std::vector<size_t> polygon(count);
    std::iota(polygon.begin(), polygon.end(), size_t{0});
    while (polygon.size() > 3) {
        bool clipped = false;
        for (size_t i = 0; i < polygon.size(); ++i) {
            const auto a = polygon[(i + polygon.size() - 1) % polygon.size()];
            const auto b = polygon[i], c = polygon[(i + 1) % polygon.size()];
            if (signedArea(a, b, c) <= epsilon) {
                continue;
            }
            bool contains = false;
            for (auto q : polygon) {
                if (q == a || q == b || q == c) {
                    continue;
                }
                if (signedArea(a, b, q) >= -epsilon &&
                    signedArea(b, c, q) >= -epsilon &&
                    signedArea(c, a, q) >= -epsilon) {
                    contains = true;
                    break;
                }
            }
            if (contains) {
                continue;
            }
            face.triangles.push_back({a, b, c});
            polygon.erase(polygon.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped) {
            throw std::runtime_error("cannot triangulate polygon");
        }
    }
    if (signedArea(polygon[0], polygon[1], polygon[2]) <= epsilon) {
        throw std::runtime_error("degenerate polygon remainder");
    }
    face.triangles.push_back({polygon[0], polygon[1], polygon[2]});
}
} // namespace

Model read(std::istream &input, const std::string &objectFilter) {
    Model model;
    std::vector<Vec3> positions, normals;
    std::vector<Vec2> uvs;
    std::vector<Face> faces;
    std::string object, group, material, smoothing, text;
    size_t lineNumber = 0;
    while (std::getline(input, text)) {
        ++lineNumber;
        for (unsigned char value : text) {
            model.sourceSignature ^= value;
            model.sourceSignature *= 1099511628211ull;
        }
        model.sourceSignature ^= 10u;
        model.sourceSignature *= 1099511628211ull;
        text = text.substr(0, text.find('#'));
        std::istringstream line(text);
        std::string key;
        if (!(line >> key)) {
            continue;
        }
        try {
            if (key == "v" || key == "vn") {
                Vec3 v{};
                if (!(line >> v[0] >> v[1] >> v[2]) || !finite(v)) {
                    throw std::runtime_error("invalid vector");
                }
                if (key == "v") {
                    positions.push_back(v);
                } else {
                    normals.push_back(v);
                }
            } else if (key == "vt") {
                Vec2 uv{};
                if (!(line >> uv[0])) {
                    throw std::runtime_error("invalid UV");
                }
                line >> std::ws;
                if (!line.eof() && !(line >> uv[1])) {
                    throw std::runtime_error("invalid UV");
                }
                if (!std::isfinite(uv[0]) || !std::isfinite(uv[1])) {
                    throw std::runtime_error("invalid UV");
                }
                uvs.push_back(uv);
            } else if (key == "o") {
                object = rest(line);
            } else if (key == "g") {
                group = rest(line);
            } else if (key == "usemtl") {
                material = rest(line);
            } else if (key == "s") {
                smoothing = rest(line);
                if (smoothing == "off" || smoothing == "0") {
                    smoothing.clear();
                }
                if (smoothing == "on") {
                    smoothing = "1";
                }
            } else if (key == "mtllib") {
                std::string name;
                while (line >> name) {
                    model.libraries.push_back(name);
                }
            } else if (key == "f") {
                Face face;
                face.object = object;
                face.group = group;
                face.material = material;
                face.smoothing = smoothing;
                std::string token;
                while (line >> token) {
                    face.corners.push_back(corner(token, positions.size(),
                                                  uvs.size(), normals.size()));
                }
                try {
                    triangulate(face, positions);
                } catch (const std::runtime_error &e) {
                    model.warnings.push_back("OBJ line " +
                                             std::to_string(lineNumber) + ": " +
                                             e.what() + " (face skipped)");
                    continue;
                }
                faces.push_back(std::move(face));
            }
        } catch (const std::exception &e) {
            throw std::runtime_error("OBJ line " + std::to_string(lineNumber) +
                                     ": " + e.what());
        }
    }
    if (input.bad()) {
        throw std::runtime_error("OBJ read failure");
    }
    // Area-weighted smoothing uses source polygon normals, so the diagonal
    // chosen when triangulating cannot change the smoothing weights.
    using SmoothKey = std::tuple<std::string, std::string, size_t>;
    std::map<SmoothKey, Vec3> smooth;
    for (const auto &face : faces) {
        if (!face.smoothing.empty()) {
            for (const auto &c : face.corners) {
                add(smooth[{face.object, face.smoothing, c.position}],
                    face.normal);
            }
        }
    }
    bool bounds = false;
    for (const auto &face : faces) {
        if (!objectFilter.empty() && face.object != objectFilter) {
            continue;
        }
        for (const auto &tri : face.triangles) {
            bool textured = true;
            for (auto i : tri) {
                textured = textured && face.corners[i].uv >= 0;
            }
            if (model.parts.empty() ||
                model.parts.back().object != face.object ||
                model.parts.back().group != face.group ||
                model.parts.back().material != face.material ||
                model.parts.back().textured != textured) {
                Part part;
                part.object = face.object;
                part.group = face.group;
                part.material = face.material;
                part.textured = textured;
                model.parts.push_back(std::move(part));
            }
            auto &part = model.parts.back();
            for (auto i : tri) {
                const auto &c = face.corners[i];
                const auto &p = positions[c.position];
                part.positions.push_back(p);
                part.uvs.push_back(c.uv < 0 ? Vec2{}
                                            : uvs[static_cast<size_t>(c.uv)]);
                Vec3 n =
                    face.smoothing.empty()
                        ? face.normal
                        : smooth.at({face.object, face.smoothing, c.position});
                if (c.normal >= 0 &&
                    length2(normals[static_cast<size_t>(c.normal)]) > 1.e-24f) {
                    n = normals[static_cast<size_t>(c.normal)];
                }
                part.normals.push_back(normalized(n));
                if (!bounds) {
                    model.minimum = model.maximum = p;
                    bounds = true;
                }
                for (size_t j = 0; j < 3; ++j) {
                    model.minimum[j] = std::min(model.minimum[j], p[j]);
                    model.maximum[j] = std::max(model.maximum[j], p[j]);
                }
            }
        }
    }
    return model;
}

std::map<std::string, Material> readMaterials(std::istream &input) {
    std::map<std::string, Material> result;
    Material *current = nullptr;
    std::string text;
    size_t number = 0;
    while (std::getline(input, text)) {
        ++number;
        std::istringstream line(text.substr(0, text.find('#')));
        std::string key;
        if (!(line >> key)) {
            continue;
        }
        if (key == "newmtl") {
            current = &result[rest(line)];
            continue;
        }
        if (!current) {
            continue;
        }
        auto &m = *current;
        bool valid = true;
        if (key == "Ka" || key == "Kd" || key == "Ks" || key == "Ke") {
            Vec3 v{};
            valid = bool(line >> v[0] >> v[1] >> v[2]) && finite(v);
            if (valid) {
                if (key == "Ka") {
                    m.ambient = v;
                } else if (key == "Kd") {
                    m.diffuse = v;
                } else if (key == "Ks") {
                    m.specular = v;
                } else {
                    m.emission = v;
                }
            }
        } else if (key == "Ns" || key == "Ni" || key == "d" || key == "Tr") {
            float v = 0.f;
            valid = bool(line >> v) && std::isfinite(v);
            if (valid) {
                if (key == "Ns") {
                    m.shininess = std::clamp(v, 0.f, 1000.f);
                } else if (key == "Ni") {
                    m.opticalDensity = v;
                } else {
                    m.opacity = std::clamp(key == "Tr" ? 1.f - v : v, 0.f, 1.f);
                }
            }
        } else if (key == "illum") {
            valid = bool(line >> m.illumination);
        } else if (key == "map_Kd") {
            std::string token;
            while (line >> token) {
                if (token == "-s" || token == "-o" || token == "-t") {
                    Vec3 v = token == "-s" ? Vec3{1, 1, 1} : Vec3{};
                    int count = 0;
                    for (size_t i = 0; i < 3; ++i) {
                        auto pos = line.tellg();
                        float value = 0;
                        if (!(line >> value)) {
                            line.clear();
                            if (pos != std::streampos(-1)) {
                                line.seekg(pos);
                            }
                            break;
                        }
                        if (!std::isfinite(value)) {
                            valid = false;
                            break;
                        }
                        v[i] = value;
                        ++count;
                    }
                    if (count == 0) {
                        valid = false;
                    }
                    if (token == "-s") {
                        m.uvScale = {v[0], v[1]};
                    }
                    if (token == "-o") {
                        m.uvOffset = {v[0], v[1]};
                    }
                } else if (token == "-clamp") {
                    std::string v;
                    line >> v;
                    m.clampMap = v == "on";
                } else if (token == "-bm" || token == "-blendu" ||
                           token == "-blendv" || token == "-imfchan" ||
                           token == "-type" || token == "-texres" ||
                           token == "-boost") {
                    std::string ignored;
                    line >> ignored;
                } else if (token == "-mm") {
                    float a, b;
                    valid = bool(line >> a >> b);
                } else if (!token.empty() && token[0] == '-') {
                    valid = false;
                    break;
                } else {
                    const auto tail = rest(line);
                    m.diffuseMap = token + (tail.empty() ? "" : " " + tail);
                    break;
                }
            }
        }
        if (!valid) {
            throw std::runtime_error("MTL line " + std::to_string(number) +
                                     ": invalid " + key);
        }
    }
    if (input.bad()) {
        throw std::runtime_error("MTL read failure");
    }
    return result;
}
} // namespace ObjModel
