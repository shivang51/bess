#include "scene/renderer/vulkan/pipelines/primitive_pipeline.h"
#include "scene/renderer/vulkan/pipelines/pipeline.h"
#include "vulkan_core.h"
#include "vulkan_texture.h"
#include <cstring>
#include <ranges>
#include <span>
#include <stdexcept>

namespace Bess::Vulkan::Pipelines {

    namespace {
        constexpr size_t kMaxFrames = Bess::Vulkan::VulkanCore::MAX_FRAMES_IN_FLIGHT;
        constexpr uint32_t kInstanceLimit = 10000;

        struct LocalPrimitiveVertex {
            glm::vec2 position;
            glm::vec2 uv;
        };
    } // namespace

    PrimitivePipeline::PrimitivePipeline(const std::shared_ptr<VulkanDevice> &device,
                                         const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                                         VkExtent2D extent)
        : Pipeline(device, renderPass, extent) {
        if (!m_fallbackTexture) {
            uint32_t white = 0xFFFFFFFF;
            m_fallbackTexture = std::make_unique<VulkanTexture>(m_device, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, &white);
        }

        createPrimitiveBuffers();
        createUniformBuffers();

        m_texArraySetsCount = (kInstanceLimit * kMaxFrames) / 2.f;

        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();

        createGraphicsPipeline(false);
        createGraphicsPipeline(true);

        ensurePrimitiveInstanceCapacity(kInstanceLimit);
    }

    PrimitivePipeline::~PrimitivePipeline() {
        cleanup();
    }

    PrimitivePipeline::PrimitivePipeline(PrimitivePipeline &&other) noexcept
        : Pipeline(std::move(other)),
          m_buffers(std::move(other.m_buffers)),
          m_textureArrayDescriptorPool(other.m_textureArrayDescriptorPool),
          m_textureArrayLayout(other.m_textureArrayLayout),
          m_textureArraySets(std::move(other.m_textureArraySets)),
          m_fallbackTexture(std::move(other.m_fallbackTexture)),
          m_cachedTextureInfos(std::move(other.m_cachedTextureInfos)),
          m_texDescSetIdx(other.m_texDescSetIdx),
          m_texArraySetsCount(other.m_texArraySetsCount),
          m_texSetsGrowthFactor(other.m_texSetsGrowthFactor),
          m_texSetsMinHeadroom(other.m_texSetsMinHeadroom),
          m_texSetsMaxCap(other.m_texSetsMaxCap),
          m_tempDescSets(std::move(other.m_tempDescSets)) {
        other.m_textureArrayDescriptorPool = VK_NULL_HANDLE;
        other.m_textureArrayLayout = VK_NULL_HANDLE;
    }

    PrimitivePipeline &PrimitivePipeline::operator=(PrimitivePipeline &&other) noexcept {
        if (this != &other) {
            cleanup();
            Pipeline::operator=(std::move(other));
            m_buffers = std::move(other.m_buffers);
            m_textureArrayDescriptorPool = other.m_textureArrayDescriptorPool;
            m_textureArrayLayout = other.m_textureArrayLayout;
            m_textureArraySets = std::move(other.m_textureArraySets);
            m_fallbackTexture = std::move(other.m_fallbackTexture);
            m_cachedTextureInfos = std::move(other.m_cachedTextureInfos);
            m_texDescSetIdx = other.m_texDescSetIdx;
            m_texArraySetsCount = other.m_texArraySetsCount;
            m_texSetsGrowthFactor = other.m_texSetsGrowthFactor;
            m_texSetsMinHeadroom = other.m_texSetsMinHeadroom;
            m_texSetsMaxCap = other.m_texSetsMaxCap;
            m_tempDescSets = std::move(other.m_tempDescSets);

            other.m_textureArrayDescriptorPool = VK_NULL_HANDLE;
            other.m_textureArrayLayout = VK_NULL_HANDLE;
        }

        return *this;
    }

    void PrimitivePipeline::beginPipeline(VkCommandBuffer commandBuffer, bool isTranslucent) {
        m_currentCommandBuffer = commandBuffer;

        vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          isTranslucent ? m_translucentPipeline : m_opaquePipeline);
        vkCmdSetViewport(m_currentCommandBuffer, 0, 1, &m_viewport);
        vkCmdSetScissor(m_currentCommandBuffer, 0, 1, &m_scissor);
    }

    void PrimitivePipeline::endPipeline() {
        m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    void PrimitivePipeline::drawPrimitiveInstances(
        VkCommandBuffer commandBuffer,
        bool isTranslucent,
        const std::vector<PrimitiveInstance> &instances,
        const std::unordered_map<std::shared_ptr<VulkanTexture>, std::vector<PrimitiveInstance>> &texturedInstances) {
        const bool hasOpaqueBatch = !instances.empty();
        const bool hasTexturedBatch = std::ranges::any_of(texturedInstances, [](const auto &entry) {
            return !entry.second.empty();
        });
        if (!hasOpaqueBatch && !hasTexturedBatch) {
            return;
        }

        beginPipeline(commandBuffer, isTranslucent);

        auto makeFallbackTextureInfos = [&]() {
            std::array<VkDescriptorImageInfo, m_texArraySize> infos{};
            infos.fill(m_fallbackTexture->getDescriptorInfo());
            return infos;
        };

        if (hasOpaqueBatch) {
            updateTextureSet(m_texDescSetIdx, makeFallbackTextureInfos());
            bindDescriptorSets(isTranslucent, getTextureArraySet(m_texDescSetIdx));
            drawBatch(instances);
            ++m_texDescSetIdx;
        }

        std::vector<PrimitiveInstance> texturedBatch;
        texturedBatch.reserve(kInstanceLimit);

        auto textureInfos = makeFallbackTextureInfos();
        size_t slot = 1;

        auto flushTexturedBatch = [&]() {
            if (texturedBatch.empty()) {
                return;
            }

            updateTextureSet(m_texDescSetIdx, textureInfos);
            bindDescriptorSets(isTranslucent, getTextureArraySet(m_texDescSetIdx));
            drawBatch(texturedBatch);
            texturedBatch.clear();
            textureInfos = makeFallbackTextureInfos();
            slot = 1;
            ++m_texDescSetIdx;
        };

        for (const auto &[texture, bucket] : texturedInstances) {
            if (bucket.empty()) {
                continue;
            }

            if (slot >= m_texArraySize) {
                flushTexturedBatch();
            }

            BESS_ASSERT(slot < m_texArraySize, "Texture slot must fit within descriptor array");
            textureInfos[slot] = texture ? texture->getDescriptorInfo() : m_fallbackTexture->getDescriptorInfo();

            for (const auto &instance : bucket) {
                auto copy = instance;
                copy.texSlotIdx = static_cast<int32_t>(slot);
                texturedBatch.emplace_back(copy);
            }

            ++slot;
        }

        flushTexturedBatch();
        endPipeline();
    }

    void PrimitivePipeline::cleanup() {
        if (m_buffers.instanceBufferMemory != VK_NULL_HANDLE && m_buffers.instanceBufferMapped != nullptr) {
            vkUnmapMemory(m_device->device(), m_buffers.instanceBufferMemory);
            m_buffers.instanceBufferMapped = nullptr;
        }

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

        for (auto buffer : m_buffers.retiredInstanceBuffers) {
            if (buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device->device(), buffer, nullptr);
            }
        }
        m_buffers.retiredInstanceBuffers.clear();

        for (auto memory : m_buffers.retiredInstanceMemories) {
            if (memory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), memory, nullptr);
            }
        }
        m_buffers.retiredInstanceMemories.clear();

        if (m_textureArrayDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device->device(), m_textureArrayDescriptorPool, nullptr);
            m_textureArrayDescriptorPool = VK_NULL_HANDLE;
        }

        if (m_textureArrayLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device->device(), m_textureArrayLayout, nullptr);
            m_textureArrayLayout = VK_NULL_HANDLE;
        }

        m_cachedTextureInfos.clear();
        m_tempDescSets.reset(m_device->device());

        Pipeline::cleanup();
    }

    void PrimitivePipeline::createGraphicsPipeline(bool isTranslucent) {
        auto vertShaderCode = readFile("assets/shaders/primitive.vert.spv");
        auto fragShaderCode = readFile("assets/shaders/primitive.frag.spv");

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

        VkVertexInputBindingDescription binding0{};
        binding0.binding = 0;
        binding0.stride = sizeof(LocalPrimitiveVertex);
        binding0.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputBindingDescription binding1{};
        binding1.binding = 1;
        binding1.stride = sizeof(PrimitiveInstance);
        binding1.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        std::array<VkVertexInputBindingDescription, 2> bindings = {binding0, binding1};

        std::array<VkVertexInputAttributeDescription, 2> localAttribs{};
        localAttribs[0].binding = 0;
        localAttribs[0].location = 0;
        localAttribs[0].format = VK_FORMAT_R32G32_SFLOAT;
        localAttribs[0].offset = offsetof(LocalPrimitiveVertex, position);

        localAttribs[1].binding = 0;
        localAttribs[1].location = 1;
        localAttribs[1].format = VK_FORMAT_R32G32_SFLOAT;
        localAttribs[1].offset = offsetof(LocalPrimitiveVertex, uv);

        std::array<VkVertexInputAttributeDescription, 13> instanceAttribs{};
        instanceAttribs[0].binding = 1;
        instanceAttribs[0].location = 2;
        instanceAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        instanceAttribs[0].offset = offsetof(PrimitiveInstance, position);

        instanceAttribs[1].binding = 1;
        instanceAttribs[1].location = 3;
        instanceAttribs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[1].offset = offsetof(PrimitiveInstance, color);

        instanceAttribs[2].binding = 1;
        instanceAttribs[2].location = 4;
        instanceAttribs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[2].offset = offsetof(PrimitiveInstance, borderRadius);

        instanceAttribs[3].binding = 1;
        instanceAttribs[3].location = 5;
        instanceAttribs[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[3].offset = offsetof(PrimitiveInstance, borderColor);

        instanceAttribs[4].binding = 1;
        instanceAttribs[4].location = 6;
        instanceAttribs[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[4].offset = offsetof(PrimitiveInstance, borderSize);

        instanceAttribs[5].binding = 1;
        instanceAttribs[5].location = 7;
        instanceAttribs[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[5].offset = offsetof(PrimitiveInstance, texData);

        instanceAttribs[6].binding = 1;
        instanceAttribs[6].location = 8;
        instanceAttribs[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        instanceAttribs[6].offset = offsetof(PrimitiveInstance, primitiveData);

        instanceAttribs[7].binding = 1;
        instanceAttribs[7].location = 9;
        instanceAttribs[7].format = VK_FORMAT_R32G32_SFLOAT;
        instanceAttribs[7].offset = offsetof(PrimitiveInstance, size);

        instanceAttribs[8].binding = 1;
        instanceAttribs[8].location = 10;
        instanceAttribs[8].format = VK_FORMAT_R32G32_UINT;
        instanceAttribs[8].offset = offsetof(PrimitiveInstance, id);

        instanceAttribs[9].binding = 1;
        instanceAttribs[9].location = 11;
        instanceAttribs[9].format = VK_FORMAT_R32_SINT;
        instanceAttribs[9].offset = offsetof(PrimitiveInstance, primitiveType);

        instanceAttribs[10].binding = 1;
        instanceAttribs[10].location = 12;
        instanceAttribs[10].format = VK_FORMAT_R32_SINT;
        instanceAttribs[10].offset = offsetof(PrimitiveInstance, isMica);

        instanceAttribs[11].binding = 1;
        instanceAttribs[11].location = 13;
        instanceAttribs[11].format = VK_FORMAT_R32_SINT;
        instanceAttribs[11].offset = offsetof(PrimitiveInstance, texSlotIdx);

        instanceAttribs[12].binding = 1;
        instanceAttribs[12].location = 14;
        instanceAttribs[12].format = VK_FORMAT_R32_SFLOAT;
        instanceAttribs[12].offset = offsetof(PrimitiveInstance, angle);

        std::array<VkVertexInputAttributeDescription, 15> allAttribs{};
        std::ranges::copy(localAttribs, allAttribs.begin());
        std::ranges::copy(instanceAttribs, allAttribs.begin() + localAttribs.size());

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
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        VkPipelineColorBlendAttachmentState pickingBlendAttachment = colorBlendAttachment;
        pickingBlendAttachment.blendEnable = VK_FALSE;

        static const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {
            colorBlendAttachment,
            pickingBlendAttachment,
        };
        auto colorBlending = createColorBlendState(colorBlendAttachments);

        BESS_ASSERT(m_descriptorSetLayout != VK_NULL_HANDLE, "Primary descriptor set layout must exist");
        BESS_ASSERT(m_textureArrayLayout != VK_NULL_HANDLE, "Texture descriptor set layout must exist");

        std::array<VkDescriptorSetLayout, 2> setLayouts = {m_descriptorSetLayout, m_textureArrayLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
        pipelineLayoutInfo.pSetLayouts = setLayouts.data();

        VkPipelineLayout *pipelineLayout = isTranslucent ? &m_transPipelineLayout : &m_opaquePipelineLayout;
        if (vkCreatePipelineLayout(m_device->device(), &pipelineLayoutInfo, nullptr, pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create primitive pipeline layout!");
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
        pipelineInfo.layout = *pipelineLayout;
        pipelineInfo.renderPass = m_renderPass->getVkHandle();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        VkPipeline *pipeline = isTranslucent ? &m_translucentPipeline : &m_opaquePipeline;
        if (vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create primitive graphics pipeline!");
        }

        vkDestroyShaderModule(m_device->device(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device->device(), vertShaderModule, nullptr);
    }

    void PrimitivePipeline::createPrimitiveBuffers() {
        if (m_buffers.vertexBuffer != VK_NULL_HANDLE) {
            return;
        }

        std::array<LocalPrimitiveVertex, 4> local = {{
            {{-0.5f, -0.5f}, {0.f, 0.f}},
            {{0.5f, -0.5f}, {1.f, 0.f}},
            {{0.5f, 0.5f}, {1.f, 1.f}},
            {{-0.5f, 0.5f}, {0.f, 1.f}},
        }};
        std::array<uint32_t, 6> idx = {{0, 1, 2, 2, 3, 0}};

        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        VkMemoryRequirements req{};

        const VkDeviceSize vSize = sizeof(LocalPrimitiveVertex) * local.size();
        bi.size = vSize;
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_buffers.vertexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create primitive vertex buffer!");
        }

        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.vertexBuffer, &req);
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_buffers.vertexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate primitive vertex buffer memory!");
        }

        vkBindBufferMemory(m_device->device(), m_buffers.vertexBuffer, m_buffers.vertexBufferMemory, 0);
        void *vdata = nullptr;
        vkMapMemory(m_device->device(), m_buffers.vertexBufferMemory, 0, vSize, 0, &vdata);
        std::memcpy(vdata, local.data(), vSize);
        vkUnmapMemory(m_device->device(), m_buffers.vertexBufferMemory);

        const VkDeviceSize iSize = sizeof(uint32_t) * idx.size();
        bi.size = iSize;
        bi.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_buffers.indexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create primitive index buffer!");
        }

        vkGetBufferMemoryRequirements(m_device->device(), m_buffers.indexBuffer, &req);
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_buffers.indexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate primitive index buffer memory!");
        }

        vkBindBufferMemory(m_device->device(), m_buffers.indexBuffer, m_buffers.indexBufferMemory, 0);
        void *idata = nullptr;
        vkMapMemory(m_device->device(), m_buffers.indexBufferMemory, 0, iSize, 0, &idata);
        std::memcpy(idata, idx.data(), iSize);
        vkUnmapMemory(m_device->device(), m_buffers.indexBufferMemory);

        ensurePrimitiveInstanceCapacity(1);
    }

    void PrimitivePipeline::ensurePrimitiveInstanceCapacity(size_t instanceCount) {
        ensureInstanceCapacity(m_buffers, instanceCount, sizeof(PrimitiveInstance));
    }

    VkDescriptorPool PrimitivePipeline::createTextureDescriptorPool(uint32_t maxSets, uint32_t descriptorCount) {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = descriptorCount;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = maxSets;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(m_device->device(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            BESS_ERROR("[PrimitivePipeline] Failed to create texture descriptor pool");
        }

        return descriptorPool;
    }

    void PrimitivePipeline::createDescriptorPool() {
        Pipeline::createDescriptorPool();

        if (m_textureArrayDescriptorPool != VK_NULL_HANDLE) {
            return;
        }

        m_textureArrayDescriptorPool = createTextureDescriptorPool(m_texArraySetsCount, m_texArraySize * m_texArraySetsCount);
    }

    void PrimitivePipeline::createTextureDescriptorSets(uint32_t descCount,
                                                        uint32_t setsCount,
                                                        VkDescriptorPool pool,
                                                        VkDescriptorSetLayout &layout,
                                                        std::vector<VkDescriptorSet> &sets) {
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = 2;
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layoutBinding.descriptorCount = descCount;
        layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &layoutBinding;

        if (layout == VK_NULL_HANDLE) {
            if (vkCreateDescriptorSetLayout(m_device->device(), &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create texture array descriptor set layout");
            }
        }

        std::vector<VkDescriptorSetLayout> layouts(setsCount, layout);

        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = setsCount;
        allocInfo.pSetLayouts = layouts.data();

        sets.resize(setsCount);
        if (vkAllocateDescriptorSets(m_device->device(), &allocInfo, sets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate texture array descriptor sets");
        }
    }

    void PrimitivePipeline::createDescriptorSets() {
        Pipeline::createDescriptorSets();
        BESS_ASSERT(m_textureArrayDescriptorPool != VK_NULL_HANDLE, "Texture descriptor pool must be valid");
        createTextureDescriptorSets(m_texArraySize, m_texArraySetsCount, m_textureArrayDescriptorPool, m_textureArrayLayout, m_textureArraySets);
    }

    void PrimitivePipeline::bindDescriptorSets(bool isTranslucent, VkDescriptorSet textureSet) const {
        VkDescriptorSet sets[] = {m_descriptorSets[m_currentFrameIndex], textureSet};
        vkCmdBindDescriptorSets(m_currentCommandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                isTranslucent ? m_transPipelineLayout : m_opaquePipelineLayout,
                                0, 2, sets, 0, nullptr);
    }

    void PrimitivePipeline::updateTextureSet(size_t descriptorSetIndex,
                                             const std::array<VkDescriptorImageInfo, PrimitivePipeline::m_texArraySize> &textureInfos) {
        VkDescriptorSet textureSet = getTextureArraySet(descriptorSetIndex);
        BESS_ASSERT(textureSet != VK_NULL_HANDLE, "Texture descriptor set must be valid");

        bool needsUpdate = true;
        if (m_cachedTextureInfos.contains(descriptorSetIndex)) {
            needsUpdate = std::memcmp(m_cachedTextureInfos.at(descriptorSetIndex).data(),
                                      textureInfos.data(),
                                      sizeof(VkDescriptorImageInfo) * textureInfos.size()) != 0;
        }

        if (!needsUpdate) {
            return;
        }

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(textureInfos.size());

        for (uint32_t i = 0; i < textureInfos.size(); ++i) {
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = textureSet;
            write.dstBinding = 2;
            write.dstArrayElement = i;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &textureInfos[i];
            writes.push_back(write);
        }

        vkUpdateDescriptorSets(m_device->device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        m_cachedTextureInfos[descriptorSetIndex] = textureInfos;
    }

    void PrimitivePipeline::drawBatch(const std::vector<PrimitiveInstance> &instances) {
        if (instances.empty()) {
            return;
        }

        ensurePrimitiveInstanceCapacity(m_instanceCounter + instances.size());

        size_t remaining = instances.size();
        size_t offset = 0;

        while (remaining > 0) {
            const size_t batchSize = std::min(remaining, static_cast<size_t>(kInstanceLimit));
            const VkDeviceSize byteOffset = (m_instanceCounter + offset) * sizeof(PrimitiveInstance);

            BESS_ASSERT(m_buffers.instanceBufferMapped != nullptr, "Instance buffer must be mapped");
            std::memcpy(static_cast<char *>(m_buffers.instanceBufferMapped) + byteOffset,
                        instances.data() + static_cast<ptrdiff_t>(offset),
                        sizeof(PrimitiveInstance) * batchSize);

            VkBuffer vertexBuffers[] = {m_buffers.vertexBuffer, m_buffers.instanceBuffer};
            VkDeviceSize offsets[] = {0, byteOffset};
            vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 2, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(m_currentCommandBuffer, m_buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(m_currentCommandBuffer, 6, static_cast<uint32_t>(batchSize), 0, 0, 0);

            offset += batchSize;
            remaining -= batchSize;
        }

        m_instanceCounter += instances.size();
    }

    VkDescriptorSet PrimitivePipeline::getTextureArraySet(size_t idx) {
        if (isTexArraySetAvailable(idx)) {
            return m_textureArraySets[idx];
        }

        if (m_tempDescSets.isDescSetAvailable(idx)) {
            BESS_WARN("[PrimitivePipeline] Reusing temp descriptor set for idx {}", idx);
            return m_tempDescSets.getSetAtIdx(idx);
        }

        BESS_INFO("[PrimitivePipeline] Created a temp descriptor pool for idx {}", idx);

        auto [pool, sets] = m_tempDescSets.reserveNextPool();
        pool = createTextureDescriptorPool(static_cast<uint32_t>(sets.size()), static_cast<uint32_t>(m_texArraySize * sets.size()));
        createTextureDescriptorSets(static_cast<uint32_t>(m_texArraySize), static_cast<uint32_t>(sets.size()), pool, m_textureArrayLayout, sets);

        auto set = m_tempDescSets.getSetAtIdx(idx);
        BESS_ASSERT(set != VK_NULL_HANDLE, "Temporary descriptor set must be valid");
        return set;
    }

    void PrimitivePipeline::resizeTexArrayDescriptorPool(uint64_t size) {
        m_device->waitForIdle();

        if (m_textureArrayDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device->device(), m_textureArrayDescriptorPool, nullptr);
        }

        m_texArraySetsCount = size;
        m_textureArraySets.clear();
        m_cachedTextureInfos.clear();
        m_textureArrayDescriptorPool = createTextureDescriptorPool(static_cast<uint32_t>(m_texArraySetsCount),
                                                                   static_cast<uint32_t>(m_texArraySize * m_texArraySetsCount));
        createTextureDescriptorSets(static_cast<uint32_t>(m_texArraySize),
                                    static_cast<uint32_t>(m_texArraySetsCount),
                                    m_textureArrayDescriptorPool,
                                    m_textureArrayLayout,
                                    m_textureArraySets);

        BESS_INFO("[PrimitivePipeline] Resized texture array descriptor sets to size {}", m_texArraySetsCount);
    }

    void PrimitivePipeline::cleanPrevStateCounter() {
        Pipeline::cleanPrevStateCounter();

        if (m_tempDescSets.maxExhaustedSize != 0) {
            const size_t required = m_texArraySetsCount + m_tempDescSets.maxExhaustedSize;
            const size_t headroom = std::max<size_t>(m_texSetsMinHeadroom, required / 2);
            const double scaled = static_cast<double>(required) * static_cast<double>(m_texSetsGrowthFactor);
            const size_t target = std::min<size_t>(static_cast<size_t>(scaled) + headroom, static_cast<size_t>(m_texSetsMaxCap));
            resizeTexArrayDescriptorPool(target);
            m_tempDescSets.reset(m_device->device());
        }

        m_texDescSetIdx = static_cast<uint32_t>(textureSetFrameBase());
    }

    void PrimitivePipeline::setTextureSetGrowthPolicy(float growthFactor, uint32_t minHeadroom, uint32_t maxSetsCap) {
        if (growthFactor >= 1.0f) {
            m_texSetsGrowthFactor = growthFactor;
        }

        m_texSetsMinHeadroom = minHeadroom;
        m_texSetsMaxCap = std::max<uint32_t>(maxSetsCap, static_cast<uint32_t>(m_texArraySetsCount));
    }

    bool PrimitivePipeline::isTexArraySetAvailable(size_t idx) const {
        return idx >= textureSetFrameBase() &&
               idx < textureSetFrameLimit() &&
               idx < m_textureArraySets.size() &&
               m_textureArraySets[idx] != VK_NULL_HANDLE;
    }

    size_t PrimitivePipeline::textureSetsPerFrame() const {
        return std::max<size_t>(1, m_texArraySetsCount / kMaxFrames);
    }

    size_t PrimitivePipeline::textureSetFrameBase() const {
        return static_cast<size_t>(m_currentFrameIndex) * textureSetsPerFrame();
    }

    size_t PrimitivePipeline::textureSetFrameLimit() const {
        return textureSetFrameBase() + textureSetsPerFrame();
    }

} // namespace Bess::Vulkan::Pipelines
