#include "project_session/scene_document.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace Bess::Session {
    namespace {
        template <typename T>
        void captureOptional(const entt::registry &registry,
                             entt::entity entity,
                             std::optional<T> &destination) {
            if (const auto *component = registry.try_get<T>(entity)) {
                destination = *component;
            }
        }

        template <typename T>
        void applyOptional(entt::registry &registry,
                           entt::entity entity,
                           const std::optional<T> &source) {
            if (source) {
                registry.emplace_or_replace<T>(entity, *source);
            } else {
                registry.remove<T>(entity);
            }
        }

        void eraseId(std::vector<UUID> &ids, UUID id) {
            ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
        }

        bool containsId(const std::vector<UUID> &ids, UUID id) {
            return std::find(ids.begin(), ids.end(), id) != ids.end();
        }
    } // namespace

    SceneDocument::SceneDocument(SceneMetadata metadata)
        : m_metadata(std::move(metadata)) {
        if (m_metadata.id == UUID::null) {
            m_metadata.id = UUID{};
        }
    }

    const SceneMetadata &SceneDocument::metadata() const noexcept {
        return m_metadata;
    }

    SceneMetadata &SceneDocument::metadata() noexcept {
        return m_metadata;
    }

    const entt::registry &SceneDocument::registry() const noexcept {
        return m_registry;
    }

    bool SceneDocument::contains(UUID id) const noexcept {
        return m_entities.contains(id);
    }

    std::size_t SceneDocument::size() const noexcept {
        return m_entities.size();
    }

    entt::entity SceneDocument::entity(UUID id) const noexcept {
        const auto found = m_entities.find(id);
        return found == m_entities.end() ? entt::null : found->second;
    }

    UUID SceneDocument::id(entt::entity handle) const noexcept {
        if (!m_registry.valid(handle)) {
            return UUID::null;
        }
        const auto *identity = m_registry.try_get<IdentityComponent>(handle);
        return identity ? identity->id : UUID::null;
    }

    Result<UUID> SceneDocument::createEntity(EntityRecord record) {
        if (record.identity.id == UUID::null) {
            record.identity.id = UUID{};
        }

        const auto entityId = record.identity.id;
        const auto parent = record.hierarchy.parent;
        record.hierarchy.parent = UUID::null;
        record.hierarchy.children.clear();

        if (auto status = createEntityWithoutHierarchy(record); !status) {
            return fail(std::move(status.error()));
        }

        if (parent != UUID::null) {
            if (auto status = setParent(entityId, parent); !status) {
                auto mutation = destroyEntity(entityId);
                (void)mutation;
                return fail(std::move(status.error()));
            }
        }

        rebuildConnectionIndex();
        return entityId;
    }

    Status
    SceneDocument::createEntityWithoutHierarchy(const EntityRecord &record) {
        const auto id = record.identity.id;
        if (id == UUID::null) {
            return fail(Error::invalidArgument(
                "A scene entity must have a non-null UUID"));
        }
        if (contains(id)) {
            return fail(
                Error::alreadyExists("A scene entity with this UUID exists"));
        }

        const auto handle = m_registry.create();
        m_entities.emplace(id, handle);

        try {
            m_registry.emplace<IdentityComponent>(handle, record.identity);
            m_registry.emplace<TransformComponent>(handle, record.transform);
            m_registry.emplace<VisualStyleComponent>(handle, record.style);
            m_registry.emplace<HierarchyComponent>(handle,
                                                   HierarchyComponent{});
            m_registry.emplace<InteractionComponent>(handle,
                                                     record.interaction);

            if (auto status = applyRecordComponents(handle, record); !status) {
                releaseRuntimeId(handle);
                m_registry.destroy(handle);
                m_entities.erase(id);
                return status;
            }
            if (auto status = assignRuntimeId(handle); !status) {
                m_registry.destroy(handle);
                m_entities.erase(id);
                return status;
            }
        } catch (const std::exception &exception) {
            if (m_registry.valid(handle)) {
                releaseRuntimeId(handle);
                m_registry.destroy(handle);
            }
            m_entities.erase(id);
            return fail(
                Error::invalidState("Failed to construct scene entity: " +
                                    std::string(exception.what())));
        }

        if (record.selected && record.interaction.selectable) {
            m_registry.emplace_or_replace<SelectedTag>(handle);
        }
        if (record.focused && record.interaction.focusable) {
            clearFocus();
            m_registry.emplace_or_replace<FocusedTag>(handle);
            m_focusedEntity = id;
        }

        return {};
    }

    Status SceneDocument::applyRecordComponents(entt::entity handle,
                                                const EntityRecord &record) {
        applyOptional(m_registry, handle, record.simulation);
        applyOptional(m_registry, handle, record.port);
        applyOptional(m_registry, handle, record.connection);
        applyOptional(m_registry, handle, record.proxyPort);
        applyOptional(m_registry, handle, record.connectionJoint);
        applyOptional(m_registry, handle, record.module);
        applyOptional(m_registry, handle, record.image);
        applyOptional(m_registry, handle, record.text);
        applyOptional(m_registry, handle, record.probe);
        applyOptional(m_registry, handle, record.probeRuntime);
        applyOptional(m_registry, handle, record.monitor);
        applyOptional(m_registry, handle, record.extensions);

        if (record.input) {
            m_registry.emplace_or_replace<InputComponent>(handle);
        } else {
            m_registry.remove<InputComponent>(handle);
        }

        if (record.group) {
            m_registry.emplace_or_replace<GroupComponent>(handle);
        } else {
            m_registry.remove<GroupComponent>(handle);
        }

        return {};
    }

    Status SceneDocument::replaceRecordComponents(entt::entity handle,
                                                  const EntityRecord &record) {
        if (!m_registry.valid(handle)) {
            return fail(Error::notFound("Cannot replace a missing entity"));
        }

        const auto currentId = id(handle);
        if (currentId != record.identity.id) {
            return fail(Error::conflict(
                "Entity identity cannot be changed while replacing data"));
        }

        m_registry.replace<IdentityComponent>(handle, record.identity);
        m_registry.replace<TransformComponent>(handle, record.transform);
        m_registry.replace<VisualStyleComponent>(handle, record.style);
        m_registry.replace<HierarchyComponent>(handle, record.hierarchy);
        m_registry.replace<InteractionComponent>(handle, record.interaction);

        if (auto status = applyRecordComponents(handle, record); !status) {
            return status;
        }

        if (record.selected && record.interaction.selectable) {
            m_registry.emplace_or_replace<SelectedTag>(handle);
        } else {
            m_registry.remove<SelectedTag>(handle);
        }

        if (record.focused && record.interaction.focusable) {
            clearFocus();
            m_registry.emplace_or_replace<FocusedTag>(handle);
            m_focusedEntity = currentId;
        } else {
            m_registry.remove<FocusedTag>(handle);
            if (m_focusedEntity == currentId) {
                m_focusedEntity = UUID::null;
            }
        }

        return {};
    }

    Result<EntityRecord> SceneDocument::captureEntity(UUID entityId) const {
        const auto handle = entity(entityId);
        if (handle == entt::null) {
            return fail(Error::notFound("Scene entity does not exist"));
        }

        EntityRecord record;
        record.identity = m_registry.get<IdentityComponent>(handle);
        record.transform = m_registry.get<TransformComponent>(handle);
        record.style = m_registry.get<VisualStyleComponent>(handle);
        record.hierarchy = m_registry.get<HierarchyComponent>(handle);
        record.interaction = m_registry.get<InteractionComponent>(handle);

        captureOptional(m_registry, handle, record.simulation);
        captureOptional(m_registry, handle, record.port);
        captureOptional(m_registry, handle, record.connection);
        captureOptional(m_registry, handle, record.proxyPort);
        captureOptional(m_registry, handle, record.connectionJoint);
        captureOptional(m_registry, handle, record.module);
        captureOptional(m_registry, handle, record.image);
        captureOptional(m_registry, handle, record.text);
        captureOptional(m_registry, handle, record.probe);
        captureOptional(m_registry, handle, record.probeRuntime);
        captureOptional(m_registry, handle, record.monitor);
        captureOptional(m_registry, handle, record.extensions);

        record.input = m_registry.all_of<InputComponent>(handle);
        record.group = m_registry.all_of<GroupComponent>(handle);
        record.selected = m_registry.all_of<SelectedTag>(handle);
        record.focused = m_registry.all_of<FocusedTag>(handle);
        return record;
    }

    std::vector<UUID>
    SceneDocument::removalClosure(UUID root, bool includeDependants) const {
        std::vector<UUID> result;
        std::unordered_set<UUID> included;

        const auto includeHierarchy = [&](auto &&self, UUID entityId) -> void {
            if (!contains(entityId) || !included.insert(entityId).second) {
                return;
            }
            result.push_back(entityId);
            const auto *hierarchy = tryGet<HierarchyComponent>(entityId);
            if (!hierarchy) {
                return;
            }
            for (const auto child : hierarchy->children) {
                self(self, child);
            }
        };

        includeHierarchy(includeHierarchy, root);
        if (!includeDependants) {
            return result;
        }

        bool changed = true;
        while (changed) {
            changed = false;

            const auto connectionView =
                m_registry.view<IdentityComponent, ConnectionComponent>();
            for (const auto handle : connectionView) {
                const auto &[identity, connection] =
                    connectionView.get<IdentityComponent, ConnectionComponent>(
                        handle);
                if (included.contains(connection.startPort) ||
                    included.contains(connection.endPort)) {
                    const auto before = included.size();
                    includeHierarchy(includeHierarchy, identity.id);
                    for (const auto joint : connection.associatedJoints) {
                        includeHierarchy(includeHierarchy, joint);
                    }
                    changed = changed || included.size() != before;
                }
            }

            const auto jointView =
                m_registry.view<IdentityComponent, ConnectionJointComponent>();
            for (const auto handle : jointView) {
                const auto &[identity, joint] =
                    jointView.get<IdentityComponent, ConnectionJointComponent>(
                        handle);
                if (included.contains(joint.connection)) {
                    const auto before = included.size();
                    includeHierarchy(includeHierarchy, identity.id);
                    changed = changed || included.size() != before;
                }
            }

            const auto proxyView =
                m_registry.view<IdentityComponent, ProxyPortComponent>();
            for (const auto handle : proxyView) {
                const auto &[identity, proxy] =
                    proxyView.get<IdentityComponent, ProxyPortComponent>(
                        handle);
                if (included.contains(proxy.inputPort) ||
                    included.contains(proxy.outputPort)) {
                    const auto before = included.size();
                    includeHierarchy(includeHierarchy, identity.id);
                    changed = changed || included.size() != before;
                }
            }

            const auto probeView =
                m_registry.view<IdentityComponent, ProbeComponent>();
            for (const auto handle : probeView) {
                const auto &[identity, probe] =
                    probeView.get<IdentityComponent, ProbeComponent>(handle);
                if (included.contains(probe.port)) {
                    const auto before = included.size();
                    includeHierarchy(includeHierarchy, identity.id);
                    changed = changed || included.size() != before;
                }
            }
        }

        return result;
    }

    Result<std::vector<EntityRecord>>
    SceneDocument::captureSubtree(UUID root, bool includeDependants) const {
        if (!contains(root)) {
            return fail(Error::notFound("Scene entity does not exist"));
        }

        std::vector<EntityRecord> records;
        const auto closure = removalClosure(root, includeDependants);
        records.reserve(closure.size());
        for (const auto entityId : closure) {
            auto record = captureEntity(entityId);
            if (!record) {
                return fail(std::move(record.error()));
            }
            records.push_back(std::move(*record));
        }
        return records;
    }

    Result<SceneMutation> SceneDocument::destroyEntity(UUID entityId) {
        if (!contains(entityId)) {
            return fail(Error::notFound("Scene entity does not exist"));
        }

        SceneMutation mutation;
        const auto closure = removalClosure(entityId, true);
        std::unordered_set<UUID> removedIds(closure.begin(), closure.end());
        mutation.removed.reserve(closure.size());

        std::unordered_set<UUID> modifiedIds;
        const auto captureModified = [&](UUID id) -> Status {
            if (removedIds.contains(id) || !modifiedIds.insert(id).second) {
                return {};
            }
            auto record = captureEntity(id);
            if (!record) {
                return fail(std::move(record.error()));
            }
            mutation.modifiedBefore.push_back(std::move(*record));
            return {};
        };

        for (const auto removedId : closure) {
            auto record = captureEntity(removedId);
            if (!record) {
                return fail(std::move(record.error()));
            }
            mutation.removed.push_back(std::move(*record));

            const auto *hierarchy = tryGet<HierarchyComponent>(removedId);
            if (hierarchy && hierarchy->parent != UUID::null &&
                !removedIds.contains(hierarchy->parent)) {
                if (auto status = captureModified(hierarchy->parent); !status) {
                    return fail(std::move(status.error()));
                }
            }
        }

        const auto monitorView =
            m_registry.view<IdentityComponent, MonitorComponent>();
        for (const auto handle : monitorView) {
            const auto &[identity, monitor] =
                monitorView.get<IdentityComponent, MonitorComponent>(handle);
            const bool affected =
                std::any_of(monitor.ports.begin(),
                            monitor.ports.end(),
                            [&removedIds](UUID id) {
                                return removedIds.contains(id);
                            }) ||
                std::any_of(
                    monitor.hiddenPorts.begin(),
                    monitor.hiddenPorts.end(),
                    [&removedIds](UUID id) { return removedIds.contains(id); });
            if (affected) {
                if (auto status = captureModified(identity.id); !status) {
                    return fail(std::move(status.error()));
                }
            }
        }

        for (const auto &record : mutation.modifiedBefore) {
            const auto handle = entity(record.identity.id);
            if (handle == entt::null) {
                continue;
            }

            if (auto *hierarchy =
                    m_registry.try_get<HierarchyComponent>(handle)) {
                hierarchy->children.erase(
                    std::remove_if(hierarchy->children.begin(),
                                   hierarchy->children.end(),
                                   [&removedIds](UUID child) {
                                       return removedIds.contains(child);
                                   }),
                    hierarchy->children.end());
            }

            if (auto *monitor = m_registry.try_get<MonitorComponent>(handle)) {
                monitor->ports.erase(
                    std::remove_if(monitor->ports.begin(),
                                   monitor->ports.end(),
                                   [&removedIds](UUID port) {
                                       return removedIds.contains(port);
                                   }),
                    monitor->ports.end());
                monitor->hiddenPorts.erase(
                    std::remove_if(monitor->hiddenPorts.begin(),
                                   monitor->hiddenPorts.end(),
                                   [&removedIds](UUID port) {
                                       return removedIds.contains(port);
                                   }),
                    monitor->hiddenPorts.end());
            }
        }

        for (auto iterator = closure.rbegin(); iterator != closure.rend();
             ++iterator) {
            const auto handle = entity(*iterator);
            if (handle == entt::null) {
                continue;
            }
            releaseRuntimeId(handle);
            if (m_focusedEntity == *iterator) {
                m_focusedEntity = UUID::null;
            }
            m_registry.destroy(handle);
            m_entities.erase(*iterator);
        }

        rebuildConnectionIndex();
        return mutation;
    }

    Status SceneDocument::restoreMutation(const SceneMutation &mutation) {
        for (const auto &record : mutation.removed) {
            if (contains(record.identity.id)) {
                return fail(Error::conflict(
                    "Cannot restore over an existing scene entity"));
            }
        }

        std::vector<UUID> created;
        created.reserve(mutation.removed.size());
        for (const auto &record : mutation.removed) {
            auto recordWithoutHierarchy = record;
            recordWithoutHierarchy.hierarchy = {};
            if (auto status =
                    createEntityWithoutHierarchy(recordWithoutHierarchy);
                !status) {
                for (const auto createdId : created) {
                    const auto handle = entity(createdId);
                    if (handle != entt::null) {
                        releaseRuntimeId(handle);
                        m_registry.destroy(handle);
                        m_entities.erase(createdId);
                    }
                }
                return status;
            }
            created.push_back(record.identity.id);
        }

        for (const auto &record : mutation.removed) {
            const auto handle = entity(record.identity.id);
            m_registry.replace<HierarchyComponent>(handle, record.hierarchy);
        }

        for (const auto &record : mutation.modifiedBefore) {
            const auto handle = entity(record.identity.id);
            if (handle == entt::null) {
                return fail(Error::conflict(
                    "A related entity disappeared before restoration"));
            }
            if (auto status = replaceRecordComponents(handle, record);
                !status) {
                return status;
            }
        }

        rebuildConnectionIndex();
        rebuildRuntimeDerivedState();
        return validate();
    }

    Status SceneDocument::setParent(UUID child, UUID parent) {
        if (child == parent || child == UUID::null) {
            return fail(
                Error::invalidArgument("An entity cannot parent itself"));
        }
        const auto childHandle = entity(child);
        const auto parentHandle = entity(parent);
        if (childHandle == entt::null || parentHandle == entt::null) {
            return fail(
                Error::notFound("Parent or child entity does not exist"));
        }
        if (wouldCreateHierarchyCycle(child, parent)) {
            return fail(Error::conflict(
                "The requested parent would create a hierarchy cycle"));
        }

        auto &childHierarchy = m_registry.get<HierarchyComponent>(childHandle);
        if (childHierarchy.parent == parent) {
            return {};
        }

        if (childHierarchy.parent != UUID::null) {
            if (auto *previous =
                    tryGet<HierarchyComponent>(childHierarchy.parent)) {
                eraseId(previous->children, child);
            }
        }

        auto &parentHierarchy =
            m_registry.get<HierarchyComponent>(parentHandle);
        if (!containsId(parentHierarchy.children, child)) {
            parentHierarchy.children.push_back(child);
        }
        childHierarchy.parent = parent;
        return {};
    }

    Status SceneDocument::detach(UUID child) {
        const auto childHandle = entity(child);
        if (childHandle == entt::null) {
            return fail(Error::notFound("Child entity does not exist"));
        }

        auto &hierarchy = m_registry.get<HierarchyComponent>(childHandle);
        if (hierarchy.parent == UUID::null) {
            return {};
        }
        if (auto *parent = tryGet<HierarchyComponent>(hierarchy.parent)) {
            eraseId(parent->children, child);
        }
        hierarchy.parent = UUID::null;
        return {};
    }

    bool SceneDocument::wouldCreateHierarchyCycle(UUID child,
                                                  UUID parent) const {
        auto cursor = parent;
        while (cursor != UUID::null) {
            if (cursor == child) {
                return true;
            }
            const auto *hierarchy = tryGet<HierarchyComponent>(cursor);
            if (!hierarchy) {
                return false;
            }
            cursor = hierarchy->parent;
        }
        return false;
    }

    std::vector<UUID> SceneDocument::roots() const {
        std::vector<UUID> result;
        result.reserve(m_entities.size());
        const auto view =
            m_registry.view<IdentityComponent, HierarchyComponent>();
        for (const auto handle : view) {
            const auto &[identity, hierarchy] =
                view.get<IdentityComponent, HierarchyComponent>(handle);
            if (hierarchy.parent == UUID::null) {
                result.push_back(identity.id);
            }
        }
        return result;
    }

    Status SceneDocument::select(UUID entityId, bool additive) {
        const auto handle = entity(entityId);
        if (handle == entt::null) {
            return fail(Error::notFound("Scene entity does not exist"));
        }
        const auto &interaction = m_registry.get<InteractionComponent>(handle);
        if (!interaction.selectable) {
            return fail(Error::conflict("Scene entity is not selectable"));
        }
        if (!additive) {
            clearSelection();
        }
        m_registry.emplace_or_replace<SelectedTag>(handle);
        return {};
    }

    Status SceneDocument::deselect(UUID entityId) {
        const auto handle = entity(entityId);
        if (handle == entt::null) {
            return fail(Error::notFound("Scene entity does not exist"));
        }
        m_registry.remove<SelectedTag>(handle);
        return {};
    }

    void SceneDocument::clearSelection() {
        std::vector<entt::entity> selected;
        const auto view = m_registry.view<SelectedTag>();
        selected.assign(view.begin(), view.end());
        for (const auto handle : selected) {
            m_registry.remove<SelectedTag>(handle);
        }
    }

    std::vector<UUID> SceneDocument::selection() const {
        std::vector<UUID> result;
        const auto view = m_registry.view<IdentityComponent, SelectedTag>();
        result.reserve(view.size_hint());
        for (const auto handle : view) {
            result.push_back(view.get<IdentityComponent>(handle).id);
        }
        return result;
    }

    Status SceneDocument::focus(UUID entityId) {
        const auto handle = entity(entityId);
        if (handle == entt::null) {
            return fail(Error::notFound("Scene entity does not exist"));
        }
        const auto &interaction = m_registry.get<InteractionComponent>(handle);
        if (!interaction.focusable) {
            return fail(Error::conflict("Scene entity is not focusable"));
        }

        clearFocus();
        m_registry.emplace_or_replace<FocusedTag>(handle);
        m_focusedEntity = entityId;
        return {};
    }

    void SceneDocument::clearFocus() {
        if (m_focusedEntity == UUID::null) {
            return;
        }
        const auto handle = entity(m_focusedEntity);
        if (handle != entt::null) {
            m_registry.remove<FocusedTag>(handle);
        }
        m_focusedEntity = UUID::null;
    }

    UUID SceneDocument::focusedEntity() const noexcept {
        return m_focusedEntity;
    }

    Status SceneDocument::assignRuntimeId(entt::entity handle) {
        if (!m_registry.valid(handle)) {
            return fail(Error::notFound(
                "Cannot assign a picking ID to a missing entity"));
        }

        uint32_t runtimeId = PickingId::invalidRuntimeId;
        if (!m_freeRuntimeIds.empty()) {
            runtimeId = m_freeRuntimeIds.back();
            m_freeRuntimeIds.pop_back();
        } else {
            if (m_nextRuntimeId == PickingId::invalidRuntimeId) {
                return fail(Error::invalidState(
                    "The scene exhausted its runtime picking IDs"));
            }
            runtimeId = m_nextRuntimeId++;
        }

        const auto entityId = id(handle);
        m_registry.emplace_or_replace<PickingComponent>(
            handle, PickingComponent{runtimeId});
        m_runtimeIds[runtimeId] = entityId;
        return {};
    }

    void SceneDocument::releaseRuntimeId(entt::entity handle) {
        const auto *picking = m_registry.try_get<PickingComponent>(handle);
        if (!picking || picking->runtimeId == PickingId::invalidRuntimeId) {
            return;
        }
        m_runtimeIds.erase(picking->runtimeId);
        m_freeRuntimeIds.push_back(picking->runtimeId);
    }

    Result<PickingId> SceneDocument::pickingId(UUID entityId,
                                               uint32_t info) const {
        const auto *picking = tryGet<PickingComponent>(entityId);
        if (!picking) {
            return fail(Error::notFound(
                "Scene entity does not have a runtime picking ID"));
        }
        return PickingId{picking->runtimeId, info};
    }

    UUID SceneDocument::entityFromPickingId(PickingId picking) const noexcept {
        if (!picking.isValid()) {
            return UUID::null;
        }
        const auto found = m_runtimeIds.find(picking.runtimeId);
        return found == m_runtimeIds.end() ? UUID::null : found->second;
    }

    void SceneDocument::rebuildConnectionIndex() {
        m_connectionsByPort.clear();
        const auto view =
            m_registry.view<IdentityComponent, ConnectionComponent>();
        for (const auto handle : view) {
            const auto &[identity, connection] =
                view.get<IdentityComponent, ConnectionComponent>(handle);
            m_connectionsByPort[connection.startPort].push_back(identity.id);
            m_connectionsByPort[connection.endPort].push_back(identity.id);
        }
    }

    void SceneDocument::rebuildRuntimeDerivedState() {
        UUID focused = UUID::null;
        const auto view = m_registry.view<IdentityComponent, FocusedTag>();
        for (const auto handle : view) {
            if (focused == UUID::null) {
                focused = view.get<IdentityComponent>(handle).id;
            } else {
                m_registry.remove<FocusedTag>(handle);
            }
        }
        m_focusedEntity = focused;
    }

    std::vector<UUID> SceneDocument::connectionsForPort(UUID port) const {
        const auto found = m_connectionsByPort.find(port);
        return found == m_connectionsByPort.end() ? std::vector<UUID>{}
                                                  : found->second;
    }

    Status SceneDocument::validate() const {
        if (m_metadata.id == UUID::null) {
            return fail(Error::invalidState("Scene UUID is null"));
        }
        if (m_entities.size() !=
            m_registry.storage<entt::entity>()->free_list()) {
            return fail(Error::invalidState(
                "Scene UUID index and EnTT registry are out of sync"));
        }

        std::size_t focusedCount = 0;
        for (const auto &[entityId, handle] : m_entities) {
            if (!m_registry.valid(handle)) {
                return fail(Error::invalidState(
                    "Scene UUID index refers to a destroyed entity"));
            }
            const auto *identity =
                m_registry.try_get<IdentityComponent>(handle);
            const auto *hierarchy =
                m_registry.try_get<HierarchyComponent>(handle);
            if (!identity || !hierarchy || identity->id != entityId) {
                return fail(Error::invalidState(
                    "Scene entity is missing required model components"));
            }

            if (hierarchy->parent != UUID::null) {
                const auto *parent =
                    tryGet<HierarchyComponent>(hierarchy->parent);
                if (!parent || !containsId(parent->children, entityId)) {
                    return fail(Error::invalidState(
                        "Scene hierarchy parent/child relation is asymmetric"));
                }
            }
            for (const auto child : hierarchy->children) {
                const auto *childHierarchy = tryGet<HierarchyComponent>(child);
                if (!childHierarchy || childHierarchy->parent != entityId) {
                    return fail(Error::invalidState(
                        "Scene hierarchy child/parent relation is asymmetric"));
                }
            }

            if (m_registry.all_of<FocusedTag>(handle)) {
                ++focusedCount;
                if (m_focusedEntity != entityId) {
                    return fail(Error::invalidState(
                        "Focused entity index is out of sync"));
                }
            }

            if (const auto *connection =
                    m_registry.try_get<ConnectionComponent>(handle)) {
                if (!contains(connection->startPort) ||
                    !contains(connection->endPort)) {
                    return fail(Error::invalidState(
                        "Connection refers to a missing endpoint"));
                }
            }
        }

        if (focusedCount > 1 ||
            (focusedCount == 0 && m_focusedEntity != UUID::null)) {
            return fail(
                Error::invalidState("Scene has an invalid focus state"));
        }
        return {};
    }
} // namespace Bess::Session
