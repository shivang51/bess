#pragma once
#include "camera.h"
#include "scene/renderer/vulkan/command_buffer.h"
#include "scene/renderer/vulkan/device.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include "scene/renderer/vulkan/primitive_renderer.h"
#include "scene/renderer/vulkan/vulkan_image_view.h"
#include "scene/renderer/vulkan/vulkan_offscreen_render_pass.h"
#include "scene/renderer/vulkan/vulkan_subtexture.h"
#include <memory>
#include <vulkan/vulkan_core.h>

using namespace Bess::Renderer2D;

namespace Bess::Canvas {
    struct QuadRenderProperties {
        float angle = 0.0f;
        glm::vec4 borderColor = {0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 borderRadius = {0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 borderSize = {0.0f, 0.0f, 0.0f, 0.0f};
        bool hasShadow = false;
        bool isMica = false;
    };

    struct GridColors {
        glm::vec4 minorColor;
        glm::vec4 majorColor;
        glm::vec4 axisXColor;
        glm::vec4 axisYColor;
    };

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

      public: // drawing api
        void grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridColors &colors);

        void quad(const glm::vec3 &pos, const glm::vec2 &size,
                  const glm::vec4 &color, int id, QuadRenderProperties properties = {});

        void texturedQuad(const glm::vec3 &pos, const glm::vec2 &size,
                          const std::shared_ptr<Vulkan::VulkanTexture> &texture,
                          const glm::vec4 &tintColor, int id, QuadRenderProperties properties = {});

        void texturedQuad(const glm::vec3 &pos, const glm::vec2 &size,
                          const std::shared_ptr<Vulkan::SubTexture> &texture,
                          const glm::vec4 &tintColor, int id, QuadRenderProperties properties = {});

        void circle(const glm::vec3 &center, float radius,
                    const glm::vec4 &color, int id, float innerRadius = 0);

        void msdfText(const std::string &text, const glm::vec3 &pos, size_t size,
                      const glm::vec4 &color, int id, float angle = 0);

        void beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, uint64_t id);

        void endPathMode(bool closePath = false, bool genFill = false, const glm::vec4 &fillColor = {}, bool genStroke = true);

        void pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, int id);

        void pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                               float weight, const glm::vec4 &color, int id);

        void pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, int id);

        static glm::vec2 getMSDFTextRenderSize(const std::string &str, float renderSize);

      private:
        void createFences(size_t count);
        void deleteFences();

        void createPickingResources();
        void cleanupPickingResources();

        void copyIdForPicking();

      private:
        int m_currentFrameIdx = 0;

        std::vector<VkFence> m_fences;

        std::shared_ptr<Camera> m_camera;

        std::unique_ptr<Vulkan::VulkanImageView> m_imgView;
        std::shared_ptr<Vulkan::VulkanOffscreenRenderPass> m_renderPass;
        std::unique_ptr<Vulkan::PrimitiveRenderer> m_primitiveRenderer;
        std::unique_ptr<Vulkan::PathRenderer> m_pathRenderer;

        std::unique_ptr<Vulkan::VulkanCommandBuffers> m_cmdBuffers;
        std::shared_ptr<Vulkan::VulkanDevice> m_device;
        VkFormat m_imgFormat;
        VkFormat m_pickingIdFormat = VK_FORMAT_R32_SINT;
        VkExtent2D m_size;

        VkCommandBuffer m_currentCmdBuffer;

        // mouse picking resources
        VkBuffer m_pickingStagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_pickingStagingBufferMemory = VK_NULL_HANDLE;

        MousePickingData m_mousePickingData = {};
        bool m_pickingCopyInFlight = false;
        uint32_t m_pickingCopyRecordedFrameIdx = 0;
        uint64_t m_pickingStagingBufferSize = 1;
    };
} // namespace Bess::Canvas
