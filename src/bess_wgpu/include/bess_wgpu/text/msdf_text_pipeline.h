#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_wgpu/wgpu_shader.h"
#include "bess_wgpu/wgpu_texture.h"
#include <cstdint>
#include <vector>
#include <webgpu/webgpu_cpp.h>

namespace Bess::Wgpu {

    struct TextureResource;

    namespace Text {

        struct MsdfTextInstance {
            float position[3] = {0.f, 0.f, 0.f};
            float pxRange = 4.f;
            float size[2] = {0.f, 0.f};
            float rotation = 0.f;
            float padding0 = 0.f;
            float color[4] = {1.f, 1.f, 1.f, 1.f};
            float uvRect[4] = {0.f, 0.f, 1.f, 1.f};
            uint32_t id[2] = {Bess::PickingId::invalidRuntimeId, 0};
            uint32_t flags[2] = {0, 0};
        };

        static_assert(sizeof(MsdfTextInstance) == 80,
                      "MsdfTextInstance must match WGSL layout");

        struct TextDrawRun {
            uint32_t firstGlyph = 0;
            uint32_t glyphCount = 0;
            float zIndex = 0.f;
        };

        class MsdfTextBatch {
          public:
            void clear() {
                m_instances.clear();
                m_drawRuns.clear();
            }

            void push(const MsdfTextInstance &instance) {
                m_instances.push_back(instance);
            }

            void prepareForRendering() {
                if (m_instances.size() > 1) {
                    std::stable_sort(m_instances.begin(), m_instances.end(),
                                     [](const MsdfTextInstance &a,
                                        const MsdfTextInstance &b) {
                                         if (a.position[2] != b.position[2]) {
                                             return a.position[2] <
                                                    b.position[2];
                                         }
                                         return false;
                                     });
                }

                m_drawRuns.clear();
                if (m_instances.empty()) {
                    return;
                }

                m_drawRuns.reserve(m_instances.size());
                for (uint32_t i = 0; i < m_instances.size(); ++i) {
                    const float zIndex = m_instances[i].position[2];
                    if (m_drawRuns.empty() ||
                        m_drawRuns.back().zIndex != zIndex) {
                        m_drawRuns.push_back({
                            .firstGlyph = i,
                            .glyphCount = 1,
                            .zIndex = zIndex,
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
                       sizeof(MsdfTextInstance);
            }

            [[nodiscard]] const MsdfTextInstance *data() const noexcept {
                return m_instances.data();
            }

            [[nodiscard]] const TextDrawRun *drawRunsData() const noexcept {
                return m_drawRuns.data();
            }

            [[nodiscard]] uint32_t drawRunsCount() const noexcept {
                return static_cast<uint32_t>(m_drawRuns.size());
            }

          private:
            std::vector<MsdfTextInstance> m_instances;
            std::vector<TextDrawRun> m_drawRuns;
        };

        class MsdfTextPipeline {
          public:
            void init(const wgpu::Device &device,
                      wgpu::TextureFormat targetFormat,
                      const wgpu::Buffer &frameBuffer, uint64_t frameBufferSize,
                      wgpu::TextureFormat pickingFormat,
                      const TextureResource &atlasResource);

            void destroy();

            [[nodiscard]] bool
            ensureInstanceBufferSize(std::size_t glyphCount);

            void uploadInstances(const wgpu::Queue &queue,
                                 const MsdfTextInstance *instances,
                                 uint64_t byteSize) const;

            void draw(wgpu::RenderPassEncoder &renderPass, uint32_t firstGlyph,
                      uint32_t glyphCount) const;

          private:
            void createShader();
            void createBindGroupLayout();
            void createPipelineLayout();
            void createSampler();
            void createBindGroup();
            void createPipelineState();

            wgpu::Device m_device;
            wgpu::TextureFormat m_targetFormat =
                wgpu::TextureFormat::BGRA8Unorm;
            wgpu::TextureFormat m_pickingFormat =
                wgpu::TextureFormat::Undefined;
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

        template <typename TAtlas>
        bool appendMsdfText(std::string_view text,
                            const Core::Renderer::FontProps &props,
                            const TAtlas &atlas, MsdfTextBatch &batch);

        template <typename TAtlas>
        glm::vec2 measureMsdfText(std::string_view text,
                                  const Core::Renderer::FontProps &props,
                                  const TAtlas &atlas);

        template <typename TAtlas>
        float msdfCenterOffsetY(std::string_view text,
                                const Core::Renderer::FontProps &props,
                                const TAtlas &atlas);

    } // namespace Text

} // namespace Bess::Wgpu
