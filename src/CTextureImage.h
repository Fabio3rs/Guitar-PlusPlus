#pragma once

#include <cstddef>
#include <memory>

// CPU-owned image data, independent of the graphics API.
struct CTextureImage {
    int Width = 0, Height = 0;
    enum class PixelFormat { Unknown, Alpha, Luminance, RGB, RGBA };
    PixelFormat Format = PixelFormat::Unknown;
    int BytesPerPixel = 0;
    size_t DataBytes = 0;
    std::unique_ptr<unsigned char[]> Data;
    std::unique_ptr<unsigned char[]> tmpData;
    bool bRevPixels = false;
    bool keepData = false;
};
