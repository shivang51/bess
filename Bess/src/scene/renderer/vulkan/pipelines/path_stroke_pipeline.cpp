#include "scene/renderer/vulkan/pipelines/path_stroke_pipeline.h"
#include "device.h"
#include "primitive_vertex.h"
#include "scene/renderer/vulkan/pipelines/pipeline.h"
#include "vulkan_core.h"
#include "vulkan_offscreen_render_pass.h"
#include <array>
#include <cstdint>
#include <cstring>

namespace Bess::Vulkan::Pipelines {

    constexpr size_t maxFrames = Bess::Vulkan::VulkanCore::MAX_FRAMES_IN_FLIGHT;
    constexpr size_t instanceLimit = 10000;

    PathStrokePipeline::PathStrokePipeline(const std::shared_ptr<VulkanDevice> &device,
                                           const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                                           VkExtent2D extent)
        : Pipeline(device, renderPass, extent) {
        createZoomUniformBuffers();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();

        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();

        createGraphicsPipeline();

        ensurePathCapacity(instanceLimit * 4, instanceLimit * 6);
    }

    PathStrokePipeline::~PathStrokePipeline() {
        cleanup();
    }

    PathStrokePipeline::PathStrokePipeline(PathStrokePipeline &&other) noexcept
        : Pipeline(std::move(other)),
          m_strokeVertices(std::move(other.m_strokeVertices)),
          m_strokeIndices(std::move(other.m_strokeIndices)),
          m_vertexBuffers(std::move(other.m_vertexBuffers)),
          m_vertexBufferMemory(std::move(other.m_vertexBufferMemory)),
          m_indexBuffers(std::move(other.m_indexBuffers)),
          m_indexBufferMemory(std::move(other.m_indexBufferMemory)),
          m_currentVertexCapacity(other.m_currentVertexCapacity),
          m_currentIndexCapacity(other.m_currentIndexCapacity),
          m_zoomUniformBuffers(std::move(other.m_zoomUniformBuffers)),
          m_zoomUniformBufferMemory(std::move(other.m_zoomUniformBufferMemory)) {
        other.m_vertexBuffers.clear();
        other.m_vertexBufferMemory.clear();
        other.m_indexBuffers.clear();
        other.m_indexBufferMemory.clear();
        other.m_zoomUniformBuffers.clear();
        other.m_zoomUniformBufferMemory.clear();
        other.m_currentVertexCapacity = 0;
        other.m_currentIndexCapacity = 0;
    }

    PathStrokePipeline &PathStrokePipeline::operator=(PathStrokePipeline &&other) noexcept {
        if (this != &other) {
            cleanup();
            Pipeline::operator=(std::move(other));
            m_strokeVertices = std::move(other.m_strokeVertices);
            m_strokeIndices = std::move(other.m_strokeIndices);
            m_vertexBuffers = std::move(other.m_vertexBuffers);
            m_vertexBufferMemory = std::move(other.m_vertexBufferMemory);
            m_indexBuffers = std::move(other.m_indexBuffers);
            m_indexBufferMemory = std::move(other.m_indexBufferMemory);
            m_currentVertexCapacity = other.m_currentVertexCapacity;
            m_currentIndexCapacity = other.m_currentIndexCapacity;
            m_zoomUniformBuffers = std::move(other.m_zoomUniformBuffers);
            m_zoomUniformBufferMemory = std::move(other.m_zoomUniformBufferMemory);

            other.m_vertexBuffers.clear();
            other.m_vertexBufferMemory.clear();
            other.m_indexBuffers.clear();
            other.m_indexBufferMemory.clear();
            other.m_zoomUniformBuffers.clear();
            other.m_zoomUniformBufferMemory.clear();
            other.m_currentVertexCapacity = 0;
            other.m_currentIndexCapacity = 0;
        }
        return *this;
    }

    void PathStrokePipeline::beginPipeline(VkCommandBuffer commandBuffer) {
        m_currentCommandBuffer = commandBuffer;
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                                &m_descriptorSets[m_currentFrameIndex], 0, nullptr);

        vkCmdSetViewport(m_currentCommandBuffer, 0, 1, &m_viewport);
        vkCmdSetScissor(m_currentCommandBuffer, 0, 1, &m_scissor);
    }

    void PathStrokePipeline::endPipeline() {
        if (m_strokeVertices.empty())
            return;

        auto &vertexBuffer = m_vertexBuffers[m_currentFrameIndex];
        constexpr VkDeviceSize vertexOffset = 0;
        constexpr VkDeviceSize indexOffset = 0;
        void *data = nullptr;

        VkDeviceSize bufferSize = m_strokeVertices.size() * sizeof(CommonVertex);
        vkMapMemory(m_device->device(), m_vertexBufferMemory[m_currentFrameIndex], vertexOffset, bufferSize, 0, &data);
        memcpy(data, m_strokeVertices.data(), bufferSize);
        vkUnmapMemory(m_device->device(), m_vertexBufferMemory[m_currentFrameIndex]);

        data = nullptr;
        bufferSize = m_strokeIndices.size() * sizeof(uint32_t);
        vkMapMemory(m_device->device(), m_indexBufferMemory[m_currentFrameIndex], indexOffset, bufferSize, 0, &data);
        memcpy(data, m_strokeIndices.data(), bufferSize);
        vkUnmapMemory(m_device->device(), m_indexBufferMemory[m_currentFrameIndex]);

        vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
        vkCmdBindIndexBuffer(m_currentCommandBuffer, m_indexBuffers[m_currentFrameIndex], indexOffset, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(m_currentCommandBuffer, static_cast<uint32_t>(m_strokeIndices.size()), 1, 0, 0, 0);

        m_strokeVertices.clear();
        m_strokeIndices.clear();
        m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    void PathStrokePipeline::setPathData(const std::vector<CommonVertex> &strokeVertices, const std::vector<uint32_t> &strokeIndices) {

        if (strokeVertices.empty())
            return;

        m_strokeVertices = strokeVertices;
        m_strokeIndices = strokeIndices;

        ensurePathCapacity(m_strokeVertices.size(), m_strokeIndices.size());
    }

    void PathStrokePipeline::updateUniformBuffer(const UniformBufferObject &ubo) {
        Pipeline::updateUniformBuffer(ubo);
    }

    void PathStrokePipeline::cleanup() {
        for (size_t i = 0; i < m_zoomUniformBuffers.size(); i++) {
            if (m_zoomUniformBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device->device(), m_zoomUniformBuffers[i], nullptr);
            }
            if (m_zoomUniformBufferMemory[i] != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_zoomUniformBufferMemory[i], nullptr);
            }
        }

        for (size_t i = 0; i < m_vertexBuffers.size(); i++) {
            if (m_vertexBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device->device(), m_vertexBuffers[i], nullptr);
            }
            if (m_vertexBufferMemory[i] != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_vertexBufferMemory[i], nullptr);
            }
        }

        for (size_t i = 0; i < m_indexBuffers.size(); i++) {
            if (m_indexBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device->device(), m_indexBuffers[i], nullptr);
            }
            if (m_indexBufferMemory[i] != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_indexBufferMemory[i], nullptr);
            }
        }

        Pipeline::cleanup();
    }

    void PathStrokePipeline::createGraphicsPipeline() {
        auto vertShaderCode = readFile("assets/shaders/common.vert.spv");
        auto fragShaderCode = readFile("assets/shaders/path.frag.spv");

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

        m_shaderStages = {vert, frag};

        auto bindingDescription = CommonVertex::getBindingDescription();
        auto attributeDescriptions = CommonVertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        inputAssembly.primitiveRestartEnable = VK_TRUE;

        auto viewportState = createViewportState();
        auto rasterizer = createRasterizationState();
        auto multisampling = createMultisampleState();
        auto depthStencil = createDepthStencilState();

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        auto pickingBlendAttachment = colorBlendAttachment;
        pickingBlendAttachment.blendEnable = VK_FALSE;

        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {colorBlendAttachment, pickingBlendAttachment};

        auto colorBlending = createColorBlendState(colorBlendAttachments);
        colorBlending.pAttachments = colorBlendAttachments.data();
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        // Create pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(m_device->device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create path pipeline layout!");
        }

        auto dynamicState = createDynamicState();

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = m_shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass->getVkHandle();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create path graphics pipeline!");
        }

        vkDestroyShaderModule(m_device->device(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device->device(), vertShaderModule, nullptr);
    }

    void PathStrokePipeline::createVertexBuffer() {
        m_vertexBuffers.resize(maxFrames);
        m_vertexBufferMemory.resize(maxFrames);

        for (size_t i = 0; i < maxFrames; i++) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(CommonVertex) * 1000;
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_vertexBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create path vertex buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_device->device(), m_vertexBuffers[i], &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_vertexBufferMemory[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate path vertex buffer memory!");
            }

            vkBindBufferMemory(m_device->device(), m_vertexBuffers[i], m_vertexBufferMemory[i], 0);
        }

        m_currentVertexCapacity = 1000;
    }

    void PathStrokePipeline::createIndexBuffer() {
        m_indexBuffers.resize(maxFrames);
        m_indexBufferMemory.resize(maxFrames);

        for (size_t i = 0; i < maxFrames; i++) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(uint32_t) * 2000;
            bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_indexBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create path index buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_device->device(), m_indexBuffers[i], &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_indexBufferMemory[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate path index buffer memory!");
            }

            vkBindBufferMemory(m_device->device(), m_indexBuffers[i], m_indexBufferMemory[i], 0);
        }

        m_currentIndexCapacity = 2000;
    }

    void PathStrokePipeline::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding zoomLayoutBinding{};
        zoomLayoutBinding.binding = 1;
        zoomLayoutBinding.descriptorCount = 1;
        zoomLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        zoomLayoutBinding.pImmutableSamplers = nullptr;
        zoomLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, zoomLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_device->device(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create path descriptor set layout!");
        }
    }

    void PathStrokePipeline::createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 4; // 2 for UBO + 2 for zoom

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = maxFrames;

        if (vkCreateDescriptorPool(m_device->device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create path descriptor pool!");
        }
    }

    void PathStrokePipeline::createDescriptorSets() {
        if (m_uniformBuffers.empty()) {
            return;
        }

        m_descriptorSets.resize(maxFrames);

        std::vector<VkDescriptorSetLayout> layouts(maxFrames, m_descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = maxFrames;
        allocInfo.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(m_device->device(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate path descriptor sets!");
        }

        for (size_t i = 0; i < maxFrames; i++) {
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

            // Zoom uniform binding (1)
            VkDescriptorBufferInfo zoomBufferInfo{};
            zoomBufferInfo.buffer = m_zoomUniformBuffers[i];
            zoomBufferInfo.offset = 0;
            zoomBufferInfo.range = sizeof(float);

            VkWriteDescriptorSet zoomDescriptorWrite{};
            zoomDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            zoomDescriptorWrite.dstSet = m_descriptorSets[i];
            zoomDescriptorWrite.dstBinding = 1;
            zoomDescriptorWrite.dstArrayElement = 0;
            zoomDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            zoomDescriptorWrite.descriptorCount = 1;
            zoomDescriptorWrite.pBufferInfo = &zoomBufferInfo;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites = {uboDescriptorWrite, zoomDescriptorWrite};
            vkUpdateDescriptorSets(m_device->device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void PathStrokePipeline::ensurePathBuffers() {
        // This method can be used to ensure buffers are ready
    }

    void PathStrokePipeline::ensurePathCapacity(size_t vertexCount, size_t indexCount) {
        bool needVertexResize = vertexCount > m_currentVertexCapacity;
        bool needIndexResize = indexCount > m_currentIndexCapacity;

        if (!needVertexResize && !needIndexResize) {
            return; // Already have enough capacity
        }

        // Calculate new capacities with some headroom
        size_t newVertexCapacity = needVertexResize ? std::max(vertexCount * 2, m_currentVertexCapacity * 2) : m_currentVertexCapacity;
        size_t newIndexCapacity = needIndexResize ? std::max(indexCount * 2, m_currentIndexCapacity * 2) : m_currentIndexCapacity;

        // Destroy old vertex buffers if we need to resize
        if (needVertexResize) {
            for (size_t i = 0; i < m_vertexBuffers.size(); i++) {
                if (m_vertexBuffers[i] != VK_NULL_HANDLE) {
                    vkDestroyBuffer(m_device->device(), m_vertexBuffers[i], nullptr);
                }
                if (m_vertexBufferMemory[i] != VK_NULL_HANDLE) {
                    vkFreeMemory(m_device->device(), m_vertexBufferMemory[i], nullptr);
                }
            }
        }

        // Destroy old index buffers if we need to resize
        if (needIndexResize) {
            for (size_t i = 0; i < m_indexBuffers.size(); i++) {
                if (m_indexBuffers[i] != VK_NULL_HANDLE) {
                    vkDestroyBuffer(m_device->device(), m_indexBuffers[i], nullptr);
                }
                if (m_indexBufferMemory[i] != VK_NULL_HANDLE) {
                    vkFreeMemory(m_device->device(), m_indexBufferMemory[i], nullptr);
                }
            }
        }

        // Create new vertex buffers with required capacity
        if (needVertexResize) {
            for (size_t i = 0; i < maxFrames; i++) {
                VkBufferCreateInfo bufferInfo{};
                bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bufferInfo.size = sizeof(CommonVertex) * newVertexCapacity;
                bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

                if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_vertexBuffers[i]) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create resized path vertex buffer!");
                }

                VkMemoryRequirements memRequirements;
                vkGetBufferMemoryRequirements(m_device->device(), m_vertexBuffers[i], &memRequirements);

                VkMemoryAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocInfo.allocationSize = memRequirements.size;
                allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

                if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_vertexBufferMemory[i]) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to allocate resized path vertex buffer memory!");
                }

                vkBindBufferMemory(m_device->device(), m_vertexBuffers[i], m_vertexBufferMemory[i], 0);
            }
            m_currentVertexCapacity = newVertexCapacity;
        }

        // Create new index buffers with required capacity
        if (needIndexResize) {
            for (size_t i = 0; i < maxFrames; i++) {
                VkBufferCreateInfo bufferInfo{};
                bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bufferInfo.size = sizeof(uint32_t) * newIndexCapacity;
                bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
                bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

                if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_indexBuffers[i]) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create resized path index buffer!");
                }

                VkMemoryRequirements memRequirements;
                vkGetBufferMemoryRequirements(m_device->device(), m_indexBuffers[i], &memRequirements);

                VkMemoryAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocInfo.allocationSize = memRequirements.size;
                allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

                if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_indexBufferMemory[i]) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to allocate resized path index buffer memory!");
                }

                vkBindBufferMemory(m_device->device(), m_indexBuffers[i], m_indexBufferMemory[i], 0);
            }
            m_currentIndexCapacity = newIndexCapacity;
        }
    }

    void PathStrokePipeline::createZoomUniformBuffers() {
        m_zoomUniformBuffers.resize(maxFrames);
        m_zoomUniformBufferMemory.resize(maxFrames);

        for (size_t i = 0; i < maxFrames; i++) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(float);
            bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_zoomUniformBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create zoom uniform buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_device->device(), m_zoomUniformBuffers[i], &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_zoomUniformBufferMemory[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate zoom uniform buffer memory!");
            }

            vkBindBufferMemory(m_device->device(), m_zoomUniformBuffers[i], m_zoomUniformBufferMemory[i], 0);
        }
    }

    void PathStrokePipeline::updateZoomUniformBuffer(float zoom) {
        if (m_zoomUniformBufferMemory.empty() || m_currentFrameIndex >= m_zoomUniformBufferMemory.size()) {
            return;
        }

        void *data = nullptr;
        vkMapMemory(m_device->device(), m_zoomUniformBufferMemory[m_currentFrameIndex], 0, sizeof(float), 0, &data);
        memcpy(data, &zoom, sizeof(float));
        vkUnmapMemory(m_device->device(), m_zoomUniformBufferMemory[m_currentFrameIndex]);
    }

} // namespace Bess::Vulkan::Pipelines
