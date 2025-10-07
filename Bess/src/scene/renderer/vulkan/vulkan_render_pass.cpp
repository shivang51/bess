#include "scene/renderer/vulkan/vulkan_render_pass.h"
#include <array>
#include <cassert>
#include <stdexcept>

namespace Bess::Renderer2D::Vulkan {
    VulkanRenderPass::VulkanRenderPass(const std::shared_ptr<VulkanDevice> &device, const VkFormat colorFormat, const VkFormat depthFormat)
        : m_device(device), m_colorFormat(colorFormat), m_depthFormat(depthFormat) {
        createRenderPass();
    }

    VulkanRenderPass::~VulkanRenderPass() {
        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device->device(), m_renderPass, nullptr);
        }
    }

    VulkanRenderPass::VulkanRenderPass(VulkanRenderPass &&other) noexcept
        : m_device(other.m_device),
          m_colorFormat(other.m_colorFormat),
          m_depthFormat(other.m_depthFormat),
          m_renderPass(other.m_renderPass) {
        other.m_renderPass = VK_NULL_HANDLE;
    }

    VulkanRenderPass &VulkanRenderPass::operator=(VulkanRenderPass &&other) noexcept {
        if (this != &other) {
            if (m_renderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_device->device(), m_renderPass, nullptr);
            }

            m_colorFormat = other.m_colorFormat;
            m_depthFormat = other.m_depthFormat;
            m_renderPass = other.m_renderPass;

            other.m_renderPass = VK_NULL_HANDLE;
        }
        return *this;
    }

    void VulkanRenderPass::begin(VkCommandBuffer cmdBuffer, const VkFramebuffer framebuffer, const VkExtent2D extent) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.framebuffer = framebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;

        constexpr VkClearValue clearColor = {{{0.0F, 0.0F, 0.0F, 1.0F}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        m_recordingCmdBuffer = cmdBuffer;
    }

    void VulkanRenderPass::end() {
        assert(m_recordingCmdBuffer != VK_NULL_HANDLE);
        vkCmdEndRenderPass(m_recordingCmdBuffer);
        m_recordingCmdBuffer = VK_NULL_HANDLE;
    }

    void VulkanRenderPass::createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_colorFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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
        renderPassInfo.attachmentCount = static_cast<uint32_t>(1);
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_device->device(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }
    }
} // namespace Bess::Renderer2D::Vulkan
