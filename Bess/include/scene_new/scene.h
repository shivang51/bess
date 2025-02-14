#pragma once

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

      private:
        std::unique_ptr<Gl::FrameBuffer> m_msaaFramebuffer;
        std::unique_ptr<Gl::FrameBuffer> m_framebuffer;
        glm::vec2 m_size;

      private:
        entt::registry m_Registry;
    };
} // namespace Bess::Canvas
