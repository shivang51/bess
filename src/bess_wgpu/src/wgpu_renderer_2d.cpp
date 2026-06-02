#include "bess_wgpu/wgpu_renderer_2d.h"
#include "bess_wgpu/piplines/quad_pipeline.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "glfw3webgpu.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <png.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Bess::Wgpu {
    namespace {
        using Bess::Core::Renderer::Color;
        using Bess::Core::Renderer::QuadRenderPass;
        using Bess::Core::Renderer::Renderer2DExtent;
        using Bess::Core::Renderer::Renderer2DTargetFormat;
        using Bess::Wgpu::Piplines::QuadInstance;

        bool isTransparent(const Core::Renderer::QuadProps &props,
                           const Core::Renderer::RoundedBorderProps *rounded) {
            if (props.renderPass == QuadRenderPass::Opaque) {
                return false;
            }
            if (props.renderPass == QuadRenderPass::Transparent) {
                return true;
            }

            if (props.color.a < 0.999f) {
                return true;
            }
            if (rounded != nullptr && rounded->color.a < 0.999f) {
                return true;
            }
            return false;
        }

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

        void makeQuadInstanceInPlace(
            QuadInstance& instance,
            const Core::Renderer::QuadProps &props,
            const Core::Renderer::RoundedBorderProps *roundedProps) {
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
            instance.padding = 0.f;

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
            } else {
                instance.radius[0] = 0.f;
                instance.radius[1] = 0.f;
                instance.radius[2] = 0.f;
                instance.radius[3] = 0.f;
                instance.borderSize[0] = 0.f;
                instance.borderSize[1] = 0.f;
                instance.borderSize[2] = 0.f;
                instance.borderSize[3] = 0.f;
                instance.borderColor[0] = 0.f;
                instance.borderColor[1] = 0.f;
                instance.borderColor[2] = 0.f;
                instance.borderColor[3] = 0.f;
            }
        }

        class QuadBatch {
          public:
            void configure(uint32_t initialCapacity, uint32_t maxCapacity) {
                m_maxCapacity = std::max(1u, maxCapacity);
                // Pre-allocate to max capacity to avoid reallocations and debug bounds checking overhead
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

            QuadInstance& push(Core::Renderer::TextureHandle texture) {
                if (m_instanceCount >= m_maxCapacity) {
                    throw std::runtime_error("WGPU quad batch capacity exceeded");
                }
                const uint32_t instanceIndex = m_instanceCount;
                if (m_drawRunsCount == 0 || m_drawRunsPtr[m_drawRunsCount - 1].texture != texture) {
                    m_drawRunsPtr[m_drawRunsCount++] = {.texture = texture,
                                                        .firstInstance = instanceIndex,
                                                        .instanceCount = 1};
                } else {
                    m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
                }
                return m_gpuInstancesPtr[m_instanceCount++];
            }

            void prepareForRendering(bool sortBackToFront) {
                if (sortBackToFront && m_instanceCount > 1) {
                    std::vector<uint32_t> indices(m_instanceCount);
                    uint32_t* indicesPtr = indices.data();
                    for (uint32_t i = 0; i < m_instanceCount; ++i) {
                        indicesPtr[i] = i;
                    }

                    std::stable_sort(
                        indices.begin(), indices.end(),
                        [this](uint32_t a, uint32_t b) {
                            if (m_gpuInstancesPtr[a].zIndex != m_gpuInstancesPtr[b].zIndex) {
                                return m_gpuInstancesPtr[a].zIndex < m_gpuInstancesPtr[b].zIndex;
                            }
                            return a < b;
                        });

                    std::vector<Core::Renderer::TextureHandle> textures(m_instanceCount);
                    Core::Renderer::TextureHandle* texPtr = textures.data();
                    for (uint32_t r = 0; r < m_drawRunsCount; ++r) {
                        const auto &run = m_drawRunsPtr[r];
                        for (uint32_t i = 0; i < run.instanceCount; ++i) {
                            texPtr[run.firstInstance + i] = run.texture;
                        }
                    }

                    std::vector<QuadInstance> sortedInstances(m_instanceCount);
                    QuadInstance* sortedPtr = sortedInstances.data();
                    m_drawRunsCount = 0;

                    for (uint32_t i = 0; i < m_instanceCount; ++i) {
                        uint32_t oldIdx = indicesPtr[i];
                        sortedPtr[i] = m_gpuInstancesPtr[oldIdx];
                        Core::Renderer::TextureHandle tex = texPtr[oldIdx];

                        if (m_drawRunsCount == 0 ||
                            m_drawRunsPtr[m_drawRunsCount - 1].texture != tex) {
                            m_drawRunsPtr[m_drawRunsCount++] = {.texture = tex,
                                                                .firstInstance = i,
                                                                .instanceCount = 1};
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
                return static_cast<uint64_t>(m_instanceCount) * sizeof(QuadInstance);
            }

            [[nodiscard]] const QuadInstance *data() const noexcept {
                return m_gpuInstancesPtr;
            }

            [[nodiscard]] const DrawRun *drawRunsData() const noexcept {
                return m_drawRunsPtr;
            }

            [[nodiscard]] uint32_t drawRunsCount() const noexcept {
                return m_drawRunsCount;
            }

          private:
            std::vector<QuadInstance> m_gpuInstances;
            std::vector<DrawRun> m_drawRuns;
            QuadInstance* m_gpuInstancesPtr = nullptr;
            DrawRun* m_drawRunsPtr = nullptr;
            uint32_t m_instanceCount = 0;
            uint32_t m_drawRunsCount = 0;
            uint32_t m_maxCapacity = 1;
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
        Piplines::SharedFrameBuffer sharedFrameBuffer;
        std::unique_ptr<Piplines::QuadPipeline> quadPipeline;
        wgpu::CommandEncoder commandEncoder;
        std::unordered_map<Core::Renderer::TextureHandle, TextureResource>
            textures;
        std::shared_ptr<WgpuTexture> defaultTexture;

        QuadBatch opaqueQuadBatch;
        QuadBatch transparentQuadBatch;
        Core::Renderer::Renderer2DStats stats;
        Color clearColor{0.f, 0.f, 0.f, 1.f};
        bool shouldClear = true;
        bool frameStarted = false;

        void createDevice();
        void createOffscreenTarget();
        void createDepthTarget();
        void createWindowSurface();
        void configureWindowSurface(uint32_t width, uint32_t height);
        void createDefaultTexture();
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
        descriptor.format = wgpu::TextureFormat::Depth24Plus;
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
        if (!quadPipeline) {
            return;
        }

        for (auto &[handle, texture] : textures) {
            texture.bindGroup = quadPipeline->createTextureBindGroup(
                texture.view, "TextureBindGroup_" + std::to_string(handle));
        }
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
        m_impl->opaqueQuadBatch.configure(createInfo.batching.initialQuadCapacity,
                                          createInfo.batching.maxQuadCapacity);
        m_impl->transparentQuadBatch.configure(
            createInfo.batching.initialQuadCapacity,
            createInfo.batching.maxQuadCapacity);
        m_impl->createDevice();
        m_impl->createWindowSurface();
        m_impl->createOffscreenTarget();
        m_impl->createDepthTarget();
        m_impl->sharedFrameBuffer.init(m_impl->device);
        m_impl->quadPipeline = std::make_unique<Piplines::QuadPipeline>();
        m_impl->quadPipeline->init(m_impl->device, m_impl->targetFormat,
                                   m_impl->sharedFrameBuffer.getBuffer(),
                                   m_impl->sharedFrameBuffer.getSize());
        if (m_impl->quadPipeline->ensureInstanceBufferSize(
                std::max(1u, createInfo.batching.initialQuadCapacity))) {
            m_impl->recreateTextureBindGroups();
        }
        m_impl->createDefaultTexture();
    }

    void WgpuRenderer2D::destroy() {
        if (m_impl == nullptr) {
            return;
        }
        m_impl->commandEncoder = nullptr;
        if (m_impl->quadPipeline) {
            m_impl->quadPipeline->destroy();
            m_impl->quadPipeline = nullptr;
        }
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
        m_impl->opaqueQuadBatch.clear();
        m_impl->transparentQuadBatch.clear();
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

        if (frameInfo.extent.width != 0 && frameInfo.extent.height != 0 &&
            (frameInfo.extent.width != m_impl->extent.width ||
             frameInfo.extent.height != m_impl->extent.height)) {
            resize(frameInfo.extent);
        }

        m_impl->clearColor = frameInfo.clearColor;
        m_impl->shouldClear = frameInfo.shouldClear;
        m_impl->opaqueQuadBatch.clear();
        m_impl->transparentQuadBatch.clear();
        m_impl->stats = {};

        m_impl->frameTargetTexture = frameInfo.targetTexture;
        m_impl->frameUsesSurface = frameInfo.targetTexture == 0;
        m_impl->frameStarted = true;
    }

    void WgpuRenderer2D::endFrame() {
        if (!m_impl->frameStarted) {
            return;
        }

        m_impl->commandEncoder = m_impl->device.CreateCommandEncoder();

        wgpu::RenderPassColorAttachment colorAttachment{};
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

        colorAttachment.view = targetView;
        colorAttachment.loadOp =
            m_impl->shouldClear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        colorAttachment.storeOp = wgpu::StoreOp::Store;
        colorAttachment.clearValue = toWgpuColor(m_impl->clearColor);

        wgpu::RenderPassDepthStencilAttachment depthAttachment{};
        depthAttachment.view = m_impl->depthTargetView;
        depthAttachment.depthLoadOp =
            m_impl->shouldClear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        depthAttachment.depthStoreOp = wgpu::StoreOp::Store;
        depthAttachment.depthClearValue = 1.0f;

        wgpu::RenderPassDescriptor renderPassDescriptor{};
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;
        renderPassDescriptor.depthStencilAttachment = &depthAttachment;

        wgpu::RenderPassEncoder renderPass =
            m_impl->commandEncoder.BeginRenderPass(&renderPassDescriptor);

        auto renderBatch = [&](QuadBatch &batch, bool sortBackToFront,
                               const wgpu::RenderPipeline &pipeline) {
            if (batch.empty()) {
                return;
            }

            batch.prepareForRendering(sortBackToFront);
            if (m_impl->quadPipeline->ensureInstanceBufferSize(batch.count())) {
                m_impl->recreateTextureBindGroups();
            }

            m_impl->quadPipeline->uploadInstances(m_impl->queue, batch.data(),
                                                  batch.byteSize());
            m_impl->stats.uploadedBytes += batch.byteSize();

            renderPass.SetPipeline(pipeline);
            
            const uint32_t runCount = batch.drawRunsCount();
            const DrawRun* runs = batch.drawRunsData();
            for (uint32_t i = 0; i < runCount; ++i) {
                const auto &run = runs[i];
                const auto &texture = m_impl->getTexture(run.texture);
                renderPass.SetBindGroup(0, texture.bindGroup);
                renderPass.Draw(6, run.instanceCount, 0, run.firstInstance);
                m_impl->stats.drawCallCount++;
            }
        };

        m_impl->sharedFrameBuffer.update(m_impl->queue, m_impl->extent.width,
                                         m_impl->extent.height);
        renderBatch(m_impl->opaqueQuadBatch, false,
                    m_impl->quadPipeline->getOpaquePipeline());
        renderBatch(m_impl->transparentQuadBatch, true,
                    m_impl->quadPipeline->getTransparentPipeline());

        renderPass.End();
        wgpu::CommandBuffer commandBuffer = m_impl->commandEncoder.Finish();
        m_impl->queue.Submit(1, &commandBuffer);

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

        const auto targetTexture =
            m_impl->lastCompletedTargetTexture != 0
                ? m_impl->getTexture(m_impl->lastCompletedTargetTexture).texture
                : m_impl->offscreenTarget;

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
        source.texture = targetTexture;
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
                mappedData + (static_cast<size_t>(row) * paddedBytesPerRow);
            uint8_t *dst =
                rgba.data() + (static_cast<size_t>(row) * unpaddedBytesPerRow);

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

    void
    WgpuRenderer2D::unregisterTexture(Core::Renderer::TextureHandle texture) {
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

        if (isTransparent(props, nullptr)) {
            makeQuadInstanceInPlace(m_impl->transparentQuadBatch.push(props.texture), props, nullptr);
        } else {
            makeQuadInstanceInPlace(m_impl->opaqueQuadBatch.push(props.texture), props, nullptr);
        }
        m_impl->stats.quadCount = m_impl->opaqueQuadBatch.count() +
                                  m_impl->transparentQuadBatch.count();
    }

    void WgpuRenderer2D::drawRoundedQuad(
        const Core::Renderer::QuadProps &props,
        const Core::Renderer::RoundedBorderProps &roundedProps) {
        if (!m_impl->frameStarted) {
            return;
        }

        if (isTransparent(props, &roundedProps)) {
            makeQuadInstanceInPlace(m_impl->transparentQuadBatch.push(props.texture), props, &roundedProps);
        } else {
            makeQuadInstanceInPlace(m_impl->opaqueQuadBatch.push(props.texture), props, &roundedProps);
        }
        m_impl->stats.quadCount = m_impl->opaqueQuadBatch.count() +
                                  m_impl->transparentQuadBatch.count();
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
