#include "scene/renderer/vulkan/pipelines/circle_pipeline.h"
#include "primitive_vertex.h"
#include "scene/renderer/vulkan/pipelines/pipeline.h"
#include "vulkan_core.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan::Pipelines {

    constexpr uint32_t instanceLimit = 10000;

    CirclePipeline::CirclePipeline(const std::shared_ptr<VulkanDevice> &device,
                                   const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                                   VkExtent2D extent)
        : Pipeline(device, renderPass, extent) {

        createCircleBuffers();
        createUniformBuffers();

        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();

        createGraphicsPipeline(false);
        createGraphicsPipeline(true);

        ensureCircleInstanceCapacity(instanceLimit);
    }

    CirclePipeline::~CirclePipeline() {
        cleanup();
    }

    CirclePipeline::CirclePipeline(CirclePipeline &&other) noexcept
        : Pipeline(std::move(other)),
          m_buffers(other.m_buffers),
          m_instances(std::move(other.m_instances)) {
    }

    CirclePipeline &CirclePipeline::operator=(CirclePipeline &&other) noexcept {
        if (this != &other) {
            cleanup();
            Pipeline::operator=(std::move(other));
            m_buffers = other.m_buffers;
            m_instances = std::move(other.m_instances);
        }
        return *this;
    }

    void CirclePipeline::beginPipeline(VkCommandBuffer commandBuffer, bool isTranslucent) {
        m_currentCommandBuffer = commandBuffer;
        m_instances.clear();

        vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          isTranslucent ? m_translucentPipeline : m_opaquePipeline);
        vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                isTranslucent ? m_transPipelineLayout : m_opaquePipelineLayout,
                                0, 1, &m_descriptorSets[m_currentFrameIndex], 0, nullptr);

        vkCmdSetViewport(m_currentCommandBuffer, 0, 1, &m_viewport);
        vkCmdSetScissor(m_currentCommandBuffer, 0, 1, &m_scissor);
    }

    void CirclePipeline::endPipeline() {
        if (m_instances.empty())
            return;

        auto draw = [&](const std::span<CircleInstance> &instances, VkDeviceSize offset) {
            if (instances.empty())
                return;
            void *data = nullptr;
            vkMapMemory(m_device->device(), m_buffers.instanceBufferMemory, offset, sizeof(CircleInstance) * instances.size(), 0, &data);
            memcpy(data, instances.data(), sizeof(CircleInstance) * instances.size());
            vkUnmapMemory(m_device->device(), m_buffers.instanceBufferMemory);

            std::array<VkBuffer, 2> vbs = {m_buffers.vertexBuffer, m_buffers.instanceBuffer};
            std::array<VkDeviceSize, 2> offs = {0, offset};
            vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 2, vbs.data(), offs.data());
            vkCmdBindIndexBuffer(m_currentCommandBuffer, m_buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(m_currentCommandBuffer, 6, static_cast<uint32_t>(instances.size()), 0, 0, 0);
        };

        auto flush = [&](std::vector<CircleInstance> &instances, VkDeviceSize bufferOffset = 0) {
            if (instances.empty())
                return;

            size_t remaining = instances.size();
            size_t offset = 0;

            while (remaining > 0) {
                size_t batchSize = std::min(remaining, (size_t)instanceLimit);
                auto span = std::span(instances.begin() + offset, instances.begin() + offset + batchSize);
                draw(span, bufferOffset + (offset * sizeof(CircleInstance)));
                offset += batchSize;
                remaining -= batchSize;
            }
        };

        flush(m_instances, m_instanceCounter * sizeof(CircleInstance));
        m_instanceCounter += m_instances.size();
        m_instances.clear();

        m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    void CirclePipeline::setCirclesData(const std::vector<CircleInstance> &opaque) {
        auto size = opaque.size();
        if (size == 0)
            return;
        ensureCircleInstanceCapacity(size);
        m_instances = opaque;
    }

    void CirclePipeline::cleanup() {
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

    void CirclePipeline::createGraphicsPipeline(bool isTranslucent) {
        auto vertShaderCode = readFile("assets/shaders/circle.vert.spv");
        auto fragShaderCode = readFile("assets/shaders/circle.frag.spv");

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

        // Binding 0: local circle verts (position vec2 + texcoord vec2)
        VkVertexInputBindingDescription binding0{};
        binding0.binding = 0;
        binding0.stride = sizeof(float) * 4; // vec2 + vec2
        binding0.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // Binding 1: instance data
        VkVertexInputBindingDescription binding1{};
        binding1.binding = 1;
        binding1.stride = sizeof(CircleInstance);
        binding1.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        std::array<VkVertexInputBindingDescription, 2> bindings = {binding0, binding1};

        std::array<VkVertexInputAttributeDescription, 2> localAttribs{};
        localAttribs[0].binding = 0;
        localAttribs[0].location = 0;
        localAttribs[0].format = VK_FORMAT_R32G32_SFLOAT;
        localAttribs[0].offset = 0;

        localAttribs[1].binding = 0;
        localAttribs[1].location = 1;
        localAttribs[1].format = VK_FORMAT_R32G32_SFLOAT;
        localAttribs[1].offset = sizeof(float) * 2;

        std::array<VkVertexInputAttributeDescription, 5> instanceAttribs{};
        instanceAttribs[0].binding = 1;
        instanceAttribs[0].location = 2;
        instanceAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        instanceAttribs[0].offset = offsetof(CircleInstance, position);

        instanceAttribs[1].binding = 1;
        instanceAttribs[1].location = 3;
        instanceAttribs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[1].offset = offsetof(CircleInstance, color);

        instanceAttribs[2].binding = 1;
        instanceAttribs[2].location = 4;
        instanceAttribs[2].format = VK_FORMAT_R32_SFLOAT;
        instanceAttribs[2].offset = offsetof(CircleInstance, radius);

        instanceAttribs[3].binding = 1;
        instanceAttribs[3].location = 5;
        instanceAttribs[3].format = VK_FORMAT_R32_SFLOAT;
        instanceAttribs[3].offset = offsetof(CircleInstance, innerRadius);

        instanceAttribs[4].binding = 1;
        instanceAttribs[4].location = 6;
        instanceAttribs[4].format = VK_FORMAT_R32_SINT;
        instanceAttribs[4].offset = offsetof(CircleInstance, id);

        std::array<VkVertexInputAttributeDescription, 7> allAttribs{};
        std::copy(localAttribs.begin(), localAttribs.end(), allAttribs.begin());
        std::copy(instanceAttribs.begin(), instanceAttribs.end(), allAttribs.begin() + 2);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
        vertexInputInfo.pVertexBindingDescriptions = bindings.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(allAttribs.size());
        vertexInputInfo.pVertexAttributeDescriptions = allAttribs.data();

        auto inputAssembly = createInputAssemblyState();
        auto viewportState = createViewportState();
        auto rasterizer = createRasterizationState();
        auto multisampling = createMultisampleState();
        auto depthStencil = createDepthStencilState(isTranslucent);

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendAttachmentState pickingBlendAttachment = colorBlendAttachment;
        pickingBlendAttachment.blendEnable = VK_FALSE;

        static const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {colorBlendAttachment, pickingBlendAttachment};

        auto colorBlending = createColorBlendState(colorBlendAttachments);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

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
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = isTranslucent ? m_transPipelineLayout : m_opaquePipelineLayout;
        pipelineInfo.renderPass = m_renderPass->getVkHandle();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if (isTranslucent) {
            if (vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_translucentPipeline) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create path graphics pipeline!");
            }
        } else {
            if (vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_opaquePipeline) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create path graphics pipeline!");
            }
        }

        vkDestroyShaderModule(m_device->device(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device->device(), vertShaderModule, nullptr);
    }

    void CirclePipeline::createCircleBuffers() {
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

        VkDeviceSize vSize = sizeof(float) * local.size();
        bi.size = vSize;
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_buffers.vertexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create circle vertex buffer!");
        }
        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.vertexBuffer, &req);
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_buffers.vertexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate circle vertex buffer memory!");
        }
        vkBindBufferMemory(m_device->device(), m_buffers.vertexBuffer, m_buffers.vertexBufferMemory, 0);
        void *vdata = nullptr;
        vkMapMemory(m_device->device(), m_buffers.vertexBufferMemory, 0, vSize, 0, &vdata);
        memcpy(vdata, local.data(), vSize);
        vkUnmapMemory(m_device->device(), m_buffers.vertexBufferMemory);

        VkDeviceSize iSize = sizeof(uint32_t) * idx.size();
        bi.size = iSize;
        bi.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_buffers.indexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create circle index buffer!");
        }
        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.indexBuffer, &req);
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_buffers.indexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate circle index buffer memory!");
        }
        vkBindBufferMemory(m_device->device(), m_buffers.indexBuffer, m_buffers.indexBufferMemory, 0);
        void *idata = nullptr;
        vkMapMemory(m_device->device(), m_buffers.indexBufferMemory, 0, iSize, 0, &idata);
        memcpy(idata, idx.data(), iSize);
        vkUnmapMemory(m_device->device(), m_buffers.indexBufferMemory);

        ensureCircleInstanceCapacity(1);
    }

    void CirclePipeline::ensureCircleInstanceCapacity(size_t instanceCount) {
        if (instanceCount <= m_buffers.instanceCapacity) {
            return;
        }

        if (m_buffers.instanceBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->device(), m_buffers.instanceBuffer, nullptr);
            vkFreeMemory(m_device->device(), m_buffers.instanceBufferMemory, nullptr);
        }

        VkDeviceSize size = sizeof(CircleInstance) * instanceCount;
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = size;
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_buffers.instanceBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create circle instance buffer!");
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.instanceBuffer, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_buffers.instanceBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate circle instance buffer memory!");
        }
        vkBindBufferMemory(m_device->device(), m_buffers.instanceBuffer, m_buffers.instanceBufferMemory, 0);

        m_buffers.instanceCapacity = instanceCount;
    }

    constexpr size_t maxFrames = Bess::Vulkan::VulkanCore::MAX_FRAMES_IN_FLIGHT;
    void CirclePipeline::createDescriptorPool() {
        Pipeline::createDescriptorPool();
    }

    void CirclePipeline::createDescriptorSets() {
        Pipeline::createDescriptorSets();
    }

} // namespace Bess::Vulkan::Pipelines
