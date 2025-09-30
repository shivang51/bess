#pragma once

#include "scene/renderer/vulkan/vulkan_core.h"
#include "scene/renderer/vulkan/primitive_renderer.h"
#include "camera.h"
#include <glm.hpp>
#include <memory>

namespace Bess::Renderer2D {

    class VulkanRenderer {
    public:
        VulkanRenderer() = default;
        ~VulkanRenderer() = default;

        static void init();
        static void shutdown();

        static void begin(std::shared_ptr<Camera> camera);
        static void end();

        static void grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridColors &colors);

    private:
        static std::shared_ptr<Vulkan::PrimitiveRenderer> m_primitiveRenderer;
        static std::shared_ptr<Camera> m_camera;
    };

} // namespace Bess::Renderer2D
