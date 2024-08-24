#include "renderer/gl/framebuffer.h"
#include "settings/viewport_theme.h"
#include "glad/glad.h"
#include "window.h"
#include <iostream>

namespace Bess::Gl {
FrameBuffer::FrameBuffer(float width, float height) {
    assert(Bess::Window::isGladInitialized);

    m_width = width;
    m_height = height;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    m_textures.emplace_back(
        std::make_unique<Texture>(GL_RGB, GL_RGB, m_width, m_height));
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           m_textures.back()->getId(), 0);

    m_textures.emplace_back(
        std::make_unique<Texture>(GL_R32I, GL_RED_INTEGER, m_width, m_height));

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                           m_textures.back()->getId(), 0);

    glGenRenderbuffers(1, &m_rbo);

    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          (GLsizei)m_width, (GLsizei)m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, m_rbo);

    GLenum attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[-] Framebuffer is not complete: " << status << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

FrameBuffer::~FrameBuffer() {
    glDeleteFramebuffers(1, &m_fbo);
    m_textures.clear();
    glDeleteRenderbuffers(1, &m_rbo);
}

GLuint FrameBuffer::getTexture() const { return m_textures[0]->getId(); }

void FrameBuffer::bind() const {
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo));
    clear();
}

void FrameBuffer::clear() const {
    GL_CHECK(glClearColor(ViewportTheme::backgroundColor.x, ViewportTheme::backgroundColor.y,
                          ViewportTheme::backgroundColor.z, 1.0f));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    int value = -1;
    GL_CHECK(glClearTexImage(m_textures[1]->getId(), 0, GL_RED_INTEGER, GL_INT,
                             &value));
}

void FrameBuffer::unbind() const { glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); }

void FrameBuffer::resize(float width, float height) {
    m_width = width;
    m_height = height;
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);

    for (auto &texture : m_textures) {
        texture->resize(width, height);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          (GLsizei)m_width, (GLsizei)m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, m_rbo);
}

int FrameBuffer::readId(int x, int y) const {
    GL_CHECK(glReadBuffer(GL_COLOR_ATTACHMENT1));
    int id = -1;
    GL_CHECK(glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &id));
    return id;
}

void FrameBuffer::readPixel(int x, int y) const {
    GL_CHECK(glReadBuffer(GL_COLOR_ATTACHMENT1));
    float data[3] = {0.0f};
    GL_CHECK(glReadPixels(x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, data));
}

glm::vec2 FrameBuffer::getSize() const { return {m_width, m_height}; }
} // namespace Bess::Gl
