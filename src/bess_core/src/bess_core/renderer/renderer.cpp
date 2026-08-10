#include "bess_core/renderer/renderer_2d.h"
#include <cstring>
#include <stdexcept>

namespace Bess::Core::Renderer {
    IRenderer2D::~IRenderer2D() = default;

    TextureReadbackResult IRenderer2D::readTexture(TextureHandle texture,
                                                   uint32_t x,
                                                   uint32_t y,
                                                   uint32_t width,
                                                   uint32_t height) {
        return readTexture({.texture = texture,
                            .x = x,
                            .y = y,
                            .width = width,
                            .height = height});
    }

    PickingId
    IRenderer2D::readPickingId(TextureHandle texture, uint32_t x, uint32_t y) {
        const auto ids = readPickingIds(texture, x, y, 1, 1);
        return ids.empty() ? PickingId::invalid() : ids.front();
    }

    std::vector<PickingId> IRenderer2D::readPickingIds(TextureHandle texture,
                                                       uint32_t x,
                                                       uint32_t y,
                                                       uint32_t width,
                                                       uint32_t height) {
        const auto readback = readTexture(texture, x, y, width, height);
        if (readback.format != Renderer2DTargetFormat::RG32Uint) {
            throw std::runtime_error(
                "readPickingIds requires an RG32Uint texture");
        }
        if (readback.bytesPerPixel != sizeof(uint32_t) * 2) {
            throw std::runtime_error(
                "readPickingIds expected 2 uint32 channels per pixel");
        }

        const size_t pixelCount = static_cast<size_t>(readback.width) *
                                  static_cast<size_t>(readback.height);
        if (readback.pixels.size() <
            pixelCount * static_cast<size_t>(readback.bytesPerPixel)) {
            throw std::runtime_error(
                "readPickingIds received an incomplete readback buffer");
        }

        std::vector<PickingId> ids(pixelCount);
        const auto *src = readback.pixels.data();
        for (size_t i = 0; i < pixelCount; ++i) {
            PickingId id{};
            std::memcpy(&id.runtimeId,
                        src + (i * readback.bytesPerPixel),
                        sizeof(uint32_t));
            std::memcpy(&id.info,
                        src + (i * readback.bytesPerPixel) + sizeof(uint32_t),
                        sizeof(uint32_t));
            ids[i] = id;
        }
        return ids;
    }

    void IRenderer2D::requestPickingId(TextureHandle texture,
                                       uint32_t x,
                                       uint32_t y) {
        requestPickingIds(
            {.texture = texture, .x = x, .y = y, .width = 1, .height = 1});
    }
} // namespace Bess::Core::Renderer
