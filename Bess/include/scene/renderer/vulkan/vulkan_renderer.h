#pragma once

#include "glm.hpp"
#include "scene/renderer/vulkan/command_buffer.h"
#include "scene/renderer/vulkan/device.h"
#include "scene/renderer/vulkan/pipeline.h"
#include "scene/renderer/vulkan/swapchain.h"
#include "scene/renderer/vulkan/vulkan_framebuffer.h"
#include "scene/renderer/vulkan/vulkan_render_pass.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

// Forward declarations for types that were in the original renderer
namespace Bess {
    class Camera;
}
namespace Bess::Renderer2D {
    struct QuadRenderProperties {
        float angle = 0.0f;
        glm::vec4 borderColor = {0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 borderRadius = {0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 borderSize = {0.0f, 0.0f, 0.0f, 0.0f};
        bool hasShadow = false;
        bool isMica = false;
    };

    struct GridColors {
        glm::vec4 minorColor;
        glm::vec4 majorColor;
        glm::vec4 axisXColor;
        glm::vec4 axisYColor;
    };
} // namespace Bess::Renderer2D

namespace Bess::Renderer2D {

    typedef std::function<void(VkInstance &, VkSurfaceKHR &)> SurfaceCreationCB;

    class VulkanRenderer {
      public:
        static VulkanRenderer &instance() {
            static VulkanRenderer render;
            return render;
        }

        static bool isInitialized;

        VulkanRenderer() = default;
        ~VulkanRenderer();

        // Delete copy constructor and assignment operator
        VulkanRenderer(const VulkanRenderer &) = delete;
        VulkanRenderer &operator=(const VulkanRenderer &) = delete;

        // Move constructor and assignment operator
        VulkanRenderer(VulkanRenderer &&other) noexcept;
        VulkanRenderer &operator=(VulkanRenderer &&other) noexcept;

        void init(const std::vector<const char *> &winExt,
                  const SurfaceCreationCB &createSurface,
                  VkExtent2D windowExtent,
                  const std::string &vertShaderPath,
                  const std::string &fragShaderPath);

        void draw();
        void cleanup();

        static void begin(const std::shared_ptr<Bess::Camera> &camera);
        static void end();

        // Rendering functions (to be implemented to match OpenGL renderer API)
        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id, QuadRenderProperties properties = {});

        static void circle(const glm::vec3 &center, float radius,
                           const glm::vec4 &color, int id, float innerRadius = 0.0f);

        static void text(const std::string &data, const glm::vec3 &pos, size_t size,
                         const glm::vec4 &color, int id, float angle = 0.f);

        static void msdfText(const std::string &data, const glm::vec3 &pos, size_t size,
                             const glm::vec4 &color, int id, float angle = 0.f);

        static void line(const glm::vec3 &start, const glm::vec3 &end, float size,
                         const glm::vec4 &color, int id);

        static void grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridColors &colors);

        // Path API
        static void beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, uint64_t id);
        static void endPathMode(bool closePath = false, bool genFill = false, const glm::vec4 &fillColor = glm::vec4(1.f), bool genStroke = true);
        static void pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, int id);
        static void pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                      float weight, const glm::vec4 &color, int id);
        static void pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, int id);

        // Text utilities
        static glm::vec2 getCharRenderSize(char ch, float renderSize);
        static glm::vec2 getTextRenderSize(const std::string &str, float renderSize);
        static glm::vec2 getMSDFTextRenderSize(const std::string &str, float renderSize);

        // Scene texture access for ImGui
        static uint64_t getSceneTextureId();

        // Getters for ImGui integration
        VkInstance getVkInstance() const { return m_vkInstance; }
        std::shared_ptr<Vulkan::VulkanDevice> getDevice() const { return m_device; }
        std::shared_ptr<Vulkan::VulkanSwapchain> getSwapchain() const { return m_swapchain; }
        std::shared_ptr<Vulkan::VulkanCommandBuffer> getCommandBuffer() const { return m_commandBuffer; }
        std::shared_ptr<Vulkan::VulkanPipeline> getPipeline() const { return m_pipeline; }

      private:
        VkResult initVkInstance(const std::vector<const char *> &winExtensions);
        VkResult validateExtensions(const std::vector<const char *> &extensions);
        VkResult validateLayers(const std::vector<const char *> &layers);
        VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo();
        VkResult createDebugMessenger();
        VkResult destroyDebugMessenger();
        void createSyncObjects();

        VkInstance m_vkInstance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_vkDebugMessenger = VK_NULL_HANDLE;
        std::shared_ptr<Vulkan::VulkanDevice> m_device;
        std::shared_ptr<Vulkan::VulkanSwapchain> m_swapchain;
        std::shared_ptr<Vulkan::VulkanPipeline> m_pipeline;
        std::shared_ptr<Vulkan::VulkanCommandBuffer> m_commandBuffer;
        std::shared_ptr<Vulkan::VulkanRenderPass> m_renderPass;
        std::shared_ptr<Vulkan::VulkanFramebuffer> m_sceneFramebuffer;
        VkSurfaceKHR m_renderSurface = VK_NULL_HANDLE;

        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        static constexpr int MAX_SEMAPHORES = 10;

        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence> m_inFlightFences;
        uint32_t m_currentFrame = 0;
        uint32_t m_semaphoreIndex = 0;
    };

} // namespace Bess::Renderer2D
