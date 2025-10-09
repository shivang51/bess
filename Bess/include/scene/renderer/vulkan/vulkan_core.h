#pragma once

#include "scene/renderer/vulkan/command_buffer.h"
#include "scene/renderer/vulkan/device.h"
#include "scene/renderer/vulkan/swapchain.h"
#include "scene/renderer/vulkan/vulkan_render_pass.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D {

    using SurfaceCreationCB = std::function<void(VkInstance &, VkSurfaceKHR &)>;

    using SwapchainRenderFn = std::function<void(VkCommandBuffer)>;

    struct FrameContext {
        std::shared_ptr<Vulkan::VulkanCommandBuffer> cmdBuffer = nullptr;
        uint32_t swapchainImgIdx;
        bool isStarted = false;
    };

    class VulkanCore {
      public:
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        static VulkanCore &instance();

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

        void beginFrame();
        void renderToSwapchain(const SwapchainRenderFn &fn);
        void endFrame();

        void cleanup(const std::function<void()> &preCmdBufferCleanup = []() {});

        std::shared_ptr<Vulkan::VulkanRenderPass> getRenderPass() const {
            return m_renderPass;
        }

        VkInstance getVkInstance() const { return m_vkInstance; }
        std::shared_ptr<Vulkan::VulkanDevice> getDevice() const { return m_device; }
        std::shared_ptr<Vulkan::VulkanSwapchain> getSwapchain() const { return m_swapchain; }
        const std::vector<std::shared_ptr<Vulkan::VulkanCommandBuffer>> &getCommandBuffer() const { return m_commandBuffers->getCmdBuffers(); }

        uint32_t getCurrentFrameIdx() const {
            return m_currentFrameIdx;
        }

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
        std::unique_ptr<Vulkan::VulkanCommandBuffers> m_commandBuffers;
        std::shared_ptr<Vulkan::VulkanRenderPass> m_renderPass;
        VkSurfaceKHR m_renderSurface = VK_NULL_HANDLE;

        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence> m_inFlightFences;
        uint32_t m_currentFrameIdx = 0;

      public:
        void recreateSwapchain(VkExtent2D newExtent);
    };

} // namespace Bess::Renderer2D
