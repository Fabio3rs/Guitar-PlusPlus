#pragma once

// Short discharge following the existing per-player PLUS reward timestamp.
class CPlusLightning {
  public:
    static constexpr double duration = 0.85;
    // Requires the player's camera and an active compatibility GL context.
    // Bits 0..4 select green, red, yellow, blue and orange; other bits are
    // ignored.
    void render(double elapsed, unsigned int noteMask);
};
