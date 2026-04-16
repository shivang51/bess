#include "common/helpers.h"
#include <algorithm>

namespace Bess::Common {
    std::string Helpers::toLowerCase(const std::string &str) {
        std::string data = str;
        std::ranges::transform(data, data.begin(),
                               [](unsigned char c) { return std::tolower(c); });
        return data;
    }

    std::string Helpers::toUpperCase(const std::string &str) {
        std::string data = str;
        std::ranges::transform(data, data.begin(),
                               [](unsigned char c) { return std::toupper(c); });
        return data;
    }
} // namespace Bess::Common
