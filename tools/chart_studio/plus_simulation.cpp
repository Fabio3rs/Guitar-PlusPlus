#include "plus_simulation.h"
#include <algorithm>
#include <iomanip>
#include <locale>
#include <stdexcept>

namespace ChartStudio {
namespace {
uint64_t randomWord(uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
std::pair<size_t, size_t> simulationRange(const PlusAnalysis &analysis,
                                          const PlusPhrase &phrase) {
    if (phrase.runtimeFirst >= 0 && phrase.runtimeLast >= phrase.runtimeFirst &&
        uint64_t(phrase.runtimeLast) < analysis.attacks.size()) {
        return {size_t(phrase.runtimeFirst), size_t(phrase.runtimeLast) + 1};
    }
    return plusAttackRange(analysis.attacks, phrase);
}

std::string quoted(const std::string &text) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') {
            out << '\\' << char(c);
        } else if (c < 32) {
            out << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                << int(c) << std::dec;
        } else {
            out << char(c);
        }
    }
    out << '"';
    return out.str();
}
} // namespace

void PlusSimulationSettings::validate() const {
    if (trials < 1 || trials > 4096 || !std::isfinite(success) || success < 0 ||
        success > 1 || !std::isfinite(reserve) || reserve <= 0 || reserve > 1 ||
        !std::isfinite(initialEnergy) || initialEnergy < 0 ||
        initialEnergy > 1 ||
        (strategy != "dense" && strategy != "immediate" &&
         strategy != "reserve" && strategy != "score" && strategy != "never")) {
        throw std::runtime_error("Invalid Plus simulation settings");
    }
}
PlusSimulation simulatePlus(const PlusAnalysis &a,
                            const std::vector<PlusPhrase> &phrases,
                            const PlusSimulationSettings &settings,
                            const std::function<void()> &checkpoint) {
    settings.validate();
    PlusSimulation total;
    total.regionEnergy.resize(a.regions.size());
    if (a.attacks.empty()) {
        return total;
    }
    std::vector<std::vector<size_t>> completedAt(a.attacks.size());
    std::vector<std::pair<size_t, size_t>> ranges;
    for (size_t p = 0; p < phrases.size(); ++p) {
        const auto range = simulationRange(a, phrases[p]);
        ranges.push_back(range);
        if (range.first < range.second) {
            completedAt[range.second - 1].push_back(p);
        }
    }
    std::vector<double> times;
    double finish = a.attacks.back().time;
    std::string identity;
    for (const auto &note : a.attacks) {
        times.push_back(note.time);
        finish = std::max(finish, note.endTime);
        identity +=
            std::to_string(note.tick) + ":" + std::to_string(note.mask) + ";";
    }
    for (const auto &region : a.regions) {
        times.push_back(region.time);
    }
    times.push_back(finish);
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end()), times.end());
    const uint64_t fingerprint =
        std::stoull(hash(identity).substr(0, 16), nullptr, 16);
    const int trials =
        settings.success == 1 || settings.success == 0 ? 1 : settings.trials;
    for (int trial = 0; trial < trials; ++trial) {
        if (checkpoint) {
            checkpoint();
        }
        std::vector<size_t> misses(a.attacks.size() + 1);
        std::vector<bool> hit(a.attacks.size());
        for (size_t i = 0; i < hit.size(); ++i) {
            const auto word =
                randomWord(settings.seed ^ fingerprint ^
                           randomWord(uint64_t(trial)) ^ randomWord(i + 1));
            hit[i] = double(word >> 11) * (1.0 / 9007199254740992.0) <
                     settings.success;
            misses[i + 1] = misses[i] + static_cast<size_t>(!hit[i]);
        }
        double energy = settings.initialEnergy;
        double previous = times.front();
        double heldUntil = previous;
        bool active = false;
        size_t noteIndex = 0;
        size_t regionIndex = 0;
        const auto activate = [&](double time) {
            if (active || energy <= 0 || settings.strategy == "never" ||
                time >= finish) {
                return false;
            }
            if (settings.strategy == "immediate") {
                return true;
            }
            if (settings.strategy == "reserve") {
                return energy + 1e-9 >= settings.reserve;
            }
            if (settings.strategy == "dense") {
                return std::any_of(
                    a.regions.begin(), a.regions.end(),
                    [&](auto r) { return time >= r.time && time < r.endTime; });
            }
            // Explicit approximate foresight: choose a rich 8 s window in the
            // next 20 s.
            const auto count = [&](double start) {
                const auto lo = std::lower_bound(
                    a.attacks.begin(), a.attacks.end(), start,
                    [](auto n, double t) { return n.time < t; });
                const auto hi = std::lower_bound(
                    a.attacks.begin(), a.attacks.end(), start + 8,
                    [](auto n, double t) { return n.time < t; });
                return double(hi - lo);
            };
            double best = count(time);
            for (double delay : {4., 8., 12., 16., 20.}) {
                best = std::max(best, count(time + delay));
            }
            return count(time) >= .9 * best || energy >= .8;
        };
        for (double time : times) {
            if (active) {
                const double end = std::min(
                    time, previous + (energy / gpp::plus::drainPerSecond));
                total.activeSeconds += end - previous;
                total.benefit +=
                    50 * std::max(0., std::min(end, heldUntil) - previous);
                energy = std::max(0., energy - ((time - previous) *
                                                gpp::plus::drainPerSecond));
                if (trial == 0) {
                    total.activations.back().end = end;
                }
                if (energy <= 1e-12) {
                    energy = 0;
                    active = false;
                }
            }
            previous = time;
            if (trial == 0) {
                total.trajectory.push_back({time, energy, active});
            }
            while (regionIndex < a.regions.size() &&
                   a.regions[regionIndex].time <= time) {
                total.regionEnergy[regionIndex] += energy;
                total.availableRegions +=
                    static_cast<double>(energy + 1e-9 >= .2);
                ++regionIndex;
            }
            if (activate(time)) {
                active = true;
                if (trial == 0) {
                    total.activations.push_back({time, time});
                }
            }
            while (noteIndex < a.attacks.size() &&
                   a.attacks[noteIndex].time == time) {
                if (hit[noteIndex]) {
                    if (active) {
                        total.benefit += 50;
                    }
                    heldUntil =
                        std::max(heldUntil, a.attacks[noteIndex].endTime - .13);
                } else {
                    heldUntil = time;
                }
                for (size_t p : completedAt[noteIndex]) {
                    const bool success =
                        misses[ranges[p].second] == misses[ranges[p].first];
                    if (success) {
                        ++total.completed;
                        total.wasted +=
                            std::max(0., energy + gpp::plus::phraseEnergy -
                                             gpp::plus::maximumEnergy);
                        energy = std::min(gpp::plus::maximumEnergy,
                                          energy + gpp::plus::phraseEnergy);
                    } else {
                        ++total.lost;
                    }
                }
                ++noteIndex;
            }
            // Newly earned energy affects subsequent time, never the earning
            // attack.
            if (activate(time)) {
                active = true;
                if (trial == 0) {
                    total.activations.push_back({time, time});
                }
            }
            if (trial == 0) {
                total.trajectory.push_back({time, energy, active});
            }
        }
        total.finalEnergy += energy;
    }
    for (double *metric :
         {&total.completed, &total.lost, &total.wasted, &total.activeSeconds,
          &total.benefit, &total.availableRegions, &total.finalEnergy}) {
        *metric /= trials;
    }
    for (auto &energy : total.regionEnergy) {
        energy /= trials;
    }
    return total;
}

std::string plusReport(const State &state, int trials, uint64_t seed,
                       const std::function<void()> &checkpoint) {
    auto analysis = analyzePlus(state);
    const auto errors = plusProblems(state);
    analysis.diagnostics.insert(analysis.diagnostics.end(), errors.begin(),
                                errors.end());
    return plusReport(analysis, state.plus, trials, seed, checkpoint);
}

std::string plusReport(const PlusAnalysis &a, const PlusDocument &document,
                       int trials, uint64_t seed,
                       const std::function<void()> &checkpoint) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(12);
    out << "{\"schema\":1,\"algorithm\":\"plus-v1\",\"seed\":" << seed
        << ",\"trials\":" << trials
        << ",\"profile\":" << quoted(document.settings.profile)
        << ",\"economy\":{\"maximum\":" << gpp::plus::maximumEnergy
        << ",\"reward\":" << gpp::plus::phraseEnergy
        << ",\"drain_per_second\":" << gpp::plus::drainPerSecond
        << "},"
           "\"interpretation\":\"Independent attack trials; sequence requires "
           "every attack. Extra strums and judgment windows are not modeled. "
           "Benefit is normalized 1x potential, not a human score prediction. "
           "Sustain holding and future-score policy are approximations. "
           "Trajectories show trial 0, metrics are means.\",\"warnings\":[";
    const auto &problems = a.diagnostics;
    for (size_t i = 0; i < problems.size(); ++i) {
        if (i != 0u) {
            out << ',';
        }
        out << quoted(problems[i]);
    }
    out << "],\"regions\":[";
    for (size_t i = 0; i < a.regions.size(); ++i) {
        if (i != 0u) {
            out << ',';
        }
        const auto &r = a.regions[i];
        out << "{\"begin_tick\":" << r.begin << ",\"end_tick\":" << r.end
            << ",\"time\":" << r.time << ",\"end_time\":" << r.endTime
            << ",\"value\":" << r.value << '}';
    }
    out << "],\"phrases\":[";
    for (size_t i = 0; i < document.phrases.size(); ++i) {
        if (i != 0u) {
            out << ',';
        }
        const auto &p = document.phrases[i];
        const auto range = simulationRange(a, p);
        out << "{\"id\":" << p.id << ",\"begin_tick\":" << p.begin
            << ",\"end_tick\":" << p.end << ",\"time\":" << a.seconds(p.begin)
            << ",\"end_time\":" << a.seconds(p.end)
            << ",\"attacks\":" << range.second - range.first
            << ",\"risk\":" << p.risk << ",\"reason\":" << quoted(p.reason)
            << '}';
    }
    out << "],\"simulations\":[";
    bool comma = false;
    for (const auto *strategy :
         {"dense", "immediate", "reserve", "score", "never"}) {
        for (double success : {1., .9, .75, .5}) {
            PlusSimulationSettings settings;
            settings.strategy = strategy;
            settings.success = success;
            settings.trials = trials;
            settings.seed = seed;
            settings.reserve = std::max(.01, document.settings.reserve);
            const auto r =
                simulatePlus(a, document.phrases, settings, checkpoint);
            if (comma) {
                out << ',';
            }
            comma = true;
            out << "{\"completion\":" << quoted("sequence")
                << ",\"strategy\":" << quoted(strategy)
                << ",\"success_per_attack\":" << success
                << ",\"completed\":" << r.completed << ",\"lost\":" << r.lost
                << ",\"wasted_energy\":" << r.wasted
                << ",\"active_seconds\":" << r.activeSeconds
                << ",\"normalized_benefit\":" << r.benefit
                << ",\"regions_with_four_seconds\":" << r.availableRegions
                << ",\"final_energy\":" << r.finalEnergy
                << ",\"region_energy\":[";
            for (size_t i = 0; i < r.regionEnergy.size(); ++i) {
                if (i != 0u) {
                    out << ',';
                }
                out << r.regionEnergy[i];
            }
            out << "],\"activations_trial_0\":[";
            for (size_t i = 0; i < r.activations.size(); ++i) {
                if (i != 0u) {
                    out << ',';
                }
                out << '[' << r.activations[i].begin << ','
                    << r.activations[i].end << ']';
            }
            out << "]}";
        }
    }
    out << "]}\n";
    return out.str();
}
} // namespace ChartStudio
