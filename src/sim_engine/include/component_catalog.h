#pragma once

#include "bess_api.h"
#include "drivers/sim_driver.h"
#include <memory>

namespace Bess::SimEngine {

    class BESS_API ComponentCatalog {
      public:
        void destroy();

        static ComponentCatalog &instance();

        // Register a new component definition.
        // If one with the same ComponentType exists, it won't be added.
        template <typename TCompDefination>
        void registerComponent(TCompDefination def) {
            if (m_compNameMap.contains(def.getName())) {
                return;
            }

            auto compPtr = std::make_shared<TCompDefination>(std::move(def));
            m_components.emplace_back(compPtr);
            m_compNameMap[compPtr->getName()] = compPtr;

            m_componentTree = nullptr;
        }

        // Register a new component definition.
        // If one with the same NAME exists, it won't be added.
        template <typename TCompDefination>
        void registerComponent(const std::shared_ptr<TCompDefination> &def) {
            if (m_compNameMap.contains(def->getName())) {
                return;
            }

            m_components.emplace_back(def);
            m_compNameMap[def->getName()] = def;
            m_componentTree = nullptr;
        }

        // Get the full list of registered components.
        const std::vector<std::shared_ptr<Drivers::ComponentDef>> &getComponents() const;

        // Get the full list of registered components as tree format, grouped based on the category.
        typedef std::unordered_map<std::string,
                                   std::vector<std::shared_ptr<Drivers::ComponentDef>>>
            ComponentTree;
        std::shared_ptr<ComponentTree> getComponentsTree();

        template <typename TCompDef>
        std::shared_ptr<TCompDef> getComponentDefinition(const std::string &name) const {
            if (m_compNameMap.contains(name)) {
                return std::dynamic_pointer_cast<TCompDef>(m_compNameMap.at(name));
            }
            return nullptr;
        }

        // Look up a component definition by its hash.
        std::shared_ptr<Drivers::ComponentDef> getComponentDefinition(const std::string &name) const;

        std::shared_ptr<Drivers::ComponentDef> getComponentDefinitionCopy(const std::string &name);

        std::shared_ptr<Drivers::ComponentDef> findDefByName(const std::string &name) const;

        bool isRegistered(const std::string &name) const;

      private:
        ComponentCatalog() = default;
        std::vector<std::shared_ptr<Drivers::ComponentDef>> m_components;
        std::shared_ptr<ComponentTree> m_componentTree = nullptr;
        std::unordered_map<std::string, std::shared_ptr<Drivers::ComponentDef>> m_compNameMap;
    };
} // namespace Bess::SimEngine
