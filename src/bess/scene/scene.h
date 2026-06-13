#pragma once

#include "common/bess_uuid.h"
#include "common/types.h"
#include "scene/camera.h"
#include "scene/scene_events.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/scene_state.h"
#include "scene/scene_types.h"
#include "scene_layer.h"
#include "sim_driver/sim_driver.h"
#include <memory>

namespace Bess::Canvas {

    class Scene {
      public:
        Scene();
        ~Scene();

      public:
        void destroy();

        void reset();
        void clear();
        void update(TimeMs ts, bool isFocused);
        void draw(const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer);

        const SceneState &getState() const;
        SceneState &getState();

        MAKE_GETTER_SETTER(bool, IsFirstFrame, m_isFirstFrame)

      public:
        const UUID &getSceneId() const;

        void addComponent(const std::shared_ptr<SceneComponent> &comp,
                          bool setZ = true);

        void updateViewportTransform(const ViewportTransform &transform);

        const ViewportTransform &getViewportTransform() const;

        PickingId getHoveredEntity() const { return m_pickingId; }
        void setPickingId(const PickingId &value);

        const PickingReadbackRequest &getPickingReadbackRequest() const;

        bool isDragging() const;
        bool isLeftMousePressed() const;
        bool isMiddleMousePressed() const;
        SceneCursor consumeCursorRequest();

        const glm::vec2 &getMousePos() const;
        glm::vec2 getSceneMousePos();
        const glm::vec2 &getCameraPos() const;
        float getCameraZoom() const;
        void setZoom(float value) const;
        const glm::mat4 &getCameraTransform() const;
        float *getCameraTransformData() const;
        void resizeCamera(const glm::vec2 &size) const;
        void panCamera(const glm::vec2 &delta) const;
        void focusCameraAt(const glm::vec2 &pos, bool smooth = true) const;

        void setSceneMode(SceneMode mode);
        SceneMode getSceneMode() const;

        void resize(const glm::vec2 &size);
        const glm::vec2 &getSize() const;

        bool deleteSceneEntity(const UUID &entUuid);
        void deleteSelectedSceneEntities();

        void setZCoord(float val);

        bool getIsSchematicView() const;
        void setIsSchematicView(bool value);
        void toggleSchematicView();

        bool isHoveredEntityValid();

        void selectAllEntities();
        void focusCameraOnSelected();
        glm::vec2 toScenePos(const glm::vec2 &mousePos) const;

        float getNextZCoord();

        bool dispatchEvent(SceneEvent &evt);

        void onLeftMouse(bool isPressed);
        void onMiddleMouse(bool isPressed);
        void onMouseMove(const glm::vec2 &pos);
        void applySelectionReadback(const std::vector<PickingId> &ids);
        void clearPickingReadbackRequest();

      private:
        void onPrePickingIdChange(const PickingId &newId);
        void onPickingIdChange();

        glm::vec2 getViewportMousePos(const glm::vec2 &mousePos) const;

      private:
        glm::vec2 m_size;
        std::shared_ptr<Camera> m_camera = nullptr;

        SceneState m_state;

        ViewportTransform m_viewportTransform;
        SceneInputState m_inputState;

        PickingId m_pickingId = PickingId::invalid();
        PickingId m_prevPickingId = PickingId::invalid();
        SceneMode m_sceneMode = SceneMode::general;

        static constexpr float m_zIncrement = 0.001f;
        float m_compZCoord = m_zIncrement;

        TimeMs m_frameTimeStep = {};

        bool m_isDestroyed = false;
        bool m_isFirstFrame = true;

        std::vector<std::unique_ptr<ISceneLayer>> m_sceneLayers;
    };
} // namespace Bess::Canvas
