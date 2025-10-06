#include "scene/renderer/vulkan/vulkan_core.h"
#include "camera.h"
#include "common/log.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "scene/renderer/vulkan_renderer.h"
#include <memory>
#include <set>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace Bess::Renderer2D {
    VulkanCore::~VulkanCore() {
        cleanup();
    }

    bool VulkanCore::isInitialized = false;

    void VulkanCore::init(const std::vector<const char *> &winExt,
                          const SurfaceCreationCB &createSurface,
                          VkExtent2D windowExtent) {
        if (isInitialized) {
            BESS_WARN("Reinitialization of VulkaCore was called...skipping");
            return;
        }

        BESS_INFO("Initializing VulkanCore");

        initVkInstance(winExt);
        createDebugMessenger();
        createSurface(m_vkInstance, m_renderSurface);
        BESS_INFO("Created VkInstance and draw surface");

        m_device = std::make_shared<Vulkan::VulkanDevice>(m_vkInstance, m_renderSurface);
        m_swapchain = std::make_shared<Vulkan::VulkanSwapchain>(m_vkInstance, m_device, m_renderSurface, windowExtent);

        m_renderPass = std::make_shared<Vulkan::VulkanRenderPass>(m_device, m_swapchain->imageFormat(), VK_FORMAT_D32_SFLOAT);

        m_offscreenImageView = std::make_shared<Vulkan::VulkanImageView>(
            m_device,
            m_swapchain->imageFormat(),
            VK_FORMAT_R32_SINT, // Picking format
            windowExtent);
        m_offscreenRenderPass = std::make_shared<Vulkan::VulkanOffscreenRenderPass>(m_device, m_swapchain->imageFormat(), VK_FORMAT_R32_SINT);
        m_offscreenImageView->createFramebuffer(m_offscreenRenderPass->getVkHandle());

        m_primitiveRenderer = std::move(std::make_shared<Vulkan::PrimitiveRenderer>(m_device, m_offscreenRenderPass, windowExtent));
        m_pathRenderer = std::move(std::make_shared<Vulkan::PathRenderer>(m_device, m_offscreenRenderPass, windowExtent));

        m_swapchain->createFramebuffers(m_renderPass->getVkHandle());

        m_commandBuffers = Vulkan::VulkanCommandBuffer::createCommandBuffers(m_device, 2);
        createSyncObjects();
        createPickingResources();

        isInitialized = true;
        BESS_INFO("Renderer Initialized");
    }

    void VulkanCore::beginOffscreenRender(const glm::vec4 &clearColor, int clearPickingId) {
        if (!m_offscreenImageView || !m_offscreenRenderPass || !m_primitiveRenderer) {
            return;
        }

        const auto cmdBuffer = m_currentFrameContext.cmdBuffer;

        m_offscreenRenderPass->begin(
            cmdBuffer->getVkHandle(),
            m_offscreenImageView->getFramebuffer(),
            m_offscreenImageView->getExtent(),
            clearColor,
            clearPickingId);

        m_primitiveRenderer->setCurrentFrameIndex(m_currentFrameIdx);
        m_primitiveRenderer->beginFrame(cmdBuffer->getVkHandle());

        if (m_pathRenderer) {
            m_pathRenderer->setCurrentFrameIndex(m_currentFrameIdx);
            m_pathRenderer->beginFrame(cmdBuffer->getVkHandle());
        }
    }

    void VulkanCore::endOffscreenRender() {
        if (!m_offscreenRenderPass || !m_primitiveRenderer) {
            return;
        }

        m_primitiveRenderer->endFrame();

        if (m_pathRenderer) {
            m_pathRenderer->endFrame();
        }

        m_offscreenRenderPass->end();

        // If a picking request is pending, record the copy now in this frame's command buffer
        if (m_pickingRequestPending && m_offscreenImageView && m_offscreenImageView->hasPickingAttachments()) {
            const VkCommandBuffer cmd = m_currentFrameContext.cmdBuffer->getVkHandle();

            const VkImage idImage = m_offscreenImageView->getPickingImage(); // resolve single-sample image
            if (idImage != VK_NULL_HANDLE) {
                // Ensure staging buffer exists
                if (m_pickingStagingBuffer == VK_NULL_HANDLE) {
                    createPickingResources();
                }

                // Barrier: COLOR_ATTACHMENT_OPTIMAL -> TRANSFER_SRC_OPTIMAL
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = idImage;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                vkCmdPipelineBarrier(cmd,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0, 0, nullptr, 0, nullptr, 1, &barrier);

                // Clamp coordinates
                VkExtent2D extent = m_offscreenImageView->getExtent();
                const int px = std::max(0, std::min(m_pendingPickingX, static_cast<int>(extent.width) - 1));
                const int py = std::max(0, std::min(m_pendingPickingY, static_cast<int>(extent.height) - 1));

                // Copy 1 pixel to staging buffer
                VkBufferImageCopy copy{};
                copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copy.imageSubresource.mipLevel = 0;
                copy.imageSubresource.baseArrayLayer = 0;
                copy.imageSubresource.layerCount = 1;
                copy.imageOffset = {px, py, 0};
                copy.imageExtent = {1, 1, 1};

                vkCmdCopyImageToBuffer(cmd, idImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       m_pickingStagingBuffer, 1, &copy);

                // Transition image back for next frame
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                vkCmdPipelineBarrier(cmd,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     0, 0, nullptr, 0, nullptr, 1, &barrier);

                // Mark in-flight; we will read after this frame fence signals
                m_pickingCopyInFlight = true;
                m_pickingCopyRecordedFrameIdx = m_currentFrameIdx;
            }
        }
    }

    void VulkanCore::resizeOffscreen(VkExtent2D extent) {
        if (!m_device || !m_offscreenImageView || !m_offscreenRenderPass) {
            return;
        }
        vkDeviceWaitIdle(m_device->device());

        m_offscreenImageView->recreate(extent, m_offscreenRenderPass->getVkHandle());

        if (m_primitiveRenderer) {
            m_primitiveRenderer.reset();
        }
        if (m_pathRenderer) {
            m_pathRenderer.reset();
        }

        m_primitiveRenderer = std::make_shared<Vulkan::PrimitiveRenderer>(m_device, m_offscreenRenderPass, extent);
        m_pathRenderer = std::make_shared<Vulkan::PathRenderer>(m_device, m_offscreenRenderPass, extent);
    }

    void VulkanCore::beginFrame() {
        vkWaitForFences(m_device->device(), 1, &m_inFlightFences[m_currentFrameIdx], VK_TRUE, UINT64_MAX);
        const auto cmdBuffer = m_commandBuffers[m_currentFrameIdx];
        m_currentFrameContext = {cmdBuffer, m_currentFrameIdx};
        cmdBuffer->beginRecording();
        vkResetFences(m_device->device(), 1, &m_inFlightFences[m_currentFrameIdx]);
    }

    void VulkanCore::renderUi() {
        if (m_swapchain->extent().width == 0 || m_swapchain->extent().height == 0) {
            return;
        }

        const VkResult result = vkAcquireNextImageKHR(m_device->device(), m_swapchain->swapchain(), UINT64_MAX,
                                                      m_imageAvailableSemaphores[m_currentFrameIdx],
                                                      VK_NULL_HANDLE, &m_currentFrameContext.swapchainImgIdx);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            BESS_WARN("Swapchain out of date, skipping frame");
            return;
        }

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }

        const auto cmdBuffer = m_currentFrameContext.cmdBuffer;

        m_renderPass->begin(cmdBuffer->getVkHandle(), m_swapchain->framebuffers()[m_currentFrameContext.swapchainImgIdx], m_swapchain->extent());
        ImDrawData *drawData = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer->getVkHandle());

        m_renderPass->end();
    }

    void VulkanCore::endFrame() {
        m_currentFrameContext.cmdBuffer->endRecording();
        VkSubmitInfo submitInfo{};

        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        const std::array<VkSemaphore, 1> waitSemaphores{m_imageAvailableSemaphores[m_currentFrameIdx]};
        constexpr std::array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.data();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = m_currentFrameContext.cmdBuffer->getVkHandlePtr();

        const auto renderFinishedSemaphore = &m_renderFinishedSemaphores[m_currentFrameContext.swapchainImgIdx];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = renderFinishedSemaphore;

        if (vkQueueSubmit(m_device->graphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrameIdx]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = renderFinishedSemaphore;

        const std::array<VkSwapchainKHR, 1> swapChains{m_swapchain->swapchain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains.data();
        presentInfo.pImageIndices = &m_currentFrameContext.swapchainImgIdx;

        const auto result = vkQueuePresentKHR(m_device->presentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            BESS_WARN("Swapchain out of date during present, will handle next frame");
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image!");
        }

        m_currentFrameIdx = (m_currentFrameIdx + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void VulkanCore::recreateSwapchain() {
        vkDeviceWaitIdle(m_device->device());

        VkExtent2D newExtent = m_swapchain->extent();

        m_swapchain = std::make_shared<Vulkan::VulkanSwapchain>(m_vkInstance, m_device, m_renderSurface, newExtent);

        m_swapchain->createFramebuffers(m_renderPass->getVkHandle());
        m_swapchain->createFramebuffers(m_renderPass->getVkHandle());

        BESS_INFO("Swapchain recreated with new extent: {}x{}", newExtent.width, newExtent.height);
    }

    void VulkanCore::recreateSwapchain(VkExtent2D newExtent) {
        if (newExtent.width == 0 || newExtent.height == 0) {
            BESS_WARN("Window minimized, skipping swapchain recreation");
            return;
        }

        VkExtent2D currentExtent = m_swapchain->extent();
        if (newExtent.width == currentExtent.width && newExtent.height == currentExtent.height) {
            return;
        }

        vkDeviceWaitIdle(m_device->device());

        VkSwapchainKHR oldSwapchain = m_swapchain->swapchain();

        m_swapchain = std::make_shared<Vulkan::VulkanSwapchain>(m_vkInstance, m_device, m_renderSurface, newExtent, oldSwapchain);

        m_swapchain->createFramebuffers(m_renderPass->getVkHandle());
    }

    void VulkanCore::cleanup(const std::function<void()> &preCmdBufferCleanup) {
        BESS_INFO("[VulkanCore] Shutting down");
        if (!m_device || m_device->device() == VK_NULL_HANDLE)
            return;
        vkDeviceWaitIdle(m_device->device());

        if (m_device && m_device->device() != VK_NULL_HANDLE) {

            for (size_t i = 0; i < m_swapchain->imageCount(); i++) {
                if (m_inFlightFences[i] != VK_NULL_HANDLE) {
                    vkWaitForFences(m_device->device(), 1, &m_inFlightFences[i], VK_TRUE, UINT64_MAX);
                    vkResetFences(m_device->device(), 1, &m_inFlightFences[i]);
                }
            }

            vkDeviceWaitIdle(m_device->device());

            for (size_t i = 0; i < m_swapchain->imageCount(); i++) {
                if (m_renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
                    vkDestroySemaphore(m_device->device(), m_renderFinishedSemaphores[i], nullptr);
                }
                if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
                    vkDestroySemaphore(m_device->device(), m_imageAvailableSemaphores[i], nullptr);
                }
                if (m_inFlightFences[i] != VK_NULL_HANDLE) {
                    vkDestroyFence(m_device->device(), m_inFlightFences[i], nullptr);
                }
            }

            // Clean up picking resources while device is still valid
            cleanupPickingResources();
        }

        m_pipeline.reset();
        m_primitiveRenderer.reset();
        m_pathRenderer.reset();
        m_offscreenImageView.reset();
        m_offscreenRenderPass.reset();
        m_swapchain.reset();
        m_renderPass.reset();
        preCmdBufferCleanup();
        Vulkan::VulkanCommandBuffer::cleanCommandBuffers(m_device);
        m_device.reset();

        if (m_renderSurface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_vkInstance, m_renderSurface, nullptr);
        }

        destroyDebugMessenger();

        if (m_vkInstance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_vkInstance, nullptr);
        }
    }

    VkResult VulkanCore::initVkInstance(const std::vector<const char *> &winExtensions) {
        std::vector<const char *> extensions = winExtensions;
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        std::string extStr = "";
        for (const auto *const ext : extensions) {
            extStr += ext;
            extStr += " | ";
        }

        BESS_INFO("[Renderer] Initializing with extensions: {}", extStr);

        if (validateExtensions(extensions) != VK_SUCCESS) {
            throw std::runtime_error("[Renderer] Extension validation failed");
        }

        const std::vector<const char *> validationLayers = {
            "VK_LAYER_KHRONOS_validation"};

        if (validateLayers(validationLayers) != VK_SUCCESS) {
            throw std::runtime_error("[Renderer] Layer validation failed");
        }

        VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Some Application",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "BessRenderingEngine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_0,
        };

        auto debugMessengerCreateInfo = getDebugMessengerCreateInfo();

        const VkInstanceCreateInfo instanceInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugMessengerCreateInfo,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = (uint32_t)validationLayers.size(),
            .ppEnabledLayerNames = validationLayers.data(),
            .enabledExtensionCount = (uint32_t)extensions.size(),
            .ppEnabledExtensionNames = extensions.data(),
        };

        if (vkCreateInstance(&instanceInfo, nullptr, &m_vkInstance) != VK_SUCCESS) {
            throw std::runtime_error("Renderer: Failed to create vulkan instance");
        }

        return VK_SUCCESS;
    }

    VkResult VulkanCore::validateExtensions(const std::vector<const char *> &extensions) const {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());

        for (const auto &extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty() ? VK_SUCCESS : VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    VkResult VulkanCore::validateLayers(const std::vector<const char *> &layers) const {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : layers) {
            bool layerFound = false;

            for (const auto &layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return VK_ERROR_LAYER_NOT_PRESENT;
            }
        }

        return VK_SUCCESS;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData) {
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            BESS_WARN("[VulkanCore][ValidationLayer] {}", pCallbackData->pMessage);
        } else {
            BESS_ERROR("[VulkanCore][ValidationLayer] {}", pCallbackData->pMessage);
        }
        return VK_FALSE;
    }

    VkDebugUtilsMessengerCreateInfoEXT VulkanCore::getDebugMessengerCreateInfo() const {
        const VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debugCallback,
        };

        return debugMessengerCreateInfo;
    }

    VkResult VulkanCore::createDebugMessenger() {
#ifndef NDEBUG
        const auto createInfo = getDebugMessengerCreateInfo();

        const auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vkInstance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(m_vkInstance, &createInfo, nullptr, &m_vkDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
#else
        return VK_SUCCESS;
#endif
    }

    VkResult VulkanCore::destroyDebugMessenger() const {
#ifndef NDEBUG
        if (m_vkDebugMessenger != VK_NULL_HANDLE) {
            const auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vkInstance, "vkDestroyDebugUtilsMessengerEXT");
            if (func != nullptr) {
                func(m_vkInstance, m_vkDebugMessenger, nullptr);
            }
        }
#endif
        return VK_SUCCESS;
    }

    void VulkanCore::createSyncObjects() {
        // Create semaphores for each frame in fli
        const auto n = m_swapchain->imageCount();
        m_imageAvailableSemaphores.resize(n);
        m_renderFinishedSemaphores.resize(n);
        m_inFlightFences.resize(n);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < n; i++) {
            if (vkCreateSemaphore(m_device->device(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image available semaphore!");
            }
        }

        for (size_t i = 0; i < n; i++) {
            if (vkCreateSemaphore(m_device->device(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create render finished semaphore!");
            }

            if (vkCreateFence(m_device->device(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create in flight fence!");
            }
        }
    }

    uint64_t VulkanCore::getSceneTextureId() {
        if (m_offscreenImageView && m_offscreenImageView->getDescriptorSet() != VK_NULL_HANDLE) {
            BESS_TRACE("[VulkanCore] Offscreen image view handle was valid");
            return (uint64_t)m_offscreenImageView->getDescriptorSet();
        }
        BESS_WARN("[VulkanCore] Offscreen image view handle was not valid");
        return 0;
    }

    int32_t VulkanCore::readPickingId(int x, int y) {
        if (!m_offscreenImageView || !m_offscreenImageView->hasPickingAttachments()) {
            return -1;
        }
        m_pendingPickingX = x;
        m_pendingPickingY = y;
        m_pickingRequestPending = true;
        return -1; // result will be available next getPickingIdResult()
    }

    int32_t VulkanCore::getPickingIdResult() {
        if (m_pickingCopyInFlight) {
            VkFence fence = m_inFlightFences[m_pickingCopyRecordedFrameIdx];
            if (vkGetFenceStatus(m_device->device(), fence) == VK_SUCCESS) {
                void *data = nullptr;
                vkMapMemory(m_device->device(), m_pickingStagingBufferMemory, 0, sizeof(int32_t), 0, &data);
                m_pickingResult = *static_cast<int32_t *>(data);
                vkUnmapMemory(m_device->device(), m_pickingStagingBufferMemory);
                m_pickingCopyInFlight = false;
                m_pickingRequestPending = false;
            }
        }
        return m_pickingResult;
    }

    void VulkanCore::createPickingResources() {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(int);
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_pickingStagingBuffer) != VK_SUCCESS) {
            BESS_ERROR("Failed to create picking staging buffer");
            return;
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->device(), m_pickingStagingBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_pickingStagingBufferMemory) != VK_SUCCESS) {
            BESS_ERROR("Failed to allocate picking staging buffer memory");
            vkDestroyBuffer(m_device->device(), m_pickingStagingBuffer, nullptr);
            return;
        }

        vkBindBufferMemory(m_device->device(), m_pickingStagingBuffer, m_pickingStagingBufferMemory, 0);
    }

    void VulkanCore::cleanupPickingResources() {
        if (m_pickingStagingBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->device(), m_pickingStagingBuffer, nullptr);
            m_pickingStagingBuffer = VK_NULL_HANDLE;
        }

        if (m_pickingStagingBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->device(), m_pickingStagingBufferMemory, nullptr);
            m_pickingStagingBufferMemory = VK_NULL_HANDLE;
        }
    }

} // namespace Bess::Renderer2D
