#pragma once

#include "types.h"
#include <any>
#include <cassert>
#include <unordered_map>
#include <vector>
namespace Bess::SimEngine::Properties {
    enum class ComponentProperty : uint8_t {
        inputCount,
        outputCount,
    };

    typedef std::unordered_map<ComponentProperty, std::any> ComponentProperties;

    typedef std::unordered_map<Properties::ComponentProperty, std::vector<std::any>> ModifiableProperties;
} // namespace Bess::SimEngine::Properties
