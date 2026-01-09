#pragma once

#include "application/types.h"
#include "bess_uuid.h"
#include "commands/commands_manager.h"
#include "component_definition.h"
#include "events/application_event.h"
#include "events/scene_events.h"
#include "events/sim_engine_events.h"
#include "scene/camera.h"
#include "scene/components/non_sim_comp.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/scene_state.h"
#include "scene/viewport.h"
#include <memory>
#include <vulkan/vulkan_core.h>

// Forward declaration
namespace Bess::Canvas {
    class ArtistManager;
}

namespace Bess::Canvas {

    enum class SceneMode : uint8_t {
        general,
        move
    };

    enum class SceneDrawMode : uint8_t {
        none,
        connection,
        selectionBox
    };

    struct LastCreatedComponent {
        std::shared_ptr<SimEngine::ComponentDefinition> componentDefinition;
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

        const SceneState &getState() const;
        SceneState &getState();

      public:
        void updateNetsFromSimEngine();

        PickingId getHoveredEntity() const {
            return m_pickingId;
        }

        const glm::vec2 &getMousePos() const;
        glm::vec2 getSceneMousePos();
        const glm::vec2 &getCameraPos() const;
        float getCameraZoom() const;
        void setZoom(float value) const;

        void setSceneMode(SceneMode mode);
        SceneMode getSceneMode() const;

        void resize(const glm::vec2 &size);
        const glm::vec2 &getSize() const;

        UUID createSimEntity(const UUID &simEngineId,
                             const std::shared_ptr<SimEngine::ComponentDefinition> &def,
                             const glm::vec2 &pos);
        UUID createNonSimEntity(const Canvas::Components::NSComponent &comp,
                                const glm::vec2 &pos);

        void deleteSceneEntity(const UUID &entUuid);
        void deleteSelectedSceneEntities();

        /// deletes entity from sim engine as well
        void deleteEntity(const UUID &entUuid);

        void setZCoord(float val);

        SimEngine::Commands::CommandsManager &getCmdManager();

        bool *getIsSchematicViewPtr();
        void toggleSchematicView();

        bool isHoveredEntityValid();

      private:
        /// to draw testing stuff
        void drawScratchContent(TFrameTime ts, const std::shared_ptr<Viewport> &viewport);

        void updatePickingId();

        void setPickingId(const PickingId &pickingId);

        void onMouseMove(const glm::vec2 &pos);
        void onLeftMouse(bool isPressed);
        void onMiddleMouse(bool isPressed);
        void onRightMouse(bool isPressed);
        void onMouseWheel(double x, double y);

        std::shared_ptr<Viewport> m_viewport;

        glm::vec2 toScenePos(const glm::vec2 &mousePos) const;
        glm::vec2 getViewportMousePos(const glm::vec2 &mousePos) const;
        bool isCursorInViewport(const glm::vec2 &pos) const;
        void drawSelectionBox();
        void handleKeyboardShortcuts();
        void copySelectedComponents();
        void generateCopiedComponents();
        bool selectEntitesInArea();
        void selectAllEntities();

        float getNextZCoord();

        glm::vec2 getSnappedPos(const glm::vec2 &pos) const;

      private:
        // Vulkan framebuffers for scene rendering
        glm::vec2 m_size, m_mousePos, m_dMousePos;
        std::shared_ptr<Camera> m_camera;

        bool m_isLeftMousePressed = false, m_isMiddleMousePressed = false;

        void onSlotClicked(const Events::SlotClickedEvent &e);

        void onCompDefOutputsResized(const SimEngine::Events::CompDefOutputsResizedEvent &e);

        void drawGhostConnection(const std::shared_ptr<PathRenderer> &pathRenderer,
                                 const glm::vec2 &startPos,
                                 const glm::vec2 &endPos);

      private:
        SceneState m_state;

        PickingId m_pickingId = PickingId::invalid();
        PickingId m_prevPickingId = PickingId::invalid();
        UUID m_connectionStartSlot = UUID::null;

        // selection box
        glm::vec2 m_selectionBoxStart;
        glm::vec2 m_selectionBoxEnd;
        bool m_selectInSelectionBox = false;
        bool m_getIdsInSelBox = false;

        bool m_isDragging = false;
        SceneDrawMode m_drawMode = SceneDrawMode::none;
        SceneMode m_sceneMode = SceneMode::general;

        const float m_zIncrement = 0.001f;
        const int snapSize = 2;
        float m_compZCoord = m_zIncrement;

        TFrameTime m_frameTimeStep = {};

        LastCreatedComponent m_lastCreatedComp = {};
        struct CopiedComponent {
            std::shared_ptr<SimEngine::ComponentDefinition> def;
            Components::NSComponent nsComp;
            int inputCount, outputCount;
        };

        std::vector<CopiedComponent> m_copiedComponents;

        SimEngine::Commands::CommandsManager m_cmdManager;

        VkExtent2D vec2Extent2D(const glm::vec2 &vec);

        bool m_isDestroyed = false;
    };
} // namespace Bess::Canvas
