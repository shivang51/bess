#include "scene/renderer/vulkan/pipelines/pipeline.h"
#include "common/log.h"
#include <cstring>
#include <fstream>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan::Pipelines {

    Pipeline::Pipeline(const std::shared_ptr<VulkanDevice> &device,
                       const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                       VkExtent2D extent)
        : m_device(device), m_renderPass(renderPass), m_extent(extent) {
    }

    void Pipeline::cleanup() {
        for (size_t i = 0; i < m_uniformBuffers.size(); i++) {
            if (m_uniformBuffers[i] != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device->device(), m_uniformBuffers[i], nullptr);
            }
            if (m_uniformBufferMemory[i] != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_uniformBufferMemory[i], nullptr);
            }
        }

        if (m_descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device->device(), m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }

        if (m_descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device->device(), m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }

        if (m_opaquePipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device->device(), m_opaquePipeline, nullptr);
            m_opaquePipeline = VK_NULL_HANDLE;
        }

        if (m_opaquePipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device->device(), m_opaquePipelineLayout, nullptr);
            m_opaquePipelineLayout = VK_NULL_HANDLE;
        }
    }

    Pipeline::Pipeline(Pipeline &&other) noexcept
        : m_device(std::move(other.m_device)),
          m_renderPass(std::move(other.m_renderPass)),
          m_extent(other.m_extent),
          m_opaquePipeline(other.m_opaquePipeline),
          m_opaquePipelineLayout(other.m_opaquePipelineLayout),
          m_descriptorSetLayout(other.m_descriptorSetLayout),
          m_descriptorPool(other.m_descriptorPool),
          m_uniformBuffers(std::move(other.m_uniformBuffers)),
          m_uniformBufferMemory(std::move(other.m_uniformBufferMemory)),
          m_currentCommandBuffer(other.m_currentCommandBuffer) {
        other.m_opaquePipeline = VK_NULL_HANDLE;
        other.m_opaquePipelineLayout = VK_NULL_HANDLE;
        other.m_descriptorSetLayout = VK_NULL_HANDLE;
        other.m_descriptorPool = VK_NULL_HANDLE;
        other.m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    Pipeline &Pipeline::operator=(Pipeline &&other) noexcept {
        if (this != &other) {
            cleanup();
            m_device = std::move(other.m_device);
            m_renderPass = std::move(other.m_renderPass);
            m_extent = other.m_extent;
            m_opaquePipeline = other.m_opaquePipeline;
            m_opaquePipelineLayout = other.m_opaquePipelineLayout;
            m_descriptorSetLayout = other.m_descriptorSetLayout;
            m_descriptorPool = other.m_descriptorPool;
            m_uniformBuffers = std::move(other.m_uniformBuffers);
            m_uniformBufferMemory = std::move(other.m_uniformBufferMemory);
            m_currentCommandBuffer = other.m_currentCommandBuffer;

            other.m_opaquePipeline = VK_NULL_HANDLE;
            other.m_opaquePipelineLayout = VK_NULL_HANDLE;
            other.m_descriptorSetLayout = VK_NULL_HANDLE;
            other.m_descriptorPool = VK_NULL_HANDLE;
            other.m_currentCommandBuffer = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Pipeline::updateUniformBuffer(const UniformBufferObject &ubo) {
        void *data = nullptr;
        vkMapMemory(m_device->device(), m_uniformBufferMemory[0], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(m_device->device(), m_uniformBufferMemory[0]);
    }

    VkShaderModule Pipeline::createShaderModule(const std::vector<char> &code) const {
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

    std::vector<char> Pipeline::readFile(const std::string &filename) const {
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

    void Pipeline::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding gridLayoutBinding{};
        gridLayoutBinding.binding = 1;
        gridLayoutBinding.descriptorCount = 1;
        gridLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        gridLayoutBinding.pImmutableSamplers = nullptr;
        gridLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, gridLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_device->device(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    void Pipeline::createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 4;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 2;

        if (vkCreateDescriptorPool(m_device->device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    void Pipeline::createDescriptorSets() {
        if (m_uniformBuffers.empty()) {
            return; // No uniform buffers to create descriptor sets for
        }

        // Create descriptor sets for each frame in flight
        constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
        m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        allocInfo.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(m_device->device(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        // Update all descriptor sets with uniform buffers
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_uniformBuffers[0];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(m_device->device(), 1, &descriptorWrite, 0, nullptr);
        }
    }

    void Pipeline::createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        m_uniformBuffers.resize(1);
        m_uniformBufferMemory.resize(1);

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_uniformBuffers[0]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create uniform buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->device(), m_uniformBuffers[0], &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_uniformBufferMemory[0]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate uniform buffer memory!");
        }

        vkBindBufferMemory(m_device->device(), m_uniformBuffers[0], m_uniformBufferMemory[0], 0);
    }

    VkPipelineInputAssemblyStateCreateInfo Pipeline::createInputAssemblyState() const {
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        return inputAssembly;
    }

    VkPipelineViewportStateCreateInfo Pipeline::createViewportState() {
        m_viewport.x = 0.0F;
        m_viewport.y = 0.0F;
        m_viewport.width = static_cast<float>(m_extent.width);
        m_viewport.height = static_cast<float>(m_extent.height);
        m_viewport.minDepth = 0.0F;
        m_viewport.maxDepth = 1.0F;

        m_scissor.offset = {0, 0};
        m_scissor.extent = m_extent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1; // we will be using dynamlic viewport
        viewportState.scissorCount = 1;
        viewportState.pViewports = nullptr;
        viewportState.pScissors = nullptr;
        return viewportState;
    }

    VkPipelineRasterizationStateCreateInfo Pipeline::createRasterizationState() const {
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0F;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        return rasterizer;
    }

    VkPipelineMultisampleStateCreateInfo Pipeline::createMultisampleState() const {
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
        return multisampling;
    }

    VkPipelineDepthStencilStateCreateInfo Pipeline::createDepthStencilState(bool isTranslucent) const {
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = isTranslucent ? VK_FALSE : VK_TRUE;
        depthStencil.depthWriteEnable = isTranslucent ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;
        return depthStencil;
    }

    VkPipelineColorBlendStateCreateInfo Pipeline::createColorBlendState(const std::vector<VkPipelineColorBlendAttachmentState> &colorBlendAttachments) const {
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
        colorBlending.pAttachments = colorBlendAttachments.data();
        colorBlending.blendConstants[0] = 0.0F;
        colorBlending.blendConstants[1] = 0.0F;
        colorBlending.blendConstants[2] = 0.0F;
        colorBlending.blendConstants[3] = 0.0F;
        return colorBlending;
    }

    VkPipelineDynamicStateCreateInfo Pipeline::createDynamicState() const {
        static std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();
        return dynamicState;
    }

    void Pipeline::resize(VkExtent2D extent) {
        m_extent = extent;

        m_viewport.x = 0.0F;
        m_viewport.y = 0.0F;
        m_viewport.width = static_cast<float>(m_extent.width);
        m_viewport.height = static_cast<float>(m_extent.height);
        m_viewport.minDepth = 0.0F;
        m_viewport.maxDepth = 1.0F;

        m_scissor.offset = {0, 0};
        m_scissor.extent = m_extent;

        m_resized = true;
    }

    void Pipeline::cleanPrevStateCounter() {
        m_instanceCounter = 0;
    }
} // namespace Bess::Vulkan::Pipelines
