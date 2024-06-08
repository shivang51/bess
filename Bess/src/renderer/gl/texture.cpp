#include "renderer/gl/texture.h"
#include "glad/glad.h"
#include <renderer/gl/gl_wrapper.h>
#include <stb_image.h>
#include <stdexcept>

namespace Bess::Gl {
Texture::Texture(const std::string &path)
    : m_path(path), m_id(0), m_width(0), m_height(0), m_bpp(0),
      m_internalFormat(GL_RGBA8), m_format(GL_RGBA) {
    loadImage(path);

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, m_data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(m_data);
}

Texture::Texture(const GLint internalFormat, const GLenum format,
                 const int width, const int height)
    : m_id(0), m_width(width), m_height(height), m_bpp(0), m_path("") {
    m_internalFormat = internalFormat;
    m_format = format;
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_width, m_height, 0, format,
                 GL_UNSIGNED_BYTE, nullptr);
}

Texture::~Texture() { glDeleteTextures(1, &m_id); }

void Texture::bind() { glBindTexture(GL_TEXTURE_2D, m_id); }

void Texture::unbind() { glBindTexture(GL_TEXTURE_2D, 0); }

GLuint Texture::getId() { return m_id; }

void Texture::loadImage(const std::string &path) {
    stbi_set_flip_vertically_on_load(1);
    m_data = stbi_load(path.c_str(), &m_width, &m_height, &m_bpp, 4);
    if (m_data == nullptr) {
        throw std::runtime_error("Failed to load texture");
    }
}

void Texture::resize(int width, int height) {
    m_width = width;
    m_height = height;
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0,
                 m_format, GL_UNSIGNED_BYTE, nullptr);
}

} // namespace Bess::Gl
