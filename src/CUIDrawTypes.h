#pragma once
#include <cstddef>
#include <vector>

namespace ui {
// Texture values are borrowed resource identifiers. Only the backend interprets
// them (currently OpenGL names); controls never create or destroy textures.
struct Rect {
    double left = 0, bottom = 0, right = 0, top = 0;
    bool contains(double px, double py) const {
        return px >= left && px <= right && py >= bottom && py <= top;
    }

    bool intersects(const Rect &r) const {
        return right >= r.left && left <= r.right && top >= r.bottom &&
               bottom <= r.top;
    }
};

struct Glyph {
    unsigned texture = 0;
    size_t atlas = 0;
    double u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    double width = 1, bearing = 0, advance = 2.0 / 3.0;
    double left = 0, right = 0, bottom = 0, top = 1;
    double height = 1, offset = 0;
    bool visible = false;
};

struct Placement {
    Glyph glyph;
    double x = 0;
};

struct TextLayout {
    std::vector<Placement> glyphs;
    std::vector<double> carets;
    double baseline = 0;
    double advance = 0, left = 0, right = 0, bottom = 0, top = 1;
    double width() const { return right - left; }

    size_t cursorAt(double x) const;
};

} // namespace ui
