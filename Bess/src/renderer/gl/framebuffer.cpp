#include "renderer/gl/framebuffer.h"
#include "glad/glad.h"
#include "settings/viewport_theme.h"
#include "window.h"
#include <iostream>

namespace Bess::Gl {
    FrameBuffer::FrameBuffer(const float width, const float height, const std::vector<FBAttachmentType> &attachments, const bool multisampled) {
        assert(Bess::Window::isGladInitialized);
        m_multisampled = multisampled;
        m_width = width;
        m_height = height;
        m_fbo = 0;

        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

        m_drawAttachments = {};
        m_colorAttachments = {};

        for (const auto attachmentType : attachments) {
            if (static_cast<int>(attachmentType) > 10) {
                m_depthBufferStencilType = attachmentType;
                const GLint internalFormat = getDepthStencilInternalFormat(attachmentType);

                glGenRenderbuffers(1, &m_rbo);
                glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
                if (multisampled) {
                    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, internalFormat, static_cast<GLsizei>(m_width),
                                                     static_cast<GLsizei>(m_height));
                } else {
                    glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, static_cast<GLsizei>(m_width),
                                          static_cast<GLsizei>(m_height));
                }
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo);
            } else {
                GLenum attachmentInd = GL_COLOR_ATTACHMENT0 + static_cast<int>(m_colorAttachments.size());
                auto [fst, snd] = getFormatsForAttachmentType(attachmentType);
                auto attachment = FBAttachment(attachmentType, width, height, fst, snd, attachmentInd, multisampled);

                if (multisampled) {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentInd, GL_TEXTURE_2D_MULTISAMPLE, attachment.getTextureId(), 0);
                } else {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentInd, GL_TEXTURE_2D, attachment.getTextureId(), 0);
                }
                m_drawAttachments.emplace_back(attachmentInd);
                m_colorAttachments.emplace_back(attachment);
            }
        }

        glDrawBuffers(static_cast<int>(m_drawAttachments.size()), m_drawAttachments.data());

        if (const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[-] Framebuffer is not complete: " << getFramebufferStatusReason(status) << std::endl;
            assert(false);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    FrameBuffer::~FrameBuffer() {
        glDeleteFramebuffers(1, &m_fbo);
        m_colorAttachments.clear();
        if (m_rbo != 0)
            glDeleteRenderbuffers(1, &m_rbo);
    }

    GLuint FrameBuffer::getColorBufferTexId(const int idx) const { return m_colorAttachments[idx].getTextureId(); }

    void FrameBuffer::bind() const {
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo));
    }

    void FrameBuffer::unbindForDraw() { glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); }

    void FrameBuffer::unbindAll() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FrameBuffer::resize(const float width, const float height) {
        bind();
        m_width = width;
        m_height = height;
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

        for (auto &attachment : m_colorAttachments) {
            attachment.resize(width, height);
        }

        GLint internalFormat = -1;

        if (m_rbo == 0)
            goto end;
        GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, m_rbo));
        internalFormat = getDepthStencilInternalFormat(m_depthBufferStencilType);
        if (m_multisampled) {
            GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, internalFormat, static_cast<GLsizei>(m_width),
                                                      static_cast<GLsizei>(m_height)));
        } else {
            GL_CHECK(glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, static_cast<GLsizei>(m_width),
                                           static_cast<GLsizei>(m_height)));
        }
        GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

        if (const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[-] Framebuffer is not complete after resize: " << getFramebufferStatusReason(status) << std::endl;
            assert(false);
        }

    end:
        unbindAll();
    }

    void FrameBuffer::clearDepthStencilBuf() {
        GL_CHECK(glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    }

    int FrameBuffer::readIntFromColAttachment(const int idx, const int x, const int y) const {
        int data = -1;
        readFromColorAttachment<GL_INT>(idx, x, y, 1, 1, &data);
        return data;
    }

    std::vector<int> FrameBuffer::readIntsFromColAttachment(const int idx, const int x, const int y, const int w, const int h) const {
        std::vector<int> data(w * h, -2);
        readFromColorAttachment<GL_INT>(idx, x, y, w, h, data.data());
        return data;
    }

    void FrameBuffer::resetDrawAttachments() const {
        bind();
        glDrawBuffers(static_cast<int>(m_drawAttachments.size()), m_drawAttachments.data());
    }

    glm::vec2 FrameBuffer::getSize() const { return {m_width, m_height}; }

    void FrameBuffer::bindColorAttachmentForDraw(const int idx) const {
        GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo));
        GL_CHECK(glDrawBuffer(GL_COLOR_ATTACHMENT0 + idx));
    }

    void FrameBuffer::bindColorAttachmentForRead(const int idx) const {
        GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo));
        GL_CHECK(glReadBuffer(GL_COLOR_ATTACHMENT0 + idx));
    }

    void FrameBuffer::blitColorBuffer(const float width, const float height) {
        GL_CHECK(glBlitFramebuffer(0, 0, static_cast<GLint>(width), static_cast<GLint>(height), 0, 0, static_cast<GLint>(width),
                                   static_cast<GLint>(height), GL_COLOR_BUFFER_BIT, GL_NEAREST));
    }

    std::pair<GLint, GLenum> FrameBuffer::getFormatsForAttachmentType(const FBAttachmentType type) {
        GLuint internalFormat;
        GLenum format;
        switch (type) {
        case FBAttachmentType::R32I_REDI: {
            internalFormat = GL_R32I;
            format = GL_RED_INTEGER;
        } break;
        case FBAttachmentType::RGB_RGB: {
            internalFormat = GL_RGB;
            format = GL_RGB;
        } break;
        case FBAttachmentType::RGBA_RGBA: {
            internalFormat = GL_RGBA;
            format = GL_RGBA;
        } break;
        default:
            throw std::runtime_error("Invalid color attachment type passed " + std::to_string(type));
        }

        return {internalFormat, format};
    }

    GLint FrameBuffer::getDepthStencilInternalFormat(const FBAttachmentType type) {
        GLint internalFormat;

        switch (type) {
        case FBAttachmentType::DEPTH24_STENCIL8: {
            internalFormat = GL_DEPTH24_STENCIL8;
        } break;
        case FBAttachmentType::DEPTH32F_STENCIL8: {
            internalFormat = GL_DEPTH32F_STENCIL8;
        } break;
        default:
            throw std::runtime_error("Invalid depth-stencil attachment. Maybe a color attachment? " + std::to_string(type));
        }

        return internalFormat;
    }

    std::string FrameBuffer::getFramebufferStatusReason(const GLenum status) {
        switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            return "GL_FRAMEBUFFER_UNDEFINED: The default framebuffer does not exist, and FBO 0 is bound.";

        case GL_FRAMEBUFFER_COMPLETE:
            return "GL_FRAMEBUFFER_COMPLETE: The framebuffer is complete.";

        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: One or more attachment points are not framebuffer attachment complete.";

        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: No images are attached to the framebuffer, or framebuffer default width/height are zero.";

        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: A draw buffer has no image attached, and is not GL_NONE.";

        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: The read buffer has no image attached.";

        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: Attached images have different number of samples or inconsistent fixed sample layout.";

        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: Layered attachments are not consistent.";

        case GL_FRAMEBUFFER_UNSUPPORTED:
            return "GL_FRAMEBUFFER_UNSUPPORTED: The combination of internal formats of the attached images violates an implementation-dependent set of restrictions.";

        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT: Attachments do not have the same width and height.";

        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            return "GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT: Attachments have different formats.";

        default:
            return "Unknown status: This status is not recognized.";
        }
    }

} // namespace Bess::Gl
