#include "utils/string_utils.h"

#include <algorithm>
#include <map>
#include <string>

namespace Bess::SimEngine::StringUtils {

    PinType toPinType(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);

        if (str == "output") {
            return PinType::output;
        }

        return PinType::input;
    }
} // namespace Bess::SimEngine::StringUtils
