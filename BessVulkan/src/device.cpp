#include "device.h"
#include "log.h"
#include <set>
#include <stdexcept>

namespace Bess::Vulkan {

    VulkanDevice::VulkanDevice(const VkInstance instance, const VkSurfaceKHR surface)
        : m_instance(instance), m_surface(surface) {
        pickPhysicalDevice();
        createLogicalDevice();
    }

    VulkanDevice::~VulkanDevice() {
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_vkDevice, m_commandPool, nullptr);
        }
        if (m_vkDevice != VK_NULL_HANDLE) {
            vkDestroyDevice(m_vkDevice, nullptr);
        }
    }

    VulkanDevice::VulkanDevice(VulkanDevice &&other) noexcept
        : m_instance(other.m_instance),
          m_surface(other.m_surface),
          m_vkPhysicalDevice(other.m_vkPhysicalDevice),
          m_vkDevice(other.m_vkDevice),
          m_queueFamilyIndices(other.m_queueFamilyIndices),
          m_graphicsQueue(other.m_graphicsQueue),
          m_presentQueue(other.m_presentQueue),
          m_commandPool(other.m_commandPool) {
        other.m_vkDevice = VK_NULL_HANDLE;
        other.m_vkPhysicalDevice = VK_NULL_HANDLE;
        other.m_graphicsQueue = VK_NULL_HANDLE;
        other.m_presentQueue = VK_NULL_HANDLE;
        other.m_commandPool = VK_NULL_HANDLE;
    }

    VulkanDevice &VulkanDevice::operator=(VulkanDevice &&other) noexcept {
        if (this != &other) {
            if (m_vkDevice != VK_NULL_HANDLE) {
                vkDestroyDevice(m_vkDevice, nullptr);
            }

            m_instance = other.m_instance;
            m_surface = other.m_surface;
            m_vkPhysicalDevice = other.m_vkPhysicalDevice;
            m_vkDevice = other.m_vkDevice;
            m_queueFamilyIndices = other.m_queueFamilyIndices;
            m_graphicsQueue = other.m_graphicsQueue;
            m_presentQueue = other.m_presentQueue;
            m_commandPool = other.m_commandPool;

            other.m_vkDevice = VK_NULL_HANDLE;
            other.m_vkPhysicalDevice = VK_NULL_HANDLE;
            other.m_graphicsQueue = VK_NULL_HANDLE;
            other.m_presentQueue = VK_NULL_HANDLE;
            other.m_commandPool = VK_NULL_HANDLE;
        }
        return *this;
    }

    void VulkanDevice::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        for (const auto &device : devices) {
            if (!isDeviceSuitable(device))
                continue;

            m_vkPhysicalDevice = device;
            break;
        }

        if (m_vkPhysicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(m_vkPhysicalDevice, &deviceProperties);
        BESS_VK_INFO("[Renderer] Selected GPU: {}", deviceProperties.deviceName);
    }

    void VulkanDevice::createLogicalDevice() {
        m_queueFamilyIndices = findQueueFamilies(m_vkPhysicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {m_queueFamilyIndices.graphicsFamily.value(), m_queueFamilyIndices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures supportedFeatures{};
        vkGetPhysicalDeviceFeatures(m_vkPhysicalDevice, &supportedFeatures);

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.independentBlend = supportedFeatures.independentBlend; // needed to blend color but not picking

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;

        const std::vector<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        const std::vector<const char *> validationLayers = {
            "VK_LAYER_KHRONOS_validation"};
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        if (vkCreateDevice(m_vkPhysicalDevice, &createInfo, nullptr, &m_vkDevice) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }

        vkGetDeviceQueue(m_vkDevice, m_queueFamilyIndices.graphicsFamily.value(), 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_vkDevice, m_queueFamilyIndices.presentFamily.value(), 0, &m_presentQueue);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(m_vkDevice, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }
    }

    bool VulkanDevice::isDeviceSuitable(const VkPhysicalDevice device) const {
        const QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = false;
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        for (const auto &extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        extensionsSupported = requiredExtensions.empty();

        return indices.isComplete() && extensionsSupported;
    }

    QueueFamilyIndices VulkanDevice::findQueueFamilies(const VkPhysicalDevice device) const {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    void VulkanDevice::submitCmdBuffers(const std::vector<VkCommandBuffer> &cmdBuffer, VkFence fence) {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = cmdBuffer.size();
        submitInfo.pCommandBuffers = cmdBuffer.data();
        if (vkQueueSubmit(m_graphicsQueue, cmdBuffer.size(), &submitInfo, fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }
    }

    VkCommandBuffer VulkanDevice::beginSingleTimeCommands() const {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_vkDevice, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void VulkanDevice::endSingleTimeCommands(const VkCommandBuffer commandBuffer) const {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_graphicsQueue);

        vkFreeCommandBuffers(m_vkDevice, m_commandPool, 1, &commandBuffer);
    }

    uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_vkPhysicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

    void VulkanDevice::waitForIdle() {
        vkDeviceWaitIdle(m_vkDevice);
    }
} // namespace Bess::Vulkan
