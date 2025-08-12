#include "utils/string_utils.h"

#include <algorithm>
#include <map>
#include <string>

namespace Bess::SimEngine::StringUtils {
    ComponentType toComponentType(std::string str) {
        static const std::map<std::string, ComponentType> stringToTypeMap = {
#define COMPONENT(name, value) {#name, ComponentType::name},
#include "component_types/component_types.def"
#undef COMPONENT
        };

        std::transform(str.begin(), str.end(), str.begin(), ::tolower);

        auto it = stringToTypeMap.find(str);
        if (it != stringToTypeMap.end()) {
            return it->second;
        }

        return ComponentType::EMPTY;
    }

    PinType toPinType(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);

        if (str == "output") {
            return PinType::output;
        }

        return PinType::input;
    }
} // namespace Bess::SimEngine::StringUtils