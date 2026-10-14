#pragma once
#include "core.h"
#include "plus_generator.h"
namespace ChartStudio {
std::pair<PlusAnalysis, PlusDocument>
readPlusReference(const fs::path &path,
                  const std::string &instrument = "[ExpertSingle]");
struct Reference {
    std::vector<gpp::charter::Note> notes;
    std::string name, instrument;
};
Reference readReference(const fs::path &path,
                        const std::string &instrument = "[ExpertSingle]");
void exportValidated(const State &state, const fs::path &path, bool replace);
std::string compare(const Reference &left, const Reference &right,
                    double leftOffset = 0, double rightOffset = 0,
                    double window = 10);
Reference asReference(const State &state);
} // namespace ChartStudio
