#pragma once

#include "component_definition.h"
#include "component_types.h"
#include <entt/entt.hpp>

namespace Bess::SimEngine {

    class ComponentCatalog {
      public:
        static ComponentCatalog &instance();

        // Register a new component definition.
        // If one with the same ComponentType exists, it won't be added.
        void registerComponent(const ComponentDefinition &def);

        // Get the full list of registered components.
        const std::vector<ComponentDefinition> &getComponents() const;

        // Get the full list of registered components as tree format, grouped based on the category.
        std::unordered_map<std::string, std::vector<ComponentDefinition>> getComponentsTree() const;

        // Look up a component definition by its enum type.
        const ComponentDefinition *getComponentDefinition(ComponentType type) const;

      private:
        ComponentCatalog() = default;
        std::vector<ComponentDefinition> components;
    };
} // namespace Bess::SimEngine
