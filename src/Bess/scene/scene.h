#pragma once

#include "application/types.h"
#include "bess_uuid.h"
#include "commands/commands_manager.h"
#include "component_definition.h"
#include "entt/entity/fwd.hpp"
#include "entt/entt.hpp"
#include "events/application_event.h"
#include "scene/camera.h"
#include "scene/components/components.h"
#include "scene/components/non_sim_comp.h"
#include "scene/viewport.h"
#include <chrono>
#include <memory>
#include <vulkan/vulkan_core.h>

// Forward declaration
namespace Bess::Canvas {
    class ArtistManager;
}

namespace Bess::Canvas {

    enum class SceneMode {
        general,
        move
    };

    enum class SceneDrawMode {
        none,
        connection,
        selectionBox
    };

    struct LastCreatedComponent {
        SimEngine::ComponentDefinition componentDefinition;
        Components::NSComponent nsComponent;
        bool set = false;
    };

    class Scene {
      public:
        Scene();
        ~Scene();

      public:
        static std::shared_ptr<Scene> instance();

        std::shared_ptr<Viewport> getViewport() {
            return m_viewport;
        }

        void destroy();

        void reset();
        void clear();
        void render();
        void renderWithViewport(const std::shared_ptr<Viewport> &viewport);
        void update(TFrameTime ts, const std::vector<ApplicationEvent> &events);

        uint64_t getTextureId() const;
        std::shared_ptr<Camera> getCamera();

        void drawSceneToViewport(const std::shared_ptr<Viewport> &viewport);

        void saveScenePNG(const std::string &path) const;

      public:
        void updateNetsFromSimEngine();

        bool isEntityHovered(const entt::entity &ent) const;

        const glm::vec2 &getMousePos() const;
        glm::vec2 getSceneMousePos();
        const glm::vec2 &getCameraPos() const;
        float getCameraZoom() const;
        void setZoom(float value) const;

        void setSceneMode(SceneMode mode);
        SceneMode getSceneMode() const;

        void resize(const glm::vec2 &size);
        entt::registry &getEnttRegistry();
        const glm::vec2 &getSize() const;

        UUID createSlotEntity(Components::SlotType type, const UUID &parent, unsigned int idx);
        UUID createSlotEntity(UUID uuid, Components::SlotType type, const UUID &parent, unsigned int idx);

        UUID createSimEntity(const UUID &simEngineEntt, const SimEngine::ComponentDefinition &comp, const glm::vec2 &pos);
        UUID createSimEntity(const UUID &simEngineEntt, const SimEngine::ComponentDefinition &comp,
                             const glm::vec2 &pos, UUID uuid, const std::vector<UUID> &inputSlotIds, const std::vector<UUID> &outputSlotIds);
        UUID createNonSimEntity(const Canvas::Components::NSComponent &comp, const glm::vec2 &pos);

        void deleteSceneEntity(const UUID &entUuid);

        /// deletes entity from sim engine as well
        void deleteEntity(const UUID &entUuid);
        void deleteConnection(const UUID &entUuid);
        void deleteConnectionFromScene(const UUID &entUuid);
        entt::entity getEntityWithUuid(const UUID &uuid);
        bool isEntityValid(const UUID &uuid);
        void setZCoord(float val);

        SimEngine::Commands::CommandsManager &getCmdManager();
        UUID generateBasicConnection(entt::entity inputSlot, entt::entity outputSlot);
        UUID connectSlots(UUID startSlot, UUID endSlot);
        UUID connectComponents(UUID compIdA, int slotIdxA, bool isAInput, UUID compIdB, int slotIdxB);

        bool *getIsSchematicViewPtr();
        void toggleSchematicView();

        std::shared_ptr<ArtistManager> getArtistManager();

      private:
        /// to draw testing stuff
        void drawScratchContent(TFrameTime ts, const std::shared_ptr<Viewport> &viewport);

        const UUID &getUuidOfEntity(entt::entity ent) const;

        // gets entity from scene that has reference to passed simulation engine uuid
        entt::entity getSceneEntityFromSimUuid(const UUID &uuid) const;

        void setLastCreatedComp(LastCreatedComponent comp);

        void removeConnectionEntt(const entt::entity ent);
        void updateHoveredId();

        void onMouseMove(const glm::vec2 &pos);
        void onLeftMouse(bool isPressed);
        void onMiddleMouse(bool isPressed);
        void onRightMouse(bool isPressed);
        void onMouseWheel(double x, double y);

        std::shared_ptr<Viewport> m_viewport;

        glm::vec2 toScenePos(const glm::vec2 &mousePos) const;
        glm::vec2 getViewportMousePos(const glm::vec2 &mousePos) const;
        bool isCursorInViewport(const glm::vec2 &pos) const;
        void drawConnection();
        void drawSelectionBox();
        void handleKeyboardShortcuts();
        void copySelectedComponents();
        void generateCopiedComponents();
        bool selectEntitesInArea();
        void toggleSelectComponent(const UUID &uuid);
        void toggleSelectComponent(entt::entity ent);
        void selectAllEntities();

        void moveConnection(entt::entity ent, glm::vec2 &dPos);
        void dragConnectionSegment(entt::entity ent);

        float getNextZCoord();

        glm::vec2 getSnappedPos(const glm::vec2 &pos) const;

      private:
        // Vulkan framebuffers for scene rendering
        glm::vec2 m_size, m_mousePos, m_dMousePos;
        std::shared_ptr<Camera> m_camera;

        bool m_isLeftMousePressed = false, m_isMiddleMousePressed = false;

      private:
        entt::registry m_registry;
        UUID m_hoveredEntity;
        UUID m_connectionStartEntity;
        // selection box
        glm::vec2 m_selectionBoxStart;
        glm::vec2 m_selectionBoxEnd;
        bool m_selectInSelectionBox = false;
        bool m_getIdsInSelBox = false;

        bool m_isSchematicView = false;

        SceneDrawMode m_drawMode = SceneDrawMode::none;
        SceneMode m_sceneMode = SceneMode::general;

        const float m_zIncrement = 0.001;
        const int snapSize = 2;
        float m_compZCoord = m_zIncrement;

        bool m_isDragging = false;
        std::unordered_map<entt::entity, glm::vec2> m_dragOffsets = {};
        std::unordered_map<UUID, Components::TransformComponent> m_dragStartTransforms = {};
        std::unordered_map<UUID, Components::ConnectionSegmentComponent> m_dragStartConnSeg = {};

        std::unordered_map<UUID, entt::entity> m_uuidToEntt = {};

        TFrameTime m_frameTimeStep = {};

        LastCreatedComponent m_lastCreatedComp = {};
        struct CopiedComponent {
            SimEngine::ComponentDefinition def;
            Components::NSComponent nsComp;
            int inputCount, outputCount;
        };

        std::vector<CopiedComponent> m_copiedComponents = {};

        SimEngine::Commands::CommandsManager m_cmdManager;

        VkExtent2D vec2Extent2D(const glm::vec2 &vec);

        bool m_isDestroyed = false;
    };
} // namespace Bess::Canvas
