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

    template <typename ReturnType>
    ReturnType getProperty(const ComponentProperties &properties, ComponentProperty propertyName) {
        assert(properties.find(propertyName) != properties.end());
        return std::any_cast<ReturnType>(properties.at(propertyName));
    }
} // namespace Bess::SimEngine::Properties
