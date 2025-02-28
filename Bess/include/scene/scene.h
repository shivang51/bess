#pragma once

#include "camera.h"
#include "entt/entity/fwd.hpp"
#include "entt/entt.hpp"
#include "events/application_event.h"
#include "glm.hpp"
#include "scene/components/components.h"
#include "scene/renderer/gl/framebuffer.h"
#include <memory>

namespace Bess::Canvas {
    enum class SceneDrawMode {
        none,
        connection,
        selectionBox
    };

    class Scene {
      public:
        Scene();
        ~Scene();

      public:
        static Scene &instance();

        void reset();
        void render();
        void update(const std::vector<ApplicationEvent> &events);

        unsigned int getTextureId();
        std::shared_ptr<Camera> getCamera();

        void beginScene();
        void endScene();

      public:
        glm::vec2 getCameraPos();
        float getCameraZoom();
        void setZoom(float value);

        void resize(const glm::vec2 &size);
        entt::registry &getEnttRegistry();
        const glm::vec2 &getSize();

        entt::entity createSlotEntity(Components::SlotType type, entt::entity parent, uint idx);
        entt::entity createSimEntity(entt::entity simEngineEntt, std::string name, int inputs, int ouputs);
        void deleteEntity(entt::entity ent);

      private:
        void onMouseMove(const glm::vec2 &pos);
        void onLeftMouse(bool isPressed);
        void onMouseWheel(double x, double y);

        glm::vec2 getNVPMousePos(const glm::vec2 &mousePos);
        glm::vec2 getViewportMousePos(const glm::vec2 &mousePos);
        bool isCursorInViewport(const glm::vec2 &pos);
        void drawConnection();
        void drawSelectionBox();
        void handleKeyboardShortcuts();
        void connectSlots(entt::entity startSlot, entt::entity endSlot);

        void selectEntitesInArea(const glm::vec2 &start, const glm::vec2 &end);

        float getNextZCoord();

      private:
        std::unique_ptr<Gl::FrameBuffer> m_msaaFramebuffer;
        std::unique_ptr<Gl::FrameBuffer> m_normalFramebuffer;
        glm::vec2 m_size, m_mousePos;
        std::shared_ptr<Camera> m_camera;

        bool m_isLeftMousePressed = false;

      private:
        entt::registry m_registry;
        entt::entity m_hoveredEntiy;
        entt::entity m_connectionStartEntity;
        // selection box
        glm::vec2 m_selectionBoxStart;
        glm::vec2 m_selectionBoxEnd;
        bool m_selectInSelectionBox = false;

        SceneDrawMode m_drawMode = SceneDrawMode::none;

        const float m_zIncrement = 0.001;
        float m_compZCoord = m_zIncrement;
    };
} // namespace Bess::Canvas
