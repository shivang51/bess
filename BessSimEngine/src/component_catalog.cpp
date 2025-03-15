#include "component_catalog.h"
#include <algorithm>

namespace Bess::SimEngine {

    ComponentCatalog &ComponentCatalog::instance() {
        static ComponentCatalog instance;
        return instance;
    }

    void ComponentCatalog::registerComponent(const ComponentDefinition &def) {
        // Avoid duplicate registration.
        auto it = std::find_if(components.begin(), components.end(),
                               [&def](const ComponentDefinition &existing) {
                                   return existing.type == def.type;
                               });
        if (it == components.end()) {
            components.push_back(def);
        }
    }

    const std::vector<ComponentDefinition> &ComponentCatalog::getComponents() const {
        return components;
    }

    std::unordered_map<std::string, std::vector<ComponentDefinition>> ComponentCatalog::getComponentsTree() const {
        std::unordered_map<std::string, std::vector<ComponentDefinition>> catalog;
        for (auto &comp : components) {
            catalog[comp.category].emplace_back(comp);
        }
        return catalog;
    }

    const ComponentDefinition *ComponentCatalog::getComponentDefinition(ComponentType type) const {
        auto it = std::find_if(components.begin(), components.end(),
                               [type](const ComponentDefinition &def) {
                                   return def.type == type;
                               });
        assert(it != components.end());
        return &(*it);
    }

} // namespace Bess::SimEngine
