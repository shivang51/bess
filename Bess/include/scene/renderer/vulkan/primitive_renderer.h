#pragma once

#include "device.h"
#include "primitive_vertex.h"
#include "vulkan_offscreen_render_pass.h"
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan {

    class PrimitiveRenderer {
      public:
        PrimitiveRenderer(const std::shared_ptr<VulkanDevice> &device, 
                         const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                         VkExtent2D extent);
        ~PrimitiveRenderer();

        PrimitiveRenderer(const PrimitiveRenderer &) = delete;
        PrimitiveRenderer &operator=(const PrimitiveRenderer &) = delete;
        PrimitiveRenderer(PrimitiveRenderer &&other) noexcept;
        PrimitiveRenderer &operator=(PrimitiveRenderer &&other) noexcept;

        void beginFrame(VkCommandBuffer commandBuffer, const UniformBufferObject &ubo, const GridUniforms &gridUniforms);
        void endFrame();

        // Primitive rendering functions
        void drawGrid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridUniforms &gridUniforms);
        void drawQuad(const glm::vec3 &pos, const glm::vec2 &size, const glm::vec4 &color, int id);
        void drawCircle(const glm::vec3 &center, float radius, const glm::vec4 &color, int id, float innerRadius = 0.0F);
        void drawLine(const glm::vec3 &start, const glm::vec3 &end, float width, const glm::vec4 &color, int id);

        // Buffer management
        void updateVertexBuffer(const std::vector<GridVertex> &vertices);
        void updateIndexBuffer(const std::vector<uint32_t> &indices);
        void updateUniformBuffer(const UniformBufferObject &ubo, const GridUniforms &gridUniforms);

      private:
        VkShaderModule createShaderModule(const std::vector<char> &code) const;
        std::vector<char> readFile(const std::string &filename) const;

      private:
        void createDescriptorSetLayout();
        void createDescriptorPool();
        void createDescriptorSets();
        void createUniformBuffers();
        void createVertexBuffer();
        void createIndexBuffer();
        void createGraphicsPipeline();

        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanOffscreenRenderPass> m_renderPass;
        VkExtent2D m_extent;

        // Pipeline
        VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

        // Descriptor sets
        VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

        // Uniform buffers
        std::vector<VkBuffer> m_uniformBuffers;
        std::vector<VkDeviceMemory> m_uniformBufferMemory;

        std::vector<VkBuffer> m_gridUniformBuffers;
        std::vector<VkDeviceMemory> m_gridUniformBufferMemory;

        // Vertex/Index buffers
        VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
        VkBuffer m_indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;

        // Current frame data
        VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;
        std::vector<GridVertex> m_currentVertices;
        std::vector<uint32_t> m_currentIndices;
        uint32_t m_currentVertexCount = 0;
        uint32_t m_currentIndexCount = 0;
    };

} // namespace Bess::Renderer2D::Vulkan
