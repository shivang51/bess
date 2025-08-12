#pragma once
#include "component_types/component_types.h"
#include "scene/components/non_sim_comp.h"
#include <string>

namespace Bess::Common {
    class Helpers {
      public:
        static std::string getComponentIcon(SimEngine::ComponentType type);

        static std::string getComponentIcon(Canvas::Components::NSComponentType type);

        static std::string toLowerCase(const std::string &str);
    };
} // namespace Bess::Common
