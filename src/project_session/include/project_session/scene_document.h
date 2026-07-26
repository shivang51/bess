#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "project_session/components.h"
#include "project_session/result.h"

#include <entt/entity/registry.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::Session {
    struct BESS_API SceneMetadata {
        UUID id = UUID::null;
        std::string name = "Scene";
        UUID parentScene = UUID::null;
        UUID module = UUID::null;
        bool isRoot = false;
    };

    struct BESS_API EntityRecord {
        IdentityComponent identity;
        TransformComponent transform;
        VisualStyleComponent style;
        HierarchyComponent hierarchy;
        InteractionComponent interaction;

        std::optional<SimulationComponent> simulation;
        bool input = false;
        bool group = false;
        std::optional<PortComponent> port;
        std::optional<ConnectionComponent> connection;
        std::optional<ProxyPortComponent> proxyPort;
        std::optional<ConnectionJointComponent> connectionJoint;
        std::optional<ModuleComponent> module;
        std::optional<ImageComponent> image;
        std::optional<TextComponent> text;
        std::optional<ProbeComponent> probe;
        std::optional<ProbeRuntimeComponent> probeRuntime;
        std::optional<MonitorComponent> monitor;
        std::optional<ExtensionComponents> extensions;

        bool selected = false;
        bool focused = false;
    };

    struct BESS_API SceneMutation {
        std::vector<EntityRecord> removed;
        std::vector<EntityRecord> modifiedBefore;

        bool empty() const noexcept {
            return removed.empty() && modifiedBefore.empty();
        }
    };

    class BESS_API SceneDocument {
      public:
        explicit SceneDocument(SceneMetadata metadata = {});
        ~SceneDocument() = default;

        SceneDocument(const SceneDocument &) = delete;
        SceneDocument &operator=(const SceneDocument &) = delete;
        SceneDocument(SceneDocument &&) noexcept = default;
        SceneDocument &operator=(SceneDocument &&) noexcept = default;

        const SceneMetadata &metadata() const noexcept;
        SceneMetadata &metadata() noexcept;

        const entt::registry &registry() const noexcept;

        bool contains(UUID id) const noexcept;
        std::size_t size() const noexcept;
        entt::entity entity(UUID id) const noexcept;
        UUID id(entt::entity entity) const noexcept;

        Result<UUID> createEntity(EntityRecord record);
        Result<SceneMutation> destroyEntity(UUID id);
        Status restoreMutation(const SceneMutation &mutation);

        Result<EntityRecord> captureEntity(UUID id) const;
        Result<std::vector<EntityRecord>>
        captureSubtree(UUID root, bool includeDependants = true) const;

        Status setParent(UUID child, UUID parent);
        Status detach(UUID child);
        std::vector<UUID> roots() const;

        Status select(UUID id, bool additive = false);
        Status deselect(UUID id);
        void clearSelection();
        std::vector<UUID> selection() const;

        Status focus(UUID id);
        void clearFocus();
        UUID focusedEntity() const noexcept;

        Result<PickingId> pickingId(UUID id, uint32_t info = 0) const;
        UUID entityFromPickingId(PickingId id) const noexcept;

        std::vector<UUID> connectionsForPort(UUID port) const;

        template <typename T> const T *tryGet(UUID id) const noexcept {
            const auto handle = entity(id);
            return handle == entt::null ? nullptr
                                        : m_registry.try_get<T>(handle);
        }

        template <typename T> T *tryGet(UUID id) noexcept {
            const auto handle = entity(id);
            return handle == entt::null ? nullptr
                                        : m_registry.try_get<T>(handle);
        }

        template <typename T, typename... Args>
        Result<T *> emplace(UUID id, Args &&...args) {
            const auto handle = entity(id);
            if (handle == entt::null) {
                return fail(Error::notFound("Entity does not exist"));
            }
            if (m_registry.all_of<T>(handle)) {
                return fail(Error::alreadyExists(
                    "Entity already contains the requested component"));
            }
            return &m_registry.emplace<T>(handle, std::forward<Args>(args)...);
        }

        template <typename T> Status remove(UUID id) {
            const auto handle = entity(id);
            if (handle == entt::null) {
                return fail(Error::notFound("Entity does not exist"));
            }
            if (!m_registry.all_of<T>(handle)) {
                return fail(Error::notFound(
                    "Entity does not contain the requested component"));
            }
            m_registry.remove<T>(handle);
            return {};
        }

        Status validate() const;

      private:
        Status createEntityWithoutHierarchy(const EntityRecord &record);
        Status applyRecordComponents(entt::entity handle,
                                     const EntityRecord &record);
        Status replaceRecordComponents(entt::entity handle,
                                       const EntityRecord &record);
        Status assignRuntimeId(entt::entity handle);
        void releaseRuntimeId(entt::entity handle);
        bool wouldCreateHierarchyCycle(UUID child, UUID parent) const;
        std::vector<UUID> removalClosure(UUID root,
                                         bool includeDependants) const;
        void rebuildConnectionIndex();
        void rebuildRuntimeDerivedState();

      private:
        SceneMetadata m_metadata;
        entt::registry m_registry;
        std::unordered_map<UUID, entt::entity> m_entities;
        std::unordered_map<uint32_t, UUID> m_runtimeIds;
        std::vector<uint32_t> m_freeRuntimeIds;
        std::unordered_map<UUID, std::vector<UUID>> m_connectionsByPort;
        uint32_t m_nextRuntimeId = 1;
        UUID m_focusedEntity = UUID::null;
    };
} // namespace Bess::Session
