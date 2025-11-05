#pragma once
#include "bess_uuid.h"
#include "command_buffer.h"
#include "device.h"
#include "entt/entity/fwd.hpp"
#include "scene/artist/artist_manager.h"
#include "scene/camera.h"
#include "vulkan_image_view.h"
#include "vulkan_offscreen_render_pass.h"
#include "vulkan_postprocess_pipeline.h"
#include <memory>
#include <vulkan/vulkan_core.h>

using namespace Bess::Renderer2D;

namespace Bess::Canvas {
    struct MousePickingData {
        VkExtent2D startPos;
        VkExtent2D extent;
        bool pending = false;
        std::vector<int32_t> ids;
    };

    class Viewport {
      public:
        Viewport(const std::shared_ptr<Vulkan::VulkanDevice> &device, VkFormat imgFormat, VkExtent2D size);

        ~Viewport();

        void begin(int frameIdx, const glm::vec4 &clearColor, int clearPickingId);

        VkCommandBuffer end();

        void resize(VkExtent2D size);

        VkCommandBuffer getVkCmdBuffer(int idx = -1);

        void submit();
        void resizePickingBuffer(VkDeviceSize newSize);

        std::shared_ptr<Camera> getCamera();

        uint64_t getViewportTexture();
        void setPickingCoord(uint32_t x, uint32_t y, uint32_t w = 1, uint32_t h = 1);
        std::vector<int32_t> getPickingIdsResult();
        bool tryUpdatePickingResults();
        bool waitForPickingResults(uint64_t timeoutNs);

        entt::registry &getEnttRegistry();
        entt::entity getEntityWithUuid(const UUID &uuid);

        std::shared_ptr<ArtistManager> getArtistManager();

        std::vector<unsigned char> getPixelData();

      private:
        void createFences(size_t count);
        void deleteFences();

        void createPickingResources();
        void cleanupPickingResources();

        void copyIdForPicking();

        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                          VkBuffer &buffer, VkDeviceMemory &bufferMemory) const;

        void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) const;

        void performPostProcessing(VkCommandBuffer cmd);

        void initPostprocessResources();
        void cleanupPostprocessResources();

      private:
        int m_currentFrameIdx = 0;

        std::vector<VkFence> m_fences;

        std::shared_ptr<Camera> m_camera;

        std::unique_ptr<Vulkan::VulkanImageView> m_imgView;
        std::unique_ptr<Vulkan::VulkanImageView> m_straightColorImageView;
        std::shared_ptr<Vulkan::VulkanOffscreenRenderPass> m_renderPass;
        std::unique_ptr<Vulkan::VulkanPostprocessPipeline> m_postprocessPipeline;

        std::unique_ptr<Vulkan::VulkanCommandBuffers> m_cmdBuffers;
        std::shared_ptr<Vulkan::VulkanDevice> m_device;
        VkFormat m_imgFormat;
        VkFormat m_pickingIdFormat = VK_FORMAT_R32_SINT;
        VkExtent2D m_size;

        VkCommandBuffer m_currentCmdBuffer;
        VkFramebuffer m_postprocessFramebuffer = VK_NULL_HANDLE;

        // mouse picking resources
        VkBuffer m_pickingStagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_pickingStagingBufferMemory = VK_NULL_HANDLE;

        MousePickingData m_mousePickingData = {};
        bool m_pickingCopyInFlight = false;
        uint32_t m_pickingCopyRecordedFrameIdx = 0;
        uint64_t m_pickingStagingBufferSize = 1;

        std::shared_ptr<ArtistManager> m_artistManager;
    };
} // namespace Bess::Canvas
