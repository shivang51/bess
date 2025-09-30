#include "scene/renderer/vulkan/vulkan_shader.h"
#include "scene/renderer/vulkan/device.h"
#include <fstream>
#include <stdexcept>

namespace Bess::Renderer2D::Vulkan {

    VulkanShader::VulkanShader(VulkanDevice &device, const std::string &vertPath, const std::string &fragPath)
        : m_device(device) {
        const auto vertCode = readFile(vertPath);
        const auto fragCode = readFile(fragPath);

        m_vertexModule = createShaderModule(vertCode);
        m_fragmentModule = createShaderModule(fragCode);
    }

    VulkanShader::~VulkanShader() {
        if (m_vertexModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device.device(), m_vertexModule, nullptr);
        }
        if (m_fragmentModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device.device(), m_fragmentModule, nullptr);
        }
    }

    VulkanShader::VulkanShader(VulkanShader &&other) noexcept
        : m_device(other.m_device),
          m_vertexModule(other.m_vertexModule),
          m_fragmentModule(other.m_fragmentModule) {
        other.m_vertexModule = VK_NULL_HANDLE;
        other.m_fragmentModule = VK_NULL_HANDLE;
    }

    VulkanShader &VulkanShader::operator=(VulkanShader &&other) noexcept {
        if (this != &other) {
            if (m_vertexModule != VK_NULL_HANDLE) {
                vkDestroyShaderModule(m_device.device(), m_vertexModule, nullptr);
            }
            if (m_fragmentModule != VK_NULL_HANDLE) {
                vkDestroyShaderModule(m_device.device(), m_fragmentModule, nullptr);
            }

            m_vertexModule = other.m_vertexModule;
            m_fragmentModule = other.m_fragmentModule;

            other.m_vertexModule = VK_NULL_HANDLE;
            other.m_fragmentModule = VK_NULL_HANDLE;
        }
        return *this;
    }

    VkShaderModule VulkanShader::createShaderModule(const std::vector<char> &code) const {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(m_device.device(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }

        return shaderModule;
    }

    std::vector<char> VulkanShader::readFile(const std::string &filename) const {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        const size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

        file.close();

        return buffer;
    }

} // namespace Bess::Renderer2D::Vulkan
