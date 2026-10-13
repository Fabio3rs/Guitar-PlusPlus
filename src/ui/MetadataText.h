#pragma once
#include <string>

namespace ui {
// Charts commonly embed Clone Hero color/style tags. Display their text without
// interpreting markup or changing the persisted source metadata.
inline std::string metadataDisplayText(const std::string &text) {
    std::string result;
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '<') {
            const auto end = text.find('>', i);
            if (end != std::string::npos) {
                const auto tag = text.substr(i + 1, end - i - 1);
                if (tag == "b" || tag == "/b" || tag == "i" || tag == "/i" ||
                    tag == "/color" || tag.compare(0, 6, "color=") == 0) {
                    i = end + 1;
                    continue;
                }
            }
        }
        result += text[i++];
    }
    return result;
}

} // namespace ui
