#pragma once

#include "common/bess_assert.h"
#include "common/logger.h"
#include "pipeline.h"
#include "primitive_vertex.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan {
    class VulkanTexture;
}

namespace Bess::Vulkan::Pipelines {

    class PrimitivePipeline : public Pipeline {
      public:
        static constexpr size_t m_texArraySize = 32;

        PrimitivePipeline(const std::shared_ptr<VulkanDevice> &device,
                          const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                          VkExtent2D extent);
        ~PrimitivePipeline() override;

        PrimitivePipeline(const PrimitivePipeline &) = delete;
        PrimitivePipeline &operator=(const PrimitivePipeline &) = delete;
        PrimitivePipeline(PrimitivePipeline &&other) noexcept;
        PrimitivePipeline &operator=(PrimitivePipeline &&other) noexcept;

        void beginPipeline(VkCommandBuffer commandBuffer, bool isTranslucent) override;
        void endPipeline() override;

        void drawPrimitiveInstances(
            VkCommandBuffer commandBuffer,
            bool isTranslucent,
            const std::vector<PrimitiveInstance> &instances,
            const std::unordered_map<std::shared_ptr<VulkanTexture>, std::vector<PrimitiveInstance>> &texturedInstances);

        void cleanup() override;
        void cleanPrevStateCounter() override;

      private:
        void createGraphicsPipeline(bool isTranslucent) override;
        void createPrimitiveBuffers();
        void ensurePrimitiveInstanceCapacity(size_t instanceCount);

        void createDescriptorPool() override;
        void createDescriptorSets() override;

        bool isTexArraySetAvailable(size_t idx) const;
        VkDescriptorSet getTextureArraySet(size_t idx);
        void resizeTexArrayDescriptorPool(uint64_t size);
        void setTextureSetGrowthPolicy(float growthFactor, uint32_t minHeadroom, uint32_t maxSetsCap);
        size_t textureSetsPerFrame() const;
        size_t textureSetFrameBase() const;
        size_t textureSetFrameLimit() const;

        VkDescriptorPool createTextureDescriptorPool(uint32_t maxSets, uint32_t descriptorCount);
        void createTextureDescriptorSets(uint32_t descCount,
                                         uint32_t setsCount,
                                         VkDescriptorPool pool,
                                         VkDescriptorSetLayout &layout,
                                         std::vector<VkDescriptorSet> &sets);
        void bindDescriptorSets(bool isTranslucent, VkDescriptorSet textureSet) const;
        void updateTextureSet(size_t descriptorSetIndex,
                              const std::array<VkDescriptorImageInfo, m_texArraySize> &textureInfos);
        void drawBatch(const std::vector<PrimitiveInstance> &instances);

        struct TempDescSets {
            std::vector<VkDescriptorPool> pools;
            std::vector<std::vector<VkDescriptorSet>> sets;
            static constexpr size_t DESC_SET_COUNT_PER_POOL = 100;
            size_t maxExhaustedSize = 0;

            bool isDescSetAvailable(uint64_t idx) const {
                return idx < (pools.size() * DESC_SET_COUNT_PER_POOL);
            }

            VkDescriptorSet getSetAtIdx(uint64_t idx) {
                if (!isDescSetAvailable(idx)) {
                    BESS_WARN("[PrimitivePipeline] Descriptor set not found at idx {} for poolSize = {}, setsCount = {}",
                              idx, pools.size(), pools.size() * DESC_SET_COUNT_PER_POOL);
                    return VK_NULL_HANDLE;
                }

                const auto poolIdx = idx / DESC_SET_COUNT_PER_POOL;
                const auto setIdx = idx % DESC_SET_COUNT_PER_POOL;
                maxExhaustedSize = std::max(idx + 1, maxExhaustedSize);
                return sets[poolIdx][setIdx];
            }

            std::pair<VkDescriptorPool &, std::vector<VkDescriptorSet> &> reserveNextPool() {
                pools.emplace_back(VK_NULL_HANDLE);
                sets.emplace_back(DESC_SET_COUNT_PER_POOL, VK_NULL_HANDLE);
                return {pools.back(), sets.back()};
            }

            void reset(VkDevice device) {
                for (auto &pool : pools) {
                    vkDestroyDescriptorPool(device, pool, nullptr);
                }

                pools.clear();
                sets.clear();
                maxExhaustedSize = 0;
            }
        } m_tempDescSets;

        BufferSet m_buffers;
        VkDescriptorPool m_textureArrayDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_textureArrayLayout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_textureArraySets;
        std::unique_ptr<VulkanTexture> m_fallbackTexture;
        std::unordered_map<size_t, std::array<VkDescriptorImageInfo, m_texArraySize>> m_cachedTextureInfos;

        uint32_t m_texDescSetIdx = 0;
        size_t m_texArraySetsCount = 2;

        float m_texSetsGrowthFactor = 2.0f;
        uint32_t m_texSetsMinHeadroom = 64;
        uint32_t m_texSetsMaxCap = 4096;
    };

} // namespace Bess::Vulkan::Pipelines
