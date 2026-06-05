#pragma once
#include "bess_core/renderer/renderer_types.h"
#include "common/class_helpers.h"
#include "ext/vector_float2.hpp"
#include <cstdint>
#include <string>
namespace Bess::Core::Renderer {

    enum class Renderer2DTargetFormat : uint8_t;

    struct TextureCreateInfo {
        std::string path;
        // None means use the renderer's color target format for render targets.
        Renderer2DTargetFormat format{};
    };

    class ITexture {
      public:
        ITexture() = default;
        ITexture(const std::string &path);
        ITexture(const TextureCreateInfo &createInfo);
        virtual ~ITexture() = default;

        virtual void init() = 0;
        virtual void destroy() = 0;
        virtual void *getView() const = 0;
        virtual void saveToFile(const std::string &path) const;

        MAKE_GETTER_SETTER(std::string, Path, m_path);
        MAKE_GETTER_SETTER(glm::vec2, Size, m_size);
        MAKE_GETTER_SETTER(TextureHandle, Handle, m_handle);
        MAKE_GETTER_SETTER(Renderer2DTargetFormat, Format, m_format);

      protected:
        static TextureHandle getNextTextureHandle();

      protected:
        std::string m_path;
        glm::vec2 m_size;
        TextureHandle m_handle = 0;
        Renderer2DTargetFormat m_format{};
    };
} // namespace Bess::Core::Renderer
