#include "bess_wgpu/wgpu_renderer_2d.h"
#include "bess_core/renderer/font.h"
#include "bess_core/renderer/subtexture.h"
#include "bess_wgpu/piplines/path_pipeline.h"
#include "bess_wgpu/piplines/primitive_pipeline.h"
#include "bess_wgpu/wgpu_shader.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "glfw3webgpu.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <png.h>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Bess::Wgpu {
    using Core::Renderer::FontFile;
    using Core::Renderer::FontProps;
    using Core::Renderer::Glyph;
    using Core::Renderer::Path2D;
    using Core::Renderer::PathCommand;
    using Core::Renderer::PathCommandKind;
    using Core::Renderer::PathCommandStroke;
    using Core::Renderer::PathFillRule;
    using Core::Renderer::PathLineCap;
    using Core::Renderer::PathLineJoin;
    using Core::Renderer::PathProps;

    namespace {
        using Bess::Core::Renderer::Color;
        using Bess::Core::Renderer::QuadRenderPass;
        using Bess::Core::Renderer::Renderer2DExtent;
        using Bess::Core::Renderer::Renderer2DTargetFormat;
        using Bess::Core::Renderer::SubTexture;
        using Bess::Core::Renderer::TextureOrigin;
        using Bess::Wgpu::Piplines::PathCoverVertex;
        using Bess::Wgpu::Piplines::PathStencilVertex;
        using Bess::Wgpu::Piplines::PrimitiveInstance;

        constexpr wgpu::TextureFormat kDepthStencilFormat =
            wgpu::TextureFormat::Depth24PlusStencil8;
        constexpr uint32_t kPathCurveTypeLine = 0;
        constexpr uint32_t kPathCurveTypeQuadratic = 1;
        constexpr const char *kDefaultFontFile =
            "assets/fonts/Roboto/Roboto-Regular.ttf";
        constexpr const char *kDefaultMsdfFontDirectory =
            "assets/fonts/Roboto/msdf-Roboto-Regular-32";
        constexpr const char *kDefaultMsdfFontName = "Roboto-Regular";
        constexpr float kFontOutlinePixelSize = 64.f;
        constexpr uint32_t kReplacementCodepoint = 0xFFFD;

        bool isTransparent(const Core::Renderer::QuadProps &props) {
            if (props.renderPass == QuadRenderPass::Opaque) {
                return false;
            }
            if (props.renderPass == QuadRenderPass::Transparent) {
                return true;
            }

            if (props.color.a < 0.999f) {
                return true;
            }
            if (props.texture != 0) {
                return true;
            }
            if (props.borderColor.a < 0.999f) {
                return true;
            }
            return false;
        }

        bool hasPathFill(const PathProps &props) {
            return props.renderFill && props.fillColor.a > 0.f;
        }

        float strokeSizeForCommand(const PathCommand &command,
                                   const PathProps &props) {
            return command.stroke.width > 0.f ? command.stroke.width
                                              : props.strokeSize;
        }

        PickingId pickingIdForCommand(const PathCommand &command,
                                      const PathProps &props) {
            return command.stroke.hasIdOverride() ? command.stroke.id
                                                  : props.id;
        }

        bool pathHasDrawableStroke(std::span<const PathCommand> commands,
                                   const PathProps &props) {
            if (props.strokeColor.a <= 0.f) {
                return false;
            }
            if (props.strokeSize > 0.f) {
                return true;
            }

            return std::any_of(commands.begin(), commands.end(),
                               [](const PathCommand &command) {
                                   return command.stroke.width > 0.f;
                               });
        }

        bool commandNeedsStyledStrokeBaker(const PathCommand &command) {
            return command.kind != PathCommandKind::Move &&
                   command.stroke.isStyled();
        }

        bool pathNeedsStyledStrokeBaker(std::span<const PathCommand> commands,
                                        const PathProps &props) {
            if (props.strokeSize <= 0.f) {
                return true;
            }

            return std::any_of(commands.begin(), commands.end(),
                               commandNeedsStyledStrokeBaker);
        }

        bool isFillTransparent(const PathProps &props) {
            if (props.renderPass == QuadRenderPass::Opaque) {
                return false;
            }
            if (props.renderPass == QuadRenderPass::Transparent) {
                return true;
            }
            return props.fillColor.a < 0.999f;
        }

        bool isStrokeTransparent(const PathProps &props) {
            if (props.renderPass == QuadRenderPass::Opaque) {
                return false;
            }
            if (props.renderPass == QuadRenderPass::Transparent) {
                return true;
            }
            return props.strokeColor.a < 0.999f;
        }

        bool isUtf8ContinuationByte(char c) {
            const auto byte = static_cast<unsigned char>(c);
            return (byte & 0xC0) == 0x80;
        }

        uint32_t decodeUtf8(std::string_view text, size_t &offset) {
            if (offset >= text.size()) {
                return 0;
            }

            const size_t start = offset;
            const auto first = static_cast<unsigned char>(text[start]);
            if (first <= 0x7F) {
                offset = start + 1;
                return first;
            }

            uint32_t codepoint = 0;
            size_t length = 0;
            uint32_t minCodepoint = 0;
            if ((first & 0xE0) == 0xC0) {
                codepoint = first & 0x1F;
                length = 2;
                minCodepoint = 0x80;
            } else if ((first & 0xF0) == 0xE0) {
                codepoint = first & 0x0F;
                length = 3;
                minCodepoint = 0x800;
            } else if ((first & 0xF8) == 0xF0) {
                codepoint = first & 0x07;
                length = 4;
                minCodepoint = 0x10000;
            } else {
                offset = start + 1;
                return kReplacementCodepoint;
            }

            if (start + length > text.size()) {
                offset = start + 1;
                return kReplacementCodepoint;
            }

            for (size_t i = 1; i < length; ++i) {
                const char byte = text[start + i];
                if (!isUtf8ContinuationByte(byte)) {
                    offset = start + 1;
                    return kReplacementCodepoint;
                }
                codepoint =
                    (codepoint << 6) | (static_cast<uint32_t>(byte) & 0x3F);
            }

            if (codepoint < minCodepoint || codepoint > 0x10FFFF ||
                (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
                offset = start + 1;
                return kReplacementCodepoint;
            }

            offset = start + length;
            return codepoint;
        }

        PathCommand transformTextCommand(const PathCommand &command,
                                         const glm::vec2 &origin, float scale) {
            PathCommand transformed = command;
            const auto transformPoint = [&](const glm::vec2 &point) {
                return origin + (point * scale);
            };

            switch (command.kind) {
            case PathCommandKind::Move:
            case PathCommandKind::Line:
                transformed.p = transformPoint(command.p);
                break;
            case PathCommandKind::Quad:
                transformed.p = transformPoint(command.p);
                transformed.control = transformPoint(command.control);
                break;
            case PathCommandKind::Cubic:
                transformed.p = transformPoint(command.p);
                transformed.control = transformPoint(command.control);
                transformed.control2 = transformPoint(command.control2);
                break;
            case PathCommandKind::Close:
                break;
            }
            return transformed;
        }

        struct DrawRun {
            Core::Renderer::TextureHandle texture = 0;
            uint32_t firstInstance = 0;
            uint32_t instanceCount = 0;
            float zIndex = 0.f;
        };

        struct CustomQuadDrawRun {
            CustomQuadShaderHandle shader = 0;
            uint32_t firstInstance = 0;
            uint32_t instanceCount = 0;
            float zIndex = 0.f;
        };

        struct TextDrawRun {
            uint32_t firstGlyph = 0;
            uint32_t glyphCount = 0;
            float zIndex = 0.f;
        };

        enum class TransparentDrawKind : uint8_t {
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
            uint32_t order = 0;
        };

        wgpu::TextureFormat toWgpuFormat(Renderer2DTargetFormat format) {
            switch (format) {
            case Renderer2DTargetFormat::RGBA8Unorm:
                return wgpu::TextureFormat::RGBA8Unorm;
            case Renderer2DTargetFormat::RGBA16Float:
                return wgpu::TextureFormat::RGBA16Float;
            case Renderer2DTargetFormat::RG32Uint:
                return wgpu::TextureFormat::RG32Uint;
            case Renderer2DTargetFormat::None:
                return wgpu::TextureFormat::Undefined;
            case Renderer2DTargetFormat::BGRA8Unorm:
            default:
                return wgpu::TextureFormat::BGRA8Unorm;
            }
        }

        Renderer2DTargetFormat toRendererFormat(wgpu::TextureFormat format) {
            switch (format) {
            case wgpu::TextureFormat::RGBA8Unorm:
                return Renderer2DTargetFormat::RGBA8Unorm;
            case wgpu::TextureFormat::BGRA8Unorm:
                return Renderer2DTargetFormat::BGRA8Unorm;
            case wgpu::TextureFormat::RGBA16Float:
                return Renderer2DTargetFormat::RGBA16Float;
            case wgpu::TextureFormat::RG32Uint:
                return Renderer2DTargetFormat::RG32Uint;
            default:
                return Renderer2DTargetFormat::None;
            }
        }

        uint32_t bytesPerPixelForFormat(wgpu::TextureFormat format) {
            switch (format) {
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::BGRA8Unorm:
                return 4;
            case wgpu::TextureFormat::RGBA16Float:
            case wgpu::TextureFormat::RG32Uint:
                return 8;
            default:
                throw std::runtime_error(
                    "Unsupported texture format for readback");
            }
        }

        bool canUseAsPrimitiveSampledTexture(wgpu::TextureFormat format) {
            switch (format) {
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::BGRA8Unorm:
            case wgpu::TextureFormat::RGBA16Float:
                return true;
            default:
                return false;
            }
        }

        wgpu::Color toWgpuColor(const Color &color) {
            return {color.r, color.g, color.b, color.a};
        }

        uint32_t alignTo(uint32_t value, uint32_t alignment) {
            return ((value + alignment - 1) / alignment) * alignment;
        }

        Core::Renderer::TextureReadbackResult
        readTextureRegion(const wgpu::Instance &instance,
                          const wgpu::Device &device, const wgpu::Queue &queue,
                          const wgpu::Texture &texture,
                          wgpu::TextureFormat format, uint32_t textureWidth,
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
            const uint32_t paddedBytesPerRow =
                alignTo(unpaddedBytesPerRow, 256);
            const auto readbackSize =
                static_cast<uint64_t>(paddedBytesPerRow) * region.height;

            wgpu::BufferDescriptor bufferDescriptor{};
            bufferDescriptor.size = readbackSize;
            bufferDescriptor.usage =
                wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
            wgpu::Buffer readbackBuffer =
                device.CreateBuffer(&bufferDescriptor);

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
            auto mapCallback = [&mapStatus,
                                &mapError](wgpu::MapAsyncStatus status,
                                           wgpu::StringView message) {
                mapStatus = status;
                if (status != wgpu::MapAsyncStatus::Success &&
                    message.data != nullptr) {
                    mapError.assign(message.data, message.length);
                }
            };

            wgpu::Future mapFuture = readbackBuffer.MapAsync(
                wgpu::MapMode::Read, 0, readbackSize,
                wgpu::CallbackMode::WaitAnyOnly, mapCallback);
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

        void writePng(const std::string &path, const uint8_t *rgba,
                      uint32_t width, uint32_t height) {
            using FilePtr = std::unique_ptr<FILE, FileDeleter>;
            FilePtr file(std::fopen(path.c_str(), "wb"));
            if (!file) {
                throw std::runtime_error("Failed to open PNG for writing: " +
                                         path);
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
            png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGBA,
                         PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                         PNG_FILTER_TYPE_DEFAULT);
            png_write_info(png, info);

            std::vector<png_bytep> rows(height);
            const auto rowBytes = static_cast<size_t>(width) * 4;
            for (uint32_t row = 0; row < height; ++row) {
                rows[row] = (unsigned char *)(rgba + (static_cast<size_t>(row) *
                                                      rowBytes));
            }

            png_write_image(png, rows.data());
            png_write_end(png, nullptr);
            png_destroy_write_struct(&png, &info);
        }

        void writeTextureReadbackPng(
            const std::string &path,
            const Core::Renderer::TextureReadbackResult &readback) {
            if (readback.format != Renderer2DTargetFormat::RGBA8Unorm &&
                readback.format != Renderer2DTargetFormat::BGRA8Unorm) {
                throw std::runtime_error(
                    "saveToFile currently supports only 8-bit RGBA/BGRA "
                    "textures");
            }
            if (readback.bytesPerPixel != 4 || readback.empty()) {
                throw std::runtime_error(
                    "Texture readback cannot be written as PNG");
            }

            std::vector<uint8_t> rgba(readback.pixels.size());
            if (readback.format == Renderer2DTargetFormat::BGRA8Unorm) {
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

        void
        makePrimitiveInstanceInPlace(PrimitiveInstance &instance,
                                     const Core::Renderer::QuadProps &props) {
            instance.position[0] = props.position.x;
            instance.position[1] = props.position.y;
            instance.position[2] = props.zIndex;
            instance.padding0 = 0.f;
            instance.color[0] = props.color.r;
            instance.color[1] = props.color.g;
            instance.color[2] = props.color.b;
            instance.color[3] = props.color.a;
            instance.texData[0] = props.uvRect.x;
            instance.texData[1] = props.uvRect.y;
            instance.texData[2] = props.uvRect.z;
            instance.texData[3] = props.uvRect.w;
            instance.size[0] = props.size.x;
            instance.size[1] = props.size.y;
            instance.id[0] = props.id.runtimeId;
            instance.id[1] = props.id.info;
            instance.primitiveType = 0; // Quad
            instance.isMica = 0;
            instance.texSlotIdx = props.texture == 0 ? 0 : 1;
            instance.angle = props.rotation;
            instance.primitiveData[0] = 0.f;
            instance.primitiveData[1] = 0.f;
            instance.primitiveData[2] = 0.f;
            instance.primitiveData[3] = 0.f;

            instance.borderRadius[0] = props.radius.x;
            instance.borderRadius[1] = props.radius.y;
            instance.borderRadius[2] = props.radius.z;
            instance.borderRadius[3] = props.radius.w;
            instance.borderSize[0] = props.thickness.x;
            instance.borderSize[1] = props.thickness.y;
            instance.borderSize[2] = props.thickness.z;
            instance.borderSize[3] = props.thickness.w;
            instance.borderColor[0] = props.borderColor.r;
            instance.borderColor[1] = props.borderColor.g;
            instance.borderColor[2] = props.borderColor.b;
            instance.borderColor[3] = props.borderColor.a;
        }

        struct CustomQuadInstance {
            float position[3] = {0.f, 0.f, 0.f};
            float rotation = 0.f;
            float size[2] = {1.f, 1.f};
            float padding0[2] = {0.f, 0.f};
            float color[4] = {1.f, 1.f, 1.f, 1.f};
            float uvRect[4] = {0.f, 0.f, 1.f, 1.f};
            float data0[4] = {0.f, 0.f, 0.f, 0.f};
            float data1[4] = {0.f, 0.f, 0.f, 0.f};
            float data2[4] = {0.f, 0.f, 0.f, 0.f};
            float data3[4] = {0.f, 0.f, 0.f, 0.f};
            uint32_t id[2] = {0, 0};
            uint32_t flags[2] = {0, 0};
        };

        constexpr uint32_t kCustomQuadFlagApplyCameraTransform = 1u << 0u;

        void copyVec4(float *dst, const glm::vec4 &src) {
            dst[0] = src.x;
            dst[1] = src.y;
            dst[2] = src.z;
            dst[3] = src.w;
        }

        void makeCustomQuadInstanceInPlace(CustomQuadInstance &instance,
                                           const CustomQuadProps &props) {
            const auto &quad = props.quad;
            instance.position[0] = quad.position.x;
            instance.position[1] = quad.position.y;
            instance.position[2] = quad.zIndex;
            instance.rotation = quad.rotation;
            instance.size[0] = quad.size.x;
            instance.size[1] = quad.size.y;
            instance.padding0[0] = 0.f;
            instance.padding0[1] = 0.f;
            instance.color[0] = quad.color.r;
            instance.color[1] = quad.color.g;
            instance.color[2] = quad.color.b;
            instance.color[3] = quad.color.a;
            instance.uvRect[0] = quad.uvRect.x;
            instance.uvRect[1] = quad.uvRect.y;
            instance.uvRect[2] = quad.uvRect.z;
            instance.uvRect[3] = quad.uvRect.w;
            copyVec4(instance.data0, props.data[0]);
            copyVec4(instance.data1, props.data[1]);
            copyVec4(instance.data2, props.data[2]);
            copyVec4(instance.data3, props.data[3]);
            instance.id[0] = quad.id.runtimeId;
            instance.id[1] = quad.id.info;
            instance.flags[0] =
                props.transformMode ==
                        Core::Renderer::CustomQuadTransformMode::Camera
                    ? kCustomQuadFlagApplyCameraTransform
                    : 0u;
            instance.flags[1] = 0;
        }

        void
        makeCircleInstanceInPlace(PrimitiveInstance &instance,
                                  const Core::Renderer::CircleProps &props) {
            instance.position[0] = props.position.x;
            instance.position[1] = props.position.y;
            instance.position[2] = props.zIndex;
            instance.padding0 = 0.f;
            instance.color[0] = props.color.r;
            instance.color[1] = props.color.g;
            instance.color[2] = props.color.b;
            instance.color[3] = props.color.a;
            instance.texData[0] = 0.f;
            instance.texData[1] = 0.f;
            instance.texData[2] = 1.f;
            instance.texData[3] = 1.f;
            instance.size[0] = props.radius * 2.f;
            instance.size[1] = props.radius * 2.f;
            instance.id[0] = props.id.runtimeId;
            instance.id[1] = props.id.info;
            instance.primitiveType = 1; // Circle
            instance.isMica = 0;
            instance.texSlotIdx = 0;
            instance.angle = 0.f;
            instance.primitiveData[0] = props.radius;
            instance.primitiveData[1] =
                props.thickness > 0.f ? props.radius - props.thickness : 0.f;
            instance.primitiveData[2] = 0.f;
            instance.primitiveData[3] = 0.f;

            instance.borderRadius[0] = 0.f;
            instance.borderRadius[1] = 0.f;
            instance.borderRadius[2] = 0.f;
            instance.borderRadius[3] = 0.f;
            instance.borderSize[0] = 0.f;
            instance.borderSize[1] = 0.f;
            instance.borderSize[2] = 0.f;
            instance.borderSize[3] = 0.f;
            instance.borderColor[0] = 0.f;
            instance.borderColor[1] = 0.f;
            instance.borderColor[2] = 0.f;
            instance.borderColor[3] = 0.f;
        }

        void makeLineInstanceInPlace(PrimitiveInstance &instance,
                                     const Core::Renderer::LineProps &props) {
            glm::vec2 diff = props.p1 - props.p0;
            float length = glm::length(diff);
            float angle = std::atan2(diff.y, diff.x);
            glm::vec2 pos = (props.p0 + props.p1) * 0.5f;
            constexpr float aaPadding = 2.f;
            const float thickness = std::max(props.thickness, 1.f);

            instance.position[0] = pos.x;
            instance.position[1] = pos.y;
            instance.position[2] = props.zIndex;
            instance.padding0 = 0.f;
            instance.color[0] = props.color.r;
            instance.color[1] = props.color.g;
            instance.color[2] = props.color.b;
            instance.color[3] = props.color.a;
            instance.texData[0] = 0.f;
            instance.texData[1] = 0.f;
            instance.texData[2] = 1.f;
            instance.texData[3] = 1.f;
            instance.size[0] = length + (aaPadding * 2.f);
            instance.size[1] = thickness + (aaPadding * 2.f);
            instance.id[0] = props.id.runtimeId;
            instance.id[1] = props.id.info;
            instance.primitiveType = 2; // Line
            instance.isMica = 0;
            instance.texSlotIdx = 0;
            instance.angle = angle;
            instance.primitiveData[0] = length;
            instance.primitiveData[1] = thickness;
            instance.primitiveData[2] = aaPadding;
            instance.primitiveData[3] = 0.f;

            instance.borderRadius[0] = 0.f;
            instance.borderRadius[1] = 0.f;
            instance.borderRadius[2] = 0.f;
            instance.borderRadius[3] = 0.f;
            instance.borderSize[0] = 0.f;
            instance.borderSize[1] = 0.f;
            instance.borderSize[2] = 0.f;
            instance.borderSize[3] = 0.f;
            instance.borderColor[0] = 0.f;
            instance.borderColor[1] = 0.f;
            instance.borderColor[2] = 0.f;
            instance.borderColor[3] = 0.f;
        }

        class PrimitiveBatch {
          public:
            void configure(uint32_t initialCapacity, uint32_t maxCapacity) {
                m_maxCapacity = std::max(1u, maxCapacity);
                // Pre-allocate to max capacity to avoid reallocations and debug
                // bounds checking overhead
                m_gpuInstances.resize(m_maxCapacity);
                m_drawRuns.resize(m_maxCapacity);
                m_gpuInstancesPtr = m_gpuInstances.data();
                m_drawRunsPtr = m_drawRuns.data();
                m_instanceCount = 0;
                m_drawRunsCount = 0;
            }

            void clear() {
                m_instanceCount = 0;
                m_drawRunsCount = 0;
            }

            PrimitiveInstance &push(Core::Renderer::TextureHandle texture) {
                if (m_instanceCount >= m_maxCapacity) {
                    throw std::runtime_error(
                        "WGPU quad batch capacity exceeded");
                }
                const uint32_t instanceIndex = m_instanceCount;
                if (m_drawRunsCount == 0 ||
                    m_drawRunsPtr[m_drawRunsCount - 1].texture != texture) {
                    m_drawRunsPtr[m_drawRunsCount++] = {.texture = texture,
                                                        .firstInstance =
                                                            instanceIndex,
                                                        .instanceCount = 1};
                } else {
                    m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
                }
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
                    }
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
                            if (m_gpuInstancesPtr[a].position[2] !=
                                m_gpuInstancesPtr[b].position[2]) {
                                return m_gpuInstancesPtr[a].position[2] <
                                       m_gpuInstancesPtr[b].position[2];
                            }
                            return a < b;
                        });

                    m_sortTextures.resize(m_instanceCount);
                    Core::Renderer::TextureHandle *texPtr =
                        m_sortTextures.data();
                    for (uint32_t r = 0; r < m_drawRunsCount; ++r) {
                        const auto &run = m_drawRunsPtr[r];
                        for (uint32_t i = 0; i < run.instanceCount; ++i) {
                            texPtr[run.firstInstance + i] = run.texture;
                        }
                    }

                    m_sortInstances.resize(m_instanceCount);
                    PrimitiveInstance *sortedPtr = m_sortInstances.data();
                    m_drawRunsCount = 0;

                    for (uint32_t i = 0; i < m_instanceCount; ++i) {
                        uint32_t oldIdx = indicesPtr[i];
                        sortedPtr[i] = m_gpuInstancesPtr[oldIdx];
                        Core::Renderer::TextureHandle tex = texPtr[oldIdx];
                        const float zIndex = sortedPtr[i].position[2];

                        if (m_drawRunsCount == 0 ||
                            m_drawRunsPtr[m_drawRunsCount - 1].texture != tex ||
                            m_drawRunsPtr[m_drawRunsCount - 1].zIndex !=
                                zIndex) {
                            m_drawRunsPtr[m_drawRunsCount++] = {
                                .texture = tex,
                                .firstInstance = i,
                                .instanceCount = 1,
                                .zIndex = zIndex};
                        } else {
                            m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
                        }
                    }
                    // Copy back to main buffer
                    for (uint32_t i = 0; i < m_instanceCount; ++i) {
                        m_gpuInstancesPtr[i] = sortedPtr[i];
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
                       sizeof(PrimitiveInstance);
            }

            [[nodiscard]] const PrimitiveInstance *data() const noexcept {
                return m_gpuInstancesPtr;
            }

            [[nodiscard]] const DrawRun *drawRunsData() const noexcept {
                return m_drawRunsPtr;
            }

            [[nodiscard]] uint32_t drawRunsCount() const noexcept {
                return m_drawRunsCount;
            }

          private:
            std::vector<PrimitiveInstance> m_gpuInstances;
            std::vector<DrawRun> m_drawRuns;
            std::vector<uint32_t> m_sortIndices;
            std::vector<Core::Renderer::TextureHandle> m_sortTextures;
            std::vector<PrimitiveInstance> m_sortInstances;
            PrimitiveInstance *m_gpuInstancesPtr = nullptr;
            DrawRun *m_drawRunsPtr = nullptr;
            uint32_t m_instanceCount = 0;
            uint32_t m_drawRunsCount = 0;
            uint32_t m_maxCapacity = 1;
        };

        class CustomQuadBatch {
          public:
            void configure(uint32_t initialCapacity, uint32_t maxCapacity) {
                m_maxCapacity = std::max(1u, maxCapacity);
                m_gpuInstances.resize(m_maxCapacity);
                m_drawRuns.resize(m_maxCapacity);
                m_gpuInstancesPtr = m_gpuInstances.data();
                m_drawRunsPtr = m_drawRuns.data();
                m_instanceCount = 0;
                m_drawRunsCount = 0;
            }

            void clear() {
                m_instanceCount = 0;
                m_drawRunsCount = 0;
            }

            CustomQuadInstance &push(CustomQuadShaderHandle shader) {
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
                    m_drawRunsPtr[m_drawRunsCount++] = {.shader = shader,
                                                        .firstInstance =
                                                            instanceIndex,
                                                        .instanceCount = 1};
                } else {
                    m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
                }
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
                    }
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
                            if (m_gpuInstancesPtr[a].position[2] !=
                                m_gpuInstancesPtr[b].position[2]) {
                                return m_gpuInstancesPtr[a].position[2] <
                                       m_gpuInstancesPtr[b].position[2];
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
                    m_drawRunsCount = 0;

                    for (uint32_t i = 0; i < m_instanceCount; ++i) {
                        uint32_t oldIdx = indicesPtr[i];
                        sortedPtr[i] = m_gpuInstancesPtr[oldIdx];
                        CustomQuadShaderHandle shader = shaderPtr[oldIdx];
                        const float zIndex = sortedPtr[i].position[2];

                        if (m_drawRunsCount == 0 ||
                            m_drawRunsPtr[m_drawRunsCount - 1].shader !=
                                shader ||
                            m_drawRunsPtr[m_drawRunsCount - 1].zIndex !=
                                zIndex) {
                            m_drawRunsPtr[m_drawRunsCount++] = {
                                .shader = shader,
                                .firstInstance = i,
                                .instanceCount = 1,
                                .zIndex = zIndex};
                        } else {
                            m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
                        }
                    }
                    for (uint32_t i = 0; i < m_instanceCount; ++i) {
                        m_gpuInstancesPtr[i] = sortedPtr[i];
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
                       sizeof(CustomQuadInstance);
            }

            [[nodiscard]] const CustomQuadInstance *data() const noexcept {
                return m_gpuInstancesPtr;
            }

            [[nodiscard]] const CustomQuadDrawRun *
            drawRunsData() const noexcept {
                return m_drawRunsPtr;
            }

            [[nodiscard]] uint32_t drawRunsCount() const noexcept {
                return m_drawRunsCount;
            }

          private:
            std::vector<CustomQuadInstance> m_gpuInstances;
            std::vector<CustomQuadDrawRun> m_drawRuns;
            std::vector<uint32_t> m_sortIndices;
            std::vector<CustomQuadShaderHandle> m_sortShaders;
            std::vector<CustomQuadInstance> m_sortInstances;
            CustomQuadInstance *m_gpuInstancesPtr = nullptr;
            CustomQuadDrawRun *m_drawRunsPtr = nullptr;
            uint32_t m_instanceCount = 0;
            uint32_t m_drawRunsCount = 0;
            uint32_t m_maxCapacity = 1;
        };

        bool isWGSLIdentifier(std::string_view value) {
            if (value.empty()) {
                return false;
            }

            const auto isAlphaOrUnderscore = [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       c == '_';
            };
            const auto isAlphaNumericOrUnderscore = [&](char c) {
                return isAlphaOrUnderscore(c) || (c >= '0' && c <= '9');
            };

            if (!isAlphaOrUnderscore(value.front())) {
                return false;
            }
            return std::all_of(value.begin() + 1, value.end(),
                               isAlphaNumericOrUnderscore);
        }

        std::string
        buildCustomQuadShaderSource(const CustomQuadShaderDesc &desc) {
            if (desc.fragmentSource.empty()) {
                throw std::runtime_error(
                    "Custom quad shader fragment source is empty");
            }
            if (!isWGSLIdentifier(desc.fragmentEntryPoint)) {
                throw std::runtime_error(
                    "Custom quad shader fragment entry point is not a valid "
                    "WGSL identifier");
            }

            constexpr const char *kCustomQuadShaderPrelude = R"(
struct Frame {
    viewport: vec2f,
    padding: vec2f,
    camera_transform: mat4x4f,
};

struct CustomQuad {
    position: vec3f,
    rotation: f32,
    size: vec2f,
    padding0: vec2f,
    color: vec4f,
    uv_rect: vec4f,
    data0: vec4f,
    data1: vec4f,
    data2: vec4f,
    data3: vec4f,
    id: vec2u,
    flags: vec2u,
};

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) local_uv: vec2f,
    @location(2) local_pos: vec2f,
    @location(3) size: vec2f,
    @location(4) color: vec4f,
    @location(5) data0: vec4f,
    @location(6) data1: vec4f,
    @location(7) data2: vec4f,
    @location(8) data3: vec4f,
    @location(9) @interpolate(flat) id: vec2u,
};

struct CustomQuadFragmentInput {
    frag_coord: vec4f,
    uv: vec2f,
    local_uv: vec2f,
    local_pos: vec2f,
    size: vec2f,
    color: vec4f,
    data0: vec4f,
    data1: vec4f,
    data2: vec4f,
    data3: vec4f,
    viewport: vec2f,
    camera_transform: mat4x4f,
    camera_zoom: f32,
    camera_zoom_xy: vec2f,
};

struct FragmentOut {
    @location(0) color: vec4f,
};

struct FragmentOutPicking {
    @location(0) color: vec4f,
    @location(1) id: vec2u,
};

@group(0) @binding(0) var<storage, read> custom_quads: array<CustomQuad>;
@group(0) @binding(1) var<uniform> frame: Frame;

const CUSTOM_QUAD_FLAG_APPLY_CAMERA_TRANSFORM: u32 = 1u;

fn custom_quad_resolution() -> vec3f {
    return vec3f(frame.viewport, 1.0);
}

fn custom_quad_camera_zoom_xy() -> vec2f {
    return vec2f(
        abs(frame.camera_transform[0][0]) * frame.viewport.x * 0.5,
        abs(frame.camera_transform[1][1]) * frame.viewport.y * 0.5);
}

fn custom_quad_camera_zoom() -> f32 {
    let zoom_xy = custom_quad_camera_zoom_xy();
    return (zoom_xy.x + zoom_xy.y) * 0.5;
}

fn custom_quad_depth(z_index: f32) -> f32 {
    return clamp(0.5 - 0.5 * tanh(z_index * 0.01), 0.0, 1.0);
}

fn custom_quad_screen_clip_position(world: vec2f, z_index: f32) -> vec4f {
    let safe_viewport = max(frame.viewport, vec2f(1.0, 1.0));
    let clip_xy = vec2f(
        (world.x / safe_viewport.x) * 2.0,
        -(world.y / safe_viewport.y) * 2.0);
    return vec4f(clip_xy, custom_quad_depth(z_index), 1.0);
}

fn custom_quad_camera_clip_position(world: vec2f, z_index: f32) -> vec4f {
    var clip = frame.camera_transform * vec4f(world, 0.0, 1.0);
    clip.z = custom_quad_depth(z_index);
    return clip;
}

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32,
           @builtin(instance_index) instance_index: u32) -> VertexOut {
    let corners = array<vec2f, 6>(
        vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
        vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
    let local = corners[vertex_index];
    let q = custom_quads[instance_index];
    let local_coord = local - vec2f(0.5, 0.5);
    let centered = local_coord * q.size;

    let s = sin(q.rotation);
    let c = cos(q.rotation);
    let rotated = vec2f(
        centered.x * c - centered.y * s,
        centered.x * s + centered.y * c);
    let world = q.position.xy + rotated;

    var out: VertexOut;
    if ((q.flags.x & CUSTOM_QUAD_FLAG_APPLY_CAMERA_TRANSFORM) != 0u) {
        out.position = custom_quad_camera_clip_position(world, q.position.z);
    } else {
        out.position = custom_quad_screen_clip_position(world, q.position.z);
    }
    out.uv = q.uv_rect.xy + local * (q.uv_rect.zw - q.uv_rect.xy);
    out.local_uv = local;
    out.local_pos = centered;
    out.size = q.size;
    out.color = q.color;
    out.data0 = q.data0;
    out.data1 = q.data1;
    out.data2 = q.data2;
    out.data3 = q.data3;
    out.id = q.id;
    return out;
}

fn make_custom_quad_fragment_input(in: VertexOut) -> CustomQuadFragmentInput {
    var out: CustomQuadFragmentInput;
    out.frag_coord = in.position;
    out.uv = in.uv;
    out.local_uv = in.local_uv;
    out.local_pos = in.local_pos;
    out.size = in.size;
    out.color = in.color;
    out.data0 = in.data0;
    out.data1 = in.data1;
    out.data2 = in.data2;
    out.data3 = in.data3;
    out.viewport = frame.viewport;
    out.camera_transform = frame.camera_transform;
    out.camera_zoom = custom_quad_camera_zoom();
    out.camera_zoom_xy = custom_quad_camera_zoom_xy();
    return out;
}
)";

            std::string source = kCustomQuadShaderPrelude;
            source += "\n";
            source += desc.fragmentSource;
            source += R"(

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
    var out: FragmentOut;
    out.color = )";
            source += desc.fragmentEntryPoint;
            source += R"((make_custom_quad_fragment_input(in));
    return out;
}

@fragment
fn fs_main_picking(in: VertexOut) -> FragmentOutPicking {
    var out: FragmentOutPicking;
    out.color = )";
            source += desc.fragmentEntryPoint;
            source += R"((make_custom_quad_fragment_input(in));
    out.id = in.id;
    return out;
}
)";
            return source;
        }

        class CustomQuadPipeline {
          public:
            void init(const wgpu::Device &device,
                      wgpu::TextureFormat targetFormat,
                      const wgpu::Buffer &frameBuffer, uint64_t frameBufferSize,
                      wgpu::TextureFormat pickingFormat) {
                m_device = device;
                m_targetFormat = targetFormat;
                m_pickingFormat = pickingFormat;
                m_frameBuffer = frameBuffer;
                m_frameBufferSize = frameBufferSize;
                createBindGroupLayout();
                createPipelineLayout();
            }

            void destroy() {
                m_shaders.clear();
                m_bindGroup = nullptr;
                m_instanceBuffer = nullptr;
                m_instanceBufferSize = 0;
                m_pipelineLayout = nullptr;
                m_bindGroupLayout = nullptr;
                m_frameBuffer = nullptr;
                m_frameBufferSize = 0;
                m_device = nullptr;
                m_nextShaderHandle = 1;
            }

            [[nodiscard]] bool ensureInstanceBufferSize(std::size_t quadCount) {
                const auto requiredSize = std::max<std::size_t>(
                    sizeof(CustomQuadInstance),
                    quadCount * sizeof(CustomQuadInstance));
                if (m_instanceBuffer != nullptr &&
                    m_instanceBufferSize >= requiredSize) {
                    return false;
                }

                wgpu::BufferDescriptor descriptor{};
                descriptor.size = requiredSize;
                descriptor.usage =
                    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
                m_instanceBuffer = m_device.CreateBuffer(&descriptor);
                m_instanceBufferSize = requiredSize;
                createBindGroup();
                return true;
            }

            void uploadInstances(const wgpu::Queue &queue,
                                 const CustomQuadInstance *instances,
                                 uint64_t byteSize,
                                 uint64_t bufferOffset = 0) const {
                if (m_instanceBuffer == nullptr || instances == nullptr ||
                    byteSize == 0) {
                    return;
                }
                queue.WriteBuffer(m_instanceBuffer, bufferOffset, instances,
                                  byteSize);
            }

            [[nodiscard]] CustomQuadShaderHandle
            createShader(const CustomQuadShaderDesc &desc) {
                if (m_device == nullptr) {
                    throw std::runtime_error(
                        "Custom quad shaders require an initialized WGPU "
                        "device");
                }

                const CustomQuadShaderHandle handle = m_nextShaderHandle++;
                if (m_nextShaderHandle == 0) {
                    m_nextShaderHandle = 1;
                }

                ShaderResource resource;
                resource.label = desc.label.empty() ? "custom_quad_shader_" +
                                                          std::to_string(handle)
                                                    : desc.label;

                const std::string source = buildCustomQuadShaderSource(desc);
                using Core::Renderer::ShaderLanguage;
                using Core::Renderer::ShaderModuleDesc;
                using Core::Renderer::ShaderStage;
                resource.shader = std::make_unique<WgpuShader>(
                    resource.label,
                    std::vector<ShaderModuleDesc>{
                        {.language = ShaderLanguage::WGSL,
                         .stage = ShaderStage::Vertex,
                         .entryPoint = "vs_main",
                         .source = source},
                        {.language = ShaderLanguage::WGSL,
                         .stage = ShaderStage::Fragment,
                         .entryPoint = "fs_main",
                         .source = source},
                    },
                    m_device);
                createPipelineState(resource);
                m_shaders.emplace(handle, std::move(resource));
                return handle;
            }

            void destroyShader(CustomQuadShaderHandle shader) {
                m_shaders.erase(shader);
            }

            [[nodiscard]] bool hasShader(CustomQuadShaderHandle shader) const {
                return shader != 0 && m_shaders.find(shader) != m_shaders.end();
            }

            void draw(wgpu::RenderPassEncoder &renderPass,
                      CustomQuadShaderHandle shader, uint32_t firstInstance,
                      uint32_t instanceCount, bool transparent) const {
                if (instanceCount == 0) {
                    return;
                }

                const auto it = m_shaders.find(shader);
                if (it == m_shaders.end()) {
                    throw std::runtime_error(
                        "Custom quad shader handle is not registered");
                }
                if (m_bindGroup == nullptr) {
                    throw std::runtime_error(
                        "Custom quad pipeline has no bind group");
                }

                renderPass.SetPipeline(transparent
                                           ? it->second.transparentPipeline
                                           : it->second.opaquePipeline);
                renderPass.SetBindGroup(0, m_bindGroup);
                renderPass.Draw(6, instanceCount, 0, firstInstance);
            }

          private:
            struct ShaderResource {
                std::string label;
                std::unique_ptr<WgpuShader> shader;
                wgpu::RenderPipeline opaquePipeline;
                wgpu::RenderPipeline transparentPipeline;
            };

            void createBindGroupLayout() {
                std::array<wgpu::BindGroupLayoutEntry, 2> bindings{};
                bindings[0].binding = 0;
                bindings[0].visibility = wgpu::ShaderStage::Vertex;
                bindings[0].buffer.type =
                    wgpu::BufferBindingType::ReadOnlyStorage;

                bindings[1].binding = 1;
                bindings[1].visibility =
                    wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
                bindings[1].buffer.type = wgpu::BufferBindingType::Uniform;

                wgpu::BindGroupLayoutDescriptor descriptor{};
                descriptor.entryCount = bindings.size();
                descriptor.entries = bindings.data();
                m_bindGroupLayout = m_device.CreateBindGroupLayout(&descriptor);
            }

            void createPipelineLayout() {
                wgpu::PipelineLayoutDescriptor descriptor{};
                descriptor.bindGroupLayoutCount = 1;
                descriptor.bindGroupLayouts = &m_bindGroupLayout;
                m_pipelineLayout = m_device.CreatePipelineLayout(&descriptor);
            }

            void createBindGroup() {
                if (m_instanceBuffer == nullptr || m_frameBuffer == nullptr ||
                    m_bindGroupLayout == nullptr) {
                    m_bindGroup = nullptr;
                    return;
                }

                std::array<wgpu::BindGroupEntry, 2> entries{};
                entries[0].binding = 0;
                entries[0].buffer = m_instanceBuffer;
                entries[0].offset = 0;
                entries[0].size = m_instanceBufferSize;

                entries[1].binding = 1;
                entries[1].buffer = m_frameBuffer;
                entries[1].offset = 0;
                entries[1].size = m_frameBufferSize;

                wgpu::BindGroupDescriptor descriptor{};
                descriptor.layout = m_bindGroupLayout;
                descriptor.entryCount = entries.size();
                descriptor.entries = entries.data();
                m_bindGroup = m_device.CreateBindGroup(&descriptor);
            }

            void createPipelineState(ShaderResource &resource) {
                wgpu::ColorTargetState colorTargets[2]{};
                uint32_t targetCount = 1;

                colorTargets[0].format = m_targetFormat;

                wgpu::BlendState blendState{};
                blendState.color.operation = wgpu::BlendOperation::Add;
                blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
                blendState.color.dstFactor =
                    wgpu::BlendFactor::OneMinusSrcAlpha;
                blendState.alpha.operation = wgpu::BlendOperation::Add;
                blendState.alpha.srcFactor = wgpu::BlendFactor::One;
                blendState.alpha.dstFactor =
                    wgpu::BlendFactor::OneMinusSrcAlpha;
                colorTargets[0].blend = &blendState;

                if (m_pickingFormat != wgpu::TextureFormat::Undefined) {
                    colorTargets[1].format = m_pickingFormat;
                    targetCount = 2;
                }

                wgpu::FragmentState fragment{};
                fragment.module = resource.shader->getModule(
                    Core::Renderer::ShaderStage::Fragment);
                fragment.entryPoint =
                    m_pickingFormat != wgpu::TextureFormat::Undefined
                        ? "fs_main_picking"
                        : "fs_main";
                fragment.targetCount = targetCount;
                fragment.targets = colorTargets;

                wgpu::DepthStencilState depthStencil{};
                depthStencil.format = kDepthStencilFormat;
                depthStencil.depthCompare = wgpu::CompareFunction::LessEqual;
                depthStencil.depthWriteEnabled = true;

                wgpu::RenderPipelineDescriptor opaqueDescriptor{};
                opaqueDescriptor.layout = m_pipelineLayout;
                opaqueDescriptor.vertex.module = resource.shader->getModule(
                    Core::Renderer::ShaderStage::Vertex);
                opaqueDescriptor.vertex.entryPoint = "vs_main";
                opaqueDescriptor.primitive.topology =
                    wgpu::PrimitiveTopology::TriangleList;
                opaqueDescriptor.primitive.cullMode = wgpu::CullMode::None;
                opaqueDescriptor.fragment = &fragment;
                opaqueDescriptor.depthStencil = &depthStencil;

                resource.opaquePipeline =
                    m_device.CreateRenderPipeline(&opaqueDescriptor);
                if (resource.opaquePipeline == nullptr) {
                    throw std::runtime_error(
                        "Failed to create custom quad opaque pipeline");
                }

                depthStencil.depthWriteEnabled = false;
                wgpu::RenderPipelineDescriptor transparentDescriptor =
                    opaqueDescriptor;
                transparentDescriptor.depthStencil = &depthStencil;
                resource.transparentPipeline =
                    m_device.CreateRenderPipeline(&transparentDescriptor);
                if (resource.transparentPipeline == nullptr) {
                    throw std::runtime_error(
                        "Failed to create custom quad transparent pipeline");
                }
            }

            wgpu::Device m_device;
            wgpu::TextureFormat m_targetFormat =
                wgpu::TextureFormat::BGRA8Unorm;
            wgpu::TextureFormat m_pickingFormat =
                wgpu::TextureFormat::Undefined;
            wgpu::Buffer m_frameBuffer;
            uint64_t m_frameBufferSize = 0;
            wgpu::Buffer m_instanceBuffer;
            uint64_t m_instanceBufferSize = 0;
            wgpu::BindGroupLayout m_bindGroupLayout;
            wgpu::PipelineLayout m_pipelineLayout;
            wgpu::BindGroup m_bindGroup;
            std::unordered_map<CustomQuadShaderHandle, ShaderResource>
                m_shaders;
            CustomQuadShaderHandle m_nextShaderHandle = 1;
        };

        struct MsdfGlyph {
            uint32_t codepoint = 0;
            float advance = 0.f;
            glm::vec4 planeBounds{0.f}; // left, bottom, right, top in em units
            SubTexture atlasRegion;
            bool drawable = false;
        };

        class MsdfFontAtlas {
          public:
            bool load(const std::filesystem::path &fontDirectory,
                      const std::string &fontName) {
                const std::filesystem::path jsonPath =
                    fontDirectory / (fontName + ".json");
                const std::filesystem::path pngPath =
                    fontDirectory / (fontName + ".png");

                std::ifstream input(jsonPath);
                if (!input.is_open()) {
                    BESS_WARN("[WgpuRenderer2D] Failed to open MSDF font json: "
                              "{}",
                              jsonPath.string());
                    return false;
                }

                Json::Value root;
                input >> root;
                if (!root.isMember("atlas") || !root.isMember("metrics") ||
                    !root.isMember("glyphs")) {
                    BESS_WARN("[WgpuRenderer2D] Invalid MSDF font json: {}",
                              jsonPath.string());
                    return false;
                }

                const Json::Value &atlas = root["atlas"];
                const Json::Value &metrics = root["metrics"];
                m_atlasSize = {atlas.get("width", 1).asFloat(),
                               atlas.get("height", 1).asFloat()};
                m_fontSize = atlas.get("size", 32.f).asFloat();
                m_pxRange = atlas.get("distanceRange", 4.f).asFloat();
                m_lineHeight = metrics.get("lineHeight", 1.f).asFloat();
                m_ascender = metrics.get("ascender", 1.f).asFloat();
                m_descender = metrics.get("descender", 0.f).asFloat();

                try {
                    m_texture = std::make_shared<WgpuTexture>(pngPath.string());
                    m_texture->init();
                } catch (const std::exception &error) {
                    BESS_WARN("[WgpuRenderer2D] Failed to load MSDF font "
                              "atlas texture {}: {}",
                              pngPath.string(), error.what());
                    m_texture = nullptr;
                    return false;
                }

                m_glyphs.clear();
                const Json::Value &glyphs = root["glyphs"];
                for (const auto &glyphJson : glyphs) {
                    if (!glyphJson.isObject() ||
                        !glyphJson.isMember("unicode")) {
                        continue;
                    }

                    MsdfGlyph glyph;
                    glyph.codepoint = static_cast<uint32_t>(
                        glyphJson.get("unicode", 0).asUInt64());
                    glyph.advance = glyphJson.get("advance", 0.f).asFloat();

                    if (glyphJson.isMember("planeBounds") &&
                        glyphJson.isMember("atlasBounds")) {
                        const Json::Value &plane = glyphJson["planeBounds"];
                        const Json::Value &bounds = glyphJson["atlasBounds"];
                        glyph.planeBounds = {plane.get("left", 0.f).asFloat(),
                                             plane.get("bottom", 0.f).asFloat(),
                                             plane.get("right", 0.f).asFloat(),
                                             plane.get("top", 0.f).asFloat()};

                        const float left = bounds.get("left", 0.f).asFloat();
                        const float bottom =
                            bounds.get("bottom", 0.f).asFloat();
                        const float right = bounds.get("right", 0.f).asFloat();
                        const float top = bounds.get("top", 0.f).asFloat();
                        glyph.atlasRegion.reset(m_atlasSize, {left, bottom},
                                                {std::max(0.f, right - left),
                                                 std::max(0.f, top - bottom)},
                                                TextureOrigin::BottomLeft);
                        glyph.drawable = true;
                    }

                    m_glyphs[glyph.codepoint] = glyph;
                }

                return m_texture != nullptr && m_texture->getHandle() != 0 &&
                       !m_glyphs.empty();
            }

            [[nodiscard]] bool valid() const noexcept {
                return m_texture != nullptr && m_texture->getHandle() != 0 &&
                       !m_glyphs.empty();
            }

            [[nodiscard]] const MsdfGlyph *
            findGlyph(uint32_t codepoint) const noexcept {
                auto it = m_glyphs.find(codepoint);
                if (it != m_glyphs.end()) {
                    return &it->second;
                }

                it = m_glyphs.find('?');
                if (it != m_glyphs.end()) {
                    return &it->second;
                }

                it = m_glyphs.find(' ');
                return it != m_glyphs.end() ? &it->second : nullptr;
            }

            [[nodiscard]] TextureResource textureResource() const {
                return m_texture->getResource();
            }

            [[nodiscard]] float pxRange() const noexcept { return m_pxRange; }
            [[nodiscard]] float lineHeight() const noexcept {
                return m_lineHeight;
            }
            [[nodiscard]] float ascender() const noexcept { return m_ascender; }
            [[nodiscard]] float fontSize() const noexcept { return m_fontSize; }

          private:
            std::shared_ptr<WgpuTexture> m_texture;
            std::unordered_map<uint32_t, MsdfGlyph> m_glyphs;
            glm::vec2 m_atlasSize{1.f};
            float m_fontSize = 32.f;
            float m_pxRange = 4.f;
            float m_lineHeight = 1.f;
            float m_ascender = 1.f;
            float m_descender = 0.f;
        };

        struct MsdfTextInstance {
            float position[3] = {0.f, 0.f, 0.f};
            float pxRange = 4.f;
            float size[2] = {0.f, 0.f};
            float rotation = 0.f;
            float padding0 = 0.f;
            float color[4] = {1.f, 1.f, 1.f, 1.f};
            float uvRect[4] = {0.f, 0.f, 1.f, 1.f};
            uint32_t id[2] = {PickingId::invalidRuntimeId, 0};
            uint32_t flags[2] = {0, 0};
        };

        static_assert(sizeof(MsdfTextInstance) == 80,
                      "MsdfTextInstance must match WGSL layout");

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

        constexpr const char *kMsdfTextShader = R"(
struct Frame {
    viewport: vec2f,
    padding: vec2f,
    camera_transform: mat4x4f,
};

struct TextGlyph {
    position: vec3f,
    px_range: f32,
    size: vec2f,
    rotation: f32,
    padding0: f32,
    color: vec4f,
    uv_rect: vec4f,
    id: vec2u,
    flags: vec2u,
};

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) color: vec4f,
    @location(2) @interpolate(flat) id: vec2u,
    @location(3) px_range: f32,
};

struct FragmentOut {
    @location(0) color: vec4f,
};

struct FragmentOutPicking {
    @location(0) color: vec4f,
    @location(1) id: vec2u,
};

@group(0) @binding(0) var<storage, read> glyphs: array<TextGlyph>;
@group(0) @binding(1) var<uniform> frame: Frame;
@group(0) @binding(2) var font_sampler: sampler;
@group(0) @binding(3) var font_atlas: texture_2d<f32>;

fn text_depth(z_index: f32) -> f32 {
    return clamp(0.5 - 0.5 * tanh(z_index * 0.01), 0.0, 1.0);
}

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32,
           @builtin(instance_index) instance_index: u32) -> VertexOut {
    let corners = array<vec2f, 6>(
        vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
        vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
    let local = corners[vertex_index];
    let glyph = glyphs[instance_index];
    let centered = (local - vec2f(0.5, 0.5)) * glyph.size;

    let s = sin(glyph.rotation);
    let c = cos(glyph.rotation);
    let rotated = vec2f(
        centered.x * c - centered.y * s,
        centered.x * s + centered.y * c);
    let world = glyph.position.xy + rotated;

    var out: VertexOut;
    out.position = frame.camera_transform * vec4f(world, 0.0, 1.0);
    out.position.z = text_depth(glyph.position.z);
    out.uv = glyph.uv_rect.xy + local * glyph.uv_rect.zw;
    out.color = glyph.color;
    out.id = glyph.id;
    out.px_range = glyph.px_range;
    return out;
}

fn median3(v: vec3f) -> f32 {
    return max(min(v.r, v.g), min(max(v.r, v.g), v.b));
}

fn screen_px_range(uv: vec2f, px_range: f32) -> f32 {
    let dims = textureDimensions(font_atlas);
    let texture_size = vec2f(f32(dims.x), f32(dims.y));
    let unit_range = vec2f(px_range) / texture_size;
    let screen_tex_size = vec2f(1.0) / max(fwidth(uv), vec2f(0.000001));
    return max(0.5 * dot(unit_range, screen_tex_size), 0.75);
}

fn text_luminance(color: vec3f) -> f32 {
    return dot(color, vec3f(0.2126, 0.7152, 0.0722));
}

fn distance_coverage(signed_distance: f32, px_range: f32) -> f32 {
    let large_text = smoothstep(1.5, 4.0, px_range);
    let smoothing = mix(1.2, 0.9, large_text);
    return clamp(signed_distance * (px_range / smoothing) + 0.5, 0.0, 1.0);
}

fn perceptual_coverage_gamma(coverage: f32, px_range: f32, color: vec3f) -> f32 {
    let small_text = 1.0 - smoothstep(2.0, 5.0, px_range);
    let luma = text_luminance(color);
    let light_text_coverage = pow(coverage, 0.88);
    let dark_text_coverage = 1.0 - pow(1.0 - coverage, 0.88);
    let color_weight = smoothstep(0.35, 0.65, luma);
    let gamma_coverage =
        mix(dark_text_coverage, light_text_coverage, color_weight);
    return mix(coverage, gamma_coverage, small_text * 0.45);
}

fn shade_text(in: VertexOut) -> vec4f {
    let tex = textureSample(font_atlas, font_sampler, in.uv);
    let msdf_distance = median3(tex.rgb) - 0.5;
    let sdf_distance = tex.a - 0.5;
    let px_range = screen_px_range(in.uv, in.px_range);
    let msdf_weight = smoothstep(1.75, 4.0, px_range);
    let signed_distance = mix(sdf_distance, msdf_distance, msdf_weight);
    let coverage = distance_coverage(signed_distance, px_range);
    let alpha = perceptual_coverage_gamma(coverage, px_range, in.color.rgb);
    return vec4f(in.color.rgb, in.color.a * alpha);
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
    var out: FragmentOut;
    out.color = shade_text(in);
    if (out.color.a < 0.001) {
        discard;
    }
    return out;
}

@fragment
fn fs_main_picking(in: VertexOut) -> FragmentOutPicking {
    var out: FragmentOutPicking;
    out.color = shade_text(in);
    if (out.color.a < 0.001) {
        discard;
    }
    out.id = in.id;
    return out;
}
)";

        class MsdfTextPipeline {
          public:
            void init(const wgpu::Device &device,
                      wgpu::TextureFormat targetFormat,
                      const wgpu::Buffer &frameBuffer, uint64_t frameBufferSize,
                      wgpu::TextureFormat pickingFormat,
                      const TextureResource &atlasResource) {
                m_device = device;
                m_targetFormat = targetFormat;
                m_pickingFormat = pickingFormat;
                m_frameBuffer = frameBuffer;
                m_frameBufferSize = frameBufferSize;
                m_atlasView = atlasResource.view;
                createShader();
                createBindGroupLayout();
                createPipelineLayout();
                createSampler();
                createPipelineState();
            }

            void destroy() {
                m_bindGroup = nullptr;
                m_sampler = nullptr;
                m_atlasView = nullptr;
                m_instanceBuffer = nullptr;
                m_instanceBufferSize = 0;
                m_pipeline = nullptr;
                m_pipelineLayout = nullptr;
                m_bindGroupLayout = nullptr;
                m_shader = nullptr;
                m_frameBuffer = nullptr;
                m_frameBufferSize = 0;
                m_device = nullptr;
            }

            [[nodiscard]] bool
            ensureInstanceBufferSize(std::size_t glyphCount) {
                const auto requiredSize = std::max<std::size_t>(
                    sizeof(MsdfTextInstance),
                    glyphCount * sizeof(MsdfTextInstance));
                if (m_instanceBuffer != nullptr &&
                    m_instanceBufferSize >= requiredSize) {
                    return false;
                }

                wgpu::BufferDescriptor descriptor{};
                descriptor.size = requiredSize;
                descriptor.usage =
                    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
                m_instanceBuffer = m_device.CreateBuffer(&descriptor);
                m_instanceBufferSize = requiredSize;
                createBindGroup();
                return true;
            }

            void uploadInstances(const wgpu::Queue &queue,
                                 const MsdfTextInstance *instances,
                                 uint64_t byteSize) const {
                if (m_instanceBuffer == nullptr || instances == nullptr ||
                    byteSize == 0) {
                    return;
                }
                queue.WriteBuffer(m_instanceBuffer, 0, instances, byteSize);
            }

            void draw(wgpu::RenderPassEncoder &renderPass, uint32_t firstGlyph,
                      uint32_t glyphCount) const {
                if (glyphCount == 0) {
                    return;
                }
                if (m_pipeline == nullptr || m_bindGroup == nullptr) {
                    throw std::runtime_error(
                        "MSDF text pipeline is not ready for drawing");
                }
                renderPass.SetPipeline(m_pipeline);
                renderPass.SetBindGroup(0, m_bindGroup);
                renderPass.Draw(6, glyphCount, 0, firstGlyph);
            }

          private:
            void createShader() {
                using Core::Renderer::ShaderLanguage;
                using Core::Renderer::ShaderModuleDesc;
                using Core::Renderer::ShaderStage;
                m_shader = std::make_unique<WgpuShader>(
                    "renderer_2d_msdf_text",
                    std::vector<ShaderModuleDesc>{
                        {.language = ShaderLanguage::WGSL,
                         .stage = ShaderStage::Vertex,
                         .entryPoint = "vs_main",
                         .source = kMsdfTextShader},
                        {.language = ShaderLanguage::WGSL,
                         .stage = ShaderStage::Fragment,
                         .entryPoint = "fs_main",
                         .source = kMsdfTextShader},
                    },
                    m_device);
            }

            void createBindGroupLayout() {
                std::array<wgpu::BindGroupLayoutEntry, 4> bindings{};
                bindings[0].binding = 0;
                bindings[0].visibility = wgpu::ShaderStage::Vertex;
                bindings[0].buffer.type =
                    wgpu::BufferBindingType::ReadOnlyStorage;

                bindings[1].binding = 1;
                bindings[1].visibility =
                    wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
                bindings[1].buffer.type = wgpu::BufferBindingType::Uniform;

                bindings[2].binding = 2;
                bindings[2].visibility = wgpu::ShaderStage::Fragment;
                bindings[2].sampler.type = wgpu::SamplerBindingType::Filtering;

                bindings[3].binding = 3;
                bindings[3].visibility = wgpu::ShaderStage::Fragment;
                bindings[3].texture.sampleType = wgpu::TextureSampleType::Float;
                bindings[3].texture.viewDimension =
                    wgpu::TextureViewDimension::e2D;

                wgpu::BindGroupLayoutDescriptor descriptor{};
                descriptor.entryCount = bindings.size();
                descriptor.entries = bindings.data();
                m_bindGroupLayout = m_device.CreateBindGroupLayout(&descriptor);
            }

            void createPipelineLayout() {
                wgpu::PipelineLayoutDescriptor descriptor{};
                descriptor.bindGroupLayoutCount = 1;
                descriptor.bindGroupLayouts = &m_bindGroupLayout;
                m_pipelineLayout = m_device.CreatePipelineLayout(&descriptor);
            }

            void createSampler() {
                wgpu::SamplerDescriptor descriptor{};
                descriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
                descriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
                descriptor.addressModeW = wgpu::AddressMode::ClampToEdge;
                descriptor.magFilter = wgpu::FilterMode::Linear;
                descriptor.minFilter = wgpu::FilterMode::Linear;
                descriptor.mipmapFilter = wgpu::MipmapFilterMode::Linear;
                m_sampler = m_device.CreateSampler(&descriptor);
            }

            void createBindGroup() {
                if (m_instanceBuffer == nullptr || m_frameBuffer == nullptr ||
                    m_bindGroupLayout == nullptr || m_sampler == nullptr ||
                    m_atlasView == nullptr) {
                    m_bindGroup = nullptr;
                    return;
                }

                std::array<wgpu::BindGroupEntry, 4> entries{};
                entries[0].binding = 0;
                entries[0].buffer = m_instanceBuffer;
                entries[0].offset = 0;
                entries[0].size = m_instanceBufferSize;

                entries[1].binding = 1;
                entries[1].buffer = m_frameBuffer;
                entries[1].offset = 0;
                entries[1].size = m_frameBufferSize;

                entries[2].binding = 2;
                entries[2].sampler = m_sampler;

                entries[3].binding = 3;
                entries[3].textureView = m_atlasView;

                wgpu::BindGroupDescriptor descriptor{};
                descriptor.layout = m_bindGroupLayout;
                descriptor.entryCount = entries.size();
                descriptor.entries = entries.data();
                m_bindGroup = m_device.CreateBindGroup(&descriptor);
            }

            void createPipelineState() {
                wgpu::ColorTargetState colorTargets[2]{};
                uint32_t targetCount = 1;
                colorTargets[0].format = m_targetFormat;

                wgpu::BlendState blendState{};
                blendState.color.operation = wgpu::BlendOperation::Add;
                blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
                blendState.color.dstFactor =
                    wgpu::BlendFactor::OneMinusSrcAlpha;
                blendState.alpha.operation = wgpu::BlendOperation::Add;
                blendState.alpha.srcFactor = wgpu::BlendFactor::One;
                blendState.alpha.dstFactor =
                    wgpu::BlendFactor::OneMinusSrcAlpha;
                colorTargets[0].blend = &blendState;

                if (m_pickingFormat != wgpu::TextureFormat::Undefined) {
                    colorTargets[1].format = m_pickingFormat;
                    targetCount = 2;
                }

                wgpu::FragmentState fragment{};
                fragment.module =
                    m_shader->getModule(Core::Renderer::ShaderStage::Fragment);
                fragment.entryPoint =
                    m_pickingFormat != wgpu::TextureFormat::Undefined
                        ? "fs_main_picking"
                        : "fs_main";
                fragment.targetCount = targetCount;
                fragment.targets = colorTargets;

                wgpu::DepthStencilState depthStencil{};
                depthStencil.format = kDepthStencilFormat;
                depthStencil.depthCompare = wgpu::CompareFunction::LessEqual;
                depthStencil.depthWriteEnabled = false;

                wgpu::RenderPipelineDescriptor descriptor{};
                descriptor.layout = m_pipelineLayout;
                descriptor.vertex.module =
                    m_shader->getModule(Core::Renderer::ShaderStage::Vertex);
                descriptor.vertex.entryPoint = "vs_main";
                descriptor.primitive.topology =
                    wgpu::PrimitiveTopology::TriangleList;
                descriptor.primitive.cullMode = wgpu::CullMode::None;
                descriptor.fragment = &fragment;
                descriptor.depthStencil = &depthStencil;

                m_pipeline = m_device.CreateRenderPipeline(&descriptor);
                if (m_pipeline == nullptr) {
                    throw std::runtime_error(
                        "Failed to create MSDF text render pipeline");
                }
            }

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

        struct PathDrawRange {
            uint32_t firstStencilVertex = 0;
            uint32_t stencilVertexCount = 0;
            uint32_t firstCoverVertex = 0;
            uint32_t coverVertexCount = 0;
            bool evenOddFill = true;
            float zIndex = 0.f;
        };

        struct BakedPath {
            std::vector<PathStencilVertex> stencilVertices;
            std::array<PathCoverVertex, 6> coverVertices{};
            bool evenOddFill = true;
            bool valid = false;
        };

        struct PathBakeMetrics {
            float screenScale = 1.f;
            float pixelWorldSize = 1.f;
        };

        struct StrokeMeshParams {
            PathBakeMetrics metrics;
            float halfWidth = 0.5f;
            float fringe = 1.f;
            float overlap = 1.f;
        };

        struct StyledStrokeSegment {
            glm::vec2 from{0.f};
            glm::vec2 to{0.f};
            float fromHalfWidth = 0.5f;
            float toHalfWidth = 0.5f;
            PickingId id = PickingId::invalid();
        };

        PathBakeMetrics makePathBakeMetrics(const float *cameraTransform,
                                            const Renderer2DExtent &extent) {
            if (cameraTransform == nullptr || extent.width == 0 ||
                extent.height == 0) {
                return {};
            }

            const float viewportWidth = static_cast<float>(extent.width);
            const float viewportHeight = static_cast<float>(extent.height);
            const glm::vec2 xAxis(cameraTransform[0] * viewportWidth * 0.5f,
                                  cameraTransform[1] * viewportHeight * 0.5f);
            const glm::vec2 yAxis(cameraTransform[4] * viewportWidth * 0.5f,
                                  cameraTransform[5] * viewportHeight * 0.5f);
            const float screenScale =
                std::max({glm::length(xAxis), glm::length(yAxis), 0.0001f});
            return {.screenScale = screenScale,
                    .pixelWorldSize = 1.f / screenScale};
        }

        StrokeMeshParams makeStrokeMeshParams(const PathProps &props,
                                              const PathBakeMetrics &metrics) {
            const float requestedHalfWidth =
                std::max(props.strokeSize * 0.5f, 0.0001f);
            const float pixelHalfWidth = metrics.pixelWorldSize * 0.5f;
            return {.metrics = metrics,
                    .halfWidth = std::max(requestedHalfWidth, pixelHalfWidth),
                    .fringe = std::max(metrics.pixelWorldSize * 0.75f,
                                       requestedHalfWidth * 0.02f),
                    .overlap = metrics.pixelWorldSize * 0.75f};
        }

        class PathBatch {
          public:
            void clear() {
                m_stencilVertices.clear();
                m_coverVertices.clear();
                m_drawRanges.clear();
            }

            void push(BakedPath &&path, float zIndex) {
                if (!path.valid || path.stencilVertices.empty()) {
                    return;
                }

                PathDrawRange range{};
                range.firstStencilVertex =
                    static_cast<uint32_t>(m_stencilVertices.size());
                range.stencilVertexCount =
                    static_cast<uint32_t>(path.stencilVertices.size());
                range.firstCoverVertex =
                    static_cast<uint32_t>(m_coverVertices.size());
                range.coverVertexCount =
                    static_cast<uint32_t>(path.coverVertices.size());
                range.evenOddFill = path.evenOddFill;
                range.zIndex = zIndex;

                m_stencilVertices.insert(m_stencilVertices.end(),
                                         path.stencilVertices.begin(),
                                         path.stencilVertices.end());
                m_coverVertices.insert(m_coverVertices.end(),
                                       path.coverVertices.begin(),
                                       path.coverVertices.end());
                m_drawRanges.push_back(range);
            }

            void prepareForRendering(bool sortBackToFront) {
                if (!sortBackToFront || m_drawRanges.size() <= 1) {
                    return;
                }

                std::stable_sort(
                    m_drawRanges.begin(), m_drawRanges.end(),
                    [](const PathDrawRange &a, const PathDrawRange &b) {
                        return a.zIndex < b.zIndex;
                    });
            }

            [[nodiscard]] bool empty() const noexcept {
                return m_drawRanges.empty();
            }

            [[nodiscard]] uint32_t drawCount() const noexcept {
                return static_cast<uint32_t>(m_drawRanges.size());
            }

            [[nodiscard]] uint32_t stencilVertexCount() const noexcept {
                return static_cast<uint32_t>(m_stencilVertices.size());
            }

            [[nodiscard]] uint32_t coverVertexCount() const noexcept {
                return static_cast<uint32_t>(m_coverVertices.size());
            }

            [[nodiscard]] uint64_t stencilByteSize() const noexcept {
                return static_cast<uint64_t>(m_stencilVertices.size()) *
                       sizeof(PathStencilVertex);
            }

            [[nodiscard]] uint64_t coverByteSize() const noexcept {
                return static_cast<uint64_t>(m_coverVertices.size()) *
                       sizeof(PathCoverVertex);
            }

            [[nodiscard]] const PathStencilVertex *
            stencilData() const noexcept {
                return m_stencilVertices.data();
            }

            [[nodiscard]] const PathCoverVertex *coverData() const noexcept {
                return m_coverVertices.data();
            }

            [[nodiscard]] const PathDrawRange *drawRanges() const noexcept {
                return m_drawRanges.data();
            }

          private:
            std::vector<PathStencilVertex> m_stencilVertices;
            std::vector<PathCoverVertex> m_coverVertices;
            std::vector<PathDrawRange> m_drawRanges;
        };

        struct PathStrokeDrawRange {
            uint32_t firstVertex = 0;
            uint32_t vertexCount = 0;
            float zIndex = 0.f;
        };

        class PathStrokeBatch {
          public:
            void clear() {
                m_vertices.clear();
                m_drawRanges.clear();
            }

            void push(std::vector<PathCoverVertex> &&vertices, float zIndex) {
                if (vertices.empty()) {
                    return;
                }

                PathStrokeDrawRange range{};
                range.firstVertex = static_cast<uint32_t>(m_vertices.size());
                range.vertexCount = static_cast<uint32_t>(vertices.size());
                range.zIndex = zIndex;
                m_vertices.insert(m_vertices.end(), vertices.begin(),
                                  vertices.end());
                m_drawRanges.push_back(range);
            }

            void prepareForRendering(bool sortBackToFront) {
                if (!sortBackToFront || m_drawRanges.size() <= 1) {
                    return;
                }

                std::stable_sort(m_drawRanges.begin(), m_drawRanges.end(),
                                 [](const PathStrokeDrawRange &a,
                                    const PathStrokeDrawRange &b) {
                                     return a.zIndex < b.zIndex;
                                 });
            }

            [[nodiscard]] bool empty() const noexcept {
                return m_drawRanges.empty();
            }

            [[nodiscard]] uint32_t drawCount() const noexcept {
                return static_cast<uint32_t>(m_drawRanges.size());
            }

            [[nodiscard]] uint32_t vertexCount() const noexcept {
                return static_cast<uint32_t>(m_vertices.size());
            }

            [[nodiscard]] uint64_t byteSize() const noexcept {
                return static_cast<uint64_t>(m_vertices.size()) *
                       sizeof(PathCoverVertex);
            }

            [[nodiscard]] const PathCoverVertex *data() const noexcept {
                return m_vertices.data();
            }

            [[nodiscard]] const PathStrokeDrawRange *
            drawRanges() const noexcept {
                return m_drawRanges.data();
            }

          private:
            std::vector<PathCoverVertex> m_vertices;
            std::vector<PathStrokeDrawRange> m_drawRanges;
        };

        float signedArea2(const glm::vec2 &a, const glm::vec2 &b,
                          const glm::vec2 &c) {
            const glm::vec2 ab = b - a;
            const glm::vec2 ac = c - a;
            return (ab.x * ac.y) - (ab.y * ac.x);
        }

        bool nearlyDegenerateTriangle(const glm::vec2 &a, const glm::vec2 &b,
                                      const glm::vec2 &c) {
            return std::abs(signedArea2(a, b, c)) < 0.0001f;
        }

        void setStencilVertex(PathStencilVertex &vertex, const glm::vec2 &pos,
                              float z, const glm::vec2 &curveCoord,
                              uint32_t curveType) {
            vertex.position[0] = pos.x;
            vertex.position[1] = pos.y;
            vertex.position[2] = z;
            vertex.curveCoord[0] = curveCoord.x;
            vertex.curveCoord[1] = curveCoord.y;
            vertex.curveType = curveType;
        }

        void appendStencilTriangle(std::vector<PathStencilVertex> &vertices,
                                   const glm::vec2 &p0, const glm::vec2 &p1,
                                   const glm::vec2 &p2, float z,
                                   const glm::vec2 &c0, const glm::vec2 &c1,
                                   const glm::vec2 &c2, uint32_t curveType) {
            if (nearlyDegenerateTriangle(p0, p1, p2)) {
                return;
            }

            const size_t base = vertices.size();
            vertices.resize(base + 3);
            setStencilVertex(vertices[base + 0], p0, z, c0, curveType);
            setStencilVertex(vertices[base + 1], p1, z, c1, curveType);
            setStencilVertex(vertices[base + 2], p2, z, c2, curveType);
        }

        void appendLineAnchorTriangle(std::vector<PathStencilVertex> &vertices,
                                      const glm::vec2 &anchor,
                                      const glm::vec2 &from,
                                      const glm::vec2 &to, float z) {
            appendStencilTriangle(vertices, anchor, from, to, z, glm::vec2(0.f),
                                  glm::vec2(0.f), glm::vec2(0.f),
                                  kPathCurveTypeLine);
        }

        void appendQuadraticHull(std::vector<PathStencilVertex> &vertices,
                                 const glm::vec2 &from,
                                 const glm::vec2 &control, const glm::vec2 &to,
                                 float z) {
            appendStencilTriangle(vertices, from, control, to, z,
                                  glm::vec2(0.f, 0.f), glm::vec2(0.5f, 0.f),
                                  glm::vec2(1.f, 1.f), kPathCurveTypeQuadratic);
        }

        void growBounds(glm::vec2 &minPt, glm::vec2 &maxPt,
                        const glm::vec2 &p) {
            minPt = glm::min(minPt, p);
            maxPt = glm::max(maxPt, p);
        }

        void setCoverVertex(PathCoverVertex &vertex, const glm::vec2 &pos,
                            float z, const Color &color, const PickingId &id) {
            vertex.position[0] = pos.x;
            vertex.position[1] = pos.y;
            vertex.position[2] = z;
            vertex.color[0] = color.r;
            vertex.color[1] = color.g;
            vertex.color[2] = color.b;
            vertex.color[3] = color.a;
            vertex.id[0] = id.runtimeId;
            vertex.id[1] = id.info;
        }

        std::array<PathCoverVertex, 6>
        makeCoverVertices(const glm::vec2 &minPt, const glm::vec2 &maxPt,
                          const PathProps &props) {
            std::array<PathCoverVertex, 6> vertices{};
            const glm::vec2 p0(minPt.x, minPt.y);
            const glm::vec2 p1(maxPt.x, minPt.y);
            const glm::vec2 p2(minPt.x, maxPt.y);
            const glm::vec2 p3(maxPt.x, maxPt.y);

            setCoverVertex(vertices[0], p0, props.zIndex, props.fillColor,
                           props.id);
            setCoverVertex(vertices[1], p1, props.zIndex, props.fillColor,
                           props.id);
            setCoverVertex(vertices[2], p2, props.zIndex, props.fillColor,
                           props.id);
            setCoverVertex(vertices[3], p2, props.zIndex, props.fillColor,
                           props.id);
            setCoverVertex(vertices[4], p1, props.zIndex, props.fillColor,
                           props.id);
            setCoverVertex(vertices[5], p3, props.zIndex, props.fillColor,
                           props.id);
            return vertices;
        }

        glm::vec2 evalQuadratic(const glm::vec2 &p0, const glm::vec2 &control,
                                const glm::vec2 &p1, float t) {
            const float invT = 1.f - t;
            return (invT * invT * p0) + (2.f * invT * t * control) +
                   (t * t * p1);
        }

        glm::vec2 evalCubic(const glm::vec2 &p0, const glm::vec2 &control1,
                            const glm::vec2 &control2, const glm::vec2 &p1,
                            float t) {
            const float invT = 1.f - t;
            const float invT2 = invT * invT;
            const float t2 = t * t;
            return (invT2 * invT * p0) + (3.f * invT2 * t * control1) +
                   (3.f * invT * t2 * control2) + (t2 * t * p1);
        }

        float curveTolerance(const PathProps &props,
                             const PathBakeMetrics &metrics) {
            return std::max(props.curveTolerance * metrics.pixelWorldSize,
                            0.001f);
        }

        int quadraticSegmentCount(const glm::vec2 &p0, const glm::vec2 &control,
                                  const glm::vec2 &p1, const PathProps &props,
                                  const PathBakeMetrics &metrics) {
            const float controlNet =
                glm::distance(p0, control) + glm::distance(control, p1);
            const float curvature = glm::length(p0 - (2.f * control) + p1);
            const float tolerance = curveTolerance(props, metrics);
            const float lengthSegments = std::ceil(controlNet / 24.f);
            const float curveSegments =
                std::ceil(std::sqrt(curvature / tolerance));
            return std::clamp(
                static_cast<int>(std::max(lengthSegments, curveSegments)), 1,
                128);
        }

        int cubicSegmentCount(const glm::vec2 &p0, const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &p1,
                              const PathProps &props,
                              const PathBakeMetrics &metrics) {
            const float controlNet = glm::distance(p0, control1) +
                                     glm::distance(control1, control2) +
                                     glm::distance(control2, p1);
            const float curvature =
                std::max(glm::length(p0 - (2.f * control1) + control2),
                         glm::length(control1 - (2.f * control2) + p1));
            const float tolerance = curveTolerance(props, metrics);
            const float lengthSegments = std::ceil(controlNet / 18.f);
            const float curveSegments =
                std::ceil(std::sqrt((3.f * curvature) / tolerance));
            return std::clamp(
                static_cast<int>(std::max(lengthSegments, curveSegments)), 1,
                192);
        }

        BakedPath bakePath(std::span<const PathCommand> commands,
                           const PathProps &props,
                           const PathBakeMetrics &metrics) {
            BakedPath baked{};
            if (commands.empty() || !hasPathFill(props)) {
                return baked;
            }

            glm::vec2 minPt(std::numeric_limits<float>::max());
            glm::vec2 maxPt(-std::numeric_limits<float>::max());
            bool hasBounds = false;
            auto recordPoint = [&](const glm::vec2 &p) {
                growBounds(minPt, maxPt, p);
                hasBounds = true;
            };

            bool contourOpen = false;
            glm::vec2 anchor(0.f);
            glm::vec2 current(0.f);

            auto closeContour = [&](bool explicitClose) {
                if (!contourOpen || (!explicitClose && !props.closePath)) {
                    return;
                }
                appendLineAnchorTriangle(baked.stencilVertices, anchor, current,
                                         anchor, props.zIndex);
                current = anchor;
            };

            for (const auto &cmd : commands) {
                switch (cmd.kind) {
                case PathCommandKind::Move:
                    closeContour(false);
                    anchor = cmd.p;
                    current = cmd.p;
                    contourOpen = true;
                    recordPoint(cmd.p);
                    break;
                case PathCommandKind::Line:
                    if (!contourOpen) {
                        anchor = current;
                        contourOpen = true;
                        recordPoint(anchor);
                    }
                    appendLineAnchorTriangle(baked.stencilVertices, anchor,
                                             current, cmd.p, props.zIndex);
                    current = cmd.p;
                    recordPoint(cmd.p);
                    break;
                case PathCommandKind::Quad:
                    if (!contourOpen) {
                        anchor = current;
                        contourOpen = true;
                        recordPoint(anchor);
                    }
                    if (nearlyDegenerateTriangle(current, cmd.control, cmd.p)) {
                        appendLineAnchorTriangle(baked.stencilVertices, anchor,
                                                 current, cmd.p, props.zIndex);
                    } else {
                        appendLineAnchorTriangle(baked.stencilVertices, anchor,
                                                 current, cmd.p, props.zIndex);
                        appendQuadraticHull(baked.stencilVertices, current,
                                            cmd.control, cmd.p, props.zIndex);
                    }
                    current = cmd.p;
                    recordPoint(cmd.control);
                    recordPoint(cmd.p);
                    break;
                case PathCommandKind::Cubic:
                    if (!contourOpen) {
                        anchor = current;
                        contourOpen = true;
                        recordPoint(anchor);
                    }
                    {
                        const int segments = cubicSegmentCount(
                            current, cmd.control, cmd.control2, cmd.p, props,
                            metrics);
                        glm::vec2 prev = current;
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            glm::vec2 next = evalCubic(current, cmd.control,
                                                       cmd.control2, cmd.p, t);
                            appendLineAnchorTriangle(baked.stencilVertices,
                                                     anchor, prev, next,
                                                     props.zIndex);
                            prev = next;
                            recordPoint(next);
                        }
                    }
                    current = cmd.p;
                    recordPoint(cmd.control);
                    recordPoint(cmd.control2);
                    recordPoint(cmd.p);
                    break;
                case PathCommandKind::Close:
                    closeContour(true);
                    contourOpen = false;
                    break;
                }
            }

            closeContour(false);

            if (!hasBounds || baked.stencilVertices.empty()) {
                return baked;
            }

            constexpr float coverPadding = 1.f;
            minPt -= glm::vec2(coverPadding);
            maxPt += glm::vec2(coverPadding);
            if (maxPt.x <= minPt.x || maxPt.y <= minPt.y) {
                return baked;
            }

            baked.coverVertices = makeCoverVertices(minPt, maxPt, props);
            baked.evenOddFill = props.fillRule == PathFillRule::EvenOdd;
            baked.valid = true;
            return baked;
        }

        glm::vec2 safeNormalize(const glm::vec2 &v) {
            const float len = glm::length(v);
            if (len < 0.0001f) {
                return glm::vec2(1.f, 0.f);
            }
            return v / len;
        }

        glm::vec2 perpendicular(const glm::vec2 &v) {
            return glm::vec2(-v.y, v.x);
        }

        void appendStrokeVertex(std::vector<PathCoverVertex> &vertices,
                                const glm::vec2 &pos, const PathProps &props,
                                float alphaScale = 1.f) {
            auto &vertex = vertices.emplace_back();
            Color color = props.strokeColor;
            color.a *= std::clamp(alphaScale, 0.f, 1.f);
            setCoverVertex(vertex, pos, props.zIndex, color, props.id);
        }

        void appendStrokeTriangle(std::vector<PathCoverVertex> &vertices,
                                  const glm::vec2 &a, const glm::vec2 &b,
                                  const glm::vec2 &c, const PathProps &props,
                                  float alphaA = 1.f, float alphaB = 1.f,
                                  float alphaC = 1.f) {
            if (nearlyDegenerateTriangle(a, b, c)) {
                return;
            }
            appendStrokeVertex(vertices, a, props, alphaA);
            appendStrokeVertex(vertices, b, props, alphaB);
            appendStrokeVertex(vertices, c, props, alphaC);
        }

        void appendRoundCap(std::vector<PathCoverVertex> &vertices,
                            const glm::vec2 &center,
                            const glm::vec2 &capDirection,
                            const PathProps &props,
                            const StrokeMeshParams &mesh, float halfWidth) {
            constexpr float pi = 3.14159265358979323846f;
            const glm::vec2 dir = safeNormalize(capDirection);
            const float centerAngle = std::atan2(dir.y, dir.x);
            const int segments = std::clamp(
                static_cast<int>(
                    std::ceil(pi * halfWidth * mesh.metrics.screenScale / 4.f)),
                8, 96);

            glm::vec2 prevInner =
                center + glm::vec2(std::cos(centerAngle - (pi * 0.5f)),
                                   std::sin(centerAngle - (pi * 0.5f))) *
                             halfWidth;
            glm::vec2 prevOuter =
                center + glm::vec2(std::cos(centerAngle - (pi * 0.5f)),
                                   std::sin(centerAngle - (pi * 0.5f))) *
                             (halfWidth + mesh.fringe);
            for (int i = 1; i <= segments; ++i) {
                const float t =
                    static_cast<float>(i) / static_cast<float>(segments);
                const float angle = centerAngle - (pi * 0.5f) + (pi * t);
                glm::vec2 nextInner =
                    center +
                    glm::vec2(std::cos(angle), std::sin(angle)) * halfWidth;
                glm::vec2 nextOuter =
                    center + glm::vec2(std::cos(angle), std::sin(angle)) *
                                 (halfWidth + mesh.fringe);
                appendStrokeTriangle(vertices, center, prevInner, nextInner,
                                     props);
                appendStrokeTriangle(vertices, prevInner, prevOuter, nextInner,
                                     props, 1.f, 0.f, 1.f);
                appendStrokeTriangle(vertices, nextInner, prevOuter, nextOuter,
                                     props, 1.f, 0.f, 0.f);
                prevInner = nextInner;
                prevOuter = nextOuter;
            }
        }

        void appendRoundCap(std::vector<PathCoverVertex> &vertices,
                            const glm::vec2 &center,
                            const glm::vec2 &capDirection,
                            const PathProps &props,
                            const StrokeMeshParams &mesh) {
            appendRoundCap(vertices, center, capDirection, props, mesh,
                           mesh.halfWidth);
        }

        void appendRoundJoin(std::vector<PathCoverVertex> &vertices,
                             const glm::vec2 &center,
                             const glm::vec2 &prevOuterNormal,
                             const glm::vec2 &nextOuterNormal,
                             const PathProps &props,
                             const StrokeMeshParams &mesh) {
            const float a0 = std::atan2(prevOuterNormal.y, prevOuterNormal.x);
            float delta =
                std::atan2((prevOuterNormal.x * nextOuterNormal.y) -
                               (prevOuterNormal.y * nextOuterNormal.x),
                           glm::dot(prevOuterNormal, nextOuterNormal));
            if (std::abs(delta) < 0.0001f) {
                return;
            }

            const int segments = std::clamp(
                static_cast<int>(std::ceil(std::abs(delta) * mesh.halfWidth *
                                           mesh.metrics.screenScale / 4.f)),
                4, 96);
            glm::vec2 prevInner = center + prevOuterNormal * mesh.halfWidth;
            glm::vec2 prevOuter =
                center + prevOuterNormal * (mesh.halfWidth + mesh.fringe);
            for (int i = 1; i <= segments; ++i) {
                const float t =
                    static_cast<float>(i) / static_cast<float>(segments);
                const float angle = a0 + (delta * t);
                const glm::vec2 normal(std::cos(angle), std::sin(angle));
                glm::vec2 nextInner = center + normal * mesh.halfWidth;
                glm::vec2 nextOuter =
                    center + normal * (mesh.halfWidth + mesh.fringe);
                appendStrokeTriangle(vertices, center, prevInner, nextInner,
                                     props);
                appendStrokeTriangle(vertices, prevInner, prevOuter, nextInner,
                                     props, 1.f, 0.f, 1.f);
                appendStrokeTriangle(vertices, nextInner, prevOuter, nextOuter,
                                     props, 1.f, 0.f, 0.f);
                prevInner = nextInner;
                prevOuter = nextOuter;
            }
        }

        void appendRoundJoin(std::vector<PathCoverVertex> &vertices,
                             const glm::vec2 &center,
                             const glm::vec2 &prevOuterNormal,
                             const glm::vec2 &nextOuterNormal,
                             float prevHalfWidth, float nextHalfWidth,
                             const PathProps &props,
                             const StrokeMeshParams &mesh) {
            const float a0 = std::atan2(prevOuterNormal.y, prevOuterNormal.x);
            float delta =
                std::atan2((prevOuterNormal.x * nextOuterNormal.y) -
                               (prevOuterNormal.y * nextOuterNormal.x),
                           glm::dot(prevOuterNormal, nextOuterNormal));
            if (std::abs(delta) < 0.0001f) {
                return;
            }

            const float maxHalfWidth = std::max(prevHalfWidth, nextHalfWidth);
            const int segments = std::clamp(
                static_cast<int>(std::ceil(std::abs(delta) * maxHalfWidth *
                                           mesh.metrics.screenScale / 4.f)),
                4, 96);
            glm::vec2 prevInner = center + prevOuterNormal * prevHalfWidth;
            glm::vec2 prevOuter =
                center + prevOuterNormal * (prevHalfWidth + mesh.fringe);
            for (int i = 1; i <= segments; ++i) {
                const float t =
                    static_cast<float>(i) / static_cast<float>(segments);
                const float angle = a0 + (delta * t);
                const float halfWidth =
                    prevHalfWidth + ((nextHalfWidth - prevHalfWidth) * t);
                const glm::vec2 normal(std::cos(angle), std::sin(angle));
                glm::vec2 nextInner = center + normal * halfWidth;
                glm::vec2 nextOuter =
                    center + normal * (halfWidth + mesh.fringe);
                appendStrokeTriangle(vertices, center, prevInner, nextInner,
                                     props);
                appendStrokeTriangle(vertices, prevInner, prevOuter, nextInner,
                                     props, 1.f, 0.f, 1.f);
                appendStrokeTriangle(vertices, nextInner, prevOuter, nextOuter,
                                     props, 1.f, 0.f, 0.f);
                prevInner = nextInner;
                prevOuter = nextOuter;
            }
        }

        void appendStrokeJoin(std::vector<PathCoverVertex> &vertices,
                              const glm::vec2 &center, const glm::vec2 &prevDir,
                              const glm::vec2 &nextDir, const PathProps &props,
                              const StrokeMeshParams &mesh) {
            const float turn =
                (prevDir.x * nextDir.y) - (prevDir.y * nextDir.x);
            if (std::abs(turn) < 0.0001f) {
                return;
            }

            const glm::vec2 prevNormal = perpendicular(prevDir);
            const glm::vec2 nextNormal = perpendicular(nextDir);
            const glm::vec2 prevLeft = center + prevNormal * mesh.halfWidth;
            const glm::vec2 prevRight = center - prevNormal * mesh.halfWidth;
            const glm::vec2 nextLeft = center + nextNormal * mesh.halfWidth;
            const glm::vec2 nextRight = center - nextNormal * mesh.halfWidth;

            appendStrokeTriangle(vertices, center, prevLeft, nextLeft, props);
            appendStrokeTriangle(vertices, center, nextRight, prevRight, props);

            const float side = turn > 0.f ? -1.f : 1.f;
            const glm::vec2 prevOuterNormal = prevNormal * side;
            const glm::vec2 nextOuterNormal = nextNormal * side;
            const glm::vec2 prevOuter =
                center + prevOuterNormal * mesh.halfWidth;
            const glm::vec2 nextOuter =
                center + nextOuterNormal * mesh.halfWidth;

            if (props.lineJoin == PathLineJoin::Round) {
                appendRoundJoin(vertices, center, prevOuterNormal,
                                nextOuterNormal, props, mesh);
                return;
            }

            if (props.lineJoin == PathLineJoin::Miter) {
                glm::vec2 miter = prevOuterNormal + nextOuterNormal;
                if (glm::length(miter) > 0.0001f) {
                    miter = safeNormalize(miter);
                    const float denom = glm::dot(miter, nextOuterNormal);
                    if (std::abs(denom) > 0.0001f) {
                        const float miterLength = mesh.halfWidth / denom;
                        const float limit =
                            mesh.halfWidth * std::max(props.miterLimit, 1.f);
                        if (std::abs(miterLength) <= limit) {
                            const glm::vec2 miterPoint =
                                center + miter * miterLength;
                            appendStrokeTriangle(vertices, prevOuter,
                                                 miterPoint, nextOuter, props);
                            return;
                        }
                    }
                }
            }

            appendStrokeTriangle(vertices, center, prevOuter, nextOuter, props);
        }

        void appendStyledStrokeJoin(std::vector<PathCoverVertex> &vertices,
                                    const glm::vec2 &center,
                                    const glm::vec2 &prevDir,
                                    const glm::vec2 &nextDir,
                                    float prevHalfWidth, float nextHalfWidth,
                                    const PickingId &id, const PathProps &props,
                                    const StrokeMeshParams &mesh) {
            PathProps joinProps = props;
            joinProps.id = id;

            const float turn =
                (prevDir.x * nextDir.y) - (prevDir.y * nextDir.x);
            if (std::abs(turn) < 0.0001f) {
                return;
            }

            const glm::vec2 prevNormal = perpendicular(prevDir);
            const glm::vec2 nextNormal = perpendicular(nextDir);
            const glm::vec2 prevLeft = center + prevNormal * prevHalfWidth;
            const glm::vec2 prevRight = center - prevNormal * prevHalfWidth;
            const glm::vec2 nextLeft = center + nextNormal * nextHalfWidth;
            const glm::vec2 nextRight = center - nextNormal * nextHalfWidth;

            appendStrokeTriangle(vertices, center, prevLeft, nextLeft,
                                 joinProps);
            appendStrokeTriangle(vertices, center, nextRight, prevRight,
                                 joinProps);

            const float side = turn > 0.f ? -1.f : 1.f;
            const glm::vec2 prevOuterNormal = prevNormal * side;
            const glm::vec2 nextOuterNormal = nextNormal * side;
            const glm::vec2 prevOuter =
                center + prevOuterNormal * prevHalfWidth;
            const glm::vec2 nextOuter =
                center + nextOuterNormal * nextHalfWidth;

            if (props.lineJoin == PathLineJoin::Round) {
                appendRoundJoin(vertices, center, prevOuterNormal,
                                nextOuterNormal, prevHalfWidth, nextHalfWidth,
                                joinProps, mesh);
                return;
            }

            if (props.lineJoin == PathLineJoin::Miter &&
                std::abs(prevHalfWidth - nextHalfWidth) < 0.0001f) {
                glm::vec2 miter = prevOuterNormal + nextOuterNormal;
                if (glm::length(miter) > 0.0001f) {
                    miter = safeNormalize(miter);
                    const float denom = glm::dot(miter, nextOuterNormal);
                    if (std::abs(denom) > 0.0001f) {
                        const float miterLength = nextHalfWidth / denom;
                        const float limit =
                            nextHalfWidth * std::max(props.miterLimit, 1.f);
                        if (std::abs(miterLength) <= limit) {
                            const glm::vec2 miterPoint =
                                center + miter * miterLength;
                            appendStrokeTriangle(vertices, prevOuter,
                                                 miterPoint, nextOuter,
                                                 joinProps);
                            return;
                        }
                    }
                }
            }

            appendStrokeTriangle(vertices, center, prevOuter, nextOuter,
                                 joinProps);
        }

        void appendStrokeSegment(std::vector<PathCoverVertex> &vertices,
                                 const glm::vec2 &from, const glm::vec2 &to,
                                 const PathProps &props,
                                 const StrokeMeshParams &mesh, bool extendStart,
                                 bool extendEnd) {
            const glm::vec2 delta = to - from;
            if (glm::length(delta) < 0.0001f) {
                return;
            }

            const glm::vec2 dir = safeNormalize(delta);
            const glm::vec2 segmentFrom =
                from - dir * (extendStart ? mesh.overlap : 0.f);
            const glm::vec2 segmentTo =
                to + dir * (extendEnd ? mesh.overlap : 0.f);
            const glm::vec2 normal = perpendicular(dir);
            const glm::vec2 innerNormal = normal * mesh.halfWidth;
            const glm::vec2 outerNormal =
                normal * (mesh.halfWidth + mesh.fringe);
            const glm::vec2 leftFrom = segmentFrom + innerNormal;
            const glm::vec2 rightFrom = segmentFrom - innerNormal;
            const glm::vec2 leftTo = segmentTo + innerNormal;
            const glm::vec2 rightTo = segmentTo - innerNormal;
            const glm::vec2 outerLeftFrom = segmentFrom + outerNormal;
            const glm::vec2 outerLeftTo = segmentTo + outerNormal;
            const glm::vec2 outerRightFrom = segmentFrom - outerNormal;
            const glm::vec2 outerRightTo = segmentTo - outerNormal;

            appendStrokeTriangle(vertices, leftFrom, rightFrom, leftTo, props);
            appendStrokeTriangle(vertices, leftTo, rightFrom, rightTo, props);
            appendStrokeTriangle(vertices, leftFrom, outerLeftFrom, leftTo,
                                 props, 1.f, 0.f, 1.f);
            appendStrokeTriangle(vertices, leftTo, outerLeftFrom, outerLeftTo,
                                 props, 1.f, 0.f, 0.f);
            appendStrokeTriangle(vertices, rightFrom, rightTo, outerRightFrom,
                                 props, 1.f, 1.f, 0.f);
            appendStrokeTriangle(vertices, rightTo, outerRightTo,
                                 outerRightFrom, props, 1.f, 0.f, 0.f);
        }

        void appendStyledStrokeSegment(std::vector<PathCoverVertex> &vertices,
                                       const StyledStrokeSegment &segment,
                                       const PathProps &props,
                                       const StrokeMeshParams &mesh,
                                       bool extendStart, bool extendEnd) {
            PathProps segmentProps = props;
            segmentProps.id = segment.id;

            const glm::vec2 delta = segment.to - segment.from;
            if (glm::length(delta) < 0.0001f || segment.fromHalfWidth <= 0.f ||
                segment.toHalfWidth <= 0.f) {
                return;
            }

            const glm::vec2 dir = safeNormalize(delta);
            const glm::vec2 segmentFrom =
                segment.from - dir * (extendStart ? mesh.overlap : 0.f);
            const glm::vec2 segmentTo =
                segment.to + dir * (extendEnd ? mesh.overlap : 0.f);
            const glm::vec2 normal = perpendicular(dir);
            const glm::vec2 fromInnerNormal = normal * segment.fromHalfWidth;
            const glm::vec2 toInnerNormal = normal * segment.toHalfWidth;
            const glm::vec2 fromOuterNormal =
                normal * (segment.fromHalfWidth + mesh.fringe);
            const glm::vec2 toOuterNormal =
                normal * (segment.toHalfWidth + mesh.fringe);

            const glm::vec2 leftFrom = segmentFrom + fromInnerNormal;
            const glm::vec2 rightFrom = segmentFrom - fromInnerNormal;
            const glm::vec2 leftTo = segmentTo + toInnerNormal;
            const glm::vec2 rightTo = segmentTo - toInnerNormal;
            const glm::vec2 outerLeftFrom = segmentFrom + fromOuterNormal;
            const glm::vec2 outerLeftTo = segmentTo + toOuterNormal;
            const glm::vec2 outerRightFrom = segmentFrom - fromOuterNormal;
            const glm::vec2 outerRightTo = segmentTo - toOuterNormal;

            appendStrokeTriangle(vertices, leftFrom, rightFrom, leftTo,
                                 segmentProps);
            appendStrokeTriangle(vertices, leftTo, rightFrom, rightTo,
                                 segmentProps);
            appendStrokeTriangle(vertices, leftFrom, outerLeftFrom, leftTo,
                                 segmentProps, 1.f, 0.f, 1.f);
            appendStrokeTriangle(vertices, leftTo, outerLeftFrom, outerLeftTo,
                                 segmentProps, 1.f, 0.f, 0.f);
            appendStrokeTriangle(vertices, rightFrom, rightTo, outerRightFrom,
                                 segmentProps, 1.f, 1.f, 0.f);
            appendStrokeTriangle(vertices, rightTo, outerRightTo,
                                 outerRightFrom, segmentProps, 1.f, 0.f, 0.f);
        }

        void appendStrokeContour(std::vector<PathCoverVertex> &vertices,
                                 std::vector<glm::vec2> points, bool closed,
                                 const PathProps &props,
                                 const StrokeMeshParams &mesh) {
            constexpr float epsilon = 0.0001f;
            if (points.size() < 2) {
                return;
            }

            std::vector<glm::vec2> compacted;
            compacted.reserve(points.size());
            for (const auto &point : points) {
                if (compacted.empty() ||
                    glm::distance(compacted.back(), point) > epsilon) {
                    compacted.push_back(point);
                }
            }
            points = std::move(compacted);
            if (points.size() < 2) {
                return;
            }

            const bool explicitlyClosed =
                glm::distance(points.front(), points.back()) < epsilon;
            if (closed && explicitlyClosed) {
                points.pop_back();
            }
            if (closed && points.size() < 3) {
                closed = false;
            }

            const size_t count = points.size();

            glm::vec2 startCapCenter = points.front();
            glm::vec2 endCapCenter = points.back();
            glm::vec2 startDir = safeNormalize(points[1] - points[0]);
            glm::vec2 endDir =
                safeNormalize(points[count - 1] - points[count - 2]);

            if (!closed && props.lineCap == PathLineCap::Square) {
                points.front() -= startDir * mesh.halfWidth;
                points.back() += endDir * mesh.halfWidth;
            }

            auto pointAt = [&](ptrdiff_t index) -> const glm::vec2 & {
                const auto wrapped = static_cast<size_t>(
                    (index + static_cast<ptrdiff_t>(count)) %
                    static_cast<ptrdiff_t>(count));
                return points[wrapped];
            };

            const size_t segmentCount = closed ? count : count - 1;
            for (size_t i = 0; i < segmentCount; ++i) {
                const size_t next = (i + 1) % count;
                const bool extendStart = closed || i > 0;
                const bool extendEnd = closed || i + 1 < segmentCount;
                appendStrokeSegment(vertices, points[i], points[next], props,
                                    mesh, extendStart, extendEnd);
            }

            const size_t joinCount = closed ? count : count > 2 ? count - 2 : 0;
            for (size_t join = 0; join < joinCount; ++join) {
                const size_t i = closed ? join : join + 1;
                const glm::vec2 prev = pointAt(static_cast<ptrdiff_t>(i) - 1);
                const glm::vec2 curr = points[i];
                const glm::vec2 next = pointAt(static_cast<ptrdiff_t>(i) + 1);
                appendStrokeJoin(vertices, curr, safeNormalize(curr - prev),
                                 safeNormalize(next - curr), props, mesh);
            }

            if (!closed && props.lineCap == PathLineCap::Round) {
                appendRoundCap(vertices, startCapCenter, -startDir, props,
                               mesh);
                appendRoundCap(vertices, endCapCenter, endDir, props, mesh);
            }
        }

        void
        appendStyledStrokeContour(std::vector<PathCoverVertex> &vertices,
                                  std::vector<StyledStrokeSegment> segments,
                                  bool closed, const PathProps &props,
                                  const StrokeMeshParams &mesh) {
            constexpr float epsilon = 0.0001f;
            if (segments.empty()) {
                return;
            }

            std::vector<StyledStrokeSegment> compacted;
            compacted.reserve(segments.size());
            for (const auto &segment : segments) {
                if (glm::distance(segment.from, segment.to) <= epsilon ||
                    segment.fromHalfWidth <= 0.f ||
                    segment.toHalfWidth <= 0.f) {
                    continue;
                }
                compacted.push_back(segment);
            }
            segments = std::move(compacted);
            if (segments.empty()) {
                return;
            }

            if (closed && glm::distance(segments.back().to,
                                        segments.front().from) > epsilon) {
                segments.push_back(
                    {.from = segments.back().to,
                     .to = segments.front().from,
                     .fromHalfWidth = segments.back().toHalfWidth,
                     .toHalfWidth = segments.front().fromHalfWidth,
                     .id = segments.back().id});
            }

            if (closed && segments.size() < 2) {
                closed = false;
            }

            const glm::vec2 startCapCenter = segments.front().from;
            const glm::vec2 endCapCenter = segments.back().to;
            const glm::vec2 startDir =
                safeNormalize(segments.front().to - segments.front().from);
            const glm::vec2 endDir =
                safeNormalize(segments.back().to - segments.back().from);
            const float startHalfWidth = segments.front().fromHalfWidth;
            const float endHalfWidth = segments.back().toHalfWidth;

            if (!closed && props.lineCap == PathLineCap::Square) {
                segments.front().from -= startDir * startHalfWidth;
                segments.back().to += endDir * endHalfWidth;
            }

            const size_t segmentCount = segments.size();
            for (size_t i = 0; i < segmentCount; ++i) {
                const bool extendStart = closed || i > 0;
                const bool extendEnd = closed || i + 1 < segmentCount;
                appendStyledStrokeSegment(vertices, segments[i], props, mesh,
                                          extendStart, extendEnd);
            }

            const size_t joinCount =
                closed ? segmentCount
                       : (segmentCount > 1 ? segmentCount - 1 : 0);
            for (size_t join = 0; join < joinCount; ++join) {
                const size_t prevIndex = join;
                const size_t nextIndex = (join + 1) % segmentCount;
                const StyledStrokeSegment &prev = segments[prevIndex];
                const StyledStrokeSegment &next = segments[nextIndex];
                if (glm::distance(prev.to, next.from) > epsilon) {
                    continue;
                }

                appendStyledStrokeJoin(
                    vertices, prev.to, safeNormalize(prev.to - prev.from),
                    safeNormalize(next.to - next.from), prev.toHalfWidth,
                    next.fromHalfWidth, next.id, props, mesh);
            }

            if (!closed && props.lineCap == PathLineCap::Round) {
                PathProps startCapProps = props;
                startCapProps.id = segments.front().id;
                PathProps endCapProps = props;
                endCapProps.id = segments.back().id;
                appendRoundCap(vertices, startCapCenter, -startDir,
                               startCapProps, mesh, startHalfWidth);
                appendRoundCap(vertices, endCapCenter, endDir, endCapProps,
                               mesh, endHalfWidth);
            }
        }

        float halfWidthForCommand(const PathCommand &command,
                                  const PathProps &props,
                                  const PathBakeMetrics &metrics) {
            const float strokeSize = strokeSizeForCommand(command, props);
            if (strokeSize <= 0.f) {
                return 0.f;
            }

            const float requestedHalfWidth = strokeSize * 0.5f;
            const float pixelHalfWidth = metrics.pixelWorldSize * 0.5f;
            return std::max(requestedHalfWidth, pixelHalfWidth);
        }

        void
        appendSolidStyledPolyline(std::vector<StyledStrokeSegment> &contour,
                                  const std::vector<glm::vec2> &points,
                                  float halfWidth, const PickingId &id) {
            if (points.size() < 2 || halfWidth <= 0.f) {
                return;
            }

            for (size_t i = 1; i < points.size(); ++i) {
                contour.push_back({.from = points[i - 1],
                                   .to = points[i],
                                   .fromHalfWidth = halfWidth,
                                   .toHalfWidth = halfWidth,
                                   .id = id});
            }
        }

        void appendDashedStyledPolyline(std::vector<PathCoverVertex> &vertices,
                                        const std::vector<glm::vec2> &points,
                                        float halfWidth, const PickingId &id,
                                        const PathCommandStroke &stroke,
                                        const PathProps &props,
                                        const StrokeMeshParams &mesh) {
            constexpr float epsilon = 0.0001f;
            if (points.size() < 2 || halfWidth <= 0.f) {
                return;
            }

            const float dashLength = std::max(stroke.dashLength, 0.f);
            const float gapLength = std::max(stroke.gapLength, 0.f);
            const float patternLength = dashLength + gapLength;
            if (dashLength <= epsilon || gapLength <= epsilon ||
                patternLength <= epsilon) {
                std::vector<StyledStrokeSegment> solidContour;
                appendSolidStyledPolyline(solidContour, points, halfWidth, id);
                appendStyledStrokeContour(vertices, std::move(solidContour),
                                          false, props, mesh);
                return;
            }

            float phase = std::fmod(stroke.dashOffset, patternLength);
            if (phase < 0.f) {
                phase += patternLength;
            }
            bool drawing = phase < dashLength;
            float remaining =
                drawing ? dashLength - phase : patternLength - phase;
            if (remaining <= epsilon) {
                drawing = !drawing;
                remaining = drawing ? dashLength : gapLength;
            }

            std::vector<StyledStrokeSegment> dashContour;
            auto flushDash = [&]() {
                if (dashContour.empty()) {
                    return;
                }

                appendStyledStrokeContour(vertices, std::move(dashContour),
                                          false, props, mesh);
                dashContour.clear();
            };

            for (size_t i = 1; i < points.size(); ++i) {
                const glm::vec2 from = points[i - 1];
                const glm::vec2 to = points[i];
                const glm::vec2 delta = to - from;
                const float length = glm::length(delta);
                if (length <= epsilon) {
                    continue;
                }

                const glm::vec2 dir = delta / length;
                float consumed = 0.f;
                glm::vec2 cursor = from;
                while (consumed < length - epsilon) {
                    const float step = std::min(remaining, length - consumed);
                    const glm::vec2 next = cursor + (dir * step);
                    if (drawing && step > epsilon) {
                        dashContour.push_back({.from = cursor,
                                               .to = next,
                                               .fromHalfWidth = halfWidth,
                                               .toHalfWidth = halfWidth,
                                               .id = id});
                    }

                    consumed += step;
                    cursor = next;
                    remaining -= step;
                    if (remaining <= epsilon) {
                        if (drawing) {
                            flushDash();
                        }
                        drawing = !drawing;
                        remaining = drawing ? dashLength : gapLength;
                    }
                }
            }

            flushDash();
        }

        std::vector<PathCoverVertex>
        bakeStyledPathStroke(std::span<const PathCommand> commands,
                             const PathProps &props,
                             const PathBakeMetrics &metrics) {
            std::vector<PathCoverVertex> vertices;
            if (commands.empty() || !pathHasDrawableStroke(commands, props)) {
                return vertices;
            }

            const StrokeMeshParams mesh = makeStrokeMeshParams(props, metrics);
            std::vector<StyledStrokeSegment> contour;
            std::vector<glm::vec2> polyline;
            glm::vec2 current(0.f);
            glm::vec2 contourStart(0.f);
            bool contourOpen = false;

            auto flushContour = [&](bool closed) {
                if (contour.empty()) {
                    return;
                }

                appendStyledStrokeContour(vertices, std::move(contour), closed,
                                          props, mesh);
                contour.clear();
            };

            auto emitPolyline = [&](const PathCommand &command,
                                    const std::vector<glm::vec2> &points) {
                if (command.stroke.breakBefore) {
                    flushContour(false);
                }

                const float halfWidth =
                    halfWidthForCommand(command, props, metrics);
                if (halfWidth <= 0.f) {
                    flushContour(false);
                    return;
                }

                if (command.stroke.isDashed()) {
                    flushContour(false);
                    appendDashedStyledPolyline(
                        vertices, points, halfWidth,
                        pickingIdForCommand(command, props), command.stroke,
                        props, mesh);
                    return;
                }

                appendSolidStyledPolyline(contour, points, halfWidth,
                                          pickingIdForCommand(command, props));
                if (command.stroke.breakAfter) {
                    flushContour(false);
                }
            };

            auto startImplicitContour = [&]() {
                if (contourOpen) {
                    return;
                }

                contourStart = current;
                contourOpen = true;
            };

            for (const auto &cmd : commands) {
                switch (cmd.kind) {
                case PathCommandKind::Move:
                    flushContour(props.closePath);
                    current = cmd.p;
                    contourStart = cmd.p;
                    contourOpen = true;
                    break;
                case PathCommandKind::Line:
                    startImplicitContour();
                    polyline.clear();
                    polyline.push_back(current);
                    polyline.push_back(cmd.p);
                    emitPolyline(cmd, polyline);
                    current = cmd.p;
                    break;
                case PathCommandKind::Quad:
                    startImplicitContour();
                    polyline.clear();
                    polyline.push_back(current);
                    if (nearlyDegenerateTriangle(current, cmd.control, cmd.p)) {
                        polyline.push_back(cmd.p);
                    } else {
                        const int segments = quadraticSegmentCount(
                            current, cmd.control, cmd.p, props, metrics);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            polyline.push_back(
                                evalQuadratic(current, cmd.control, cmd.p, t));
                        }
                    }
                    emitPolyline(cmd, polyline);
                    current = cmd.p;
                    break;
                case PathCommandKind::Cubic:
                    startImplicitContour();
                    polyline.clear();
                    polyline.push_back(current);
                    {
                        const int segments = cubicSegmentCount(
                            current, cmd.control, cmd.control2, cmd.p, props,
                            metrics);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            polyline.push_back(evalCubic(
                                current, cmd.control, cmd.control2, cmd.p, t));
                        }
                    }
                    emitPolyline(cmd, polyline);
                    current = cmd.p;
                    break;
                case PathCommandKind::Close:
                    if (contourOpen) {
                        polyline.clear();
                        polyline.push_back(current);
                        polyline.push_back(contourStart);
                        emitPolyline(cmd, polyline);
                        current = contourStart;
                        flushContour(!cmd.stroke.breakBefore &&
                                     !cmd.stroke.breakAfter &&
                                     !cmd.stroke.isDashed());
                    }
                    contourOpen = false;
                    break;
                }
            }

            flushContour(props.closePath);
            return vertices;
        }

        std::vector<PathCoverVertex>
        bakePathStroke(std::span<const PathCommand> commands,
                       const PathProps &props, const PathBakeMetrics &metrics) {
            std::vector<PathCoverVertex> vertices;
            if (commands.empty() || !pathHasDrawableStroke(commands, props)) {
                return vertices;
            }
            if (pathNeedsStyledStrokeBaker(commands, props)) {
                return bakeStyledPathStroke(commands, props, metrics);
            }

            const StrokeMeshParams mesh = makeStrokeMeshParams(props, metrics);
            std::vector<glm::vec2> contour;
            glm::vec2 current(0.f);

            auto flushContour = [&](bool closed) {
                if (contour.empty()) {
                    return;
                }

                appendStrokeContour(vertices, std::move(contour), closed, props,
                                    mesh);
                contour.clear();
            };

            for (const auto &cmd : commands) {
                switch (cmd.kind) {
                case PathCommandKind::Move:
                    flushContour(props.closePath);
                    contour.push_back(cmd.p);
                    current = cmd.p;
                    break;
                case PathCommandKind::Line:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    contour.push_back(cmd.p);
                    current = cmd.p;
                    break;
                case PathCommandKind::Quad:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    if (nearlyDegenerateTriangle(current, cmd.control, cmd.p)) {
                        contour.push_back(cmd.p);
                    } else {
                        const int segments = quadraticSegmentCount(
                            current, cmd.control, cmd.p, props, metrics);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            contour.push_back(
                                evalQuadratic(current, cmd.control, cmd.p, t));
                        }
                    }
                    current = cmd.p;
                    break;
                case PathCommandKind::Cubic:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    {
                        const int segments = cubicSegmentCount(
                            current, cmd.control, cmd.control2, cmd.p, props,
                            metrics);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            contour.push_back(evalCubic(
                                current, cmd.control, cmd.control2, cmd.p, t));
                        }
                    }
                    current = cmd.p;
                    break;
                case PathCommandKind::Close:
                    if (!contour.empty()) {
                        current = contour.front();
                    }
                    flushContour(true);
                    break;
                }
            }

            flushContour(props.closePath);
            return vertices;
        }

        std::vector<PathCoverVertex> bakePathFillAntiAlias(
            std::span<const PathCommand> commands, const PathProps &props,
            const PathBakeMetrics &metrics, float fringeScale) {
            std::vector<PathCoverVertex> vertices;
            if (commands.empty() || !hasPathFill(props) || fringeScale <= 0.f) {
                return vertices;
            }

            PathProps fringeProps = props;
            fringeProps.strokeColor = props.fillColor;
            fringeProps.lineJoin = PathLineJoin::Round;
            fringeProps.lineCap = PathLineCap::Round;

            const StrokeMeshParams mesh{
                .metrics = metrics,
                .halfWidth = 0.f,
                .fringe =
                    std::max(metrics.pixelWorldSize * fringeScale, 0.0001f),
                .overlap = 0.f};

            std::vector<glm::vec2> contour;
            glm::vec2 current(0.f);

            auto flushContour = [&](bool closed) {
                if (contour.empty()) {
                    return;
                }

                appendStrokeContour(vertices, std::move(contour), closed,
                                    fringeProps, mesh);
                contour.clear();
            };

            for (const auto &cmd : commands) {
                switch (cmd.kind) {
                case PathCommandKind::Move:
                    flushContour(props.closePath);
                    contour.push_back(cmd.p);
                    current = cmd.p;
                    break;
                case PathCommandKind::Line:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    contour.push_back(cmd.p);
                    current = cmd.p;
                    break;
                case PathCommandKind::Quad:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    if (nearlyDegenerateTriangle(current, cmd.control, cmd.p)) {
                        contour.push_back(cmd.p);
                    } else {
                        const int segments = quadraticSegmentCount(
                            current, cmd.control, cmd.p, props, metrics);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            contour.push_back(
                                evalQuadratic(current, cmd.control, cmd.p, t));
                        }
                    }
                    current = cmd.p;
                    break;
                case PathCommandKind::Cubic:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    {
                        const int segments = cubicSegmentCount(
                            current, cmd.control, cmd.control2, cmd.p, props,
                            metrics);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            contour.push_back(evalCubic(
                                current, cmd.control, cmd.control2, cmd.p, t));
                        }
                    }
                    current = cmd.p;
                    break;
                case PathCommandKind::Close:
                    if (!contour.empty()) {
                        current = contour.front();
                    }
                    flushContour(true);
                    break;
                }
            }

            flushContour(props.closePath);
            return vertices;
        }

        void submitPathCommands(std::span<const PathCommand> commands,
                                const PathProps &props,
                                const PathBakeMetrics &metrics,
                                PathBatch &opaquePathBatch,
                                PathBatch &transparentPathBatch,
                                PathStrokeBatch &opaquePathStrokeBatch,
                                PathStrokeBatch &transparentPathStrokeBatch) {
            if (commands.empty()) {
                return;
            }

            if (hasPathFill(props)) {
                BakedPath baked = bakePath(commands, props, metrics);
                if (baked.valid) {
                    if (isFillTransparent(props)) {
                        transparentPathBatch.push(std::move(baked),
                                                  props.zIndex);
                    } else {
                        opaquePathBatch.push(std::move(baked), props.zIndex);
                    }
                }
            }

            if (pathHasDrawableStroke(commands, props)) {
                const bool forceTransparentStroke =
                    hasPathFill(props) && isFillTransparent(props);
                const bool strokeIsTransparent =
                    forceTransparentStroke || isStrokeTransparent(props);
                PathStrokeBatch &strokeBatch = strokeIsTransparent
                                                   ? transparentPathStrokeBatch
                                                   : opaquePathStrokeBatch;
                strokeBatch.push(bakePathStroke(commands, props, metrics),
                                 props.zIndex);
            }
        }

        bool appendMsdfText(std::string_view text, const FontProps &props,
                            const MsdfFontAtlas &atlas, MsdfTextBatch &batch) {
            if (!atlas.valid()) {
                return false;
            }

            const float fontSize = props.fontSize;
            const float lineStartX = props.position.x;
            const float lineHeight =
                props.lineHeight > 0.f
                    ? props.lineHeight
                    : std::max(atlas.lineHeight() * fontSize, fontSize);

            const MsdfGlyph *spaceGlyph = atlas.findGlyph(' ');
            const float spaceAdvance =
                spaceGlyph != nullptr && spaceGlyph->advance > 0.f
                    ? spaceGlyph->advance * fontSize
                    : fontSize * 0.25f;

            size_t offset = 0;
            glm::vec2 baseline{props.position.x, props.position.y};

            auto advanceLine = [&]() {
                baseline.x = lineStartX;
                baseline.y += lineHeight;
            };

            while (offset < text.size()) {
                const uint32_t codepoint = decodeUtf8(text, offset);
                if (codepoint == 0) {
                    break;
                }

                if (codepoint == '\r') {
                    if (offset < text.size() && text[offset] == '\n') {
                        ++offset;
                    }
                    advanceLine();
                    continue;
                }

                if (codepoint == '\n') {
                    advanceLine();
                    continue;
                }

                if (codepoint == '\t') {
                    baseline.x += spaceAdvance * std::max(props.tabSize, 1.f) +
                                  props.letterSpacing;
                    continue;
                }

                const MsdfGlyph *glyph = atlas.findGlyph(codepoint);
                if (glyph == nullptr) {
                    continue;
                }

                if (glyph->drawable) {
                    const glm::vec4 &bounds = glyph->planeBounds;
                    const float left = baseline.x + bounds.x * fontSize;
                    const float right = baseline.x + bounds.z * fontSize;
                    const float top = baseline.y - bounds.w * fontSize;
                    const float bottom = baseline.y - bounds.y * fontSize;
                    const glm::vec2 size{
                        std::max(0.f, right - left),
                        std::max(0.f, bottom - top),
                    };

                    if (size.x > 0.f && size.y > 0.f) {
                        MsdfTextInstance instance;
                        instance.position[0] = left + size.x * 0.5f;
                        instance.position[1] = top + size.y * 0.5f;
                        instance.position[2] = props.zIndex;
                        instance.pxRange = atlas.pxRange();
                        instance.size[0] = size.x;
                        instance.size[1] = size.y;
                        instance.color[0] = props.color.r;
                        instance.color[1] = props.color.g;
                        instance.color[2] = props.color.b;
                        instance.color[3] = props.color.a;
                        const glm::vec4 &uv = glyph->atlasRegion.getStartWH();
                        instance.uvRect[0] = uv.x;
                        instance.uvRect[1] = uv.y;
                        instance.uvRect[2] = uv.z;
                        instance.uvRect[3] = uv.w;
                        instance.id[0] = props.id.runtimeId;
                        instance.id[1] = props.id.info;
                        batch.push(instance);
                    }
                }

                const float advance = glyph->advance > 0.f
                                          ? glyph->advance * fontSize
                                          : fontSize * 0.5f;
                baseline.x += advance + props.letterSpacing;
            }

            return true;
        }

        glm::vec2 measureMsdfText(std::string_view text,
                                  const FontProps &props,
                                  const MsdfFontAtlas &atlas) {
            if (!atlas.valid() || text.empty() || props.fontSize <= 0.f) {
                return {0.f, 0.f};
            }

            const float fontSize = props.fontSize;

            const MsdfGlyph *spaceGlyph = atlas.findGlyph(' ');
            const float spaceAdvance =
                spaceGlyph != nullptr && spaceGlyph->advance > 0.f
                    ? spaceGlyph->advance * fontSize
                    : fontSize * 0.25f;

            float lineAdvance = 0.f;
            float lineInkMin = 0.f;
            float lineInkMax = 0.f;
            bool hasLineInk = false;
            float maxWidth = 0.f;
            float totalHeight = fontSize;

            auto finishLine = [&]() {
                float lineWidth = lineAdvance;
                if (hasLineInk) {
                    const float inkMin = std::min(0.f, lineInkMin);
                    const float inkMax = std::max(lineAdvance, lineInkMax);
                    lineWidth = std::max(lineWidth, inkMax - inkMin);
                }
                maxWidth = std::max(maxWidth, lineWidth);

                lineAdvance = 0.f;
                lineInkMin = 0.f;
                lineInkMax = 0.f;
                hasLineInk = false;
            };

            size_t offset = 0;
            while (offset < text.size()) {
                const uint32_t codepoint = decodeUtf8(text, offset);
                if (codepoint == 0) {
                    break;
                }

                if (codepoint == '\r') {
                    if (offset < text.size() && text[offset] == '\n') {
                        ++offset;
                    }
                    finishLine();
                    totalHeight += fontSize;
                    continue;
                }

                if (codepoint == '\n') {
                    finishLine();
                    totalHeight += fontSize;
                    continue;
                }

                if (codepoint == '\t') {
                    lineAdvance +=
                        spaceAdvance * std::max(props.tabSize, 1.f) +
                        props.letterSpacing;
                    continue;
                }

                const MsdfGlyph *glyph = atlas.findGlyph(codepoint);
                if (glyph == nullptr) {
                    continue;
                }

                if (glyph->drawable) {
                    const glm::vec4 &bounds = glyph->planeBounds;
                    const float glyphLeft = lineAdvance + bounds.x * fontSize;
                    const float glyphRight = lineAdvance + bounds.z * fontSize;
                    if (hasLineInk) {
                        lineInkMin = std::min(lineInkMin, glyphLeft);
                        lineInkMax = std::max(lineInkMax, glyphRight);
                    } else {
                        lineInkMin = glyphLeft;
                        lineInkMax = glyphRight;
                        hasLineInk = true;
                    }
                }

                const float advance = glyph->advance > 0.f
                                          ? glyph->advance * fontSize
                                          : fontSize * 0.5f;
                lineAdvance += advance + props.letterSpacing;
            }

            finishLine();
            return {maxWidth, totalHeight};
        }

        float msdfCenterOffsetY(std::string_view text, const FontProps &props,
                                const MsdfFontAtlas &atlas) {
            if (!atlas.valid() || text.empty() || props.fontSize <= 0.f) {
                return 0.f;
            }

            const float fontSize = props.fontSize;
            const float lineHeight =
                props.lineHeight > 0.f
                    ? props.lineHeight
                    : std::max(atlas.lineHeight() * fontSize, fontSize);

            float baselineY = 0.f;
            float inkTop = std::numeric_limits<float>::max();
            float inkBottom = std::numeric_limits<float>::lowest();
            bool hasInk = false;

            size_t offset = 0;
            while (offset < text.size()) {
                const uint32_t codepoint = decodeUtf8(text, offset);
                if (codepoint == 0) {
                    break;
                }

                if (codepoint == '\r') {
                    if (offset < text.size() && text[offset] == '\n') {
                        ++offset;
                    }
                    baselineY += lineHeight;
                    continue;
                }

                if (codepoint == '\n') {
                    baselineY += lineHeight;
                    continue;
                }

                if (codepoint == '\t') {
                    continue;
                }

                const MsdfGlyph *glyph = atlas.findGlyph(codepoint);
                if (glyph == nullptr || !glyph->drawable) {
                    continue;
                }

                const glm::vec4 &bounds = glyph->planeBounds;
                const float top = baselineY - bounds.w * fontSize;
                const float bottom = baselineY - bounds.y * fontSize;
                inkTop = std::min(inkTop, top);
                inkBottom = std::max(inkBottom, bottom);
                hasInk = true;
            }

            return hasInk ? -((inkTop + inkBottom) * 0.5f)
                          : fontSize * 0.35f;
        }

        glm::vec2 measurePathText(std::string_view text,
                                  const FontProps &props, FontFile &font) {
            if (text.empty() || props.fontSize <= 0.f ||
                font.getSize() <= 0.f) {
                return {0.f, 0.f};
            }

            const float scale = props.fontSize / font.getSize();
            const Glyph &spaceGlyph = font.getGlyph(U' ');
            const float spaceAdvance =
                std::max(spaceGlyph.advanceX * scale, props.fontSize * 0.25f);

            float lineAdvance = 0.f;
            float maxWidth = 0.f;
            float totalHeight = props.fontSize;

            auto finishLine = [&]() {
                maxWidth = std::max(maxWidth, lineAdvance);
                lineAdvance = 0.f;
            };

            size_t offset = 0;
            while (offset < text.size()) {
                const uint32_t codepoint = decodeUtf8(text, offset);
                if (codepoint == 0) {
                    break;
                }

                if (codepoint == '\r') {
                    if (offset < text.size() && text[offset] == '\n') {
                        ++offset;
                    }
                    finishLine();
                    totalHeight += props.fontSize;
                    continue;
                }

                if (codepoint == '\n') {
                    finishLine();
                    totalHeight += props.fontSize;
                    continue;
                }

                if (codepoint == '\t') {
                    lineAdvance +=
                        spaceAdvance * std::max(props.tabSize, 1.f) +
                        props.letterSpacing;
                    continue;
                }

                const Glyph &glyph =
                    font.getGlyph(static_cast<char32_t>(codepoint));
                const float advance =
                    glyph.advanceX > 0.f
                        ? glyph.advanceX * scale
                        : std::max(glyph.width * scale,
                                   props.fontSize * 0.5f);
                lineAdvance += advance + props.letterSpacing;
            }

            finishLine();
            return {maxWidth, totalHeight};
        }

        float pathCenterOffsetY(std::string_view text, const FontProps &props,
                                FontFile &font) {
            if (text.empty() || props.fontSize <= 0.f ||
                font.getSize() <= 0.f) {
                return 0.f;
            }

            const float scale = props.fontSize / font.getSize();
            const float defaultLineHeight = font.lineHeight() * scale;
            const float lineHeight =
                props.lineHeight > 0.f
                    ? props.lineHeight
                    : (defaultLineHeight > 0.f ? defaultLineHeight
                                               : props.fontSize);

            float baselineY = 0.f;
            float inkTop = std::numeric_limits<float>::max();
            float inkBottom = std::numeric_limits<float>::lowest();
            bool hasInk = false;

            size_t offset = 0;
            while (offset < text.size()) {
                const uint32_t codepoint = decodeUtf8(text, offset);
                if (codepoint == 0) {
                    break;
                }

                if (codepoint == '\r') {
                    if (offset < text.size() && text[offset] == '\n') {
                        ++offset;
                    }
                    baselineY += lineHeight;
                    continue;
                }

                if (codepoint == '\n') {
                    baselineY += lineHeight;
                    continue;
                }

                if (codepoint == '\t') {
                    continue;
                }

                const Glyph &glyph =
                    font.getGlyph(static_cast<char32_t>(codepoint));
                const auto bounds = glyph.path.bounds();
                if (!bounds.valid) {
                    continue;
                }

                inkTop = std::min(inkTop, baselineY + bounds.min.y * scale);
                inkBottom =
                    std::max(inkBottom, baselineY + bounds.max.y * scale);
                hasInk = true;
            }

            return hasInk ? -((inkTop + inkBottom) * 0.5f)
                          : props.fontSize * 0.35f;
        }

        class TextureSource final : public Core::Renderer::ITexture {
          public:
            explicit TextureSource(
                const Core::Renderer::TextureCreateInfo &createInfo)
                : ITexture(createInfo) {}

            void init() override {}
            void destroy() override {}
        };

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
    } // namespace

    struct WgpuRenderer2D::Impl {
        Core::Renderer::Renderer2DCreateInfo createInfo;
        Renderer2DExtent extent;
        Core::Renderer::Renderer2DTargetFormat targetFormatType =
            Core::Renderer::Renderer2DTargetFormat::BGRA8Unorm;
        wgpu::TextureFormat targetFormat = wgpu::TextureFormat::BGRA8Unorm;

        wgpu::Instance instance;
        wgpu::Adapter adapter;
        wgpu::Device device;
        wgpu::Queue queue;
        wgpu::Surface surface;
        wgpu::SurfaceConfiguration surfaceConfiguration;
        wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::BGRA8Unorm;
        wgpu::PresentMode surfacePresentMode = wgpu::PresentMode::Fifo;
        wgpu::CompositeAlphaMode surfaceAlphaMode =
            wgpu::CompositeAlphaMode::Opaque;
        GLFWwindow *windowHandle = nullptr;
        bool surfaceConfigured = false;
        bool frameUsesSurface = false;
        Core::Renderer::TextureHandle frameTargetTexture = 0;
        Core::Renderer::TextureHandle lastCompletedTargetTexture = 0;

        wgpu::Texture offscreenTarget;
        wgpu::TextureView offscreenTargetView;
        wgpu::Texture depthTarget;
        wgpu::TextureView depthTargetView;
        float *cameraTransform = nullptr;
        Piplines::SharedFrameBuffer sharedFrameBuffer;
        std::unique_ptr<Piplines::PrimitivePipeline> primitivePipeline;
        std::unique_ptr<Piplines::PathPipeline> pathPipeline;
        std::unique_ptr<CustomQuadPipeline> customQuadPipeline;
        std::unique_ptr<MsdfTextPipeline> textPipeline;
        wgpu::CommandEncoder commandEncoder;
        std::unordered_map<Core::Renderer::TextureHandle, TextureResource>
            textures;
        std::shared_ptr<WgpuTexture> defaultTexture;

        PrimitiveBatch opaquePrimitiveBatch;
        PrimitiveBatch transparentPrimitiveBatch;
        CustomQuadBatch opaqueCustomQuadBatch;
        CustomQuadBatch transparentCustomQuadBatch;
        PathStrokeBatch opaquePathStrokeBatch;
        PathStrokeBatch transparentPathStrokeBatch;
        PathBatch opaquePathBatch;
        PathBatch transparentPathBatch;
        MsdfTextBatch textBatch;
        std::vector<PathCommand> activePathCommands;
        PathProps activePathProps;
        bool pathStarted = false;
        std::unique_ptr<FontFile> fontFile;
        std::unique_ptr<MsdfFontAtlas> msdfFontAtlas;
        std::vector<TransparentDrawItem> transparentDrawItems;
        std::vector<PathCommand> textPathCommandsScratch;
        Core::Renderer::Renderer2DStats stats;
        Color clearColor{0.f, 0.f, 0.f, 1.f};
        bool shouldClear = true;
        bool frameStarted = false;
        wgpu::TextureFormat pickingFormat = wgpu::TextureFormat::Undefined;
        Core::Renderer::TextureHandle pickingTextureHandle = 0;
        QueuedPickingReadback queuedPickingReadback;
        std::array<std::shared_ptr<AsyncPickingReadbackSlot>, 3>
            pickingReadbackSlots;
        size_t nextPickingReadbackSlot = 0;
        uint64_t nextPickingReadbackSequence = 1;
        uint64_t lastDeliveredPickingReadbackSequence = 0;

        void createDevice();
        void createOffscreenTarget();
        void createDepthTarget();
        void createWindowSurface();
        void configureWindowSurface(uint32_t width, uint32_t height);
        void createDefaultTexture();
        void recreateTextureBindGroups();
        void recordQueuedPickingReadback();
        void beginRecordedPickingReadbackMaps();
        void processAsyncEvents() const;
        [[nodiscard]] std::shared_ptr<AsyncPickingReadbackSlot>
        acquirePickingReadbackSlot(uint64_t requiredSize);
        [[nodiscard]] bool tryConsumePickingReadback(
            Core::Renderer::PickingReadbackResult &result);
        [[nodiscard]] bool hasPickingReadbackWork() const noexcept;
        void resetPickingReadbacks() noexcept;
        [[nodiscard]] const TextureResource &
        getTexture(Core::Renderer::TextureHandle texture) const;
        [[nodiscard]] uint32_t primitiveStatsCount() const noexcept {
            return opaquePrimitiveBatch.count() +
                   transparentPrimitiveBatch.count();
        }
        [[nodiscard]] uint32_t customQuadStatsCount() const noexcept {
            return opaqueCustomQuadBatch.count() +
                   transparentCustomQuadBatch.count();
        }
        [[nodiscard]] uint32_t quadStatsCount() const noexcept {
            return primitiveStatsCount() + customQuadStatsCount() +
                   textBatch.count();
        }
    };

    void WgpuRenderer2D::Impl::createDevice() {
        struct RequestResult {
            wgpu::Adapter adapter;
            wgpu::Device device;
            std::string error;
        };

        wgpu::InstanceDescriptor instanceDescriptor{};
        instanceDescriptor.capabilities.timedWaitAnyEnable = true;
        instance = wgpu::CreateInstance(&instanceDescriptor);
        if (instance == nullptr) {
            throw std::runtime_error("Failed to create WebGPU instance");
        }

        wgpu::RequestAdapterOptions adapterOptions{};
        RequestResult adapterResult;
        auto adapterCallback =
            [&adapterResult](wgpu::RequestAdapterStatus status,
                             wgpu::Adapter adapter, wgpu::StringView message) {
                if (status != wgpu::RequestAdapterStatus::Success) {
                    adapterResult.error =
                        message.data != nullptr
                            ? std::string(message.data, message.length)
                            : "unknown adapter error";
                    return;
                }
                adapterResult.adapter = std::move(adapter);
            };

        instance.WaitAny(instance.RequestAdapter(
                             &adapterOptions, wgpu::CallbackMode::WaitAnyOnly,
                             adapterCallback),
                         UINT64_MAX);
        adapter = adapterResult.adapter;
        if (adapter == nullptr) {
            throw std::runtime_error("Failed to request WebGPU adapter: " +
                                     adapterResult.error);
        }

        wgpu::DeviceDescriptor deviceDescriptor{};
        deviceDescriptor.SetUncapturedErrorCallback(
            [](const wgpu::Device &, wgpu::ErrorType type,
               wgpu::StringView message) {
                BESS_ERROR("Dawn Validation Error [{}]: {}",
                           static_cast<int>(type),
                           std::string_view(message.data, message.length));
            });
        deviceDescriptor.SetDeviceLostCallback(
            wgpu::CallbackMode::AllowSpontaneous,
            [](const wgpu::Device &, wgpu::DeviceLostReason reason,
               wgpu::StringView message) {
                BESS_ERROR("Dawn Device Lost [{}]: {}",
                           static_cast<int>(reason),
                           std::string_view(message.data, message.length));
            });
        RequestResult deviceResult;
        auto deviceCallback = [&deviceResult](wgpu::RequestDeviceStatus status,
                                              wgpu::Device device,
                                              wgpu::StringView message) {
            if (status != wgpu::RequestDeviceStatus::Success) {
                deviceResult.error =
                    message.data != nullptr
                        ? std::string(message.data, message.length)
                        : "unknown device error";
                return;
            }
            deviceResult.device = std::move(device);
        };

        instance.WaitAny(adapter.RequestDevice(&deviceDescriptor,
                                               wgpu::CallbackMode::WaitAnyOnly,
                                               deviceCallback),
                         UINT64_MAX);
        device = deviceResult.device;
        if (device == nullptr) {
            throw std::runtime_error("Failed to request WebGPU device: " +
                                     deviceResult.error);
        }

        queue = device.GetQueue();
    }

    void WgpuRenderer2D::Impl::createOffscreenTarget() {
        wgpu::TextureDescriptor descriptor{};
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {std::max(1u, extent.width),
                           std::max(1u, extent.height), 1};
        descriptor.format = targetFormat;
        descriptor.mipLevelCount = 1;
        descriptor.sampleCount = 1;
        descriptor.usage = wgpu::TextureUsage::RenderAttachment |
                           wgpu::TextureUsage::TextureBinding |
                           wgpu::TextureUsage::CopySrc;
        descriptor.label = "OffscreenRenderTarget";

        offscreenTarget = device.CreateTexture(&descriptor);
        offscreenTargetView = offscreenTarget.CreateView();
    }

    void WgpuRenderer2D::Impl::createDepthTarget() {
        wgpu::TextureDescriptor descriptor{};
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {std::max(1u, extent.width),
                           std::max(1u, extent.height), 1};
        descriptor.format = kDepthStencilFormat;
        descriptor.mipLevelCount = 1;
        descriptor.sampleCount = 1;
        descriptor.usage = wgpu::TextureUsage::RenderAttachment;
        descriptor.label = "DepthRenderTarget";

        depthTarget = device.CreateTexture(&descriptor);
        depthTargetView = depthTarget.CreateView();
    }

    void WgpuRenderer2D::Impl::createDefaultTexture() {
        const std::array<uint8_t, 4> whitePixel{255, 255, 255, 255};
        defaultTexture = WgpuTexture::fromPixels(whitePixel.data(), 1, 1);
    }

    void WgpuRenderer2D::Impl::recreateTextureBindGroups() {
        if (!primitivePipeline) {
            return;
        }

        for (auto &[handle, texture] : textures) {
            if (!canUseAsPrimitiveSampledTexture(texture.format)) {
                texture.bindGroup = nullptr;
                continue;
            }
            texture.bindGroup = primitivePipeline->createTextureBindGroup(
                texture.view, "TextureBindGroup_" + std::to_string(handle));
        }
    }

    std::shared_ptr<AsyncPickingReadbackSlot>
    WgpuRenderer2D::Impl::acquirePickingReadbackSlot(uint64_t requiredSize) {
        for (size_t offset = 0; offset < pickingReadbackSlots.size();
             ++offset) {
            const size_t index = (nextPickingReadbackSlot + offset) %
                                 pickingReadbackSlots.size();
            auto &slot = pickingReadbackSlots[index];
            if (slot == nullptr) {
                slot = std::make_shared<AsyncPickingReadbackSlot>();
            }
            if (!slot->isReusable()) {
                continue;
            }

            if (slot->buffer == nullptr || slot->bufferSize < requiredSize) {
                wgpu::BufferDescriptor descriptor{};
                descriptor.size = requiredSize;
                descriptor.usage =
                    wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
                slot->buffer = device.CreateBuffer(&descriptor);
                slot->bufferSize = requiredSize;
            }

            nextPickingReadbackSlot = (index + 1) % pickingReadbackSlots.size();
            return slot;
        }
        return nullptr;
    }

    void WgpuRenderer2D::Impl::recordQueuedPickingReadback() {
        if (!queuedPickingReadback.queued) {
            return;
        }

        QueuedPickingReadback request = queuedPickingReadback;
        queuedPickingReadback.queued = false;

        if (request.resource.texture == nullptr ||
            request.resource.format != wgpu::TextureFormat::RG32Uint) {
            return;
        }

        const uint32_t bytesPerPixel =
            bytesPerPixelForFormat(request.resource.format);
        const uint32_t unpaddedBytesPerRow =
            request.region.width * bytesPerPixel;
        const uint32_t paddedBytesPerRow = alignTo(unpaddedBytesPerRow, 256);
        const uint64_t requiredSize =
            static_cast<uint64_t>(paddedBytesPerRow) * request.region.height;

        auto slot = acquirePickingReadbackSlot(requiredSize);
        if (slot == nullptr) {
            queuedPickingReadback = request;
            return;
        }

        slot->state = AsyncPickingReadbackSlot::State::CopyRecorded;
        slot->mappedSize = requiredSize;
        slot->x = request.region.x;
        slot->y = request.region.y;
        slot->width = request.region.width;
        slot->height = request.region.height;
        slot->paddedBytesPerRow = paddedBytesPerRow;
        slot->unpaddedBytesPerRow = unpaddedBytesPerRow;
        slot->sequence = request.sequence;
        slot->mapStatus = wgpu::MapAsyncStatus::Error;
        slot->mapError.clear();

        wgpu::TexelCopyTextureInfo source{};
        source.texture = request.resource.texture;
        source.mipLevel = 0;
        source.origin = {request.region.x, request.region.y, 0};
        source.aspect = wgpu::TextureAspect::All;

        wgpu::TexelCopyBufferInfo destination{};
        destination.buffer = slot->buffer;
        destination.layout.offset = 0;
        destination.layout.bytesPerRow = paddedBytesPerRow;
        destination.layout.rowsPerImage = request.region.height;

        wgpu::Extent3D copySize{request.region.width, request.region.height, 1};
        commandEncoder.CopyTextureToBuffer(&source, &destination, &copySize);
    }

    void WgpuRenderer2D::Impl::beginRecordedPickingReadbackMaps() {
        for (auto &slot : pickingReadbackSlots) {
            if (slot == nullptr ||
                slot->state != AsyncPickingReadbackSlot::State::CopyRecorded) {
                continue;
            }

            slot->state = AsyncPickingReadbackSlot::State::Mapping;
            auto callbackSlot = slot;
            slot->buffer.MapAsync(
                wgpu::MapMode::Read, 0, slot->mappedSize,
                wgpu::CallbackMode::AllowProcessEvents,
                [callbackSlot](wgpu::MapAsyncStatus status,
                               wgpu::StringView message) {
                    callbackSlot->mapStatus = status;
                    callbackSlot->mapError.clear();
                    if (status != wgpu::MapAsyncStatus::Success &&
                        message.data != nullptr) {
                        callbackSlot->mapError.assign(message.data,
                                                      message.length);
                    }
                    callbackSlot->state =
                        status == wgpu::MapAsyncStatus::Success
                            ? AsyncPickingReadbackSlot::State::Ready
                            : AsyncPickingReadbackSlot::State::Failed;
                });
        }
    }

    void WgpuRenderer2D::Impl::processAsyncEvents() const {
        if (instance != nullptr) {
            instance.ProcessEvents();
        }
    }

    bool WgpuRenderer2D::Impl::tryConsumePickingReadback(
        Core::Renderer::PickingReadbackResult &result) {
        processAsyncEvents();

        std::shared_ptr<AsyncPickingReadbackSlot> newestReady;
        for (auto &slot : pickingReadbackSlots) {
            if (slot == nullptr) {
                continue;
            }
            if (slot->state == AsyncPickingReadbackSlot::State::Failed) {
                slot->state = AsyncPickingReadbackSlot::State::Idle;
                continue;
            }
            if (slot->state != AsyncPickingReadbackSlot::State::Ready) {
                continue;
            }
            if (slot->sequence <= lastDeliveredPickingReadbackSequence) {
                slot->buffer.Unmap();
                slot->state = AsyncPickingReadbackSlot::State::Idle;
                continue;
            }
            if (newestReady == nullptr ||
                slot->sequence > newestReady->sequence) {
                newestReady = slot;
            }
        }

        if (newestReady == nullptr) {
            return false;
        }

        for (auto &slot : pickingReadbackSlots) {
            if (slot == nullptr || slot == newestReady ||
                slot->state != AsyncPickingReadbackSlot::State::Ready) {
                continue;
            }
            if (slot->sequence < newestReady->sequence) {
                slot->buffer.Unmap();
                slot->state = AsyncPickingReadbackSlot::State::Idle;
            }
        }

        const auto *mappedData = static_cast<const uint8_t *>(
            newestReady->buffer.GetConstMappedRange(0,
                                                    newestReady->mappedSize));
        if (mappedData == nullptr) {
            newestReady->buffer.Unmap();
            newestReady->state = AsyncPickingReadbackSlot::State::Idle;
            return false;
        }

        result = {};
        result.x = newestReady->x;
        result.y = newestReady->y;
        result.width = newestReady->width;
        result.height = newestReady->height;
        const size_t pixelCount = static_cast<size_t>(result.width) *
                                  static_cast<size_t>(result.height);
        result.ids.resize(pixelCount);

        for (uint32_t row = 0; row < result.height; ++row) {
            const auto *srcRow =
                mappedData +
                static_cast<size_t>(row) * newestReady->paddedBytesPerRow;
            for (uint32_t col = 0; col < result.width; ++col) {
                const size_t pixelIndex =
                    static_cast<size_t>(row) * result.width + col;
                const auto *src =
                    srcRow + static_cast<size_t>(col) * sizeof(uint32_t) * 2;
                PickingId id{};
                std::memcpy(&id.runtimeId, src, sizeof(uint32_t));
                std::memcpy(&id.info, src + sizeof(uint32_t), sizeof(uint32_t));
                result.ids[pixelIndex] = id;
            }
        }

        newestReady->buffer.Unmap();
        newestReady->state = AsyncPickingReadbackSlot::State::Idle;
        lastDeliveredPickingReadbackSequence = newestReady->sequence;
        return true;
    }

    bool WgpuRenderer2D::Impl::hasPickingReadbackWork() const noexcept {
        if (queuedPickingReadback.queued) {
            return true;
        }
        for (const auto &slot : pickingReadbackSlots) {
            if (slot == nullptr) {
                continue;
            }
            if (slot->state != AsyncPickingReadbackSlot::State::Idle &&
                slot->state != AsyncPickingReadbackSlot::State::Failed) {
                return true;
            }
        }
        return false;
    }

    void WgpuRenderer2D::Impl::resetPickingReadbacks() noexcept {
        queuedPickingReadback = {};
        for (auto &slot : pickingReadbackSlots) {
            if (slot != nullptr &&
                (slot->state == AsyncPickingReadbackSlot::State::Mapping ||
                 slot->state == AsyncPickingReadbackSlot::State::Ready)) {
                slot->buffer.Unmap();
            }
            slot = nullptr;
        }
        nextPickingReadbackSlot = 0;
        lastDeliveredPickingReadbackSequence = 0;
    }

    const TextureResource &WgpuRenderer2D::Impl::getTexture(
        Core::Renderer::TextureHandle texture) const {
        if (texture == 0) {
            return textures.at(defaultTexture->getHandle());
        }

        BESS_ASSERT(!textures.empty(), "No textures available in renderer");
        const auto it = textures.find(texture);
        if (it != textures.end()) {
            return it->second;
        }

        BESS_ASSERT(false, "Requested texture handle {} not found in renderer",
                    texture);
        return textures.at(defaultTexture->getHandle());
    }

    WgpuRenderer2D::WgpuRenderer2D() : m_impl(std::make_unique<Impl>()) {}

    WgpuRenderer2D::~WgpuRenderer2D() { destroy(); }

    void WgpuRenderer2D::init(
        const Core::Renderer::Renderer2DCreateInfo &createInfo) {
        destroy();
        m_impl = std::make_unique<Impl>();
        m_impl->createInfo = createInfo;
        m_impl->extent = createInfo.extent;
        m_impl->targetFormatType = createInfo.targetFormat;
        m_impl->targetFormat = toWgpuFormat(createInfo.targetFormat);
        if (createInfo.surface.type ==
            Core::Renderer::Renderer2DNativeSurfaceType::PlatformHandle) {
            m_impl->windowHandle =
                static_cast<GLFWwindow *>(createInfo.surface.handle);
        }
        m_impl->opaquePrimitiveBatch.configure(
            createInfo.batching.initialQuadCapacity,
            createInfo.batching.maxQuadCapacity);
        m_impl->transparentPrimitiveBatch.configure(
            createInfo.batching.initialQuadCapacity,
            createInfo.batching.maxQuadCapacity);
        m_impl->opaqueCustomQuadBatch.configure(
            createInfo.batching.initialQuadCapacity,
            createInfo.batching.maxQuadCapacity);
        m_impl->transparentCustomQuadBatch.configure(
            createInfo.batching.initialQuadCapacity,
            createInfo.batching.maxQuadCapacity);
        m_impl->createDevice();
        m_impl->createWindowSurface();
        m_impl->createOffscreenTarget();
        m_impl->createDepthTarget();
        m_impl->sharedFrameBuffer.init(m_impl->device);
        m_impl->pickingFormat = toWgpuFormat(createInfo.pickingFormat);
        m_impl->primitivePipeline =
            std::make_unique<Piplines::PrimitivePipeline>();
        m_impl->primitivePipeline->init(m_impl->device, m_impl->targetFormat,
                                        m_impl->sharedFrameBuffer.getBuffer(),
                                        m_impl->sharedFrameBuffer.getSize(),
                                        m_impl->pickingFormat);
        m_impl->pathPipeline = std::make_unique<Piplines::PathPipeline>();
        m_impl->pathPipeline->init(m_impl->device, m_impl->targetFormat,
                                   m_impl->sharedFrameBuffer.getBuffer(),
                                   m_impl->sharedFrameBuffer.getSize(),
                                   m_impl->pickingFormat);
        m_impl->customQuadPipeline = std::make_unique<CustomQuadPipeline>();
        m_impl->customQuadPipeline->init(m_impl->device, m_impl->targetFormat,
                                         m_impl->sharedFrameBuffer.getBuffer(),
                                         m_impl->sharedFrameBuffer.getSize(),
                                         m_impl->pickingFormat);
        if (m_impl->primitivePipeline->ensureInstanceBufferSize(
                std::max(1u, createInfo.batching.initialQuadCapacity))) {
            m_impl->recreateTextureBindGroups();
        }
        static_cast<void>(m_impl->customQuadPipeline->ensureInstanceBufferSize(
            std::max(1u, createInfo.batching.initialQuadCapacity)));
        m_impl->createDefaultTexture();

        m_impl->msdfFontAtlas = std::make_unique<MsdfFontAtlas>();
        if (m_impl->msdfFontAtlas->load(kDefaultMsdfFontDirectory,
                                        kDefaultMsdfFontName)) {
            m_impl->textPipeline = std::make_unique<MsdfTextPipeline>();
            m_impl->textPipeline->init(
                m_impl->device, m_impl->targetFormat,
                m_impl->sharedFrameBuffer.getBuffer(),
                m_impl->sharedFrameBuffer.getSize(), m_impl->pickingFormat,
                m_impl->msdfFontAtlas->textureResource());
            static_cast<void>(m_impl->textPipeline->ensureInstanceBufferSize(
                std::max(1u, createInfo.batching.initialQuadCapacity)));
        } else {
            m_impl->msdfFontAtlas = nullptr;
            BESS_WARN("[WgpuRenderer2D] MSDF font atlas unavailable; falling "
                      "back to outline text rendering");
        }

        const std::string fontPath = createInfo.fontFile.empty()
                                         ? kDefaultFontFile
                                         : createInfo.fontFile;
        m_impl->fontFile = std::make_unique<FontFile>(fontPath);
        if (!m_impl->fontFile->isValid() ||
            !m_impl->fontFile->init(kFontOutlinePixelSize, 0, 255)) {
            BESS_WARN("[WgpuRenderer2D] Failed to initialize font file: {}",
                      fontPath);
            m_impl->fontFile = nullptr;
        }
    }

    void WgpuRenderer2D::destroy() {
        if (m_impl == nullptr) {
            return;
        }
        m_impl->commandEncoder = nullptr;
        m_impl->resetPickingReadbacks();
        if (m_impl->primitivePipeline) {
            m_impl->primitivePipeline->destroy();
            m_impl->primitivePipeline = nullptr;
        }
        if (m_impl->pathPipeline) {
            m_impl->pathPipeline->destroy();
            m_impl->pathPipeline = nullptr;
        }
        if (m_impl->customQuadPipeline) {
            m_impl->customQuadPipeline->destroy();
            m_impl->customQuadPipeline = nullptr;
        }
        if (m_impl->textPipeline) {
            m_impl->textPipeline->destroy();
            m_impl->textPipeline = nullptr;
        }
        m_impl->msdfFontAtlas = nullptr;
        m_impl->sharedFrameBuffer.destroy();
        m_impl->textures.clear();
        m_impl->surface = nullptr;
        m_impl->surfaceConfigured = false;
        m_impl->frameUsesSurface = false;
        m_impl->frameTargetTexture = 0;
        m_impl->lastCompletedTargetTexture = 0;
        m_impl->depthTargetView = nullptr;
        m_impl->depthTarget = nullptr;
        m_impl->offscreenTargetView = nullptr;
        m_impl->offscreenTarget = nullptr;
        m_impl->queue = nullptr;
        m_impl->device = nullptr;
        m_impl->adapter = nullptr;
        m_impl->instance = nullptr;
        m_impl->opaquePrimitiveBatch.clear();
        m_impl->transparentPrimitiveBatch.clear();
        m_impl->opaqueCustomQuadBatch.clear();
        m_impl->transparentCustomQuadBatch.clear();
        m_impl->opaquePathStrokeBatch.clear();
        m_impl->transparentPathStrokeBatch.clear();
        m_impl->opaquePathBatch.clear();
        m_impl->transparentPathBatch.clear();
        m_impl->textBatch.clear();
        m_impl->activePathCommands.clear();
        m_impl->textPathCommandsScratch.clear();
        m_impl->fontFile = nullptr;
        m_impl->pathStarted = false;
        m_impl->stats = {};
        m_impl->frameStarted = false;
    }

    void WgpuRenderer2D::Impl::createWindowSurface() {
        if (windowHandle == nullptr) {
            return;
        }

        surface = wgpu::Surface(
            glfwCreateWindowWGPUSurface(instance.Get(), windowHandle));
        if (surface == nullptr) {
            throw std::runtime_error("Failed to create WebGPU surface");
        }

        wgpu::SurfaceCapabilities capabilities;
        surface.GetCapabilities(adapter, &capabilities);
        if (capabilities.formatCount == 0 ||
            capabilities.presentModeCount == 0 ||
            capabilities.alphaModeCount == 0) {
            throw std::runtime_error(
                "WebGPU surface reports no supported configuration");
        }

        surfaceFormat = capabilities.formats[0];
        surfacePresentMode = capabilities.presentModes[0];
        surfaceAlphaMode = capabilities.alphaModes[0];
    }

    void WgpuRenderer2D::Impl::configureWindowSurface(uint32_t width,
                                                      uint32_t height) {
        if (surface == nullptr || device == nullptr) {
            return;
        }

        surfaceConfiguration.device = device;
        surfaceConfiguration.usage = wgpu::TextureUsage::RenderAttachment;
        surfaceConfiguration.format = surfaceFormat;
        surfaceConfiguration.presentMode = surfacePresentMode;
        surfaceConfiguration.alphaMode = surfaceAlphaMode;
        surfaceConfiguration.width = std::max(1u, width);
        surfaceConfiguration.height = std::max(1u, height);
        surfaceConfiguration.viewFormatCount = 0;
        surfaceConfiguration.viewFormats = nullptr;

        surface.Configure(&surfaceConfiguration);
        surfaceConfigured = true;
    }

    void WgpuRenderer2D::resize(const Renderer2DExtent &extent) {
        m_impl->extent = extent;
        if (m_impl->device != nullptr) {
            m_impl->createOffscreenTarget();
            m_impl->createDepthTarget();
        }
    }

    void WgpuRenderer2D::beginFrame(
        const Core::Renderer::Renderer2DFrameInfo &frameInfo) {
        if (m_impl->device == nullptr) {
            throw std::runtime_error("WgpuRenderer2D is not initialized");
        }
        m_impl->processAsyncEvents();

        if (frameInfo.extent.width != 0 && frameInfo.extent.height != 0 &&
            (frameInfo.extent.width != m_impl->extent.width ||
             frameInfo.extent.height != m_impl->extent.height)) {
            resize(frameInfo.extent);
        }

        m_impl->clearColor = frameInfo.clearColor;
        m_impl->shouldClear = frameInfo.shouldClear;
        m_impl->opaquePrimitiveBatch.clear();
        m_impl->transparentPrimitiveBatch.clear();
        m_impl->opaqueCustomQuadBatch.clear();
        m_impl->transparentCustomQuadBatch.clear();
        m_impl->opaquePathStrokeBatch.clear();
        m_impl->transparentPathStrokeBatch.clear();
        m_impl->opaquePathBatch.clear();
        m_impl->transparentPathBatch.clear();
        m_impl->textBatch.clear();
        m_impl->activePathCommands.clear();
        m_impl->pathStarted = false;
        m_impl->stats = {};
        m_impl->cameraTransform = nullptr;

        m_impl->frameTargetTexture = frameInfo.targetTexture;
        m_impl->pickingTextureHandle = frameInfo.pickingTexture;
        m_impl->frameUsesSurface = frameInfo.targetTexture == 0;
        m_impl->frameStarted = true;
        m_impl->cameraTransform = frameInfo.cameraTransform;
    }

    void WgpuRenderer2D::endFrame() {
        if (!m_impl->frameStarted) {
            return;
        }

        if (m_impl->pathStarted) {
            endPath();
        }

        m_impl->commandEncoder = m_impl->device.CreateCommandEncoder();

        wgpu::SurfaceTexture surfaceTexture{};
        wgpu::TextureView targetView;

        if (m_impl->frameUsesSurface) {
            m_impl->surface.GetCurrentTexture(&surfaceTexture);
            if (surfaceTexture.status !=
                wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal) {
                m_impl->commandEncoder = nullptr;
                m_impl->frameStarted = false;
                return;
            }
            targetView = surfaceTexture.texture.CreateView();
        } else if (m_impl->frameTargetTexture != 0) {
            targetView = m_impl->getTexture(m_impl->frameTargetTexture).view;
        } else {
            targetView = m_impl->offscreenTargetView;
        }

        wgpu::RenderPassColorAttachment colorAttachments[2]{};
        uint32_t colorAttachmentCount = 1;

        colorAttachments[0].view = targetView;
        colorAttachments[0].loadOp =
            m_impl->shouldClear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        colorAttachments[0].storeOp = wgpu::StoreOp::Store;
        colorAttachments[0].clearValue = toWgpuColor(m_impl->clearColor);

        // Attach picking target if available
        if (m_impl->pickingFormat != wgpu::TextureFormat::Undefined &&
            m_impl->pickingTextureHandle != 0) {
            const auto &pickingRes =
                m_impl->getTexture(m_impl->pickingTextureHandle);
            colorAttachments[1].view = pickingRes.view;
            colorAttachments[1].loadOp = wgpu::LoadOp::Clear;
            colorAttachments[1].storeOp = wgpu::StoreOp::Store;
            colorAttachments[1].clearValue = {
                static_cast<double>(PickingId::invalidRuntimeId), 0.0, 0.0,
                0.0};
            colorAttachmentCount = 2;
        }

        wgpu::RenderPassDepthStencilAttachment depthAttachment{};
        depthAttachment.view = m_impl->depthTargetView;
        depthAttachment.depthLoadOp =
            m_impl->shouldClear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        depthAttachment.depthStoreOp = wgpu::StoreOp::Store;
        depthAttachment.depthClearValue = 1.0f;
        depthAttachment.stencilLoadOp = wgpu::LoadOp::Clear;
        depthAttachment.stencilStoreOp = wgpu::StoreOp::Store;
        depthAttachment.stencilClearValue = 0;

        wgpu::RenderPassDescriptor renderPassDescriptor{};
        renderPassDescriptor.colorAttachmentCount = colorAttachmentCount;
        renderPassDescriptor.colorAttachments = colorAttachments;
        renderPassDescriptor.depthStencilAttachment = &depthAttachment;

        m_impl->opaquePrimitiveBatch.prepareForRendering(false);
        m_impl->transparentPrimitiveBatch.prepareForRendering(true);
        m_impl->opaqueCustomQuadBatch.prepareForRendering(false);
        m_impl->transparentCustomQuadBatch.prepareForRendering(true);
        m_impl->opaquePathStrokeBatch.prepareForRendering(false);
        m_impl->transparentPathStrokeBatch.prepareForRendering(true);
        m_impl->opaquePathBatch.prepareForRendering(false);
        m_impl->transparentPathBatch.prepareForRendering(true);
        m_impl->textBatch.prepareForRendering();

        const uint32_t opaqueInstanceOffset = 0;
        const uint32_t transparentInstanceOffset =
            m_impl->opaquePrimitiveBatch.count();
        const uint32_t totalInstanceCount =
            transparentInstanceOffset +
            m_impl->transparentPrimitiveBatch.count();

        const uint32_t opaqueCustomInstanceOffset = 0;
        const uint32_t transparentCustomInstanceOffset =
            m_impl->opaqueCustomQuadBatch.count();
        const uint32_t totalCustomInstanceCount =
            transparentCustomInstanceOffset +
            m_impl->transparentCustomQuadBatch.count();

        const uint32_t opaqueStencilVertexOffset = 0;
        const uint32_t transparentStencilVertexOffset =
            m_impl->opaquePathBatch.stencilVertexCount();
        const uint32_t totalStencilVertexCount =
            transparentStencilVertexOffset +
            m_impl->transparentPathBatch.stencilVertexCount();

        const uint32_t opaqueCoverVertexOffset = 0;
        const uint32_t transparentCoverVertexOffset =
            m_impl->opaquePathBatch.coverVertexCount();
        const uint32_t totalCoverVertexCount =
            transparentCoverVertexOffset +
            m_impl->transparentPathBatch.coverVertexCount();

        const uint32_t opaqueStrokeVertexOffset = 0;
        const uint32_t transparentStrokeVertexOffset =
            m_impl->opaquePathStrokeBatch.vertexCount();
        const uint32_t totalStrokeVertexCount =
            transparentStrokeVertexOffset +
            m_impl->transparentPathStrokeBatch.vertexCount();

        const uint32_t totalTextGlyphCount = m_impl->textBatch.count();

        if (totalInstanceCount > 0 &&
            m_impl->primitivePipeline->ensureInstanceBufferSize(
                totalInstanceCount)) {
            m_impl->recreateTextureBindGroups();
        }

        if (totalCustomInstanceCount > 0) {
            static_cast<void>(
                m_impl->customQuadPipeline->ensureInstanceBufferSize(
                    totalCustomInstanceCount));
        }

        if (totalTextGlyphCount > 0 && m_impl->textPipeline != nullptr) {
            static_cast<void>(m_impl->textPipeline->ensureInstanceBufferSize(
                totalTextGlyphCount));
        }

        if (!m_impl->opaquePrimitiveBatch.empty()) {
            m_impl->primitivePipeline->uploadInstances(
                m_impl->queue, m_impl->opaquePrimitiveBatch.data(),
                m_impl->opaquePrimitiveBatch.byteSize(),
                opaqueInstanceOffset * sizeof(Piplines::PrimitiveInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->opaquePrimitiveBatch.byteSize();
        }

        if (!m_impl->transparentPrimitiveBatch.empty()) {
            m_impl->primitivePipeline->uploadInstances(
                m_impl->queue, m_impl->transparentPrimitiveBatch.data(),
                m_impl->transparentPrimitiveBatch.byteSize(),
                transparentInstanceOffset *
                    sizeof(Piplines::PrimitiveInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->transparentPrimitiveBatch.byteSize();
        }

        if (!m_impl->opaqueCustomQuadBatch.empty()) {
            m_impl->customQuadPipeline->uploadInstances(
                m_impl->queue, m_impl->opaqueCustomQuadBatch.data(),
                m_impl->opaqueCustomQuadBatch.byteSize(),
                opaqueCustomInstanceOffset * sizeof(CustomQuadInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->opaqueCustomQuadBatch.byteSize();
        }

        if (!m_impl->transparentCustomQuadBatch.empty()) {
            m_impl->customQuadPipeline->uploadInstances(
                m_impl->queue, m_impl->transparentCustomQuadBatch.data(),
                m_impl->transparentCustomQuadBatch.byteSize(),
                transparentCustomInstanceOffset * sizeof(CustomQuadInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->transparentCustomQuadBatch.byteSize();
        }

        if (!m_impl->textBatch.empty() && m_impl->textPipeline != nullptr) {
            m_impl->textPipeline->uploadInstances(m_impl->queue,
                                                  m_impl->textBatch.data(),
                                                  m_impl->textBatch.byteSize());
            m_impl->stats.uploadedBytes += m_impl->textBatch.byteSize();
        }

        m_impl->stats.quadCount =
            totalInstanceCount + totalCustomInstanceCount + totalTextGlyphCount;

        if (totalStencilVertexCount > 0) {
            static_cast<void>(
                m_impl->pathPipeline->ensureStencilVertexBufferSize(
                    totalStencilVertexCount));
        }

        if (totalCoverVertexCount > 0) {
            static_cast<void>(m_impl->pathPipeline->ensureCoverVertexBufferSize(
                totalCoverVertexCount));
        }

        if (totalStrokeVertexCount > 0) {
            static_cast<void>(
                m_impl->pathPipeline->ensureStrokeVertexBufferSize(
                    totalStrokeVertexCount));
        }

        if (!m_impl->opaquePathBatch.empty()) {
            m_impl->pathPipeline->uploadStencilVertices(
                m_impl->queue, m_impl->opaquePathBatch.stencilData(),
                m_impl->opaquePathBatch.stencilByteSize(),
                opaqueStencilVertexOffset * sizeof(PathStencilVertex));
            m_impl->pathPipeline->uploadCoverVertices(
                m_impl->queue, m_impl->opaquePathBatch.coverData(),
                m_impl->opaquePathBatch.coverByteSize(),
                opaqueCoverVertexOffset * sizeof(PathCoverVertex));
            m_impl->stats.uploadedBytes +=
                m_impl->opaquePathBatch.stencilByteSize() +
                m_impl->opaquePathBatch.coverByteSize();
        }

        if (!m_impl->transparentPathBatch.empty()) {
            m_impl->pathPipeline->uploadStencilVertices(
                m_impl->queue, m_impl->transparentPathBatch.stencilData(),
                m_impl->transparentPathBatch.stencilByteSize(),
                transparentStencilVertexOffset * sizeof(PathStencilVertex));
            m_impl->pathPipeline->uploadCoverVertices(
                m_impl->queue, m_impl->transparentPathBatch.coverData(),
                m_impl->transparentPathBatch.coverByteSize(),
                transparentCoverVertexOffset * sizeof(PathCoverVertex));
            m_impl->stats.uploadedBytes +=
                m_impl->transparentPathBatch.stencilByteSize() +
                m_impl->transparentPathBatch.coverByteSize();
        }

        if (!m_impl->opaquePathStrokeBatch.empty()) {
            m_impl->pathPipeline->uploadStrokeVertices(
                m_impl->queue, m_impl->opaquePathStrokeBatch.data(),
                m_impl->opaquePathStrokeBatch.byteSize(),
                opaqueStrokeVertexOffset * sizeof(PathCoverVertex));
            m_impl->stats.uploadedBytes +=
                m_impl->opaquePathStrokeBatch.byteSize();
        }

        if (!m_impl->transparentPathStrokeBatch.empty()) {
            m_impl->pathPipeline->uploadStrokeVertices(
                m_impl->queue, m_impl->transparentPathStrokeBatch.data(),
                m_impl->transparentPathStrokeBatch.byteSize(),
                transparentStrokeVertexOffset * sizeof(PathCoverVertex));
            m_impl->stats.uploadedBytes +=
                m_impl->transparentPathStrokeBatch.byteSize();
        }

        m_impl->sharedFrameBuffer.setCameraTransform(m_impl->cameraTransform);
        m_impl->sharedFrameBuffer.update(m_impl->queue, m_impl->extent.width,
                                         m_impl->extent.height);

        wgpu::RenderPassEncoder renderPass =
            m_impl->commandEncoder.BeginRenderPass(&renderPassDescriptor);

        auto renderBatch = [&](PrimitiveBatch &batch, uint32_t instanceOffset,
                               const wgpu::RenderPipeline &pipeline) {
            if (batch.empty()) {
                return;
            }

            renderPass.SetPipeline(pipeline);

            const uint32_t runCount = batch.drawRunsCount();
            const DrawRun *runs = batch.drawRunsData();
            for (uint32_t i = 0; i < runCount; ++i) {
                const auto &run = runs[i];
                const auto &texture = m_impl->getTexture(run.texture);
                renderPass.SetBindGroup(0, texture.bindGroup);
                renderPass.Draw(6, run.instanceCount, 0,
                                instanceOffset + run.firstInstance);
                m_impl->stats.drawCallCount++;
            }
        };

        auto renderPrimitiveRun = [&](const DrawRun &run,
                                      uint32_t instanceOffset,
                                      const wgpu::RenderPipeline &pipeline) {
            if (run.instanceCount == 0) {
                return;
            }

            renderPass.SetPipeline(pipeline);
            const auto &texture = m_impl->getTexture(run.texture);
            renderPass.SetBindGroup(0, texture.bindGroup);
            renderPass.Draw(6, run.instanceCount, 0,
                            instanceOffset + run.firstInstance);
            m_impl->stats.drawCallCount++;
        };

        auto renderCustomQuadBatch = [&](const CustomQuadBatch &batch,
                                         uint32_t instanceOffset,
                                         bool transparent) {
            if (batch.empty()) {
                return;
            }

            const uint32_t runCount = batch.drawRunsCount();
            const CustomQuadDrawRun *runs = batch.drawRunsData();
            for (uint32_t i = 0; i < runCount; ++i) {
                const auto &run = runs[i];
                m_impl->customQuadPipeline->draw(
                    renderPass, run.shader, instanceOffset + run.firstInstance,
                    run.instanceCount, transparent);
                m_impl->stats.drawCallCount++;
            }
        };

        auto renderCustomQuadRun = [&](const CustomQuadDrawRun &run,
                                       uint32_t instanceOffset,
                                       bool transparent) {
            if (run.instanceCount == 0) {
                return;
            }

            m_impl->customQuadPipeline->draw(renderPass, run.shader,
                                             instanceOffset + run.firstInstance,
                                             run.instanceCount, transparent);
            m_impl->stats.drawCallCount++;
        };

        auto renderPathBatch = [&](const PathBatch &batch,
                                   uint32_t stencilVertexOffset,
                                   uint32_t coverVertexOffset,
                                   bool transparent) {
            if (batch.empty()) {
                return;
            }

            const PathDrawRange *ranges = batch.drawRanges();
            const uint32_t rangeCount = batch.drawCount();
            for (uint32_t i = 0; i < rangeCount; ++i) {
                const auto &range = ranges[i];
                m_impl->pathPipeline->drawPath(
                    renderPass, stencilVertexOffset + range.firstStencilVertex,
                    range.stencilVertexCount,
                    coverVertexOffset + range.firstCoverVertex,
                    range.coverVertexCount, transparent, range.evenOddFill);
                m_impl->stats.drawCallCount += 2;
            }
        };

        auto renderPathRange =
            [&](const PathDrawRange &range, uint32_t stencilVertexOffset,
                uint32_t coverVertexOffset, bool transparent) {
                m_impl->pathPipeline->drawPath(
                    renderPass, stencilVertexOffset + range.firstStencilVertex,
                    range.stencilVertexCount,
                    coverVertexOffset + range.firstCoverVertex,
                    range.coverVertexCount, transparent, range.evenOddFill);
                m_impl->stats.drawCallCount += 2;
            };

        auto renderPathStrokeBatch = [&](const PathStrokeBatch &batch,
                                         uint32_t vertexOffset,
                                         bool transparent) {
            if (batch.empty()) {
                return;
            }

            const PathStrokeDrawRange *ranges = batch.drawRanges();
            const uint32_t rangeCount = batch.drawCount();
            for (uint32_t i = 0; i < rangeCount; ++i) {
                const auto &range = ranges[i];
                m_impl->pathPipeline->drawStroke(
                    renderPass, vertexOffset + range.firstVertex,
                    range.vertexCount, transparent);
                m_impl->stats.drawCallCount++;
            }
        };

        auto renderPathStrokeRange = [&](const PathStrokeDrawRange &range,
                                         uint32_t vertexOffset,
                                         bool transparent) {
            m_impl->pathPipeline->drawStroke(renderPass,
                                             vertexOffset + range.firstVertex,
                                             range.vertexCount, transparent);
            m_impl->stats.drawCallCount++;
        };

        auto renderTextRun = [&](const TextDrawRun &run) {
            if (run.glyphCount == 0 || m_impl->textPipeline == nullptr) {
                return;
            }

            m_impl->textPipeline->draw(renderPass, run.firstGlyph,
                                       run.glyphCount);
            m_impl->stats.drawCallCount++;
        };

        renderBatch(m_impl->opaquePrimitiveBatch, opaqueInstanceOffset,
                    m_impl->primitivePipeline->getOpaquePipeline());
        renderCustomQuadBatch(m_impl->opaqueCustomQuadBatch,
                              opaqueCustomInstanceOffset, false);
        renderPathBatch(m_impl->opaquePathBatch, opaqueStencilVertexOffset,
                        opaqueCoverVertexOffset, false);
        renderPathStrokeBatch(m_impl->opaquePathStrokeBatch,
                              opaqueStrokeVertexOffset, false);

        m_impl->transparentDrawItems.clear();
        m_impl->transparentDrawItems.reserve(
            m_impl->transparentPrimitiveBatch.drawRunsCount() +
            m_impl->transparentCustomQuadBatch.drawRunsCount() +
            m_impl->transparentPathBatch.drawCount() +
            m_impl->transparentPathStrokeBatch.drawCount() +
            m_impl->textBatch.drawRunsCount());

        uint32_t transparentDrawOrder = 0;
        const DrawRun *transparentPrimitiveRuns =
            m_impl->transparentPrimitiveBatch.drawRunsData();
        for (uint32_t i = 0;
             i < m_impl->transparentPrimitiveBatch.drawRunsCount(); ++i) {
            m_impl->transparentDrawItems.push_back({
                .kind = TransparentDrawKind::Primitive,
                .zIndex = transparentPrimitiveRuns[i].zIndex,
                .index = i,
                .order = transparentDrawOrder++,
            });
        }

        const CustomQuadDrawRun *transparentCustomQuadRuns =
            m_impl->transparentCustomQuadBatch.drawRunsData();
        for (uint32_t i = 0;
             i < m_impl->transparentCustomQuadBatch.drawRunsCount(); ++i) {
            m_impl->transparentDrawItems.push_back({
                .kind = TransparentDrawKind::CustomQuad,
                .zIndex = transparentCustomQuadRuns[i].zIndex,
                .index = i,
                .order = transparentDrawOrder++,
            });
        }

        const PathDrawRange *transparentPathRanges =
            m_impl->transparentPathBatch.drawRanges();
        for (uint32_t i = 0; i < m_impl->transparentPathBatch.drawCount();
             ++i) {
            m_impl->transparentDrawItems.push_back({
                .kind = TransparentDrawKind::PathFill,
                .zIndex = transparentPathRanges[i].zIndex,
                .index = i,
                .order = transparentDrawOrder++,
            });
        }

        const PathStrokeDrawRange *transparentPathStrokeRanges =
            m_impl->transparentPathStrokeBatch.drawRanges();
        for (uint32_t i = 0; i < m_impl->transparentPathStrokeBatch.drawCount();
             ++i) {
            m_impl->transparentDrawItems.push_back({
                .kind = TransparentDrawKind::PathStroke,
                .zIndex = transparentPathStrokeRanges[i].zIndex,
                .index = i,
                .order = transparentDrawOrder++,
            });
        }

        const TextDrawRun *textRuns = m_impl->textBatch.drawRunsData();
        for (uint32_t i = 0; i < m_impl->textBatch.drawRunsCount(); ++i) {
            m_impl->transparentDrawItems.push_back({
                .kind = TransparentDrawKind::Text,
                .zIndex = textRuns[i].zIndex,
                .index = i,
                .order = transparentDrawOrder++,
            });
        }

        std::stable_sort(
            m_impl->transparentDrawItems.begin(),
            m_impl->transparentDrawItems.end(),
            [](const TransparentDrawItem &a, const TransparentDrawItem &b) {
                if (a.zIndex != b.zIndex) {
                    return a.zIndex < b.zIndex;
                }
                return a.order < b.order;
            });

        for (const TransparentDrawItem &item : m_impl->transparentDrawItems) {
            switch (item.kind) {
            case TransparentDrawKind::Primitive:
                renderPrimitiveRun(
                    transparentPrimitiveRuns[item.index],
                    transparentInstanceOffset,
                    m_impl->primitivePipeline->getTransparentPipeline());
                break;
            case TransparentDrawKind::CustomQuad:
                renderCustomQuadRun(transparentCustomQuadRuns[item.index],
                                    transparentCustomInstanceOffset, true);
                break;
            case TransparentDrawKind::PathFill:
                renderPathRange(transparentPathRanges[item.index],
                                transparentStencilVertexOffset,
                                transparentCoverVertexOffset, true);
                break;
            case TransparentDrawKind::PathStroke:
                renderPathStrokeRange(transparentPathStrokeRanges[item.index],
                                      transparentStrokeVertexOffset, true);
                break;
            case TransparentDrawKind::Text:
                renderTextRun(textRuns[item.index]);
                break;
            }
        }

        renderPass.End();
        m_impl->recordQueuedPickingReadback();

        wgpu::CommandBuffer commandBuffer = m_impl->commandEncoder.Finish();
        m_impl->queue.Submit(1, &commandBuffer);
        m_impl->beginRecordedPickingReadbackMaps();

        if (m_impl->frameUsesSurface) {
            m_impl->surface.Present();
        }

        m_impl->commandEncoder = nullptr;
        m_impl->frameStarted = false;
        m_impl->lastCompletedTargetTexture = m_impl->frameTargetTexture;
        m_impl->frameTargetTexture = 0;
        m_impl->frameUsesSurface = false;
    }

    void WgpuRenderer2D::clear(const Color &color) {
        m_impl->clearColor = color;
        m_impl->shouldClear = true;
    }

    Core::Renderer::TextureReadbackResult WgpuRenderer2D::readTexture(
        const Core::Renderer::TextureReadbackRegion &region) {
        if (m_impl->device == nullptr) {
            throw std::runtime_error("WgpuRenderer2D is not initialized");
        }

        if (m_impl->frameStarted) {
            endFrame();
        }

        if (region.texture == 0) {
            throw std::runtime_error(
                "readTexture requires a non-zero texture handle");
        }

        const auto &resource = m_impl->getTexture(region.texture);
        return readTextureRegion(
            m_impl->instance, m_impl->device, m_impl->queue, resource.texture,
            resource.format, resource.width, resource.height, region);
    }

    void WgpuRenderer2D::requestPickingIds(
        const Core::Renderer::TextureReadbackRegion &region) {
        if (m_impl->device == nullptr) {
            throw std::runtime_error("WgpuRenderer2D is not initialized");
        }
        if (region.texture == 0) {
            throw std::runtime_error(
                "requestPickingIds requires a non-zero texture handle");
        }
        if (region.width == 0 || region.height == 0) {
            throw std::runtime_error(
                "requestPickingIds region must be non-empty");
        }

        const auto &resource = m_impl->getTexture(region.texture);
        if (resource.format != wgpu::TextureFormat::RG32Uint) {
            throw std::runtime_error(
                "requestPickingIds requires an RG32Uint texture");
        }
        if (region.x >= resource.width || region.y >= resource.height) {
            throw std::runtime_error(
                "requestPickingIds region starts outside the texture bounds");
        }

        Core::Renderer::TextureReadbackRegion clamped = region;
        clamped.width = std::min(region.width, resource.width - region.x);
        clamped.height = std::min(region.height, resource.height - region.y);

        m_impl->queuedPickingReadback.resource = resource;
        m_impl->queuedPickingReadback.region = clamped;
        m_impl->queuedPickingReadback.sequence =
            m_impl->nextPickingReadbackSequence++;
        if (m_impl->nextPickingReadbackSequence == 0) {
            m_impl->nextPickingReadbackSequence = 1;
        }
        m_impl->queuedPickingReadback.queued = true;
    }

    bool WgpuRenderer2D::tryGetPickingIds(
        Core::Renderer::PickingReadbackResult &result) {
        if (m_impl->device == nullptr) {
            return false;
        }
        return m_impl->tryConsumePickingReadback(result);
    }

    bool WgpuRenderer2D::isPickingReadbackPending() const noexcept {
        return m_impl != nullptr && m_impl->hasPickingReadbackWork();
    }

    void
    WgpuRenderer2D::saveTextureToFile(Core::Renderer::TextureHandle texture,
                                      const std::string &path) {
        if (texture == 0) {
            throw std::runtime_error(
                "saveTextureToFile requires a non-zero texture handle");
        }

        const auto &resource = m_impl->getTexture(texture);
        const auto readback = readTexture({.texture = texture,
                                           .x = 0,
                                           .y = 0,
                                           .width = resource.width,
                                           .height = resource.height});
        writeTextureReadbackPng(path, readback);
    }

    void WgpuRenderer2D::saveTargetToFile(const std::string &path) {
        if (m_impl->device == nullptr) {
            throw std::runtime_error("WgpuRenderer2D is not initialized");
        }

        if (m_impl->frameStarted) {
            endFrame();
        }

        Core::Renderer::TextureReadbackResult readback;
        if (m_impl->lastCompletedTargetTexture != 0) {
            const auto &resource =
                m_impl->getTexture(m_impl->lastCompletedTargetTexture);
            readback = readTextureRegion(m_impl->instance, m_impl->device,
                                         m_impl->queue, resource.texture,
                                         resource.format, resource.width,
                                         resource.height,
                                         {.texture = resource.handle,
                                          .x = 0,
                                          .y = 0,
                                          .width = resource.width,
                                          .height = resource.height});
        } else {
            const uint32_t width = std::max(1u, m_impl->extent.width);
            const uint32_t height = std::max(1u, m_impl->extent.height);
            readback = readTextureRegion(m_impl->instance, m_impl->device,
                                         m_impl->queue, m_impl->offscreenTarget,
                                         m_impl->targetFormat, width, height,
                                         {.texture = 0,
                                          .x = 0,
                                          .y = 0,
                                          .width = width,
                                          .height = height});
        }

        writeTextureReadbackPng(path, readback);
    }

    Core::Renderer::Renderer2DStats WgpuRenderer2D::getStats() const noexcept {
        return m_impl->stats;
    }

    void
    WgpuRenderer2D::unregisterTexture(Core::Renderer::TextureHandle texture) {
        if (m_impl->queuedPickingReadback.queued &&
            m_impl->queuedPickingReadback.resource.handle == texture) {
            m_impl->queuedPickingReadback = {};
        }
        m_impl->textures.erase(texture);
        m_impl->recreateTextureBindGroups();
    }

    void WgpuRenderer2D::registerTexture(const TextureResource &texture) {
        m_impl->textures[texture.handle] = texture;
        m_impl->recreateTextureBindGroups();
    }

    void WgpuRenderer2D::drawQuad(const Core::Renderer::QuadProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }

        if (isTransparent(props)) {
            makePrimitiveInstanceInPlace(
                m_impl->transparentPrimitiveBatch.push(props.texture), props);
        } else {
            makePrimitiveInstanceInPlace(
                m_impl->opaquePrimitiveBatch.push(props.texture), props);
        }
        m_impl->stats.quadCount = m_impl->quadStatsCount();
    }

    CustomQuadShaderHandle
    WgpuRenderer2D::createCustomQuadShader(const CustomQuadShaderDesc &desc) {
        if (m_impl->customQuadPipeline == nullptr) {
            throw std::runtime_error(
                "WgpuRenderer2D is not initialized for custom quad shaders");
        }
        return m_impl->customQuadPipeline->createShader(desc);
    }

    void
    WgpuRenderer2D::destroyCustomQuadShader(CustomQuadShaderHandle shader) {
        if (m_impl->customQuadPipeline == nullptr || shader == 0) {
            return;
        }
        m_impl->customQuadPipeline->destroyShader(shader);
    }

    void WgpuRenderer2D::drawCustomQuad(const CustomQuadProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }
        if (m_impl->customQuadPipeline == nullptr ||
            !m_impl->customQuadPipeline->hasShader(props.shader)) {
            throw std::runtime_error(
                "Custom quad shader handle is not registered");
        }

        if (isTransparent(props.quad)) {
            makeCustomQuadInstanceInPlace(
                m_impl->transparentCustomQuadBatch.push(props.shader), props);
        } else {
            makeCustomQuadInstanceInPlace(
                m_impl->opaqueCustomQuadBatch.push(props.shader), props);
        }
        m_impl->stats.quadCount = m_impl->quadStatsCount();
    }

    void WgpuRenderer2D::drawCustomQuad(
        const Core::Renderer::QuadProps &quad, CustomQuadShaderHandle shader,
        std::array<glm::vec4, 4> data,
        Core::Renderer::CustomQuadTransformMode transformMode) {
        drawCustomQuad(CustomQuadProps{.quad = quad,
                                       .shader = shader,
                                       .data = data,
                                       .transformMode = transformMode});
    }

    void WgpuRenderer2D::drawCircle(const Core::Renderer::CircleProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }

        if (props.color.a < 1.0f) {
            makeCircleInstanceInPlace(m_impl->transparentPrimitiveBatch.push(0),
                                      props);
        } else {
            makeCircleInstanceInPlace(m_impl->opaquePrimitiveBatch.push(0),
                                      props);
        }
        m_impl->stats.quadCount = m_impl->quadStatsCount();
    }

    void WgpuRenderer2D::drawLine(const Core::Renderer::LineProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }

        if (props.color.a < 1.0f) {
            makeLineInstanceInPlace(m_impl->transparentPrimitiveBatch.push(0),
                                    props);
        } else {
            makeLineInstanceInPlace(m_impl->opaquePrimitiveBatch.push(0),
                                    props);
        }
        m_impl->stats.quadCount = m_impl->quadStatsCount();
    }

    void WgpuRenderer2D::drawFont(std::string_view text,
                                  const FontProps &props) {
        if (!m_impl->frameStarted || text.empty() || props.color.a <= 0.f ||
            props.fontSize <= 0.f) {
            return;
        }

        if (m_impl->pathStarted) {
            endPath();
        }

        if (m_impl->textPipeline != nullptr &&
            m_impl->msdfFontAtlas != nullptr &&
            appendMsdfText(text, props, *m_impl->msdfFontAtlas,
                           m_impl->textBatch)) {
            m_impl->stats.quadCount = m_impl->quadStatsCount();
            return;
        }

        if (m_impl->fontFile == nullptr) {
            return;
        }

        const float fontBaseSize = m_impl->fontFile->getSize();
        if (fontBaseSize <= 0.f) {
            return;
        }

        const float scale = props.fontSize / fontBaseSize;
        const float defaultLineHeight = m_impl->fontFile->lineHeight() * scale;
        const float lineHeight =
            props.lineHeight > 0.f
                ? props.lineHeight
                : (defaultLineHeight > 0.f ? defaultLineHeight
                                           : props.fontSize);

        const Glyph &spaceGlyph = m_impl->fontFile->getGlyph(U' ');
        const float spaceAdvance =
            std::max(spaceGlyph.advanceX * scale, props.fontSize * 0.25f);

        const PathBakeMetrics metrics =
            makePathBakeMetrics(m_impl->cameraTransform, m_impl->extent);

        const float lineStartX = props.position.x;
        glm::vec2 cursor{props.position.x, props.position.y};
        bool hasTextPathProps = false;
        PathProps textPathProps{};
        m_impl->textPathCommandsScratch.clear();
        m_impl->textPathCommandsScratch.reserve(text.size() * 16u);

        auto flushTextPath = [&]() {
            if (!hasTextPathProps || m_impl->textPathCommandsScratch.empty()) {
                return;
            }

            PathProps pathProps = textPathProps;
            pathProps.fillColor = props.color;
            pathProps.strokeColor.a = 0.f;
            pathProps.strokeSize = 0.f;
            pathProps.renderFill = true;
            pathProps.zIndex = props.zIndex;
            pathProps.id = props.id;
            pathProps.renderPass = props.renderPass;
            const std::span<const PathCommand> textCommands{
                m_impl->textPathCommandsScratch.data(),
                m_impl->textPathCommandsScratch.size()};
            submitPathCommands(
                textCommands, pathProps, metrics, m_impl->opaquePathBatch,
                m_impl->transparentPathBatch, m_impl->opaquePathStrokeBatch,
                m_impl->transparentPathStrokeBatch);

            if (props.antiAlias) {
                m_impl->transparentPathStrokeBatch.push(
                    bakePathFillAntiAlias(textCommands, pathProps, metrics,
                                          props.antiAliasFringeScale),
                    props.zIndex);
            }

            m_impl->textPathCommandsScratch.clear();
            hasTextPathProps = false;
        };

        size_t offset = 0;
        while (offset < text.size()) {
            const uint32_t codepoint = decodeUtf8(text, offset);
            if (codepoint == 0) {
                break;
            }

            if (codepoint == '\r') {
                flushTextPath();
                if (offset < text.size() && text[offset] == '\n') {
                    ++offset;
                }
                cursor.x = lineStartX;
                cursor.y += lineHeight;
                continue;
            }

            if (codepoint == '\n') {
                flushTextPath();
                cursor.x = lineStartX;
                cursor.y += lineHeight;
                continue;
            }

            if (codepoint == '\t') {
                cursor.x += spaceAdvance * std::max(props.tabSize, 1.f) +
                            props.letterSpacing;
                continue;
            }

            const Glyph &glyph =
                m_impl->fontFile->getGlyph(static_cast<char32_t>(codepoint));
            if (!glyph.path.empty()) {
                if (hasTextPathProps &&
                    (textPathProps.fillRule != glyph.pathProps.fillRule ||
                     textPathProps.curveTolerance !=
                         glyph.pathProps.curveTolerance)) {
                    flushTextPath();
                }
                if (!hasTextPathProps) {
                    textPathProps = glyph.pathProps;
                    hasTextPathProps = true;
                }
                for (const PathCommand &command : glyph.path.commands()) {
                    m_impl->textPathCommandsScratch.push_back(
                        transformTextCommand(command, cursor, scale));
                }
            }

            const float advance =
                glyph.advanceX > 0.f
                    ? glyph.advanceX * scale
                    : std::max(glyph.width * scale, props.fontSize * 0.5f);
            cursor.x += advance + props.letterSpacing;
        }
        flushTextPath();
    }

    glm::vec2 WgpuRenderer2D::measureText(std::string_view text,
                                          const FontProps &props) {
        if (text.empty() || props.fontSize <= 0.f) {
            return {0.f, 0.f};
        }

        if (m_impl->msdfFontAtlas != nullptr &&
            m_impl->msdfFontAtlas->valid()) {
            return measureMsdfText(text, props, *m_impl->msdfFontAtlas);
        }

        if (m_impl->fontFile != nullptr) {
            return measurePathText(text, props, *m_impl->fontFile);
        }

        const float safeFontSize = std::max(props.fontSize, 1.f);
        float currentLineWidth = 0.f;
        float maxLineWidth = 0.f;
        float totalHeight = safeFontSize;
        for (const char ch : text) {
            if (ch == '\n') {
                maxLineWidth = std::max(maxLineWidth, currentLineWidth);
                currentLineWidth = 0.f;
                totalHeight += safeFontSize;
                continue;
            }
            currentLineWidth += safeFontSize * 0.6f + props.letterSpacing;
        }

        maxLineWidth = std::max(maxLineWidth, currentLineWidth);
        return {maxLineWidth, totalHeight};
    }

    float WgpuRenderer2D::textCenterOffsetY(std::string_view text,
                                            const FontProps &props) {
        if (text.empty() || props.fontSize <= 0.f) {
            return 0.f;
        }

        if (m_impl->msdfFontAtlas != nullptr &&
            m_impl->msdfFontAtlas->valid()) {
            return msdfCenterOffsetY(text, props, *m_impl->msdfFontAtlas);
        }

        if (m_impl->fontFile != nullptr) {
            return pathCenterOffsetY(text, props, *m_impl->fontFile);
        }

        return std::max(props.fontSize, 1.f) * 0.35f;
    }

    void WgpuRenderer2D::drawPath(std::span<const PathCommand> commands,
                                  const PathProps &props) {
        if (!m_impl->frameStarted || commands.empty()) {
            return;
        }

        if (m_impl->pathStarted) {
            endPath();
        }

        const PathBakeMetrics metrics =
            makePathBakeMetrics(m_impl->cameraTransform, m_impl->extent);
        submitPathCommands(commands, props, metrics, m_impl->opaquePathBatch,
                           m_impl->transparentPathBatch,
                           m_impl->opaquePathStrokeBatch,
                           m_impl->transparentPathStrokeBatch);
    }

    void WgpuRenderer2D::drawPath(const Path2D &path, const PathProps &props) {
        drawPath(path.commands(), props);
    }

    void WgpuRenderer2D::beginPath(const PathProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }

        if (m_impl->pathStarted) {
            endPath();
        }

        m_impl->activePathCommands.clear();
        m_impl->activePathProps = props;
        m_impl->pathStarted = true;
    }

    void WgpuRenderer2D::pathMoveTo(const glm::vec2 &pos) {
        if (!m_impl->frameStarted) {
            return;
        }
        if (!m_impl->pathStarted) {
            beginPath();
        }

        m_impl->activePathCommands.push_back(
            {.kind = PathCommandKind::Move, .p = pos});
    }

    void WgpuRenderer2D::pathLineTo(const glm::vec2 &pos,
                                    const PathCommandStroke &stroke) {
        if (!m_impl->frameStarted) {
            return;
        }
        if (!m_impl->pathStarted) {
            beginPath();
        }

        m_impl->activePathCommands.push_back(PathCommand::lineTo(pos, stroke));
    }

    void WgpuRenderer2D::pathQuadTo(const glm::vec2 &control,
                                    const glm::vec2 &pos,
                                    const PathCommandStroke &stroke) {
        if (!m_impl->frameStarted) {
            return;
        }
        if (!m_impl->pathStarted) {
            beginPath();
        }

        m_impl->activePathCommands.push_back(
            PathCommand::quadTo(control, pos, stroke));
    }

    void WgpuRenderer2D::pathQuadraticTo(const glm::vec2 &control,
                                         const glm::vec2 &pos,
                                         const PathCommandStroke &stroke) {
        pathQuadTo(control, pos, stroke);
    }

    void WgpuRenderer2D::pathCubicTo(const glm::vec2 &control1,
                                     const glm::vec2 &control2,
                                     const glm::vec2 &pos,
                                     const PathCommandStroke &stroke) {
        if (!m_impl->frameStarted) {
            return;
        }
        if (!m_impl->pathStarted) {
            beginPath();
        }

        m_impl->activePathCommands.push_back(
            PathCommand::cubicTo(control1, control2, pos, stroke));
    }

    void WgpuRenderer2D::pathCubicBezierTo(const glm::vec2 &control1,
                                           const glm::vec2 &control2,
                                           const glm::vec2 &pos,
                                           const PathCommandStroke &stroke) {
        pathCubicTo(control1, control2, pos, stroke);
    }

    void WgpuRenderer2D::pathBezierCurveTo(const glm::vec2 &control1,
                                           const glm::vec2 &control2,
                                           const glm::vec2 &pos,
                                           const PathCommandStroke &stroke) {
        pathCubicTo(control1, control2, pos, stroke);
    }

    void WgpuRenderer2D::pathClose(const PathCommandStroke &stroke) {
        if (!m_impl->frameStarted) {
            return;
        }
        if (!m_impl->pathStarted) {
            beginPath();
        }

        m_impl->activePathCommands.push_back(PathCommand::closePath(stroke));
    }

    void WgpuRenderer2D::endPath() {
        if (!m_impl->pathStarted) {
            return;
        }

        const PathBakeMetrics metrics =
            makePathBakeMetrics(m_impl->cameraTransform, m_impl->extent);

        const std::span<const PathCommand> commands{
            m_impl->activePathCommands.data(),
            m_impl->activePathCommands.size()};
        submitPathCommands(
            commands, m_impl->activePathProps, metrics, m_impl->opaquePathBatch,
            m_impl->transparentPathBatch, m_impl->opaquePathStrokeBatch,
            m_impl->transparentPathStrokeBatch);

        m_impl->activePathCommands.clear();
        m_impl->pathStarted = false;
    }

    void WgpuRenderer2D::drawImGui(
        const std::function<void(void *)> &imguiRenderFn) {

        // if someframe is already started skip ui
        if (m_impl->frameStarted) {
            return;
        }

        m_impl->commandEncoder = m_impl->device.CreateCommandEncoder();

        wgpu::RenderPassColorAttachment colorAttachment{};
        colorAttachment.view = m_impl->offscreenTargetView;
        colorAttachment.loadOp = wgpu::LoadOp::Load;
        colorAttachment.storeOp = wgpu::StoreOp::Store;

        wgpu::RenderPassDescriptor renderPassDescriptor{};
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;

        wgpu::RenderPassEncoder renderPass =
            m_impl->commandEncoder.BeginRenderPass(&renderPassDescriptor);

        imguiRenderFn(renderPass.Get());

        renderPass.End();
        wgpu::CommandBuffer commandBuffer = m_impl->commandEncoder.Finish();
        m_impl->queue.Submit(1, &commandBuffer);

        m_impl->commandEncoder = nullptr;
    }

    void
    WgpuRenderer2D::drawToWindow(const std::shared_ptr<Window> &window,
                                 const std::function<void(void *)> &renderFn) {
        if (m_impl->surface == nullptr || m_impl->windowHandle == nullptr ||
            m_impl->device == nullptr) {
            return;
        }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_impl->windowHandle, &width, &height);
        if (width <= 0 || height <= 0) {
            return;
        }

        if (!m_impl->surfaceConfigured ||
            m_impl->surfaceConfiguration.width !=
                static_cast<uint32_t>(width) ||
            m_impl->surfaceConfiguration.height !=
                static_cast<uint32_t>(height)) {
            m_impl->configureWindowSurface(static_cast<uint32_t>(width),
                                           static_cast<uint32_t>(height));
        }

        wgpu::SurfaceTexture surfaceTexture;
        m_impl->surface.GetCurrentTexture(&surfaceTexture);
        if (surfaceTexture.status !=
            wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal) {
            return;
        }

        wgpu::TextureView targetView = surfaceTexture.texture.CreateView();

        m_impl->commandEncoder = m_impl->device.CreateCommandEncoder();

        wgpu::RenderPassColorAttachment colorAttachment{};
        colorAttachment.view = targetView;
        colorAttachment.loadOp = wgpu::LoadOp::Load;
        colorAttachment.storeOp = wgpu::StoreOp::Store;

        wgpu::RenderPassDescriptor renderPassDescriptor{};
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;

        wgpu::RenderPassEncoder renderPass =
            m_impl->commandEncoder.BeginRenderPass(&renderPassDescriptor);

        renderFn(renderPass.Get());

        renderPass.End();
        wgpu::CommandBuffer commandBuffer = m_impl->commandEncoder.Finish();
        m_impl->queue.Submit(1, &commandBuffer);
        m_impl->surface.Present();

        m_impl->commandEncoder = nullptr;
    }

    wgpu::Device WgpuRenderer2D::getDevice() const { return m_impl->device; }
    wgpu::Queue WgpuRenderer2D::getQueue() const { return m_impl->queue; }

    wgpu::TextureView WgpuRenderer2D::getCurrentTargetView() const {
        return m_impl->offscreenTargetView;
    }

    [[nodiscard]] wgpu::TextureFormat WgpuRenderer2D::getTargetFormat() const {
        return m_impl->targetFormat;
    }

    [[nodiscard]] Core::Renderer::Renderer2DTargetFormat
    WgpuRenderer2D::getTargetFormatType() const {
        return m_impl->targetFormatType;
    }

    [[nodiscard]] wgpu::TextureFormat WgpuRenderer2D::getSurfaceFormat() const {
        return m_impl->surfaceFormat;
    }

} // namespace Bess::Wgpu
