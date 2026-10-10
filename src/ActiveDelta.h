#pragma once
#include <cmath>
#include <algorithm>

// Owned by the thread that runs gameplay updates. Reset at loading/pause/song
// boundaries; the first sample resumes the clock without billing idle time.
class ActiveDelta {
    double previous = 0.0;
    bool running = false;
  public:
    void reset() { running = false; }
    double sample(double now) {
        if (!std::isfinite(now)) { reset(); return 0.0; }
        const double delta = running && now >= previous ? now - previous : 0.0;
        previous = now;
        running = true;
        return delta;
    }
};

// Preserve the current early-release tolerance; intersect this update with the
// judged sustain span so the final tick cannot award time past its boundary.
inline double sustainActiveSeconds(double start, double length, double now, double delta) {
    if (!std::isfinite(delta) || delta <= 0.0) { return 0.0; }
    return std::max(0.0, std::min(now, start + length - 0.13) -
                         std::max(now - delta, start));
}
