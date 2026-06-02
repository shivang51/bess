#pragma once

#include <array>
#include <cstdint>
#include <webgpu/webgpu_cpp.h>

namespace Bess::Wgpu::Piplines {

  struct FrameUniform {
    float viewport[2] = {1.f, 1.f};
    float padding[2] = {0.f, 0.f};
    float cameraTransform[16] = {
      1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
      0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
    };
  };

  class SharedFrameBuffer {
    public:
    void init(const wgpu::Device &device) {
      m_device = device;

      wgpu::BufferDescriptor frameBufferDescriptor{};
      frameBufferDescriptor.size = sizeof(FrameUniform);
      frameBufferDescriptor.usage =
        wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
      m_buffer = m_device.CreateBuffer(&frameBufferDescriptor);
    }

    void destroy() {
      m_buffer = nullptr;
      m_device = nullptr;
    }

    void update(const wgpu::Queue &queue, uint32_t width,
          uint32_t height) {
      if (m_buffer == nullptr) {
        return;
      }

      m_frameUniform.viewport[0] = static_cast<float>(width);
      m_frameUniform.viewport[1] = static_cast<float>(height);
      queue.WriteBuffer(m_buffer, 0, &m_frameUniform,
                sizeof(FrameUniform));
    }

    void setCameraTransform(const std::array<float, 16> &cameraTransform) {
      for (uint32_t index = 0; index < 16; ++index) {
        m_frameUniform.cameraTransform[index] = cameraTransform[index];
      }
    }

    [[nodiscard]] const wgpu::Buffer &getBuffer() const { return m_buffer; }
    [[nodiscard]] uint64_t getSize() const { return sizeof(FrameUniform); }

    private:
    wgpu::Device m_device;
    wgpu::Buffer m_buffer;
    FrameUniform m_frameUniform;
  };

    class Pipeline {
      public:
        virtual ~Pipeline() = default;

        virtual void init(const wgpu::Device &device,
              wgpu::TextureFormat targetFormat,
              const wgpu::Buffer &frameBuffer,
              uint64_t frameBufferSize,
              wgpu::TextureFormat pickingFormat = wgpu::TextureFormat::Undefined) = 0;
        virtual void destroy() = 0;
    };

} // namespace Bess::Wgpu::Piplines
