#include "bess_wgpu/text/msdf_text_pipeline.h"

#include "bess_core/renderer/msdf_font.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_wgpu/wgpu_renderer_2d.h"
#include "bess_wgpu/wgpu_shader.h"
#include "bess_wgpu/wgpu_texture.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace Bess::Wgpu::Text {

    namespace {

        constexpr uint32_t kReplacementCodepoint = 0xFFFD;

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

    }

    void MsdfTextPipeline::init(const wgpu::Device &device,
                                wgpu::TextureFormat targetFormat,
                                const wgpu::Buffer &frameBuffer,
                                uint64_t frameBufferSize,
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

    void MsdfTextPipeline::destroy() {
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

    bool MsdfTextPipeline::ensureInstanceBufferSize(std::size_t glyphCount) {
        const auto requiredSize = std::max<std::size_t>(
            sizeof(MsdfTextInstance), glyphCount * sizeof(MsdfTextInstance));
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

    void MsdfTextPipeline::uploadInstances(const wgpu::Queue &queue,
                                           const MsdfTextInstance *instances,
                                           uint64_t byteSize) const {
        if (m_instanceBuffer == nullptr || instances == nullptr ||
            byteSize == 0) {
            return;
        }
        queue.WriteBuffer(m_instanceBuffer, 0, instances, byteSize);
    }

    void MsdfTextPipeline::draw(wgpu::RenderPassEncoder &renderPass,
                                uint32_t firstGlyph,
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

    void MsdfTextPipeline::createShader() {
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

    void MsdfTextPipeline::createBindGroupLayout() {
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

    void MsdfTextPipeline::createPipelineLayout() {
        wgpu::PipelineLayoutDescriptor descriptor{};
        descriptor.bindGroupLayoutCount = 1;
        descriptor.bindGroupLayouts = &m_bindGroupLayout;
        m_pipelineLayout = m_device.CreatePipelineLayout(&descriptor);
    }

    void MsdfTextPipeline::createSampler() {
        wgpu::SamplerDescriptor descriptor{};
        descriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
        descriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
        descriptor.addressModeW = wgpu::AddressMode::ClampToEdge;
        descriptor.magFilter = wgpu::FilterMode::Linear;
        descriptor.minFilter = wgpu::FilterMode::Linear;
        descriptor.mipmapFilter = wgpu::MipmapFilterMode::Linear;
        m_sampler = m_device.CreateSampler(&descriptor);
    }

    void MsdfTextPipeline::createBindGroup() {
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

    void MsdfTextPipeline::createPipelineState() {
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
                "Failed to create MSDF text render pipeline");
        }
    }

    template <typename TAtlas>
    bool appendMsdfText(std::string_view text,
                        const Core::Renderer::FontProps &props,
                        const TAtlas &atlas, MsdfTextBatch &batch) {
        if (!atlas.valid()) {
            return false;
        }

        const float fontSize = props.fontSize;
        const float lineStartX = props.position.x;
        const float lineHeight =
            props.lineHeight > 0.f
                ? props.lineHeight
                : std::max(atlas.lineHeight() * fontSize, fontSize);

        const Core::Renderer::MsdfGlyph *spaceGlyph = atlas.findGlyph(' ');
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
                baseline.x +=
                    (spaceAdvance * std::max(props.tabSize, 1.f)) +
                    props.letterSpacing;
                continue;
            }

            const Core::Renderer::MsdfGlyph *glyph =
                atlas.findGlyph(codepoint);
            if (glyph == nullptr) {
                continue;
            }

            if (glyph->drawable) {
                const glm::vec4 &bounds = glyph->planeBounds;
                const float left = baseline.x + (bounds.x * fontSize);
                const float right = baseline.x + (bounds.z * fontSize);
                const float top = baseline.y - (bounds.w * fontSize);
                const float bottom = baseline.y - (bounds.y * fontSize);
                const glm::vec2 size{
                    std::max(0.f, right - left),
                    std::max(0.f, bottom - top),
                };

                if (size.x > 0.f && size.y > 0.f) {
                    MsdfTextInstance instance;
                    instance.position[0] = left + (size.x * 0.5f);
                    instance.position[1] = top + (size.y * 0.5f);
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

    template <typename TAtlas>
    glm::vec2 measureMsdfText(std::string_view text,
                              const Core::Renderer::FontProps &props,
                              const TAtlas &atlas) {
        if (!atlas.valid() || text.empty() || props.fontSize <= 0.f) {
            return {0.f, 0.f};
        }

        const float fontSize = props.fontSize;

        const Core::Renderer::MsdfGlyph *spaceGlyph = atlas.findGlyph(' ');
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
                    (spaceAdvance * std::max(props.tabSize, 1.f)) +
                    props.letterSpacing;
                continue;
            }

            const Core::Renderer::MsdfGlyph *glyph =
                atlas.findGlyph(codepoint);
            if (glyph == nullptr) {
                continue;
            }

            if (glyph->drawable) {
                const glm::vec4 &bounds = glyph->planeBounds;
                const float glyphLeft = lineAdvance + (bounds.x * fontSize);
                const float glyphRight =
                    lineAdvance + (bounds.z * fontSize);
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

    template <typename TAtlas>
    float msdfCenterOffsetY(std::string_view text,
                            const Core::Renderer::FontProps &props,
                            const TAtlas &atlas) {
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

            const Core::Renderer::MsdfGlyph *glyph =
                atlas.findGlyph(codepoint);
            if (glyph == nullptr || !glyph->drawable) {
                continue;
            }

            const glm::vec4 &bounds = glyph->planeBounds;
            const float top = baselineY - (bounds.w * fontSize);
            const float bottom = baselineY - (bounds.y * fontSize);
            inkTop = std::min(inkTop, top);
            inkBottom = std::max(inkBottom, bottom);
            hasInk = true;
        }

        return hasInk ? -((inkTop + inkBottom) * 0.5f) : fontSize * 0.35f;
    }

    template bool
    appendMsdfText<Core::Renderer::MsdfFontAtlas<WgpuTexture>>(
        std::string_view, const Core::Renderer::FontProps &,
        const Core::Renderer::MsdfFontAtlas<WgpuTexture> &, MsdfTextBatch &);

    template glm::vec2
    measureMsdfText<Core::Renderer::MsdfFontAtlas<WgpuTexture>>(
        std::string_view, const Core::Renderer::FontProps &,
        const Core::Renderer::MsdfFontAtlas<WgpuTexture> &);

    template float
    msdfCenterOffsetY<Core::Renderer::MsdfFontAtlas<WgpuTexture>>(
        std::string_view, const Core::Renderer::FontProps &,
        const Core::Renderer::MsdfFontAtlas<WgpuTexture> &);

}
