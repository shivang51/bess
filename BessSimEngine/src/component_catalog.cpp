#include "component_catalog.h"
#include <algorithm>

namespace Bess::SimEngine {

    ComponentCatalog &ComponentCatalog::instance() {
        static ComponentCatalog instance;
        return instance;
    }

    void ComponentCatalog::registerComponent(const ComponentDefinition &def) {
        // Avoid duplicate registration.
        auto it = std::find_if(m_components.begin(), m_components.end(),
                               [&def](const ComponentDefinition &existing) {
                                   return existing.type == def.type;
                               });
        if (it == m_components.end()) {
            m_components.push_back(def);
        }
    }

    const std::vector<ComponentDefinition> &ComponentCatalog::getComponents() const {
        return m_components;
    }

    std::shared_ptr<ComponentCatalog::ComponentTree> ComponentCatalog::getComponentsTree(){
        if (m_componentTree != nullptr)
            return m_componentTree;

        m_componentTree = std::make_shared<ComponentTree>();

        for (auto &comp : m_components) {
            m_componentTree->operator[](comp.category).emplace_back(&comp);
        }
        return m_componentTree;
    }

    const ComponentDefinition *ComponentCatalog::getComponentDefinition(ComponentType type) const {
        auto it = std::find_if(m_components.begin(), m_components.end(),
                               [type](const ComponentDefinition &def) {
                                   return def.type == type;
                               });
        assert(it != m_components.end());
        return std::to_address(it);
    }

} // namespace Bess::SimEngine
