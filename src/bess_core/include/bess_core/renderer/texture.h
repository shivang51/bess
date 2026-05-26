#pragma once
#include "common/class_helpers.h"
#include "ext/vector_float2.hpp"
#include <string>
namespace Bess::Core::Renderer {

    class ITexture {
      public:
        ITexture() = default;
        ITexture(const std::string &path);
        virtual ~ITexture() = default;

        virtual void init() = 0;
        virtual void destroy() = 0;

        MAKE_GETTER_SETTER(std::string, Path, m_path);
        MAKE_GETTER_SETTER(glm::vec2, Size, m_size);

      protected:
        std::string m_path;
        glm::vec2 m_size;
    };
} // namespace Bess::Core::Renderer
