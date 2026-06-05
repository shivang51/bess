#pragma once
#include "bess_core/renderer/renderer_types.h"
#include "common/class_helpers.h"
#include "ext/vector_float2.hpp"
#include <string>
namespace Bess::Core::Renderer {

    struct TextureCreateInfo {
        std::string path;
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

        MAKE_GETTER_SETTER(std::string, Path, m_path);
        MAKE_GETTER_SETTER(glm::vec2, Size, m_size);
        MAKE_GETTER_SETTER(TextureHandle, Handle, m_handle);

      protected:
        static TextureHandle getNextTextureHandle();

      protected:
        std::string m_path;
        glm::vec2 m_size;
        TextureHandle m_handle = 0;
    };
} // namespace Bess::Core::Renderer
