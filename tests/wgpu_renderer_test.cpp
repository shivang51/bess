#include "bess_wgpu/wgpu_renderer_2d.h"

#include <gtest/gtest.h>

TEST(WgpuRendererTest, OptimalAndSuboptimalSurfaceTexturesArePresentable) {
    using Bess::Wgpu::Detail::isPresentableSurfaceTextureStatus;
    using Status = wgpu::SurfaceGetCurrentTextureStatus;

    EXPECT_TRUE(isPresentableSurfaceTextureStatus(Status::SuccessOptimal));
    EXPECT_TRUE(isPresentableSurfaceTextureStatus(Status::SuccessSuboptimal));
    EXPECT_FALSE(isPresentableSurfaceTextureStatus(Status::Timeout));
    EXPECT_FALSE(isPresentableSurfaceTextureStatus(Status::Outdated));
    EXPECT_FALSE(isPresentableSurfaceTextureStatus(Status::Lost));
    EXPECT_FALSE(isPresentableSurfaceTextureStatus(Status::Error));
}
