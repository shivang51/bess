#include "bess_wgpu/wgpu_renderer_2d.h"
#include "bess_wgpu/shaders/quad_shader.h"
#include "bess_wgpu/wgpu_shader.h"
#include "common/logger.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <png.h>
#include <stb_image.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
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
            float uvRect[4] = {0.f, 0.f, 1.f, 1.f};
            float rotation = 0.f;
            float zIndex = 0.f;
            float useTexture = 0.f;
            float padding = 0.f;
        };

        struct QueuedQuad {
            QuadInstance instance;
            Core::Renderer::TextureHandle texture = 0;
            uint64_t sequence = 0;
        };

        struct DrawRun {
            Core::Renderer::TextureHandle texture = 0;
            uint32_t firstInstance = 0;
            uint32_t instanceCount = 0;
        };

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

        struct StbiImageDeleter {
            void operator()(stbi_uc *pixels) const { stbi_image_free(pixels); }
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
                rows[row] = const_cast<png_bytep>(
                    rgba + static_cast<size_t>(row) * rowBytes);
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
            instance.uvRect[0] = props.uvRect.x;
            instance.uvRect[1] = props.uvRect.y;
            instance.uvRect[2] = props.uvRect.z;
            instance.uvRect[3] = props.uvRect.w;
            instance.rotation = props.rotation;
            instance.zIndex = props.zIndex;
            instance.useTexture = props.texture == 0 ? 0.f : 1.f;

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

        class QuadBatch {
          public:
            void configure(uint32_t initialCapacity, uint32_t maxCapacity) {
                m_maxCapacity = std::max(1u, maxCapacity);
                m_instances.reserve(
                    std::min(std::max(1u, initialCapacity), m_maxCapacity));
            }

            void clear() {
                m_instances.clear();
                m_gpuInstances.clear();
                m_drawRuns.clear();
                m_nextSequence = 0;
            }

            void push(const QuadInstance &instance,
                      Core::Renderer::TextureHandle texture) {
                if (m_instances.size() >= m_maxCapacity) {
                    throw std::runtime_error(
                        "WGPU quad batch capacity exceeded");
                }
                m_instances.push_back({instance, texture, m_nextSequence++});
            }

            void sortForRendering() {
                std::stable_sort(
                    m_instances.begin(), m_instances.end(),
                    [](const QueuedQuad &left, const QueuedQuad &right) {
                        if (left.instance.zIndex != right.instance.zIndex) {
                            return left.instance.zIndex < right.instance.zIndex;
                        }
                        return left.sequence < right.sequence;
                    });

                m_gpuInstances.clear();
                m_gpuInstances.reserve(m_instances.size());
                m_drawRuns.clear();

                for (const auto &quad : m_instances) {
                    const uint32_t instanceIndex =
                        static_cast<uint32_t>(m_gpuInstances.size());
                    m_gpuInstances.emplace_back(quad.instance);

                    if (m_drawRuns.empty() ||
                        m_drawRuns.back().texture != quad.texture) {
                        m_drawRuns.push_back({.texture = quad.texture,
                                              .firstInstance = instanceIndex,
                                              .instanceCount = 1});
                    } else {
                        m_drawRuns.back().instanceCount++;
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
                       sizeof(QuadInstance);
            }

            [[nodiscard]] const QuadInstance *data() const noexcept {
                return m_gpuInstances.data();
            }

            [[nodiscard]] const std::vector<DrawRun> &
            getDrawRuns() const noexcept {
                return m_drawRuns;
            }

          private:
            std::vector<QueuedQuad> m_instances;
            std::vector<QuadInstance> m_gpuInstances;
            std::vector<DrawRun> m_drawRuns;
            uint32_t m_maxCapacity = 1;
            uint64_t m_nextSequence = 0;
        };

        struct TextureResource {
            wgpu::Texture texture;
            wgpu::TextureView view;
            wgpu::BindGroup bindGroup;
            uint32_t width = 1;
            uint32_t height = 1;
        };

        class TextureSource final : public Core::Renderer::ITexture {
          public:
            explicit TextureSource(
                const Core::Renderer::TextureCreateInfo &createInfo)
                : ITexture(createInfo) {}

            void init() override {}
            void destroy() override {}
        };
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
        std::unique_ptr<WgpuShader> quadShader;
        wgpu::BindGroupLayout bindGroupLayout;
        wgpu::Sampler textureSampler;
        wgpu::Buffer quadBuffer;
        wgpu::Buffer frameBuffer;
        wgpu::CommandEncoder commandEncoder;
        std::unordered_map<Core::Renderer::TextureHandle, TextureResource>
            textures;
        Core::Renderer::TextureHandle nextTextureHandle = 1;

        QuadBatch quadBatch;
        std::size_t quadBufferSize = 0;
        Core::Renderer::Renderer2DStats stats;
        Color clearColor{0.f, 0.f, 0.f, 1.f};
        bool shouldClear = true;
        bool frameStarted = false;

        void createDevice();
        void createShaders();
        void createOffscreenTarget();
        void createPipeline();
        void createTextureSampler();
        Core::Renderer::TextureHandle
        createTextureFromPixels(const uint8_t *pixels, uint32_t width,
                                uint32_t height, bool isDefaultTexture);
        void createDefaultTexture();
        void ensureQuadBufferSize(std::size_t quadCount);
        void recreateTextureBindGroups();
        [[nodiscard]] const TextureResource &
        getTexture(Core::Renderer::TextureHandle texture) const;
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
        deviceDescriptor.SetUncapturedErrorCallback(
            [](const wgpu::Device &, wgpu::ErrorType type,
               wgpu::StringView message) {
#undef LOGGER_NAME
#define LOGGER_NAME "BessWgpu"
                BESS_ERROR("Dawn Validation Error [{}]: {}",
                           static_cast<int>(type),
                           std::string_view(message.data, message.length));
#undef LOGGER_NAME
            });
        deviceDescriptor.SetDeviceLostCallback(
            wgpu::CallbackMode::AllowSpontaneous,
            [](const wgpu::Device &, wgpu::DeviceLostReason reason,
               wgpu::StringView message) {
#undef LOGGER_NAME
#define LOGGER_NAME "BessWgpu"
                BESS_ERROR("Dawn Device Lost [{}]: {}",
                           static_cast<int>(reason),
                           std::string_view(message.data, message.length));
#undef LOGGER_NAME
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
            deviceResult.device = device;
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

    void WgpuRenderer2D::Impl::createShaders() {
        quadShader = std::make_unique<WgpuShader>(
            "renderer_2d_quad", Shaders::getQuadShaderModules(), device);
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
        if (!quadShader) {
            createShaders();
        }

        std::array<wgpu::BindGroupLayoutEntry, 4> bindings{};
        bindings[0].binding = 0;
        bindings[0].visibility = wgpu::ShaderStage::Vertex;
        bindings[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

        bindings[1].binding = 1;
        bindings[1].visibility = wgpu::ShaderStage::Vertex;
        bindings[1].buffer.type = wgpu::BufferBindingType::Uniform;

        bindings[2].binding = 2;
        bindings[2].visibility = wgpu::ShaderStage::Fragment;
        bindings[2].sampler.type = wgpu::SamplerBindingType::Filtering;

        bindings[3].binding = 3;
        bindings[3].visibility = wgpu::ShaderStage::Fragment;
        bindings[3].texture.sampleType = wgpu::TextureSampleType::Float;
        bindings[3].texture.viewDimension = wgpu::TextureViewDimension::e2D;

        wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{};
        bindGroupLayoutDescriptor.entryCount = bindings.size();
        bindGroupLayoutDescriptor.entries = bindings.data();
        bindGroupLayout =
            device.CreateBindGroupLayout(&bindGroupLayoutDescriptor);

        wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor{};
        pipelineLayoutDescriptor.bindGroupLayoutCount = 1;
        pipelineLayoutDescriptor.bindGroupLayouts = &bindGroupLayout;
        wgpu::PipelineLayout pipelineLayout =
            device.CreatePipelineLayout(&pipelineLayoutDescriptor);

        wgpu::ColorTargetState colorTarget{};
        colorTarget.format = targetFormat;
        wgpu::BlendState blendState{};
        blendState.color.operation = wgpu::BlendOperation::Add;
        blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
        blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        blendState.alpha.operation = wgpu::BlendOperation::Add;
        blendState.alpha.srcFactor = wgpu::BlendFactor::One;
        blendState.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        colorTarget.blend = &blendState;

        wgpu::FragmentState fragment{};
        fragment.module =
            quadShader->getModule(Core::Renderer::ShaderStage::Fragment);
        fragment.entryPoint =
            quadShader->getEntryPoint(Core::Renderer::ShaderStage::Fragment)
                .c_str();
        fragment.targetCount = 1;
        fragment.targets = &colorTarget;

        wgpu::RenderPipelineDescriptor pipelineDescriptor{};
        pipelineDescriptor.layout = pipelineLayout;
        pipelineDescriptor.vertex.module =
            quadShader->getModule(Core::Renderer::ShaderStage::Vertex);
        pipelineDescriptor.vertex.entryPoint =
            quadShader->getEntryPoint(Core::Renderer::ShaderStage::Vertex)
                .c_str();
        pipelineDescriptor.primitive.topology =
            wgpu::PrimitiveTopology::TriangleList;
        pipelineDescriptor.primitive.cullMode = wgpu::CullMode::None;
        pipelineDescriptor.fragment = &fragment;

        quadPipeline = device.CreateRenderPipeline(&pipelineDescriptor);

        wgpu::BufferDescriptor frameBufferDescriptor{};
        frameBufferDescriptor.size = sizeof(FrameUniform);
        frameBufferDescriptor.usage =
            wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        frameBuffer = device.CreateBuffer(&frameBufferDescriptor);
    }

    void WgpuRenderer2D::Impl::createTextureSampler() {
        wgpu::SamplerDescriptor descriptor{};
        descriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
        descriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
        descriptor.addressModeW = wgpu::AddressMode::ClampToEdge;
        descriptor.magFilter = wgpu::FilterMode::Linear;
        descriptor.minFilter = wgpu::FilterMode::Linear;
        descriptor.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
        textureSampler = device.CreateSampler(&descriptor);
    }

    Core::Renderer::TextureHandle WgpuRenderer2D::Impl::createTextureFromPixels(
        const uint8_t *pixels, uint32_t width, uint32_t height,
        bool isDefaultTexture) {
        if (pixels == nullptr || width == 0 || height == 0) {
            throw std::runtime_error("Invalid texture pixel data");
        }

        wgpu::TextureDescriptor descriptor{};
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {width, height, 1};
        descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
        descriptor.mipLevelCount = 1;
        descriptor.sampleCount = 1;
        descriptor.usage =
            wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;

        TextureResource resource;
        resource.texture = device.CreateTexture(&descriptor);
        resource.view = resource.texture.CreateView();
        resource.width = width;
        resource.height = height;

        wgpu::TexelCopyTextureInfo destination{};
        destination.texture = resource.texture;
        destination.mipLevel = 0;
        destination.origin = {0, 0, 0};
        destination.aspect = wgpu::TextureAspect::All;

        wgpu::TexelCopyBufferLayout layout{};
        layout.offset = 0;
        layout.bytesPerRow = width * 4;
        layout.rowsPerImage = height;

        wgpu::Extent3D writeSize{width, height, 1};
        queue.WriteTexture(&destination, pixels,
                           static_cast<size_t>(width) * height * 4, &layout,
                           &writeSize);

        const Core::Renderer::TextureHandle handle =
            isDefaultTexture ? 0 : nextTextureHandle++;
        textures[handle] = std::move(resource);
        recreateTextureBindGroups();
        return handle;
    }

    void WgpuRenderer2D::Impl::createDefaultTexture() {
        const std::array<uint8_t, 4> whitePixel{255, 255, 255, 255};
        createTextureFromPixels(whitePixel.data(), 1, 1, true);
    }

    void WgpuRenderer2D::Impl::ensureQuadBufferSize(std::size_t quadCount) {
        const auto requiredSize = std::max<std::size_t>(
            sizeof(QuadInstance), quadCount * sizeof(QuadInstance));
        if (quadBuffer != nullptr && quadBufferSize >= requiredSize) {
            return;
        }

        wgpu::BufferDescriptor descriptor{};
        descriptor.size = requiredSize;
        descriptor.usage =
            wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        quadBuffer = device.CreateBuffer(&descriptor);
        quadBufferSize = requiredSize;
        recreateTextureBindGroups();
    }

    void WgpuRenderer2D::Impl::recreateTextureBindGroups() {
        if (quadBuffer == nullptr || frameBuffer == nullptr ||
            bindGroupLayout == nullptr || textureSampler == nullptr) {
            return;
        }

        for (auto &[handle, texture] : textures) {
            std::array<wgpu::BindGroupEntry, 4> entries{};
            entries[0].binding = 0;
            entries[0].buffer = quadBuffer;
            entries[0].offset = 0;
            entries[0].size = quadBufferSize;

            entries[1].binding = 1;
            entries[1].buffer = frameBuffer;
            entries[1].offset = 0;
            entries[1].size = sizeof(FrameUniform);

            entries[2].binding = 2;
            entries[2].sampler = textureSampler;

            entries[3].binding = 3;
            entries[3].textureView = texture.view;

            wgpu::BindGroupDescriptor descriptor{};
            descriptor.layout = bindGroupLayout;
            descriptor.entryCount = entries.size();
            descriptor.entries = entries.data();
            texture.bindGroup = device.CreateBindGroup(&descriptor);
        }
    }

    const TextureResource &WgpuRenderer2D::Impl::getTexture(
        Core::Renderer::TextureHandle texture) const {
        const auto it = textures.find(texture);
        if (it != textures.end()) {
            return it->second;
        }
        return textures.at(0);
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
        m_impl->quadBatch.configure(createInfo.batching.initialQuadCapacity,
                                    createInfo.batching.maxQuadCapacity);
        m_impl->createDevice();
        m_impl->createOffscreenTarget();
        m_impl->createShaders();
        m_impl->createPipeline();
        m_impl->createTextureSampler();
        m_impl->ensureQuadBufferSize(
            std::max(1u, createInfo.batching.initialQuadCapacity));
        m_impl->createDefaultTexture();
    }

    void WgpuRenderer2D::destroy() {
        if (m_impl == nullptr) {
            return;
        }
        m_impl->commandEncoder = nullptr;
        m_impl->bindGroupLayout = nullptr;
        m_impl->quadPipeline = nullptr;
        m_impl->quadShader = nullptr;
        m_impl->textureSampler = nullptr;
        m_impl->quadBuffer = nullptr;
        m_impl->frameBuffer = nullptr;
        m_impl->textures.clear();
        m_impl->nextTextureHandle = 1;
        m_impl->offscreenTargetView = nullptr;
        m_impl->offscreenTarget = nullptr;
        m_impl->queue = nullptr;
        m_impl->device = nullptr;
        m_impl->adapter = nullptr;
        m_impl->instance = nullptr;
        m_impl->quadBatch.clear();
        m_impl->stats = {};
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
        m_impl->quadBatch.clear();
        m_impl->stats = {};
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

        if (!m_impl->quadBatch.empty()) {
            m_impl->quadBatch.sortForRendering();
            m_impl->ensureQuadBufferSize(m_impl->quadBatch.count());
            m_impl->queue.WriteBuffer(m_impl->quadBuffer, 0,
                                      m_impl->quadBatch.data(),
                                      m_impl->quadBatch.byteSize());
            m_impl->stats.uploadedBytes += m_impl->quadBatch.byteSize();

            FrameUniform frameUniform{};
            frameUniform.viewport[0] = static_cast<float>(m_impl->extent.width);
            frameUniform.viewport[1] =
                static_cast<float>(m_impl->extent.height);
            m_impl->queue.WriteBuffer(m_impl->frameBuffer, 0, &frameUniform,
                                      sizeof(frameUniform));

            renderPass.SetPipeline(m_impl->quadPipeline);
            for (const auto &run : m_impl->quadBatch.getDrawRuns()) {
                const auto &texture = m_impl->getTexture(run.texture);
                renderPass.SetBindGroup(0, texture.bindGroup);
                renderPass.Draw(6, run.instanceCount, 0, run.firstInstance);
                m_impl->stats.drawCallCount++;
            }
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

    Core::Renderer::Renderer2DStats WgpuRenderer2D::getStats() const noexcept {
        return m_impl->stats;
    }

    Core::Renderer::TextureHandle
    WgpuRenderer2D::createTexture(Core::Renderer::ITexture &texture) {
        if (m_impl->device == nullptr) {
            throw std::runtime_error("WgpuRenderer2D is not initialized");
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        std::unique_ptr<stbi_uc, StbiImageDeleter> pixels(stbi_load(
            texture.getPath().c_str(), &width, &height, &channels, 4));
        if (pixels == nullptr) {
            throw std::runtime_error("Failed to load texture: " +
                                     texture.getPath());
        }

        const auto handle = m_impl->createTextureFromPixels(
            pixels.get(), static_cast<uint32_t>(width),
            static_cast<uint32_t>(height), false);
        texture.setSize(
            {static_cast<float>(width), static_cast<float>(height)});
        texture.setHandle(handle);
        return handle;
    }

    Core::Renderer::TextureHandle WgpuRenderer2D::createTexture(
        const Core::Renderer::TextureCreateInfo &createInfo) {
        TextureSource texture(createInfo);
        return createTexture(texture);
    }

    void WgpuRenderer2D::destroyTexture(Core::Renderer::TextureHandle texture) {
        if (texture == 0) {
            return;
        }
        m_impl->textures.erase(texture);
    }

    void WgpuRenderer2D::drawQuad(const Core::Renderer::QuadProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }
        m_impl->quadBatch.push(makeQuadInstance(props, nullptr), props.texture);
        m_impl->stats.quadCount = m_impl->quadBatch.count();
    }

    void WgpuRenderer2D::drawRoundedQuad(
        const Core::Renderer::QuadProps &props,
        const Core::Renderer::RoundedBorderProps &roundedProps) {
        if (!m_impl->frameStarted) {
            return;
        }
        m_impl->quadBatch.push(makeQuadInstance(props, &roundedProps),
                               props.texture);
        m_impl->stats.quadCount = m_impl->quadBatch.count();
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

    wgpu::Device WgpuRenderer2D::getDevice() const { return m_impl->device; }
    wgpu::Queue WgpuRenderer2D::getQueue() const { return m_impl->queue; }

    wgpu::TextureView WgpuRenderer2D::getCurrentTargetView() const {
        return m_impl->offscreenTargetView;
    }

    [[nodiscard]] wgpu::TextureFormat WgpuRenderer2D::getTargetFormat() const {
        return m_impl->targetFormat;
    }

} // namespace Bess::Wgpu
