#pragma once

#include "application/app_types.h"
#include "application/events/application_event.h"
#include "bess_uuid.h"
#include "commands/commands_manager.h"
#include "events/sim_engine_events.h"
#include "scene/camera.h"
#include "scene/scene_events.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/sim_scene_comp_draw_hook.h"
#include "scene/scene_state/scene_state.h"
#include "scene/viewport.h"
#include <memory>
#include <vulkan/vulkan_core.h>

// Forward declaration
namespace Bess::Canvas {
    class ArtistManager;
}

namespace Bess::Canvas {

    // Need to contain the size and pos of the UI viewport in which
    // scene is rendered, so that mouse coordinates can be transformed correctly
    struct ViewportTransform {
        glm::vec2 pos;
        glm::vec2 size;
    };

    enum class SceneMode : uint8_t {
        general,
    };

    enum class SceneDrawMode : uint8_t {
        none,
        connection,
        selectionBox
    };

    class Scene {
      public:
        Scene();
        ~Scene();

      public:
        std::shared_ptr<Viewport> getViewport();

        void destroy();

        void reset();
        void clear();
        void render();
        void renderWithViewport(const std::shared_ptr<Viewport> &viewport);
        void update(TFrameTime ts, const std::vector<ApplicationEvent> &events);

        uint64_t getTextureId() const;
        std::shared_ptr<Camera> getCamera();

        void drawSceneToViewport(const std::shared_ptr<Viewport> &viewport);

        const SceneState &getState() const;
        SceneState &getState();

      public:
        void addComponent(const std::shared_ptr<SceneComponent> &comp, bool setZ = true);

        void updateViewportTransform(const ViewportTransform &transform);

        const ViewportTransform &getViewportTransform() const;

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

        void deleteSceneEntity(const UUID &entUuid);
        void deleteSelectedSceneEntities();

        void setZCoord(float val);

        SimEngine::Commands::CommandsManager &getCmdManager();

        bool *getIsSchematicViewPtr();
        void toggleSchematicView();

        bool isHoveredEntityValid();

        std::shared_ptr<SimSceneCompDrawHook> getPluginDrawHookForComponentHash(uint64_t compHash) const;

        bool hasPluginDrawHookForComponentHash(uint64_t compHash) const;

        void selectAllEntities();
        void focusCameraOnSelected();

      private:
        /// to draw testing stuff
        void drawScratchContent(TFrameTime ts, const std::shared_ptr<Viewport> &viewport);

        void updatePickingId();

        void setPickingId(const PickingId &pickingId);

        void onMouseMove(const glm::vec2 &pos);
        void onLeftMouse(bool isPressed);
        void onLeftDoubleClick();
        void onMiddleMouse(bool isPressed);
        void onRightMouse(bool isPressed);
        void onMouseWheel(double x, double y);

        std::shared_ptr<Viewport> m_viewport;

        glm::vec2 toScenePos(const glm::vec2 &mousePos) const;
        glm::vec2 getViewportMousePos(const glm::vec2 &mousePos) const;
        bool isCursorInViewport(const glm::vec2 &pos) const;
        void drawSelectionBox();
        bool selectEntitesInArea();

        float getNextZCoord();

        glm::vec2 getSnappedPos(const glm::vec2 &pos) const;

      private:
        // Vulkan framebuffers for scene rendering
        glm::vec2 m_size, m_mousePos, m_dMousePos;
        std::shared_ptr<Camera> m_camera;

        bool m_isLeftMousePressed = false, m_isMiddleMousePressed = false;

        void onCompDefOutputsResized(const SimEngine::Events::CompDefOutputsResizedEvent &e);
        void onCompDefInputsResized(const SimEngine::Events::CompDefInputsResizedEvent &e);

        void onConnectionRemoved(const Events::ConnectionRemovedEvent &e);

        void drawGhostConnection(const std::shared_ptr<PathRenderer> &pathRenderer,
                                 const glm::vec2 &startPos,
                                 const glm::vec2 &endPos);

        void loadComponentFromPlugins();
        void cleanupPlugins();

      private:
        SceneState m_state;

        bool m_isCtrlPressed = false, m_isShiftPressed = false;
        ViewportTransform m_viewportTransform;

        PickingId m_pickingId = PickingId::invalid();
        PickingId m_prevPickingId = PickingId::invalid();

        // selection box
        glm::vec2 m_selectionBoxStart;
        glm::vec2 m_selectionBoxEnd;
        bool m_selectInSelectionBox = false;
        bool m_getIdsInSelBox = false;

        bool m_isDragging = false;
        SceneDrawMode m_drawMode = SceneDrawMode::none;
        SceneMode m_sceneMode = SceneMode::general;

        static constexpr float m_zIncrement = 0.001f;
        static constexpr float snapSize = 2.f;
        float m_compZCoord = m_zIncrement;

        TFrameTime m_frameTimeStep = {};

        SimEngine::Commands::CommandsManager m_cmdManager;

        VkExtent2D vec2Extent2D(const glm::vec2 &vec);

        bool m_isDestroyed = false;
        std::unordered_map<uint64_t, std::shared_ptr<Canvas::SimSceneCompDrawHook>> m_pluginSceneDrawHooks;
    };
} // namespace Bess::Canvas
