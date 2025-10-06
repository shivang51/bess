#pragma once

#include "scene/renderer/vulkan/command_buffer.h"
#include "scene/renderer/vulkan/device.h"
#include "scene/renderer/vulkan/imgui_pipeline.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include "scene/renderer/vulkan/pipeline.h"
#include "scene/renderer/vulkan/primitive_renderer.h"
#include "scene/renderer/vulkan/swapchain.h"
#include "scene/renderer/vulkan/vulkan_image_view.h"
#include "scene/renderer/vulkan/vulkan_offscreen_render_pass.h"
#include "scene/renderer/vulkan/vulkan_render_pass.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

// Forward declarations for types that were in the original renderer
namespace Bess {
    class Camera;
}
// moved to vulkan_renderer.h

namespace Bess::Renderer2D {

    typedef std::function<void(VkInstance &, VkSurfaceKHR &)> SurfaceCreationCB;

    struct FrameContext {
        std::shared_ptr<Vulkan::VulkanCommandBuffer> cmdBuffer = nullptr;
        uint32_t swapchainImgIdx;
    };

    class VulkanCore {

      public:
        static VulkanCore &instance() {
            static VulkanCore inst;
            return inst;
        }

        static bool isInitialized;

        VulkanCore() = default;
        ~VulkanCore();

        VulkanCore(const VulkanCore &) = delete;
        VulkanCore &operator=(const VulkanCore &) = delete;
        VulkanCore(VulkanCore &&) = delete;
        VulkanCore &operator=(VulkanCore &&) = delete;

        void init(const std::vector<const char *> &winExt,
                  const SurfaceCreationCB &createSurface,
                  VkExtent2D windowExtent);

        /// typical flow
        void beginFrame();
        void beginOffscreenRender(const glm::vec4 &clearColor, int clearPickingId = -1);
        void endOffscreenRender();
        void renderUi();
        void endFrame();

        void cleanup(const std::function<void()> &preCmdBufferCleanup = []() {});

        std::shared_ptr<Vulkan::VulkanRenderPass> getRenderPass() const {
            return m_renderPass;
        }

        // Scene texture access for ImGui
        uint64_t getSceneTextureId();

        std::weak_ptr<Vulkan::PrimitiveRenderer> getPrimitiveRenderer() const { return m_primitiveRenderer; }
        std::weak_ptr<Vulkan::PathRenderer> getPathRenderer() const { return m_pathRenderer; }
        void resizeOffscreen(VkExtent2D extent);
        VkExtent2D getOffscreenExtent() const {
            return m_offscreenImageView ? m_offscreenImageView->getExtent() : VkExtent2D{0, 0};
        }

        // Mouse picking functionality
        int32_t readPickingId(int x, int y);
        int32_t getPickingIdResult(); // Get result from previous frame

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
        void createPickingResources();
        void cleanupPickingResources();

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
        std::shared_ptr<Vulkan::PathRenderer> m_pathRenderer;
        VkSurfaceKHR m_renderSurface = VK_NULL_HANDLE;

        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence> m_inFlightFences;
        uint32_t m_currentFrameIdx = 0;

        // Mouse picking resources
        VkBuffer m_pickingStagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_pickingStagingBufferMemory = VK_NULL_HANDLE;
        int m_pendingPickingX = -1;
        int m_pendingPickingY = -1;
        int32_t m_pickingResult = -1;
        bool m_pickingRequestPending = false;
        bool m_pickingCopyInFlight = false;
        uint32_t m_pickingCopyRecordedFrameIdx = 0;

      public:
        void recreateSwapchain();
        void recreateSwapchain(VkExtent2D newExtent);
    };

} // namespace Bess::Renderer2D
