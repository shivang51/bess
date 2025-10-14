#include "scene/renderer/vulkan/pipelines/quad_pipeline.h"
#include "common/log.h"
#include "primitive_vertex.h"
#include "scene/renderer/vulkan/pipelines/pipeline.h"
#include "vulkan_core.h"
#include "vulkan_texture.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan::Pipelines {

    constexpr uint32_t instanceLimit = 10000;

    QuadPipeline::QuadPipeline(const std::shared_ptr<VulkanDevice> &device,
                               const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                               VkExtent2D extent)
        : Pipeline(device, renderPass, extent) {

        if (!m_fallbackTexture) {
            uint32_t white = 0xFFFFFFFF;
            m_fallbackTexture = std::make_unique<VulkanTexture>(m_device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &white);
        }

        createQuadBuffers();
        createUniformBuffers();

        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();

        createGraphicsPipeline(true);
        createGraphicsPipeline(false);

        ensureQuadInstanceCapacity(instanceLimit);
    }

    QuadPipeline::~QuadPipeline() {
        cleanup();
    }

    QuadPipeline::QuadPipeline(QuadPipeline &&other) noexcept
        : Pipeline(std::move(other)),
          m_buffers(std::move(other.m_buffers)),
          m_instances(std::move(other.m_instances)) {
    }

    QuadPipeline &QuadPipeline::operator=(QuadPipeline &&other) noexcept {
        if (this != &other) {
            cleanup();
            Pipeline::operator=(std::move(other));
            m_buffers = std::move(other.m_buffers);
            m_instances = std::move(other.m_instances);
        }
        return *this;
    }

    void QuadPipeline::beginPipeline(VkCommandBuffer commandBuffer, bool isTranslucent) {
        m_currentCommandBuffer = commandBuffer;
        m_instances.clear();

        vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          isTranslucent ? m_translucentPipeline : m_opaquePipeline);
        VkDescriptorSet sets[] = {m_descriptorSets[m_currentFrameIndex], m_textureArraySets[m_currentFrameIndex]};
        vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                isTranslucent ? m_transPipelineLayout : m_opaquePipelineLayout,
                                0, 2, sets, 0, nullptr);

        vkCmdSetViewport(m_currentCommandBuffer, 0, 1, &m_viewport);
        vkCmdSetScissor(m_currentCommandBuffer, 0, 1, &m_scissor);
    }

    void QuadPipeline::endPipeline() {
        if (m_instances.empty())
            return;

        auto draw = [&](const std::span<QuadInstance> &instances, VkDeviceSize offset) {
            if (instances.empty())
                return;
            void *data = nullptr;
            vkMapMemory(m_device->device(), m_buffers.instanceBufferMemory, offset, sizeof(QuadInstance) * instances.size(), 0, &data);
            memcpy(data, instances.data(), sizeof(QuadInstance) * instances.size());
            vkUnmapMemory(m_device->device(), m_buffers.instanceBufferMemory);

            VkBuffer vbs[] = {m_buffers.vertexBuffer, m_buffers.instanceBuffer};
            VkDeviceSize offs[] = {0, offset};
            vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 2, vbs, offs);
            vkCmdBindIndexBuffer(m_currentCommandBuffer, m_buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(m_currentCommandBuffer, 6, static_cast<uint32_t>(instances.size()), 0, 0, 0);
        };

        auto flush = [&](std::vector<QuadInstance> &instances, VkDeviceSize bufferOffset = 0) {
            if (instances.empty())
                return;

            size_t remaining = instances.size();
            size_t offset = 0;

            while (remaining > 0) {
                size_t batchSize = std::min(remaining, (size_t)instanceLimit);
                auto span = std::span<QuadInstance>(instances.data() + static_cast<ptrdiff_t>(offset), batchSize);
                draw(span, bufferOffset + (offset * sizeof(QuadInstance)));
                offset += batchSize;
                remaining -= batchSize;
            }
        };

        flush(m_instances, m_instanceCounter * sizeof(QuadInstance));
        m_instanceCounter += m_instances.size();

        m_instances.clear();

        m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    void QuadPipeline::setQuadsData(const std::vector<QuadInstance> &instances) {
        auto size = instances.size();
        if (size == 0)
            return;
        ensureQuadInstanceCapacity(size + m_instanceCounter);
        m_instances = instances;
    }

    void QuadPipeline::setQuadsData(const std::vector<QuadInstance> &instances,
                                    std::unordered_map<std::shared_ptr<VulkanTexture>, std::vector<QuadInstance>> &texutredData) {
        size_t texturedCount = 0;
        for (auto &entry : texutredData)
            texturedCount += entry.second.size();
        auto totalCount = instances.size() + texturedCount;
        if (totalCount == 0)
            return;
        m_instances = instances;
        ensureQuadInstanceCapacity(totalCount + m_instanceCounter);

        m_textureInfos.fill(m_fallbackTexture->getDescriptorInfo());
        int i = 1;
        for (auto &entry : texutredData) {
            m_textureInfos[i] = entry.first->getDescriptorInfo();
            for (auto &vertex : entry.second)
                vertex.texSlotIdx = i;

            m_instances.insert(m_instances.end(),
                               entry.second.begin(), entry.second.end());

            i++;
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

    void QuadPipeline::cleanup() {
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

        if (m_opaquePipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device->device(), m_opaquePipeline, nullptr);
            m_opaquePipeline = VK_NULL_HANDLE;
        }

        if (m_opaquePipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device->device(), m_opaquePipelineLayout, nullptr);
            m_opaquePipelineLayout = VK_NULL_HANDLE;
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
    }

    void QuadPipeline::createGraphicsPipeline(bool isTranslucent) {
        auto vertShaderCode = readFile("assets/shaders/quad.vert.spv");
        auto fragShaderCode = readFile("assets/shaders/quad.frag.spv");

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

        // Binding 0: local quad verts (position vec3 + texcoord vec2)
        VkVertexInputBindingDescription binding0{};
        binding0.binding = 0;
        binding0.stride = sizeof(float) * 5; // vec3 + vec2
        binding0.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // Binding 1: instance data
        VkVertexInputBindingDescription binding1{};
        binding1.binding = 1;
        binding1.stride = sizeof(QuadInstance);
        binding1.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        std::array<VkVertexInputBindingDescription, 2> bindings = {binding0, binding1};

        // Local vertex attributes (binding 0)
        std::array<VkVertexInputAttributeDescription, 2> localAttribs{};
        localAttribs[0].binding = 0;
        localAttribs[0].location = 0;
        localAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        localAttribs[0].offset = 0;

        localAttribs[1].binding = 0;
        localAttribs[1].location = 1;
        localAttribs[1].format = VK_FORMAT_R32G32_SFLOAT;
        localAttribs[1].offset = sizeof(float) * 3;

        // Instance attributes (binding 1)
        std::array<VkVertexInputAttributeDescription, 10> instanceAttribs{};
        instanceAttribs[0].binding = 1;
        instanceAttribs[0].location = 2;
        instanceAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        instanceAttribs[0].offset = offsetof(QuadInstance, position);

        instanceAttribs[1].binding = 1;
        instanceAttribs[1].location = 3;
        instanceAttribs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[1].offset = offsetof(QuadInstance, color);

        instanceAttribs[2].binding = 1;
        instanceAttribs[2].location = 4;
        instanceAttribs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[2].offset = offsetof(QuadInstance, borderRadius);

        instanceAttribs[3].binding = 1;
        instanceAttribs[3].location = 5;
        instanceAttribs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[3].offset = offsetof(QuadInstance, borderColor);

        instanceAttribs[4].binding = 1;
        instanceAttribs[4].location = 6;
        instanceAttribs[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[4].offset = offsetof(QuadInstance, borderSize);

        instanceAttribs[5].binding = 1;
        instanceAttribs[5].location = 7;
        instanceAttribs[5].format = VK_FORMAT_R32G32_SFLOAT;
        instanceAttribs[5].offset = offsetof(QuadInstance, size);

        instanceAttribs[6].binding = 1;
        instanceAttribs[6].location = 8;
        instanceAttribs[6].format = VK_FORMAT_R32_SINT;
        instanceAttribs[6].offset = offsetof(QuadInstance, id);

        instanceAttribs[7].binding = 1;
        instanceAttribs[7].location = 9;
        instanceAttribs[7].format = VK_FORMAT_R32_SINT;
        instanceAttribs[7].offset = offsetof(QuadInstance, isMica);

        instanceAttribs[8].binding = 1;
        instanceAttribs[8].location = 10;
        instanceAttribs[8].format = VK_FORMAT_R32_SINT;
        instanceAttribs[8].offset = offsetof(QuadInstance, texSlotIdx);

        instanceAttribs[9].binding = 1;
        instanceAttribs[9].location = 11;
        instanceAttribs[9].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[9].offset = offsetof(QuadInstance, texData);

        std::array<VkVertexInputAttributeDescription, 12> allAttribs{};
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
        auto depthStencil = createDepthStencilState(isTranslucent);

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

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

        if (isTranslucent) {
            if (vkCreatePipelineLayout(m_device->device(), &pipelineLayoutInfo, nullptr, &m_transPipelineLayout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create quad pipeline layout!");
            }
        } else {
            if (vkCreatePipelineLayout(m_device->device(), &pipelineLayoutInfo, nullptr, &m_opaquePipelineLayout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create quad pipeline layout!");
            }
        }
        auto dynamicState = createDynamicState();

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = isTranslucent ? m_transPipelineLayout : m_opaquePipelineLayout;
        pipelineInfo.renderPass = m_renderPass->getVkHandle();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      isTranslucent ? &m_translucentPipeline : &m_opaquePipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create quad graphics pipeline!");
        }

        vkDestroyShaderModule(m_device->device(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device->device(), vertShaderModule, nullptr);
    }

    void QuadPipeline::createQuadBuffers() {
        if (m_buffers.vertexBuffer != VK_NULL_HANDLE)
            return;

        struct LocalVertex {
            glm::vec3 pos;
            glm::vec2 uv;
        };
        std::array<LocalVertex, 4> local = {{
            {{-0.5f, -0.5f, 0.f}, {0.f, 0.f}},
            {{0.5f, -0.5f, 0.f}, {1.f, 0.f}},
            {{0.5f, 0.5f, 0.f}, {1.f, 1.f}},
            {{-0.5f, 0.5f, 0.f}, {0.f, 1.f}},
        }};
        std::array<uint32_t, 6> idx = {{0, 1, 2, 2, 3, 0}};

        VkDeviceSize vSize = sizeof(LocalVertex) * local.size();
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = vSize;
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_buffers.vertexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create quad vertex buffer!");
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.vertexBuffer, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_buffers.vertexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate quad vertex buffer memory!");
        }
        vkBindBufferMemory(m_device->device(), m_buffers.vertexBuffer, m_buffers.vertexBufferMemory, 0);
        void *vdata = nullptr;
        vkMapMemory(m_device->device(), m_buffers.vertexBufferMemory, 0, vSize, 0, &vdata);
        memcpy(vdata, local.data(), vSize);
        vkUnmapMemory(m_device->device(), m_buffers.vertexBufferMemory);

        // Allocate and upload quad index buffer
        VkDeviceSize iSize = sizeof(uint32_t) * idx.size();
        bi.size = iSize;
        bi.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_buffers.indexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create quad index buffer!");
        }
        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.indexBuffer, &req);
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_buffers.indexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate quad index buffer memory!");
        }
        vkBindBufferMemory(m_device->device(), m_buffers.indexBuffer, m_buffers.indexBufferMemory, 0);
        void *idata = nullptr;
        vkMapMemory(m_device->device(), m_buffers.indexBufferMemory, 0, iSize, 0, &idata);
        memcpy(idata, idx.data(), iSize);
        vkUnmapMemory(m_device->device(), m_buffers.indexBufferMemory);

        ensureQuadInstanceCapacity(1);
    }

    void QuadPipeline::ensureQuadInstanceCapacity(size_t instanceCount) {
        if (instanceCount <= m_buffers.instanceCapacity) {
            return; // Already have enough capacity
        }

        // Destroy old instance buffer if it exists
        if (m_buffers.instanceBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->device(), m_buffers.instanceBuffer, nullptr);
            vkFreeMemory(m_device->device(), m_buffers.instanceBufferMemory, nullptr);
        }

        // Create new instance buffer with required capacity
        VkDeviceSize size = sizeof(QuadInstance) * instanceCount;
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = size;
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_buffers.instanceBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create quad instance buffer!");
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.instanceBuffer, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_buffers.instanceBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate quad instance buffer memory!");
        }
        vkBindBufferMemory(m_device->device(), m_buffers.instanceBuffer, m_buffers.instanceBufferMemory, 0);

        m_buffers.instanceCapacity = instanceCount;
    }

    constexpr size_t maxFrames = Bess::Vulkan::VulkanCore::MAX_FRAMES_IN_FLIGHT;
    void QuadPipeline::createDescriptorPool() {
        Pipeline::createDescriptorPool();

        if (m_textureArrayDescriptorPool != VK_NULL_HANDLE)
            return;
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = m_texArraySize * maxFrames;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = maxFrames;
        if (vkCreateDescriptorPool(m_device->device(), &poolInfo, nullptr, &m_textureArrayDescriptorPool) != VK_SUCCESS) {
            BESS_ERROR("[QuadPipeline] Failed to create texture descriptor pool");
        }
    }

    void QuadPipeline::createDescriptorSets() {
        Pipeline::createDescriptorSets();
        m_textureArraySets.resize(maxFrames);

        VkDescriptorSetLayoutBinding texArrayBinding{};
        texArrayBinding.binding = 2;
        texArrayBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texArrayBinding.descriptorCount = 32; // e.g. 128
        texArrayBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &texArrayBinding;

        vkCreateDescriptorSetLayout(m_device->device(), &layoutInfo, nullptr, &m_textureArrayLayout);

        std::vector<VkDescriptorSetLayoutCreateInfo> layoutInfos(maxFrames, layoutInfo);
        std::vector<VkDescriptorSetLayout> layouts(maxFrames, m_textureArrayLayout);

        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};

        allocInfo.descriptorPool = m_textureArrayDescriptorPool;
        allocInfo.descriptorSetCount = maxFrames;
        allocInfo.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(m_device->device(), &allocInfo, m_textureArraySets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture array descriptor set");
        }
    }
} // namespace Bess::Vulkan::Pipelines
