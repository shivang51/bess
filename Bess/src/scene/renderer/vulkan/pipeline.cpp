#include "scene/renderer/vulkan/pipeline.h"
#include "common/log.h"
#include "scene/renderer/vulkan/vulkan_render_pass.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace Bess::Renderer2D::Vulkan {

    VulkanPipeline::VulkanPipeline(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanSwapchain> swapchain)
        : m_device(device), m_swapchain(swapchain) {
    }

    VulkanPipeline::~VulkanPipeline() {
        if (m_graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device->device(), m_graphicsPipeline, nullptr);
        }
        if (m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device->device(), m_pipelineLayout, nullptr);
        }
    }

    VulkanPipeline::VulkanPipeline(VulkanPipeline &&other) noexcept
        : m_device(other.m_device),
          m_swapchain(other.m_swapchain),
          m_graphicsPipeline(other.m_graphicsPipeline),
          m_pipelineLayout(other.m_pipelineLayout) {
        other.m_graphicsPipeline = VK_NULL_HANDLE;
        other.m_pipelineLayout = VK_NULL_HANDLE;
    }

    VulkanPipeline &VulkanPipeline::operator=(VulkanPipeline &&other) noexcept {
        if (this != &other) {
            if (m_graphicsPipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_device->device(), m_graphicsPipeline, nullptr);
            }
            if (m_pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_device->device(), m_pipelineLayout, nullptr);
            }
            m_graphicsPipeline = other.m_graphicsPipeline;
            m_pipelineLayout = other.m_pipelineLayout;

            other.m_graphicsPipeline = VK_NULL_HANDLE;
            other.m_pipelineLayout = VK_NULL_HANDLE;
        }
        return *this;
    }

    void VulkanPipeline::createGraphicsPipeline(const std::string &vertShaderPath, const std::string &fragShaderPath, std::shared_ptr<VulkanRenderPass> renderPass) {
        auto vertShaderCode = readFile(vertShaderPath);
        auto fragShaderCode = readFile(fragShaderPath);

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{vertShaderStageInfo, fragShaderStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_swapchain->extent().width;
        viewport.height = (float)m_swapchain->extent().height;
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_swapchain->extent();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0F;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Fixed for proper font rendering
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Fixed for proper font rendering
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // Create pipeline layout without descriptor sets for basic rendering
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        if (vkCreatePipelineLayout(m_device->device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = renderPass->getVkHandle();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(m_device->device(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device->device(), vertShaderModule, nullptr);
    }

    VkShaderModule VulkanPipeline::createShaderModule(const std::vector<char> &code) const {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(m_device->device(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }

        return shaderModule;
    }

    std::vector<char> VulkanPipeline::readFile(const std::string &filename) const {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        const size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

} // namespace Bess::Renderer2D::Vulkan
