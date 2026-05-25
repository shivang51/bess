#pragma once
#include "command_buffer.h"
#include "common/bess_api.h"
#include "common/class_helpers.h"
#include "common/sub_system.h"
#include "device.h"
#include "swapchain.h"
#include "vulkan_render_pass.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace Bess::Vulkan {

    using SurfaceCreationCB = std::function<void(VkInstance &, VkSurfaceKHR &)>;

    using SwapchainRenderFn = std::function<void(VkCommandBuffer)>;

    struct BESS_API FrameContext {
        std::shared_ptr<VulkanCommandBuffer> cmdBuffer = nullptr;
        uint32_t swapchainImgIdx;
        bool isStarted = false;
    };

    class BESS_API VulkanCore : public ISubSystem {
      public:
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

        void onPreDraw() override;
        void onPostDraw() override;

        void onInit() override;
        void onPostInit() override;
        void onDestroy() override;

        VulkanCore() = default;
        ~VulkanCore() override;

        VulkanCore(const VulkanCore &) = delete;
        VulkanCore &operator=(const VulkanCore &) = delete;
        VulkanCore(VulkanCore &&) = delete;
        VulkanCore &operator=(VulkanCore &&) = delete;

        void beginFrame();
        void renderToSwapchain(const SwapchainRenderFn &fn);
        void endFrame();

        void cleanup(const std::function<void()> &preCmdBufferCleanup = []() {
        });

        MAKE_GETTER_SETTER(VkSurfaceKHR, RenderSurface, m_renderSurface)
        MAKE_GETTER_SETTER(std::vector<const char *>, WinExt, m_windowExt)
        MAKE_GETTER_SETTER(SurfaceCreationCB, CreateSurfaceFn,
                           m_createSurfaceFn)

        std::shared_ptr<VulkanRenderPass> getRenderPass() const;

        VkInstance getVkInstance() const;
        std::shared_ptr<VulkanDevice> getDevice() const;
        std::shared_ptr<VulkanSwapchain> getSwapchain() const;
        const std::vector<std::shared_ptr<VulkanCommandBuffer>> &
        getCommandBuffer() const;

        uint32_t getCurrentFrameIdx() const;

        void recreateSwapchain(VkExtent2D newExtent);

      private:
        VkResult initVkInstance(const std::vector<const char *> &winExtensions);
        VkResult
        validateExtensions(const std::vector<const char *> &extensions) const;
        VkResult validateLayers(const std::vector<const char *> &layers) const;
        VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo() const;
        VkResult createDebugMessenger();
        VkResult destroyDebugMessenger() const;
        void createSyncObjects();

        FrameContext m_currentFrameContext = {};

        VkInstance m_vkInstance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_vkDebugMessenger = VK_NULL_HANDLE;
        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanSwapchain> m_swapchain;
        std::unique_ptr<VulkanCommandBuffers> m_commandBuffers;
        std::shared_ptr<VulkanRenderPass> m_renderPass;
        VkSurfaceKHR m_renderSurface = VK_NULL_HANDLE;

        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence> m_inFlightFences;
        uint32_t m_currentFrameIdx = 0;

        bool m_isDestroyed = false;

        bool m_hasSwapchainImg = false;

        std::vector<const char *> m_windowExt;
        SurfaceCreationCB m_createSurfaceFn;
    };

} // namespace Bess::Vulkan
