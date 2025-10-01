#pragma once

#include "pipeline.h"
#include <vector>
#include <unordered_map>

namespace Bess::Renderer2D::Vulkan {
    class VulkanTexture;
}

namespace Bess::Renderer2D::Vulkan::Pipelines {

    class QuadPipeline : public Pipeline {
    public:
        QuadPipeline(const std::shared_ptr<VulkanDevice> &device,
                     const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                     VkExtent2D extent);
        ~QuadPipeline() override;

        QuadPipeline(const QuadPipeline &) = delete;
        QuadPipeline &operator=(const QuadPipeline &) = delete;
        QuadPipeline(QuadPipeline &&other) noexcept;
        QuadPipeline &operator=(QuadPipeline &&other) noexcept;

        void beginPipeline(VkCommandBuffer commandBuffer) override;
        void endPipeline() override;
        void cleanup() override;

        void drawQuad(const glm::vec3 &pos,
                      const glm::vec2 &size,
                      const glm::vec4 &color,
                      int id,
                      const glm::vec4 &borderRadius,
                      const glm::vec4 &borderSize,
                      const glm::vec4 &borderColor,
                      int isMica);

        void drawTexturedQuad(const glm::vec3 &pos,
                      const glm::vec2 &size,
                      const glm::vec4 &tint,
                      int id,
                      const glm::vec4 &borderRadius,
                      const glm::vec4 &borderSize,
                      const glm::vec4 &borderColor,
                      int isMica,
                      const std::shared_ptr<VulkanTexture> &texture);

    private:
        void createQuadPipeline();
        void ensureQuadBuffers();
        void ensureQuadInstanceCapacity(size_t instanceCount);
        void ensureTextureDescriptorPool(uint32_t capacity = 128);

        BufferSet m_buffers;
        std::vector<QuadInstance> m_pendingQuadInstances;
        std::unordered_map<std::shared_ptr<VulkanTexture>, std::vector<QuadInstance>> m_textureToInstances;
        std::unordered_map<std::shared_ptr<VulkanTexture>, VkDescriptorSet> m_textureSetCache;
        std::vector<VkDescriptorSet> m_frameAllocatedSets;
        VkDescriptorPool m_textureDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_samplerSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet m_fallbackSamplerSet = VK_NULL_HANDLE;
        std::shared_ptr<VulkanTexture> m_fallbackTexture;
    };

} // namespace Bess::Renderer2D::Vulkan::Pipelines

