#pragma once

#include "bess_wgpu/wgpu_renderer_2d.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <png.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <webgpu/webgpu_cpp.h>

namespace Bess::Wgpu::Renderer2DDetail {

    inline wgpu::TextureFormat
    toWgpuFormat(Core::Renderer::Renderer2DTargetFormat format) {
        switch (format) {
        case Core::Renderer::Renderer2DTargetFormat::RGBA8Unorm:
            return wgpu::TextureFormat::RGBA8Unorm;
        case Core::Renderer::Renderer2DTargetFormat::RGBA16Float:
            return wgpu::TextureFormat::RGBA16Float;
        case Core::Renderer::Renderer2DTargetFormat::RG32Uint:
            return wgpu::TextureFormat::RG32Uint;
        case Core::Renderer::Renderer2DTargetFormat::None:
            return wgpu::TextureFormat::Undefined;
        case Core::Renderer::Renderer2DTargetFormat::BGRA8Unorm:
        default:
            return wgpu::TextureFormat::BGRA8Unorm;
        }
    }

    inline Core::Renderer::Renderer2DTargetFormat
    toRendererFormat(wgpu::TextureFormat format) {
        switch (format) {
        case wgpu::TextureFormat::RGBA8Unorm:
            return Core::Renderer::Renderer2DTargetFormat::RGBA8Unorm;
        case wgpu::TextureFormat::BGRA8Unorm:
            return Core::Renderer::Renderer2DTargetFormat::BGRA8Unorm;
        case wgpu::TextureFormat::RGBA16Float:
            return Core::Renderer::Renderer2DTargetFormat::RGBA16Float;
        case wgpu::TextureFormat::RG32Uint:
            return Core::Renderer::Renderer2DTargetFormat::RG32Uint;
        default:
            return Core::Renderer::Renderer2DTargetFormat::None;
        }
    }

    inline uint32_t bytesPerPixelForFormat(wgpu::TextureFormat format) {
        switch (format) {
        case wgpu::TextureFormat::RGBA8Unorm:
        case wgpu::TextureFormat::BGRA8Unorm:
            return 4;
        case wgpu::TextureFormat::RGBA16Float:
        case wgpu::TextureFormat::RG32Uint:
            return 8;
        default:
            throw std::runtime_error("Unsupported texture format for readback");
        }
    }

    inline bool canUseAsPrimitiveSampledTexture(wgpu::TextureFormat format) {
        switch (format) {
        case wgpu::TextureFormat::RGBA8Unorm:
        case wgpu::TextureFormat::BGRA8Unorm:
        case wgpu::TextureFormat::RGBA16Float:
            return true;
        default:
            return false;
        }
    }

    inline wgpu::Color toWgpuColor(const Core::Renderer::Color &color) {
        return {color.r, color.g, color.b, color.a};
    }

    inline uint32_t alignTo(uint32_t value, uint32_t alignment) {
        return ((value + alignment - 1) / alignment) * alignment;
    }

    inline Core::Renderer::TextureReadbackResult
    readTextureRegion(const wgpu::Instance &instance,
                      const wgpu::Device &device,
                      const wgpu::Queue &queue,
                      const wgpu::Texture &texture,
                      wgpu::TextureFormat format,
                      uint32_t textureWidth,
                      uint32_t textureHeight,
                      const Core::Renderer::TextureReadbackRegion &region) {
        if (texture == nullptr) {
            throw std::runtime_error("Cannot read a null WGPU texture");
        }
        if (region.width == 0 || region.height == 0) {
            throw std::runtime_error(
                "Texture readback region must be non-empty");
        }
        if (region.x >= textureWidth || region.y >= textureHeight ||
            region.width > textureWidth - region.x ||
            region.height > textureHeight - region.y) {
            throw std::runtime_error(
                "Texture readback region is outside the texture bounds");
        }

        const uint32_t bytesPerPixel = bytesPerPixelForFormat(format);
        const uint32_t unpaddedBytesPerRow = region.width * bytesPerPixel;
        const uint32_t paddedBytesPerRow = alignTo(unpaddedBytesPerRow, 256);
        const auto readbackSize =
            static_cast<uint64_t>(paddedBytesPerRow) * region.height;

        wgpu::BufferDescriptor bufferDescriptor{};
        bufferDescriptor.size = readbackSize;
        bufferDescriptor.usage =
            wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer readbackBuffer = device.CreateBuffer(&bufferDescriptor);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        wgpu::TexelCopyTextureInfo source{};
        source.texture = texture;
        source.mipLevel = 0;
        source.origin = {region.x, region.y, 0};
        source.aspect = wgpu::TextureAspect::All;

        wgpu::TexelCopyBufferInfo destination{};
        destination.buffer = readbackBuffer;
        destination.layout.offset = 0;
        destination.layout.bytesPerRow = paddedBytesPerRow;
        destination.layout.rowsPerImage = region.height;

        wgpu::Extent3D copySize{region.width, region.height, 1};
        encoder.CopyTextureToBuffer(&source, &destination, &copySize);

        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        queue.Submit(1, &commandBuffer);

        wgpu::MapAsyncStatus mapStatus = wgpu::MapAsyncStatus::Error;
        std::string mapError;
        auto mapCallback = [&mapStatus, &mapError](wgpu::MapAsyncStatus status,
                                                   wgpu::StringView message) {
            mapStatus = status;
            if (status != wgpu::MapAsyncStatus::Success &&
                message.data != nullptr) {
                mapError.assign(message.data, message.length);
            }
        };

        wgpu::Future mapFuture =
            readbackBuffer.MapAsync(wgpu::MapMode::Read,
                                    0,
                                    readbackSize,
                                    wgpu::CallbackMode::WaitAnyOnly,
                                    mapCallback);
        if (instance.WaitAny(mapFuture, UINT64_MAX) !=
            wgpu::WaitStatus::Success) {
            throw std::runtime_error(
                "Timed out waiting for WGPU texture readback");
        }
        if (mapStatus != wgpu::MapAsyncStatus::Success) {
            throw std::runtime_error(
                "Failed to map WGPU texture readback buffer: " + mapError);
        }

        const auto *mappedData = static_cast<const uint8_t *>(
            readbackBuffer.GetConstMappedRange(0, readbackSize));
        if (mappedData == nullptr) {
            readbackBuffer.Unmap();
            throw std::runtime_error(
                "Failed to access WGPU texture readback data");
        }

        Core::Renderer::TextureReadbackResult result;
        result.format = toRendererFormat(format);
        result.width = region.width;
        result.height = region.height;
        result.bytesPerPixel = bytesPerPixel;
        result.pixels.resize(static_cast<size_t>(region.height) *
                             unpaddedBytesPerRow);

        for (uint32_t row = 0; row < region.height; ++row) {
            const uint8_t *src =
                mappedData + (static_cast<size_t>(row) * paddedBytesPerRow);
            uint8_t *dst = result.pixels.data() +
                           (static_cast<size_t>(row) * unpaddedBytesPerRow);
            std::copy(src, src + unpaddedBytesPerRow, dst);
        }

        readbackBuffer.Unmap();
        return result;
    }

    struct FileDeleter {
        void operator()(FILE *file) const {
            if (file != nullptr) {
                std::fclose(file);
            }
        }
    };

    inline void writePng(const std::string &path,
                         const uint8_t *rgba,
                         uint32_t width,
                         uint32_t height) {
        using FilePtr = std::unique_ptr<FILE, FileDeleter>;
        FilePtr file(std::fopen(path.c_str(), "wb"));
        if (!file) {
            throw std::runtime_error("Failed to open PNG for writing: " + path);
        }

        png_structp png = png_create_write_struct(
            PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (png == nullptr) {
            throw std::runtime_error("Failed to create PNG write struct");
        }

        png_infop info = png_create_info_struct(png);
        if (info == nullptr) {
            png_destroy_write_struct(&png, nullptr);
            throw std::runtime_error("Failed to create PNG info struct");
        }

        if (setjmp(png_jmpbuf(png))) {
            png_destroy_write_struct(&png, &info);
            throw std::runtime_error("Failed to write PNG: " + path);
        }

        png_init_io(png, file.get());
        png_set_IHDR(png,
                     info,
                     width,
                     height,
                     8,
                     PNG_COLOR_TYPE_RGBA,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_DEFAULT,
                     PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png, info);

        std::vector<png_bytep> rows(height);
        const auto rowBytes = static_cast<size_t>(width) * 4;
        for (uint32_t row = 0; row < height; ++row) {
            rows[row] =
                (unsigned char *)(rgba + (static_cast<size_t>(row) * rowBytes));
        }

        png_write_image(png, rows.data());
        png_write_end(png, nullptr);
        png_destroy_write_struct(&png, &info);
    }

    inline void writeTextureReadbackPng(
        const std::string &path,
        const Core::Renderer::TextureReadbackResult &readback) {
        if (readback.format !=
                Core::Renderer::Renderer2DTargetFormat::RGBA8Unorm &&
            readback.format !=
                Core::Renderer::Renderer2DTargetFormat::BGRA8Unorm) {
            throw std::runtime_error(
                "saveToFile currently supports only 8-bit RGBA/BGRA "
                "textures");
        }
        if (readback.bytesPerPixel != 4 || readback.empty()) {
            throw std::runtime_error(
                "Texture readback cannot be written as PNG");
        }

        std::vector<uint8_t> rgba(readback.pixels.size());
        if (readback.format ==
            Core::Renderer::Renderer2DTargetFormat::BGRA8Unorm) {
            const size_t pixelCount = static_cast<size_t>(readback.width) *
                                      static_cast<size_t>(readback.height);
            for (size_t i = 0; i < pixelCount; ++i) {
                const size_t offset = i * readback.bytesPerPixel;
                rgba[offset + 0] = readback.pixels[offset + 2];
                rgba[offset + 1] = readback.pixels[offset + 1];
                rgba[offset + 2] = readback.pixels[offset + 0];
                rgba[offset + 3] = readback.pixels[offset + 3];
            }
        } else {
            rgba = readback.pixels;
        }

        writePng(path, rgba.data(), readback.width, readback.height);
    }

    struct QueuedPickingReadback {
        TextureResource resource;
        Core::Renderer::TextureReadbackRegion region;
        uint64_t sequence = 0;
        bool queued = false;
    };

    struct AsyncPickingReadbackSlot {
        enum class State : uint8_t {
            Idle,
            CopyRecorded,
            Mapping,
            Ready,
            Failed,
        };

        State state = State::Idle;
        wgpu::Buffer buffer;
        uint64_t bufferSize = 0;
        uint64_t mappedSize = 0;
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t paddedBytesPerRow = 0;
        uint32_t unpaddedBytesPerRow = 0;
        uint64_t sequence = 0;
        wgpu::MapAsyncStatus mapStatus = wgpu::MapAsyncStatus::Error;
        std::string mapError;

        [[nodiscard]] bool isReusable() const noexcept {
            return state == State::Idle || state == State::Failed;
        }
    };

} // namespace Bess::Wgpu::Renderer2DDetail
