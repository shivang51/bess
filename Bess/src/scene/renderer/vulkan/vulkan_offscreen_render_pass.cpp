#include "scene/renderer/vulkan/vulkan_offscreen_render_pass.h"
#include "glm.hpp"
#include <array>
#include <cassert>
#include <stdexcept>

namespace Bess::Renderer2D::Vulkan {
    VulkanOffscreenRenderPass::VulkanOffscreenRenderPass(const std::shared_ptr<VulkanDevice> &device, VkFormat colorFormat)
        : m_device(device), m_colorFormat(colorFormat) {
        createRenderPass();
    }

    VulkanOffscreenRenderPass::~VulkanOffscreenRenderPass() {
        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device->device(), m_renderPass, nullptr);
        }
    }

    VulkanOffscreenRenderPass::VulkanOffscreenRenderPass(VulkanOffscreenRenderPass &&other) noexcept
        : m_device(other.m_device),
          m_colorFormat(other.m_colorFormat),
          m_renderPass(other.m_renderPass) {
        other.m_renderPass = VK_NULL_HANDLE;
    }

    VulkanOffscreenRenderPass &VulkanOffscreenRenderPass::operator=(VulkanOffscreenRenderPass &&other) noexcept {
        if (this != &other) {
            if (m_renderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_device->device(), m_renderPass, nullptr);
            }

            m_colorFormat = other.m_colorFormat;
            m_renderPass = other.m_renderPass;

            other.m_renderPass = VK_NULL_HANDLE;
        }
        return *this;
    }

    void VulkanOffscreenRenderPass::begin(VkCommandBuffer cmdBuffer,
                                          VkFramebuffer framebuffer,
                                          VkExtent2D extent,
                                          const glm::vec4 &clearColor) {
        m_recordingCmdBuffer = cmdBuffer;

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.framebuffer = framebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;

        std::array<VkClearValue, 1> clearValues{};
        clearValues[0].color = {{clearColor.r, clearColor.g, clearColor.b, clearColor.a}};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(m_recordingCmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanOffscreenRenderPass::end() {
        assert(m_recordingCmdBuffer != VK_NULL_HANDLE);
        vkCmdEndRenderPass(m_recordingCmdBuffer);
        m_recordingCmdBuffer = VK_NULL_HANDLE;
    }

    void VulkanOffscreenRenderPass::createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_colorFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_device->device(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen render pass!");
        }
    }

} // namespace Bess::Renderer2D::Vulkan
