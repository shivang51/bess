#include "scene/renderer/vulkan/pipelines/grid_pipeline.h"
#include "common/log.h"
#include <glm.hpp>
#include <ext/matrix_transform.hpp>

namespace Bess::Renderer2D::Vulkan::Pipelines {

    GridPipeline::GridPipeline(const std::shared_ptr<VulkanDevice> &device,
                               const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                               VkExtent2D extent)
        : Pipeline(device, renderPass, extent) {
        createGridUniformBuffers();
        createVertexBuffer();
        createIndexBuffer();
        createGraphicsPipeline();
    }

    GridPipeline::~GridPipeline() {
        cleanup();
    }

    GridPipeline::GridPipeline(GridPipeline &&other) noexcept
        : Pipeline(std::move(other)),
          m_buffers(std::move(other.m_buffers)),
          m_currentVertices(std::move(other.m_currentVertices)),
          m_currentIndices(std::move(other.m_currentIndices)),
          m_currentVertexCount(other.m_currentVertexCount),
          m_currentIndexCount(other.m_currentIndexCount),
          m_gridUniformBuffers(std::move(other.m_gridUniformBuffers)),
          m_gridUniformBufferMemory(std::move(other.m_gridUniformBufferMemory)) {
        other.m_currentVertexCount = 0;
        other.m_currentIndexCount = 0;
    }

    GridPipeline &GridPipeline::operator=(GridPipeline &&other) noexcept {
        if (this != &other) {
            cleanup();
            Pipeline::operator=(std::move(other));
            m_buffers = std::move(other.m_buffers);
            m_currentVertices = std::move(other.m_currentVertices);
            m_currentIndices = std::move(other.m_currentIndices);
            m_currentVertexCount = other.m_currentVertexCount;
            m_currentIndexCount = other.m_currentIndexCount;
            m_gridUniformBuffers = std::move(other.m_gridUniformBuffers);
            m_gridUniformBufferMemory = std::move(other.m_gridUniformBufferMemory);

            other.m_currentVertexCount = 0;
            other.m_currentIndexCount = 0;
        }
        return *this;
    }

    void GridPipeline::beginPipeline(VkCommandBuffer commandBuffer) {
        m_currentCommandBuffer = commandBuffer;
        m_currentVertices.clear();
        m_currentIndices.clear();
        m_currentVertexCount = 0;
        m_currentIndexCount = 0;

        vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        // Update descriptor binding 1 with latest GridUniforms buffer before binding
        VkDescriptorBufferInfo gridInfo{};
        gridInfo.buffer = m_gridUniformBuffers[0];
        gridInfo.offset = 0;
        gridInfo.range = sizeof(GridUniforms);

        VkWriteDescriptorSet gridWrite{};
        gridWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        gridWrite.dstSet = m_descriptorSet;
        gridWrite.dstBinding = 1;
        gridWrite.dstArrayElement = 0;
        gridWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        gridWrite.descriptorCount = 1;
        gridWrite.pBufferInfo = &gridInfo;

        vkUpdateDescriptorSets(m_device->device(), 1, &gridWrite, 0, nullptr);

        vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

        VkBuffer vertexBuffers[] = {m_buffers.vertexBuffer};
        constexpr VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(m_currentCommandBuffer, m_buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void GridPipeline::endPipeline() {
        if (m_currentVertexCount > 0) {
            vkCmdDrawIndexed(m_currentCommandBuffer, m_currentIndexCount, 1, 0, 0, 0);
        }
        m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    void GridPipeline::cleanup() {
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

        for (size_t i = 0; i < m_gridUniformBuffers.size(); i++) {
            vkDestroyBuffer(m_device->device(), m_gridUniformBuffers[i], nullptr);
            vkFreeMemory(m_device->device(), m_gridUniformBufferMemory[i], nullptr);
        }
    }

    void GridPipeline::drawGrid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridUniforms &gridUniforms) {
        // Build a quad centered at pos that covers the viewport span
        const float halfW = size.x * 0.5F;
        const float halfH = size.y * 0.5F;
        std::vector<GridVertex> vertices(4);

        auto transform = glm::translate(glm::mat4(1.0f), pos);
        transform = glm::scale(transform, {size.x, size.y, 1.f});

        struct TemplateVertex {
            glm::vec2 position;
            glm::vec2 texCoord;
        };

        constexpr std::array<TemplateVertex, 4> QuadTemplateVertices = {{
            {{-0.5f, -0.5f}, {0.0f, 0.0f}}, // Position, TexCoord (Bottom-left)
            {{0.5f, -0.5f}, {1.0f, 0.0f}},  // (Bottom-right)
            {{0.5f, 0.5f}, {1.0f, 1.0f}},   // (Top-right)
            {{-0.5f, 0.5f}, {0.0f, 1.0f}}   // (Top-left)
        }};

        for (int i = 0; i < 4; i++) {
            auto &vertex = vertices[i];
            vertex.position = transform * glm::vec4(QuadTemplateVertices[i].position, 0.f, 1.f);
            vertex.fragId = id;
            vertex.ar = size.x / size.y;
            vertex.fragColor = gridUniforms.gridMajorColor;
            vertex.texCoord = QuadTemplateVertices[i].texCoord;
        }

        std::vector<uint32_t> indices = {
            0, 1, 2, 2, 3, 0};

        updateVertexBuffer(vertices);
        updateIndexBuffer(indices);

        // Actually draw the grid immediately
        vkCmdDrawIndexed(m_currentCommandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        // Prevent endPipeline from issuing a second draw with stale buffers
        m_currentVertexCount = 0;
        m_currentIndexCount = 0;
    }

    void GridPipeline::updateGridUniforms(const GridUniforms &gridUniforms) {
        void *data = nullptr;
        vkMapMemory(m_device->device(), m_gridUniformBufferMemory[0], 0, sizeof(gridUniforms), 0, &data);
        memcpy(data, &gridUniforms, sizeof(gridUniforms));
        vkUnmapMemory(m_device->device(), m_gridUniformBufferMemory[0]);
    }

    void GridPipeline::createGraphicsPipeline() {
        auto vertShaderCode = readFile("assets/shaders/grid_vert.spv");
        auto fragShaderCode = readFile("assets/shaders/grid_line_frag.spv");

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

        auto bindingDescription = GridVertex::getBindingDescription();
        auto attributeDescriptions = GridVertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0F;
        viewport.y = 0.0F;
        viewport.width = static_cast<float>(m_extent.width);
        viewport.height = static_cast<float>(m_extent.height);
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_extent;

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
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;

        // Color attachment 0 (main color)
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        // Color attachment 1 (picking ID - identical to color attachment but no blending)
        VkPipelineColorBlendAttachmentState pickingBlendAttachment{};
        pickingBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        pickingBlendAttachment.blendEnable = VK_FALSE;
        pickingBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        pickingBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        pickingBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        pickingBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pickingBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        pickingBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        std::array<VkPipelineColorBlendAttachmentState, 2> colorBlendAttachments = {colorBlendAttachment, pickingBlendAttachment};

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
        colorBlending.pAttachments = colorBlendAttachments.data();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;

        if (vkCreatePipelineLayout(m_device->device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
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
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass->getVkHandle();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(m_device->device(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device->device(), vertShaderModule, nullptr);
    }

    void GridPipeline::createGridUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(GridUniforms);

        m_gridUniformBuffers.resize(1);
        m_gridUniformBufferMemory.resize(1);

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_gridUniformBuffers[0]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create grid uniform buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->device(), m_gridUniformBuffers[0], &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_gridUniformBufferMemory[0]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate grid uniform buffer memory!");
        }

        vkBindBufferMemory(m_device->device(), m_gridUniformBuffers[0], m_gridUniformBufferMemory[0], 0);
    }

    void GridPipeline::createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(GridVertex) * 1000; // Initial capacity

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_buffers.vertexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.vertexBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_buffers.vertexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate vertex buffer memory!");
        }

        vkBindBufferMemory(m_device->device(), m_buffers.vertexBuffer, m_buffers.vertexBufferMemory, 0);
    }

    void GridPipeline::createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(uint32_t) * 1000; // Initial capacity

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_buffers.indexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create index buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.indexBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_buffers.indexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate index buffer memory!");
        }

        vkBindBufferMemory(m_device->device(), m_buffers.indexBuffer, m_buffers.indexBufferMemory, 0);
    }

    void GridPipeline::updateVertexBuffer(const std::vector<GridVertex> &vertices) {
        m_currentVertices = vertices;
        m_currentVertexCount = static_cast<uint32_t>(vertices.size());

        void *data = nullptr;
        vkMapMemory(m_device->device(), m_buffers.vertexBufferMemory, 0, sizeof(GridVertex) * vertices.size(), 0, &data);
        memcpy(data, vertices.data(), sizeof(GridVertex) * vertices.size());
        vkUnmapMemory(m_device->device(), m_buffers.vertexBufferMemory);
    }

    void GridPipeline::updateIndexBuffer(const std::vector<uint32_t> &indices) {
        m_currentIndices = indices;
        m_currentIndexCount = static_cast<uint32_t>(indices.size());

        void *data = nullptr;
        vkMapMemory(m_device->device(), m_buffers.indexBufferMemory, 0, sizeof(uint32_t) * indices.size(), 0, &data);
        memcpy(data, indices.data(), sizeof(uint32_t) * indices.size());
        vkUnmapMemory(m_device->device(), m_buffers.indexBufferMemory);
    }

} // namespace Bess::Renderer2D::Vulkan::Pipelines
