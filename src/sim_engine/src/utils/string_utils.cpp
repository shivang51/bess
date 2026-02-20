#include "utils/string_utils.h"

#include <algorithm>
#include <map>
#include <string>

namespace Bess::SimEngine::StringUtils {

    SlotType toPinType(std::string str) {
        std::ranges::transform(str, str.begin(), ::tolower);

        if (str == "output") {
            return SlotType::digitalOutput;
        }

        return SlotType::digitalInput;
    }
} // namespace Bess::SimEngine::StringUtils
