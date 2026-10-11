#pragma once

#include <cstddef>
#include <memory>

// CPU-owned image data. Format contains the existing numeric pixel format;
// this type itself requires neither an OpenGL header nor a current context.
struct CTextureImage {
    int Width = 0, Height = 0;
    int Format = 0;
    int BytesPerPixel = 0;
    size_t DataBytes = 0;
    std::unique_ptr<unsigned char[]> Data;
    std::unique_ptr<unsigned char[]> tmpData;
    bool bRevPixels = false;
    bool keepData = false;
};
