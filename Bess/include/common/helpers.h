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

        template <std::size_t N1, std::size_t N2>
        static constexpr auto concat(const char (&a)[N1], const char (&b)[N2]) {
            std::array<char, N1 + N2 - 1> result{}; // -1 to drop null from first
            for (std::size_t i = 0; i < N1 - 1; i++)
                result[i] = a[i];
            for (std::size_t i = 0; i < N2; i++)
                result[N1 - 1 + i] = b[i];
            return result;
        }
    };
} // namespace Bess::Common
