#pragma once

#include "common/log.h"
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

        void beginPipeline(VkCommandBuffer commandBuffer, bool isTranslucent) override;
        void endPipeline() override;

        void setQuadsData(const std::vector<QuadInstance> &instances,
                          std::unordered_map<std::shared_ptr<VulkanTexture>, std::vector<QuadInstance>> &texutredData);

        void setQuadsData(const std::vector<QuadInstance> &instances);
        void drawQuadInstances(VkCommandBuffer cmd,
                               bool isTranslucent,
                               const std::vector<QuadInstance> &instances,
                               std::unordered_map<std::shared_ptr<VulkanTexture>, std::vector<QuadInstance>> &texturedData);
        void cleanup() override;

        void cleanPrevStateCounter() override;

      private:
        void createGraphicsPipeline(bool isTranslucent) override;
        void createQuadBuffers();
        void ensureQuadInstanceCapacity(size_t instanceCount);

        void createDescriptorPool() override;
        void createDescriptorSets() override;

        bool isTexArraySetAvailable(size_t idx) const;

        VkDescriptorSet getTextureArraySet(uint8_t idx);

        void resizeTexArrayDescriptorPool(uint64_t size);
        void setTextureSetGrowthPolicy(float growthFactor, uint32_t minHeadroom, uint32_t maxSetsCap);

        VkDescriptorPool createDescriptorPool(uint32_t maxSets, uint32_t descriptorCount);
        void createDescriptorSets(uint32_t descCount, uint32_t setsCount,
                                  VkDescriptorPool pool, VkDescriptorSetLayout &layout, std::vector<VkDescriptorSet> &sets);

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
                    BESS_WARN("[TempDescSets] Descriptor set was not found at idx {} for poolSize = {}, setsCount = {}",
                              idx, pools.size(), pools.size() * DESC_SET_COUNT_PER_POOL);
                    return VK_NULL_HANDLE;
                }
                auto poolIdx = idx / DESC_SET_COUNT_PER_POOL;
                auto setIdx = idx % DESC_SET_COUNT_PER_POOL;
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

        static constexpr size_t m_texArraySize = 32;

        BufferSet m_buffers;
        std::vector<QuadInstance> m_instances;
        std::vector<VkDescriptorSet> m_textureArraySets;
        VkDescriptorPool m_textureArrayDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_textureArrayLayout = VK_NULL_HANDLE;
        std::unique_ptr<VulkanTexture> m_fallbackTexture;
        std::array<VkDescriptorImageInfo, m_texArraySize> m_textureInfos;
        std::unordered_map<uint32_t, std::array<VkDescriptorImageInfo, m_texArraySize>> m_cachedTextureInfos;

        bool m_isTranslucentFlow = false;
        uint8_t m_texDescSetIdx = 0;

        size_t m_texArraySetsCount = 2; // One for opaque flow, one for translucent flow
       
        float m_texSetsGrowthFactor = 2.0f;
        uint32_t m_texSetsMinHeadroom = 64;
        uint32_t m_texSetsMaxCap = 4096;
    };

} // namespace Bess::Vulkan::Pipelines
