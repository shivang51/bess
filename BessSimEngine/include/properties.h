#pragma once

#include <any>
#include <cassert>
#include <unordered_map>
namespace Bess::SimEngine::Properties {
    enum class ComponentProperty {
        inputCount,
        outputCount,
    };

    typedef std::unordered_map<ComponentProperty, std::any> ComponentProperties;
} // namespace Bess::SimEngine::Properties
