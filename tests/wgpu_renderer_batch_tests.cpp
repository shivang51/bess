#include "wgpu_renderer_2d_batches.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>

namespace {
    using namespace Bess::Wgpu::Renderer2DDetail;

    TEST(WgpuRendererBatchTests,
         PrimitiveStorageGrowsGeometricallyAndPreservesDrawState) {
        PrimitiveBatch batch;
        batch.configure(1, 4);
        EXPECT_EQ(batch.capacity(), 1u);

        for (uint32_t index = 0; index < 4; ++index) {
            auto &instance = batch.push(index + 1, index + 10);
            instance.position[0] = static_cast<float>(index + 100);
        }

        EXPECT_EQ(batch.capacity(), 4u);
        ASSERT_EQ(batch.count(), 4u);
        ASSERT_EQ(batch.drawRunsCount(), 4u);
        for (uint32_t index = 0; index < batch.count(); ++index) {
            EXPECT_FLOAT_EQ(batch.data()[index].position[0],
                            static_cast<float>(index + 100));
            EXPECT_EQ(batch.drawRunsData()[index].texture, index + 1);
            EXPECT_EQ(batch.drawRunsData()[index].submitOrder, index + 10);
        }

        EXPECT_THROW(static_cast<void>(batch.push(5, 15)), std::runtime_error);
    }

    TEST(WgpuRendererBatchTests,
         CustomAndShadowStorageGrowWithoutLosingInstances) {
        CustomQuadBatch custom;
        custom.configure(1, 3);
        for (uint32_t index = 0; index < 3; ++index) {
            auto &instance = custom.push(index + 1, index);
            instance.position[1] = static_cast<float>(index + 20);
        }
        EXPECT_EQ(custom.capacity(), 3u);
        ASSERT_EQ(custom.count(), 3u);
        for (uint32_t index = 0; index < custom.count(); ++index) {
            EXPECT_FLOAT_EQ(custom.data()[index].position[1],
                            static_cast<float>(index + 20));
        }
        EXPECT_THROW(static_cast<void>(custom.push(4, 3)), std::runtime_error);

        ShadowBatch shadows;
        shadows.configure(1, 3);
        for (uint32_t index = 0; index < 3; ++index) {
            auto &instance = shadows.push(index);
            instance.position[2] = static_cast<float>(3 - index);
        }
        EXPECT_EQ(shadows.capacity(), 3u);
        shadows.prepareForRendering();
        ASSERT_EQ(shadows.count(), 3u);
        EXPECT_FLOAT_EQ(shadows.data()[0].position[2], 1.f);
        EXPECT_FLOAT_EQ(shadows.data()[1].position[2], 2.f);
        EXPECT_FLOAT_EQ(shadows.data()[2].position[2], 3.f);
        EXPECT_THROW(static_cast<void>(shadows.push(3)), std::runtime_error);
    }
} // namespace
