#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/g_app_context.h"
#include "bess_core/scene/scene_component_types.h"
#include "bess_core/scene/scene_ser_reg.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "event_dispatcher.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace Bess::Canvas {
    void SceneState::setRuntime(SceneRuntimeCtx runtime) {
        const bool wasReady = m_runtime.scenes && m_runtime.sim;
        m_runtime = runtime;
        if (!wasReady && m_runtime.scenes && m_runtime.sim) {
            for (const auto &[id, comp] : m_componentsMap) {
                (void)id;
                if (comp) {
                    comp->onRuntimeReady(*this);
                }
            }
        }
    }

    const SceneRuntimeCtx &SceneState::runtime() const {
        return m_runtime;
    }

    SceneRuntimeCtx &SceneState::runtime() {
        return m_runtime;
    }

    bool SceneState::trackComp(SceneComponent &comp,
                               Json::Value before,
                               std::string key) {
        const auto after = comp.toEditJson();
        if (before == after) {
            return true;
        }
        if (comp.sceneState() != this || !m_runtime.compEdit ||
            !m_runtime.compEdit(
                m_sceneId, comp.getUuid(), before, after, std::move(key))) {
            comp.applyJson(before);
            return false;
        }
        return true;
    }

    bool SceneState::addTx(std::shared_ptr<SceneComponent> comp,
                           std::vector<std::shared_ptr<SceneComponent>> kids) {
        return m_runtime.add &&
               m_runtime.add(m_sceneId, std::move(comp), std::move(kids));
    }

    bool SceneState::addConnTx(std::shared_ptr<SceneComponent> conn) {
        return m_runtime.addConn &&
               m_runtime.addConn(m_sceneId, std::move(conn));
    }

    bool SceneState::nameTx(UUID comp, std::string name) {
        return m_runtime.name &&
               m_runtime.name(m_sceneId, comp, std::move(name));
    }

    std::vector<UUID> SceneState::connDeps(UUID conn) const {
        return m_runtime.connDeps ? m_runtime.connDeps(m_sceneId, conn)
                                  : std::vector<UUID>{};
    }

    bool SceneState::addBatchTx(std::vector<SceneAddOp> ops, bool hist) {
        return m_runtime.addBatch &&
               m_runtime.addBatch(m_sceneId, std::move(ops), hist);
    }

    void SceneState::clear() {
        std::lock_guard lock(m_componentsMutex);
        for (const auto &[id, comp] : m_componentsMap) {
            (void)id;
            if (comp) {
                comp->bindState(nullptr);
            }
        }
        m_runtimeIdMap.clear();
        m_componentsMap.clear();
        m_rootComponents.clear();
        m_freeRuntimeIds.clear();
        m_selectedComponents.clear();
        m_focusedUIComponent = UUID::null;
        if (m_uiNodeRegistry) {
            m_uiNodeRegistry->clear();
        } else {
            m_uiNodeRegistry = std::make_shared<UI::UINodeRegistry>();
        }

        // 0 will be used by widgets
        m_runtimeIdMap[0] = UUID::null;
    }

    std::shared_ptr<SceneComponent>
    SceneState::getComponentByUuidSP(const UUID &uuid) const {
        std::lock_guard lock(m_componentsMutex);
        auto itr = m_componentsMap.find(uuid);
        if (itr != m_componentsMap.end()) {
            return itr->second;
        }
        return nullptr;
    }

    SceneComponent *SceneState::getComponentByUuid(const UUID &uuid) const {
        auto itr = m_componentsMap.find(uuid);

        if (itr != m_componentsMap.end()) {
            return itr->second.get();
        }

        return nullptr;
    }

    const HashMap<UUID, std::shared_ptr<SceneComponent>> &
    SceneState::getAllComponents() const {
        return m_componentsMap;
    }

    const OrderedSet<UUID> &SceneState::getRootComponents() const {
        return m_rootComponents;
    }

    bool SceneState::isComponentValid(const UUID &uuid) const {
        return m_componentsMap.contains(uuid);
    }

    void SceneState::clearSelectedComponents() {
        std::lock_guard lock(m_componentsMutex);
        for (const auto &[uuid, selected] : m_selectedComponents) {
            m_componentsMap[uuid]->setIsSelected(false);
        }
        m_selectedComponents.clear();
    }

    void SceneState::addSelectedComponent(const UUID &uuid) {
        if (!isComponentValid(uuid))
            return;

        std::lock_guard lock(m_componentsMutex);
        m_selectedComponents[uuid] = true;
        m_componentsMap.at(uuid)->setIsSelected(true);
    }

    void SceneState::addSelectedComponent(const PickingId &id) {
        if (!id.isValid() || (id.info & PickingId::InfoFlags::unSelectable)) {
            return;
        }

        addSelectedComponent(runtimeIdToUuid(id.runtimeId));
    }

    void SceneState::removeSelectedComponent(const UUID &uuid) {
        if (!isComponentValid(uuid))
            return;

        std::lock_guard lock(m_componentsMutex);
        m_selectedComponents.erase(uuid);
        m_componentsMap.at(uuid)->setIsSelected(false);
    }

    void SceneState::removeSelectedComponent(const PickingId &id) {
        if (!id.isValid())
            return;
        removeSelectedComponent(runtimeIdToUuid(id.runtimeId));
    }

    bool SceneState::isComponentSelected(const UUID &uuid) const {
        auto itr = m_selectedComponents.find(uuid);

        if (itr == m_selectedComponents.end() || !itr->second) {
            return false;
        }

        return isComponentValid(uuid);
    }

    bool SceneState::isComponentSelected(const PickingId &pickingId) const {
        if (!pickingId.isValid())
            return false;

        return isComponentSelected(runtimeIdToUuid(pickingId.runtimeId));
    }

    const HashMap<UUID, bool> &SceneState::getSelectedComponents() const {
        return m_selectedComponents;
    }

    const UUID &SceneState::getFocusedUIComponent() const {
        return m_focusedUIComponent;
    }

    bool SceneState::isUIComponentFocused(const UUID &uuid) const {
        return m_focusedUIComponent == uuid && isComponentValid(uuid);
    }

    SceneComponent *SceneState::getFocusedUIComponentPtr() const {
        if (m_focusedUIComponent == UUID::null) {
            return nullptr;
        }

        return getComponentByUuid(m_focusedUIComponent);
    }

    bool SceneState::focusUIComponent(const UUID &uuid,
                                      const Events::FocusEvent &event) {
        auto next = getComponentByUuid(uuid);
        if (next == nullptr || !next->isFocusable()) {
            clearUIFocus(event);
            return false;
        }

        if (m_focusedUIComponent == uuid) {
            return true;
        }

        clearUIFocus(event);
        m_focusedUIComponent = uuid;

        auto focusEvent = event;
        focusEvent.entityUuid = uuid;
        focusEvent.sceneState = this;
        next->onFocusGained(focusEvent);
        return true;
    }

    void SceneState::clearUIFocus(const Events::FocusEvent &event) {
        if (m_focusedUIComponent == UUID::null) {
            return;
        }

        const auto previousId = m_focusedUIComponent;
        m_focusedUIComponent = UUID::null;

        auto previous = getComponentByUuid(previousId);
        if (previous == nullptr) {
            return;
        }

        auto focusEvent = event;
        focusEvent.entityUuid = previousId;
        focusEvent.sceneState = this;
        previous->onFocusLost(focusEvent);
    }

    void SceneState::attachChild(const UUID &parentId,
                                 const UUID &childId,
                                 bool emitEvent) {
        auto parent = getComponentByUuid(parentId);
        auto child = getComponentByUuid(childId);

        BESS_ASSERT(parent, "Parent was not found");
        BESS_ASSERT(child, "Child was not found");

        auto prevParentId = child->getParentComponent();
        if (prevParentId != UUID::null && prevParentId != parentId) {
            auto prevParent = getComponentByUuid(prevParentId);
            prevParent->removeChildComponent(childId);
        }

        if (!parent->getChildComponents().contains(childId)) {
            parent->addChildComponent(childId);
        }

        child->setParentComponent(parentId);

        BESS_INFO("[SceneState] Attached component {} to parent component {}",
                  (uint64_t)childId,
                  (uint64_t)parentId);

        if (emitEvent) {
            auto &appCtx = GAppContext::getInstance();
            auto eventDispatcher =
                appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>();
            eventDispatcher->queue(
                Events::EntityReparentedEvent{.entityUuid = childId,
                                              .newParentUuid = parentId,
                                              .prevParent = prevParentId,
                                              .sceneId = m_sceneId,
                                              .state = this});
        }

        m_rootComponents.erase(childId);
    }

    void SceneState::detachChild(const UUID &childId, bool emitEvent) {
        const auto &child = getComponentByUuid(childId);
        BESS_ASSERT(child, "(Detach Child) Child not found");

        const auto parentId = child->getParentComponent();
        if (parentId == UUID::null) {
            return;
        }

        const auto &parent = getComponentByUuid(parentId);

        parent->removeChildComponent(childId);
        child->setParentComponent(UUID::null);

        BESS_INFO("[SceneState] Detached component {} from parent component {}",
                  (uint64_t)childId,
                  (uint64_t)parentId);

        if (emitEvent) {
            auto &appCtx = GAppContext::getInstance();
            auto eventDispatcher =
                appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>();
            eventDispatcher->queue(
                Events::EntityReparentedEvent{.entityUuid = childId,
                                              .newParentUuid = UUID::null,
                                              .prevParent = parentId,
                                              .sceneId = m_sceneId,
                                              .state = this});
        }

        m_rootComponents.insert(childId);
    }

    void SceneState::assignRuntimeId(const UUID &uuid) {
        auto component = getComponentByUuid(uuid);
        BESS_ASSERT(component, "Component was not found");

        uint32_t runtimeId = component->getRuntimeId();

        BESS_ASSERT(runtimeId == PickingId::invalidRuntimeId,
                    "Component already has a runtimeId assigned");

        if (!m_freeRuntimeIds.empty()) {
            runtimeId = *m_freeRuntimeIds.rbegin();
            m_freeRuntimeIds.erase(runtimeId);
            BESS_ASSERT(runtimeIdToUuid(runtimeId) == UUID::null,
                        "Reusing runtimeId that is still mapped");
        } else {
            runtimeId = static_cast<uint32_t>(m_runtimeIdMap.size());
        }

        component->setRuntimeId(runtimeId);
        m_runtimeIdMap[runtimeId] = uuid;
    }

    std::shared_ptr<SceneComponent>
    SceneState::getComponentByPickingId(const PickingId &id) const {
        if (!id.isValid())
            return nullptr;

        return getComponentByUuidSP(runtimeIdToUuid(id.runtimeId));
    }

    std::vector<UUID> SceneState::removeComponent(const UUID &uuid,
                                                  const UUID &callerId) {

        BESS_INFO("[SceneState] Removing component {}", (uint64_t)uuid);
        auto component = getComponentByUuid(uuid);
        BESS_ASSERT(component, "Component was not found");

        /// For now, Preventing removing child components directly
        /// If parent is not the caller, then do not remove
        /// TODO(Shivang): Add lifetime ownership management later
        if (component->getParentComponent() != UUID::null &&
            callerId != UUID::master &&
            component->getParentComponent() != callerId) {
            BESS_WARN("[SceneState] Attempt to remove child component {} "
                      "directly prevented",
                      (uint64_t)uuid);
            return {};
        }

        std::vector<UUID> removedUuids = component->cleanup(*this, callerId);
        removedUuids.push_back(uuid);

        const uint32_t runtimeId = component->getRuntimeId();
        if (runtimeId != PickingId::invalidRuntimeId) {
            component->setRuntimeId(
                PickingId::invalidRuntimeId); // Don't remove this its not
                                              // redundant
            m_runtimeIdMap[runtimeId] = UUID::null;
            m_freeRuntimeIds.insert(runtimeId);
        }

        removeSelectedComponent(uuid);
        if (m_focusedUIComponent == uuid) {
            clearUIFocus({
                .entityUuid = uuid,
                .sceneState = this,
            });
        }

        if (component->getParentComponent() == UUID::null) {
            m_rootComponents.erase(uuid);
        }

        std::unique_lock lock(m_componentsMutex);
        m_componentsMap.erase(uuid);
        lock.unlock();
        component->bindState(nullptr);

        if (callerId == UUID::master &&
            component->getParentComponent() != UUID::null) {
            auto parentComp =
                getComponentByUuid(component->getParentComponent());
            if (parentComp) {
                parentComp->removeChildComponent(uuid);
            }
        }

        auto &appCtx = GAppContext::getInstance();
        auto eventDispatcher =
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>();
        eventDispatcher->queue(
            Events::ComponentRemovedEvent{.uuid = uuid,
                                          .type = component->getType(),
                                          .sceneId = m_sceneId,
                                          .state = this});

        return removedUuids;
    }

    void SceneState::orphanComponent(const UUID &uuid) {
        auto component = getComponentByUuid(uuid);
        BESS_ASSERT(component, "Component was not found");

        auto parentId = component->getParentComponent();
        if (parentId != UUID::null) {
            component->setParentComponent(UUID::null);
            m_rootComponents.insert(uuid);
        }

        BESS_INFO("[SceneState] Orphaned component {}", (uint64_t)uuid);

        auto &appCtx = GAppContext::getInstance();
        auto eventDispatcher =
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>();
        eventDispatcher->queue(
            Events::EntityReparentedEvent{.entityUuid = uuid,
                                          .newParentUuid = UUID::null,
                                          .prevParent = parentId,
                                          .sceneId = m_sceneId,
                                          .state = this});
    }

    bool SceneState::isRootComponent(const UUID &uuid) const {
        return m_rootComponents.contains(uuid);
    }

    SceneState::SceneState() {
        clear();
    };

    SceneState::SceneState(const SceneState &scene) {
        BESS_ERROR("SceneState copy is not allowed");
        BESS_ASSERT(false, "SceneState copy is not allowed");
    }

    void SceneState::removeFromMap(const UUID &uuid) {
        auto component = getComponentByUuid(uuid);
        BESS_ASSERT(component, "Component was not found");

        const uint32_t runtimeId = component->getRuntimeId();
        if (runtimeId != PickingId::invalidRuntimeId) {
            component->setRuntimeId(
                PickingId::invalidRuntimeId); // Don't remove this its not
                                              // redundant
            m_runtimeIdMap[runtimeId] = UUID::null;
            m_freeRuntimeIds.insert(runtimeId);
        }

        m_rootComponents.erase(uuid);
        m_selectedComponents.erase(uuid);
        if (m_focusedUIComponent == uuid) {
            clearUIFocus({
                .entityUuid = uuid,
                .sceneState = this,
            });
        }
        component->bindState(nullptr);
        m_componentsMap.erase(uuid);
    }

    const UUID &SceneState::runtimeIdToUuid(uint32_t runtimeId) const {
        auto itr = m_runtimeIdMap.find(runtimeId);
        if (itr == m_runtimeIdMap.end()) {
            BESS_ASSERT(
                false, "RuntimeId {} not found in runtimeIdMap", runtimeId);
            return UUID::null;
        }
        return itr->second;
    }

} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SceneState &state, Json::Value &j) {
        j["components"] = Json::Value(Json::arrayValue);

        for (const auto &[uuid, component] : state.getAllComponents()) {
            component->beforeSerialize(state);
            j["components"].append(component->toJson());
        }

        JsonConvert::toJsonValue(state.getSceneId(), j["sceneId"]);
        JsonConvert::toJsonValue(state.getModuleId(), j["moduleId"]);
        JsonConvert::toJsonValue(state.getParentSceneId(), j["parentSceneId"]);
        j["isRootScene"] = state.getIsRootScene();
    }

    void fromJsonValue(const Json::Value &j, Bess::Canvas::SceneState &state) {
        fromJsonValue(j, state, {});
    }

    void fromJsonValue(const Json::Value &j,
                       Bess::Canvas::SceneState &state,
                       const Bess::Canvas::SceneLoadCtx &ctx) {
        state.clear();

        if (!j.isMember("components") || !j["components"].isArray()) {
            return;
        }

        JsonConvert::fromJsonValue(j["sceneId"], state.getSceneId());
        JsonConvert::fromJsonValue(j["moduleId"], state.getModuleId());
        JsonConvert::fromJsonValue(j["parentSceneId"],
                                   state.getParentSceneId());
        state.setIsRootScene(j["isRootScene"].asBool());

        std::vector<std::shared_ptr<Canvas::SceneComponent>>
            deserializedComponents;
        deserializedComponents.reserve(j["components"].size());

        for (const auto &compJson : j["components"]) {
            if (!compJson.isMember("typeName")) {
                BESS_WARN("Component JSON is missing typeName field. Skipping "
                          "component.");
                continue;
            }

            std::shared_ptr<Canvas::SceneComponent> comp = nullptr;

            const auto typeName = compJson["typeName"].asString();

            comp = Canvas::SceneSerReg::createComponentFromJson(compJson);
            if (!comp) {
                BESS_WARN("No deserializer found for {}. Skipping component.",
                          typeName);
                continue;
            }

            comp->onLoaded(ctx);

            state.addComponent(comp, false, false);
            deserializedComponents.push_back(comp);
        }

        for (const auto &comp : deserializedComponents) {
            comp->onAttach(state);
        }
    }

} // namespace Bess::JsonConvert
