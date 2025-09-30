#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>

namespace Bess::Renderer2D::Vulkan {

    class VulkanDevice;

    class VulkanShader {
    public:
        VulkanShader(VulkanDevice& device, const std::string& vertPath, const std::string& fragPath);
        ~VulkanShader();

        // Delete copy constructor and assignment operator
        VulkanShader(const VulkanShader&) = delete;
        VulkanShader& operator=(const VulkanShader&) = delete;

        // Move constructor and assignment operator
        VulkanShader(VulkanShader&& other) noexcept;
        VulkanShader& operator=(VulkanShader&& other) noexcept;

        VkShaderModule getVertexModule() const { return m_vertexModule; }
        VkShaderModule getFragmentModule() const { return m_fragmentModule; }

    private:
        VulkanDevice& m_device;
        VkShaderModule m_vertexModule = VK_NULL_HANDLE;
        VkShaderModule m_fragmentModule = VK_NULL_HANDLE;

        VkShaderModule createShaderModule(const std::vector<char>& code) const;
        std::vector<char> readFile(const std::string& filename) const;
    };

} // namespace Bess::Renderer2D::Vulkan
