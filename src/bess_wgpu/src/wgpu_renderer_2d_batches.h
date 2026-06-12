#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "bess_wgpu/piplines/custom_quad_pipeline.h"
#include "bess_wgpu/piplines/primitive_pipeline.h"
#include "bess_wgpu/piplines/shadow_pipeline.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace Bess::Wgpu::Renderer2DDetail {

    struct DrawRun {
        Core::Renderer::TextureHandle texture = 0;
        uint32_t firstInstance = 0;
        uint32_t instanceCount = 0;
        float zIndex = 0.f;
        uint64_t submitOrder = 0;
    };

    struct CustomQuadDrawRun {
        CustomQuadShaderHandle shader = 0;
        uint32_t firstInstance = 0;
        uint32_t instanceCount = 0;
        float zIndex = 0.f;
        uint64_t submitOrder = 0;
    };

    struct ShadowDrawRun {
        uint32_t firstInstance = 0;
        uint32_t instanceCount = 0;
        float zIndex = 0.f;
        uint64_t submitOrder = 0;
    };

    enum class TransparentDrawKind : uint8_t {
        Shadow,
        Primitive,
        CustomQuad,
        PathFill,
        PathStroke,
        Text,
    };

    struct TransparentDrawItem {
        TransparentDrawKind kind = TransparentDrawKind::Primitive;
        float zIndex = 0.f;
        uint32_t index = 0;
        uint64_t order = 0;
    };

    class PrimitiveBatch {
      public:
        void configure(uint32_t initialCapacity, uint32_t maxCapacity) {
            (void)initialCapacity;
            m_maxCapacity = std::max(1u, maxCapacity);
            m_gpuInstances.resize(m_maxCapacity);
            m_submitOrders.resize(m_maxCapacity);
            m_drawRuns.resize(m_maxCapacity);
            m_gpuInstancesPtr = m_gpuInstances.data();
            m_submitOrdersPtr = m_submitOrders.data();
            m_drawRunsPtr = m_drawRuns.data();
            m_instanceCount = 0;
            m_drawRunsCount = 0;
        }

        void clear() {
            m_instanceCount = 0;
            m_drawRunsCount = 0;
        }

        Piplines::PrimitiveInstance &
        push(Core::Renderer::TextureHandle texture, uint64_t submitOrder) {
            if (m_instanceCount >= m_maxCapacity) {
                throw std::runtime_error("WGPU quad batch capacity exceeded");
            }
            const uint32_t instanceIndex = m_instanceCount;
            if (m_drawRunsCount == 0 ||
                m_drawRunsPtr[m_drawRunsCount - 1].texture != texture) {
                m_drawRunsPtr[m_drawRunsCount++] = {
                    .texture = texture,
                    .firstInstance = instanceIndex,
                    .instanceCount = 1,
                    .submitOrder = submitOrder,
                };
            } else {
                m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
            }
            m_submitOrdersPtr[m_instanceCount] = submitOrder;
            return m_gpuInstancesPtr[m_instanceCount++];
        }

        void prepareForRendering(bool sortBackToFront) {
            if (!sortBackToFront || m_instanceCount == 0) {
                return;
            }

            if (m_instanceCount == 1) {
                if (m_drawRunsCount == 1) {
                    m_drawRunsPtr[0].zIndex =
                        m_gpuInstancesPtr[0].position[2];
                    m_drawRunsPtr[0].submitOrder = m_submitOrdersPtr[0];
                }
                return;
            }

            m_sortIndices.resize(m_instanceCount);
            uint32_t *indicesPtr = m_sortIndices.data();
            for (uint32_t i = 0; i < m_instanceCount; ++i) {
                indicesPtr[i] = i;
            }

            std::stable_sort(
                m_sortIndices.begin(), m_sortIndices.end(),
                [this](uint32_t a, uint32_t b) {
                    if (m_gpuInstancesPtr[a].position[2] !=
                        m_gpuInstancesPtr[b].position[2]) {
                        return m_gpuInstancesPtr[a].position[2] <
                               m_gpuInstancesPtr[b].position[2];
                    }
                    if (m_submitOrdersPtr[a] != m_submitOrdersPtr[b]) {
                        return m_submitOrdersPtr[a] < m_submitOrdersPtr[b];
                    }
                    return a < b;
                });

            m_sortTextures.resize(m_instanceCount);
            Core::Renderer::TextureHandle *texPtr = m_sortTextures.data();
            for (uint32_t r = 0; r < m_drawRunsCount; ++r) {
                const auto &run = m_drawRunsPtr[r];
                for (uint32_t i = 0; i < run.instanceCount; ++i) {
                    texPtr[run.firstInstance + i] = run.texture;
                }
            }

            m_sortInstances.resize(m_instanceCount);
            Piplines::PrimitiveInstance *sortedPtr = m_sortInstances.data();
            m_sortSubmitOrders.resize(m_instanceCount);
            uint64_t *sortedOrdersPtr = m_sortSubmitOrders.data();
            m_drawRunsCount = 0;

            for (uint32_t i = 0; i < m_instanceCount; ++i) {
                uint32_t oldIdx = indicesPtr[i];
                sortedPtr[i] = m_gpuInstancesPtr[oldIdx];
                sortedOrdersPtr[i] = m_submitOrdersPtr[oldIdx];
                Core::Renderer::TextureHandle tex = texPtr[oldIdx];
                const float zIndex = sortedPtr[i].position[2];
                const uint64_t submitOrder = sortedOrdersPtr[i];

                if (m_drawRunsCount == 0 ||
                    m_drawRunsPtr[m_drawRunsCount - 1].texture != tex ||
                    m_drawRunsPtr[m_drawRunsCount - 1].zIndex != zIndex) {
                    m_drawRunsPtr[m_drawRunsCount++] = {
                        .texture = tex,
                        .firstInstance = i,
                        .instanceCount = 1,
                        .zIndex = zIndex,
                        .submitOrder = submitOrder,
                    };
                } else {
                    m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
                }
            }
            for (uint32_t i = 0; i < m_instanceCount; ++i) {
                m_gpuInstancesPtr[i] = sortedPtr[i];
                m_submitOrdersPtr[i] = sortedOrdersPtr[i];
            }
        }

        [[nodiscard]] bool empty() const noexcept {
            return m_instanceCount == 0;
        }

        [[nodiscard]] uint32_t count() const noexcept {
            return m_instanceCount;
        }

        [[nodiscard]] uint64_t byteSize() const noexcept {
            return static_cast<uint64_t>(m_instanceCount) *
                   sizeof(Piplines::PrimitiveInstance);
        }

        [[nodiscard]] const Piplines::PrimitiveInstance *
        data() const noexcept {
            return m_gpuInstancesPtr;
        }

        [[nodiscard]] const DrawRun *drawRunsData() const noexcept {
            return m_drawRunsPtr;
        }

        [[nodiscard]] uint32_t drawRunsCount() const noexcept {
            return m_drawRunsCount;
        }

      private:
        std::vector<Piplines::PrimitiveInstance> m_gpuInstances;
        std::vector<uint64_t> m_submitOrders;
        std::vector<DrawRun> m_drawRuns;
        std::vector<uint32_t> m_sortIndices;
        std::vector<Core::Renderer::TextureHandle> m_sortTextures;
        std::vector<Piplines::PrimitiveInstance> m_sortInstances;
        std::vector<uint64_t> m_sortSubmitOrders;
        Piplines::PrimitiveInstance *m_gpuInstancesPtr = nullptr;
        uint64_t *m_submitOrdersPtr = nullptr;
        DrawRun *m_drawRunsPtr = nullptr;
        uint32_t m_instanceCount = 0;
        uint32_t m_drawRunsCount = 0;
        uint32_t m_maxCapacity = 1;
    };

    class CustomQuadBatch {
      public:
        void configure(uint32_t initialCapacity, uint32_t maxCapacity) {
            (void)initialCapacity;
            m_maxCapacity = std::max(1u, maxCapacity);
            m_gpuInstances.resize(m_maxCapacity);
            m_submitOrders.resize(m_maxCapacity);
            m_drawRuns.resize(m_maxCapacity);
            m_gpuInstancesPtr = m_gpuInstances.data();
            m_submitOrdersPtr = m_submitOrders.data();
            m_drawRunsPtr = m_drawRuns.data();
            m_instanceCount = 0;
            m_drawRunsCount = 0;
        }

        void clear() {
            m_instanceCount = 0;
            m_drawRunsCount = 0;
        }

        CustomQuadInstance &push(CustomQuadShaderHandle shader,
                                 uint64_t submitOrder) {
            if (shader == 0) {
                throw std::runtime_error(
                    "Custom quad shader handle must be non-zero");
            }
            if (m_instanceCount >= m_maxCapacity) {
                throw std::runtime_error(
                    "WGPU custom quad batch capacity exceeded");
            }

            const uint32_t instanceIndex = m_instanceCount;
            if (m_drawRunsCount == 0 ||
                m_drawRunsPtr[m_drawRunsCount - 1].shader != shader) {
                m_drawRunsPtr[m_drawRunsCount++] = {
                    .shader = shader,
                    .firstInstance = instanceIndex,
                    .instanceCount = 1,
                    .submitOrder = submitOrder,
                };
            } else {
                m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
            }
            m_submitOrdersPtr[m_instanceCount] = submitOrder;
            return m_gpuInstancesPtr[m_instanceCount++];
        }

        void prepareForRendering(bool sortBackToFront) {
            if (!sortBackToFront || m_instanceCount == 0) {
                return;
            }

            if (m_instanceCount == 1) {
                if (m_drawRunsCount == 1) {
                    m_drawRunsPtr[0].zIndex =
                        m_gpuInstancesPtr[0].position[2];
                    m_drawRunsPtr[0].submitOrder = m_submitOrdersPtr[0];
                }
                return;
            }

            m_sortIndices.resize(m_instanceCount);
            uint32_t *indicesPtr = m_sortIndices.data();
            for (uint32_t i = 0; i < m_instanceCount; ++i) {
                indicesPtr[i] = i;
            }

            std::stable_sort(
                m_sortIndices.begin(), m_sortIndices.end(),
                [this](uint32_t a, uint32_t b) {
                    if (m_gpuInstancesPtr[a].position[2] !=
                        m_gpuInstancesPtr[b].position[2]) {
                        return m_gpuInstancesPtr[a].position[2] <
                               m_gpuInstancesPtr[b].position[2];
                    }
                    if (m_submitOrdersPtr[a] != m_submitOrdersPtr[b]) {
                        return m_submitOrdersPtr[a] < m_submitOrdersPtr[b];
                    }
                    return a < b;
                });

            m_sortShaders.resize(m_instanceCount);
            CustomQuadShaderHandle *shaderPtr = m_sortShaders.data();
            for (uint32_t r = 0; r < m_drawRunsCount; ++r) {
                const auto &run = m_drawRunsPtr[r];
                for (uint32_t i = 0; i < run.instanceCount; ++i) {
                    shaderPtr[run.firstInstance + i] = run.shader;
                }
            }

            m_sortInstances.resize(m_instanceCount);
            CustomQuadInstance *sortedPtr = m_sortInstances.data();
            m_sortSubmitOrders.resize(m_instanceCount);
            uint64_t *sortedOrdersPtr = m_sortSubmitOrders.data();
            m_drawRunsCount = 0;

            for (uint32_t i = 0; i < m_instanceCount; ++i) {
                uint32_t oldIdx = indicesPtr[i];
                sortedPtr[i] = m_gpuInstancesPtr[oldIdx];
                sortedOrdersPtr[i] = m_submitOrdersPtr[oldIdx];
                CustomQuadShaderHandle shader = shaderPtr[oldIdx];
                const float zIndex = sortedPtr[i].position[2];
                const uint64_t submitOrder = sortedOrdersPtr[i];

                if (m_drawRunsCount == 0 ||
                    m_drawRunsPtr[m_drawRunsCount - 1].shader != shader ||
                    m_drawRunsPtr[m_drawRunsCount - 1].zIndex != zIndex) {
                    m_drawRunsPtr[m_drawRunsCount++] = {
                        .shader = shader,
                        .firstInstance = i,
                        .instanceCount = 1,
                        .zIndex = zIndex,
                        .submitOrder = submitOrder,
                    };
                } else {
                    m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
                }
            }
            for (uint32_t i = 0; i < m_instanceCount; ++i) {
                m_gpuInstancesPtr[i] = sortedPtr[i];
                m_submitOrdersPtr[i] = sortedOrdersPtr[i];
            }
        }

        [[nodiscard]] bool empty() const noexcept {
            return m_instanceCount == 0;
        }

        [[nodiscard]] uint32_t count() const noexcept {
            return m_instanceCount;
        }

        [[nodiscard]] uint64_t byteSize() const noexcept {
            return static_cast<uint64_t>(m_instanceCount) *
                   sizeof(CustomQuadInstance);
        }

        [[nodiscard]] const CustomQuadInstance *data() const noexcept {
            return m_gpuInstancesPtr;
        }

        [[nodiscard]] const CustomQuadDrawRun *drawRunsData() const noexcept {
            return m_drawRunsPtr;
        }

        [[nodiscard]] uint32_t drawRunsCount() const noexcept {
            return m_drawRunsCount;
        }

      private:
        std::vector<CustomQuadInstance> m_gpuInstances;
        std::vector<uint64_t> m_submitOrders;
        std::vector<CustomQuadDrawRun> m_drawRuns;
        std::vector<uint32_t> m_sortIndices;
        std::vector<CustomQuadShaderHandle> m_sortShaders;
        std::vector<CustomQuadInstance> m_sortInstances;
        std::vector<uint64_t> m_sortSubmitOrders;
        CustomQuadInstance *m_gpuInstancesPtr = nullptr;
        uint64_t *m_submitOrdersPtr = nullptr;
        CustomQuadDrawRun *m_drawRunsPtr = nullptr;
        uint32_t m_instanceCount = 0;
        uint32_t m_drawRunsCount = 0;
        uint32_t m_maxCapacity = 1;
    };

    class ShadowBatch {
      public:
        void configure(uint32_t initialCapacity, uint32_t maxCapacity) {
            (void)initialCapacity;
            m_maxCapacity = std::max(1u, maxCapacity);
            m_instances.resize(m_maxCapacity);
            m_submitOrders.resize(m_maxCapacity);
            m_drawRuns.resize(m_maxCapacity);
            m_instancesPtr = m_instances.data();
            m_submitOrdersPtr = m_submitOrders.data();
            m_drawRunsPtr = m_drawRuns.data();
            m_instanceCount = 0;
            m_drawRunsCount = 0;
        }

        void clear() {
            m_instanceCount = 0;
            m_drawRunsCount = 0;
        }

        Piplines::ShadowInstance &push(uint64_t submitOrder) {
            if (m_instanceCount >= m_maxCapacity) {
                throw std::runtime_error("WGPU shadow batch capacity exceeded");
            }
            m_submitOrdersPtr[m_instanceCount] = submitOrder;
            return m_instancesPtr[m_instanceCount++];
        }

        void prepareForRendering() {
            if (m_instanceCount == 0) {
                return;
            }

            if (m_instanceCount > 1) {
                m_sortIndices.resize(m_instanceCount);
                uint32_t *indicesPtr = m_sortIndices.data();
                for (uint32_t i = 0; i < m_instanceCount; ++i) {
                    indicesPtr[i] = i;
                }
                std::stable_sort(
                    m_sortIndices.begin(), m_sortIndices.end(),
                    [this](uint32_t a, uint32_t b) {
                        if (m_instancesPtr[a].position[2] !=
                            m_instancesPtr[b].position[2]) {
                            return m_instancesPtr[a].position[2] <
                                   m_instancesPtr[b].position[2];
                        }
                        if (m_submitOrdersPtr[a] != m_submitOrdersPtr[b]) {
                            return m_submitOrdersPtr[a] <
                                   m_submitOrdersPtr[b];
                        }
                        return a < b;
                    });

                m_sortInstances.resize(m_instanceCount);
                m_sortSubmitOrders.resize(m_instanceCount);
                for (uint32_t i = 0; i < m_instanceCount; ++i) {
                    const uint32_t oldIdx = indicesPtr[i];
                    m_sortInstances[i] = m_instancesPtr[oldIdx];
                    m_sortSubmitOrders[i] = m_submitOrdersPtr[oldIdx];
                }
                for (uint32_t i = 0; i < m_instanceCount; ++i) {
                    m_instancesPtr[i] = m_sortInstances[i];
                    m_submitOrdersPtr[i] = m_sortSubmitOrders[i];
                }
            }

            m_drawRunsCount = 0;
            for (uint32_t i = 0; i < m_instanceCount; ++i) {
                const float zIndex = m_instancesPtr[i].position[2];
                const uint64_t submitOrder = m_submitOrdersPtr[i];
                if (m_drawRunsCount == 0 ||
                    m_drawRunsPtr[m_drawRunsCount - 1].zIndex != zIndex) {
                    m_drawRunsPtr[m_drawRunsCount++] = {
                        .firstInstance = i,
                        .instanceCount = 1,
                        .zIndex = zIndex,
                        .submitOrder = submitOrder,
                    };
                } else {
                    m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
                }
            }
        }

        [[nodiscard]] bool empty() const noexcept {
            return m_instanceCount == 0;
        }

        [[nodiscard]] uint32_t count() const noexcept {
            return m_instanceCount;
        }

        [[nodiscard]] uint64_t byteSize() const noexcept {
            return static_cast<uint64_t>(m_instanceCount) *
                   sizeof(Piplines::ShadowInstance);
        }

        [[nodiscard]] const Piplines::ShadowInstance *data() const noexcept {
            return m_instancesPtr;
        }

        [[nodiscard]] const ShadowDrawRun *drawRunsData() const noexcept {
            return m_drawRunsPtr;
        }

        [[nodiscard]] uint32_t drawRunsCount() const noexcept {
            return m_drawRunsCount;
        }

      private:
        std::vector<Piplines::ShadowInstance> m_instances;
        std::vector<uint64_t> m_submitOrders;
        std::vector<uint32_t> m_sortIndices;
        std::vector<Piplines::ShadowInstance> m_sortInstances;
        std::vector<uint64_t> m_sortSubmitOrders;
        std::vector<ShadowDrawRun> m_drawRuns;
        Piplines::ShadowInstance *m_instancesPtr = nullptr;
        uint64_t *m_submitOrdersPtr = nullptr;
        ShadowDrawRun *m_drawRunsPtr = nullptr;
        uint32_t m_instanceCount = 0;
        uint32_t m_drawRunsCount = 0;
        uint32_t m_maxCapacity = 1;
    };

} // namespace Bess::Wgpu::Renderer2DDetail
