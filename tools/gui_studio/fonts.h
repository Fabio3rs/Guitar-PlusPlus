#pragma once
#include "CFonts.h"
#include <filesystem>
#include <optional>

namespace GuiStudio {
struct AtlasDefinition {
    std::string path, file, mapping;
    CFonts::AtlasGrid grid;
};
struct GlyphAdjustment {
    std::optional<double> height, offset, advance, bearing;
};
struct FontDefinition {
    std::string name;
    std::vector<AtlasDefinition> atlases;
    CFonts::FaceMetrics metrics;
    std::map<unsigned, GlyphAdjustment> adjustments;
};
using FontDefinitions = std::vector<FontDefinition>;
FontDefinitions readFonts(const std::filesystem::path &);
void writeFonts(const std::filesystem::path &, const FontDefinitions &);
FontDefinitions defaultFonts();
std::string character(unsigned);
} // namespace GuiStudio
