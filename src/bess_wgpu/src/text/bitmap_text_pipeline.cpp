#include "bess_wgpu/text/bitmap_text_pipeline.h"

#include "bess_wgpu/wgpu_shader.h"
#include "common/logger.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace Bess::Wgpu::Text {
    namespace {
        constexpr uint32_t kReplacementCodepoint = 0xFFFD;
        constexpr uint32_t kAtlasPadding = 1;
        // Keep the load target paired with FT_RENDER_MODE_NORMAL. LIGHT
        // hinting can add nearly transparent leading rows to some glyphs
        // (notably Roboto's lowercase 'r'), giving otherwise equal x-height
        // glyphs different bitmap extents before they reach the GPU.
        constexpr FT_Int32 kBitmapGlyphLoadFlags =
            FT_LOAD_DEFAULT | FT_LOAD_TARGET_NORMAL;
        constexpr FT_Render_Mode kBitmapGlyphRenderMode = FT_RENDER_MODE_NORMAL;
        static_assert(FT_LOAD_TARGET_MODE(kBitmapGlyphLoadFlags) ==
                      kBitmapGlyphRenderMode);

        uint64_t glyphKey(uint32_t codepoint, uint32_t pixelSize) {
            return (static_cast<uint64_t>(pixelSize) << 32u) |
                   static_cast<uint64_t>(codepoint);
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

        constexpr const char *kBitmapTextShader = R"(
struct Frame {
    viewport: vec2f,
    padding: vec2f,
    camera_transform: mat4x4f,
};

struct TextGlyph {
    position: vec3f,
    snap_anchor_x: f32,
    size: vec2f,
    rotation: f32,
    snap_anchor_y: f32,
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

const TEXT_FLAG_APPLY_CAMERA_TRANSFORM: u32 = 1u;

fn text_depth(z_index: f32) -> f32 {
    return clamp(0.5 - 0.5 * tanh(z_index * 0.01), 0.0, 1.0);
}

fn text_screen_clip_position(world: vec2f, z_index: f32) -> vec4f {
    let safe_viewport = max(frame.viewport, vec2f(1.0, 1.0));
    let clip_xy = vec2f(
        (world.x / safe_viewport.x) * 2.0,
        -(world.y / safe_viewport.y) * 2.0);
    return vec4f(clip_xy, text_depth(z_index), 1.0);
}

fn text_camera_clip_position(world: vec2f, z_index: f32) -> vec4f {
    var clip = frame.camera_transform * vec4f(world, 0.0, 1.0);
    clip.z = text_depth(z_index);
    return clip;
}

fn snap_screen_world_to_pixel(world: vec2f) -> vec2f {
    let safe_viewport = max(frame.viewport, vec2f(1.0, 1.0));
    let pixel = world + safe_viewport * 0.5;
    return round(pixel) - safe_viewport * 0.5;
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
    var out: VertexOut;
    if ((glyph.flags.x & TEXT_FLAG_APPLY_CAMERA_TRANSFORM) != 0u) {
        let world = glyph.position.xy + rotated;
        out.position = text_camera_clip_position(world, glyph.position.z);
    } else {
        // Every glyph on a logical line receives the same translation from
        // its shared baseline anchor. Snapping per-glyph top-left positions
        // lets different bearings round in opposite directions, causing
        // lowercase and capital glyphs to land on different baselines.
        let snap_anchor = vec2f(glyph.snap_anchor_x, glyph.snap_anchor_y);
        let snap_delta = snap_screen_world_to_pixel(snap_anchor) - snap_anchor;
        let world = glyph.position.xy + snap_delta + rotated;
        out.position = text_screen_clip_position(world, glyph.position.z);
    }
    out.uv = glyph.uv_rect.xy + local * glyph.uv_rect.zw;
    out.color = glyph.color;
    out.id = glyph.id;
    return out;
}

fn shade_text(in: VertexOut) -> vec4f {
    let mask = textureSample(font_atlas, font_sampler, in.uv).r;
    return vec4f(in.color.rgb, in.color.a * mask);
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

        bool copyGlyphBitmap(const FT_Bitmap &bitmap,
                             std::vector<uint8_t> &outPixels) {
            const auto width = static_cast<uint32_t>(bitmap.width);
            const auto height = static_cast<uint32_t>(bitmap.rows);
            outPixels.assign(static_cast<size_t>(width) * height, 0);
            if (width == 0 || height == 0) {
                return true;
            }

            const int pitch = bitmap.pitch;
            if (bitmap.buffer == nullptr || pitch == 0) {
                return false;
            }

            if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
                const uint8_t scale =
                    bitmap.num_grays > 1 && bitmap.num_grays < 256
                        ? static_cast<uint8_t>(255 / (bitmap.num_grays - 1))
                        : 1;
                for (uint32_t y = 0; y < height; ++y) {
                    const uint32_t srcY =
                        pitch < 0 ? (height - 1u - y) : y;
                    const auto *src =
                        bitmap.buffer + static_cast<int64_t>(srcY) *
                                            static_cast<int64_t>(std::abs(pitch));
                    uint8_t *dst = outPixels.data() +
                                   static_cast<size_t>(y) * width;
                    for (uint32_t x = 0; x < width; ++x) {
                        dst[x] = static_cast<uint8_t>(src[x] * scale);
                    }
                }
                return true;
            }

            if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
                for (uint32_t y = 0; y < height; ++y) {
                    const uint32_t srcY =
                        pitch < 0 ? (height - 1u - y) : y;
                    const auto *src =
                        bitmap.buffer + static_cast<int64_t>(srcY) *
                                            static_cast<int64_t>(std::abs(pitch));
                    uint8_t *dst = outPixels.data() +
                                   static_cast<size_t>(y) * width;
                    for (uint32_t x = 0; x < width; ++x) {
                        const uint8_t byte = src[x >> 3u];
                        const uint8_t bit =
                            static_cast<uint8_t>(0x80u >> (x & 7u));
                        dst[x] = (byte & bit) != 0 ? 255 : 0;
                    }
                }
                return true;
            }

            return false;
        }
    } // namespace

    bool BitmapFontAtlas::init(const wgpu::Device &device,
                               const wgpu::Queue &queue,
                               const std::string &fontPath,
                               uint32_t atlasSize,
                               uint32_t minPixelSize,
                               uint32_t maxPixelSize) {
        destroy();

        m_device = device;
        m_queue = queue;
        m_minPixelSize = std::max(1u, minPixelSize);
        m_maxPixelSize = std::max(m_minPixelSize, maxPixelSize);

        FT_Library library = nullptr;
        if (FT_Init_FreeType(&library) != 0) {
            BESS_WARN("[BitmapFontAtlas] Failed to initialize FreeType");
            return false;
        }
        m_library = library;

        FT_Face face = nullptr;
        if (FT_New_Face(library, fontPath.c_str(), 0, &face) != 0) {
            BESS_WARN("[BitmapFontAtlas] Failed to load font: {}", fontPath);
            destroy();
            return false;
        }
        m_face = face;

        if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0) {
            BESS_WARN("[BitmapFontAtlas] Font has no Unicode charmap: {}",
                      fontPath);
        }

        const uint32_t safeAtlasSize =
            std::clamp(atlasSize, 256u, 4096u);
        if (!createTexture(safeAtlasSize)) {
            destroy();
            return false;
        }

        return true;
    }

    void BitmapFontAtlas::destroy() {
        if (m_face != nullptr) {
            FT_Done_Face(static_cast<FT_Face>(m_face));
            m_face = nullptr;
        }
        if (m_library != nullptr) {
            FT_Done_FreeType(static_cast<FT_Library>(m_library));
            m_library = nullptr;
        }
        m_textureView = nullptr;
        m_texture = nullptr;
        m_device = nullptr;
        m_queue = nullptr;
        m_glyphs.clear();
        m_metrics.clear();
        m_atlasSize = 0;
        m_currentPixelSize = 0;
        m_cursorX = kAtlasPadding;
        m_cursorY = kAtlasPadding;
        m_rowHeight = 0;
    }

    bool BitmapFontAtlas::valid() const noexcept {
        return m_device != nullptr && m_queue != nullptr && m_texture != nullptr &&
               m_textureView != nullptr && m_face != nullptr;
    }

    uint32_t BitmapFontAtlas::quantizePixelSize(float projectedPixelSize) const {
        if (!std::isfinite(projectedPixelSize)) {
            return m_minPixelSize;
        }
        const auto rounded = static_cast<int32_t>(std::lround(projectedPixelSize));
        return std::clamp<uint32_t>(static_cast<uint32_t>(std::max(1, rounded)),
                                    m_minPixelSize,
                                    m_maxPixelSize);
    }

    BitmapTextLineMetrics
    BitmapFontAtlas::metricsForSize(uint32_t pixelSize) {
        const uint32_t bucket = quantizePixelSize(static_cast<float>(pixelSize));
        if (auto it = m_metrics.find(bucket); it != m_metrics.end()) {
            return it->second;
        }

        if (!selectPixelSize(bucket)) {
            return {.lineHeight = static_cast<float>(bucket),
                    .ascender = static_cast<float>(bucket),
                    .descender = 0.f};
        }

        auto metrics = readCurrentMetrics();
        m_metrics[bucket] = metrics;
        return metrics;
    }

    const BitmapGlyph *BitmapFontAtlas::ensureGlyph(uint32_t codepoint,
                                                    uint32_t pixelSize) {
        if (!valid()) {
            return nullptr;
        }

        const uint32_t bucket = quantizePixelSize(static_cast<float>(pixelSize));
        const uint64_t key = glyphKey(codepoint, bucket);
        if (auto it = m_glyphs.find(key); it != m_glyphs.end()) {
            return &it->second;
        }

        if (!selectPixelSize(bucket)) {
            return nullptr;
        }

        FT_Face face = static_cast<FT_Face>(m_face);
        const FT_UInt glyphIndex =
            FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
        if (glyphIndex == 0) {
            return nullptr;
        }

        if (FT_Load_Glyph(face, glyphIndex, kBitmapGlyphLoadFlags) != 0) {
            return nullptr;
        }

        const float advance =
            static_cast<float>(face->glyph->advance.x) / 64.f;
        if (FT_Render_Glyph(face->glyph, kBitmapGlyphRenderMode) != 0) {
            return nullptr;
        }

        const FT_Bitmap &bitmap = face->glyph->bitmap;
        const uint32_t width = static_cast<uint32_t>(bitmap.width);
        const uint32_t height = static_cast<uint32_t>(bitmap.rows);
        if (width == 0 || height == 0) {
            return cacheEmptyGlyph(key, codepoint, bucket, advance);
        }

        uint32_t atlasX = 0;
        uint32_t atlasY = 0;
        if (!reserveRegion(width, height, atlasX, atlasY)) {
            BESS_WARN("[BitmapFontAtlas] Glyph atlas full; falling back to MSDF");
            return nullptr;
        }

        std::vector<uint8_t> pixels;
        if (!copyGlyphBitmap(bitmap, pixels)) {
            return nullptr;
        }
        if (!uploadGlyph(atlasX, atlasY, width, height, pixels.data())) {
            return nullptr;
        }

        BitmapGlyph glyph{};
        glyph.codepoint = codepoint;
        glyph.pixelSize = bucket;
        glyph.advance = advance;
        glyph.offsetX = static_cast<float>(face->glyph->bitmap_left);
        glyph.offsetY = static_cast<float>(face->glyph->bitmap_top);
        glyph.width = static_cast<float>(width);
        glyph.height = static_cast<float>(height);
        glyph.uvRect = {
            static_cast<float>(atlasX) / static_cast<float>(m_atlasSize),
            static_cast<float>(atlasY) / static_cast<float>(m_atlasSize),
            static_cast<float>(width) / static_cast<float>(m_atlasSize),
            static_cast<float>(height) / static_cast<float>(m_atlasSize),
        };
        glyph.drawable = true;

        auto [it, _] = m_glyphs.emplace(key, glyph);
        return &it->second;
    }

    const wgpu::TextureView &BitmapFontAtlas::getTextureView() const {
        if (m_textureView == nullptr) {
            throw std::runtime_error("Bitmap font atlas is not initialized");
        }
        return m_textureView;
    }

    uint32_t BitmapFontAtlas::atlasSize() const noexcept {
        return m_atlasSize;
    }

    uint32_t BitmapFontAtlas::maxPixelSize() const noexcept {
        return m_maxPixelSize;
    }

    uint32_t BitmapFontAtlas::minPixelSize() const noexcept {
        return m_minPixelSize;
    }

    bool BitmapFontAtlas::createTexture(uint32_t atlasSize) {
        if (m_device == nullptr || m_queue == nullptr || atlasSize == 0) {
            return false;
        }

        wgpu::TextureDescriptor descriptor{};
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {atlasSize, atlasSize, 1};
        descriptor.format = wgpu::TextureFormat::R8Unorm;
        descriptor.mipLevelCount = 1;
        descriptor.sampleCount = 1;
        descriptor.usage =
            wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
        descriptor.label = "BitmapFontAtlas";
        m_texture = m_device.CreateTexture(&descriptor);
        if (m_texture == nullptr) {
            return false;
        }
        m_textureView = m_texture.CreateView();
        if (m_textureView == nullptr) {
            return false;
        }

        m_atlasSize = atlasSize;
        m_cursorX = kAtlasPadding;
        m_cursorY = kAtlasPadding;
        m_rowHeight = 0;

        const uint32_t bytesPerRow = ((atlasSize + 255u) / 256u) * 256u;
        std::vector<uint8_t> zeros(static_cast<size_t>(bytesPerRow) *
                                       atlasSize,
                                   0);
        wgpu::TexelCopyTextureInfo destination{};
        destination.texture = m_texture;
        destination.mipLevel = 0;
        destination.origin = {0, 0, 0};
        destination.aspect = wgpu::TextureAspect::All;

        wgpu::TexelCopyBufferLayout layout{};
        layout.offset = 0;
        layout.bytesPerRow = bytesPerRow;
        layout.rowsPerImage = atlasSize;

        wgpu::Extent3D writeSize{atlasSize, atlasSize, 1};
        m_queue.WriteTexture(&destination,
                             zeros.data(),
                             zeros.size(),
                             &layout,
                             &writeSize);
        return true;
    }

    bool BitmapFontAtlas::selectPixelSize(uint32_t pixelSize) {
        if (m_face == nullptr) {
            return false;
        }
        const uint32_t bucket = quantizePixelSize(static_cast<float>(pixelSize));
        if (m_currentPixelSize == bucket) {
            return true;
        }
        if (FT_Set_Pixel_Sizes(static_cast<FT_Face>(m_face), 0, bucket) != 0) {
            return false;
        }
        m_currentPixelSize = bucket;
        return true;
    }

    BitmapTextLineMetrics BitmapFontAtlas::readCurrentMetrics() const {
        const auto face = static_cast<FT_Face>(m_face);
        if (face == nullptr || face->size == nullptr) {
            return {};
        }

        const auto &metrics = face->size->metrics;
        const float lineHeight =
            static_cast<float>(metrics.height > 0
                                   ? metrics.height
                                   : metrics.ascender - metrics.descender) /
            64.f;
        return {
            .lineHeight = std::max(lineHeight, 1.f),
            .ascender = static_cast<float>(metrics.ascender) / 64.f,
            .descender = static_cast<float>(metrics.descender) / 64.f,
        };
    }

    const BitmapGlyph *BitmapFontAtlas::cacheEmptyGlyph(uint64_t key,
                                                        uint32_t codepoint,
                                                        uint32_t pixelSize,
                                                        float advance) {
        BitmapGlyph glyph{};
        glyph.codepoint = codepoint;
        glyph.pixelSize = pixelSize;
        glyph.advance = advance;
        glyph.drawable = false;
        auto [it, _] = m_glyphs.emplace(key, glyph);
        return &it->second;
    }

    bool BitmapFontAtlas::reserveRegion(uint32_t width,
                                        uint32_t height,
                                        uint32_t &x,
                                        uint32_t &y) {
        if (m_atlasSize == 0 || width == 0 || height == 0) {
            return false;
        }

        const uint32_t paddedWidth = width + (kAtlasPadding * 2u);
        const uint32_t paddedHeight = height + (kAtlasPadding * 2u);
        if (paddedWidth > m_atlasSize || paddedHeight > m_atlasSize) {
            return false;
        }

        if (m_cursorX + paddedWidth > m_atlasSize) {
            m_cursorX = kAtlasPadding;
            m_cursorY += std::max(m_rowHeight, paddedHeight);
            m_rowHeight = 0;
        }

        if (m_cursorY + paddedHeight > m_atlasSize) {
            return false;
        }

        x = m_cursorX + kAtlasPadding;
        y = m_cursorY + kAtlasPadding;
        m_cursorX += paddedWidth;
        m_rowHeight = std::max(m_rowHeight, paddedHeight);
        return true;
    }

    bool BitmapFontAtlas::uploadGlyph(uint32_t x,
                                      uint32_t y,
                                      uint32_t width,
                                      uint32_t height,
                                      const uint8_t *pixels) {
        if (m_texture == nullptr || pixels == nullptr || width == 0 ||
            height == 0) {
            return false;
        }

        wgpu::TexelCopyTextureInfo destination{};
        destination.texture = m_texture;
        destination.mipLevel = 0;
        destination.origin = {x, y, 0};
        destination.aspect = wgpu::TextureAspect::All;

        wgpu::TexelCopyBufferLayout layout{};
        layout.offset = 0;
        const uint32_t bytesPerRow = ((width + 255u) / 256u) * 256u;
        std::vector<uint8_t> uploadRows;
        const uint8_t *uploadPixels = pixels;
        size_t uploadSize = static_cast<size_t>(width) * height;
        if (bytesPerRow != width) {
            uploadSize = static_cast<size_t>(bytesPerRow) * height;
            uploadRows.assign(uploadSize, 0);
            for (uint32_t row = 0; row < height; ++row) {
                const uint8_t *src =
                    pixels + static_cast<size_t>(row) * width;
                uint8_t *dst =
                    uploadRows.data() + static_cast<size_t>(row) * bytesPerRow;
                std::copy(src, src + width, dst);
            }
            uploadPixels = uploadRows.data();
        }

        layout.bytesPerRow = bytesPerRow;
        layout.rowsPerImage = height;

        wgpu::Extent3D writeSize{width, height, 1};
        m_queue.WriteTexture(&destination,
                             uploadPixels,
                             uploadSize,
                             &layout,
                             &writeSize);
        return true;
    }

    void BitmapTextPipeline::init(const wgpu::Device &device,
                                  wgpu::TextureFormat targetFormat,
                                  const wgpu::Buffer &frameBuffer,
                                  uint64_t frameBufferSize,
                                  wgpu::TextureFormat pickingFormat,
                                  const wgpu::TextureView &atlasView) {
        m_device = device;
        m_targetFormat = targetFormat;
        m_pickingFormat = pickingFormat;
        m_frameBuffer = frameBuffer;
        m_frameBufferSize = frameBufferSize;
        m_atlasView = atlasView;
        createShader();
        createBindGroupLayout();
        createPipelineLayout();
        createSampler();
        createPipelineState();
    }

    void BitmapTextPipeline::destroy() {
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

    bool BitmapTextPipeline::ensureInstanceBufferSize(std::size_t glyphCount) {
        const auto requiredSize = std::max<std::size_t>(
            sizeof(BitmapTextInstance), glyphCount * sizeof(BitmapTextInstance));
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

    void BitmapTextPipeline::uploadInstances(
        const wgpu::Queue &queue,
        const BitmapTextInstance *instances,
        uint64_t byteSize) const {
        if (m_instanceBuffer == nullptr || instances == nullptr ||
            byteSize == 0) {
            return;
        }
        queue.WriteBuffer(m_instanceBuffer, 0, instances, byteSize);
    }

    const wgpu::RenderPipeline &BitmapTextPipeline::getPipeline() const {
        if (m_pipeline == nullptr || m_bindGroup == nullptr) {
            throw std::runtime_error(
                "Bitmap text pipeline is not ready for drawing");
        }
        return m_pipeline;
    }

    const wgpu::BindGroup &BitmapTextPipeline::getBindGroup() const {
        if (m_pipeline == nullptr || m_bindGroup == nullptr) {
            throw std::runtime_error(
                "Bitmap text pipeline is not ready for drawing");
        }
        return m_bindGroup;
    }

    void BitmapTextPipeline::drawInstances(
        wgpu::RenderPassEncoder &renderPass,
        uint32_t firstGlyph,
        uint32_t glyphCount) const {
        if (glyphCount == 0) {
            return;
        }
        renderPass.Draw(6, glyphCount, 0, firstGlyph);
    }

    void BitmapTextPipeline::draw(wgpu::RenderPassEncoder &renderPass,
                                  uint32_t firstGlyph,
                                  uint32_t glyphCount) const {
        if (glyphCount == 0) {
            return;
        }
        renderPass.SetPipeline(getPipeline());
        renderPass.SetBindGroup(0, getBindGroup());
        drawInstances(renderPass, firstGlyph, glyphCount);
    }

    void BitmapTextPipeline::createShader() {
        using Core::Renderer::ShaderLanguage;
        using Core::Renderer::ShaderModuleDesc;
        using Core::Renderer::ShaderStage;
        m_shader =
            std::make_unique<WgpuShader>("renderer_2d_bitmap_text",
                                         std::vector<ShaderModuleDesc>{
                                             {.language = ShaderLanguage::WGSL,
                                              .stage = ShaderStage::Vertex,
                                              .entryPoint = "vs_main",
                                              .source = kBitmapTextShader},
                                             {.language = ShaderLanguage::WGSL,
                                              .stage = ShaderStage::Fragment,
                                              .entryPoint = "fs_main",
                                              .source = kBitmapTextShader},
                                         },
                                         m_device);
    }

    void BitmapTextPipeline::createBindGroupLayout() {
        std::array<wgpu::BindGroupLayoutEntry, 4> bindings{};
        bindings[0].binding = 0;
        bindings[0].visibility = wgpu::ShaderStage::Vertex;
        bindings[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

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
        bindings[3].texture.viewDimension = wgpu::TextureViewDimension::e2D;

        wgpu::BindGroupLayoutDescriptor descriptor{};
        descriptor.entryCount = bindings.size();
        descriptor.entries = bindings.data();
        m_bindGroupLayout = m_device.CreateBindGroupLayout(&descriptor);
    }

    void BitmapTextPipeline::createPipelineLayout() {
        wgpu::PipelineLayoutDescriptor descriptor{};
        descriptor.bindGroupLayoutCount = 1;
        descriptor.bindGroupLayouts = &m_bindGroupLayout;
        m_pipelineLayout = m_device.CreatePipelineLayout(&descriptor);
    }

    void BitmapTextPipeline::createSampler() {
        wgpu::SamplerDescriptor descriptor{};
        descriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
        descriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
        descriptor.addressModeW = wgpu::AddressMode::ClampToEdge;
        descriptor.magFilter = wgpu::FilterMode::Linear;
        descriptor.minFilter = wgpu::FilterMode::Linear;
        descriptor.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
        m_sampler = m_device.CreateSampler(&descriptor);
    }

    void BitmapTextPipeline::createBindGroup() {
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

    void BitmapTextPipeline::createPipelineState() {
        wgpu::ColorTargetState colorTargets[2]{};
        uint32_t targetCount = 1;
        colorTargets[0].format = m_targetFormat;

        wgpu::BlendState blendState{};
        blendState.color.operation = wgpu::BlendOperation::Add;
        blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
        blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        blendState.alpha.operation = wgpu::BlendOperation::Add;
        blendState.alpha.srcFactor = wgpu::BlendFactor::One;
        blendState.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        colorTargets[0].blend = &blendState;

        if (m_pickingFormat != wgpu::TextureFormat::Undefined) {
            colorTargets[1].format = m_pickingFormat;
            targetCount = 2;
        }

        wgpu::FragmentState fragment{};
        fragment.module =
            m_shader->getModule(Core::Renderer::ShaderStage::Fragment);
        fragment.entryPoint = m_pickingFormat != wgpu::TextureFormat::Undefined
                                  ? "fs_main_picking"
                                  : "fs_main";
        fragment.targetCount = targetCount;
        fragment.targets = colorTargets;

        wgpu::DepthStencilState depthStencil{};
        depthStencil.format = wgpu::TextureFormat::Depth24PlusStencil8;
        depthStencil.depthCompare = wgpu::CompareFunction::LessEqual;
        depthStencil.depthWriteEnabled = false;

        wgpu::RenderPipelineDescriptor descriptor{};
        descriptor.layout = m_pipelineLayout;
        descriptor.vertex.module =
            m_shader->getModule(Core::Renderer::ShaderStage::Vertex);
        descriptor.vertex.entryPoint = "vs_main";
        descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        descriptor.primitive.cullMode = wgpu::CullMode::None;
        descriptor.fragment = &fragment;
        descriptor.depthStencil = &depthStencil;

        m_pipeline = m_device.CreateRenderPipeline(&descriptor);
        if (m_pipeline == nullptr) {
            throw std::runtime_error(
                "Failed to create bitmap text render pipeline");
        }
    }

    bool ensureBitmapTextGlyphs(std::string_view text,
                                float projectedPixelSize,
                                BitmapFontAtlas &atlas) {
        if (!atlas.valid() || text.empty() || projectedPixelSize <= 0.f) {
            return false;
        }

        const uint32_t pixelSize = atlas.quantizePixelSize(projectedPixelSize);
        size_t offset = 0;
        while (offset < text.size()) {
            const uint32_t codepoint = decodeUtf8(text, offset);
            if (codepoint == 0) {
                break;
            }
            if (codepoint == '\r' || codepoint == '\n' ||
                codepoint == '\t') {
                continue;
            }
            if (atlas.ensureGlyph(codepoint, pixelSize) == nullptr) {
                return false;
            }
        }
        return true;
    }

    bool appendBitmapText(std::string_view text,
                          const Core::Renderer::FontProps &props,
                          float projectedPixelSize,
                          BitmapFontAtlas &atlas,
                          BitmapTextBatch &batch,
                          uint64_t submitOrder,
                          Core::Renderer::RendererScissorState scissor) {
        if (props.fontSize <= 0.f ||
            !ensureBitmapTextGlyphs(text, projectedPixelSize, atlas)) {
            return false;
        }

        const uint32_t pixelSize = atlas.quantizePixelSize(projectedPixelSize);
        const float scale = props.fontSize / static_cast<float>(pixelSize);
        const float lineStartX = props.position.x;
        const BitmapTextLineMetrics metrics = atlas.metricsForSize(pixelSize);
        const float lineHeight =
            props.lineHeight > 0.f
                ? props.lineHeight
                : std::max(metrics.lineHeight * scale, props.fontSize);

        const BitmapGlyph *spaceGlyph = atlas.ensureGlyph(' ', pixelSize);
        const float spaceAdvance =
            spaceGlyph != nullptr && spaceGlyph->advance > 0.f
                ? spaceGlyph->advance * scale
                : props.fontSize * 0.25f;

        glm::vec2 baseline{props.position.x, props.position.y};
        auto advanceLine = [&]() {
            baseline.x = lineStartX;
            baseline.y += lineHeight;
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
                advanceLine();
                continue;
            }
            if (codepoint == '\n') {
                advanceLine();
                continue;
            }
            if (codepoint == '\t') {
                baseline.x += (spaceAdvance * std::max(props.tabSize, 1.f)) +
                              props.letterSpacing;
                continue;
            }

            const BitmapGlyph *glyph = atlas.ensureGlyph(codepoint, pixelSize);
            if (glyph == nullptr) {
                return false;
            }

            if (glyph->drawable) {
                const float left = baseline.x + (glyph->offsetX * scale);
                const float top = baseline.y - (glyph->offsetY * scale);
                const glm::vec2 size{
                    std::max(0.f, glyph->width * scale),
                    std::max(0.f, glyph->height * scale),
                };

                if (size.x > 0.f && size.y > 0.f) {
                    BitmapTextInstance instance;
                    instance.position[0] = left + (size.x * 0.5f);
                    instance.position[1] = top + (size.y * 0.5f);
                    instance.position[2] = props.zIndex;
                    instance.snapAnchorX = lineStartX;
                    instance.size[0] = size.x;
                    instance.size[1] = size.y;
                    instance.rotation = 0.f;
                    instance.snapAnchorY = baseline.y;
                    instance.color[0] = props.color.r;
                    instance.color[1] = props.color.g;
                    instance.color[2] = props.color.b;
                    instance.color[3] = props.color.a;
                    instance.uvRect[0] = glyph->uvRect.x;
                    instance.uvRect[1] = glyph->uvRect.y;
                    instance.uvRect[2] = glyph->uvRect.z;
                    instance.uvRect[3] = glyph->uvRect.w;
                    instance.id[0] = props.id.runtimeId;
                    instance.id[1] = props.id.info;
                    instance.flags[0] =
                        props.transformMode ==
                                Core::Renderer::RenderTransformMode::Camera
                            ? kBitmapTextFlagApplyCameraTransform
                            : 0u;
                    batch.push(instance, submitOrder, scissor);
                }
            }

            const float advance = glyph->advance > 0.f
                                      ? glyph->advance * scale
                                      : props.fontSize * 0.5f;
            baseline.x += advance + props.letterSpacing;
        }

        return true;
    }

    glm::vec2 measureBitmapText(std::string_view text,
                                const Core::Renderer::FontProps &props,
                                BitmapFontAtlas &atlas) {
        if (!atlas.valid() || text.empty() || props.fontSize <= 0.f) {
            return {0.f, 0.f};
        }

        const uint32_t pixelSize = atlas.quantizePixelSize(props.fontSize);
        const float scale = props.fontSize / static_cast<float>(pixelSize);
        const BitmapGlyph *spaceGlyph = atlas.ensureGlyph(' ', pixelSize);
        const float spaceAdvance =
            spaceGlyph != nullptr && spaceGlyph->advance > 0.f
                ? spaceGlyph->advance * scale
                : props.fontSize * 0.25f;

        const BitmapTextLineMetrics metrics = atlas.metricsForSize(pixelSize);
        const float lineHeight =
            props.lineHeight > 0.f
                ? props.lineHeight
                : std::max(metrics.lineHeight * scale, props.fontSize);

        float lineAdvance = 0.f;
        float lineInkMin = 0.f;
        float lineInkMax = 0.f;
        bool hasLineInk = false;
        float maxWidth = 0.f;
        float totalHeight = std::max(lineHeight, props.fontSize);

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
                totalHeight += lineHeight;
                continue;
            }
            if (codepoint == '\n') {
                finishLine();
                totalHeight += lineHeight;
                continue;
            }
            if (codepoint == '\t') {
                lineAdvance += (spaceAdvance * std::max(props.tabSize, 1.f)) +
                               props.letterSpacing;
                continue;
            }

            const BitmapGlyph *glyph = atlas.ensureGlyph(codepoint, pixelSize);
            if (glyph == nullptr) {
                continue;
            }

            if (glyph->drawable) {
                const float glyphLeft = lineAdvance + glyph->offsetX * scale;
                const float glyphRight = glyphLeft + glyph->width * scale;
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
                                      ? glyph->advance * scale
                                      : props.fontSize * 0.5f;
            lineAdvance += advance + props.letterSpacing;
        }

        finishLine();
        return {maxWidth, totalHeight};
    }

    float bitmapCenterOffsetY(std::string_view text,
                              const Core::Renderer::FontProps &props,
                              BitmapFontAtlas &atlas) {
        if (!atlas.valid() || text.empty() || props.fontSize <= 0.f) {
            return 0.f;
        }

        const uint32_t pixelSize = atlas.quantizePixelSize(props.fontSize);
        const float scale = props.fontSize / static_cast<float>(pixelSize);
        const BitmapTextLineMetrics metrics = atlas.metricsForSize(pixelSize);
        const float lineHeight =
            props.lineHeight > 0.f
                ? props.lineHeight
                : std::max(metrics.lineHeight * scale, props.fontSize);

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

            const BitmapGlyph *glyph = atlas.ensureGlyph(codepoint, pixelSize);
            if (glyph == nullptr || !glyph->drawable) {
                continue;
            }

            const float top = baselineY - (glyph->offsetY * scale);
            const float bottom = top + (glyph->height * scale);
            inkTop = std::min(inkTop, top);
            inkBottom = std::max(inkBottom, bottom);
            hasInk = true;
        }

        return hasInk ? -((inkTop + inkBottom) * 0.5f)
                      : props.fontSize * 0.35f;
    }

} // namespace Bess::Wgpu::Text
