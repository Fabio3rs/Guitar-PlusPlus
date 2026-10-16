#pragma once
#include "core.h"
namespace ChartStudio {
void synthProcess(const std::vector<std::string> &args,
                  const std::function<void()> &checkpoint);
}
