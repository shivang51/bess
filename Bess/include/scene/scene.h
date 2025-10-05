#pragma once

#include "bess_uuid.h"
#include "camera.h"
#include "commands/commands_manager.h"
#include "component_definition.h"
#include "entt/entity/fwd.hpp"
#include "entt/entt.hpp"
#include "events/application_event.h"
#include "modules/schematic_gen/schematic_view.h"
#include "scene/components/components.h"
#include "scene/components/non_sim_comp.h"
#include "types.h"
#include <chrono>
#include <memory>

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
        std::shared_ptr<const SimEngine::ComponentDefinition> componentDefinition = nullptr;
        int inputCount = -1;
        int outputCount = -1;
    };

    class Scene {
      public:
        Scene();
        ~Scene();

      public:
        static std::shared_ptr<Scene> instance();

        void reset();
        void clear();
        void render();
        void renderWithCamera(std::shared_ptr<Camera> camera);
        void update(TFrameTime ts, const std::vector<ApplicationEvent> &events);

        uint64_t getTextureId() const;
        std::shared_ptr<Camera> getCamera();

        void drawScene(std::shared_ptr<Camera> camera);
        void beginScene() const;
        void endScene() const;

        void setLastCreatedComp(LastCreatedComponent comp);

        void saveScenePNG(const std::string &path) const;

        friend class Modules::SchematicGen::SchematicView;

      public:
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

        UUID createSimEntity(const UUID &simEngineEntt, std::shared_ptr<const SimEngine::ComponentDefinition> comp, const glm::vec2 &pos);
        UUID createSimEntity(const UUID &simEngineEntt, std::shared_ptr<const SimEngine::ComponentDefinition> comp,
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
        UUID generateBasicConnection(entt::entity startSlot, entt::entity endSlot);
        UUID connectSlots(UUID startSlot, UUID endSlot);
        UUID connectComponents(UUID compIdA, int slotIdxA, bool isAInput, UUID compIdB, int slotIdxB);

        bool *getIsSchematicViewPtr();
        void toggleSchematicView();

        std::shared_ptr<ArtistManager> getArtistManager();

      private:
        const UUID &getUuidOfEntity(entt::entity ent);

        // gets entity from scene that has reference to passed simulation engine uuid
        entt::entity getSceneEntityFromSimUuid(const UUID &uuid) const;

        void removeConnectionEntt(const entt::entity ent);
        void updateHoveredId();

        void onMouseMove(const glm::vec2 &pos);
        void onLeftMouse(bool isPressed);
        void onMiddleMouse(bool isPressed);
        void onRightMouse(bool isPressed);
        void onMouseWheel(double x, double y);

        glm::vec2 toScenePos(const glm::vec2 &mousePos) const;
        glm::vec2 getViewportMousePos(const glm::vec2 &mousePos) const;
        bool isCursorInViewport(const glm::vec2 &pos) const;
        void drawConnection();
        void drawSelectionBox();
        void handleKeyboardShortcuts();
        void copySelectedComponents();
        void generateCopiedComponents();
        void selectEntitesInArea(const glm::vec2 &start, const glm::vec2 &end);
        void toggleSelectComponent(const UUID &uuid);
        void toggleSelectComponent(entt::entity ent);
        void selectAllEntities();

        void moveConnection(entt::entity ent, glm::vec2 &dPos);
        void dragConnectionSegment(entt::entity ent);

        float getNextZCoord();

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

        LastCreatedComponent m_lastCreatedComp = {};
        struct CopiedComponent {
            std::shared_ptr<const SimEngine::ComponentDefinition> def = nullptr;
            Components::NSComponent nsComp;
            int inputCount, outputCount;

            bool isSimComp() {
                return def != nullptr;
            }
        };
        std::vector<CopiedComponent> m_copiedComponents = {};

        // OpenGL textures removed for Vulkan migration
        // TODO: Implement Vulkan textures

        SimEngine::Commands::CommandsManager m_cmdManager;

        std::shared_ptr<ArtistManager> m_artistManager;
    };
} // namespace Bess::Canvas
