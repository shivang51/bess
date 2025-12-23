#pragma once

#include "bess_api.h"
#include "component_definition.h"
#include <cstdint>
#include <entt/entt.hpp>
#include <memory>

namespace Bess::SimEngine {

    class BESS_API ComponentCatalog {
      public:
        void destroy();

        static ComponentCatalog &instance();

        // Register a new component definition.
        // If one with the same ComponentType exists, it won't be added.
        void registerComponent(ComponentDefinition def);

        // Get the full list of registered components.
        const std::vector<std::shared_ptr<const ComponentDefinition>> &getComponents() const;

        // Get the full list of registered components as tree format, grouped based on the category.
        typedef std::unordered_map<std::string, std::vector<std::shared_ptr<const ComponentDefinition>>> ComponentTree;
        std::shared_ptr<ComponentTree> getComponentsTree();

        // Look up a component definition by its hash.
        std::shared_ptr<const ComponentDefinition> getComponentDefinition(uint64_t hash) const;
        ComponentDefinition getComponentDefinitionCopy(uint64_t hash);

        bool isRegistered(uint64_t hash) const;

      private:
        ComponentCatalog() = default;
        std::vector<std::shared_ptr<const ComponentDefinition>> m_components;
        std::shared_ptr<ComponentTree> m_componentTree = nullptr;
        std::unordered_map<uint64_t, std::shared_ptr<const ComponentDefinition>> m_componentHashMap;
    };
} // namespace Bess::SimEngine
