#pragma once
#include "component_types.h"
#include <string>

namespace Bess::Common {
    class Helpers {
      public:
        static std::string getComponentIcon(SimEngine::ComponentType type);

        static std::string toLowerCase(const std::string &str);
    };
} // namespace Bess::Common
