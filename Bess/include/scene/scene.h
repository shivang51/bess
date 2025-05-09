#pragma once

#include "camera.h"
#include "component_catalog.h"
#include "component_definition.h"
#include "entt/entity/fwd.hpp"
#include "entt/entt.hpp"
#include "events/application_event.h"
#include "scene/components/components.h"
#include "scene/renderer/gl/framebuffer.h"
#include <memory>

namespace Bess::Canvas {
    enum class SceneDrawMode {
        none,
        connection,
        selectionBox
    };

    struct LastCreatedComponent {
        const SimEngine::ComponentDefinition *componentDefinition;
        int inputCount = -1;
        int outputCount = -1;
    };

    class Scene {
      public:
        Scene();
        ~Scene();

      public:
        static Scene &instance();

        void reset();
        void clear();
        void render();
        void update(const std::vector<ApplicationEvent> &events);

        unsigned int getTextureId();
        std::shared_ptr<Camera> getCamera();

        void beginScene();
        void endScene();

        void setLastCreatedComp(const LastCreatedComponent &comp);

      public:
        glm::vec2 getCameraPos();
        float getCameraZoom();
        void setZoom(float value);

        void resize(const glm::vec2 &size);
        entt::registry &getEnttRegistry();
        const glm::vec2 &getSize();

        UUID createSlotEntity(Components::SlotType type, const UUID &parent, unsigned int idx);
        UUID createSimEntity(const UUID &simEngineEntt, const SimEngine::ComponentDefinition &comp, const glm::vec2 &pos);

        void deleteEntity(const UUID &entUuid);
        void deleteConnection(const UUID &entUuid);
        entt::entity getEntityWithUuid(const UUID &uuid);
        bool isEntityValid(const UUID &uuid);
        void setZCoord(float val);

      private:
        const UUID &getUuidOfEntity(entt::entity ent);

        void updateHoveredId();

        void onMouseMove(const glm::vec2 &pos);
        void onLeftMouse(bool isPressed);
        void onMiddleMouse(bool isPressed);
        void onRightMouse(bool isPressed);
        void onMouseWheel(double x, double y);

        glm::vec2 getNVPMousePos(const glm::vec2 &mousePos);
        glm::vec2 getViewportMousePos(const glm::vec2 &mousePos);
        bool isCursorInViewport(const glm::vec2 &pos);
        void drawConnection();
        void drawSelectionBox();
        void handleKeyboardShortcuts();
        void connectSlots(entt::entity startSlot, entt::entity endSlot);
        void generateBasicConnection(entt::entity startSlot, entt::entity endSlot);
        void copySelectedComponents();
        void generateCopiedComponents();
        void selectEntitesInArea(const glm::vec2 &start, const glm::vec2 &end);
        void toggleSelectComponent(const UUID &uuid);
        void toggleSelectComponent(entt::entity ent);
        void selectAllEntities();

        void moveConnection(entt::entity ent, glm::vec2 &dPos);
        void dragConnectionSegment(entt::entity ent, const glm::vec2 &dPos);

        float getNextZCoord();

      private:
        std::unique_ptr<Gl::FrameBuffer> m_msaaFramebuffer;
        std::unique_ptr<Gl::FrameBuffer> m_shadowFramebuffer;
        std::unique_ptr<Gl::FrameBuffer> m_placeHolderFramebuffer;
        std::unique_ptr<Gl::FrameBuffer> m_normalFramebuffer;
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

        SceneDrawMode m_drawMode = SceneDrawMode::none;

        const float m_zIncrement = 0.001;
        float m_compZCoord = m_zIncrement;
        bool m_isDragging = false;

        LastCreatedComponent m_lastCreatedComp = {};
        std::vector<SimEngine::ComponentType> m_copiedComponents = {};
    };
} // namespace Bess::Canvas
