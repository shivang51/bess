#include "renderer/gl/framebuffer.h"
#include "glm.hpp"
#include "window.h"
#include <iostream>
#include "common/theme.h"

namespace Bess::Gl
{
  FrameBuffer::FrameBuffer(float width, float height)
  {
    assert(Bess::Window::isGladInitialized);

    m_width = width;
    m_height = height;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(2, m_textures);
    for (int i = 0; i < 2; i++)
    {
      glBindTexture(GL_TEXTURE_2D, m_textures[i]);
      if (i == 0)
      {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)width, (GLsizei)height, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, NULL);
      }
      else
      {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, (GLsizei)width, (GLsizei)height, 0, GL_RED_INTEGER,
                     GL_UNSIGNED_BYTE, NULL);
      }
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                             GL_TEXTURE_2D, m_textures[i], 0);
    }

    glGenRenderbuffers(1, &m_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)width, (GLsizei)height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, m_rbo);

    GLenum attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
      std::cerr << "Framebuffer is not complete" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
  }

  FrameBuffer::~FrameBuffer()
  {
    glDeleteFramebuffers(1, &m_fbo);
    glDeleteTextures(2, m_textures);
    glDeleteRenderbuffers(1, &m_rbo);
  }

  GLuint FrameBuffer::getTexture() const { return m_textures[0]; }

  void FrameBuffer::bind() const
  {
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo));
    clear();
  }

  void FrameBuffer::clear() const
  {
    GL_CHECK(glClearColor(Theme::backgroundColor.x, Theme::backgroundColor.y, Theme::backgroundColor.z, 1.0f));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    int value = -1;
    GL_CHECK(glClearTexImage(m_textures[1], 0, GL_RED_INTEGER, GL_INT, &value));
  }

  void FrameBuffer::unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

  void FrameBuffer::resize(float width, float height)
  {
    m_width = width;
    m_height = height;

    glViewport(0, 0, (GLsizei)width, (GLsizei)height);

    for (int i = 0; i < 2; i++)
    {
      glBindTexture(GL_TEXTURE_2D, m_textures[i]);
      if (i == 0)
      {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)width, (GLsizei)height, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, NULL);
      }
      else
      {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, (GLsizei)width, (GLsizei)height, 0, GL_RED_INTEGER,
                     GL_UNSIGNED_BYTE, NULL);
      }
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                             GL_TEXTURE_2D, m_textures[i], 0);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)width, (GLsizei)height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, m_rbo);
  }

  int FrameBuffer::readId(int x, int y) const
  {
    GL_CHECK(glReadBuffer(GL_COLOR_ATTACHMENT1));
    int id = -2;
    GL_CHECK(glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &id));
    return id;
  }

  void FrameBuffer::readPixel(int x, int y) const
  {
    GL_CHECK(glReadBuffer(GL_COLOR_ATTACHMENT1));
    float data[3] = {0.0f};
    GL_CHECK(glReadPixels(x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, data));
    std::cout << "Pixel: " << data[0] << " " << data[1] << " " << data[2]
              << std::endl;
  }

  glm::vec2 FrameBuffer::getSize() const { return {m_width, m_height}; }
} // namespace Bess::Gl
