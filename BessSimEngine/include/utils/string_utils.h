#pragma once

#include "component_types/component_types.h"
#include "types.h"
#include <string>

namespace Bess::SimEngine::StringUtils {
    ComponentType toComponentType(std::string str);

    PinType toPinType(std::string str);
} // namespace Bess::SimEngine::StringUtils