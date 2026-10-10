#pragma once
#include "GPPOBJ.h"
#include <memory>

// Garage rendering shared by menus and gameplay. GL resources are
// context-owned.
class CBackgroundRenderer {
    struct Impl;
    std::unique_ptr<Impl> impl;
    CBackgroundRenderer();

  public:
    int quality = 2; // 0 original, 1 lighting, 2 shadows 512, 3 shadows 1024
    double shadowBias = 0.001;
    bool ambientOcclusion = true;
    static CBackgroundRenderer &inst();
    static void shutdown();
    void prepare();
    void draw(GPPOBJ &model);
    size_t shadowUpdates() const;
    ~CBackgroundRenderer();
    CBackgroundRenderer(const CBackgroundRenderer &) = delete;
    CBackgroundRenderer &operator=(const CBackgroundRenderer &) = delete;
};
