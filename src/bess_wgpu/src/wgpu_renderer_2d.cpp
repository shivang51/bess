#include "bess_wgpu/wgpu_renderer_2d.h"
#include "bess_core/renderer/font.h"
#include "bess_core/renderer/msdf_font.h"
#include "bess_core/renderer/subtexture.h"
#include "bess_wgpu/path_baker.h"
#include "bess_wgpu/piplines/custom_quad_pipeline.h"
#include "bess_wgpu/piplines/path_pipeline.h"
#include "bess_wgpu/piplines/primitive_pipeline.h"
#include "bess_wgpu/piplines/shadow_pipeline.h"
#include "bess_wgpu/text/msdf_text_pipeline.h"
#include "bess_wgpu/wgpu_shader.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "glfw3webgpu.h"
#include "wgpu_renderer_2d_batches.h"
#include "wgpu_renderer_2d_instances.h"
#include "wgpu_renderer_2d_readback.h"
#include "wgpu_renderer_2d_text.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Bess::Wgpu {
    using namespace Renderer2DDetail;

    using Core::Renderer::FontFile;
    using Core::Renderer::FontProps;
    using Core::Renderer::Glyph;
    typedef Core::Renderer::MsdfFontAtlas<Bess::Wgpu::WgpuTexture>
        MsdfFontAtlas;
    using Core::Renderer::Path2D;
    using Core::Renderer::PathCommand;
    using Core::Renderer::PathCommandKind;
    using Core::Renderer::PathCommandStroke;
    using Core::Renderer::PathProps;

    namespace {
        using Bess::Core::Renderer::Color;
        using Bess::Core::Renderer::Renderer2DExtent;
        using Bess::Wgpu::Piplines::PathCoverVertex;
        using Bess::Wgpu::Piplines::PathInstance;
        using Bess::Wgpu::Piplines::PathStencilVertex;

        using Bess::Wgpu::BakedPathSubmission;
        using Bess::Wgpu::bakePathFillAntiAlias;
        using Bess::Wgpu::bakePathSubmission;
        using Bess::Wgpu::makePathBakeMetrics;
        using Bess::Wgpu::PathBakeMetrics;
        using Bess::Wgpu::PathBatch;
        using Bess::Wgpu::PathStrokeBatch;
        using Bess::Wgpu::submitBakedPathSubmission;
        using Bess::Wgpu::submitPathCommands;

        constexpr wgpu::TextureFormat kDepthStencilFormat =
            wgpu::TextureFormat::Depth24PlusStencil8;
        constexpr const char *kDefaultFontFile =
            "assets/fonts/Roboto/Roboto-Regular.ttf";
        constexpr const char *kDefaultMsdfFontDirectory = "assets/bess_fonts";
        constexpr const char *kDefaultMsdfFontName = "bess_fonts_merged";
        constexpr float kFontOutlinePixelSize = 64.f;
        constexpr uint64_t kPathCacheMaxIdleFrames = 240;
        constexpr std::size_t kPathCachePruneThreshold = 512;
        constexpr std::size_t kPathCacheHardLimit = 2048;

        bool sameColor(const Color &a, const Color &b) noexcept {
            return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
        }

        bool samePickingId(const PickingId &a, const PickingId &b) noexcept {
            return a.runtimeId == b.runtimeId && a.info == b.info;
        }

        bool samePathBounds(const Core::Renderer::PathBounds &a,
                            const Core::Renderer::PathBounds &b) noexcept {
            if (a.valid != b.valid) {
                return false;
            }
            if (!a.valid) {
                return true;
            }
            return a.min == b.min && a.max == b.max;
        }

        bool samePathProps(const PathProps &a, const PathProps &b) noexcept {
            return sameColor(a.fillColor, b.fillColor) &&
                   sameColor(a.strokeColor, b.strokeColor) &&
                   a.strokeSize == b.strokeSize &&
                   a.miterLimit == b.miterLimit &&
                   a.curveTolerance == b.curveTolerance &&
                   a.renderFill == b.renderFill &&
                   samePickingId(a.id, b.id) && a.renderPass == b.renderPass &&
                   a.fillRule == b.fillRule && a.lineJoin == b.lineJoin &&
                   a.lineCap == b.lineCap && a.closePath == b.closePath;
        }

        bool samePathBakeMetrics(const PathBakeMetrics &a,
                                 const PathBakeMetrics &b) noexcept {
            return a.screenScale == b.screenScale &&
                   a.pixelWorldSize == b.pixelWorldSize;
        }

        PathBakeMetrics makePathBakeMetricsForTransform(
            Core::Renderer::RenderTransformMode transformMode,
            const float *cameraTransform,
            const Renderer2DExtent &extent) {
            if (transformMode == Core::Renderer::RenderTransformMode::Screen) {
                return {};
            }

            return makePathBakeMetrics(cameraTransform, extent);
        }

        PathBakeMetrics
        makePathBakeMetricsForProps(const PathProps &props,
                                    const float *cameraTransform,
                                    const Renderer2DExtent &extent) {
            return makePathBakeMetricsForTransform(
                props.transformMode, cameraTransform, extent);
        }

        bool supportsPresentMode(const wgpu::SurfaceCapabilities &capabilities,
                                 wgpu::PresentMode mode) noexcept {
            for (size_t i = 0; i < capabilities.presentModeCount; ++i) {
                if (capabilities.presentModes[i] == mode) {
                    return true;
                }
            }
            return false;
        }

        wgpu::PresentMode chooseSurfacePresentMode(
            const wgpu::SurfaceCapabilities &capabilities) noexcept {
            constexpr std::array preferredModes{
                wgpu::PresentMode::Mailbox,
                wgpu::PresentMode::Immediate,
                wgpu::PresentMode::FifoRelaxed,
                wgpu::PresentMode::Fifo,
            };

            for (wgpu::PresentMode mode : preferredModes) {
                if (supportsPresentMode(capabilities, mode)) {
                    return mode;
                }
            }

            return capabilities.presentModes[0];
        }

        struct CachedPathEntry {
            bool initialized = false;
            uint64_t revision = 0;
            std::size_t commandCount = 0;
            const PathCommand *commandData = nullptr;
            Core::Renderer::PathBounds bounds{};
            PathProps props{};
            PathBakeMetrics metrics{};
            BakedPathSubmission submission{};
            uint64_t lastUsedFrame = 0;
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
        wgpu::PresentMode surfacePresentMode = wgpu::PresentMode::Immediate;
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
        std::unique_ptr<Piplines::ShadowPipeline> shadowPipeline;
        std::unique_ptr<CustomQuadPipeline> customQuadPipeline;
        std::unique_ptr<Text::MsdfTextPipeline> textPipeline;
        wgpu::CommandEncoder commandEncoder;
        std::vector<wgpu::CommandBuffer> pendingCommandBuffers;
        std::unordered_map<Core::Renderer::TextureHandle, TextureResource>
            textures;
        std::shared_ptr<WgpuTexture> defaultTexture;

        PrimitiveBatch opaquePrimitiveBatch;
        PrimitiveBatch transparentPrimitiveBatch;
        CustomQuadBatch opaqueCustomQuadBatch;
        CustomQuadBatch transparentCustomQuadBatch;
        ShadowBatch shadowBatch;
        PathStrokeBatch opaquePathStrokeBatch;
        PathStrokeBatch transparentPathStrokeBatch;
        PathBatch opaquePathBatch;
        PathBatch transparentPathBatch;
        Text::MsdfTextBatch textBatch;
        std::vector<PathCommand> activePathCommands;
        PathProps activePathProps;
        bool pathStarted = false;
        uint64_t activePathSubmitOrder = 0;
        std::unique_ptr<FontFile> fontFile;
        std::unique_ptr<MsdfFontAtlas> msdfFontAtlas;
        std::vector<TransparentDrawItem> transparentDrawItems;
        std::vector<PathCommand> textPathCommandsScratch;
        std::unordered_map<const Path2D *, CachedPathEntry> pathCache;
        Core::Renderer::Renderer2DStats stats;
        Color clearColor{0.f, 0.f, 0.f, 1.f};
        bool shouldClear = true;
        bool frameStarted = false;
        uint64_t frameSequence = 0;
        uint64_t nextDrawSubmitOrder = 1;
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
        [[nodiscard]] uint64_t nextSubmitOrder() noexcept;
        void prunePathCache();
        [[nodiscard]] const BakedPathSubmission &
        cachedPathSubmission(const Path2D &path,
                             const PathProps &props,
                             const PathBakeMetrics &metrics);
        void queueCommandBuffer(wgpu::CommandBuffer commandBuffer);
        void flushPendingCommandBuffers();
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
                   shadowBatch.count() + textBatch.count();
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
                             wgpu::Adapter adapter,
                             wgpu::StringView message) {
                if (status != wgpu::RequestAdapterStatus::Success) {
                    adapterResult.error =
                        message.data != nullptr
                            ? std::string(message.data, message.length)
                            : "unknown adapter error";
                    return;
                }
                adapterResult.adapter = std::move(adapter);
            };

        instance.WaitAny(
            instance.RequestAdapter(&adapterOptions,
                                    wgpu::CallbackMode::WaitAnyOnly,
                                    adapterCallback),
            UINT64_MAX);
        adapter = adapterResult.adapter;
        if (adapter == nullptr) {
            throw std::runtime_error("Failed to request WebGPU adapter: " +
                                     adapterResult.error);
        }

        wgpu::DeviceDescriptor deviceDescriptor{};
        deviceDescriptor.SetUncapturedErrorCallback(
            [](const wgpu::Device &,
               wgpu::ErrorType type,
               wgpu::StringView message) {
                BESS_ERROR("Dawn Validation Error [{}]: {}",
                           static_cast<int>(type),
                           std::string_view(message.data, message.length));
            });
        deviceDescriptor.SetDeviceLostCallback(
            wgpu::CallbackMode::AllowSpontaneous,
            [](const wgpu::Device &,
               wgpu::DeviceLostReason reason,
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
        descriptor.size = {
            std::max(1u, extent.width), std::max(1u, extent.height), 1};
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
        descriptor.size = {
            std::max(1u, extent.width), std::max(1u, extent.height), 1};
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

    uint64_t WgpuRenderer2D::Impl::nextSubmitOrder() noexcept {
        const uint64_t order = nextDrawSubmitOrder++;
        if (nextDrawSubmitOrder == 0) {
            nextDrawSubmitOrder = 1;
        }
        return order;
    }

    void WgpuRenderer2D::Impl::prunePathCache() {
        if (pathCache.size() < kPathCachePruneThreshold) {
            return;
        }

        for (auto it = pathCache.begin(); it != pathCache.end();) {
            if (frameSequence > it->second.lastUsedFrame &&
                frameSequence - it->second.lastUsedFrame >
                    kPathCacheMaxIdleFrames) {
                it = pathCache.erase(it);
            } else {
                ++it;
            }
        }

        while (pathCache.size() > kPathCacheHardLimit) {
            auto oldest = pathCache.begin();
            auto it = pathCache.begin();
            ++it;
            for (; it != pathCache.end(); ++it) {
                if (it->second.lastUsedFrame < oldest->second.lastUsedFrame) {
                    oldest = it;
                }
            }
            pathCache.erase(oldest);
        }
    }

    const BakedPathSubmission &
    WgpuRenderer2D::Impl::cachedPathSubmission(const Path2D &path,
                                               const PathProps &props,
                                               const PathBakeMetrics &metrics) {
        CachedPathEntry &entry = pathCache[&path];
        const auto bounds = path.bounds();
        const auto commandCount = path.commandCount();
        const PathCommand *commandData = path.data();
        const bool cacheHit = entry.initialized &&
                              entry.revision == path.revision() &&
                              entry.commandCount == commandCount &&
                              entry.commandData == commandData &&
                              samePathBounds(entry.bounds, bounds) &&
                              samePathProps(entry.props, props) &&
                              samePathBakeMetrics(entry.metrics, metrics);

        if (!cacheHit) {
            const auto commands = path.commands();
            entry.revision = path.revision();
            entry.commandCount = commandCount;
            entry.commandData = commandData;
            entry.bounds = bounds;
            entry.props = props;
            entry.metrics = metrics;
            entry.submission = bakePathSubmission(commands, props, metrics);
            entry.initialized = true;
        }

        entry.lastUsedFrame = frameSequence;
        return entry.submission;
    }

    void WgpuRenderer2D::Impl::queueCommandBuffer(
        wgpu::CommandBuffer commandBuffer) {
        if (commandBuffer == nullptr) {
            return;
        }
        pendingCommandBuffers.push_back(std::move(commandBuffer));
    }

    void WgpuRenderer2D::Impl::flushPendingCommandBuffers() {
        if (pendingCommandBuffers.empty()) {
            return;
        }

        queue.Submit(pendingCommandBuffers.size(),
                     pendingCommandBuffers.data());
        pendingCommandBuffers.clear();
        beginRecordedPickingReadbackMaps();
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
                wgpu::MapMode::Read,
                0,
                slot->mappedSize,
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
            const auto *srcRow = mappedData + (static_cast<size_t>(row) *
                                               newestReady->paddedBytesPerRow);
            for (uint32_t col = 0; col < result.width; ++col) {
                const size_t pixelIndex =
                    (static_cast<size_t>(row) * result.width) + col;
                const auto *src =
                    srcRow + (static_cast<size_t>(col) * sizeof(uint32_t) * 2);
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

        BESS_ASSERT(false,
                    "Requested texture handle {} not found in renderer",
                    texture);
        return textures.at(defaultTexture->getHandle());
    }

    WgpuRenderer2D::WgpuRenderer2D() : m_impl(std::make_unique<Impl>()) {
    }

    WgpuRenderer2D::~WgpuRenderer2D() {
        destroy();
    }

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
        m_impl->shadowBatch.configure(createInfo.batching.initialQuadCapacity,
                                      createInfo.batching.maxQuadCapacity);
        m_impl->createDevice();
        m_impl->createWindowSurface();
        m_impl->createOffscreenTarget();
        m_impl->createDepthTarget();
        m_impl->sharedFrameBuffer.init(m_impl->device);
        m_impl->pickingFormat = toWgpuFormat(createInfo.pickingFormat);
        m_impl->primitivePipeline =
            std::make_unique<Piplines::PrimitivePipeline>();
        m_impl->primitivePipeline->init(m_impl->device,
                                        m_impl->targetFormat,
                                        m_impl->sharedFrameBuffer.getBuffer(),
                                        m_impl->sharedFrameBuffer.getSize(),
                                        m_impl->pickingFormat);
        m_impl->pathPipeline = std::make_unique<Piplines::PathPipeline>();
        m_impl->pathPipeline->init(m_impl->device,
                                   m_impl->targetFormat,
                                   m_impl->sharedFrameBuffer.getBuffer(),
                                   m_impl->sharedFrameBuffer.getSize(),
                                   m_impl->pickingFormat);
        m_impl->shadowPipeline = std::make_unique<Piplines::ShadowPipeline>();
        m_impl->shadowPipeline->init(m_impl->device,
                                     m_impl->targetFormat,
                                     m_impl->sharedFrameBuffer.getBuffer(),
                                     m_impl->sharedFrameBuffer.getSize(),
                                     m_impl->pickingFormat);
        m_impl->customQuadPipeline = std::make_unique<CustomQuadPipeline>();
        m_impl->customQuadPipeline->init(m_impl->device,
                                         m_impl->targetFormat,
                                         m_impl->sharedFrameBuffer.getBuffer(),
                                         m_impl->sharedFrameBuffer.getSize(),
                                         m_impl->pickingFormat);
        if (m_impl->primitivePipeline->ensureInstanceBufferSize(
                std::max(1u, createInfo.batching.initialQuadCapacity))) {
            m_impl->recreateTextureBindGroups();
        }
        static_cast<void>(m_impl->customQuadPipeline->ensureInstanceBufferSize(
            std::max(1u, createInfo.batching.initialQuadCapacity)));
        static_cast<void>(m_impl->shadowPipeline->ensureInstanceBufferSize(
            std::max(1u, createInfo.batching.initialQuadCapacity)));
        m_impl->createDefaultTexture();

        m_impl->msdfFontAtlas = std::make_unique<MsdfFontAtlas>();
        if (m_impl->msdfFontAtlas->load(kDefaultMsdfFontDirectory,
                                        kDefaultMsdfFontName)) {
            m_impl->textPipeline = std::make_unique<Text::MsdfTextPipeline>();
            m_impl->textPipeline->init(
                m_impl->device,
                m_impl->targetFormat,
                m_impl->sharedFrameBuffer.getBuffer(),
                m_impl->sharedFrameBuffer.getSize(),
                m_impl->pickingFormat,
                m_impl->msdfFontAtlas->getTexture()->getResource());
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
        if (m_impl->shadowPipeline) {
            m_impl->shadowPipeline->destroy();
            m_impl->shadowPipeline = nullptr;
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
        m_impl->shadowBatch.clear();
        m_impl->opaquePathStrokeBatch.clear();
        m_impl->transparentPathStrokeBatch.clear();
        m_impl->opaquePathBatch.clear();
        m_impl->transparentPathBatch.clear();
        m_impl->textBatch.clear();
        m_impl->activePathCommands.clear();
        m_impl->textPathCommandsScratch.clear();
        m_impl->pathCache.clear();
        m_impl->pendingCommandBuffers.clear();
        m_impl->fontFile = nullptr;
        m_impl->pathStarted = false;
        m_impl->activePathSubmitOrder = 0;
        m_impl->frameSequence = 0;
        m_impl->nextDrawSubmitOrder = 1;
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
        surfacePresentMode = chooseSurfacePresentMode(capabilities);
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
            m_impl->flushPendingCommandBuffers();
            m_impl->createOffscreenTarget();
            m_impl->createDepthTarget();
        }
    }

    void WgpuRenderer2D::beginFrame(
        const Core::Renderer::Renderer2DFrameInfo &frameInfo) {
        if (m_impl->device == nullptr) {
            throw std::runtime_error("WgpuRenderer2D is not initialized");
        }
        m_impl->flushPendingCommandBuffers();
        m_impl->processAsyncEvents();
        ++m_impl->frameSequence;
        if (m_impl->frameSequence == 0) {
            m_impl->frameSequence = 1;
        }
        m_impl->nextDrawSubmitOrder = 1;
        m_impl->prunePathCache();

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
        m_impl->shadowBatch.clear();
        m_impl->opaquePathStrokeBatch.clear();
        m_impl->transparentPathStrokeBatch.clear();
        m_impl->opaquePathBatch.clear();
        m_impl->transparentPathBatch.clear();
        m_impl->textBatch.clear();
        m_impl->activePathCommands.clear();
        m_impl->pathStarted = false;
        m_impl->activePathSubmitOrder = 0;
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

        if (m_impl->pickingFormat != wgpu::TextureFormat::Undefined &&
            m_impl->pickingTextureHandle != 0) {
            const auto &pickingRes =
                m_impl->getTexture(m_impl->pickingTextureHandle);
            colorAttachments[1].view = pickingRes.view;
            colorAttachments[1].loadOp = wgpu::LoadOp::Clear;
            colorAttachments[1].storeOp = wgpu::StoreOp::Store;
            colorAttachments[1].clearValue = {
                static_cast<double>(PickingId::invalidRuntimeId),
                0.0,
                0.0,
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
        m_impl->shadowBatch.prepareForRendering();
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

        const uint32_t totalShadowInstanceCount = m_impl->shadowBatch.count();

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

        const uint32_t opaquePathInstanceOffset = 0;
        const uint32_t transparentPathInstanceOffset =
            m_impl->opaquePathBatch.instanceCount();
        const uint32_t opaquePathStrokeInstanceOffset =
            transparentPathInstanceOffset +
            m_impl->transparentPathBatch.instanceCount();
        const uint32_t transparentPathStrokeInstanceOffset =
            opaquePathStrokeInstanceOffset +
            m_impl->opaquePathStrokeBatch.instanceCount();
        const uint32_t totalPathInstanceCount =
            transparentPathStrokeInstanceOffset +
            m_impl->transparentPathStrokeBatch.instanceCount();

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

        if (totalShadowInstanceCount > 0) {
            static_cast<void>(m_impl->shadowPipeline->ensureInstanceBufferSize(
                totalShadowInstanceCount));
        }

        if (totalTextGlyphCount > 0 && m_impl->textPipeline != nullptr) {
            static_cast<void>(m_impl->textPipeline->ensureInstanceBufferSize(
                totalTextGlyphCount));
        }

        if (!m_impl->opaquePrimitiveBatch.empty()) {
            m_impl->primitivePipeline->uploadInstances(
                m_impl->queue,
                m_impl->opaquePrimitiveBatch.data(),
                m_impl->opaquePrimitiveBatch.byteSize(),
                opaqueInstanceOffset * sizeof(Piplines::PrimitiveInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->opaquePrimitiveBatch.byteSize();
        }

        if (!m_impl->transparentPrimitiveBatch.empty()) {
            m_impl->primitivePipeline->uploadInstances(
                m_impl->queue,
                m_impl->transparentPrimitiveBatch.data(),
                m_impl->transparentPrimitiveBatch.byteSize(),
                transparentInstanceOffset *
                    sizeof(Piplines::PrimitiveInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->transparentPrimitiveBatch.byteSize();
        }

        if (!m_impl->opaqueCustomQuadBatch.empty()) {
            m_impl->customQuadPipeline->uploadInstances(
                m_impl->queue,
                m_impl->opaqueCustomQuadBatch.data(),
                m_impl->opaqueCustomQuadBatch.byteSize(),
                opaqueCustomInstanceOffset * sizeof(CustomQuadInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->opaqueCustomQuadBatch.byteSize();
        }

        if (!m_impl->transparentCustomQuadBatch.empty()) {
            m_impl->customQuadPipeline->uploadInstances(
                m_impl->queue,
                m_impl->transparentCustomQuadBatch.data(),
                m_impl->transparentCustomQuadBatch.byteSize(),
                transparentCustomInstanceOffset * sizeof(CustomQuadInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->transparentCustomQuadBatch.byteSize();
        }

        if (!m_impl->shadowBatch.empty()) {
            m_impl->shadowPipeline->uploadInstances(
                m_impl->queue,
                m_impl->shadowBatch.data(),
                m_impl->shadowBatch.byteSize());
            m_impl->stats.uploadedBytes += m_impl->shadowBatch.byteSize();
        }

        if (!m_impl->textBatch.empty() && m_impl->textPipeline != nullptr) {
            m_impl->textPipeline->uploadInstances(m_impl->queue,
                                                  m_impl->textBatch.data(),
                                                  m_impl->textBatch.byteSize());
            m_impl->stats.uploadedBytes += m_impl->textBatch.byteSize();
        }

        m_impl->stats.quadCount =
            totalInstanceCount + totalCustomInstanceCount +
            totalShadowInstanceCount + totalTextGlyphCount;

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

        if (totalPathInstanceCount > 0) {
            static_cast<void>(m_impl->pathPipeline->ensureInstanceBufferSize(
                totalPathInstanceCount));
        }

        if (!m_impl->opaquePathBatch.empty()) {
            m_impl->pathPipeline->uploadStencilVertices(
                m_impl->queue,
                m_impl->opaquePathBatch.stencilData(),
                m_impl->opaquePathBatch.stencilByteSize(),
                opaqueStencilVertexOffset * sizeof(PathStencilVertex));
            m_impl->pathPipeline->uploadCoverVertices(
                m_impl->queue,
                m_impl->opaquePathBatch.coverData(),
                m_impl->opaquePathBatch.coverByteSize(),
                opaqueCoverVertexOffset * sizeof(PathCoverVertex));
            m_impl->pathPipeline->uploadInstances(
                m_impl->queue,
                m_impl->opaquePathBatch.instanceData(),
                m_impl->opaquePathBatch.instanceByteSize(),
                opaquePathInstanceOffset * sizeof(PathInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->opaquePathBatch.stencilByteSize() +
                m_impl->opaquePathBatch.coverByteSize() +
                m_impl->opaquePathBatch.instanceByteSize();
        }

        if (!m_impl->transparentPathBatch.empty()) {
            m_impl->pathPipeline->uploadStencilVertices(
                m_impl->queue,
                m_impl->transparentPathBatch.stencilData(),
                m_impl->transparentPathBatch.stencilByteSize(),
                transparentStencilVertexOffset * sizeof(PathStencilVertex));
            m_impl->pathPipeline->uploadCoverVertices(
                m_impl->queue,
                m_impl->transparentPathBatch.coverData(),
                m_impl->transparentPathBatch.coverByteSize(),
                transparentCoverVertexOffset * sizeof(PathCoverVertex));
            m_impl->pathPipeline->uploadInstances(
                m_impl->queue,
                m_impl->transparentPathBatch.instanceData(),
                m_impl->transparentPathBatch.instanceByteSize(),
                transparentPathInstanceOffset * sizeof(PathInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->transparentPathBatch.stencilByteSize() +
                m_impl->transparentPathBatch.coverByteSize() +
                m_impl->transparentPathBatch.instanceByteSize();
        }

        if (!m_impl->opaquePathStrokeBatch.empty()) {
            m_impl->pathPipeline->uploadStrokeVertices(
                m_impl->queue,
                m_impl->opaquePathStrokeBatch.data(),
                m_impl->opaquePathStrokeBatch.byteSize(),
                opaqueStrokeVertexOffset * sizeof(PathCoverVertex));
            m_impl->pathPipeline->uploadInstances(
                m_impl->queue,
                m_impl->opaquePathStrokeBatch.instanceData(),
                m_impl->opaquePathStrokeBatch.instanceByteSize(),
                opaquePathStrokeInstanceOffset * sizeof(PathInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->opaquePathStrokeBatch.byteSize() +
                m_impl->opaquePathStrokeBatch.instanceByteSize();
        }

        if (!m_impl->transparentPathStrokeBatch.empty()) {
            m_impl->pathPipeline->uploadStrokeVertices(
                m_impl->queue,
                m_impl->transparentPathStrokeBatch.data(),
                m_impl->transparentPathStrokeBatch.byteSize(),
                transparentStrokeVertexOffset * sizeof(PathCoverVertex));
            m_impl->pathPipeline->uploadInstances(
                m_impl->queue,
                m_impl->transparentPathStrokeBatch.instanceData(),
                m_impl->transparentPathStrokeBatch.instanceByteSize(),
                transparentPathStrokeInstanceOffset * sizeof(PathInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->transparentPathStrokeBatch.byteSize() +
                m_impl->transparentPathStrokeBatch.instanceByteSize();
        }

        m_impl->sharedFrameBuffer.setCameraTransform(m_impl->cameraTransform);
        m_impl->sharedFrameBuffer.update(
            m_impl->queue, m_impl->extent.width, m_impl->extent.height);

        wgpu::RenderPassEncoder renderPass =
            m_impl->commandEncoder.BeginRenderPass(&renderPassDescriptor);

        enum class RenderPipelineKind : uint8_t {
            PrimitiveOpaque,
            PrimitiveTransparent,
            CustomQuadOpaque,
            CustomQuadTransparent,
            Shadow,
            PathStencilNonZero,
            PathStencilEvenOdd,
            PathCoverOpaque,
            PathCoverTransparent,
            PathStrokeOpaque,
            PathStrokeTransparent,
            Text,
        };

        struct RenderPipelineKey {
            RenderPipelineKind kind = RenderPipelineKind::PrimitiveOpaque;
            uint64_t resource = 0;

            [[nodiscard]] bool
            operator==(const RenderPipelineKey &other) const noexcept {
                return kind == other.kind && resource == other.resource;
            }
        };

        enum class RenderBindGroupKind : uint8_t {
            PrimitiveTexture,
            CustomQuad,
            Shadow,
            Path,
            Text,
        };

        struct RenderBindGroupKey {
            RenderBindGroupKind kind = RenderBindGroupKind::PrimitiveTexture;
            uint64_t resource = 0;

            [[nodiscard]] bool
            operator==(const RenderBindGroupKey &other) const noexcept {
                return kind == other.kind && resource == other.resource;
            }
        };

        struct RenderPassStateCache {
            bool hasPipeline = false;
            bool hasBindGroup = false;
            RenderPipelineKey pipelineKey{};
            RenderBindGroupKey bindGroupKey{};

            void setPipeline(wgpu::RenderPassEncoder &pass,
                             RenderPipelineKey key,
                             const wgpu::RenderPipeline &pipeline) {
                if (!hasPipeline || !(pipelineKey == key)) {
                    pass.SetPipeline(pipeline);
                    pipelineKey = key;
                    hasPipeline = true;
                }
            }

            void setBindGroup(wgpu::RenderPassEncoder &pass,
                              RenderBindGroupKey key,
                              const wgpu::BindGroup &bindGroup) {
                if (!hasBindGroup || !(bindGroupKey == key)) {
                    pass.SetBindGroup(0, bindGroup);
                    bindGroupKey = key;
                    hasBindGroup = true;
                }
            }
        };

        RenderPassStateCache passState;

        auto renderBatch = [&](PrimitiveBatch &batch,
                               uint32_t instanceOffset,
                               const wgpu::RenderPipeline &pipeline,
                               RenderPipelineKey pipelineKey) {
            if (batch.empty()) {
                return;
            }

            passState.setPipeline(renderPass, pipelineKey, pipeline);

            const uint32_t runCount = batch.drawRunsCount();
            const DrawRun *runs = batch.drawRunsData();
            for (uint32_t i = 0; i < runCount; ++i) {
                const auto &run = runs[i];
                const auto &texture = m_impl->getTexture(run.texture);
                passState.setBindGroup(
                    renderPass,
                    {RenderBindGroupKind::PrimitiveTexture, run.texture},
                    texture.bindGroup);
                renderPass.Draw(6,
                                run.instanceCount,
                                0,
                                instanceOffset + run.firstInstance);
                m_impl->stats.drawCallCount++;
            }
        };

        auto renderPrimitiveRun = [&](const DrawRun &run,
                                      uint32_t instanceOffset,
                                      const wgpu::RenderPipeline &pipeline,
                                      RenderPipelineKey pipelineKey) {
            if (run.instanceCount == 0) {
                return;
            }

            passState.setPipeline(renderPass, pipelineKey, pipeline);
            const auto &texture = m_impl->getTexture(run.texture);
            passState.setBindGroup(
                renderPass,
                {RenderBindGroupKind::PrimitiveTexture, run.texture},
                texture.bindGroup);
            renderPass.Draw(
                6, run.instanceCount, 0, instanceOffset + run.firstInstance);
            m_impl->stats.drawCallCount++;
        };

        auto renderCustomQuadRun = [&](const CustomQuadDrawRun &run,
                                       uint32_t instanceOffset,
                                       bool transparent) {
            if (run.instanceCount == 0) {
                return;
            }

            passState.setPipeline(
                renderPass,
                {transparent ? RenderPipelineKind::CustomQuadTransparent
                             : RenderPipelineKind::CustomQuadOpaque,
                 run.shader},
                m_impl->customQuadPipeline->getPipeline(run.shader,
                                                        transparent));
            passState.setBindGroup(renderPass,
                                   {RenderBindGroupKind::CustomQuad, 0},
                                   m_impl->customQuadPipeline->getBindGroup());
            m_impl->customQuadPipeline->drawInstances(renderPass,
                                                      instanceOffset +
                                                          run.firstInstance,
                                                      run.instanceCount);
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
                renderCustomQuadRun(runs[i], instanceOffset, transparent);
            }
        };

        auto renderShadowRun = [&](const ShadowDrawRun &run) {
            if (run.instanceCount == 0) {
                return;
            }

            passState.setPipeline(renderPass,
                                  {RenderPipelineKind::Shadow, 0},
                                  m_impl->shadowPipeline->getPipeline());
            passState.setBindGroup(renderPass,
                                   {RenderBindGroupKind::Shadow, 0},
                                   m_impl->shadowPipeline->getBindGroup());
            m_impl->shadowPipeline->drawInstances(
                renderPass, run.firstInstance, run.instanceCount);
            m_impl->stats.drawCallCount++;
        };

        auto renderPathRange = [&](const PathDrawRange &range,
                                   uint32_t stencilVertexOffset,
                                   uint32_t coverVertexOffset,
                                   uint32_t instanceOffset,
                                   bool transparent) {
            if (range.stencilVertexCount == 0 || range.coverVertexCount == 0) {
                return;
            }

            passState.setBindGroup(renderPass,
                                   {RenderBindGroupKind::Path, 0},
                                   m_impl->pathPipeline->getBindGroup());
            passState.setPipeline(
                renderPass,
                {range.evenOddFill ? RenderPipelineKind::PathStencilEvenOdd
                                   : RenderPipelineKind::PathStencilNonZero,
                 0},
                m_impl->pathPipeline->getStencilPipeline(range.evenOddFill));
            renderPass.SetVertexBuffer(
                0,
                m_impl->pathPipeline->getStencilVertexBuffer(),
                static_cast<uint64_t>(stencilVertexOffset +
                                      range.firstStencilVertex) *
                    sizeof(PathStencilVertex),
                static_cast<uint64_t>(range.stencilVertexCount) *
                    sizeof(PathStencilVertex));
            renderPass.SetVertexBuffer(
                1,
                m_impl->pathPipeline->getInstanceBuffer(),
                static_cast<uint64_t>(instanceOffset + range.firstInstance) *
                    sizeof(PathInstance),
                sizeof(PathInstance));
            renderPass.Draw(range.stencilVertexCount, 1, 0, 0);

            passState.setPipeline(
                renderPass,
                {transparent ? RenderPipelineKind::PathCoverTransparent
                             : RenderPipelineKind::PathCoverOpaque,
                 0},
                m_impl->pathPipeline->getCoverPipeline(transparent));
            renderPass.SetVertexBuffer(
                0,
                m_impl->pathPipeline->getCoverVertexBuffer(),
                static_cast<uint64_t>(coverVertexOffset +
                                      range.firstCoverVertex) *
                    sizeof(PathCoverVertex),
                static_cast<uint64_t>(range.coverVertexCount) *
                    sizeof(PathCoverVertex));
            renderPass.SetVertexBuffer(
                1,
                m_impl->pathPipeline->getInstanceBuffer(),
                static_cast<uint64_t>(instanceOffset + range.firstInstance) *
                    sizeof(PathInstance),
                sizeof(PathInstance));
            renderPass.Draw(range.coverVertexCount, 1, 0, 0);
            m_impl->stats.drawCallCount += 2;
        };

        auto renderPathBatch = [&](const PathBatch &batch,
                                   uint32_t stencilVertexOffset,
                                   uint32_t coverVertexOffset,
                                   uint32_t instanceOffset,
                                   bool transparent) {
            if (batch.empty()) {
                return;
            }

            const PathDrawRange *ranges = batch.drawRanges();
            const uint32_t rangeCount = batch.drawCount();
            for (uint32_t i = 0; i < rangeCount; ++i) {
                renderPathRange(ranges[i],
                                stencilVertexOffset,
                                coverVertexOffset,
                                instanceOffset,
                                transparent);
            }
        };

        auto renderPathStrokeRange = [&](const PathStrokeDrawRange &range,
                                         uint32_t vertexOffset,
                                         uint32_t instanceOffset,
                                         bool transparent) {
            if (range.vertexCount == 0) {
                return;
            }

            passState.setBindGroup(renderPass,
                                   {RenderBindGroupKind::Path, 0},
                                   m_impl->pathPipeline->getBindGroup());
            passState.setPipeline(
                renderPass,
                {transparent ? RenderPipelineKind::PathStrokeTransparent
                             : RenderPipelineKind::PathStrokeOpaque,
                 0},
                m_impl->pathPipeline->getStrokePipeline(transparent));
            renderPass.SetVertexBuffer(
                0,
                m_impl->pathPipeline->getStrokeVertexBuffer(),
                static_cast<uint64_t>(vertexOffset + range.firstVertex) *
                    sizeof(PathCoverVertex),
                static_cast<uint64_t>(range.vertexCount) *
                    sizeof(PathCoverVertex));
            renderPass.SetVertexBuffer(
                1,
                m_impl->pathPipeline->getInstanceBuffer(),
                static_cast<uint64_t>(instanceOffset + range.firstInstance) *
                    sizeof(PathInstance),
                sizeof(PathInstance));
            renderPass.Draw(range.vertexCount, 1, 0, 0);
            m_impl->stats.drawCallCount++;
        };

        auto renderPathStrokeBatch = [&](const PathStrokeBatch &batch,
                                         uint32_t vertexOffset,
                                         uint32_t instanceOffset,
                                         bool transparent) {
            if (batch.empty()) {
                return;
            }

            const PathStrokeDrawRange *ranges = batch.drawRanges();
            const uint32_t rangeCount = batch.drawCount();
            for (uint32_t i = 0; i < rangeCount; ++i) {
                renderPathStrokeRange(
                    ranges[i], vertexOffset, instanceOffset, transparent);
            }
        };

        auto renderTextRun = [&](const Text::TextDrawRun &run) {
            if (run.glyphCount == 0 || m_impl->textPipeline == nullptr) {
                return;
            }

            passState.setPipeline(renderPass,
                                  {RenderPipelineKind::Text, 0},
                                  m_impl->textPipeline->getPipeline());
            passState.setBindGroup(renderPass,
                                   {RenderBindGroupKind::Text, 0},
                                   m_impl->textPipeline->getBindGroup());
            m_impl->textPipeline->drawInstances(
                renderPass, run.firstGlyph, run.glyphCount);
            m_impl->stats.drawCallCount++;
        };

        renderBatch(m_impl->opaquePrimitiveBatch,
                    opaqueInstanceOffset,
                    m_impl->primitivePipeline->getOpaquePipeline(),
                    {RenderPipelineKind::PrimitiveOpaque, 0});
        renderCustomQuadBatch(
            m_impl->opaqueCustomQuadBatch, opaqueCustomInstanceOffset, false);
        renderPathBatch(m_impl->opaquePathBatch,
                        opaqueStencilVertexOffset,
                        opaqueCoverVertexOffset,
                        opaquePathInstanceOffset,
                        false);
        renderPathStrokeBatch(m_impl->opaquePathStrokeBatch,
                              opaqueStrokeVertexOffset,
                              opaquePathStrokeInstanceOffset,
                              false);

        m_impl->transparentDrawItems.clear();
        m_impl->transparentDrawItems.reserve(
            m_impl->shadowBatch.drawRunsCount() +
            m_impl->transparentPrimitiveBatch.drawRunsCount() +
            m_impl->transparentCustomQuadBatch.drawRunsCount() +
            m_impl->transparentPathBatch.drawCount() +
            m_impl->transparentPathStrokeBatch.drawCount() +
            m_impl->textBatch.drawRunsCount());

        const ShadowDrawRun *shadowRuns = m_impl->shadowBatch.drawRunsData();
        for (uint32_t i = 0; i < m_impl->shadowBatch.drawRunsCount(); ++i) {
            m_impl->transparentDrawItems.push_back({
                .kind = TransparentDrawKind::Shadow,
                .zIndex = shadowRuns[i].zIndex,
                .index = i,
                .order = shadowRuns[i].submitOrder,
            });
        }

        const DrawRun *transparentPrimitiveRuns =
            m_impl->transparentPrimitiveBatch.drawRunsData();
        for (uint32_t i = 0;
             i < m_impl->transparentPrimitiveBatch.drawRunsCount();
             ++i) {
            m_impl->transparentDrawItems.push_back({
                .kind = TransparentDrawKind::Primitive,
                .zIndex = transparentPrimitiveRuns[i].zIndex,
                .index = i,
                .order = transparentPrimitiveRuns[i].submitOrder,
            });
        }

        const CustomQuadDrawRun *transparentCustomQuadRuns =
            m_impl->transparentCustomQuadBatch.drawRunsData();
        for (uint32_t i = 0;
             i < m_impl->transparentCustomQuadBatch.drawRunsCount();
             ++i) {
            m_impl->transparentDrawItems.push_back({
                .kind = TransparentDrawKind::CustomQuad,
                .zIndex = transparentCustomQuadRuns[i].zIndex,
                .index = i,
                .order = transparentCustomQuadRuns[i].submitOrder,
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
                .order = transparentPathRanges[i].submitOrder,
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
                .order = transparentPathStrokeRanges[i].submitOrder,
            });
        }

        const Text::TextDrawRun *textRuns = m_impl->textBatch.drawRunsData();
        for (uint32_t i = 0; i < m_impl->textBatch.drawRunsCount(); ++i) {
            m_impl->transparentDrawItems.push_back({
                .kind = TransparentDrawKind::Text,
                .zIndex = textRuns[i].zIndex,
                .index = i,
                .order = textRuns[i].submitOrder,
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
            case TransparentDrawKind::Shadow:
                renderShadowRun(shadowRuns[item.index]);
                break;
            case TransparentDrawKind::Primitive:
                renderPrimitiveRun(
                    transparentPrimitiveRuns[item.index],
                    transparentInstanceOffset,
                    m_impl->primitivePipeline->getTransparentPipeline(),
                    {RenderPipelineKind::PrimitiveTransparent, 0});
                break;
            case TransparentDrawKind::CustomQuad:
                renderCustomQuadRun(transparentCustomQuadRuns[item.index],
                                    transparentCustomInstanceOffset,
                                    true);
                break;
            case TransparentDrawKind::PathFill: {
                const auto &r =
                    m_impl->transparentPathBatch.drawRanges()[item.index];
                renderPathRange(r,
                                transparentStencilVertexOffset,
                                transparentCoverVertexOffset,
                                transparentPathInstanceOffset,
                                true);
            } break;
            case TransparentDrawKind::PathStroke: {
                const auto &r =
                    m_impl->transparentPathStrokeBatch.drawRanges()[item.index];
                renderPathStrokeRange(r,
                                      transparentStrokeVertexOffset,
                                      transparentPathStrokeInstanceOffset,
                                      true);
            } break;
            case TransparentDrawKind::Text:
                renderTextRun(textRuns[item.index]);
                break;
            }
        }

        renderPass.End();
        m_impl->recordQueuedPickingReadback();

        wgpu::CommandBuffer commandBuffer = m_impl->commandEncoder.Finish();
        m_impl->queueCommandBuffer(std::move(commandBuffer));

        if (m_impl->frameUsesSurface) {
            m_impl->flushPendingCommandBuffers();
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
        m_impl->flushPendingCommandBuffers();

        if (region.texture == 0) {
            throw std::runtime_error(
                "readTexture requires a non-zero texture handle");
        }

        const auto &resource = m_impl->getTexture(region.texture);
        return readTextureRegion(m_impl->instance,
                                 m_impl->device,
                                 m_impl->queue,
                                 resource.texture,
                                 resource.format,
                                 resource.width,
                                 resource.height,
                                 region);
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
        m_impl->flushPendingCommandBuffers();

        Core::Renderer::TextureReadbackResult readback;
        if (m_impl->lastCompletedTargetTexture != 0) {
            const auto &resource =
                m_impl->getTexture(m_impl->lastCompletedTargetTexture);
            readback = readTextureRegion(m_impl->instance,
                                         m_impl->device,
                                         m_impl->queue,
                                         resource.texture,
                                         resource.format,
                                         resource.width,
                                         resource.height,
                                         {.texture = resource.handle,
                                          .x = 0,
                                          .y = 0,
                                          .width = resource.width,
                                          .height = resource.height});
        } else {
            const uint32_t width = std::max(1u, m_impl->extent.width);
            const uint32_t height = std::max(1u, m_impl->extent.height);
            readback = readTextureRegion(m_impl->instance,
                                         m_impl->device,
                                         m_impl->queue,
                                         m_impl->offscreenTarget,
                                         m_impl->targetFormat,
                                         width,
                                         height,
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

        const uint64_t submitOrder = m_impl->nextSubmitOrder();
        if (hasDrawableShadow(props.shadow)) {
            makeQuadShadowInstanceInPlace(m_impl->shadowBatch.push(submitOrder),
                                          props);
        }

        if (isTransparent(props)) {
            makePrimitiveInstanceInPlace(m_impl->transparentPrimitiveBatch.push(
                                             props.texture, submitOrder),
                                         props);
        } else {
            makePrimitiveInstanceInPlace(
                m_impl->opaquePrimitiveBatch.push(props.texture, submitOrder),
                props);
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

        const uint64_t submitOrder = m_impl->nextSubmitOrder();
        if (hasDrawableShadow(props.quad.shadow)) {
            makeQuadShadowInstanceInPlace(m_impl->shadowBatch.push(submitOrder),
                                          props.quad,
                                          props.transformMode);
        }

        if (isTransparent(props.quad)) {
            makeCustomQuadInstanceInPlace(
                m_impl->transparentCustomQuadBatch.push(props.shader,
                                                        submitOrder),
                props);
        } else {
            makeCustomQuadInstanceInPlace(
                m_impl->opaqueCustomQuadBatch.push(props.shader, submitOrder),
                props);
        }
        m_impl->stats.quadCount = m_impl->quadStatsCount();
    }

    void WgpuRenderer2D::drawCustomQuad(
        const Core::Renderer::QuadProps &quad,
        CustomQuadShaderHandle shader,
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

        const uint64_t submitOrder = m_impl->nextSubmitOrder();
        if (hasDrawableShadow(props.shadow)) {
            makeCircleShadowInstanceInPlace(
                m_impl->shadowBatch.push(submitOrder), props);
        }

        if (props.color.a < 1.0f) {
            makeCircleInstanceInPlace(
                m_impl->transparentPrimitiveBatch.push(0, submitOrder), props);
        } else {
            makeCircleInstanceInPlace(
                m_impl->opaquePrimitiveBatch.push(0, submitOrder), props);
        }
        m_impl->stats.quadCount = m_impl->quadStatsCount();
    }

    void WgpuRenderer2D::drawLine(const Core::Renderer::LineProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }

        const uint64_t submitOrder = m_impl->nextSubmitOrder();
        if (hasDrawableShadow(props.shadow)) {
            makeLineShadowInstanceInPlace(m_impl->shadowBatch.push(submitOrder),
                                          props);
        }

        if (props.color.a < 1.0f) {
            makeLineInstanceInPlace(
                m_impl->transparentPrimitiveBatch.push(0, submitOrder), props);
        } else {
            makeLineInstanceInPlace(
                m_impl->opaquePrimitiveBatch.push(0, submitOrder), props);
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

        const uint64_t submitOrder = m_impl->nextSubmitOrder();
        if (m_impl->textPipeline != nullptr &&
            m_impl->msdfFontAtlas != nullptr &&
            Text::appendMsdfText(text,
                                 props,
                                 *m_impl->msdfFontAtlas,
                                 m_impl->textBatch,
                                 submitOrder)) {
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
                : (0.f < defaultLineHeight ? defaultLineHeight
                                           : props.fontSize);

        const Glyph &spaceGlyph = m_impl->fontFile->getGlyph(U' ');
        const float spaceAdvance =
            std::max(spaceGlyph.advanceX * scale, props.fontSize * 0.25f);

        const PathBakeMetrics metrics = makePathBakeMetricsForTransform(
            props.transformMode, m_impl->cameraTransform, m_impl->extent);

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
            pathProps.transformMode = props.transformMode;
            const std::span<const PathCommand> textCommands{
                m_impl->textPathCommandsScratch.data(),
                m_impl->textPathCommandsScratch.size()};
            submitPathCommands(textCommands,
                               pathProps,
                               metrics,
                               submitOrder,
                               m_impl->opaquePathBatch,
                               m_impl->transparentPathBatch,
                               m_impl->opaquePathStrokeBatch,
                               m_impl->transparentPathStrokeBatch);

            if (props.antiAlias) {
                m_impl->transparentPathStrokeBatch.push(
                    bakePathFillAntiAlias(textCommands,
                                          pathProps,
                                          metrics,
                                          props.antiAliasFringeScale),
                    pathProps,
                    submitOrder);
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
                cursor.x += (spaceAdvance * std::max(props.tabSize, 1.f)) +
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
            return Text::measureMsdfText(text, props, *m_impl->msdfFontAtlas);
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
            currentLineWidth += (safeFontSize * 0.6f) + props.letterSpacing;
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
            return Text::msdfCenterOffsetY(text, props, *m_impl->msdfFontAtlas);
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

        const PathBakeMetrics metrics = makePathBakeMetricsForProps(
            props, m_impl->cameraTransform, m_impl->extent);
        submitPathCommands(commands,
                           props,
                           metrics,
                           m_impl->nextSubmitOrder(),
                           m_impl->opaquePathBatch,
                           m_impl->transparentPathBatch,
                           m_impl->opaquePathStrokeBatch,
                           m_impl->transparentPathStrokeBatch);
    }

    void WgpuRenderer2D::drawPath(const Path2D &path, const PathProps &props) {
        if (!m_impl->frameStarted || path.empty()) {
            return;
        }

        if (m_impl->pathStarted) {
            endPath();
        }

        const PathBakeMetrics metrics = makePathBakeMetricsForProps(
            props, m_impl->cameraTransform, m_impl->extent);
        const uint64_t submitOrder = m_impl->nextSubmitOrder();
        const BakedPathSubmission &submission =
            m_impl->cachedPathSubmission(path, props, metrics);
        submitBakedPathSubmission(submission,
                                  props,
                                  submitOrder,
                                  m_impl->opaquePathBatch,
                                  m_impl->transparentPathBatch,
                                  m_impl->opaquePathStrokeBatch,
                                  m_impl->transparentPathStrokeBatch);
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
        m_impl->activePathSubmitOrder = m_impl->nextSubmitOrder();
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

    void WgpuRenderer2D::pathCubicTo(const glm::vec2 &control1,
                                     const glm::vec2 &control2,
                                     const glm::vec2 &pos,
                                     float strokeWidth) {
        pathCubicTo(control1,
                    control2,
                    pos,
                    Core::Renderer::PathCommandStroke::withWidth(strokeWidth));
    }

    void WgpuRenderer2D::pathCubicTo(const glm::vec2 &control1,
                                     const glm::vec2 &control2,
                                     const glm::vec2 &pos,
                                     float strokeWidth,
                                     PickingId id) {
        pathCubicTo(
            control1,
            control2,
            pos,
            Core::Renderer::PathCommandStroke::withWidthAndId(strokeWidth, id));
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

    void WgpuRenderer2D::pathClose(float strokeWidth) {
        pathClose(Core::Renderer::PathCommandStroke::withWidth(strokeWidth));
    }

    void WgpuRenderer2D::pathClose(float strokeWidth, PickingId id) {
        pathClose(
            Core::Renderer::PathCommandStroke::withWidthAndId(strokeWidth, id));
    }

    void WgpuRenderer2D::endPath() {
        if (!m_impl->pathStarted) {
            return;
        }

        const PathBakeMetrics metrics = makePathBakeMetricsForProps(
            m_impl->activePathProps, m_impl->cameraTransform, m_impl->extent);

        const std::span<const PathCommand> commands{
            m_impl->activePathCommands.data(),
            m_impl->activePathCommands.size()};
        submitPathCommands(commands,
                           m_impl->activePathProps,
                           metrics,
                           m_impl->activePathSubmitOrder,
                           m_impl->opaquePathBatch,
                           m_impl->transparentPathBatch,
                           m_impl->opaquePathStrokeBatch,
                           m_impl->transparentPathStrokeBatch);

        m_impl->activePathCommands.clear();
        m_impl->activePathSubmitOrder = 0;
        m_impl->pathStarted = false;
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
            m_impl->flushPendingCommandBuffers();
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
            m_impl->flushPendingCommandBuffers();
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
        m_impl->queueCommandBuffer(std::move(commandBuffer));
        m_impl->flushPendingCommandBuffers();
        m_impl->surface.Present();

        m_impl->commandEncoder = nullptr;
    }

    wgpu::Device WgpuRenderer2D::getDevice() const {
        return m_impl->device;
    }
    wgpu::Queue WgpuRenderer2D::getQueue() const {
        return m_impl->queue;
    }

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
