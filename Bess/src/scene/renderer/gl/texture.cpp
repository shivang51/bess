#include "scene/renderer/gl/texture.h"

#include <iostream>
#include <ostream>

#include "glad/glad.h"
#include <stb_image.h>
#include <stdexcept>

#include "scene/renderer/gl/gl_wrapper.h"

namespace Bess::Gl {
    Texture::Texture(const std::string &path)
        : m_id(0), m_width(0), m_height(0), m_bpp(0), m_path(path),
          m_internalFormat(GL_RGBA8), m_format(GL_RGBA), m_multisampled(false) {
        stbi_set_flip_vertically_on_load(1);
        const auto data = stbi_load(path.c_str(), &m_width, &m_height, &m_bpp, 4);
        if (data == nullptr) {
            throw std::runtime_error("Failed to load texture");
        }

        glGenTextures(1, &m_id);
        glBindTexture(GL_TEXTURE_2D, m_id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
    }

    Texture::Texture(const GLint internalFormat, const GLenum format, const int width, const int height, const void *data, const bool multisampled)
        : m_id(0), m_width(width), m_height(height), m_bpp(0), m_multisampled(multisampled) {
        m_internalFormat = internalFormat;
        m_format = format;
        glGenTextures(1, &m_id);

        if (multisampled) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_id);
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, internalFormat, m_width, m_height, GL_TRUE);
        } else {
            glBindTexture(GL_TEXTURE_2D, m_id);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            auto dataType = (format == GL_RED_INTEGER) ? GL_INT : GL_UNSIGNED_BYTE;
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_width, m_height, 0, format, dataType, data);
        }
    }

    Texture::~Texture() { glDeleteTextures(1, &m_id); }

    void Texture::bind() const {
        if (m_multisampled) {
            GL_CHECK(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_id));
        } else {
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, m_id));
        }
    }

    void Texture::unbind() const {
        if (m_multisampled) {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
        } else {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    GLuint Texture::getId() const { return m_id; }

    void Texture::setData(const void *data) const {
        this->bind();
        glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, m_format, GL_UNSIGNED_BYTE, data);
    }

    void Texture::resize(const int width, const int height, const void *data) {
        m_width = width;
        m_height = height;
        bind();
        if (m_multisampled) {
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, m_internalFormat, m_width, m_height, GL_TRUE);
        } else {
            auto dataType = (m_format == GL_RED_INTEGER) ? GL_INT : GL_UNSIGNED_BYTE;
            glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0,
                         m_format, dataType, data);
        }
    }

} // namespace Bess::Gl
