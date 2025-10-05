#include "scene/renderer/vulkan/pipelines/text_pipeline.h"
#include "common/log.h"
#include "scene/renderer/vulkan/pipelines/pipeline.h"
#include "scene/renderer/vulkan/primitive_vertex.h"
#include "scene/renderer/vulkan/vulkan_texture.h"
#include "asset_manager/asset_manager.h"
#include "assets.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Bess::Renderer2D::Vulkan::Pipelines {

    TextPipeline::TextPipeline(const std::shared_ptr<VulkanDevice> &device,
                               const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                               VkExtent2D extent)
        : Pipeline(device, renderPass, extent) {

        createDescriptorPool();
        createTextUniformBuffers();
        createDescriptorSets();

        if (!m_fallbackTexture) {
            uint32_t white = 0xFFFFFFFF;
            m_fallbackTexture = std::make_unique<VulkanTexture>(m_device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &white);
        }

        ensureTextBuffers();
        createGraphicsPipeline();
    }

    TextPipeline::~TextPipeline() {
        cleanup();
    }

    TextPipeline::TextPipeline(TextPipeline &&other) noexcept
        : Pipeline(std::move(other)),
          m_buffers(other.m_buffers),
          m_opaqueInstances(std::move(other.m_opaqueInstances)),
          m_translucentInstances(std::move(other.m_translucentInstances)),
          m_textureArraySets(std::move(other.m_textureArraySets)),
          m_textureArrayDescriptorPool(other.m_textureArrayDescriptorPool),
          m_textureArrayLayout(other.m_textureArrayLayout),
          m_fallbackTexture(std::move(other.m_fallbackTexture)),
          m_textureInfos(other.m_textureInfos) {
        other.m_textureArrayDescriptorPool = VK_NULL_HANDLE;
        other.m_textureArrayLayout = VK_NULL_HANDLE;
    }

    TextPipeline &TextPipeline::operator=(TextPipeline &&other) noexcept {
        if (this != &other) {
            cleanup();
            Pipeline::operator=(std::move(other));
            m_buffers = other.m_buffers;
            m_opaqueInstances = std::move(other.m_opaqueInstances);
            m_translucentInstances = std::move(other.m_translucentInstances);
            m_textureArraySets = std::move(other.m_textureArraySets);
            m_textureArrayDescriptorPool = other.m_textureArrayDescriptorPool;
            m_textureArrayLayout = other.m_textureArrayLayout;
            m_fallbackTexture = std::move(other.m_fallbackTexture);
            m_textureInfos = other.m_textureInfos;

            other.m_textureArrayDescriptorPool = VK_NULL_HANDLE;
            other.m_textureArrayLayout = VK_NULL_HANDLE;
        }
        return *this;
    }

    void TextPipeline::beginPipeline(VkCommandBuffer commandBuffer) {
        m_currentCommandBuffer = commandBuffer;
        m_opaqueInstances.clear();
        m_translucentInstances.clear();

        vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        VkDescriptorSet sets[] = {m_descriptorSets[m_currentFrameIndex], m_textureArraySets[m_currentFrameIndex]};
        vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 2, sets, 0, nullptr);
    }

    void TextPipeline::endPipeline() {
        auto draw = [&](const std::vector<InstanceVertex> &instances, VkDeviceSize offset) {
            if (instances.empty())
                return;
            void *data = nullptr;
            vkMapMemory(m_device->device(), m_buffers.instanceBufferMemory, offset, sizeof(InstanceVertex) * instances.size(), 0, &data);
            memcpy(data, instances.data(), sizeof(InstanceVertex) * instances.size());
            vkUnmapMemory(m_device->device(), m_buffers.instanceBufferMemory);

            std::array<VkBuffer, 2> vbs = {m_buffers.vertexBuffer, m_buffers.instanceBuffer};
            std::array<VkDeviceSize, 2> offs = {0, offset};
            vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 2, vbs.data(), offs.data());
            vkCmdBindIndexBuffer(m_currentCommandBuffer, m_buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(m_currentCommandBuffer, 6, static_cast<uint32_t>(instances.size()), 0, 0, 0);
        };

        ensureTextInstanceCapacity(m_opaqueInstances.size() + m_translucentInstances.size());
        VkDeviceSize translucentOffset = m_opaqueInstances.size() * sizeof(InstanceVertex);

        draw(m_opaqueInstances, 0);
        m_opaqueInstances.clear();

        std::ranges::sort(m_translucentInstances, [](InstanceVertex &a, InstanceVertex &b) -> bool {
            return a.position.z < b.position.z;
        });
        draw(m_translucentInstances, translucentOffset);
        m_translucentInstances.clear();

        m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    void TextPipeline::setTextData(const std::vector<InstanceVertex> &opaque,
                                   const std::vector<InstanceVertex> &translucent) {
        m_opaqueInstances = opaque;
        m_translucentInstances = translucent;
        

        // Fill with fallback texture first
        m_textureInfos.fill(m_fallbackTexture->getDescriptorInfo());
        
        // Get the MSDF font texture and set it at slot 1 (where text instances expect it)
        auto msdfFont = Assets::AssetManager::instance().get(Assets::Fonts::robotoMsdf);
        if (msdfFont && msdfFont->getTextureAtlas()) {
            m_textureInfos[1] = msdfFont->getTextureAtlas()->getDescriptorInfo();
        }

        std::vector<VkWriteDescriptorSet> writes;
        for (uint32_t i = 0; i < m_textureInfos.size(); i++) {
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = m_textureArraySets[m_currentFrameIndex];
            write.dstBinding = 2;
            write.dstArrayElement = i;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &m_textureInfos.at(i);
            writes.push_back(write);
        }

        vkUpdateDescriptorSets(m_device->device(), (uint32_t)writes.size(), writes.data(), 0, nullptr);
    }

    void TextPipeline::updateTextUniforms(const TextUniforms &textUniforms) {
        if (m_textUniformBufferMemory.empty()) {
            BESS_WARN("[TextPipeline] Text uniform buffer memory not initialized");
            return;
        }

        if (m_currentFrameIndex >= m_textUniformBufferMemory.size()) {
            BESS_WARN("[TextPipeline] Current frame index {} out of bounds for uniform buffer memory (size: {})",
                      m_currentFrameIndex, m_textUniformBufferMemory.size());
            return;
        }


        void *data = nullptr;
        vkMapMemory(m_device->device(), m_textUniformBufferMemory[m_currentFrameIndex], 0, sizeof(TextUniforms), 0, &data);
        memcpy(data, &textUniforms, sizeof(TextUniforms));
        vkUnmapMemory(m_device->device(), m_textUniformBufferMemory[m_currentFrameIndex]);
    }

    void TextPipeline::cleanup() {
        if (m_buffers.vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->device(), m_buffers.vertexBuffer, nullptr);
            vkFreeMemory(m_device->device(), m_buffers.vertexBufferMemory, nullptr);
            m_buffers.vertexBuffer = VK_NULL_HANDLE;
            m_buffers.vertexBufferMemory = VK_NULL_HANDLE;
        }

        if (m_buffers.indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->device(), m_buffers.indexBuffer, nullptr);
            vkFreeMemory(m_device->device(), m_buffers.indexBufferMemory, nullptr);
            m_buffers.indexBuffer = VK_NULL_HANDLE;
            m_buffers.indexBufferMemory = VK_NULL_HANDLE;
        }

        if (m_buffers.instanceBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->device(), m_buffers.instanceBuffer, nullptr);
            vkFreeMemory(m_device->device(), m_buffers.instanceBufferMemory, nullptr);
            m_buffers.instanceBuffer = VK_NULL_HANDLE;
            m_buffers.instanceBufferMemory = VK_NULL_HANDLE;
        }

        if (m_textureArrayDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device->device(), m_textureArrayDescriptorPool, nullptr);
        }

        if (m_textureArrayLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device->device(), m_textureArrayLayout, nullptr);
        }

        if (m_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device->device(), m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
        }

        if (m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device->device(), m_pipelineLayout, nullptr);
            m_pipelineLayout = VK_NULL_HANDLE;
        }

        if (m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device->device(), m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }

        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device->device(), m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }

        for (size_t i = 0; i < m_uniformBuffers.size(); i++) {
            vkDestroyBuffer(m_device->device(), m_uniformBuffers[i], nullptr);
            vkFreeMemory(m_device->device(), m_uniformBufferMemory[i], nullptr);
        }

        for (size_t i = 0; i < m_textUniformBuffers.size(); i++) {
            vkDestroyBuffer(m_device->device(), m_textUniformBuffers[i], nullptr);
            vkFreeMemory(m_device->device(), m_textUniformBufferMemory[i], nullptr);
        }
    }

    constexpr size_t maxFrames = 2;

    void TextPipeline::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding textLayoutBinding{};
        textLayoutBinding.binding = 1;
        textLayoutBinding.descriptorCount = 1;
        textLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        textLayoutBinding.pImmutableSamplers = nullptr;
        textLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, textLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_device->device(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create text descriptor set layout!");
        }
    }

    void TextPipeline::createTextUniformBuffers() {
        m_textUniformBuffers.resize(maxFrames);
        m_textUniformBufferMemory.resize(maxFrames);

        VkDeviceSize bufferSize = sizeof(TextUniforms);

        for (size_t i = 0; i < maxFrames; i++) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_textUniformBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create text uniform buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_device->device(), m_textUniformBuffers[i], &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_textUniformBufferMemory[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate text uniform buffer memory!");
            }

            vkBindBufferMemory(m_device->device(), m_textUniformBuffers[i], m_textUniformBufferMemory[i], 0);
        }
    }

    void TextPipeline::createGraphicsPipeline() {
        auto vertShaderCode = readFile("assets/shaders/instance.vert.spv");
        auto fragShaderCode = readFile("assets/shaders/text.frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vert{};
        vert.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert.module = vertShaderModule;
        vert.pName = "main";
        VkPipelineShaderStageCreateInfo frag{};
        frag.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag.module = fragShaderModule;
        frag.pName = "main";
        std::array<VkPipelineShaderStageCreateInfo, 2> stages{vert, frag};

        // Binding 0: local quad verts (position vec2 + texcoord vec2)
        VkVertexInputBindingDescription binding0{};
        binding0.binding = 0;
        binding0.stride = sizeof(float) * 4; // vec2 + vec2
        binding0.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // Binding 1: instance data
        VkVertexInputBindingDescription binding1{};
        binding1.binding = 1;
        binding1.stride = sizeof(InstanceVertex);
        binding1.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        std::array<VkVertexInputBindingDescription, 2> bindings = {binding0, binding1};

        // Local vertex attributes (binding 0)
        std::array<VkVertexInputAttributeDescription, 2> localAttribs{};
        localAttribs[0].binding = 0;
        localAttribs[0].location = 0;
        localAttribs[0].format = VK_FORMAT_R32G32_SFLOAT;
        localAttribs[0].offset = 0;

        localAttribs[1].binding = 0;
        localAttribs[1].location = 1;
        localAttribs[1].format = VK_FORMAT_R32G32_SFLOAT;
        localAttribs[1].offset = sizeof(float) * 2;

        // Instance attributes (binding 1)
        std::array<VkVertexInputAttributeDescription, 7> instanceAttribs{};
        instanceAttribs[0].binding = 1;
        instanceAttribs[0].location = 2;
        instanceAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        instanceAttribs[0].offset = offsetof(InstanceVertex, position);

        instanceAttribs[1].binding = 1;
        instanceAttribs[1].location = 3;
        instanceAttribs[1].format = VK_FORMAT_R32G32_SFLOAT;
        instanceAttribs[1].offset = offsetof(InstanceVertex, size);

        instanceAttribs[2].binding = 1;
        instanceAttribs[2].location = 4;
        instanceAttribs[2].format = VK_FORMAT_R32_SFLOAT;
        instanceAttribs[2].offset = offsetof(InstanceVertex, angle);

        instanceAttribs[3].binding = 1;
        instanceAttribs[3].location = 5;
        instanceAttribs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[3].offset = offsetof(InstanceVertex, color);

        instanceAttribs[4].binding = 1;
        instanceAttribs[4].location = 6;
        instanceAttribs[4].format = VK_FORMAT_R32_SINT;
        instanceAttribs[4].offset = offsetof(InstanceVertex, id);

        instanceAttribs[5].binding = 1;
        instanceAttribs[5].location = 7;
        instanceAttribs[5].format = VK_FORMAT_R32_SINT;
        instanceAttribs[5].offset = offsetof(InstanceVertex, texSlotIdx);

        instanceAttribs[6].binding = 1;
        instanceAttribs[6].location = 8;
        instanceAttribs[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[6].offset = offsetof(InstanceVertex, texData);

        std::array<VkVertexInputAttributeDescription, 9> allAttribs{};
        std::copy(localAttribs.begin(), localAttribs.end(), allAttribs.begin());
        std::copy(instanceAttribs.begin(), instanceAttribs.end(), allAttribs.begin() + 2);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
        vertexInputInfo.pVertexBindingDescriptions = bindings.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(allAttribs.size());
        vertexInputInfo.pVertexAttributeDescriptions = allAttribs.data();

        // Use common pipeline state creation methods
        auto inputAssembly = createInputAssemblyState();
        auto viewportState = createViewportState();
        auto rasterizer = createRasterizationState();
        auto multisampling = createMultisampleState();
        auto depthStencil = createDepthStencilState();

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendAttachmentState pickingBlendAttachment = colorBlendAttachment;
        pickingBlendAttachment.blendEnable = VK_FALSE;

        static const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {colorBlendAttachment, pickingBlendAttachment};

        // Use common color blend state creation
        auto colorBlending = createColorBlendState(colorBlendAttachments);

        std::array<VkDescriptorSetLayout, 2> setLayouts = {m_descriptorSetLayout, m_textureArrayLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
        pipelineLayoutInfo.pSetLayouts = setLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(m_device->device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create text pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass->getVkHandle();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create text graphics pipeline!");
        }

        vkDestroyShaderModule(m_device->device(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device->device(), vertShaderModule, nullptr);
    }

    void TextPipeline::ensureTextBuffers() {
        // Create local quad vertex data
        std::vector<float> local = {
            -0.5f, -0.5f, 0.0f, 0.0f, // bottom-left
            0.5f, -0.5f, 1.0f, 0.0f,  // bottom-right
            0.5f, 0.5f, 1.0f, 1.0f,   // top-right
            -0.5f, 0.5f, 0.0f, 1.0f   // top-left
        };

        std::vector<uint32_t> idx = {
            0, 1, 2, 2, 3, 0};

        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

        VkMemoryRequirements req{};

        // Allocate and upload text vertex buffer
        VkDeviceSize vSize = sizeof(float) * local.size();
        bi.size = vSize;
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_buffers.vertexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create text vertex buffer!");
        }
        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.vertexBuffer, &req);
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_buffers.vertexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate text vertex buffer memory!");
        }
        vkBindBufferMemory(m_device->device(), m_buffers.vertexBuffer, m_buffers.vertexBufferMemory, 0);
        void *vdata = nullptr;
        vkMapMemory(m_device->device(), m_buffers.vertexBufferMemory, 0, vSize, 0, &vdata);
        memcpy(vdata, local.data(), vSize);
        vkUnmapMemory(m_device->device(), m_buffers.vertexBufferMemory);

        // Allocate and upload text index buffer
        VkDeviceSize iSize = sizeof(uint32_t) * idx.size();
        bi.size = iSize;
        bi.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_buffers.indexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create text index buffer!");
        }
        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.indexBuffer, &req);
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_buffers.indexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate text index buffer memory!");
        }
        vkBindBufferMemory(m_device->device(), m_buffers.indexBuffer, m_buffers.indexBufferMemory, 0);
        void *idata = nullptr;
        vkMapMemory(m_device->device(), m_buffers.indexBufferMemory, 0, iSize, 0, &idata);
        memcpy(idata, idx.data(), iSize);
        vkUnmapMemory(m_device->device(), m_buffers.indexBufferMemory);

        ensureTextInstanceCapacity(1);
    }

    void TextPipeline::ensureTextInstanceCapacity(size_t instanceCount) {
        if (instanceCount <= m_buffers.instanceCapacity) {
            return; // Already have enough capacity
        }

        // Destroy old instance buffer if it exists
        if (m_buffers.instanceBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->device(), m_buffers.instanceBuffer, nullptr);
            vkFreeMemory(m_device->device(), m_buffers.instanceBufferMemory, nullptr);
        }

        // Create new instance buffer with required capacity
        VkDeviceSize size = sizeof(InstanceVertex) * instanceCount;
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = size;
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_buffers.instanceBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create text instance buffer!");
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.instanceBuffer, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_buffers.instanceBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate text instance buffer memory!");
        }
        vkBindBufferMemory(m_device->device(), m_buffers.instanceBuffer, m_buffers.instanceBufferMemory, 0);

        m_buffers.instanceCapacity = instanceCount;
    }

    void TextPipeline::createDescriptorPool() {
        // Override base class to include TextUniforms in the main descriptor pool
        std::array<VkDescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 4; // 2 for UBO + 2 for TextUniforms

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = maxFrames;

        if (vkCreateDescriptorPool(m_device->device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create text descriptor pool!");
        }

        // Create texture array descriptor pool
        if (m_textureArrayDescriptorPool != VK_NULL_HANDLE)
            return;

        VkDescriptorPoolSize texPoolSize{};
        texPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texPoolSize.descriptorCount = m_texArraySize * maxFrames;

        VkDescriptorPoolCreateInfo texPoolInfo{};
        texPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        texPoolInfo.poolSizeCount = 1;
        texPoolInfo.pPoolSizes = &texPoolSize;
        texPoolInfo.maxSets = maxFrames;
        if (vkCreateDescriptorPool(m_device->device(), &texPoolInfo, nullptr, &m_textureArrayDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture descriptor pool");
        }
    }

    void TextPipeline::createDescriptorSets() {
        if (m_uniformBuffers.empty()) {
            return; // No uniform buffers to create descriptor sets for
        }

        // Create descriptor sets for each frame in flight
        m_descriptorSets.resize(maxFrames);

        std::vector<VkDescriptorSetLayout> layouts(maxFrames, m_descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = maxFrames;
        allocInfo.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(m_device->device(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        // Update all descriptor sets with both UBO and TextUniforms
        for (size_t i = 0; i < maxFrames; ++i) {
            // UBO binding (0)
            VkDescriptorBufferInfo uboBufferInfo{};
            uboBufferInfo.buffer = m_uniformBuffers[0];
            uboBufferInfo.offset = 0;
            uboBufferInfo.range = sizeof(UniformBufferObject);

            VkWriteDescriptorSet uboDescriptorWrite{};
            uboDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uboDescriptorWrite.dstSet = m_descriptorSets[i];
            uboDescriptorWrite.dstBinding = 0;
            uboDescriptorWrite.dstArrayElement = 0;
            uboDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboDescriptorWrite.descriptorCount = 1;
            uboDescriptorWrite.pBufferInfo = &uboBufferInfo;

            // TextUniforms binding (1)
            VkDescriptorBufferInfo textBufferInfo{};
            textBufferInfo.buffer = m_textUniformBuffers[i];
            textBufferInfo.offset = 0;
            textBufferInfo.range = sizeof(TextUniforms);

            VkWriteDescriptorSet textDescriptorWrite{};
            textDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            textDescriptorWrite.dstSet = m_descriptorSets[i];
            textDescriptorWrite.dstBinding = 1;
            textDescriptorWrite.dstArrayElement = 0;
            textDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            textDescriptorWrite.descriptorCount = 1;
            textDescriptorWrite.pBufferInfo = &textBufferInfo;

            // Update both bindings at once
            std::array<VkWriteDescriptorSet, 2> descriptorWrites = {uboDescriptorWrite, textDescriptorWrite};
            vkUpdateDescriptorSets(m_device->device(), 2, descriptorWrites.data(), 0, nullptr);
        }

        // Create texture array descriptor sets
        m_textureArraySets.resize(maxFrames);

        std::array<VkDescriptorSetLayoutBinding, 1> texBindings{};

        // Texture array binding
        texBindings[0].binding = 2;
        texBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texBindings[0].descriptorCount = 32;
        texBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = static_cast<uint32_t>(texBindings.size());
        layoutInfo.pBindings = texBindings.data();

        vkCreateDescriptorSetLayout(m_device->device(), &layoutInfo, nullptr, &m_textureArrayLayout);

        auto texLayouts = std::vector(maxFrames, m_textureArrayLayout);

        VkDescriptorSetAllocateInfo texAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        texAllocInfo.descriptorPool = m_textureArrayDescriptorPool;
        texAllocInfo.descriptorSetCount = maxFrames;
        texAllocInfo.pSetLayouts = texLayouts.data();

        if (vkAllocateDescriptorSets(m_device->device(), &texAllocInfo, m_textureArraySets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate texture descriptor sets");
        }
    }

} // namespace Bess::Renderer2D::Vulkan::Pipelines
