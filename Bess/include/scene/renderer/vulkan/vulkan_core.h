#pragma once

#include "glm.hpp"
#include "scene/renderer/vulkan/command_buffer.h"
#include "scene/renderer/vulkan/device.h"
#include "scene/renderer/vulkan/imgui_pipeline.h"
#include "scene/renderer/vulkan/pipeline.h"
#include "scene/renderer/vulkan/swapchain.h"
#include "scene/renderer/vulkan/vulkan_render_pass.h"
#include "scene/renderer/vulkan/vulkan_image_view.h"
#include "scene/renderer/vulkan/vulkan_offscreen_render_pass.h"
#include "scene/renderer/vulkan/primitive_renderer.h"
#include "scene/renderer/vulkan/primitive_vertex.h"
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

    struct FrameContext {
        std::shared_ptr<Vulkan::VulkanCommandBuffer> cmdBuffer = nullptr;
        uint32_t swapchainImgIdx;
    };

    class VulkanCore {

      public:
        static VulkanCore &instance() {
            static VulkanCore render;
            return render;
        }

        static bool isInitialized;

        VulkanCore() = default;
        ~VulkanCore();

        VulkanCore(const VulkanCore &) = delete;
        VulkanCore &operator=(const VulkanCore &) = delete;

        void init(const std::vector<const char *> &winExt,
                  const SurfaceCreationCB &createSurface,
                  VkExtent2D windowExtent);

        void beginFrame();
        void draw();
        void draw(const std::shared_ptr<Camera> &camera, const GridColors &gridColors);
        void beginOffscreenRender();
        void endOffscreenRender();
        void renderUi();
        void endFrame();

        void cleanup();

        std::shared_ptr<Vulkan::VulkanRenderPass> getRenderPass() const {
            return m_renderPass;
        }

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
        uint64_t getSceneTextureId();
        
        std::shared_ptr<Vulkan::PrimitiveRenderer> getPrimitiveRenderer() const { return m_primitiveRenderer; }
        void resizeOffscreen(VkExtent2D extent);
        VkExtent2D getOffscreenExtent() const {
            return m_offscreenImageView ? m_offscreenImageView->getExtent() : VkExtent2D{0, 0};
        }

        // Getters for ImGui integration
        VkInstance getVkInstance() const { return m_vkInstance; }
        std::shared_ptr<Vulkan::VulkanDevice> getDevice() const { return m_device; }
        std::shared_ptr<Vulkan::VulkanSwapchain> getSwapchain() const { return m_swapchain; }
        const std::vector<std::shared_ptr<Vulkan::VulkanCommandBuffer>> &getCommandBuffer() const { return m_commandBuffers; }
        std::shared_ptr<Vulkan::VulkanPipeline> getPipeline() const { return m_pipeline; }

      private:
        VkResult initVkInstance(const std::vector<const char *> &winExtensions);
        VkResult validateExtensions(const std::vector<const char *> &extensions) const;
        VkResult validateLayers(const std::vector<const char *> &layers) const;
        VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo() const;
        VkResult createDebugMessenger();
        VkResult destroyDebugMessenger() const;
        void createSyncObjects();

        FrameContext m_currentFrameContext = {};

        VkInstance m_vkInstance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_vkDebugMessenger = VK_NULL_HANDLE;
        std::shared_ptr<Vulkan::VulkanDevice> m_device;
        std::shared_ptr<Vulkan::VulkanSwapchain> m_swapchain;
        std::shared_ptr<Vulkan::VulkanPipeline> m_pipeline;
        std::vector<std::shared_ptr<Vulkan::VulkanCommandBuffer>> m_commandBuffers;
        std::shared_ptr<Vulkan::VulkanRenderPass> m_renderPass;
        std::shared_ptr<Vulkan::VulkanOffscreenRenderPass> m_offscreenRenderPass;
        std::shared_ptr<Vulkan::VulkanImageView> m_offscreenImageView;
        std::shared_ptr<Vulkan::PrimitiveRenderer> m_primitiveRenderer;
        VkSurfaceKHR m_renderSurface = VK_NULL_HANDLE;

        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence> m_inFlightFences;
        uint32_t m_currentFrameIdx = 0;

      public:
        void recreateSwapchain();
        void recreateSwapchain(VkExtent2D newExtent);
    };

} // namespace Bess::Renderer2D
