#pragma once

#include "camera.h"
#include "entt/entity/fwd.hpp"
#include "entt/entt.hpp"
#include "glm.hpp"
#include "scene/renderer/gl/framebuffer.h"

namespace Bess::Canvas {
    class Scene {
      public:
        Scene();
        ~Scene();

      public:
        void reset();
        void render();
        void update();

        unsigned int getTextureId();

        void beginScene();
        void endScene();

      public:
        glm::vec2 getCameraPos();
        void resize(const glm::vec2 &size);
        const entt::registry &getEnttRegistry() const;

      private:
        std::unique_ptr<Gl::FrameBuffer> m_msaaFramebuffer;
        std::unique_ptr<Gl::FrameBuffer> m_normalFramebuffer;
        glm::vec2 m_size;
        std::shared_ptr<Camera> m_camera;

      private:
        entt::registry m_Registry;
    };
} // namespace Bess::Canvas
