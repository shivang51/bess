#include "scene/renderer/vulkan/vulkan_core.h"
#include "camera.h"
#include "common/log.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "ui/ui.h"
#include <memory>
#include <set>
#include <stdexcept>

namespace Bess::Renderer2D {
    VulkanCore::~VulkanCore() {
        cleanup();
    }

    bool VulkanCore::isInitialized = false;

    void VulkanCore::init(const std::vector<const char *> &winExt,
                          const SurfaceCreationCB &createSurface,
                          VkExtent2D windowExtent) {
        if (isInitialized) {
            BESS_WARN("Reinitialization of renderer was called...skipping");
            return;
        }

        BESS_INFO("Initializing Renderer");

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
            windowExtent,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        m_offscreenRenderPass = std::make_shared<Vulkan::VulkanOffscreenRenderPass>(m_device, m_swapchain->imageFormat());
        m_offscreenImageView->createFramebuffer(m_offscreenRenderPass->getVkHandle());

        // m_pipeline = std::make_shared<Vulkan::VulkanPipeline>(m_device, m_swapchain);
        // m_pipeline->createGraphicsPipeline(vertShaderPath, fragShaderPath, m_renderPass);

        m_swapchain->createFramebuffers(m_renderPass->getVkHandle());

        m_commandBuffers = Vulkan::VulkanCommandBuffer::createCommandBuffers(m_device, 2);
        createSyncObjects();

        isInitialized = true;
        BESS_INFO("Renderer Initialized");
    }

    void VulkanCore::draw() {
        if (!m_offscreenImageView || !m_offscreenRenderPass) {
            return;
        }

        const auto cmdBuffer = m_currentFrameContext.cmdBuffer;

        m_offscreenRenderPass->begin(
            cmdBuffer->getVkHandle(),
            m_offscreenImageView->getFramebuffer(),
            m_offscreenImageView->getExtent(),
            glm::vec4(1.0F, 0.0F, 1.0F, 1.0F) // Pink clear color
        );

        m_offscreenRenderPass->end();
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
            BESS_INFO("Swapchain extent unchanged, skipping recreation");
            return;
        }

        BESS_INFO("Recreating swapchain due to extent change ({}x{} -> {}x{})",
                  currentExtent.width, currentExtent.height, newExtent.width, newExtent.height);

        vkDeviceWaitIdle(m_device->device());

        VkSwapchainKHR oldSwapchain = m_swapchain->swapchain();

        m_swapchain = std::make_shared<Vulkan::VulkanSwapchain>(m_vkInstance, m_device, m_renderSurface, newExtent, oldSwapchain);

        m_swapchain->createFramebuffers(m_renderPass->getVkHandle());

        BESS_INFO("Swapchain recreated with new extent: {}x{}", newExtent.width, newExtent.height);
    }

    void VulkanCore::cleanup() {
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

        m_pipeline.reset();
        m_offscreenImageView.reset();
        m_offscreenRenderPass.reset();
        m_renderPass.reset();
        m_swapchain.reset();
        UI::vulkanCleanup(m_device);
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
        BESS_ERROR("[VulkanCore][ValidationLayer] {}", pCallbackData->pMessage);
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

    void VulkanCore::begin(const std::shared_ptr<Bess::Camera> &camera) {
        // TODO: Implement camera setup
    }

    void VulkanCore::end() {
        // Don't automatically draw - this will be called explicitly from the application loop
        // instance().draw();
    }

    void VulkanCore::quad(const glm::vec3 &pos, const glm::vec2 &size,
                          const glm::vec4 &color, int id, QuadRenderProperties properties) {
        // TODO: Implement quad rendering
    }

    void VulkanCore::circle(const glm::vec3 &center, float radius,
                            const glm::vec4 &color, int id, float innerRadius) {
        // TODO: Implement circle rendering
    }

    void VulkanCore::text(const std::string &data, const glm::vec3 &pos, size_t size,
                          const glm::vec4 &color, int id, float angle) {
        // TODO: Implement text rendering
    }

    void VulkanCore::line(const glm::vec3 &start, const glm::vec3 &end, float size,
                          const glm::vec4 &color, int id) {
        // TODO: Implement line rendering
    }

    void VulkanCore::msdfText(const std::string &data, const glm::vec3 &pos, size_t size,
                              const glm::vec4 &color, int id, float angle) {
        // TODO: Implement MSDF text rendering
    }

    void VulkanCore::grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridColors &colors) {
        // TODO: Implement grid rendering
    }

    void VulkanCore::beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, uint64_t id) {
        // TODO: Implement path mode
    }

    void VulkanCore::endPathMode(bool closePath, bool genFill, const glm::vec4 &fillColor, bool genStroke) {
        // TODO: Implement path mode
    }

    void VulkanCore::pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, int id) {
        // TODO: Implement path line
    }

    void VulkanCore::pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                       float weight, const glm::vec4 &color, int id) {
        // TODO: Implement cubic bezier path
    }

    void VulkanCore::pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, int id) {
        // TODO: Implement quadratic bezier path
    }

    glm::vec2 VulkanCore::getCharRenderSize(char ch, float renderSize) {
        // TODO: Implement character size calculation
        return glm::vec2(0.0f);
    }

    glm::vec2 VulkanCore::getTextRenderSize(const std::string &str, float renderSize) {
        // TODO: Implement text size calculation
        return glm::vec2(0.0f);
    }

    glm::vec2 VulkanCore::getMSDFTextRenderSize(const std::string &str, float renderSize) {
        // TODO: Implement MSDF text size calculation
        return glm::vec2(0.0f);
    }

} // namespace Bess::Renderer2D
