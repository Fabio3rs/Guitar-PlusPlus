#include "CFonts.h"
#include "CLog.h"
#include "GPPGame.h"
#include "utf8.h"
#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
std::string narrow(const std::wstring &s) {
    std::string out;

    if (sizeof(wchar_t) == 2) {
        utf8::utf16to8(s.begin(), s.end(), std::back_inserter(out));
    } else {
        utf8::utf32to8(s.begin(), s.end(), std::back_inserter(out));
    }

    return out;
}

CEngine::RenderDoubleStruct glyphQuad(const CFonts::Glyph &g, double x,
                                      double y, double z, double sx, double sy,
                                      double sz) {
    CEngine::RenderDoubleStruct q{};
    q.Text = g.texture;
    q.x1 = q.x4 = x;
    q.x2 = q.x3 = x + g.width * sx;
    q.y1 = q.y2 = y + (g.offset + g.height) * sy;
    q.y3 = q.y4 = y + g.offset * sy;
    q.z1 = q.z2 = z + (g.offset + g.height) * sz;
    q.z3 = q.z4 = z + g.offset * sz;
    q.TextureX1 = g.u0;
    q.TextureX2 = g.u1;
    q.TextureY1 = g.v1;
    q.TextureY2 = g.v0;
    return q;
}

size_t byteAt(const std::string &s, size_t position) {
    auto it = s.begin();

    while (position-- && it != s.end()) {
        utf8::next(it, s.end());
    }

    return static_cast<size_t>(it - s.begin());
}
} // namespace

CFonts &CFonts::fonts() {
    static CFonts instance(true);
    return instance;
}

bool CFonts::analyzeCell(const ImageView &im, AtlasGrid grid, int col, int row,
                         InkBounds &out) {
    out = {};

    if (!im.data || im.width <= 0 || im.height <= 0 || im.components <= 0 ||
        im.components > 4 || im.alpha < -1 || im.alpha >= im.components ||
        grid.columns <= 0 || grid.rows <= 0 || col < 0 || row < 0 ||
        col >= grid.columns || row >= grid.rows || im.width % grid.columns ||
        im.height % grid.rows) {
        return false;
    }

    if (static_cast<size_t>(im.width) >
        std::numeric_limits<size_t>::max() /
            static_cast<size_t>(im.components)) {
        return false;
    }

    const size_t stride =
        static_cast<size_t>(im.width) * static_cast<size_t>(im.components);

    if (stride > im.bytes / static_cast<size_t>(im.height)) {
        return false;
    }

    const int w = im.width / grid.columns, h = im.height / grid.rows;

    if (!w || !h) {
        return false;
    }

    const int x0 = col * w, y0 = (grid.rows - row - 1) * h;
    out.left = w;
    out.bottom = h;

    for (int y = 0; y < h; ++y) {
        const auto *line = im.data + static_cast<size_t>(y0 + y) * stride;

        for (int x = 0; x < w; ++x) {
            if (im.alpha < 0 || line[static_cast<size_t>(x0 + x) *
                                         static_cast<size_t>(im.components) +
                                     static_cast<size_t>(im.alpha)]) {
                out.visible = true;
                out.left = std::min(out.left, x);
                out.right = std::max(out.right, x + 1);
                out.bottom = std::min(out.bottom, y);
                out.top = std::max(out.top, y + 1);
            }
        }
    }

    if (!out.visible) {
        out = {};
    }

    return true;
}

bool CFonts::Font::registerTexture(const std::string &path,
                                   const std::string &texture,
                                   const std::string &text, AtlasGrid grid) {
    auto &game = GPPGame::GuitarPP();

    if (std::this_thread::get_id() != game.mainthread) {
        return false;
    }

    const auto &loaded = game.loadTexture(path, texture, nullptr, true);
    const unsigned id = loaded.getTextId();

    if (!id || id == ~0u) {
        return false;
    }

    CEngine::GLFWimage decoded;
    const auto retained = loaded.getImageData();
    auto pixels = [&](ImageView &view) {
        const CEngine::GLFWimage *image = retained.get();
        if (!image || !image->Data) {
            if (!CEngine::engine().readImage((path + "/" + texture).c_str(),
                                             &decoded, 0)) {
                return false;
            }
            image = &decoded;
        }
        view = {image->Data.get(),
                image->DataBytes,
                image->Width,
                image->Height,
                image->BytesPerPixel,
                image->BytesPerPixel == 4
                    ? 3
                    : (image->Format == GL_ALPHA ? 0 : -1)};
        return true;
    };
    return registerImage(path + "/" + texture, id, {}, text, grid, pixels);
}

bool CFonts::Font::registerImage(
    const std::string &key, unsigned id, ImageView view,
    const std::string &text, AtlasGrid grid,
    const std::function<bool(ImageView &)> &readPixels) {
    struct Cell {
        unsigned code;
        int col, row;
    };

    std::vector<Cell> cells;
    int col = 0, row = 0, columns = 0;

    try {
        for (auto it = text.begin(); it != text.end();) {
            auto cp = utf8::next(it, text.end());

            if (col == std::numeric_limits<int>::max() ||
                row == std::numeric_limits<int>::max() - 1) {
                return false;
            }

            if (cp == '\n') {
                columns = std::max(columns, col);
                col = 0;
                ++row;
            } else {
                cells.push_back({cp, col++, row});
            }
        }
    } catch (const std::exception &) {
        return false;
    }

    columns = std::max(columns, col);

    if (cells.empty()) {
        return false;
    }

    if (grid.columns == 0 && grid.rows == 0) {
        grid = {columns, row + 1};
    }

    if (grid.columns <= 0 || grid.rows <= 0) {
        return false;
    }

    for (const auto &cell : cells) {
        if (cell.col >= grid.columns || cell.row >= grid.rows) {
            return false;
        }
    }

    if (!id || id == ~0u) {
        return false;
    }

    auto cached = textures.find(key);

    if (cached != textures.end() && cached->second.texture == id &&
        cached->second.registration == text &&
        cached->second.grid.columns == grid.columns &&
        cached->second.grid.rows == grid.rows) {
        bool intact = true;

        for (const auto &cell : cells) {
            auto glyph = chars.find(cell.code);

            if (glyph == chars.end() ||
                glyph->second.atlas != cached->second.id) {
                intact = false;
            }
        }

        if (intact) {
            bool changed = false;

            for (auto &entry : chars) {
                if (entry.second.atlas == cached->second.id) {
                    auto &g = entry.second;
                    const double bearing = g.visible ? .05 - g.left : 0;
                    const double advance =
                        g.visible ? std::max(.3, g.right - g.left) + .1
                                  : 2.0 / 3.0;
                    changed = changed || g.height != 1 || g.offset != 0 ||
                              g.bearing != bearing || g.advance != advance;
                    g.height = 1;
                    g.offset = 0;
                    g.bearing = bearing;
                    g.advance = advance;
                }
            }

            if (changed) {
                ++changes;
            }

            return true;
        }
    }

    if (readPixels && !readPixels(view)) {
        return false;
    }
    std::map<unsigned, Glyph> prepared;
    const size_t atlasID = nextAtlas + 1;

    for (const auto &cell : cells) {
        InkBounds bounds;

        if (!analyzeCell(view, grid, cell.col, cell.row, bounds)) {
            return false;
        }

        const double w = view.width / grid.columns, h = view.height / grid.rows;
        Glyph glyph;
        glyph.texture = id;
        glyph.atlas = atlasID;
        glyph.u0 = static_cast<double>(cell.col) / grid.columns;
        glyph.u1 = static_cast<double>(cell.col + 1) / grid.columns;
        glyph.v1 = 1.0 - static_cast<double>(cell.row) / grid.rows;
        glyph.v0 = 1.0 - static_cast<double>(cell.row + 1) / grid.rows;
        glyph.width = w / h;
        glyph.visible = bounds.visible;
        glyph.left = bounds.left / h;
        glyph.right = bounds.right / h;
        glyph.bottom = bounds.bottom / h;
        glyph.top = bounds.top / h;
        // Explicit side bearings and advance; no heuristics during drawing.
        glyph.bearing = bounds.visible ? 0.05 - glyph.left : 0.0;
        glyph.advance = bounds.visible
                            ? std::max(0.3, glyph.right - glyph.left) + 0.1
                            : 2.0 / 3.0;
        prepared[cell.code] = glyph;
    }

    // Prepare complete value maps: a failed allocation leaves the old font
    // valid.
    auto newChars = chars;
    auto newTextures = textures;
    auto old = textures.find(key);

    if (old != textures.end()) {
        for (auto it = newChars.begin(); it != newChars.end();) {
            if (it->second.atlas == old->second.id) {
                it = newChars.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (const auto &entry : prepared) {
        newChars[entry.first] = entry.second;
    }

    newTextures[key] = {atlasID, id, grid, text};
    chars.swap(newChars);
    textures.swap(newTextures);
    nextAtlas = atlasID;
    ++changes;
    ascii.fill(nullptr);

    for (const auto &entry : chars) {
        if (entry.first < ascii.size()) {
            ascii[entry.first] = &entry.second;
        }
    }

    return true;
}

void CFonts::Font::registerTexture(const std::string &p, const std::string &t,
                                   const std::string &s) {
    registerTexture(p, t, s, {});
}

void CFonts::Font::registerTexture(const std::string &p, const std::string &t,
                                   const std::wstring &s) {
    try {
        registerTexture(p, t, narrow(s), {});
    } catch (const std::exception &) {
    }
}

size_t CFonts::utf8Size(const std::string &s) {
    size_t count = 0;

    try {
        for (auto it = s.begin(); it != s.end(); ++count) {
            utf8::next(it, s.end());
        }
    } catch (const std::exception &) {
        return std::string::npos;
    }

    return count;
}

size_t CFonts::utf8InsertAt(std::string &s, const std::string &insert,
                            size_t at) {
    const size_t count = utf8Size(s), added = utf8Size(insert);

    if (count == std::string::npos || added == std::string::npos) {
        return std::string::npos;
    }

    at = std::min(at, count);
    const size_t offset = byteAt(s, at);
    s.insert(offset, insert); // std::string handles self-insertion.
    return at + added;
}

void CFonts::utf8RemoveLast(std::string &s) {
    const size_t count = utf8Size(s);

    if (count && count != std::string::npos) {
        s.erase(byteAt(s, count - 1));
    }
}

void CFonts::utf8RemoveAtRange(std::string &s, int at, int count) {
    if (at < 0 || count <= 0) {
        return;
    }

    const size_t length = utf8Size(s);

    if (length == std::string::npos || static_cast<size_t>(at) >= length) {
        return;
    }

    const size_t begin = byteAt(s, static_cast<size_t>(at));
    const size_t end =
        byteAt(s, static_cast<size_t>(at) +
                      std::min(static_cast<size_t>(count),
                               length - static_cast<size_t>(at)));
    s.erase(begin, end - begin);
}

size_t CFonts::TextLayout::cursorAt(double x) const {
    if (carets.empty()) {
        return 0;
    }

    auto it = std::lower_bound(carets.begin(), carets.end(), x);

    if (it == carets.begin()) {
        return 0;
    }

    if (it == carets.end()) {
        return carets.size() - 1;
    }

    const size_t i = static_cast<size_t>(it - carets.begin());
    return x - carets[i - 1] <= carets[i] - x ? i - 1 : i;
}

CFonts::TextLayout CFonts::walk(std::string_view text, const std::string &name,
                                TextLayout *output,
                                void (*emit)(void *, const Glyph &, double),
                                void *context) const {
    TextLayout result;
    auto f = fontsReg.find(name);

    if (f == fontsReg.end()) {
        f = fontsReg.find("default");
    }

    if (f != fontsReg.end()) {
        result.baseline = f->second.metrics.baseline;
        result.top = result.baseline + f->second.metrics.ascent;
        result.bottom = result.baseline - f->second.metrics.descent;
    }

    if (output) {
        output->glyphs.clear();
        output->carets.clear();
        output->carets.push_back(0);
    }

    double pen = 0;

    for (auto it = text.begin(); it != text.end();) {
        auto start = it;
        unsigned cp;

        try {
            cp = utf8::next(it, text.end());
        } catch (const std::exception &) {
            it = start + 1;
            cp = 0xfffd;
        }

        const Glyph *g = nullptr;

        if (f != fontsReg.end()) {
            if (cp < f->second.ascii.size()) {
                g = f->second.ascii[cp];
            } else {
                auto found = f->second.chars.find(cp);

                if (found != f->second.chars.end()) {
                    g = &found->second;
                }
            }

            if (!g && cp != ' ' && cp != '\t') {
                g = f->second.ascii['?'];
            }
        }

        if (cp == '\n' || cp == '\r') { /* single-line compatibility */
        } else if ((cp == ' ' || cp == '\t') && !g) {
            pen += cp == '\t' ? 8.0 / 3.0 : 2.0 / 3.0;
        } else if (g) {
            const double x = pen + g->bearing;

            if (g->visible) {
                result.left = std::min(result.left, x + g->left);
                result.right = std::max(result.right, x + g->right);
                result.bottom =
                    std::min(result.bottom, g->offset + g->bottom * g->height);
                result.top =
                    std::max(result.top, g->offset + g->top * g->height);

                if (output) {
                    output->glyphs.push_back({*g, x});
                }

                if (emit) {
                    emit(context, *g, x);
                }
            }

            pen += g->advance;
        } else {
            pen += 2.0 / 3.0;
        }

        if (output) {
            output->carets.push_back(pen);
        }
    }

    result.advance = pen;
    result.right = std::max(result.right, pen);

    if (output) {
        output->baseline = result.baseline;
        output->advance = pen;
        output->left = result.left;
        output->right = result.right;
        output->bottom = result.bottom;
        output->top = result.top;
    }

    return result;
}

void CFonts::layoutText(std::string_view text, TextLayout &out,
                        const std::string &font) const {
    walk(text, font, &out, {});
}

double CFonts::getXSizeInScreen(const std::string &s, double size,
                                const std::string &f) {
    return walk(s, f, nullptr, {}).width() * size;
}

double CFonts::getCenterPos(const std::string &s, double size, double x,
                            const std::string &f) {
    auto bounds = walk(s, f, nullptr, {});
    return x - (bounds.left + bounds.right) * size / 2;
}

void CFonts::drawGlyph(const Glyph &g, double x, double y, double z, double sx,
                       double sy, double sz, bool buffered, bool threeD) {
    const auto q = glyphQuad(g, x, y, z, sx, sy, sz);

    if (buffered) {
        const auto *color = bufferedColor;

        if (!usedBatches ||
            batches[usedBatches - 1].data.texture != g.texture ||
            !std::equal(color, color + 4, batches[usedBatches - 1].color)) {
            if (usedBatches == batches.size()) {
                batches.emplace_back();
            }

            auto &batch = batches[usedBatches++];
            batch.data.clear();
            batch.data.texture = g.texture;
            batch.data.useColors = false;
            std::copy(color, color + 4, batch.color);
        }

        CEngine::pushQuad(batches[usedBatches - 1].data, q);
    } else if (threeD) {
        CEngine::engine().Render3DQuad(q);
    } else {
        CEngine::engine().Render2DQuad(q);
    }
}

void CFonts::drawText(std::string_view text, const std::string &font, double x,
                      double y, double z, double sx, double sy, double sz,
                      bool buffered, bool threeD) {
    struct Context {
        CFonts *fonts;
        double x, y, z, sx, sy, sz;
        bool buffered, threeD;
    } context{this, x, y, z, sx, sy, sz, buffered, threeD};

    if (buffered) {
        glGetDoublev(GL_CURRENT_COLOR, bufferedColor);
    }

    walk(
        text, font, nullptr,
        [](void *data, const Glyph &g, double pos) {
            const auto &c = *static_cast<Context *>(data);
            c.fonts->drawGlyph(g, c.x + pos * c.sx, c.y, c.z, c.sx, c.sy, c.sz,
                               c.buffered, c.threeD);
        },
        &context);
}

void CFonts::drawTextInScreen(const std::string &s, double x, double y,
                              double size, const std::string &font) {
    drawText(s, font, x, y, 0, size, size, 0, false, false);
}

void CFonts::drawTextInScreenWithBuffer(const std::string &s, double x,
                                        double y, double size,
                                        const std::string &font) {
    drawText(s, font, x, y, 0, size, size, 0, true, false);
}

void CFonts::draw3DTextInScreen(const std::string &s, double x, double y,
                                double z, double sx, double sy, double sz,
                                const std::string &font) {
    drawText(s, font, x, y, z, sx, sy, sz, false, true);
}

void CFonts::drawLayout(const TextLayout &layout, double x, double y,
                        double size) {
    layoutBuffer.clear();

    for (const auto &p : layout.glyphs) {
        if (!layoutBuffer.vArray.empty() &&
            layoutBuffer.texture != p.glyph.texture) {
            CEngine::engine().drawTrianglesWithAlpha(layoutBuffer);
            layoutBuffer.clear();
        }

        layoutBuffer.texture = p.glyph.texture;
        CEngine::pushQuad(layoutBuffer, glyphQuad(p.glyph, x + p.x * size, y, 0,
                                                  size, size, 0));
    }

    if (!layoutBuffer.vArray.empty()) {
        CEngine::engine().drawTrianglesWithAlpha(layoutBuffer);
    }

    layoutBuffer.clear();
}

void CFonts::drawAllBuffers() {
    if (usedBatches) {
        GLdouble color[4];
        glGetDoublev(GL_CURRENT_COLOR, color);

        for (size_t i = 0; i < usedBatches; ++i) {
            glColor4dv(batches[i].color);
            CEngine::engine().drawTrianglesWithAlpha(batches[i].data);
            batches[i].data.clear();
        }

        glColor4dv(color);
        usedBatches = 0;
    }

    if (!textAlerts.empty()) {
        auto &alert = textAlerts.front();

        if (!alert.callback || !alert.callback(alert)) {
            textAlerts.pop_front();

            if (!textAlerts.empty()) {
                textAlerts.front().startTime = CEngine::engine().getTime();
            }
        }
    }
}

void CFonts::addTextAlert(const textAlert &t) { textAlerts.push_back(t); }

void CFonts::addTextAlert(textAlert &&t) { textAlerts.push_back(std::move(t)); }

bool CFonts::setFontCharMetrics(const std::string &name,
                                const std::string &text, double height,
                                double offset) {
    auto f = fontsReg.find(name);

    if (f == fontsReg.end() || text.empty() || !std::isfinite(height) ||
        height <= 0 || !std::isfinite(offset)) {
        return false;
    }

    std::vector<Glyph *> glyphs;

    try {
        for (auto it = text.begin(); it != text.end();) {
            auto g = f->second.chars.find(utf8::next(it, text.end()));

            if (g == f->second.chars.end()) {
                return false;
            }

            glyphs.push_back(&g->second);
        }
    } catch (const std::exception &) {
        return false;
    }

    for (auto *g : glyphs) {
        g->height = height;
        g->offset = offset;
    }

    ++metricsRevision;
    return true;
}

bool CFonts::setFontCharSpacing(const std::string &name,
                                const std::string &text, double advance,
                                double bearing) {
    auto f = fontsReg.find(name);

    if (f == fontsReg.end() || text.empty() || !std::isfinite(advance) ||
        advance < 0 || !std::isfinite(bearing)) {
        return false;
    }

    std::vector<Glyph *> glyphs;

    try {
        for (auto it = text.begin(); it != text.end();) {
            auto g = f->second.chars.find(utf8::next(it, text.end()));

            if (g == f->second.chars.end()) {
                return false;
            }

            glyphs.push_back(&g->second);
        }
    } catch (const std::exception &) {
        return false;
    }

    for (auto *g : glyphs) {
        g->advance = advance;
        g->bearing = bearing;
    }

    ++metricsRevision;
    return true;
}

std::string CFonts::addTextureToFont(const std::string &f, const std::string &p,
                                     const std::string &t, const std::string &s,
                                     AtlasGrid grid) {
    auto existing = fontsReg.find(f);

    if (existing != fontsReg.end()) {
        const size_t before = existing->second.changes;

        if (!existing->second.registerTexture(p, t, s, grid)) {
            return {};
        }

        if (before == existing->second.changes) {
            return p + "/" + t;
        }
    } else {
        Font prepared;

        if (!prepared.registerTexture(p, t, s, grid)) {
            return {};
        }

        fontsReg.emplace(f, std::move(prepared));
    }

    ++metricsRevision;
    return p + "/" + t;
}

std::string CFonts::addTextureToFont(const std::string &f, const std::string &p,
                                     const std::string &t,
                                     const std::string &s) {
    return addTextureToFont(f, p, t, s, {});
}

std::string CFonts::addTextureToFont(const std::string &f, const std::string &p,
                                     const std::string &t,
                                     const std::wstring &s) {
    try {
        return addTextureToFont(f, p, t, narrow(s), {});
    } catch (const std::exception &) {
        return {};
    }
}

CFonts::CFonts(bool loadDefault) {
    if (!loadDefault) {
        return;
    }

    addTextureToFont(
        "default", "data/sprites", "FONT.tga",
        "0123456789ABCDEFGHIJKLMNOPQRSTUV\nWXYZabcdefghijklmnopqrstuvwxyz.,\n"
        "ÃÂÁÀÄÊÉÈËÎÍÌÏÕÔÓÒÖŨÛÚÙÜãâáàäêéèë\n"
        "îíìïõôóòöũûúùüÑñÇç:\\|/<>()-+=*[]\n?!{}@#$%&ªº£¬_",
        {32, 8});
    setFontMetrics("default", 14.0 / 64.0, 50.0 / 64.0, 14.0 / 64.0);
}

bool CFonts::setFontMetrics(const std::string &name, double baseline,
                            double ascent, double descent) {
    auto f = fontsReg.find(name);

    if (f == fontsReg.end() || !std::isfinite(baseline) ||
        !std::isfinite(ascent) || !std::isfinite(descent) || ascent < 0 ||
        descent < 0 || ascent + descent <= 0 ||
        !std::isfinite(ascent + descent)) {
        return false;
    }

    f->second.metrics = {baseline, ascent, descent};
    ++metricsRevision;
    return true;
}

CFonts::Inspection CFonts::inspect(const std::string &name) const {
    const auto found = fontsReg.find(name);
    if (found == fontsReg.end()) {
        return {};
    }
    return {found->second.chars, found->second.metrics};
}

bool CFonts::addImageToFont(const std::string &name, const std::string &key,
                            unsigned texture, const ImageView &image,
                            const std::string &mapping, AtlasGrid grid) {
    auto found = fontsReg.find(name);
    if (found == fontsReg.end()) {
        Font font;
        if (!font.registerImage(key, texture, image, mapping, grid)) {
            return false;
        }
        fontsReg.emplace(name, std::move(font));
    } else {
        const auto before = found->second.changes;
        if (!found->second.registerImage(key, texture, image, mapping, grid)) {
            return false;
        }
        if (before == found->second.changes) {
            return true;
        }
    }
    ++metricsRevision;
    return true;
}
