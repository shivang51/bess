#pragma once

#include "glad/glad.h"
#include <string>
namespace Bess::Gl {
    class Texture {
      public:
        explicit Texture(const std::string &path);
        Texture(GLint internalFormat, GLenum format, int width, int height, const void *data = nullptr, bool multisampled = false);

        ~Texture();
        void bind(int slotIdx = -1) const;

        void unbind() const;
        GLuint getId() const;

        void resize(int width, int height, const void *data = nullptr);

        void setData(const void *data);

        void saveToPath(const std::string &path, bool bindTexture = true) const;

      private:
        int getChannelsFromFormat() const;
      private:
        GLuint m_id;
        int m_width, m_height;
        int m_bpp; // bits per pixel
        std::string m_path;

        GLint m_internalFormat;
        GLenum m_format;

        bool m_multisampled{};
    };
} // namespace Bess::Gl
