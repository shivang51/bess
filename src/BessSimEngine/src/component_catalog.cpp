#include "component_catalog.h"
#include "logger.h"
#include <algorithm>

namespace Bess::SimEngine {

    ComponentCatalog &ComponentCatalog::instance() {
        static ComponentCatalog instance;
        return instance;
    }

    void ComponentCatalog::registerComponent(ComponentDefinition def, const SpecialType specialType) {
        if (m_componentHashMap.contains(def.getHash())) {
            return;
        }

        auto compPtr = std::make_shared<const ComponentDefinition>(std::move(def));
        m_components.emplace_back(compPtr);
        m_componentHashMap[compPtr->getHash()] = compPtr;

        if (specialType != SpecialType::none) {
            m_specialTypeMap[specialType] = compPtr->getHash();
        }

        m_componentTree = nullptr;
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
        if (m_componentHashMap.contains(hash)) {
            return m_componentHashMap.at(hash);
        }

        return nullptr;
    }

    ComponentDefinition ComponentCatalog::getComponentDefinitionCopy(uint64_t hash) {
        if (m_componentHashMap.contains(hash)) {
            return *(m_componentHashMap.at(hash));
        }

        BESS_SE_ERROR("Component definition with hash {} not found", hash);
        throw std::runtime_error("Component definition not found");
    }

    std::shared_ptr<const ComponentDefinition> ComponentCatalog::getSpecialCompDef(SpecialType specialType) const {
        if (m_specialTypeMap.contains(specialType)) {
            uint64_t hash = m_specialTypeMap.at(specialType);
            return getComponentDefinition(hash);
        }
        return nullptr;
    }

    bool ComponentCatalog::isSpecialCompDef(uint64_t hash, SpecialType type) const {
        if (!m_specialTypeMap.contains(type))
            return false;

        return m_specialTypeMap.at(type) == hash;
    }

    bool ComponentCatalog::isRegistered(uint64_t hash) const {
        return m_componentHashMap.contains(hash);
    }

    void ComponentCatalog::destroy() {
        m_specialTypeMap.clear();
        m_componentHashMap.clear();
        m_componentTree = nullptr;
        m_components.clear();
    }
} // namespace Bess::SimEngine
