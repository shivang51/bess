#pragma once
#include "fwd.hpp"
#include "gl/gl_wrapper.h"

namespace Bess::Gl {
class FrameBuffer {
  public:
    FrameBuffer() = default;

    FrameBuffer(float width, float height);
    ~FrameBuffer();

    GLuint getTexture() const;

    glm::vec2 getSize() const;

    void bind() const;
    void unbind() const;
    void resize(float width, float height);

    int readId(int x, int y) const;
    void readPixel(int x, int y) const;

    void clear() const;

  private:
    GLuint m_fbo;
    GLuint m_textures[2];
    GLuint m_rbo;

    float m_width;
    float m_height;
};
} // namespace Bess::Gl
