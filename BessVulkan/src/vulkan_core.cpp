#include "vulkan_core.h"
#include "log.h"
#include <cstring>
#include <memory>
#include <set>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan {
    VulkanCore::~VulkanCore() {
        cleanup();
    }

    bool VulkanCore::isInitialized = false;

    void VulkanCore::init(const std::vector<const char *> &winExt,
                          const SurfaceCreationCB &createSurface,
                          VkExtent2D windowExtent) {
        Logger::getInstance().initLogger("BessVulkan");
        if (isInitialized) {
            BESS_VK_WARN("Reinitialization of VulkaCore was called...skipping");
            return;
        }

        BESS_VK_INFO("Initializing VulkanCore");

        initVkInstance(winExt);
        createDebugMessenger();
        createSurface(m_vkInstance, m_renderSurface);
        BESS_VK_INFO("Created VkInstance and draw surface");

        m_device = std::make_shared<VulkanDevice>(m_vkInstance, m_renderSurface);
        m_swapchain = std::make_shared<VulkanSwapchain>(m_vkInstance, m_device, m_renderSurface, windowExtent);

        m_renderPass = std::make_shared<VulkanRenderPass>(m_device, m_swapchain->imageFormat(), VK_FORMAT_D32_SFLOAT);

        m_swapchain->createFramebuffers(m_renderPass->getVkHandle());

        m_commandBuffers = std::make_unique<VulkanCommandBuffers>(m_device, 2);
        createSyncObjects();

        isInitialized = true;
        BESS_VK_INFO("Renderer Initialized");
    }

    void VulkanCore::beginFrame() {
        if (m_currentFrameContext.isStarted) {
            BESS_VK_WARN("[VulkanCore] Frame is already started, skipping");
            return;
        }
        vkWaitForFences(m_device->device(), 1, &m_inFlightFences[m_currentFrameIdx], VK_TRUE, UINT64_MAX);
        const auto cmdBuffer = m_commandBuffers->at(m_currentFrameIdx);
        m_currentFrameContext = {cmdBuffer, m_currentFrameIdx, true};
        cmdBuffer->beginRecording();
        vkResetFences(m_device->device(), 1, &m_inFlightFences[m_currentFrameIdx]);
    }

    void VulkanCore::renderToSwapchain(const SwapchainRenderFn &fn) {
        if (m_swapchain->extent().width == 0 || m_swapchain->extent().height == 0) {
            return;
        }

        const VkResult result = vkAcquireNextImageKHR(m_device->device(), m_swapchain->swapchain(), UINT64_MAX,
                                                      m_imageAvailableSemaphores[m_currentFrameIdx],
                                                      VK_NULL_HANDLE, &m_currentFrameContext.swapchainImgIdx);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            BESS_VK_WARN("Swapchain out of date, skipping frame");
            return;
        }

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }

        const auto cmdBuffer = m_currentFrameContext.cmdBuffer;

        m_renderPass->begin(cmdBuffer->getVkHandle(), m_swapchain->framebuffers()[m_currentFrameContext.swapchainImgIdx], m_swapchain->extent());
        fn(cmdBuffer->getVkHandle());
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
            BESS_VK_WARN("Swapchain out of date during present, will handle next frame");
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image!");
        }

        m_currentFrameIdx = (m_currentFrameIdx + 1) % MAX_FRAMES_IN_FLIGHT;
        m_currentFrameContext.isStarted = false;
    }

    void VulkanCore::recreateSwapchain(VkExtent2D newExtent) {
        if (newExtent.width == 0 || newExtent.height == 0) {
            BESS_VK_WARN("Window minimized, skipping swapchain recreation");
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
        BESS_VK_INFO("[VulkanCore] Shutting down");
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
        }

        m_swapchain.reset();
        m_renderPass.reset();
        preCmdBufferCleanup();
        m_commandBuffers.reset();
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
        std::string extStr;
        for (const auto *const ext : extensions) {
            extStr += ext;
            extStr += " | ";
        }

        BESS_VK_INFO("[Renderer] Initializing with extensions: {}", extStr);

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
            .pApplicationName = "Bess",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "BessRenderingEngine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_0,
        };

        auto debugMessengerCreateInfo = getDebugMessengerCreateInfo();

        const VkInstanceCreateInfo instanceInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = &debugMessengerCreateInfo,
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
        uint32_t layerCount = 0;
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
            BESS_VK_WARN("[VulkanCore][ValidationLayer] {}", pCallbackData->pMessage);
        } else {
            BESS_VK_ERROR("[VulkanCore][ValidationLayer] {}", pCallbackData->pMessage);
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

    VulkanCore &VulkanCore::instance() {
        static VulkanCore inst;
        return inst;
    }
} // namespace Bess::Vulkan
