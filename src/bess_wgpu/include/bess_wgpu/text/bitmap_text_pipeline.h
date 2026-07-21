#pragma once

#include "common/bess_api.h"

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_wgpu/wgpu_shader.h"
#include "common/types.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <webgpu/webgpu_cpp.h>

namespace Bess::Wgpu::Text {
    constexpr uint32_t kBitmapTextFlagApplyCameraTransform = 1u << 0u;
    inline constexpr std::array<std::string_view, 4>
        kBundledBitmapFallbackFontPaths = {
            "assets/bess_fonts/FontAwesomeIcons_remapped.ttf",
            "assets/bess_fonts/CodIcons_remapped.ttf",
            "assets/bess_fonts/ComponentIcons_remapped.ttf",
            "assets/bess_fonts/MaterialIcons_remapped.ttf",
    };

    struct BESS_API BitmapGlyph {
        uint32_t codepoint = 0;
        uint32_t pixelSize = 0;
        // Zero is the primary text face. Non-zero values identify bundled
        // fallback faces, which are currently the remapped icon catalogs.
        uint32_t faceIndex = 0;
        float advance = 0.f;
        float offsetX = 0.f;
        float offsetY = 0.f;
        float width = 0.f;
        float height = 0.f;
        glm::vec4 uvRect{0.f};
        bool drawable = false;
    };

    struct BESS_API BitmapTextLineMetrics {
        float lineHeight = 0.f;
        float ascender = 0.f;
        float descender = 0.f;
    };

    class BESS_API BitmapFontAtlas {
      public:
        bool init(const wgpu::Device &device,
                  const wgpu::Queue &queue,
                  const std::string &fontPath,
                  uint32_t atlasSize = 2048,
                  uint32_t minPixelSize = 8,
                  uint32_t maxPixelSize = 24);
        bool init(const wgpu::Device &device,
                  const wgpu::Queue &queue,
                  const std::string &primaryFontPath,
                  std::span<const std::string_view> fallbackFontPaths,
                  uint32_t atlasSize = 2048,
                  uint32_t minPixelSize = 8,
                  uint32_t maxPixelSize = 24);
        void destroy();

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] uint32_t
        quantizePixelSize(float projectedPixelSize) const;
        [[nodiscard]] BitmapTextLineMetrics metricsForSize(uint32_t pixelSize);
        [[nodiscard]] const BitmapGlyph *ensureGlyph(uint32_t codepoint,
                                                     uint32_t pixelSize);
        [[nodiscard]] const wgpu::TextureView &getTextureView() const;
        [[nodiscard]] uint32_t atlasSize() const noexcept;
        [[nodiscard]] uint32_t maxPixelSize() const noexcept;
        [[nodiscard]] uint32_t minPixelSize() const noexcept;

      private:
        bool createTexture(uint32_t atlasSize);
        bool loadFace(std::string_view fontPath, bool required);
        bool selectPixelSize(std::size_t faceIndex, uint32_t pixelSize);
        [[nodiscard]] BitmapTextLineMetrics
        readCurrentMetrics(std::size_t faceIndex) const;
        [[nodiscard]] const BitmapGlyph *cacheEmptyGlyph(uint64_t key,
                                                         uint32_t codepoint,
                                                         uint32_t pixelSize,
                                                         float advance,
                                                         uint32_t faceIndex);
        [[nodiscard]] bool reserveRegion(uint32_t width,
                                         uint32_t height,
                                         uint32_t &x,
                                         uint32_t &y);
        [[nodiscard]] bool uploadGlyph(uint32_t x,
                                       uint32_t y,
                                       uint32_t width,
                                       uint32_t height,
                                       const uint8_t *pixels);

        wgpu::Device m_device;
        wgpu::Queue m_queue;
        wgpu::Texture m_texture;
        wgpu::TextureView m_textureView;
        void *m_library = nullptr;
        // The primary text face is first. Remaining faces are ordered
        // fallbacks (for example remapped icon fonts); FreeType faces remain
        // opaque in this public header.
        std::vector<void *> m_faces;
        std::vector<uint32_t> m_currentPixelSizes;
        uint32_t m_atlasSize = 0;
        uint32_t m_minPixelSize = 8;
        uint32_t m_maxPixelSize = 24;
        uint32_t m_cursorX = 1;
        uint32_t m_cursorY = 1;
        uint32_t m_rowHeight = 0;
        HashMap<uint64_t, BitmapGlyph> m_glyphs;
        HashMap<uint32_t, BitmapTextLineMetrics> m_metrics;
    };

    struct BESS_API BitmapTextInstance {
        float position[3] = {0.f, 0.f, 0.f};
        // Screen-space glyphs on the same line share this snap anchor. Keeping
        // it separate from the glyph quad prevents bearing-dependent baseline
        // shifts while retaining the storage-buffer alignment padding.
        float snapAnchorX = 0.f;
        float size[2] = {0.f, 0.f};
        float rotation = 0.f;
        float snapAnchorY = 0.f;
        float color[4] = {1.f, 1.f, 1.f, 1.f};
        float uvRect[4] = {0.f, 0.f, 1.f, 1.f};
        uint32_t id[2] = {Bess::PickingId::invalidRuntimeId, 0};
        uint32_t flags[2] = {0, 0};
    };

    static_assert(sizeof(BitmapTextInstance) == 80,
                  "BitmapTextInstance must match WGSL layout");
    static_assert(offsetof(BitmapTextInstance, snapAnchorX) == 12);
    static_assert(offsetof(BitmapTextInstance, snapAnchorY) == 28);

    struct BESS_API BitmapTextDrawRun {
        uint32_t firstGlyph = 0;
        uint32_t glyphCount = 0;
        float zIndex = 0.f;
        uint64_t submitOrder = 0;
        Core::Renderer::RendererScissorState scissor{};
    };

    class BESS_API BitmapTextBatch {
      public:
        void clear() {
            m_instances.clear();
            m_submitOrders.clear();
            m_scissors.clear();
            m_drawRuns.clear();
        }

        void push(const BitmapTextInstance &instance,
                  uint64_t submitOrder = 0,
                  Core::Renderer::RendererScissorState scissor = {}) {
            m_instances.push_back(instance);
            m_submitOrders.push_back(submitOrder);
            m_scissors.push_back(scissor);
        }

        void prepareForRendering() {
            if (m_instances.size() > 1) {
                m_sortIndices.resize(m_instances.size());
                for (uint32_t i = 0; i < m_instances.size(); ++i) {
                    m_sortIndices[i] = i;
                }

                std::stable_sort(
                    m_sortIndices.begin(),
                    m_sortIndices.end(),
                    [this](uint32_t a, uint32_t b) {
                        if (m_instances[a].position[2] !=
                            m_instances[b].position[2]) {
                            return m_instances[a].position[2] <
                                   m_instances[b].position[2];
                        }
                        if (m_submitOrders[a] != m_submitOrders[b]) {
                            return m_submitOrders[a] < m_submitOrders[b];
                        }
                        return a < b;
                    });

                m_sortInstances.resize(m_instances.size());
                m_sortSubmitOrders.resize(m_instances.size());
                m_sortScissors.resize(m_instances.size());
                for (uint32_t i = 0; i < m_sortIndices.size(); ++i) {
                    const uint32_t oldIdx = m_sortIndices[i];
                    m_sortInstances[i] = m_instances[oldIdx];
                    m_sortSubmitOrders[i] = m_submitOrders[oldIdx];
                    m_sortScissors[i] = m_scissors[oldIdx];
                }
                m_instances = m_sortInstances;
                m_submitOrders = m_sortSubmitOrders;
                m_scissors = m_sortScissors;
            }

            m_drawRuns.clear();
            if (m_instances.empty()) {
                return;
            }

            m_drawRuns.reserve(m_instances.size());
            for (uint32_t i = 0; i < m_instances.size(); ++i) {
                const float zIndex = m_instances[i].position[2];
                const uint64_t submitOrder = m_submitOrders[i];
                const auto scissor = m_scissors[i];
                if (m_drawRuns.empty() || m_drawRuns.back().zIndex != zIndex ||
                    m_drawRuns.back().scissor != scissor) {
                    m_drawRuns.push_back({
                        .firstGlyph = i,
                        .glyphCount = 1,
                        .zIndex = zIndex,
                        .submitOrder = submitOrder,
                        .scissor = scissor,
                    });
                } else {
                    m_drawRuns.back().glyphCount++;
                }
            }
        }

        [[nodiscard]] bool empty() const noexcept {
            return m_instances.empty();
        }
        [[nodiscard]] uint32_t count() const noexcept {
            return static_cast<uint32_t>(m_instances.size());
        }
        [[nodiscard]] uint64_t byteSize() const noexcept {
            return static_cast<uint64_t>(m_instances.size()) *
                   sizeof(BitmapTextInstance);
        }
        [[nodiscard]] const BitmapTextInstance *data() const noexcept {
            return m_instances.data();
        }
        [[nodiscard]] const BitmapTextDrawRun *drawRunsData() const noexcept {
            return m_drawRuns.data();
        }
        [[nodiscard]] uint32_t drawRunsCount() const noexcept {
            return static_cast<uint32_t>(m_drawRuns.size());
        }

      private:
        std::vector<BitmapTextInstance> m_instances;
        std::vector<uint64_t> m_submitOrders;
        std::vector<Core::Renderer::RendererScissorState> m_scissors;
        std::vector<uint32_t> m_sortIndices;
        std::vector<BitmapTextInstance> m_sortInstances;
        std::vector<uint64_t> m_sortSubmitOrders;
        std::vector<Core::Renderer::RendererScissorState> m_sortScissors;
        std::vector<BitmapTextDrawRun> m_drawRuns;
    };

    class BESS_API BitmapTextPipeline {
      public:
        BitmapTextPipeline() = default;
        BitmapTextPipeline(const BitmapTextPipeline &) = delete;
        BitmapTextPipeline &operator=(const BitmapTextPipeline &) = delete;

        void init(const wgpu::Device &device,
                  wgpu::TextureFormat targetFormat,
                  const wgpu::Buffer &frameBuffer,
                  uint64_t frameBufferSize,
                  wgpu::TextureFormat pickingFormat,
                  const wgpu::TextureView &atlasView);

        void destroy();

        [[nodiscard]] bool ensureInstanceBufferSize(std::size_t glyphCount);

        void uploadInstances(const wgpu::Queue &queue,
                             const BitmapTextInstance *instances,
                             uint64_t byteSize) const;

        [[nodiscard]] const wgpu::RenderPipeline &getPipeline() const;
        [[nodiscard]] const wgpu::BindGroup &getBindGroup() const;

        void drawInstances(wgpu::RenderPassEncoder &renderPass,
                           uint32_t firstGlyph,
                           uint32_t glyphCount) const;
        void draw(wgpu::RenderPassEncoder &renderPass,
                  uint32_t firstGlyph,
                  uint32_t glyphCount) const;

      private:
        void createShader();
        void createBindGroupLayout();
        void createPipelineLayout();
        void createSampler();
        void createBindGroup();
        void createPipelineState();

        wgpu::Device m_device;
        wgpu::TextureFormat m_targetFormat = wgpu::TextureFormat::BGRA8Unorm;
        wgpu::TextureFormat m_pickingFormat = wgpu::TextureFormat::Undefined;
        wgpu::Buffer m_frameBuffer;
        uint64_t m_frameBufferSize = 0;
        wgpu::Buffer m_instanceBuffer;
        uint64_t m_instanceBufferSize = 0;
        wgpu::TextureView m_atlasView;
        wgpu::Sampler m_sampler;
        wgpu::BindGroupLayout m_bindGroupLayout;
        wgpu::PipelineLayout m_pipelineLayout;
        wgpu::BindGroup m_bindGroup;
        wgpu::RenderPipeline m_pipeline;
        std::unique_ptr<WgpuShader> m_shader;
    };

    bool appendBitmapText(std::string_view text,
                          const Core::Renderer::FontProps &props,
                          float projectedPixelSize,
                          BitmapFontAtlas &atlas,
                          BitmapTextBatch &batch,
                          uint64_t submitOrder = 0,
                          Core::Renderer::RendererScissorState scissor = {});

    bool ensureBitmapTextGlyphs(std::string_view text,
                                float projectedPixelSize,
                                BitmapFontAtlas &atlas);

    glm::vec2 measureBitmapText(std::string_view text,
                                const Core::Renderer::FontProps &props,
                                BitmapFontAtlas &atlas);

    float bitmapCenterOffsetX(std::string_view text,
                              const Core::Renderer::FontProps &props,
                              BitmapFontAtlas &atlas);

    float bitmapCenterOffsetY(std::string_view text,
                              const Core::Renderer::FontProps &props,
                              BitmapFontAtlas &atlas);

} // namespace Bess::Wgpu::Text
