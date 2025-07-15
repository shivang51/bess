#pragma once

#include "glad/glad.h"
#include <string>
#include <functional>

namespace Bess::Gl {
    class Texture {
      public:
        // Texture() = default;
        Texture(const std::string &path);
        Texture(GLint internalFormat, GLenum format, int width, int height, const void *data = nullptr, bool multisampled = false);

        ~Texture();
        void bind(int slotIdx = -1) const;

        void unbind() const;
        GLuint getId() const;

        void resize(int width, int height, const void *data = nullptr);

        void setData(const void *data);

        void saveToPath(const std::string &path, bool bindTexture = true) const;

        bool operator==(const Texture &other) const {
            return (m_id == other.m_id);
        }

        size_t getWidth() const { return m_width; }
        size_t getHeight() const { return m_height; }


      private:
        int getChannelsFromFormat() const;
      private:
        GLuint m_id = 0;
        size_t m_width, m_height;
        int m_bpp; // bits per pixel
        std::string m_path;

        GLint m_internalFormat;
        GLenum m_format;

        bool m_multisampled{};
    };
} // namespace Bess::Gl

namespace std {
    template <>
    struct hash<Bess::Gl::Texture> {
        size_t operator()(const Bess::Gl::Texture &texture) const {
            // Use the OpenGL ID as the basis for the hash.
            // Since GLuint is an unsigned integer, directly casting to size_t or
            // using it is usually fine, as std::hash for integral types
            // typically just returns the value itself or a slightly transformed version.
            return std::hash<GLuint>{}(texture.getId());
        }
    };
} // namespace std