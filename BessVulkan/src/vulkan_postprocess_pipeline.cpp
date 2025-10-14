#include "vulkan_postprocess_pipeline.h"
#include "vulkan_shader.h"
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan {

    VulkanPostprocessPipeline::VulkanPostprocessPipeline(const std::shared_ptr<VulkanDevice> &device, VkFormat colorFormat)
        : m_device(device), m_colorFormat(colorFormat) {
        createRenderPass();
        createDescriptorSetLayout();
        createPipeline();
    }

    VulkanPostprocessPipeline::~VulkanPostprocessPipeline() {
        if (m_device && m_device->device() != VK_NULL_HANDLE) {
            if (m_descriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(m_device->device(), m_descriptorPool, nullptr);
            }
            if (m_descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_device->device(), m_descriptorSetLayout, nullptr);
            }
            if (m_pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_device->device(), m_pipeline, nullptr);
            }
            if (m_pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_device->device(), m_pipelineLayout, nullptr);
            }
            if (m_renderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_device->device(), m_renderPass, nullptr);
            }
        }
    }

    VulkanPostprocessPipeline::VulkanPostprocessPipeline(VulkanPostprocessPipeline &&other) noexcept
        : m_device(std::move(other.m_device)),
          m_colorFormat(other.m_colorFormat),
          m_renderPass(other.m_renderPass),
          m_descriptorSetLayout(other.m_descriptorSetLayout),
          m_pipelineLayout(other.m_pipelineLayout),
          m_pipeline(other.m_pipeline),
          m_descriptorPool(other.m_descriptorPool),
          m_descriptorSet(other.m_descriptorSet) {
        other.m_renderPass = VK_NULL_HANDLE;
        other.m_descriptorSetLayout = VK_NULL_HANDLE;
        other.m_pipelineLayout = VK_NULL_HANDLE;
        other.m_pipeline = VK_NULL_HANDLE;
        other.m_descriptorPool = VK_NULL_HANDLE;
        other.m_descriptorSet = VK_NULL_HANDLE;
    }

    VulkanPostprocessPipeline &VulkanPostprocessPipeline::operator=(VulkanPostprocessPipeline &&other) noexcept {
        if (this != &other) {
            if (m_device && m_device->device() != VK_NULL_HANDLE) {
                if (m_descriptorPool != VK_NULL_HANDLE) {
                    vkDestroyDescriptorPool(m_device->device(), m_descriptorPool, nullptr);
                }
                if (m_descriptorSetLayout != VK_NULL_HANDLE) {
                    vkDestroyDescriptorSetLayout(m_device->device(), m_descriptorSetLayout, nullptr);
                }
                if (m_pipeline != VK_NULL_HANDLE) {
                    vkDestroyPipeline(m_device->device(), m_pipeline, nullptr);
                }
                if (m_pipelineLayout != VK_NULL_HANDLE) {
                    vkDestroyPipelineLayout(m_device->device(), m_pipelineLayout, nullptr);
                }
                if (m_renderPass != VK_NULL_HANDLE) {
                    vkDestroyRenderPass(m_device->device(), m_renderPass, nullptr);
                }
            }

            m_device = other.m_device;
            m_colorFormat = other.m_colorFormat;
            m_renderPass = other.m_renderPass;
            m_descriptorSetLayout = other.m_descriptorSetLayout;
            m_pipelineLayout = other.m_pipelineLayout;
            m_pipeline = other.m_pipeline;
            m_descriptorPool = other.m_descriptorPool;
            m_descriptorSet = other.m_descriptorSet;

            other.m_renderPass = VK_NULL_HANDLE;
            other.m_descriptorSetLayout = VK_NULL_HANDLE;
            other.m_pipelineLayout = VK_NULL_HANDLE;
            other.m_pipeline = VK_NULL_HANDLE;
            other.m_descriptorPool = VK_NULL_HANDLE;
            other.m_descriptorSet = VK_NULL_HANDLE;
        }
        return *this;
    }

    void VulkanPostprocessPipeline::createRenderPass() {
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
            throw std::runtime_error("Failed to create postprocess render pass!");
        }
    }

    void VulkanPostprocessPipeline::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 0;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &samplerLayoutBinding;

        if (vkCreateDescriptorSetLayout(m_device->device(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create postprocess descriptor set layout!");
        }
    }

    void VulkanPostprocessPipeline::createPipeline() {
        // Load shaders
        auto vertShaderCode = readFile("assets/shaders/postprocess.vert.spv");
        auto fragShaderCode = readFile("assets/shaders/unpremultiply.frag.spv");

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

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertShaderStageInfo, fragShaderStageInfo};

        // No vertex input for fullscreen triangle
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = 1.0f;  // Will be set dynamically
        viewport.height = 1.0f; // Will be set dynamically
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {1, 1}; // Will be set dynamically

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
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE; // No blending for straight alpha

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;

        if (vkCreatePipelineLayout(m_device->device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create postprocess pipeline layout!");
        }

        static std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

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
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create postprocess graphics pipeline!");
        }

        // Clean up shader modules
        vkDestroyShaderModule(m_device->device(), vertShaderModule, nullptr);
        vkDestroyShaderModule(m_device->device(), fragShaderModule, nullptr);
    }

    void VulkanPostprocessPipeline::createDescriptorSet(VkImageView inputImageView, VkSampler inputSampler) {
        // Create descriptor pool
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(m_device->device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create postprocess descriptor pool!");
        }

        // Allocate descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_descriptorSetLayout;

        if (vkAllocateDescriptorSets(m_device->device(), &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate postprocess descriptor sets!");
        }

        // Update descriptor set
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = inputImageView;
        imageInfo.sampler = inputSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device->device(), 1, &descriptorWrite, 0, nullptr);
    }

    void VulkanPostprocessPipeline::updateDescriptorSet(VkImageView inputImageView, VkSampler inputSampler) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = inputImageView;
        imageInfo.sampler = inputSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device->device(), 1, &descriptorWrite, 0, nullptr);
    }

    std::vector<char> VulkanPostprocessPipeline::readFile(const std::string &filename) const {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        size_t fileSize = static_cast<size_t>(file.tellg());

        // Ensure file size is a multiple of 4 for SPIR-V
        size_t alignedSize = (fileSize + 3) & ~3;
        std::vector<char> buffer(alignedSize);

        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

        // Zero out any padding bytes
        if (alignedSize > fileSize) {
            std::memset(buffer.data() + fileSize, 0, alignedSize - fileSize);
        }

        file.close();

        return buffer;
    }

    VkShaderModule VulkanPostprocessPipeline::createShaderModule(const std::vector<char> &code) const {
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

} // namespace Bess::Vulkan
