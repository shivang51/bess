#include "component_catalog.h"
#include "common/logger.h"
#include "drivers/sim_driver.h"
#include <string>

namespace Bess::SimEngine {

    ComponentCatalog &ComponentCatalog::instance() {
        static ComponentCatalog instance;
        return instance;
    }

    const std::vector<std::shared_ptr<Drivers::CompDef>> &ComponentCatalog::getComponents() const {
        return m_components;
    }

    std::shared_ptr<ComponentCatalog::ComponentTree> ComponentCatalog::getComponentsTree() {
        if (m_componentTree != nullptr)
            return m_componentTree;

        m_componentTree = std::make_shared<ComponentTree>();

        for (const auto &comp : m_components) {
            m_componentTree->operator[](comp->getGroupName()).emplace_back(comp);
        }
        return m_componentTree;
    }

    std::shared_ptr<Drivers::CompDef> ComponentCatalog::getComponentDefinition(const std::string &name) const {
        if (m_compNameMap.contains(name)) {
            return m_compNameMap.at(name);
        }

        return nullptr;
    }

    std::shared_ptr<Drivers::CompDef> ComponentCatalog::getComponentDefinitionCopy(const std::string &name) {
        if (m_compNameMap.contains(name)) {
            return m_compNameMap.at(name)->clone();
        }

        BESS_ERROR("Component definition with name {} not found", name);
        throw std::runtime_error("Component definition not found");
    }

    bool ComponentCatalog::isRegistered(const std::string &name) const {
        return m_compNameMap.contains(name);
    }

    void ComponentCatalog::destroy() {
        m_compNameMap.clear();
        m_componentTree = nullptr;
        m_components.clear();
    }

    std::shared_ptr<Drivers::CompDef> ComponentCatalog::findDefByName(const std::string &name) const {
        for (const auto &comp : m_components) {
            if (comp->getName() == name) {
                return comp;
            }
        }
        return nullptr;
    }
} // namespace Bess::SimEngine
