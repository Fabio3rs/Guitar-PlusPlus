#pragma once
#include "CEngine.h"
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string_view>
#include <vector>

class CFonts {
  public:
    struct AtlasGrid {
        int columns = 0, rows = 0;
    };

    // Owned by the caller for the duration of analysis; rows are bottom-up.
    struct ImageView {
        const unsigned char *data = nullptr;
        size_t bytes = 0;
        int width = 0, height = 0, components = 4;
        int alpha = 3; // -1 means opaque
    };

    struct InkBounds {
        int left = 0, right = 0, bottom = 0, top = 0;
        bool visible = false;
    };

    static bool analyzeCell(const ImageView &, AtlasGrid, int column, int row,
                            InkBounds &);

    struct FaceMetrics {
        double baseline = 0, ascent = 1, descent = 0;
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

    class Font {
        struct Texture {
            size_t id = 0;
            unsigned texture = 0;
            AtlasGrid grid;
            std::string registration;
        };

        std::map<std::string, Texture> textures;
        std::map<unsigned, Glyph> chars;
        std::array<const Glyph *, 128> ascii{};
        size_t nextAtlas = 0, changes = 0;
        FaceMetrics metrics;
        friend CFonts;
        bool registerImage(const std::string &, unsigned, ImageView,
                           const std::string &, AtlasGrid,
                           const std::function<bool(ImageView &)> & = {});

      public:
        Font() = default;
        Font(const Font &) = delete;
        Font &operator=(const Font &) = delete;
        Font(Font &&) = default;
        Font &operator=(Font &&) = default;
        // Returns false without publishing a partial registration.
        bool registerTexture(const std::string &, const std::string &,
                             const std::string &, AtlasGrid);
        void registerTexture(const std::string &, const std::string &,
                             const std::string &);
        void registerTexture(const std::string &, const std::string &,
                             const std::wstring &);
    };

    // Independent registries do not load assets or initialize the game.
    explicit CFonts(bool loadDefault = false);
    struct Inspection {
        std::map<unsigned, Glyph> glyphs;
        FaceMetrics metrics;
    };
    Inspection inspect(const std::string &name) const;
    // Texture remains owned by the caller; pixels are consumed synchronously.
    bool addImageToFont(const std::string &name, const std::string &key,
                        unsigned texture, const ImageView &image,
                        const std::string &mapping, AtlasGrid grid);

    struct textAlert {
        int status = 0;
        double startTime = 0;
        std::string msg;
        std::function<bool(textAlert &)> callback;
    };

    // Invalid UTF-8: size/insert return npos; removal leaves the input intact.
    // All positions/counts are Unicode scalar values, not bytes or graphemes.
    static size_t utf8Size(const std::string &);
    static size_t utf8InsertAt(std::string &, const std::string &, size_t);
    static void utf8RemoveLast(std::string &);
    static void utf8RemoveAtRange(std::string &, int, int);

    bool setFontCharMetrics(const std::string &, const std::string &,
                            double height, double offset);
    bool setFontMetrics(const std::string &, double baseline, double ascent,
                        double descent);
    bool setFontCharSpacing(const std::string &, const std::string &,
                            double advance, double bearing);
    std::string addTextureToFont(const std::string &, const std::string &,
                                 const std::string &, const std::wstring &);
    std::string addTextureToFont(const std::string &, const std::string &,
                                 const std::string &, const std::string &);
    std::string addTextureToFont(const std::string &, const std::string &,
                                 const std::string &, const std::string &,
                                 AtlasGrid);
    size_t revision() const { return metricsRevision; }

    // Unit-size layout. Reuse the destination's vector capacity.
    void layoutText(std::string_view, TextLayout &,
                    const std::string &font = "default") const;
    double getCenterPos(const std::string &, double, double,
                        const std::string & = "default");
    double getXSizeInScreen(const std::string &, double,
                            const std::string & = "default");
    void drawTextInScreenWithBuffer(const std::string &, double, double, double,
                                    const std::string & = "default");
    void drawTextInScreen(const std::string &, double, double, double,
                          const std::string & = "default");
    void draw3DTextInScreen(const std::string &, double, double, double, double,
                            double, double, const std::string & = "default");
    void drawLayout(const TextLayout &, double x, double y, double size);
    void drawAllBuffers();
    void addTextAlert(const textAlert &);
    void addTextAlert(textAlert &&);
    static CFonts &fonts();

  private:
    struct Batch {
        CEngine::dTriangleWithAlpha data;
        double color[4]{};
    };

    std::vector<Batch> batches;
    CEngine::dTriangleWithAlpha layoutBuffer;
    size_t usedBatches = 0;
    double bufferedColor[4]{};
    std::map<std::string, Font> fontsReg;
    std::deque<textAlert> textAlerts;
    size_t metricsRevision = 0;
    TextLayout walk(std::string_view, const std::string &, TextLayout *,
                    void (*emit)(void *, const Glyph &, double),
                    void *context = nullptr) const;
    void drawText(std::string_view, const std::string &, double, double, double,
                  double, double, double, bool, bool);
    void drawGlyph(const Glyph &, double, double, double, double, double,
                   double, bool, bool);
    CFonts(const CFonts &) = delete;
};
