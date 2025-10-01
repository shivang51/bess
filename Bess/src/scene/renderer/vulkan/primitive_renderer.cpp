#include "scene/renderer/vulkan/primitive_renderer.h"
#include "ext/matrix_transform.hpp"
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace Bess::Renderer2D::Vulkan {

    PrimitiveRenderer::PrimitiveRenderer(const std::shared_ptr<VulkanDevice> &device,
                                         const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                                         VkExtent2D extent)
        : m_device(device), m_renderPass(renderPass), m_extent(extent) {
        createDescriptorSetLayout();
        createDescriptorPool();
        createUniformBuffers();
        createDescriptorSets();
        createVertexBuffer();
        createIndexBuffer();
        createGraphicsPipeline();
    }

    PrimitiveRenderer::~PrimitiveRenderer() {
        if (m_device && m_device->device() != VK_NULL_HANDLE) {
            if (m_graphicsPipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_device->device(), m_graphicsPipeline, nullptr);
            }
            if (m_pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_device->device(), m_pipelineLayout, nullptr);
            }
            if (m_descriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(m_device->device(), m_descriptorPool, nullptr);
            }
            if (m_descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_device->device(), m_descriptorSetLayout, nullptr);
            }
            for (size_t i = 0; i < m_uniformBuffers.size(); i++) {
                if (m_uniformBuffers[i] != VK_NULL_HANDLE) {
                    vkDestroyBuffer(m_device->device(), m_uniformBuffers[i], nullptr);
                }
                if (m_uniformBufferMemory[i] != VK_NULL_HANDLE) {
                    vkFreeMemory(m_device->device(), m_uniformBufferMemory[i], nullptr);
                }
            }
            for (size_t i = 0; i < m_gridUniformBuffers.size(); i++) {
                if (m_gridUniformBuffers[i] != VK_NULL_HANDLE) {
                    vkDestroyBuffer(m_device->device(), m_gridUniformBuffers[i], nullptr);
                }
                if (m_gridUniformBufferMemory[i] != VK_NULL_HANDLE) {
                    vkFreeMemory(m_device->device(), m_gridUniformBufferMemory[i], nullptr);
                }
            }
            if (m_vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device->device(), m_vertexBuffer, nullptr);
            }
            if (m_vertexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_vertexBufferMemory, nullptr);
            }
            if (m_indexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_device->device(), m_indexBuffer, nullptr);
            }
            if (m_indexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_indexBufferMemory, nullptr);
            }
        }
    }

    PrimitiveRenderer::PrimitiveRenderer(PrimitiveRenderer &&other) noexcept
        : m_device(other.m_device),
          m_renderPass(other.m_renderPass),
          m_extent(other.m_extent),
          m_graphicsPipeline(other.m_graphicsPipeline),
          m_pipelineLayout(other.m_pipelineLayout),
          m_descriptorSetLayout(other.m_descriptorSetLayout),
          m_descriptorPool(other.m_descriptorPool),
          m_descriptorSet(other.m_descriptorSet),
          m_uniformBuffers(std::move(other.m_uniformBuffers)),
          m_uniformBufferMemory(std::move(other.m_uniformBufferMemory)),
          m_gridUniformBuffers(std::move(other.m_gridUniformBuffers)),
          m_gridUniformBufferMemory(std::move(other.m_gridUniformBufferMemory)),
          m_vertexBuffer(other.m_vertexBuffer),
          m_vertexBufferMemory(other.m_vertexBufferMemory),
          m_indexBuffer(other.m_indexBuffer),
          m_indexBufferMemory(other.m_indexBufferMemory),
          m_currentCommandBuffer(other.m_currentCommandBuffer),
          m_currentVertices(std::move(other.m_currentVertices)),
          m_currentIndices(std::move(other.m_currentIndices)),
          m_currentVertexCount(other.m_currentVertexCount),
          m_currentIndexCount(other.m_currentIndexCount) {
        other.m_graphicsPipeline = VK_NULL_HANDLE;
        other.m_pipelineLayout = VK_NULL_HANDLE;
        other.m_descriptorSetLayout = VK_NULL_HANDLE;
        other.m_descriptorPool = VK_NULL_HANDLE;
        other.m_descriptorSet = VK_NULL_HANDLE;
        other.m_uniformBuffers.clear();
        other.m_uniformBufferMemory.clear();
        other.m_gridUniformBuffers.clear();
        other.m_gridUniformBufferMemory.clear();
        other.m_vertexBuffer = VK_NULL_HANDLE;
        other.m_vertexBufferMemory = VK_NULL_HANDLE;
        other.m_indexBuffer = VK_NULL_HANDLE;
        other.m_indexBufferMemory = VK_NULL_HANDLE;
        other.m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    PrimitiveRenderer &PrimitiveRenderer::operator=(PrimitiveRenderer &&other) noexcept {
        if (this != &other) {
            // Cleanup existing resources
            if (m_device && m_device->device() != VK_NULL_HANDLE) {
                if (m_graphicsPipeline != VK_NULL_HANDLE) {
                    vkDestroyPipeline(m_device->device(), m_graphicsPipeline, nullptr);
                }
                if (m_pipelineLayout != VK_NULL_HANDLE) {
                    vkDestroyPipelineLayout(m_device->device(), m_pipelineLayout, nullptr);
                }
                if (m_descriptorPool != VK_NULL_HANDLE) {
                    vkDestroyDescriptorPool(m_device->device(), m_descriptorPool, nullptr);
                }
                if (m_descriptorSetLayout != VK_NULL_HANDLE) {
                    vkDestroyDescriptorSetLayout(m_device->device(), m_descriptorSetLayout, nullptr);
                }
                for (size_t i = 0; i < m_uniformBuffers.size(); i++) {
                    if (m_uniformBuffers[i] != VK_NULL_HANDLE) {
                        vkDestroyBuffer(m_device->device(), m_uniformBuffers[i], nullptr);
                    }
                    if (m_uniformBufferMemory[i] != VK_NULL_HANDLE) {
                        vkFreeMemory(m_device->device(), m_uniformBufferMemory[i], nullptr);
                    }
                }
                for (size_t i = 0; i < m_gridUniformBuffers.size(); i++) {
                    if (m_gridUniformBuffers[i] != VK_NULL_HANDLE) {
                        vkDestroyBuffer(m_device->device(), m_gridUniformBuffers[i], nullptr);
                    }
                    if (m_gridUniformBufferMemory[i] != VK_NULL_HANDLE) {
                        vkFreeMemory(m_device->device(), m_gridUniformBufferMemory[i], nullptr);
                    }
                }
                if (m_vertexBuffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(m_device->device(), m_vertexBuffer, nullptr);
                }
                if (m_vertexBufferMemory != VK_NULL_HANDLE) {
                    vkFreeMemory(m_device->device(), m_vertexBufferMemory, nullptr);
                }
                if (m_indexBuffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(m_device->device(), m_indexBuffer, nullptr);
                }
                if (m_indexBufferMemory != VK_NULL_HANDLE) {
                    vkFreeMemory(m_device->device(), m_indexBufferMemory, nullptr);
                }
            }

            // Move resources
            m_device = other.m_device;
            m_renderPass = other.m_renderPass;
            m_extent = other.m_extent;
            m_graphicsPipeline = other.m_graphicsPipeline;
            m_pipelineLayout = other.m_pipelineLayout;
            m_descriptorSetLayout = other.m_descriptorSetLayout;
            m_descriptorPool = other.m_descriptorPool;
            m_descriptorSet = other.m_descriptorSet;
            m_uniformBuffers = std::move(other.m_uniformBuffers);
            m_uniformBufferMemory = std::move(other.m_uniformBufferMemory);
            m_gridUniformBuffers = std::move(other.m_gridUniformBuffers);
            m_gridUniformBufferMemory = std::move(other.m_gridUniformBufferMemory);
            m_vertexBuffer = other.m_vertexBuffer;
            m_vertexBufferMemory = other.m_vertexBufferMemory;
            m_indexBuffer = other.m_indexBuffer;
            m_indexBufferMemory = other.m_indexBufferMemory;
            m_currentCommandBuffer = other.m_currentCommandBuffer;
            m_currentVertices = std::move(other.m_currentVertices);
            m_currentIndices = std::move(other.m_currentIndices);
            m_currentVertexCount = other.m_currentVertexCount;
            m_currentIndexCount = other.m_currentIndexCount;

            // Reset other
            other.m_graphicsPipeline = VK_NULL_HANDLE;
            other.m_pipelineLayout = VK_NULL_HANDLE;
            other.m_descriptorSetLayout = VK_NULL_HANDLE;
            other.m_descriptorPool = VK_NULL_HANDLE;
            other.m_descriptorSet = VK_NULL_HANDLE;
            other.m_uniformBuffers.clear();
            other.m_uniformBufferMemory.clear();
            other.m_gridUniformBuffers.clear();
            other.m_gridUniformBufferMemory.clear();
            other.m_vertexBuffer = VK_NULL_HANDLE;
            other.m_vertexBufferMemory = VK_NULL_HANDLE;
            other.m_indexBuffer = VK_NULL_HANDLE;
            other.m_indexBufferMemory = VK_NULL_HANDLE;
            other.m_currentCommandBuffer = VK_NULL_HANDLE;
        }
        return *this;
    }

    void PrimitiveRenderer::beginFrame(VkCommandBuffer commandBuffer) {
        m_currentCommandBuffer = commandBuffer;
        m_currentVertices.clear();
        m_currentIndices.clear();
        m_currentVertexCount = 0;
        m_currentIndexCount = 0;

        vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
        vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

        VkBuffer vertexBuffers[] = {m_vertexBuffer};
        constexpr VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(m_currentCommandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void PrimitiveRenderer::endFrame() {
        if (m_currentVertexCount > 0) {
            vkCmdDrawIndexed(m_currentCommandBuffer, m_currentIndexCount, 1, 0, 0, 0);
        }
        m_currentCommandBuffer = VK_NULL_HANDLE;
    }
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

    void PrimitiveRenderer::drawGrid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridUniforms &gridUniforms) {
        // Build a quad centered at pos that covers the viewport span
        const float halfW = size.x * 0.5F;
        const float halfH = size.y * 0.5F;
        std::vector<GridVertex> vertices(4);

        auto transform = glm::translate(glm::mat4(1.0f), pos);
        transform = glm::scale(transform, {size.x, size.y, 1.f});

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
    }

    void PrimitiveRenderer::drawQuad(const glm::vec3 &pos, const glm::vec2 &size, const glm::vec4 &color, int id) {
        ensureQuadBuffers();

        // Local quad verts (unit quad centered at origin)
        std::array<glm::vec2, 4> local = {{{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}}};
        std::array<uint32_t, 6> idx = {{0, 1, 2, 2, 3, 0}};

        // Upload instance data
        QuadInstance instance{};
        instance.position = pos;
        instance.color = color;
        instance.borderRadius = glm::vec4(0.0f);
        instance.borderColor = glm::vec4(0.0f);
        instance.borderSize = glm::vec4(0.0f);
        instance.size = size;
        instance.id = id;
        instance.isMica = 0;
        instance.texSlotIdx = 0;
        instance.texData = glm::vec4(0.0f);

        void *data = nullptr;
        vkMapMemory(m_device->device(), m_quadInstanceBufferMemory, 0, sizeof(QuadInstance), 0, &data);
        memcpy(data, &instance, sizeof(QuadInstance));
        vkUnmapMemory(m_device->device(), m_quadInstanceBufferMemory);

        // Create a small vertex/index buffer on the fly using existing buffers for simplicity
        std::vector<GridVertex> vertices(4);
        for (int i = 0; i < 4; i++) {
            vertices[i].position = glm::vec4(local[i], 0.0f, 1.0f);
            vertices[i].texCoord = glm::vec2((i == 1 || i == 2) ? 1.f : 0.f, (i >= 2) ? 1.f : 0.f);
            vertices[i].fragId = id;
            vertices[i].fragColor = color;
            vertices[i].ar = 0;
        }
        updateVertexBuffer(vertices);
        updateIndexBuffer({0, 1, 2, 2, 3, 0});

        // Bind quad pipeline
        vkCmdBindPipeline(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_quadPipeline);

        // Bind vertex/index (binding 0) and instance buffer (binding 1)
        VkBuffer vbs[] = {m_vertexBuffer, m_quadInstanceBuffer};
        VkDeviceSize offs[] = {0, 0};
        vkCmdBindVertexBuffers(m_currentCommandBuffer, 0, 2, vbs, offs);
        vkCmdBindIndexBuffer(m_currentCommandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // Bind descriptor sets
        vkCmdBindDescriptorSets(m_currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_quadPipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

        // Draw one instance
        vkCmdDrawIndexed(m_currentCommandBuffer, 6, 1, 0, 0, 0);
    }

    void PrimitiveRenderer::drawCircle(const glm::vec3 &center, float radius, const glm::vec4 &color, int id, float innerRadius) {
        // Implementation for circle rendering
        // This is a placeholder - you can implement actual circle rendering here
    }

    void PrimitiveRenderer::drawLine(const glm::vec3 &start, const glm::vec3 &end, float width, const glm::vec4 &color, int id) {
        // Implementation for line rendering
        // This is a placeholder - you can implement actual line rendering here
    }

    void PrimitiveRenderer::updateVertexBuffer(const std::vector<GridVertex> &vertices) {
        m_currentVertices = vertices;
        m_currentVertexCount = static_cast<uint32_t>(vertices.size());

        void *data = nullptr;
        vkMapMemory(m_device->device(), m_vertexBufferMemory, 0, sizeof(GridVertex) * vertices.size(), 0, &data);
        memcpy(data, vertices.data(), sizeof(GridVertex) * vertices.size());
        vkUnmapMemory(m_device->device(), m_vertexBufferMemory);
    }

    void PrimitiveRenderer::updateIndexBuffer(const std::vector<uint32_t> &indices) {
        m_currentIndices = indices;
        m_currentIndexCount = static_cast<uint32_t>(indices.size());

        void *data = nullptr;
        vkMapMemory(m_device->device(), m_indexBufferMemory, 0, sizeof(uint32_t) * indices.size(), 0, &data);
        memcpy(data, indices.data(), sizeof(uint32_t) * indices.size());
        vkUnmapMemory(m_device->device(), m_indexBufferMemory);
    }

    void PrimitiveRenderer::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding gridUboLayoutBinding{};
        gridUboLayoutBinding.binding = 1;
        gridUboLayoutBinding.descriptorCount = 1;
        gridUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        gridUboLayoutBinding.pImmutableSamplers = nullptr;
        gridUboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, gridUboLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_device->device(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    void PrimitiveRenderer::createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(m_device->device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    void PrimitiveRenderer::createDescriptorSets() {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_descriptorSetLayout;

        if (vkAllocateDescriptorSets(m_device->device(), &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[0];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorBufferInfo gridBufferInfo{};
        gridBufferInfo.buffer = m_gridUniformBuffers[0];
        gridBufferInfo.offset = 0;
        gridBufferInfo.range = sizeof(GridUniforms);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &gridBufferInfo;

        vkUpdateDescriptorSets(m_device->device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    void PrimitiveRenderer::createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);
        VkDeviceSize gridBufferSize = sizeof(GridUniforms);

        // Create MVP uniform buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        m_uniformBuffers.resize(1);
        m_uniformBufferMemory.resize(1);

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

        // Create grid uniform buffer
        m_gridUniformBuffers.resize(1);
        m_gridUniformBufferMemory.resize(1);

        bufferInfo.size = gridBufferSize;
        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_gridUniformBuffers[0]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create grid uniform buffer!");
        }

        vkGetBufferMemoryRequirements(m_device->device(), m_gridUniformBuffers[0], &memRequirements);
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_gridUniformBufferMemory[0]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate grid uniform buffer memory!");
        }

        vkBindBufferMemory(m_device->device(), m_gridUniformBuffers[0], m_gridUniformBufferMemory[0], 0);
    }

    void PrimitiveRenderer::createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(GridVertex) * 1000; // Max 1000 vertices

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_vertexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->device(), m_vertexBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_vertexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate vertex buffer memory!");
        }

        vkBindBufferMemory(m_device->device(), m_vertexBuffer, m_vertexBufferMemory, 0);
    }

    void PrimitiveRenderer::createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(uint32_t) * 1000; // Max 1000 indices

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_indexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create index buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->device(), m_indexBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_indexBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate index buffer memory!");
        }

        vkBindBufferMemory(m_device->device(), m_indexBuffer, m_indexBufferMemory, 0);
    }

    void PrimitiveRenderer::createGraphicsPipeline() {
        // Load shaders
        auto vertShaderCode = readFile("assets/shaders/grid_vert.spv");
        auto fragShaderCode = readFile("assets/shaders/grid_line_frag.spv");

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

        // Vertex input
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
        // Disable culling to ensure primitives are visible regardless of winding
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        // Enable alpha blending so grid blends nicely over clear color
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

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
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
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

        if (vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(m_device->device(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device->device(), vertShaderModule, nullptr);
    }

    void PrimitiveRenderer::createQuadPipeline() {
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

        // Binding 0: local quad verts (position/texcoord via GridVertex subset)
        auto binding0 = GridVertex::getBindingDescription();
        auto attrs0 = GridVertex::getAttributeDescriptions();
        // We'll use only loc 0,1 from GridVertex and ignore others
        // Binding 1: instance data
        VkVertexInputBindingDescription binding1{};
        binding1.binding = 1;
        binding1.stride = sizeof(QuadInstance);
        binding1.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        std::array<VkVertexInputBindingDescription, 2> bindings{binding0, binding1};

        // Attribute descriptions: reuse 0,1 from GridVertex then add instance attrs 2..11
        std::vector<VkVertexInputAttributeDescription> attrs;
        attrs.reserve(2 + 10);
        attrs.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(GridVertex, position)});
        attrs.push_back({1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(GridVertex, texCoord)});
        attrs.push_back({2, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(QuadInstance, position)});
        attrs.push_back({3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(QuadInstance, color)});
        attrs.push_back({4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(QuadInstance, borderRadius)});
        attrs.push_back({5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(QuadInstance, borderColor)});
        attrs.push_back({6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(QuadInstance, borderSize)});
        attrs.push_back({7, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(QuadInstance, size)});
        attrs.push_back({8, 1, VK_FORMAT_R32_SINT, offsetof(QuadInstance, id)});
        attrs.push_back({9, 1, VK_FORMAT_R32_SINT, offsetof(QuadInstance, isMica)});
        attrs.push_back({10, 1, VK_FORMAT_R32_SINT, offsetof(QuadInstance, texSlotIdx)});
        attrs.push_back({11, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(QuadInstance, texData)});

        VkPipelineVertexInputStateCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
        vi.pVertexBindingDescriptions = bindings.data();
        vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        vi.pVertexAttributeDescriptions = attrs.data();

        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport{};
        viewport.width = static_cast<float>(m_extent.width);
        viewport.height = static_cast<float>(m_extent.height);
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;
        VkRect2D scissor{};
        scissor.extent = m_extent;

        VkPipelineViewportStateCreateInfo vp{};
        vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.viewportCount = 1;
        vp.pViewports = &viewport;
        vp.scissorCount = 1;
        vp.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_NONE;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;

        VkPipelineColorBlendAttachmentState att{};
        att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        att.blendEnable = VK_TRUE;
        att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        att.colorBlendOp = VK_BLEND_OP_ADD;
        att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        att.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments = &att;

        VkPipelineLayoutCreateInfo pl{};
        pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pl.setLayoutCount = 1;
        pl.pSetLayouts = &m_descriptorSetLayout;
        if (vkCreatePipelineLayout(m_device->device(), &pl, nullptr, &m_quadPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create quad pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo gp{};
        gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gp.stageCount = static_cast<uint32_t>(stages.size());
        gp.pStages = stages.data();
        gp.pVertexInputState = &vi;
        gp.pInputAssemblyState = &ia;
        gp.pViewportState = &vp;
        gp.pRasterizationState = &rs;
        gp.pMultisampleState = &ms;
        gp.pColorBlendState = &cb;
        gp.layout = m_quadPipelineLayout;
        gp.renderPass = m_renderPass->getVkHandle();
        gp.subpass = 0;

        if (vkCreateGraphicsPipelines(m_device->device(), VK_NULL_HANDLE, 1, &gp, nullptr, &m_quadPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create quad graphics pipeline!");
        }

        vkDestroyShaderModule(m_device->device(), fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device->device(), vertShaderModule, nullptr);
    }

    void PrimitiveRenderer::ensureQuadBuffers() {
        if (m_quadInstanceBuffer != VK_NULL_HANDLE) return;
        VkDeviceSize size = sizeof(QuadInstance);
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = size;
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(m_device->device(), &bi, nullptr, &m_quadInstanceBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create quad instance buffer!");
        }
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(m_device->device(), m_quadInstanceBuffer, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = m_device->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(m_device->device(), &ai, nullptr, &m_quadInstanceBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate quad instance buffer memory!");
        }
        vkBindBufferMemory(m_device->device(), m_quadInstanceBuffer, m_quadInstanceBufferMemory, 0);

        // Create quad pipeline on first use
        createQuadPipeline();
    }

    void PrimitiveRenderer::updateUniformBuffer(const UniformBufferObject &ubo, const GridUniforms &gridUniforms) {
        void *data = nullptr;
        vkMapMemory(m_device->device(), m_uniformBufferMemory[0], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(m_device->device(), m_uniformBufferMemory[0]);

        vkMapMemory(m_device->device(), m_gridUniformBufferMemory[0], 0, sizeof(gridUniforms), 0, &data);
        memcpy(data, &gridUniforms, sizeof(gridUniforms));
        vkUnmapMemory(m_device->device(), m_gridUniformBufferMemory[0]);
    }

    VkShaderModule PrimitiveRenderer::createShaderModule(const std::vector<char> &code) const {
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

    std::vector<char> PrimitiveRenderer::readFile(const std::string &filename) const {
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

} // namespace Bess::Renderer2D::Vulkan
