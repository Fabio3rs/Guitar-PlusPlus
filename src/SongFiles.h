#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace songfiles {
using Paths = std::vector<std::filesystem::path>;
std::string lower(std::string value);
Paths list(const std::filesystem::path &directory);
std::filesystem::path named(const Paths &, const std::string &name);
Paths charts(const std::filesystem::path &directory, const Paths &files);
std::filesystem::path audio(const std::filesystem::path &directory,
                            const Paths &files);
} // namespace songfiles
