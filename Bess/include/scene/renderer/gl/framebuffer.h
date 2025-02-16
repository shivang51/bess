#pragma once
#include "fb_attachment.h"
#include "fwd.hpp"
#include "gl_wrapper.h"
#include "glad/glad.h"
#include "scene/renderer/gl/texture.h"
#include <iostream>
#include <memory>
#include <vector>

namespace Bess::Gl {

    class FrameBuffer {
      public:
        FrameBuffer() = default;

        FrameBuffer(float width, float height, const std::vector<FBAttachmentType> &attachments, bool multisampled = false);
        ~FrameBuffer();

        GLuint getColorBufferTexId(int idx = 0) const;

        glm::vec2 getSize() const;

        void bindColorAttachmentForDraw(int idx) const;
        void bindColorAttachmentForRead(int idx) const;

        static void blitColorBuffer(float width, float height);

        void bind() const;
        static void unbindForDraw();
        static void unbindAll();
        void resize(float width, float height);

        template <GLenum GlType>
        void clearColorAttachment(const unsigned int idx, const void *data) const {
            bind();
            auto [_, type] = getFormatsForAttachmentType(m_colorAttachments[idx].getType());
            auto &attachment = m_colorAttachments[idx];
            attachment.bindTexture();
            GL_CHECK(glClearTexImage(attachment.getTextureId(), 0, attachment.getFormat(), GlType, data));
        }

        static void clearDepthStencilBuf();

        int32_t readIntFromColAttachment(int idx, int x, int y) const;

        std::vector<int> readIntsFromColAttachment(int idx, int x, int y, int w, int h) const;

        void resetDrawAttachments() const;

        template <GLenum GlType>
        void readFromColorAttachment(const int idx, const int x, const int y, const int w, const int h, void *data) const {
            auto &attachment = m_colorAttachments[idx];
            bindColorAttachmentForRead(idx);
            GL_CHECK(glReadPixels(x, y, w, h, attachment.getFormat(), GlType, data));
        }

        template <GLenum GlType>
        void readNFromColorAttachment(const int idx, const int x, const int y, const int w, const int h, const int n, void *data) const {
            auto &attachment = m_colorAttachments[idx];
            bindColorAttachmentForRead(idx);
            GL_CHECK(glReadnPixels(x, y, w, h, attachment.getFormat(), GlType, n, data));
        }

      private:
        GLuint m_fbo;

        std::vector<FBAttachment> m_colorAttachments;
        std::vector<GLenum> m_drawAttachments;
        GLuint m_rbo = 0;

        float m_width;
        float m_height;
        bool m_multisampled;

        FBAttachmentType m_depthBufferStencilType;

      private:
        static std::pair<GLint, GLenum> getFormatsForAttachmentType(FBAttachmentType type);
        static GLint getDepthStencilInternalFormat(FBAttachmentType type);
        static std::string getFramebufferStatusReason(GLenum status);
    };
} // namespace Bess::Gl
