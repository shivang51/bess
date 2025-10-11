#include "scene/renderer/vulkan/pipelines/path_fill_pipeline.h"
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

    PathFillPipeline::PathFillPipeline(const std::shared_ptr<VulkanDevice> &device,
                                       const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                                       VkExtent2D extent)
        : Pipeline(device, renderPass, extent) {
        createZoomUniformBuffers();
        createVertexBuffer();
        createIndexBuffer();
        createInstanceBuffer();
        createUniformBuffers();

        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();

        createGraphicsPipeline();

        ensurePathCapacity(instanceLimit * 4, instanceLimit * 6);
    }

    PathFillPipeline::~PathFillPipeline() {
        cleanup();
    }

    PathFillPipeline::PathFillPipeline(PathFillPipeline &&other) noexcept
        : Pipeline(std::move(other)),
          m_fillVertices(std::move(other.m_fillVertices)),
          m_fillIndices(std::move(other.m_fillIndices)),
          m_instances(std::move(other.m_instances)),
          m_vertexBuffers(std::move(other.m_vertexBuffers)),
          m_vertexBufferMemory(std::move(other.m_vertexBufferMemory)),
          m_indexBuffers(std::move(other.m_indexBuffers)),
          m_indexBufferMemory(std::move(other.m_indexBufferMemory)),
          m_instanceBuffers(std::move(other.m_instanceBuffers)),
          m_instanceBufferMemory(std::move(other.m_instanceBufferMemory)),
          m_currentVertexCapacity(other.m_currentVertexCapacity),
          m_currentIndexCapacity(other.m_currentIndexCapacity),
          m_currentInstanceCapacity(other.m_currentInstanceCapacity),
          m_zoomUniformBuffers(std::move(other.m_zoomUniformBuffers)),
          m_zoomUniformBufferMemory(std::move(other.m_zoomUniformBufferMemory)) {
        other.m_vertexBuffers.clear();
        other.m_vertexBufferMemory.clear();
        other.m_indexBuffers.clear();
        other.m_indexBufferMemory.clear();
        other.m_instanceBuffers.clear();
        other.m_instanceBufferMemory.clear();
        other.m_zoomUniformBuffers.clear();
        other.m_zoomUniformBufferMemory.clear();
        other.m_currentVertexCapacity = 0;
        other.m_currentIndexCapacity = 0;
        other.m_currentInstanceCapacity = 0;
    }

    PathFillPipeline &PathFillPipeline::operator=(PathFillPipeline &&other) noexcept {
        if (this != &other) {
            cleanup();
            Pipeline::operator=(std::move(other));
            m_fillVertices = std::move(other.m_fillVertices);
            m_fillIndices = std::move(other.m_fillIndices);
            m_instances = std::move(other.m_instances);
            m_vertexBuffers = std::move(other.m_vertexBuffers);
            m_vertexBufferMemory = std::move(other.m_vertexBufferMemory);
            m_indexBuffers = std::move(other.m_indexBuffers);
            m_indexBufferMemory = std::move(other.m_indexBufferMemory);
            m_instanceBuffers = std::move(other.m_instanceBuffers);
            m_instanceBufferMemory = std::move(other.m_instanceBufferMemory);
            m_currentVertexCapacity = other.m_currentVertexCapacity;
            m_currentIndexCapacity = other.m_currentIndexCapacity;
            m_currentInstanceCapacity = other.m_currentInstanceCapacity;
            m_zoomUniformBuffers = std::move(other.m_zoomUniformBuffers);
            m_zoomUniformBufferMemory = std::move(other.m_zoomUniformBufferMemory);

            other.m_vertexBuffers.clear();
            other.m_vertexBufferMemory.clear();
            other.m_indexBuffers.clear();
            other.m_indexBufferMemory.clear();
            other.m_instanceBuffers.clear();
            other.m_instanceBufferMemory.clear();
            other.m_zoomUniformBuffers.clear();
            other.m_zoomUniformBufferMemory.clear();
            other.m_currentVertexCapacity = 0;
            other.m_currentIndexCapacity = 0;
            other.m_currentInstanceCapacity = 0;
        }
        return *this;
    }

    void PathFillPipeline::beginPipeline(VkCommandBuffer commandBuffer) {
        m_currentCommandBuffer = commandBuffer;
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                                &m_descriptorSets[m_currentFrameIndex], 0, nullptr);

        vkCmdSetViewport(m_currentCommandBuffer, 0, 1, &m_viewport);
        vkCmdSetScissor(m_currentCommandBuffer, 0, 1, &m_scissor);
    }

    void PathFillPipeline::endPipeline() {
        auto &vertexBuffer = m_vertexBuffers[m_currentFrameIndex];
        if (!m_fillVertices.empty() && !m_fillIndices.empty()) {
            void *data = nullptr;
            VkDeviceSize bufferSize = m_fillVertices.size() * sizeof(CommonVertex);
            vkMapMemory(m_device->device(), m_vertexBufferMemory[m_currentFrameIndex], 0, bufferSize, 0, &data);
            memcpy(data, m_fillVertices.data(), bufferSize);
            vkUnmapMemory(m_device->device(), m_vertexBufferMemory[m_currentFrameIndex]);

            data = nullptr;
            bufferSize = m_fillIndices.size() * sizeof(uint32_t);
            vkMapMemory(m_device->device(), m_indexBufferMemory[m_currentFrameIndex], 0, bufferSize, 0, &data);
            memcpy(data, m_fillIndices.data(), bufferSize);
            vkUnmapMemory(m_device->device(), m_indexBufferMemory[m_currentFrameIndex]);

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, &vertexBuffer, &offset);
            vkCmdBindIndexBuffer(m_currentCommandBuffer, m_indexBuffers[m_currentFrameIndex], 0, VK_INDEX_TYPE_UINT32);
            
            if (!m_drawCalls.empty()) {
                for (const auto &dc : m_drawCalls) {
                    struct { glm::vec4 t; glm::vec4 s; } pc;
                    pc.t = glm::vec4(dc.translation, 0.0f);
                    pc.s = glm::vec4(dc.scale, 0.0f, 0.0f);
                    vkCmdPushConstants(m_currentCommandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                                       0, sizeof(pc), &pc);
                    vkCmdDrawIndexed(m_currentCommandBuffer, dc.indexCount, 1, dc.firstIndex, 0, 0);
                }
            } else {
                // Push translation + scale to GPU
                struct { glm::vec4 t; glm::vec4 s; } pc;
                pc.t = glm::vec4(m_translation, 0.0f);
                pc.s = glm::vec4(m_scale, 0.0f, 0.0f);
                vkCmdPushConstants(m_currentCommandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                                  0, sizeof(pc), &pc);
                
                // Draw
                vkCmdDrawIndexed(m_currentCommandBuffer, static_cast<uint32_t>(m_fillIndices.size()), 1, 0, 0, 0);
            }
        }

        m_fillVertices.clear();
        m_fillIndices.clear();
        m_instances.clear();
        m_drawCalls.clear();
        m_translation = glm::vec3(0.0f);
        m_scale = glm::vec2(1.0f);
        m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    void PathFillPipeline::setPathData(const std::vector<CommonVertex> &fillVertices, const std::vector<uint32_t> &fillIndices) {
        if (fillVertices.empty())
            return;

        m_fillVertices = fillVertices;
        m_fillIndices = fillIndices;

        ensurePathCapacity(m_fillVertices.size(), m_fillIndices.size());
    }

    void PathFillPipeline::setInstancedPathData(const std::vector<CommonVertex> &fillVertices,
                                               const std::vector<uint32_t> &fillIndices,
                                               const std::vector<PathInstance> &instances) {
        if (fillVertices.empty() || instances.empty())
            return;

        m_fillVertices = fillVertices;
        m_fillIndices = fillIndices;
        m_instances = instances;

        ensurePathCapacity(m_fillVertices.size(), m_fillIndices.size());
        ensureInstanceCapacity(m_instances.size());
    }

    void PathFillPipeline::setPathDataWithTranslation(const std::vector<CommonVertex> &fillVertices,
                                                     const std::vector<uint32_t> &fillIndices,
                                                     const glm::vec3 &translation) {
        if (fillVertices.empty())
            return;

        m_fillVertices = fillVertices;
        m_fillIndices = fillIndices;
        m_translation = translation;
        m_scale = glm::vec2(1.0f); // default scale

        ensurePathCapacity(m_fillVertices.size(), m_fillIndices.size());
    }

    void PathFillPipeline::setBatchedPathData(const std::vector<CommonVertex> &fillVertices,
                                              const std::vector<uint32_t> &fillIndices,
                                              const std::vector<FillDrawCall> &drawCalls) {
        if (fillVertices.empty() || fillIndices.empty() || drawCalls.empty())
            return;

        m_fillVertices = fillVertices;
        m_fillIndices = fillIndices;
        m_drawCalls = drawCalls;

        ensurePathCapacity(m_fillVertices.size(), m_fillIndices.size());
    }

    void PathFillPipeline::updateUniformBuffer(const UniformBufferObject &ubo) {
        Pipeline::updateUniformBuffer(ubo);
    }

    void PathFillPipeline::cleanup() {
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

        for (size_t i = 0; i < m_instanceBuffers.size(); i++) {
            if (m_instanceBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device->device(), m_instanceBuffers[i], nullptr);
            }
            if (m_instanceBufferMemory[i] != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_instanceBufferMemory[i], nullptr);
            }
        }

        Pipeline::cleanup();
    }

    void PathFillPipeline::createGraphicsPipeline() {
        auto vertShaderCode = readFile("assets/shaders/path_push_constants.vert.spv");
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
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

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

        // Define push constant range for translation
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(glm::vec4) * 2; // translation + scale

        // Create pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

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

    void PathFillPipeline::createVertexBuffer() {
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

    void PathFillPipeline::createIndexBuffer() {
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

    void PathFillPipeline::createInstanceBuffer() {
        m_instanceBuffers.resize(maxFrames);
        m_instanceBufferMemory.resize(maxFrames);

        for (size_t i = 0; i < maxFrames; i++) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(PathInstance) * instanceLimit;
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_instanceBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create path instance buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_device->device(), m_instanceBuffers[i], &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_instanceBufferMemory[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate path instance buffer memory!");
            }

            vkBindBufferMemory(m_device->device(), m_instanceBuffers[i], m_instanceBufferMemory[i], 0);
        }

        m_currentInstanceCapacity = instanceLimit;
    }

    void PathFillPipeline::ensureInstanceCapacity(size_t instanceCount) {
        if (instanceCount <= m_currentInstanceCapacity) {
            return; // Already have enough capacity
        }

        // Calculate new capacity with some headroom
        size_t newInstanceCapacity = std::max(instanceCount * 2, m_currentInstanceCapacity * 2);

        // Destroy old instance buffers
        for (size_t i = 0; i < m_instanceBuffers.size(); i++) {
            if (m_instanceBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device->device(), m_instanceBuffers[i], nullptr);
            }
            if (m_instanceBufferMemory[i] != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_instanceBufferMemory[i], nullptr);
            }
        }

        // Create new instance buffers with required capacity
        for (size_t i = 0; i < maxFrames; i++) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(PathInstance) * newInstanceCapacity;
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_instanceBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create resized path instance buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_device->device(), m_instanceBuffers[i], &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_instanceBufferMemory[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate resized path instance buffer memory!");
            }

            vkBindBufferMemory(m_device->device(), m_instanceBuffers[i], m_instanceBufferMemory[i], 0);
        }
        m_currentInstanceCapacity = newInstanceCapacity;
    }

    void PathFillPipeline::createDescriptorSetLayout() {
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

    void PathFillPipeline::createDescriptorPool() {
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

    void PathFillPipeline::createDescriptorSets() {
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

    void PathFillPipeline::ensurePathBuffers() {
        // This method can be used to ensure buffers are ready
    }

    void PathFillPipeline::ensurePathCapacity(size_t vertexCount, size_t indexCount) {
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

    void PathFillPipeline::createZoomUniformBuffers() {
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

    void PathFillPipeline::updateZoomUniformBuffer(float zoom) {
        if (m_zoomUniformBufferMemory.empty() || m_currentFrameIndex >= m_zoomUniformBufferMemory.size()) {
            return;
        }

        void *data = nullptr;
        vkMapMemory(m_device->device(), m_zoomUniformBufferMemory[m_currentFrameIndex], 0, sizeof(float), 0, &data);
        memcpy(data, &zoom, sizeof(float));
        vkUnmapMemory(m_device->device(), m_zoomUniformBufferMemory[m_currentFrameIndex]);
    }

} // namespace Bess::Vulkan::Pipelines
