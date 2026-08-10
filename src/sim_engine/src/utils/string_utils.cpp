#include "utils/string_utils.h"

#include <algorithm>
#include <map>
#include <string>

namespace Bess::SimEngine::StringUtils {

    PortDirection toPortDirection(std::string str) {
        std::ranges::transform(str, str.begin(), ::tolower);

        if (str == "output") {
            return PortDirection::output;
        }

        return PortDirection::input;
    }
} // namespace Bess::SimEngine::StringUtils
