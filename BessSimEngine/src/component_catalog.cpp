#include "component_catalog.h"
#include "logger.h"
#include <algorithm>

namespace Bess::SimEngine {

    ComponentCatalog &ComponentCatalog::instance() {
        static ComponentCatalog instance;
        return instance;
    }

    void ComponentCatalog::registerComponent(ComponentDefinition def) {
        auto it = std::find_if(m_components.begin(), m_components.end(),
                               [&def](const std::shared_ptr<const ComponentDefinition> &existing) {
                                   return existing->getHash() == def.getHash();
                               });
        if (it == m_components.end()) {
            m_components.emplace_back(std::make_shared<const ComponentDefinition>(def));
        }
    }

    const std::vector<std::shared_ptr<const ComponentDefinition>> &ComponentCatalog::getComponents() const {
        return m_components;
    }

    std::shared_ptr<ComponentCatalog::ComponentTree> ComponentCatalog::getComponentsTree() {
        if (m_componentTree != nullptr)
            return m_componentTree;

        m_componentTree = std::make_shared<ComponentTree>();

        for (const auto &comp : m_components) {
            m_componentTree->operator[](comp->category).emplace_back(comp);
        }
        return m_componentTree;
    }

    std::shared_ptr<const ComponentDefinition> ComponentCatalog::getComponentDefinition(uint64_t hash) const {
        auto it = std::ranges::find_if(m_components,
                                       [&hash](const std::shared_ptr<const ComponentDefinition> &existing) {
                                           return existing->getHash() == hash;
                                       });
        assert(it != m_components.end());
        return *it;
    }

} // namespace Bess::SimEngine
