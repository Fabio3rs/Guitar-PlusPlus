#pragma once

// Opt-in, per-thread instrumentation. No allocation or output in game loops.
#ifdef GPP_PROFILE_GAMEPLAY
#include <algorithm>
#include <array>
#include <chrono>
#include <numeric>
#include <ostream>
#include <vector>

namespace gpp::profile {
using Clock = std::chrono::steady_clock;
enum Metric {
    Update,
    UpdatePlayer,
    Render,
    RenderPlayer,
    Interval,
    Mutex,
    Count
};
struct Samples {
    std::array<double, 65536> values{};
    size_t size = 0;
    size_t dropped = 0;
    void add(double us) {
        if (size < values.size()) {
            values[size++] = us;
        } else {
            ++dropped;
        }
    }
};
inline thread_local std::array<Samples, Count> samples;
inline thread_local Clock::time_point previousUpdate{};
inline double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::micro>(Clock::now() - start)
        .count();
}
struct Scope {
    Metric metric;
    Clock::time_point start = Clock::now();
    explicit Scope(Metric value) : metric(value) {}
    ~Scope() { samples[metric].add(elapsed(start)); }
};
inline void interval() {
    const auto now = Clock::now();
    if (previousUpdate != Clock::time_point{}) {
        samples[Interval].add(
            std::chrono::duration<double, std::micro>(now - previousUpdate)
                .count());
    }
    previousUpdate = now;
}
inline void reset() {
    for (auto &s : samples) {
        s.size = s.dropped = 0;
    }
    previousUpdate = {};
}
// Call only on the producing thread, after measurement has stopped.
inline void report(std::ostream &out) {
    const char *names[] = {"update",       "updatePlayer", "render",
                           "renderPlayer", "interval",     "mutex"};
    for (size_t i = 0; i < Count; ++i) {
        const auto &s = samples[i];
        if (!s.size) {
            continue;
        }
        std::vector<double> sorted(s.values.begin(), s.values.begin() + s.size);
        std::sort(sorted.begin(), sorted.end());
        out << "metric," << names[i] << ',' << s.size << ','
            << std::accumulate(sorted.begin(), sorted.end(), 0.0) / s.size
            << ',' << sorted[s.size / 2] << ',' << sorted[s.size * 95 / 100]
            << ',' << sorted[s.size * 99 / 100] << ',' << sorted.back() << ','
            << s.dropped << '\n';
    }
}
} // namespace gpp::profile
#define GPP_PROFILE_SCOPE(metric)                                              \
    gpp::profile::Scope profileScope(gpp::profile::metric)
#else
#define GPP_PROFILE_SCOPE(metric) ((void)0)
#endif
