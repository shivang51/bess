#include "component_catalog.h"
#include <algorithm>

namespace Bess::SimEngine {

    ComponentCatalog &ComponentCatalog::instance() {
        static ComponentCatalog instance;
        return instance;
    }

    void ComponentCatalog::registerComponent(ComponentDefinition def) {
        auto it = std::find_if(m_components.begin(), m_components.end(),
                               [&def](const std::shared_ptr<const ComponentDefinition> existing) {
                                   return existing->type == def.type;
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

        for (auto &comp : m_components) {
            m_componentTree->operator[](comp->category).emplace_back();
        }
        return m_componentTree;
    }

    std::shared_ptr<const ComponentDefinition> ComponentCatalog::getComponentDefinition(ComponentType type) const {
        auto it = std::find_if(m_components.begin(), m_components.end(),
                               [&type](const std::shared_ptr<const ComponentDefinition> existing) {
                                   return existing->type == type;
                               });
        assert(it != m_components.end());
        return *it;
    }

} // namespace Bess::SimEngine
