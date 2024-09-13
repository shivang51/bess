#include "renderer/gl/framebuffer.h"
#include "settings/viewport_theme.h"
#include "glad/glad.h"
#include "window.h"
#include <iostream>

namespace Bess::Gl {

    FrameBuffer::FrameBuffer(float width, float height, const std::vector<FBAttachmentType> attachements, bool multisampled) {
        assert(Bess::Window::isGladInitialized);

        m_width = width;
        m_height = height;

        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

        std::vector<GLenum> drawAttachments = {};
        m_colorAttachments = {};

        for (auto attachmentType : attachements) {
            if ((int)attachmentType > 10) {
                m_depthBufferStencilType = attachmentType;
                switch (attachmentType) {
                case FBAttachmentType::DEPTH24_STENCIL8: {
                    glGenRenderbuffers(1, &m_rbo);
                    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
                    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)m_width, (GLsizei)m_height);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo);
                } break;
                case FBAttachmentType::DEPTH32F_STENCIL8: {
                    glGenRenderbuffers(1, &m_rbo);
                    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
                    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, (GLsizei)m_width, (GLsizei)m_height);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo);
                } break;
                default:
                    throw std::runtime_error("Invalid depth-stencil attachment. Maybe a color attachemnt? " + std::to_string(attachmentType));
                    break;
                }
            } else {

                GLenum attachmentInd = GL_COLOR_ATTACHMENT0 + (int)m_colorAttachments.size();

                auto formats = getFormatsForAttachementType(attachmentType);

                auto attachment = FBAttachment(attachmentType, width, height, formats.first, formats.second, attachmentInd);
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentInd, GL_TEXTURE_2D, attachment.getTextureId(), 0);
                
                drawAttachments.emplace_back(attachmentInd);
                m_colorAttachments.emplace_back(attachment);
            }
        }
        
        glDrawBuffers((int)drawAttachments.size(), drawAttachments.data());

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[-] Framebuffer is not complete: " << getFramebufferStatusReason(status) << std::endl;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    FrameBuffer::~FrameBuffer() {
        glDeleteFramebuffers(1, &m_fbo);
        m_colorAttachments.clear();
        if (m_rbo != 0) glDeleteRenderbuffers(1, &m_rbo);
    }

    GLuint FrameBuffer::getColorBufferTexId(int idx) const { return m_colorAttachments[idx].getTextureId(); }

    void FrameBuffer::bind() const {
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo));
    }

    //void FrameBuffer::clear() const {
    //    GL_CHECK(glClearColor(ViewportTheme::backgroundColor.x, ViewportTheme::backgroundColor.y,
    //                          ViewportTheme::backgroundColor.z, 1.0f));
    //    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    //    int value = -1;
    //    GL_CHECK(glClearTexImage(m_textures[1]->getId(), 0, GL_RED_INTEGER, GL_INT, &value));
    //}

    void FrameBuffer::unbind() const { glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); }

    void FrameBuffer::resize(float width, float height) {
        m_width = width;
        m_height = height;
        glViewport(0, 0, (GLsizei)width, (GLsizei)height);

        for (auto &attachment : m_colorAttachments) {
            attachment.resize(width, height);
        }

        if (m_rbo == 0)
            return;

        glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
        switch (m_depthBufferStencilType) {
        case FBAttachmentType::DEPTH24_STENCIL8: {
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)m_width, (GLsizei)m_height);
        } break;
        case FBAttachmentType::DEPTH32F_STENCIL8: {
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, (GLsizei)m_width, (GLsizei)m_height);
        } break;
        default:
            break;
        }
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo);
    }

    void FrameBuffer::clearDepthStencilBuf() {
        GL_CHECK(glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    }

    int FrameBuffer::readIntFromColAttachment(int idx, int x, int y) const {
        return readFromColorAttachment<int, GL_INT>(idx, x, y);
    }

    glm::vec2 FrameBuffer::getSize() const { return {m_width, m_height}; }


    std::pair<GLenum, GLuint> FrameBuffer::getFormatsForAttachementType(FBAttachmentType type) {
        GLuint internalFormat = -1;
        GLenum format = -1;
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
            break;
        }

        return {internalFormat, format};
    }

    std::string FrameBuffer::getFramebufferStatusReason(GLenum status) {
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
