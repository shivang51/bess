#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/logger.h"
#include "stb_image.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace Bess::Canvas::SceneComponentDrawResources {
    namespace Detail {
        constexpr const char *kShadowTexturePath =
            "assets/images/shadow_texture.png";
        constexpr uint32_t kShadowTextureMaxSize = 384;

        struct StbiImageDeleter {
            void operator()(stbi_uc *pixels) const { stbi_image_free(pixels); }
        };

        [[nodiscard]] inline std::shared_ptr<Wgpu::WgpuTexture>
        loadShadowTexture() {
            int width = 0;
            int height = 0;
            int channels = 0;
            std::unique_ptr<stbi_uc, StbiImageDeleter> pixels(
                stbi_load(kShadowTexturePath, &width, &height, &channels, 4));
            if (!pixels) {
                throw std::runtime_error(std::string("Failed to load ") +
                                         kShadowTexturePath + ": " +
                                         stbi_failure_reason());
            }

            const uint32_t srcWidth = static_cast<uint32_t>(width);
            const uint32_t srcHeight = static_cast<uint32_t>(height);
            const uint32_t longest = std::max(srcWidth, srcHeight);
            if (longest <= kShadowTextureMaxSize) {
                return Wgpu::WgpuTexture::fromPixels(
                    pixels.get(), srcWidth, srcHeight);
            }

            const uint32_t dstWidth =
                std::max(1u, srcWidth * kShadowTextureMaxSize / longest);
            const uint32_t dstHeight =
                std::max(1u, srcHeight * kShadowTextureMaxSize / longest);
            std::vector<uint8_t> resized(static_cast<size_t>(dstWidth) *
                                         dstHeight * 4u);

            const stbi_uc *src = pixels.get();
            for (uint32_t y = 0; y < dstHeight; ++y) {
                const uint32_t srcY0 = y * srcHeight / dstHeight;
                const uint32_t srcY1 =
                    std::max(srcY0 + 1u, (y + 1u) * srcHeight / dstHeight);
                for (uint32_t x = 0; x < dstWidth; ++x) {
                    const uint32_t srcX0 = x * srcWidth / dstWidth;
                    const uint32_t srcX1 =
                        std::max(srcX0 + 1u, (x + 1u) * srcWidth / dstWidth);

                    uint64_t rgba[4] = {};
                    uint64_t count = 0;
                    for (uint32_t sy = srcY0; sy < srcY1; ++sy) {
                        for (uint32_t sx = srcX0; sx < srcX1; ++sx) {
                            const size_t offset =
                                (static_cast<size_t>(sy) * srcWidth + sx) * 4u;
                            rgba[0] += src[offset + 0u];
                            rgba[1] += src[offset + 1u];
                            rgba[2] += src[offset + 2u];
                            rgba[3] += src[offset + 3u];
                            ++count;
                        }
                    }

                    const size_t dstOffset =
                        (static_cast<size_t>(y) * dstWidth + x) * 4u;
                    resized[dstOffset + 0u] =
                        static_cast<uint8_t>(rgba[0] / count);
                    resized[dstOffset + 1u] =
                        static_cast<uint8_t>(rgba[1] / count);
                    resized[dstOffset + 2u] =
                        static_cast<uint8_t>(rgba[2] / count);
                    resized[dstOffset + 3u] =
                        static_cast<uint8_t>(rgba[3] / count);
                }
            }

            return Wgpu::WgpuTexture::fromPixels(
                resized.data(), dstWidth, dstHeight);
        }
    } // namespace Detail

    [[nodiscard]] inline Core::Renderer::TextureHandle
    getShadowTextureHandle() {
        static std::shared_ptr<Wgpu::WgpuTexture> shadowTexture;
        if (!shadowTexture) {
            try {
                shadowTexture = Detail::loadShadowTexture();
            } catch (const std::exception &error) {
                BESS_ERROR("{}", error.what());
                return 0;
            }
        }

        return shadowTexture ? shadowTexture->getHandle() : 0;
    }
} // namespace Bess::Canvas::SceneComponentDrawResources
