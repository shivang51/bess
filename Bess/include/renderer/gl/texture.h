#pragma once

#include "glad/glad.h"
#include <string>
namespace Bess::Gl {
class Texture {
  public:
    Texture(const std::string &path);
    Texture(const GLint internalFormat, const GLenum format, const int width,
            const int height, void* data = nullptr);

    ~Texture();
    void bind();
    void unbind();
    GLuint getId();

    void resize(int width, int height, void* data = nullptr);

    void setData(void* data);

  private:
    GLuint m_id;
    int m_width, m_height;
    int m_bpp; // bits per pixel
    std::string m_path;

    GLuint m_internalFormat;
    GLenum m_format;
};
} // namespace Bess::Gl
