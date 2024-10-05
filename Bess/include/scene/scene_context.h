#pragma once

#include "renderer/gl/framebuffer.h"
#include "scene/entity/entity.h"
#include "scene/events/event.h"
#include <queue>
#include <vector>

namespace Bess::Scene {
    class SceneContext {
      public:
        SceneContext() = default;
        ~SceneContext();

        void update();
        void render();

        void init();

        unsigned int getTextureId();

        void onEvent(const Events::Event &evt);

        void beginScene();
        void endScene();

        void addEntity(std::shared_ptr<Entity> entity);
        const std::vector<std::shared_ptr<Entity>> &getEntities() const;

      private:
        std::unique_ptr<Gl::FrameBuffer> m_msaaFramebuffer;
        std::unique_ptr<Gl::FrameBuffer> m_framebuffer;
        glm::vec2 m_size;
        std::queue<Events::Event> m_events;
        std::vector<std::shared_ptr<Entity>> m_entities;
    };
} // namespace Bess::Scene
