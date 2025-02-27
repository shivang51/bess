#pragma once

#include <entt/entt.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::SimEngine {
    // Enum for all supported component types.
    enum class ComponentType {
        INPUT,
        OUTPUT,
        AND,
        OR,
        NOT
    };

    // Definition for a digital component/gate.
    struct ComponentDefinition {
        ComponentType type; // Enum type
        std::string name;   // Human-readable name for the component.
        std::string category;
        int inputCount;  // Number of input pins.
        int outputCount; // Number of output pins.
        // Lambda function to simulate the component.
        // Returns true if the component's state changed.
        std::function<bool(entt::registry &, entt::entity)> simulationFunction;
    };

    class ComponentCatalog {
      public:
        // Access the singleton instance.
        static ComponentCatalog &instance();

        // Register a new component definition.
        // If one with the same ComponentType exists, it won't be added.
        void registerComponent(const ComponentDefinition &def);

        // Get the full list of registered components.
        const std::vector<ComponentDefinition> &getComponents() const;
        std::unordered_map<std::string, std::vector<ComponentDefinition>> getComponentsTree() const;

        // Look up a component definition by its enum type.
        const ComponentDefinition *getComponent(ComponentType type) const;

      private:
        ComponentCatalog() = default;
        std::vector<ComponentDefinition> components;
    };
} // namespace Bess::SimEngine
