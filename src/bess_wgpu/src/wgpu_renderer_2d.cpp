#include "bess_wgpu/wgpu_renderer_2d.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <png.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace Bess::Wgpu {
    namespace {
        using Bess::Core::Renderer::Color;
        using Bess::Core::Renderer::Renderer2DExtent;
        using Bess::Core::Renderer::Renderer2DTargetFormat;

        struct FrameUniform {
            float viewport[2] = {1.f, 1.f};
            float padding[2] = {0.f, 0.f};
        };

        struct QuadInstance {
            float position[2] = {0.f, 0.f};
            float size[2] = {1.f, 1.f};
            float color[4] = {1.f, 1.f, 1.f, 1.f};
            float radius[4] = {0.f, 0.f, 0.f, 0.f};
            float borderSize[4] = {0.f, 0.f, 0.f, 0.f};
            float borderColor[4] = {0.f, 0.f, 0.f, 1.f};
            float rotation = 0.f;
            float zIndex = 0.f;
            float padding[2] = {0.f, 0.f};
        };

        constexpr const char *kQuadShader = R"(
struct Frame {
    viewport: vec2f,
    padding: vec2f,
};

struct Quad {
    position: vec2f,
    size: vec2f,
    color: vec4f,
    radius: vec4f,
    border_size: vec4f,
    border_color: vec4f,
    rotation: f32,
    z_index: f32,
    padding: vec2f,
};

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) local_pos: vec2f,
    @location(1) size: vec2f,
    @location(2) color: vec4f,
    @location(3) radius: vec4f,
    @location(4) border_size: vec4f,
    @location(5) border_color: vec4f,
};

@group(0) @binding(0) var<storage, read> quads: array<Quad>;
@group(0) @binding(1) var<uniform> frame: Frame;

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32,
           @builtin(instance_index) instance_index: u32) -> VertexOut {
    let corners = array<vec2f, 6>(
        vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
        vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
    let local = corners[vertex_index];
    let q = quads[instance_index];
    let centered = (local - vec2f(0.5, 0.5)) * q.size;
    let s = sin(q.rotation);
    let c = cos(q.rotation);
    let rotated = vec2f(
        centered.x * c - centered.y * s,
        centered.x * s + centered.y * c);
    let world = q.position + rotated + q.size * 0.5;
    let ndc = vec2f(
        (world.x / frame.viewport.x) * 2.0 - 1.0,
        1.0 - (world.y / frame.viewport.y) * 2.0);

    var out: VertexOut;
    out.position = vec4f(ndc, q.z_index, 1.0);
    out.local_pos = local * q.size;
    out.size = q.size;
    out.color = q.color;
    out.radius = q.radius;
    out.border_size = q.border_size;
    out.border_color = q.border_color;
    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
    let border = max(max(in.border_size.x, in.border_size.y),
                     max(in.border_size.z, in.border_size.w));
    if (border > 0.0) {
        let near_edge = in.local_pos.x < border ||
                        in.local_pos.y < border ||
                        in.local_pos.x > in.size.x - border ||
                        in.local_pos.y > in.size.y - border;
        if (near_edge) {
            return in.border_color;
        }
    }
    return in.color;
}
)";

        wgpu::TextureFormat toWgpuFormat(Renderer2DTargetFormat format) {
            switch (format) {
            case Renderer2DTargetFormat::RGBA8Unorm:
                return wgpu::TextureFormat::RGBA8Unorm;
            case Renderer2DTargetFormat::RGBA16Float:
                return wgpu::TextureFormat::RGBA16Float;
            case Renderer2DTargetFormat::BGRA8Unorm:
            default:
                return wgpu::TextureFormat::BGRA8Unorm;
            }
        }

        wgpu::Color toWgpuColor(const Color &color) {
            return {color.r, color.g, color.b, color.a};
        }

        uint32_t alignTo(uint32_t value, uint32_t alignment) {
            return ((value + alignment - 1) / alignment) * alignment;
        }

        bool isPngWritableFormat(wgpu::TextureFormat format) {
            return format == wgpu::TextureFormat::RGBA8Unorm ||
                   format == wgpu::TextureFormat::BGRA8Unorm;
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

            png_structp png =
                png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                        nullptr);
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
                rows[row] =
                    const_cast<png_bytep>(rgba + static_cast<size_t>(row) *
                                                     rowBytes);
            }

            png_write_image(png, rows.data());
            png_write_end(png, nullptr);
            png_destroy_write_struct(&png, &info);
        }

        QuadInstance makeQuadInstance(
            const Core::Renderer::QuadProps &props,
            const Core::Renderer::RoundedBorderProps *roundedProps) {
            QuadInstance instance;
            instance.position[0] = props.position.x;
            instance.position[1] = props.position.y;
            instance.size[0] = props.size.x;
            instance.size[1] = props.size.y;
            instance.color[0] = props.color.r;
            instance.color[1] = props.color.g;
            instance.color[2] = props.color.b;
            instance.color[3] = props.color.a;
            instance.rotation = props.rotation;
            instance.zIndex = props.zIndex;

            if (roundedProps != nullptr) {
                instance.radius[0] = roundedProps->radius.x;
                instance.radius[1] = roundedProps->radius.y;
                instance.radius[2] = roundedProps->radius.z;
                instance.radius[3] = roundedProps->radius.w;
                instance.borderSize[0] = roundedProps->thickness.x;
                instance.borderSize[1] = roundedProps->thickness.y;
                instance.borderSize[2] = roundedProps->thickness.z;
                instance.borderSize[3] = roundedProps->thickness.w;
                instance.borderColor[0] = roundedProps->color.r;
                instance.borderColor[1] = roundedProps->color.g;
                instance.borderColor[2] = roundedProps->color.b;
                instance.borderColor[3] = roundedProps->color.a;
            }

            return instance;
        }
    } // namespace

    struct WgpuRenderer2D::Impl {
        Core::Renderer::Renderer2DCreateInfo createInfo;
        Renderer2DExtent extent;
        wgpu::TextureFormat targetFormat = wgpu::TextureFormat::BGRA8Unorm;

        wgpu::Instance instance;
        wgpu::Adapter adapter;
        wgpu::Device device;
        wgpu::Queue queue;

        wgpu::Texture offscreenTarget;
        wgpu::TextureView offscreenTargetView;
        wgpu::RenderPipeline quadPipeline;
        wgpu::BindGroupLayout bindGroupLayout;
        wgpu::BindGroup bindGroup;
        wgpu::Buffer quadBuffer;
        wgpu::Buffer frameBuffer;
        wgpu::CommandEncoder commandEncoder;

        std::vector<QuadInstance> quads;
        std::size_t quadBufferSize = 0;
        Color clearColor{0.f, 0.f, 0.f, 1.f};
        bool shouldClear = true;
        bool frameStarted = false;

        void createDevice();
        void createOffscreenTarget();
        void createPipeline();
        void ensureQuadBufferSize(std::size_t quadCount);
        void recreateBindGroup();
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
        auto adapterCallback = [&adapterResult](
                                   wgpu::RequestAdapterStatus status,
                                   wgpu::Adapter adapter,
                                   wgpu::StringView message) {
                if (status != wgpu::RequestAdapterStatus::Success) {
                    adapterResult.error =
                        message.data != nullptr
                            ? std::string(message.data, message.length)
                            : "unknown adapter error";
                    return;
                }
                adapterResult.adapter = adapter;
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
                deviceResult.device = device;
            };

        instance.WaitAny(adapter.RequestDevice(
                             &deviceDescriptor, wgpu::CallbackMode::WaitAnyOnly,
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

        offscreenTarget = device.CreateTexture(&descriptor);
        offscreenTargetView = offscreenTarget.CreateView();
    }

    void WgpuRenderer2D::Impl::createPipeline() {
        wgpu::ShaderSourceWGSL wgslSource{};
        wgslSource.code = kQuadShader;

        wgpu::ShaderModuleDescriptor shaderDescriptor{};
        shaderDescriptor.nextInChain = &wgslSource;
        wgpu::ShaderModule shaderModule =
            device.CreateShaderModule(&shaderDescriptor);

        std::array<wgpu::BindGroupLayoutEntry, 2> bindings{};
        bindings[0].binding = 0;
        bindings[0].visibility = wgpu::ShaderStage::Vertex;
        bindings[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

        bindings[1].binding = 1;
        bindings[1].visibility = wgpu::ShaderStage::Vertex;
        bindings[1].buffer.type = wgpu::BufferBindingType::Uniform;

        wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{};
        bindGroupLayoutDescriptor.entryCount = bindings.size();
        bindGroupLayoutDescriptor.entries = bindings.data();
        bindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDescriptor);

        wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor{};
        pipelineLayoutDescriptor.bindGroupLayoutCount = 1;
        pipelineLayoutDescriptor.bindGroupLayouts = &bindGroupLayout;
        wgpu::PipelineLayout pipelineLayout =
            device.CreatePipelineLayout(&pipelineLayoutDescriptor);

        wgpu::ColorTargetState colorTarget{};
        colorTarget.format = targetFormat;

        wgpu::FragmentState fragment{};
        fragment.module = shaderModule;
        fragment.entryPoint = "fs_main";
        fragment.targetCount = 1;
        fragment.targets = &colorTarget;

        wgpu::RenderPipelineDescriptor pipelineDescriptor{};
        pipelineDescriptor.layout = pipelineLayout;
        pipelineDescriptor.vertex.module = shaderModule;
        pipelineDescriptor.vertex.entryPoint = "vs_main";
        pipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        pipelineDescriptor.primitive.cullMode = wgpu::CullMode::None;
        pipelineDescriptor.fragment = &fragment;

        quadPipeline = device.CreateRenderPipeline(&pipelineDescriptor);

        wgpu::BufferDescriptor frameBufferDescriptor{};
        frameBufferDescriptor.size = sizeof(FrameUniform);
        frameBufferDescriptor.usage =
            wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        frameBuffer = device.CreateBuffer(&frameBufferDescriptor);
    }

    void WgpuRenderer2D::Impl::ensureQuadBufferSize(std::size_t quadCount) {
        const auto requiredSize =
            std::max<std::size_t>(sizeof(QuadInstance), quadCount * sizeof(QuadInstance));
        if (quadBuffer != nullptr && quadBufferSize >= requiredSize) {
            return;
        }

        wgpu::BufferDescriptor descriptor{};
        descriptor.size = requiredSize;
        descriptor.usage =
            wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        quadBuffer = device.CreateBuffer(&descriptor);
        quadBufferSize = requiredSize;
        recreateBindGroup();
    }

    void WgpuRenderer2D::Impl::recreateBindGroup() {
        if (quadBuffer == nullptr || frameBuffer == nullptr) {
            return;
        }

        std::array<wgpu::BindGroupEntry, 2> entries{};
        entries[0].binding = 0;
        entries[0].buffer = quadBuffer;
        entries[0].offset = 0;
        entries[0].size = quadBufferSize;

        entries[1].binding = 1;
        entries[1].buffer = frameBuffer;
        entries[1].offset = 0;
        entries[1].size = sizeof(FrameUniform);

        wgpu::BindGroupDescriptor descriptor{};
        descriptor.layout = bindGroupLayout;
        descriptor.entryCount = entries.size();
        descriptor.entries = entries.data();
        bindGroup = device.CreateBindGroup(&descriptor);
    }

    WgpuRenderer2D::WgpuRenderer2D() : m_impl(std::make_unique<Impl>()) {}

    WgpuRenderer2D::~WgpuRenderer2D() { destroy(); }

    void WgpuRenderer2D::init(
        const Core::Renderer::Renderer2DCreateInfo &createInfo) {
        destroy();
        m_impl = std::make_unique<Impl>();
        m_impl->createInfo = createInfo;
        m_impl->extent = createInfo.extent;
        m_impl->targetFormat = toWgpuFormat(createInfo.targetFormat);
        m_impl->createDevice();
        m_impl->createOffscreenTarget();
        m_impl->createPipeline();
        m_impl->ensureQuadBufferSize(1);
    }

    void WgpuRenderer2D::destroy() {
        if (m_impl == nullptr) {
            return;
        }
        m_impl->commandEncoder = nullptr;
        m_impl->bindGroup = nullptr;
        m_impl->bindGroupLayout = nullptr;
        m_impl->quadPipeline = nullptr;
        m_impl->quadBuffer = nullptr;
        m_impl->frameBuffer = nullptr;
        m_impl->offscreenTargetView = nullptr;
        m_impl->offscreenTarget = nullptr;
        m_impl->queue = nullptr;
        m_impl->device = nullptr;
        m_impl->adapter = nullptr;
        m_impl->instance = nullptr;
        m_impl->quads.clear();
        m_impl->frameStarted = false;
    }

    void WgpuRenderer2D::resize(const Renderer2DExtent &extent) {
        m_impl->extent = extent;
        if (m_impl->device != nullptr) {
            m_impl->createOffscreenTarget();
        }
    }

    void WgpuRenderer2D::beginFrame(
        const Core::Renderer::Renderer2DFrameInfo &frameInfo) {
        if (m_impl->device == nullptr) {
            throw std::runtime_error("WgpuRenderer2D is not initialized");
        }

        if (frameInfo.extent.width != 0 && frameInfo.extent.height != 0 &&
            (frameInfo.extent.width != m_impl->extent.width ||
             frameInfo.extent.height != m_impl->extent.height)) {
            resize(frameInfo.extent);
        }

        m_impl->clearColor = frameInfo.clearColor;
        m_impl->shouldClear = frameInfo.shouldClear;
        m_impl->quads.clear();
        m_impl->frameStarted = true;
    }

    void WgpuRenderer2D::endFrame() {
        if (!m_impl->frameStarted) {
            return;
        }

        m_impl->commandEncoder = m_impl->device.CreateCommandEncoder();

        wgpu::RenderPassColorAttachment colorAttachment{};
        colorAttachment.view = m_impl->offscreenTargetView;
        colorAttachment.loadOp =
            m_impl->shouldClear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        colorAttachment.storeOp = wgpu::StoreOp::Store;
        colorAttachment.clearValue = toWgpuColor(m_impl->clearColor);

        wgpu::RenderPassDescriptor renderPassDescriptor{};
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;

        wgpu::RenderPassEncoder renderPass =
            m_impl->commandEncoder.BeginRenderPass(&renderPassDescriptor);

        if (!m_impl->quads.empty()) {
            m_impl->ensureQuadBufferSize(m_impl->quads.size());
            m_impl->queue.WriteBuffer(m_impl->quadBuffer, 0, m_impl->quads.data(),
                                      m_impl->quads.size() * sizeof(QuadInstance));

            FrameUniform frameUniform{};
            frameUniform.viewport[0] = static_cast<float>(m_impl->extent.width);
            frameUniform.viewport[1] = static_cast<float>(m_impl->extent.height);
            m_impl->queue.WriteBuffer(m_impl->frameBuffer, 0, &frameUniform,
                                      sizeof(frameUniform));

            renderPass.SetPipeline(m_impl->quadPipeline);
            renderPass.SetBindGroup(0, m_impl->bindGroup);
            renderPass.Draw(6, static_cast<uint32_t>(m_impl->quads.size()));
        }

        renderPass.End();
        wgpu::CommandBuffer commandBuffer = m_impl->commandEncoder.Finish();
        m_impl->queue.Submit(1, &commandBuffer);

        m_impl->commandEncoder = nullptr;
        m_impl->frameStarted = false;
    }

    void WgpuRenderer2D::clear(const Color &color) {
        m_impl->clearColor = color;
        m_impl->shouldClear = true;
    }

    void WgpuRenderer2D::saveTargetToFile(const std::string &path) {
        if (m_impl->device == nullptr) {
            throw std::runtime_error("WgpuRenderer2D is not initialized");
        }

        if (m_impl->frameStarted) {
            endFrame();
        }

        if (!isPngWritableFormat(m_impl->targetFormat)) {
            throw std::runtime_error(
                "saveTargetToFile currently supports only 8-bit RGBA/BGRA "
                "render targets");
        }

        const uint32_t width = std::max(1u, m_impl->extent.width);
        const uint32_t height = std::max(1u, m_impl->extent.height);
        const uint32_t bytesPerPixel = 4;
        const uint32_t unpaddedBytesPerRow = width * bytesPerPixel;
        const uint32_t paddedBytesPerRow = alignTo(unpaddedBytesPerRow, 256);
        const auto readbackSize =
            static_cast<uint64_t>(paddedBytesPerRow) * height;

        wgpu::BufferDescriptor bufferDescriptor{};
        bufferDescriptor.size = readbackSize;
        bufferDescriptor.usage =
            wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer readbackBuffer =
            m_impl->device.CreateBuffer(&bufferDescriptor);

        wgpu::CommandEncoder encoder = m_impl->device.CreateCommandEncoder();

        wgpu::TexelCopyTextureInfo source{};
        source.texture = m_impl->offscreenTarget;
        source.mipLevel = 0;
        source.origin = {0, 0, 0};
        source.aspect = wgpu::TextureAspect::All;

        wgpu::TexelCopyBufferInfo destination{};
        destination.buffer = readbackBuffer;
        destination.layout.offset = 0;
        destination.layout.bytesPerRow = paddedBytesPerRow;
        destination.layout.rowsPerImage = height;

        wgpu::Extent3D copySize{width, height, 1};
        encoder.CopyTextureToBuffer(&source, &destination, &copySize);

        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        m_impl->queue.Submit(1, &commandBuffer);

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

        wgpu::Future mapFuture = readbackBuffer.MapAsync(
            wgpu::MapMode::Read, 0, readbackSize,
            wgpu::CallbackMode::WaitAnyOnly, mapCallback);
        if (m_impl->instance.WaitAny(mapFuture, UINT64_MAX) !=
            wgpu::WaitStatus::Success) {
            throw std::runtime_error("Timed out waiting for WGPU readback");
        }
        if (mapStatus != wgpu::MapAsyncStatus::Success) {
            throw std::runtime_error("Failed to map WGPU readback buffer: " +
                                     mapError);
        }

        const auto *mappedData = static_cast<const uint8_t *>(
            readbackBuffer.GetConstMappedRange(0, readbackSize));
        if (mappedData == nullptr) {
            readbackBuffer.Unmap();
            throw std::runtime_error("Failed to access WGPU readback data");
        }

        std::vector<uint8_t> rgba(static_cast<size_t>(width) * height *
                                  bytesPerPixel);
        for (uint32_t row = 0; row < height; ++row) {
            const uint8_t *src =
                mappedData + static_cast<size_t>(row) * paddedBytesPerRow;
            uint8_t *dst =
                rgba.data() + static_cast<size_t>(row) * unpaddedBytesPerRow;

            if (m_impl->targetFormat == wgpu::TextureFormat::BGRA8Unorm) {
                for (uint32_t col = 0; col < width; ++col) {
                    const uint32_t offset = col * bytesPerPixel;
                    dst[offset + 0] = src[offset + 2];
                    dst[offset + 1] = src[offset + 1];
                    dst[offset + 2] = src[offset + 0];
                    dst[offset + 3] = src[offset + 3];
                }
            } else {
                std::copy(src, src + unpaddedBytesPerRow, dst);
            }
        }

        readbackBuffer.Unmap();
        writePng(path, rgba.data(), width, height);
    }

    Core::Renderer::TextureHandle WgpuRenderer2D::createTexture(
        const Core::Renderer::ITexture &) {
        return 0;
    }

    void WgpuRenderer2D::destroyTexture(Core::Renderer::TextureHandle) {}

    void WgpuRenderer2D::drawQuad(const Core::Renderer::QuadProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }
        m_impl->quads.emplace_back(makeQuadInstance(props, nullptr));
    }

    void WgpuRenderer2D::drawRoundedQuad(
        const Core::Renderer::QuadProps &props,
        const Core::Renderer::RoundedBorderProps &roundedProps) {
        if (!m_impl->frameStarted) {
            return;
        }
        m_impl->quads.emplace_back(makeQuadInstance(props, &roundedProps));
    }

    wgpu::Device WgpuRenderer2D::getDevice() const { return m_impl->device; }
    wgpu::Queue WgpuRenderer2D::getQueue() const { return m_impl->queue; }

    wgpu::TextureView WgpuRenderer2D::getCurrentTargetView() const {
        return m_impl->offscreenTargetView;
    }

} // namespace Bess::Wgpu
