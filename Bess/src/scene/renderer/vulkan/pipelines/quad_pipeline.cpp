#include "scene/renderer/vulkan/pipelines/quad_pipeline.h"
#include "common/log.h"
#include <cstring>


namespace Bess::Renderer2D::Vulkan::Pipelines {

    QuadPipeline::QuadPipeline(const std::shared_ptr<VulkanDevice> &device,
                               const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                               VkExtent2D extent)
        : Pipeline(device, renderPass, extent) {
        createDescriptorSets(); // Create descriptor sets after uniform buffers are ready
        ensureQuadBuffers();
    }

    QuadPipeline::~QuadPipeline() {
        cleanup();
    }

    QuadPipeline::QuadPipeline(QuadPipeline &&other) noexcept
        : Pipeline(std::move(other)),
          m_buffers(std::move(other.m_buffers)),
          m_pendingQuadInstances(std::move(other.m_pendingQuadInstances)) {
    }

    QuadPipeline &QuadPipeline::operator=(QuadPipeline &&other) noexcept {
        if (this != &other) {
            cleanup();
            Pipeline::operator=(std::move(other));
            m_buffers = std::move(other.m_buffers);
            m_pendingQuadInstances = std::move(other.m_pendingQuadInstances);
        }
        return *this;
    }

    void QuadPipeline::beginPipeline(VkCommandBuffer commandBuffer) {
        m_currentCommandBuffer = commandBuffer;
        m_pendingQuadInstances.clear();
    }

    void QuadPipeline::endPipeline() {
        if (!m_pendingQuadInstances.empty()) {
            ensureQuadInstanceCapacity(m_pendingQuadInstances.size());

            // Upload all instances to GPU
            void *data = nullptr;
            vkMapMemory(m_device->device(), m_buffers.instanceBufferMemory, 0, sizeof(QuadInstance) * m_pendingQuadInstances.size(), 0, &data);
            memcpy(data, m_pendingQuadInstances.data(), sizeof(QuadInstance) * m_pendingQuadInstances.size());
            vkUnmapMemory(m_device->device(), m_buffers.instanceBufferMemory);

            // Create quad pipeline if not exists
            if (m_pipeline == VK_NULL_HANDLE) {
                createQuadPipeline();
            }

            // Bind quad pipeline and buffers
            vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

            VkBuffer vbs[] = {m_buffers.vertexBuffer, m_buffers.instanceBuffer};
            VkDeviceSize offs[] = {0, 0};
            vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 2, vbs, offs);
            vkCmdBindIndexBuffer(m_currentCommandBuffer, m_buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[m_currentFrameIndex], 0, nullptr);

            // Draw all instances in one call
            vkCmdDrawIndexed(m_currentCommandBuffer, 6, static_cast<uint32_t>(m_pendingQuadInstances.size()), 0, 0, 0);
            m_pendingQuadInstances.clear();
        }

        m_currentCommandBuffer = VK_NULL_HANDLE;
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
    }

    void QuadPipeline::drawQuad(const glm::vec3 &pos,
                                const glm::vec2 &size,
                                const glm::vec4 &color,
                                int id,
                                const glm::vec4 &borderRadius,
                                const glm::vec4 &borderSize,
                                const glm::vec4 &borderColor,
                                int isMica) {

        ensureQuadBuffers();

        // Create quad instance and queue it for batching
        QuadInstance instance{};
        instance.position = pos;
        instance.color = color;
        instance.borderRadius = borderRadius;
        instance.borderColor = borderColor;
        instance.borderSize = borderSize;
        instance.size = size;
        instance.id = id;
        instance.isMica = isMica;
        instance.texSlotIdx = 0;
        instance.texData = glm::vec4(0.0f, 0.0f, 1.f, 1.f); // Full local UV [0,1] by default

        m_pendingQuadInstances.push_back(instance);
    }

    void QuadPipeline::createQuadPipeline() {
        auto vertShaderCode = readFile("assets/shaders/quad_vert.spv");
        auto fragShaderCode = readFile("assets/shaders/quad_frag.spv");

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

        // Color attachment 0 (main color) - disable blending to match picking attachment
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE; // Disabled to match picking attachment (independentBlend not available)
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        // Color attachment 1 (picking ID - identical to color attachment)
        VkPipelineColorBlendAttachmentState pickingBlendAttachment = colorBlendAttachment;

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
            throw std::runtime_error("Failed to create quad pipeline layout!");
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
            throw std::runtime_error("Failed to create quad graphics pipeline!");
        }

        vkDestroyShaderModule(m_device->device(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device->device(), vertShaderModule, nullptr);
    }

    void QuadPipeline::ensureQuadBuffers() {
        if (m_buffers.vertexBuffer != VK_NULL_HANDLE)
            return;

        // Create static quad vertex buffer
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

        // Allocate and upload quad vertex buffer
        VkDeviceSize vsize = sizeof(LocalVertex) * local.size();
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = vsize;
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
        vkMapMemory(m_device->device(), m_buffers.vertexBufferMemory, 0, vsize, 0, &vdata);
        memcpy(vdata, local.data(), vsize);
        vkUnmapMemory(m_device->device(), m_buffers.vertexBufferMemory);

        // Allocate and upload quad index buffer
        VkDeviceSize isize = sizeof(uint32_t) * idx.size();
        bi.size = isize;
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
        vkMapMemory(m_device->device(), m_buffers.indexBufferMemory, 0, isize, 0, &idata);
        memcpy(idata, idx.data(), isize);
        vkUnmapMemory(m_device->device(), m_buffers.indexBufferMemory);

        // Create initial instance buffer with capacity for 1 instance
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

} // namespace Bess::Renderer2D::Vulkan::Pipelines
