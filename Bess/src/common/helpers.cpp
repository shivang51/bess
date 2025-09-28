#include "common/helpers.h"
#include "scene/renderer/renderer.h"
#include "ui/icons/ComponentIcons.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/icons/MaterialIcons.h"
#include <algorithm>

namespace Bess::Common {
    std::string Helpers::toLowerCase(const std::string &str) {
        std::string data = str;
        std::ranges::transform(data, data.begin(),
                               [](const unsigned char c) { return std::tolower(c); });
        return data;
    }

    std::string Helpers::getComponentIcon(const SimEngine::ComponentType type) {
        using namespace UI::Icons;
        switch (type) {
        case SimEngine::ComponentType::AND:
            return ComponentIcons::AND_GATE;
        case SimEngine::ComponentType::NAND:
            return ComponentIcons::NAND_GATE;
        case SimEngine::ComponentType::OR:
            return ComponentIcons::OR_GATE;
        case SimEngine::ComponentType::NOR:
            return ComponentIcons::NOR_GATE;
        case SimEngine::ComponentType::XOR:
            return ComponentIcons::XOR_GATE;
        case SimEngine::ComponentType::XNOR:
            return ComponentIcons::XNOR_GATE;
        default:
            break;
        }

        return std::string(" ") + FontAwesomeIcons::FA_CUBE + " ";
    }

    std::string Helpers::getComponentIcon(const Canvas::Components::NSComponentType type) {
        using namespace UI::Icons;
        switch (type) {
        case Canvas::Components::NSComponentType::text:
            return MaterialIcons::TEXT_FIELDS;
            break;
        default:
            break;
        }
        return std::string(" ") + FontAwesomeIcons::FA_CUBE + " ";
    }

} // namespace Bess::Common
