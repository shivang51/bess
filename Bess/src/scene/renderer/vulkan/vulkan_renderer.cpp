#include "scene/renderer/vulkan/vulkan_renderer.h"
#include "camera.h"
#include "common/log.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <memory>
#include <set>
#include <stdexcept>

namespace Bess::Renderer2D {
    VulkanRenderer::~VulkanRenderer() {
        cleanup();
    }

    VulkanRenderer::VulkanRenderer(VulkanRenderer &&other) noexcept
        : m_vkInstance(other.m_vkInstance),
          m_vkDebugMessenger(other.m_vkDebugMessenger),
          m_device(std::move(other.m_device)),
          m_swapchain(std::move(other.m_swapchain)),
          m_pipeline(std::move(other.m_pipeline)),
          m_commandBuffer(std::move(other.m_commandBuffer)),
          m_renderPass(std::move(other.m_renderPass)),
          m_imageAvailableSemaphores(std::move(other.m_imageAvailableSemaphores)),
          m_renderFinishedSemaphores(std::move(other.m_renderFinishedSemaphores)),
          m_inFlightFences(std::move(other.m_inFlightFences)),
          m_currentFrame(other.m_currentFrame){
        other.m_vkInstance = VK_NULL_HANDLE;
        other.m_vkDebugMessenger = VK_NULL_HANDLE;
        other.m_renderSurface = VK_NULL_HANDLE;
    }

    VulkanRenderer &VulkanRenderer::operator=(VulkanRenderer &&other) noexcept {
        if (this != &other) {
            cleanup();

            m_vkInstance = other.m_vkInstance;
            m_vkDebugMessenger = other.m_vkDebugMessenger;
            m_device = std::move(other.m_device);
            m_swapchain = std::move(other.m_swapchain);
            m_pipeline = std::move(other.m_pipeline);
            m_commandBuffer = std::move(other.m_commandBuffer);
            m_renderPass = std::move(other.m_renderPass);
            m_renderSurface = other.m_renderSurface;
            m_imageAvailableSemaphores = std::move(other.m_imageAvailableSemaphores);
            m_renderFinishedSemaphores = std::move(other.m_renderFinishedSemaphores);
            m_inFlightFences = std::move(other.m_inFlightFences);
            m_currentFrame = other.m_currentFrame;

            other.m_vkInstance = VK_NULL_HANDLE;
            other.m_vkDebugMessenger = VK_NULL_HANDLE;
            other.m_renderSurface = VK_NULL_HANDLE;
        }
        return *this;
    }

    bool VulkanRenderer::isInitialized = false;

    void VulkanRenderer::init(const std::vector<const char *> &winExt,
                              const SurfaceCreationCB &createSurface,
                              VkExtent2D windowExtent,
                              const std::string &vertShaderPath,
                              const std::string &fragShaderPath) {
        if (isInitialized) {
            BESS_WARN("Reintialization of renderer was called...skipping");
            return;
        }

        BESS_INFO("Initializing Renderer");
        if (initVkInstance(winExt) != VK_SUCCESS) {
            BESS_ERROR("Failed to created VkInstance");
            assert(false);
        }
        createDebugMessenger();
        createSurface(m_vkInstance, m_renderSurface);
        BESS_INFO("Created VkInstance and draw surface");

        m_device = std::make_shared<Vulkan::VulkanDevice>(m_vkInstance, m_renderSurface);
        m_swapchain = std::make_shared<Vulkan::VulkanSwapchain>(m_vkInstance, m_device, m_renderSurface, windowExtent);


        m_pipeline = std::make_shared<Vulkan::VulkanPipeline>(m_device, m_swapchain);
        m_pipeline->createGraphicsPipeline(vertShaderPath, fragShaderPath);

        m_imguiPipeline = std::make_shared<Vulkan::ImGuiPipeline>(m_device, m_swapchain);
        
        m_swapchain->createFramebuffers(m_pipeline->renderPass());

        m_renderPass = std::make_shared<Vulkan::VulkanRenderPass>(m_device, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_D32_SFLOAT);

        m_commandBuffer = std::make_shared<Vulkan::VulkanCommandBuffer>(m_device);
        createSyncObjects();

        isInitialized = true;
        BESS_INFO("Renderer Initialized");
    }

    void VulkanRenderer::draw() {
        // Skip rendering if window is minimized
        if (m_swapchain->extent().width == 0 || m_swapchain->extent().height == 0) {
            return;
        }
        
        vkWaitForFences(m_device->device(), 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex = 0;
        VkResult result = vkAcquireNextImageKHR(m_device->device(), m_swapchain->swapchain(), UINT64_MAX,
                                                m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            // Handle swapchain recreation - for now, just return and let the next frame handle it
            BESS_WARN("Swapchain out of date, skipping frame");
            return;
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }

        vkResetFences(m_device->device(), 1, &m_inFlightFences[m_currentFrame]);

        vkResetCommandBuffer(m_commandBuffer->commandBuffers()[m_currentFrame], 0);
        m_commandBuffer->recordCommandBuffer(m_commandBuffer->commandBuffers()[m_currentFrame], imageIndex,
                                             m_imguiPipeline->renderPass(), m_swapchain->framebuffers()[imageIndex],
                                             m_swapchain->extent(), m_imguiPipeline->pipelineLayout());

        // ImGui rendering is now handled within the command buffer recording

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        const std::array<VkSemaphore, 1> waitSemaphores{m_imageAvailableSemaphores[m_currentFrame]};
        const std::array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.data();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffer->commandBuffers()[m_currentFrame];

        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = nullptr;

        if (vkQueueSubmit(m_device->graphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 0;
        presentInfo.pWaitSemaphores = nullptr;

        const std::array<VkSwapchainKHR, 1> swapChains{m_swapchain->swapchain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains.data();
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(m_device->presentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            // Handle swapchain recreation - for now, just log and continue
            BESS_WARN("Swapchain out of date during present, will handle next frame");
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image!");
        }

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void VulkanRenderer::recreateSwapchain() {
        // Wait for device to be idle before recreating swapchain
        vkDeviceWaitIdle(m_device->device());
        
        // Get new window extent from the current swapchain
        VkExtent2D newExtent = m_swapchain->extent();
        
        // Recreate swapchain
        m_swapchain = std::make_shared<Vulkan::VulkanSwapchain>(m_vkInstance, m_device, m_renderSurface, newExtent);
        
        // Recreate framebuffers
        m_swapchain->createFramebuffers(m_pipeline->renderPass());
        m_swapchain->createFramebuffers(m_imguiPipeline->renderPass());

        BESS_INFO("Swapchain recreated with new extent: {}x{}", newExtent.width, newExtent.height);
    }

    void VulkanRenderer::recreateSwapchain(VkExtent2D newExtent) {
        // Don't recreate swapchain if window is minimized (extent = 0)
        if (newExtent.width == 0 || newExtent.height == 0) {
            BESS_WARN("Window minimized, skipping swapchain recreation");
            return;
        }
        
        // Check if the extent is actually different from current swapchain
        VkExtent2D currentExtent = m_swapchain->extent();
        if (newExtent.width == currentExtent.width && newExtent.height == currentExtent.height) {
            BESS_INFO("Swapchain extent unchanged, skipping recreation");
            return;
        }
        
        // Always recreate swapchain when extent changes
        BESS_INFO("Recreating swapchain due to extent change ({}x{} -> {}x{})", 
                 currentExtent.width, currentExtent.height, newExtent.width, newExtent.height);
        
        // Wait for device to be idle before recreating swapchain
        vkDeviceWaitIdle(m_device->device());
        
        // Store old swapchain for proper recreation
        VkSwapchainKHR oldSwapchain = m_swapchain->swapchain();
        
        // Recreate swapchain with new extent
        m_swapchain = std::make_shared<Vulkan::VulkanSwapchain>(m_vkInstance, m_device, m_renderSurface, newExtent, oldSwapchain);
        
        // Recreate framebuffers
        m_swapchain->createFramebuffers(m_imguiPipeline->renderPass());
        
        BESS_INFO("Swapchain recreated with new extent: {}x{}", newExtent.width, newExtent.height);
    }

    void VulkanRenderer::cleanup() {
        if (m_device && m_device->device() != VK_NULL_HANDLE) {
            // Wait for all operations to complete before cleanup
            vkDeviceWaitIdle(m_device->device());
            
            // Additional wait to ensure all command buffers are finished
            if (m_commandBuffer) {
                // Wait for any pending command buffer operations
                for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                    if (m_inFlightFences[i] != VK_NULL_HANDLE) {
                        vkWaitForFences(m_device->device(), 1, &m_inFlightFences[i], VK_TRUE, UINT64_MAX);
                        vkResetFences(m_device->device(), 1, &m_inFlightFences[i]);
                    }
                }
            }
            
            // Wait again after resetting fences to ensure complete synchronization
            vkDeviceWaitIdle(m_device->device());
        }

        // Destroy semaphores and fences before resetting device
        if (m_device && m_device->device() != VK_NULL_HANDLE) {
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
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

        m_renderPass.reset();
        m_commandBuffer.reset();
        m_imguiPipeline.reset(); // Destroy ImGui pipeline before device
        m_pipeline.reset();
        m_swapchain.reset();
        m_device.reset();

        if (m_renderSurface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_vkInstance, m_renderSurface, nullptr);
        }

        destroyDebugMessenger();

        if (m_vkInstance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_vkInstance, nullptr);
        }
    }

    VkResult VulkanRenderer::initVkInstance(const std::vector<const char *> &winExtensions) {
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
            // "VK_LAYER_KHRONOS_validation", // Temporarily disabled for testing
        };

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

    VkResult VulkanRenderer::validateExtensions(const std::vector<const char *> &extensions) const {
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

    VkResult VulkanRenderer::validateLayers(const std::vector<const char *> &layers) const {
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
        std::cerr << "[Renderer][ValidationLayer] " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    VkDebugUtilsMessengerCreateInfoEXT VulkanRenderer::getDebugMessengerCreateInfo() const {
        const VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debugCallback,
        };

        return debugMessengerCreateInfo;
    }

    VkResult VulkanRenderer::createDebugMessenger() {
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

    VkResult VulkanRenderer::destroyDebugMessenger() const {
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

    void VulkanRenderer::createSyncObjects() {
        // Create semaphores for each frame in flight
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(m_device->device(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image available semaphore!");
            }
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(m_device->device(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create render finished semaphore!");
            }

            if (vkCreateFence(m_device->device(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create in flight fence!");
            }
        }
    }

    void VulkanRenderer::begin(const std::shared_ptr<Bess::Camera> &camera) {
        // TODO: Implement camera setup
    }

    void VulkanRenderer::end() {
        // Don't automatically draw - this will be called explicitly from the application loop
        // instance().draw();
    }

    void VulkanRenderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                              const glm::vec4 &color, int id, QuadRenderProperties properties) {
        // TODO: Implement quad rendering
    }

    void VulkanRenderer::circle(const glm::vec3 &center, float radius,
                                const glm::vec4 &color, int id, float innerRadius) {
        // TODO: Implement circle rendering
    }

    void VulkanRenderer::text(const std::string &data, const glm::vec3 &pos, size_t size,
                              const glm::vec4 &color, int id, float angle) {
        // TODO: Implement text rendering
    }

    void VulkanRenderer::line(const glm::vec3 &start, const glm::vec3 &end, float size,
                              const glm::vec4 &color, int id) {
        // TODO: Implement line rendering
    }

    void VulkanRenderer::msdfText(const std::string &data, const glm::vec3 &pos, size_t size,
                                  const glm::vec4 &color, int id, float angle) {
        // TODO: Implement MSDF text rendering
    }

    void VulkanRenderer::grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridColors &colors) {
        // TODO: Implement grid rendering
    }

    void VulkanRenderer::beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, uint64_t id) {
        // TODO: Implement path mode
    }

    void VulkanRenderer::endPathMode(bool closePath, bool genFill, const glm::vec4 &fillColor, bool genStroke) {
        // TODO: Implement path mode
    }

    void VulkanRenderer::pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, int id) {
        // TODO: Implement path line
    }

    void VulkanRenderer::pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                           float weight, const glm::vec4 &color, int id) {
        // TODO: Implement cubic bezier path
    }

    void VulkanRenderer::pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, int id) {
        // TODO: Implement quadratic bezier path
    }

    glm::vec2 VulkanRenderer::getCharRenderSize(char ch, float renderSize) {
        // TODO: Implement character size calculation
        return glm::vec2(0.0f);
    }

    glm::vec2 VulkanRenderer::getTextRenderSize(const std::string &str, float renderSize) {
        // TODO: Implement text size calculation
        return glm::vec2(0.0f);
    }

    glm::vec2 VulkanRenderer::getMSDFTextRenderSize(const std::string &str, float renderSize) {
        // TODO: Implement MSDF text size calculation
        return glm::vec2(0.0f);
    }

    uint64_t VulkanRenderer::getSceneTextureId() {
        // if (s_instance && s_instance->m_sceneFramebuffer) {
        //     // Return the color image view as a texture ID for ImGui
        //     return reinterpret_cast<uint64_t>(s_instance->m_sceneFramebuffer->colorImageView());
        // }
        return 0;
    }
} // namespace Bess::Renderer2D
